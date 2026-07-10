/* Copyright (C) 2019 Mr Goldberg
   This file is part of the Goldberg Emulator

   The Goldberg Emulator is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   The Goldberg Emulator is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the Goldberg Emulator; if not, see
   <http://www.gnu.org/licenses/>.  */

// Direct 7034 match-flow and custom-game loading lifecycle handlers for the
// Dota Game Coordinator. Extracted from gbe_dota_handlers.cpp (Phase 3.1.5a)
// to group the 7034 connected-players / strategy-time / runtime-update /
// launch-poll / wait-for-players request family together with the 7070/8052/8053
// custom-game ready-up / started-loading / finished-loading flow that advances
// the same launch_phase state machine.
//
// Responsibility boundary: owns the local lobby launch-phase advancement,
// runtime lobby details update queuing, and connected-players response
// construction triggered by the 7034 match-flow and the custom-game loading
// lifecycle. Side-effect ownership and ordering are unchanged from the prior
// monolithic handler file; only the file location moved. The single
// handler-local static GBE_AdaptDota7034ConnectedPlayersResponsePayload moved
// with the 7034 handlers (List X, kept `static` in new TU); no cross-TU
// symbols needed externalization (List Y = 0).
//
// Migration note: the three custom-game loading handlers were previously
// extracted to dll/gbe_dota_custom_game_handlers.cpp (Phase 3.1.5) and have
// been merged into this file because they are a sub-phase of the 7034 launch
// flow. The standalone custom_game_handlers.cpp was removed.

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_request_router.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_lobby_state.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_gc_internal.h"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <random>
#include <string>
#include <vector>
#include <steammessages.pb.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/econ_gcmessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>
#include <tf2/gcsystemmsgs.pb.h>
#include <tf2/tf_gcmessages.pb.h>

using namespace gamecoordinator::tf2;

using GBE_Dota7034RequestShape = gbe::proto_wire::Dota7034RequestShape;
using GBE_Dota7034RuntimeRequest = gbe::proto_wire::Dota7034RuntimeRequest;
using GBE_Dota7034ConnectedPlayer = gbe::proto_wire::Dota7034ConnectedPlayer;
using GBE_Dota7034DisconnectedPlayer = gbe::proto_wire::Dota7034DisconnectedPlayer;
using GBE_Dota7070ReadyUpRequest = gbe::proto_wire::Dota7070ReadyUpRequest;
using GBE_Dota8052StartedLoadingRequest = gbe::proto_wire::Dota8052StartedLoadingRequest;
using GBE_Dota8053FinishedLoadingRequest = gbe::proto_wire::Dota8053FinishedLoadingRequest;


// ============================================================================
// Side-effect order documentation (see dll/gbe_dota_action_model.h for the
// canonical action type and cross-domain ordering invariants).
// ============================================================================
//
// Cross-handler boundary note: this file owns per-handler request
// parsing/mutation/response/side-effect sequencing for the 7034 match-flow
// family and the custom-game loading lifecycle. The 7034 entry handler fans
// out to internal helper methods (OwnerHeroKnownEquipReplay,
// DisconnectedPlayers, RuntimeUpdates -> WaitForPlayers / StrategyTime ->
// Fallback / Preserve / LaunchPoll, Response) which together form the 7034
// side-effect sequence; callers should not interleave other state mutations
// between these helpers. Cross-handler lobby state machine consolidation
// (launch/teardown/reconnect transitions sequenced across handlers) belongs to
// Phase 3.4, not here.
//
// GBE_AdaptDota7034ConnectedPlayersResponsePayload (file-local static, pure):
//   Builds the 7034 connected-players response payload. Reads request shape +
//   lobby member state; writes response_message. No coordinator state access.
//
// GBE_HandleDotaDirect7034Request (emsg 7034, runtime match-flow):
//   Active-lobby branch:
//   1. parse_dota7034_request_shape (pure)
//   2. If !custom_game_launch && draft_steam_id==owner: update owner_team /
//      owner_slot; if changed: PublishSharedDotaLobbyState [publish]
//   3. For each connected_player: GBE_SetDotaLobbyMemberRuntimeState
//      [coordinator mutation]; if member updated: PublishSharedDotaLobbyState
//      [publish]; if owner hero changed: GBE_HandleDotaDirectOwnerHeroKnownEquipReplay
//   4. If custom_game_launch: CaptureCurrentDotaLobbyState +
//      NormalizeDotaArcadeLobbyMemberSlots [coordinator read + mutation]; if
//      normalized: PublishSharedDotaLobbyState [publish]
//   5. GBE_HandleDotaDirectOwnerHeroKnownEquipReplay (if owner hero updated)
//   6. GBE_HandleDotaDirect7034DisconnectedPlayers (per-player mutation + publish)
//   7. If state==1 && game_state==0 && launch_phase>=SetupSynced &&
//      launch_4511_seen: GBE_TryAdvanceDotaLaunchToRun [coordinator: emits 26]
//   8. GBE_HandleDotaDirect7034RuntimeUpdates (queues runtime 26 updates; calls
//      WaitForPlayers / StrategyTime / LaunchPoll)
//   9. If custom_game_launch: return early (no 7034 response)
//   Else / fallthrough: parse_dota7034_request_shape (pure) ->
//      GBE_HandleDotaDirect7034Response
//   Invariant: member mutation + publish precedes launch advance precedes
//   runtime update precedes response. Custom-game path returns without 7034
//   response.
//
// GBE_HandleDotaDirectOwnerHeroKnownEquipReplay (helper, server-GC only):
//   1. Read client_gc->get_items() [coordinator read]
//   2. GBE_PushDotaPlayerEquippedItemsCacheToGC(server_gc, owner, items)
//      [coordinator]
//   Invariant: no lobby mutation; only re-pushes host equipped items to server
//   GC cache after owner hero becomes known.
//
// GBE_HandleDotaDirect7034DisconnectedPlayers (helper):
//   1. For each disconnected_player: GBE_SetDotaLobbyMemberRuntimeState(steam_id,
//      false, 0, false) [coordinator mutation]; if updated:
//      PublishSharedDotaLobbyState [publish]
//   Invariant: per-player mutation precedes per-player publish.
//
// GBE_HandleDotaDirect7034RuntimeUpdates (helper, dispatcher):
//   1. If custom_game_launch && state==2 && launch_phase>=RunQueued &&
//      request_game_state>lobby_game_state:
//      GBE_TryQueueDotaRuntimeLobbyDetailsUpdate [coordinator: emits 26]
//   2. GBE_HandleDotaDirect7034WaitForPlayers
//   3. If !custom_game_launch && state==2 && game_state==0 &&
//      launch_phase>=RunQueued: GBE_TryQueueDotaPrelaunch021 [coordinator:
//      emits 26]
//   4. GBE_HandleDotaDirect7034StrategyTime
//   5. GBE_HandleDotaDirect7034LaunchPoll
//   Invariant: queue ordering is custom-runtime -> wait_for_players ->
//   prelaunch021 -> strategy_time -> launch_poll.
//
// GBE_HandleDotaDirect7034StrategyTime (helper, dispatcher):
//   1. GBE_HandleDotaDirect7034StrategyTimeFallback
//   2. GBE_HandleDotaDirect7034StrategyTimePreserve
//
// GBE_HandleDotaDirect7034StrategyTimeFallback (helper):
//   1. If !custom_game_launch && state==2 && game_state==2 &&
//      request_game_state==2 && send_reason==2 && game_mode==1:
//      a. GBE_ShouldHoldDotaLanLaunchForRemoteMembers(3) [coordinator read]
//      b. If !hold: GBE_TryQueueDotaRuntimeLobbyDetailsUpdate("AP hero_selection
//         fallback", state=2, game_state=3) [coordinator: emits 26]
//   Invariant: AP fallback only fires on game_mode==1 with send_reason==2.
//
// GBE_HandleDotaDirect7034StrategyTimePreserve (helper):
//   1. If state==2 && game_state==3: log preserve note + set
//      queued_runtime_lobby_update=true
//   Invariant: no state mutation; only suppresses official 032 follow-up.
//
// GBE_HandleDotaDirect7034Response (emsg 7034 response builder):
//   1. If is_server && !host_showcase_equip_pushed && request_game_state>=4 &&
//      state==2: GBE_PushDotaPlayerEquippedItemsCacheToGC [coordinator]; set
//      host_showcase_equip_pushed=true
//   2. GBE_AdaptDota7034ConnectedPlayersResponsePayload (pure)
//   3. push_incoming_now(7034 | kProtoMask, response_message) [coordinator]
//   Invariant: showcase equip repush precedes response. Single response push.
//
// GBE_HandleDotaDirect7034LaunchPoll (helper):
//   1. If state==2 && game_state==10: return (no poll)
//   2. GBE_SendDotaPracticeLobbyDetailsUpdate [coordinator: emits 26]
//   Invariant: poll suppressed once game_state reaches 10.
//
// GBE_HandleDotaDirect7034WaitForPlayers (helper):
//   1. If custom_game_launch || state!=2 || game_state!=1: return
//   2. GBE_TryQueueDotaRuntimeLobbyDetailsUpdate("wait_for_players", state=2,
//      game_state=1) [coordinator: emits 26]; if !queued: return
//   3. If !request_advances_to_hero_selection: return
//   4. GBE_ShouldHoldDotaLanLaunchForRemoteMembers(2) [coordinator read]; if
//      !hold: GBE_TryQueueDotaRuntimeLobbyDetailsUpdate("hero_selection",
//      state=2, game_state=2) [coordinator: emits 26]
//   Invariant: wait_for_players precedes hero_selection; hero_selection is
//   suppressed while remote members not yet connected.
//
// GBE_HandleDotaCustomGameReadyUpRequest (emsg 7070 -> 7170 + state mutation):
//   1. Parse: ready_state (field 1)
//   2. build_dota_ready_up_status_payload (pure) -> push_incoming_now(7170)
//      [coordinator]
//   3. If ready_state==1 && state==2 && game_state<1 &&
//      launch_phase>=RunQueued:
//      a. GBE_local_lobby.game_state = 1 [coordinator mutation]
//      b. PublishSharedDotaLobbyState [publish]
//      c. GBE_SendDotaPracticeLobbyDetailsUpdate [coordinator: emits 26]
//   Invariant: 7170 precedes state mutation; mutation precedes publish precedes
//   details update.
//
// GBE_HandleDotaCustomGameStartedLoadingRequest (emsg 8052 -> launch advance OR
// publish + details update):
//   1. Parse: lobby_id (field 1), custom_game_id (field 2), start_time (field 4)
//   2. If lobby_id matches:
//      a. If custom_game_id!=0: GBE_local_lobby.custom_game.game_id =
//         custom_game_id [coordinator mutation]
//      b. If start_time!=0: GBE_local_lobby.game_start_time = start_time
//         [coordinator mutation]
//      c. GBE_TryAdvanceDotaLaunchToRun [coordinator: emits 26]; if !advanced:
//         PublishSharedDotaLobbyState [publish] +
//         GBE_SendDotaPracticeLobbyDetailsUpdate [coordinator: emits 26]
//   Invariant: launch advance attempt precedes publish+details fallback.
//
// GBE_HandleDotaCustomGameFinishedLoadingRequest (emsg 8053 -> state mutation +
// publish + details update):
//   1. parse_dota8053_result (pure)
//   2. If lobby_id matches:
//      a. If launch_phase>=RunQueued: state=2; if game_state<1: game_state=1
//         [coordinator mutation]
//      b. Else if state<2: state=2 [coordinator mutation]
//      c. dota8053_indicates_load_failure (pure decision)
//      d. If !load_failed: GBE_SetDotaLobbyMemberRuntimeState(local, true, 0,
//         false) [coordinator mutation] + GBE_MarkDotaLaunchPhase(Loaded) +
//         PublishDotaPracticeLobbyLocalMemberData [publish]
//      e. PublishSharedDotaLobbyState [publish]
//      f. GBE_SendDotaPracticeLobbyDetailsUpdate [coordinator: emits 26]
//   Invariant: state mutation precedes local member data publish precedes
//   shared lobby publish precedes details update.
// ============================================================================

static bool GBE_AdaptDota7034ConnectedPlayersResponsePayload(
    uint64 steam_id,
    uint32 lobby_state,
    uint32 game_state,
    uint32 owner_team,
    uint32 owner_slot,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    const GBE_Dota7034RequestShape &request_shape,
    bool compact_member_slots,
    bool include_draft_players,
    bool has_request_job,
    uint64 request_job_id,
    std::string &message)
{
    const bool include_draft = include_draft_players && (lobby_state >= 2u && game_state >= 2u);
    std::vector<uint64> connected_steam_ids;
    std::vector<uint64> disconnected_steam_ids;
    std::vector<gbe::gc_message::Dota7034Player> connected_players;
    std::vector<gbe::gc_message::Dota7034Player> disconnected_players;
    std::vector<gbe::gc_message::Dota7034Player> draft_players;

    auto resolve_draft_slot = [&](uint64 player_steam_id, uint32 slot) -> uint32 {
        if (compact_member_slots) {
            for (size_t index = 0; index < members.size(); ++index) {
                if (members[index].steam_id == player_steam_id)
                    return static_cast<uint32>(index);
            }
        }
        return slot > 0u ? (slot - 1u) : 0u;
    };

    auto append_connected_player = [&](uint64 player_steam_id, uint32 hero_id, uint32 team, uint32 slot) {
        if (player_steam_id == 0ull || std::find(connected_steam_ids.begin(), connected_steam_ids.end(), player_steam_id) != connected_steam_ids.end())
            return;

        connected_players.push_back({ player_steam_id, hero_id, team, include_draft ? resolve_draft_slot(player_steam_id, slot) : slot, lobby_state, game_state, include_draft });
        connected_steam_ids.push_back(player_steam_id);
    };

    auto append_disconnected_player = [&](uint64 player_steam_id, uint32 disconnected_lobby_state, uint32 disconnected_game_state) {
        if (player_steam_id == 0ull || std::find(disconnected_steam_ids.begin(), disconnected_steam_ids.end(), player_steam_id) != disconnected_steam_ids.end())
            return;

        disconnected_players.push_back({ player_steam_id, 0u, 0u, 0u, disconnected_lobby_state, disconnected_game_state });
        disconnected_steam_ids.push_back(player_steam_id);
    };

    if (request_shape.has_connected_player || request_shape.has_disconnected_player) {
        for (const GBE_Dota7034ConnectedPlayer &connected_player : request_shape.connected_players) {
            if (!connected_player.has_steam_id || connected_player.steam_id == 0ull)
                continue;
            uint32 hero_id = 0u;
            uint32 team = owner_team;
            uint32 slot = owner_slot;
            for (const GBE_DotaLobbyMemberState &member : members) {
                if (member.steam_id == connected_player.steam_id) {
                    hero_id = member.hero_id;
                    team = member.team;
                    slot = member.slot;
                    break;
                }
            }
            if (connected_player.has_hero_id && connected_player.hero_id != 0u)
                hero_id = connected_player.hero_id;
            append_connected_player(connected_player.steam_id, hero_id, team, slot);
        }

        for (const GBE_Dota7034DisconnectedPlayer &disconnected_player : request_shape.disconnected_players) {
            if (!disconnected_player.has_steam_id || disconnected_player.steam_id == 0ull)
                continue;
            const uint32 disconnected_lobby_state = disconnected_player.has_lobby_state ? disconnected_player.lobby_state : lobby_state;
            const uint32 disconnected_game_state = disconnected_player.has_game_state ? disconnected_player.game_state : game_state;
            append_disconnected_player(disconnected_player.steam_id, disconnected_lobby_state, disconnected_game_state);
        }
    } else {
        append_connected_player(steam_id, 0u, owner_team, owner_slot);
        for (const GBE_DotaLobbyMemberState &member : members) {
            if (member.steam_id == 0ull || member.steam_id == steam_id)
                continue;

            if (member.connected)
                append_connected_player(member.steam_id, member.hero_id, member.team, member.slot);
            else
                append_disconnected_player(member.steam_id, lobby_state, game_state);
        }
    }

    gbe::gc_message::Dota7034ExtraState extra_state{};
    extra_state.has_first_blood_happened = request_shape.has_first_blood_happened;
    extra_state.first_blood_happened = request_shape.first_blood_happened;
    extra_state.has_send_reason = request_shape.has_send_reason;
    extra_state.send_reason = request_shape.send_reason;
    extra_state.has_radiant_kills = request_shape.has_radiant_kills;
    extra_state.radiant_kills = request_shape.radiant_kills;
    extra_state.has_dire_kills = request_shape.has_dire_kills;
    extra_state.dire_kills = request_shape.dire_kills;
    extra_state.has_radiant_lead = request_shape.has_radiant_lead;
    extra_state.radiant_lead = request_shape.radiant_lead;
    extra_state.has_building_state = request_shape.has_building_state;
    extra_state.building_state = request_shape.building_state;
    bool disconnected_request_has_steam_id = false;
    bool disconnected_request_includes_local = false;
    for (const GBE_Dota7034DisconnectedPlayer &disconnected_player : request_shape.disconnected_players) {
        if (!disconnected_player.has_steam_id)
            continue;
        disconnected_request_has_steam_id = true;
        if (disconnected_player.steam_id == steam_id) {
            disconnected_request_includes_local = true;
            break;
        }
    }
    if (request_shape.has_disconnected_player && (!disconnected_request_has_steam_id || disconnected_request_includes_local)) {
        uint32 disconnected_lobby_state = lobby_state;
        uint32 disconnected_game_state = game_state;
        for (const GBE_Dota7034DisconnectedPlayer &disconnected_player : request_shape.disconnected_players) {
            if (disconnected_request_includes_local && disconnected_player.steam_id != steam_id)
                continue;
            if (disconnected_player.has_lobby_state)
                disconnected_lobby_state = disconnected_player.lobby_state;
            if (disconnected_player.has_game_state)
                disconnected_game_state = disconnected_player.game_state;
            if (!disconnected_request_includes_local || disconnected_player.steam_id == steam_id)
                break;
        }
        if (disconnected_lobby_state < lobby_state)
            disconnected_lobby_state = lobby_state;
        if (disconnected_game_state < game_state)
            disconnected_game_state = game_state;

        append_disconnected_player(steam_id, disconnected_lobby_state, disconnected_game_state);
    }

    return gbe::gc_message::build_dota_7034_connected_players_response_payload(connected_players, disconnected_players, draft_players, game_state, extra_state, has_request_job, request_job_id, message);
}


bool Steam_Game_Coordinator::GBE_HandleDotaDirect7034Request(
    uint32 request_emsg,
    const uint8 *body,
    size_t body_size,
    bool has_source_job,
    uint64 source_job)
{
    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && GBE_local_lobby.match_id != 0 && (GBE_local_lobby.server_id != 0 || !GBE_local_lobby.connect.empty())) {
        const GBE_Dota7034RuntimeRequest request = gbe::proto_wire::parse_dota7034_runtime_request(body, body_size);
        bool queued_runtime_lobby_update = false;
        const bool custom_game_launch = GBE_local_lobby.custom_game.game_id != 0ull;

        bool updated_owner_team_or_slot_from_7034 = false;
        if (!custom_game_launch && request.has_draft_steam_id && request.draft_steam_id == GBE_GetDotaLobbyOwnerSteamId()) {
            if (request.has_draft_team && GBE_local_lobby.owner_team != request.draft_team) {
                GBE_local_lobby.owner_team = request.draft_team;
                updated_owner_team_or_slot_from_7034 = true;
            }

            const uint32 draft_owner_slot = request.has_draft_team_slot ? (request.draft_team_slot + 1u) : 0u;
            if (draft_owner_slot != 0u && GBE_local_lobby.owner_slot != draft_owner_slot) {
                GBE_local_lobby.owner_slot = draft_owner_slot;
                updated_owner_team_or_slot_from_7034 = true;
            }
        }

        if (updated_owner_team_or_slot_from_7034) {
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "updated owner team/slot from 7034 draft team=%u slot=%u source_job=%llu state=%u game_state=%u",
                GBE_local_lobby.owner_team,
                GBE_local_lobby.owner_slot,
                static_cast<unsigned long long>(source_job),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state
            );
            GBE_PublishSharedDotaLobbyState("7034_draft_team_slot");
        }

        bool owner_hero_updated_from_7034 = false;
        const uint64 owner_steam_id = GBE_GetDotaLobbyOwnerSteamId();
        for (const GBE_Dota7034ConnectedPlayer &connected_player : request.connected_players) {
            if (!connected_player.has_steam_id || connected_player.steam_id == 0ull)
                continue;
            const uint32 previous_owner_hero_id = GBE_local_lobby.owner_hero_id;
            if (GBE_SetDotaLobbyMemberRuntimeState(connected_player.steam_id, true, connected_player.hero_id, connected_player.has_hero_id)) {
                GBE_PublishSharedDotaLobbyState("7034_connected_player");
                GBE_GC_DebugLog(
                    "GC_DOTA_DIRECT",
                    "marked connected player from 7034 steam_id=%llu hero_id=%u has_hero=%u source_job=%llu state=%u game_state=%u",
                    static_cast<unsigned long long>(connected_player.steam_id),
                    connected_player.hero_id,
                    connected_player.has_hero_id ? 1u : 0u,
                    static_cast<unsigned long long>(source_job),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state
                );
            }
            if (connected_player.steam_id == owner_steam_id && connected_player.has_hero_id && connected_player.hero_id != 0u && previous_owner_hero_id != GBE_local_lobby.owner_hero_id)
                owner_hero_updated_from_7034 = true;
        }

        if (custom_game_launch) {
            GBE_LocalLobby refreshed_lobby{};
            GBE_CaptureCurrentDotaLobbyState("7034_custom_runtime_member_refresh", refreshed_lobby, false);
            if (GBE_NormalizeDotaArcadeLobbyMemberSlots(GBE_local_lobby))
                GBE_PublishSharedDotaLobbyState("7034_custom_runtime_slot_normalize");
        }

        if (owner_hero_updated_from_7034) {
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "updated owner hero from 7034 request hero_id=%u source_job=%llu state=%u game_state=%u",
                GBE_local_lobby.owner_hero_id,
                static_cast<unsigned long long>(source_job),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state
            );

            GBE_HandleDotaDirectOwnerHeroKnownEquipReplay(owner_steam_id, source_job);
        }

        GBE_HandleDotaDirect7034DisconnectedPlayers(request.disconnected_players, source_job);

        if (GBE_local_lobby.state == 1u &&
            GBE_local_lobby.game_state == 0u &&
            GBE_local_lobby.launch_phase >= GBE_kDotaLaunchPhaseSetupSynced &&
            GBE_local_lobby.launch_4511_seen) {
            if (GBE_TryAdvanceDotaLaunchToRun("runtime packet after matched 4511/7034", request_emsg, source_job, "7034_launch_run_after_4511"))
                queued_runtime_lobby_update = true;
        }

        if (GBE_local_lobby.state == 1u && GBE_local_lobby.game_state == 0u) {
            const std::string request_summary = gbe::proto_wire::format_dota7034_summary(body, body_size);
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "consumed req=%u source_job=%llu note=prelaunch 7034 observed before RUN apply active=%u lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu launch_phase=%s summary=%s",
                request_emsg,
                static_cast<unsigned long long>(source_job),
                GBE_local_lobby.active ? 1u : 0u,
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state,
                static_cast<unsigned long long>(GBE_local_lobby.match_id),
                static_cast<unsigned long long>(GBE_local_lobby.server_id),
                GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase),
                request_summary.c_str()
            );
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "consumed req=%u source_job=%llu note=runtime 7034 active=%u lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu summary=%s",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            GBE_local_lobby.active ? 1u : 0u,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id),
            gbe::proto_wire::format_dota7034_summary(body, body_size).c_str()
        );

        GBE_HandleDotaDirect7034RuntimeUpdates(request_emsg, body, body_size, request, custom_game_launch, source_job, queued_runtime_lobby_update);

        if (custom_game_launch)
            return true;
    }

    const GBE_Dota7034RequestShape request_shape = gbe::proto_wire::parse_dota7034_request_shape(body, body_size);
    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "parsed req=%u source_job=%llu body_size=%zu summary=%s",
        request_emsg,
        static_cast<unsigned long long>(source_job),
        body_size,
        gbe::proto_wire::format_dota7034_summary(body, body_size).c_str()
    );

    return GBE_HandleDotaDirect7034Response(request_emsg, request_shape, body, body_size, has_source_job, source_job);
}


void Steam_Game_Coordinator::GBE_HandleDotaDirectOwnerHeroKnownEquipReplay(
    uint64 owner_steam_id,
    uint64 source_job)
{
    if (!is_server || gc_profile != GC_PROFILE_DOTA2)
        return;

    Steam_Client *steam_client = get_steam_client();
    Steam_Game_Coordinator *client_gc = steam_client ? steam_client->steam_game_coordinator : nullptr;
    if (!client_gc || owner_steam_id == 0ull)
        return;

    const CSteamID owner_id(owner_steam_id);
    const auto &client_items = client_gc->get_items();
    if (GBE_PushDotaPlayerEquippedItemsCacheToGC(this, owner_id, client_items, true, "7034_owner_hero_known_server")) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replayed host equipped items to server GC after owner hero became known: steam64=%llu hero_id=%u source_job=%llu",
            static_cast<unsigned long long>(owner_steam_id),
            GBE_local_lobby.owner_hero_id,
            static_cast<unsigned long long>(source_job)
        );
    }
}


void Steam_Game_Coordinator::GBE_HandleDotaDirect7034DisconnectedPlayers(
    const std::vector<GBE_Dota7034DisconnectedPlayer> &disconnected_players,
    uint64 source_job)
{
    for (const GBE_Dota7034DisconnectedPlayer &disconnected_player : disconnected_players) {
        if (!disconnected_player.has_steam_id || disconnected_player.steam_id == 0ull)
            continue;
        if (GBE_SetDotaLobbyMemberRuntimeState(disconnected_player.steam_id, false, 0u, false)) {
            GBE_PublishSharedDotaLobbyState("7034_disconnected_player");
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "marked disconnected player from 7034 steam_id=%llu source_job=%llu state=%u game_state=%u",
                static_cast<unsigned long long>(disconnected_player.steam_id),
                static_cast<unsigned long long>(source_job),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state
            );
        }
    }
}


void Steam_Game_Coordinator::GBE_HandleDotaDirect7034RuntimeUpdates(
    uint32 request_emsg,
    const uint8 *body,
    size_t body_size,
    const GBE_Dota7034RuntimeRequest &request,
    bool custom_game_launch,
    uint64 source_job,
    bool &queued_runtime_lobby_update)
{
    const bool request_advances_to_hero_selection = request.has_game_state && request.game_state >= 2u;

    const gbe::dota_lobby_state::LaunchLifecycleTransitionDecision runtime_game_state =
        gbe::dota_lobby_state::compute_runtime_game_state_transition(
            GBE_local_lobby,
            custom_game_launch,
            request.has_game_state,
            request.game_state,
            GBE_kDotaLaunchPhaseRunQueued,
            "custom game 7034 game_state");
    if (runtime_game_state.queue_runtime_lobby_update) {
        if (GBE_TryQueueDotaRuntimeLobbyDetailsUpdate(
                runtime_game_state.reason.c_str(),
                request_emsg,
                source_job,
                runtime_game_state.next_state,
                runtime_game_state.next_game_state,
                runtime_game_state.runtime_update_delay))
            queued_runtime_lobby_update = true;
    }

    GBE_HandleDotaDirect7034WaitForPlayers(request_emsg, body, body_size, custom_game_launch, request_advances_to_hero_selection, source_job, queued_runtime_lobby_update);

    if (!custom_game_launch && GBE_local_lobby.state == 2u && GBE_local_lobby.game_state == 0u &&
            GBE_local_lobby.launch_phase >= GBE_kDotaLaunchPhaseRunQueued) {
        if (GBE_TryQueueDotaPrelaunch021("runtime wait_for_players after 7034", request_emsg, source_job))
            queued_runtime_lobby_update = true;
    }

    GBE_HandleDotaDirect7034StrategyTime(request_emsg, body, body_size, request, custom_game_launch, source_job, queued_runtime_lobby_update);

    GBE_HandleDotaDirect7034LaunchPoll(request_emsg, source_job, queued_runtime_lobby_update);

    if (queued_runtime_lobby_update && !custom_game_launch) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "continuing req=%u source_job=%llu with connected players reply after runtime 26 updates state=%u game_state=%u",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state
        );
    }

    if (custom_game_launch) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "consumed req=%u source_job=%llu note=custom game 7034 handled by runtime 26 update state=%u game_state=%u summary=%s",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            gbe::proto_wire::format_dota7034_summary(body, body_size).c_str()
        );
    }
}


void Steam_Game_Coordinator::GBE_HandleDotaDirect7034StrategyTime(
    uint32 request_emsg,
    const uint8 *body,
    size_t body_size,
    const GBE_Dota7034RuntimeRequest &request,
    bool custom_game_launch,
    uint64 source_job,
    bool &queued_runtime_lobby_update)
{
    GBE_HandleDotaDirect7034StrategyTimeFallback(request_emsg, body, body_size, request, custom_game_launch, source_job, queued_runtime_lobby_update);
    GBE_HandleDotaDirect7034StrategyTimePreserve(request_emsg, source_job, queued_runtime_lobby_update);
}


void Steam_Game_Coordinator::GBE_HandleDotaDirect7034StrategyTimeFallback(
    uint32 request_emsg,
    const uint8 *body,
    size_t body_size,
    const GBE_Dota7034RuntimeRequest &request,
    bool custom_game_launch,
    uint64 source_job,
    bool &queued_runtime_lobby_update)
{
    if (!custom_game_launch &&
        GBE_local_lobby.state == 2u &&
        GBE_local_lobby.game_state == 2u &&
        request.has_game_state && request.game_state == 2u &&
        request.has_send_reason && request.send_reason == 2u &&
        GBE_local_lobby.game_mode == 1u) {
        uint32 remote_count = 0u;
        uint32 connected_remote_count = 0u;
        if (GBE_ShouldHoldDotaLanLaunchForRemoteMembers(3u, &remote_count, &connected_remote_count)) {
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "holding LAN AP fallback strategy_time req=%u source_job=%llu remote_connected=%u remote_total=%u state=%u game_state=%u summary=%s",
                request_emsg,
                static_cast<unsigned long long>(source_job),
                connected_remote_count,
                remote_count,
                GBE_local_lobby.state,
                GBE_local_lobby.game_state,
                gbe::proto_wire::format_dota7034_summary(body, body_size).c_str()
            );
        } else if (GBE_TryQueueDotaRuntimeLobbyDetailsUpdate("runtime AP hero_selection fallback strategy_time", request_emsg, source_job, 2u, 3u, 1.0)) {
            queued_runtime_lobby_update = true;
        }
    }
}


void Steam_Game_Coordinator::GBE_HandleDotaDirect7034StrategyTimePreserve(
    uint32 request_emsg,
    uint64 source_job,
    bool &queued_runtime_lobby_update)
{
    if (GBE_local_lobby.state == 2u && GBE_local_lobby.game_state == 3u) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "consumed req=%u source_job=%llu note=skip synthetic official 032 follow-up and preserve strategy_time state=%u game_state=%u",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state
        );
        queued_runtime_lobby_update = true;
    }
}


bool Steam_Game_Coordinator::GBE_HandleDotaDirect7034Response(
    uint32 request_emsg,
    const GBE_Dota7034RequestShape &request_shape,
    const uint8 *body,
    size_t body_size,
    bool has_source_job,
    uint64 source_job)
{
    // [FIX] Re-push host equipped items when game_state reaches TEAM_SHOWCASE (4).
    // On a listen server the login CacheSubscribed establishes the host's SO cache
    // with 27k items (no equipped_state) in the shared cache. The equip-forward
    // CacheSubscribed arrives during STRATEGY_TIME before hero spawn, so the server
    // engine sees [in cache] and does not create wearables. By re-pushing at
    // TEAM_SHOWCASE (when the server engine is about to spawn heroes), we give it
    // a fresh CacheSubscribed with only equipped items so wearables are created.
    if (is_server && !GBE_HasPushedDotaHostShowcaseEquip() &&
        request_shape.has_game_state && request_shape.game_state >= 4u &&
        GBE_local_lobby.active && GBE_local_lobby.state == 2u) {
        Steam_Client *steam_client_ptr = get_steam_client();
        Steam_Game_Coordinator *client_gc_ptr = steam_client_ptr ? steam_client_ptr->steam_game_coordinator : nullptr;
        const uint64 owner_steam64 = GBE_GetDotaLobbyOwnerSteamId();
        if (client_gc_ptr && owner_steam64 != 0ull) {
            const CSteamID owner_steam_id(owner_steam64);
            const auto &client_items = client_gc_ptr->get_items();
            if (GBE_PushDotaPlayerEquippedItemsCacheToGC(this, owner_steam_id, client_items, true, "7034_showcase_host_equip_repush")) {
                GBE_MarkDotaHostShowcaseEquipPushed();
                GBE_GC_DebugLog(
                    "GC_DOTA_DIRECT",
                    "re-pushed host equipped items at TEAM_SHOWCASE: steam64=%llu request_game_state=%u lobby_game_state=%u",
                    static_cast<unsigned long long>(owner_steam64),
                    request_shape.game_state,
                    GBE_local_lobby.game_state
                );
            }
        }
    }

    std::string response_message;
    const bool built_response = GBE_AdaptDota7034ConnectedPlayersResponsePayload(
        GBE_GetDotaLobbyOwnerSteamId(),
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot,
        GBE_local_lobby.members,
        request_shape,
        true,
        true,
        has_source_job,
        source_job,
        response_message);
    if (!built_response) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, 7034u);
        return true;
    }

    size_t response_body_offset = 8u;
    if (response_message.size() >= 8u) {
        uint32 response_header_length = 0;
        std::memcpy(&response_header_length, response_message.data() + 4, sizeof(response_header_length));
        response_body_offset += response_header_length;
    }
    const uint8 *response_body = response_body_offset <= response_message.size()
        ? reinterpret_cast<const uint8 *>(response_message.data() + response_body_offset)
        : nullptr;
    const size_t response_body_size = response_body_offset <= response_message.size()
        ? (response_message.size() - response_body_offset)
        : 0u;

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=%u resp=%u source_job=%llu size=%zu note=7034 connected players game_state=%u team=%u slot=%u summary=%s",
        request_emsg,
        7034u,
        static_cast<unsigned long long>(source_job),
        response_message.size(),
        GBE_local_lobby.game_state,
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot,
        gbe::proto_wire::format_dota7034_summary(response_body, response_body_size).c_str()
    );
    push_incoming_now(7034u | GBE_kProtoMask, response_message);
    return true;
}


void Steam_Game_Coordinator::GBE_HandleDotaDirect7034LaunchPoll(
    uint32 request_emsg,
    uint64 source_job,
    bool &queued_runtime_lobby_update)
{
    const gbe::dota_lobby_state::LaunchLifecycleTransitionDecision launch_poll =
        gbe::dota_lobby_state::compute_launch_poll_transition(
            GBE_local_lobby,
            "7034_launch_poll");
    if (!launch_poll.send_details_update)
        return;

    if (GBE_SendDotaPracticeLobbyDetailsUpdate(false, nullptr, launch_poll.reason.c_str())) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu note=7034 direct poll uses runtime 26 fallback state=%u game_state=%u",
            request_emsg,
            GBE_kDotaPracticeLobbyDetailsUpdate,
            static_cast<unsigned long long>(source_job),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state
        );
        queued_runtime_lobby_update = true;
    }
}


void Steam_Game_Coordinator::GBE_HandleDotaDirect7034WaitForPlayers(
    uint32 request_emsg,
    const uint8 *body,
    size_t body_size,
    bool custom_game_launch,
    bool request_advances_to_hero_selection,
    uint64 source_job,
    bool &queued_runtime_lobby_update)
{
    if (custom_game_launch || GBE_local_lobby.state != 2u || GBE_local_lobby.game_state != 1u)
        return;

    if (!GBE_TryQueueDotaRuntimeLobbyDetailsUpdate("runtime packet after 8870/7034 wait_for_players", request_emsg, source_job, 2u, 1u))
        return;

    queued_runtime_lobby_update = true;
    if (!request_advances_to_hero_selection)
        return;

    uint32 remote_count = 0u;
    uint32 connected_remote_count = 0u;
    if (GBE_ShouldHoldDotaLanLaunchForRemoteMembers(2u, &remote_count, &connected_remote_count)) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "holding LAN hero_selection req=%u source_job=%llu remote_connected=%u remote_total=%u state=%u game_state=%u summary=%s",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            connected_remote_count,
            remote_count,
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            gbe::proto_wire::format_dota7034_summary(body, body_size).c_str()
        );
    } else if (GBE_TryQueueDotaRuntimeLobbyDetailsUpdate("runtime packet after 8870/7034 hero_selection", request_emsg, source_job, 2u, 2u)) {
        queued_runtime_lobby_update = true;
    }
}


bool Steam_Game_Coordinator::GBE_HandleDotaCustomGameReadyUpRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job)
{
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Received direct 7070 source_job=%llu body_size=%zu body_prefix=%s",
        static_cast<unsigned long long>(source_job),
        body_size,
        gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(body), body_size, 48).c_str()
    );

    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game)) {
        const GBE_Dota7070ReadyUpRequest request = gbe::proto_wire::parse_dota7070_ready_up_request(body, body_size);
        std::string response_7170;
        if (gbe::gc_message::build_dota_ready_up_status_payload(has_source_job, source_job, GBE_local_lobby.lobby_id, 0u, request.ready_state != 0u ? request.ready_state : 1u, response_7170))
            push_incoming_now(7170u | GBE_kProtoMask, response_7170);

        const gbe::dota_lobby_state::LaunchLifecycleTransitionDecision ready_up =
            gbe::dota_lobby_state::compute_custom_game_ready_up_transition(
                GBE_local_lobby,
                request.ready_state,
                GBE_kDotaLaunchPhaseRunQueued,
                "7070_custom_game_ready_up_run_ack");
        if (ready_up.apply_lobby_state) {
            GBE_local_lobby.state = ready_up.next_state;
            GBE_local_lobby.game_state = ready_up.next_game_state;
            if (ready_up.publish_shared_state)
                GBE_PublishSharedDotaLobbyState(ready_up.reason.c_str());
            if (ready_up.send_details_update)
                GBE_SendDotaPracticeLobbyDetailsUpdate(false, nullptr, ready_up.reason.c_str());
        }
    }
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaCustomGameStartedLoadingRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job)
{
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Received direct 8052 source_job=%llu body_size=%zu body_prefix=%s",
        static_cast<unsigned long long>(source_job),
        body_size,
        gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(body), body_size, 48).c_str()
    );

    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game)) {
        const GBE_Dota8052StartedLoadingRequest request = gbe::proto_wire::parse_dota8052_started_loading_request(body, body_size);

        if (request.lobby_id == 0 || request.lobby_id == GBE_local_lobby.lobby_id) {
            if (request.custom_game_id != 0)
                GBE_local_lobby.custom_game.game_id = request.custom_game_id;
            if (request.start_time != 0)
                GBE_local_lobby.game_start_time = static_cast<uint32>(request.start_time);
            const gbe::dota_lobby_state::LaunchLifecycleTransitionDecision started_loading =
                gbe::dota_lobby_state::compute_custom_game_started_loading_transition(
                    GBE_local_lobby,
                    true,
                    GBE_kDotaLaunchPhaseSetupSynced,
                    GBE_kDotaLaunchPhaseRunQueued,
                    "8052_started_loading");
            gbe::dota_custom_game_lifecycle::ExecutionContext execution{};
            execution.transition = started_loading;
            execution.trigger_emsg = 8052u;
            execution.source_job = source_job;
            execution.runtime_update_note = "custom game 8052 started loading";
            const bool advanced_to_run = GBE_ExecuteDotaCustomGameLifecycleTransition(execution);
            if (advanced_to_run) {
                GBE_GC_DebugLog(
                    "GC_DOTA_LOBBY",
                    "[LOBBY] Advanced custom game RUN after 8052 lobby_id=%llu custom_game_id=%llu start_time=%llu state=%u game_state=%u",
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    static_cast<unsigned long long>(request.custom_game_id),
                    static_cast<unsigned long long>(request.start_time),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state
                );
            }
        }
    }
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaCustomGameFinishedLoadingRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job)
{
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Received direct 8053 source_job=%llu body_size=%zu body_prefix=%s",
        static_cast<unsigned long long>(source_job),
        body_size,
        gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(body), body_size, 48).c_str()
    );

    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game)) {
        const GBE_Dota8053FinishedLoadingRequest request = gbe::proto_wire::parse_dota8053_finished_loading_request(body, body_size);

        if (request.lobby_id == 0 || request.lobby_id == GBE_local_lobby.lobby_id) {
            const bool load_failed = gbe::proto_wire::dota8053_indicates_load_failure(request.result_code, request.result_text);
            const char *reason = load_failed ? "8053_load_failed" : "8053_finished_loading";
            const gbe::dota_lobby_state::LaunchLifecycleTransitionDecision finished_loading =
                gbe::dota_lobby_state::compute_custom_game_finished_loading_transition(
                    GBE_local_lobby,
                    true,
                    load_failed,
                    GBE_kDotaLaunchPhaseRunQueued,
                    GBE_kDotaLaunchPhaseLoaded,
                    reason);
            gbe::dota_custom_game_lifecycle::ExecutionContext execution{};
            execution.transition = finished_loading;
            execution.trigger_emsg = 8053u;
            execution.source_job = source_job;
            execution.update_local_member_runtime = !load_failed;
            execution.publish_local_member_data = !load_failed;
            GBE_ExecuteDotaCustomGameLifecycleTransition(execution);
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Applied direct 8053 lobby_id=%llu loading_duration=%llu result_code=%llu signon_states=%llu load_failed=%u result_text=%s",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(request.loading_duration),
                static_cast<unsigned long long>(request.result_code),
                static_cast<unsigned long long>(request.signon_states),
                load_failed ? 1u : 0u,
                request.result_text.c_str()
            );
        }
    }
    return true;
}
