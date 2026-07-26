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

// Split from gbe_dota_lobby_handlers.cpp (stage D.10.1). Behavior unchanged.

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_request_router.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_dota_lifecycle_state_machine.h"
#include "gbe_dota_lobby_state_store.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_gc_diagnostics.h"
#include "gbe_dota_lobby_handler_helpers.h"
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

using GBE_DotaPracticeLobbyDetailsRequest = gbe::proto_wire::DotaPracticeLobbyDetailsRequest;
using GBE_DotaPracticeLobbyCreateRequest = gbe::proto_wire::DotaPracticeLobbyCreateRequest;
using GBE_DotaPracticeLobbyJoinRequest = gbe::proto_wire::DotaPracticeLobbyJoinRequest;
using GBE_DotaInviteToLobbyRequest = gbe::proto_wire::DotaInviteToLobbyRequest;
using GBE_DotaLobbyInviteResponseRequest = gbe::proto_wire::DotaLobbyInviteResponseRequest;
using GBE_DotaPracticeLobbySetTeamSlotRequest = gbe::proto_wire::DotaPracticeLobbySetTeamSlotRequest;
using GBE_DotaPracticeLobbyKickRequest = gbe::proto_wire::DotaPracticeLobbyKickRequest;

bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyJoinRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw, bool send_join_response)
{
    GBE_DotaPracticeLobbyJoinRequest request{};
    if (!gbe::proto_wire::parse_dota_practice_lobby_join_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7044 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str());
        return true;
    }

    CSteamID matched_generic_lobby_id = k_steamIDNil;
    GBE_LocalLobby matched_lobby{};
    bool matched_generic_lobby = request.has_lobby_id && request.lobby_id != 0 &&
        GBE_FindDotaGenericLobbyByDotaLobbyId(request.lobby_id, matched_generic_lobby_id, &matched_lobby, "7044_join");

    Steam_Client *steam_client = get_steam_client();
    if (!matched_generic_lobby && steam_client && steam_client->steam_matchmaking && request.has_lobby_id && request.lobby_id != 0) {
        matched_generic_lobby_id = steam_client->steam_matchmaking->FindLobbyByDotaLobbyIdForInvite(
            request.lobby_id,
            GBE_kDotaGenericLobbyMarkerKey,
            GBE_kDotaGenericLobbyMarkerValue,
            GBE_kDotaGenericLobbyDotaLobbyIdKey);
        if (matched_generic_lobby_id.IsLobby()) {
            steam_client->steam_matchmaking->JoinLobby(matched_generic_lobby_id);
            steam_client->steam_matchmaking->RefreshLobbyCallbacksForDota();
            matched_generic_lobby = GBE_FindDotaGenericLobbyByDotaLobbyId(request.lobby_id, matched_generic_lobby_id, &matched_lobby, "7044_join_local_find");
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] 7044 local find fallback lobby_id=%llu generic_lobby_id=%llu matched=%u members=%zu",
                static_cast<unsigned long long>(request.lobby_id),
                static_cast<unsigned long long>(matched_generic_lobby_id.ConvertToUint64()),
                matched_generic_lobby ? 1u : 0u,
                matched_lobby.members.size()
            );
        }
    }

    if (!matched_generic_lobby && (!request.has_lobby_id || request.lobby_id == 0) && (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0))
        matched_lobby.lobby_id = GBE_GenerateDotaLobbyId();

    gbe::dota_lobby_flow::JoinLobbyContext join_context{};
    join_context.current_lobby = GBE_local_lobby;
    join_context.request_has_lobby_id = request.has_lobby_id;
    join_context.request_lobby_id = request.lobby_id;
    join_context.request_has_pass_key = request.has_pass_key;
    join_context.request_pass_key = request.pass_key;
    join_context.matched_generic_lobby = matched_generic_lobby;
    join_context.matched_lobby = matched_lobby;
    join_context.matched_generic_lobby_id = matched_generic_lobby_id.ConvertToUint64();
    join_context.send_join_response = send_join_response;
    join_context.local_steam_id = settings->get_local_steam_id().ConvertToUint64();
    join_context.local_account_id = settings->get_local_steam_id().GetAccountID();
    join_context.local_name = std::string(settings->get_local_name());
    join_context.good_guys_team = GBE_kDotaTeamGoodGuys;
    join_context.player_pool_team = GBE_kDotaTeamPlayerPool;

    const gbe::dota_lobby_state::JoinLobbyMergePlan join_plan = gbe::dota_lobby_flow::join_lobby_merge_plan_from_context(join_context);
    gbe::dota_lifecycle_state_machine::MachineState machine_state{};
    machine_state.generation = GBE_CurrentDotaLobbyGeneration();
    const auto generation_boundary = gbe::dota_lifecycle_state_machine::transition_generation_boundary(
        machine_state,
        { gbe::dota_lifecycle_state_machine::EventKind::Join,
          gbe::dota_lifecycle_state_machine::transport_source(wrapped),
          GBE_kDotaPracticeLobbyJoin,
          machine_state.generation });
    if (!generation_boundary.accepted() || !generation_boundary.effects.contains(
            gbe::dota_lifecycle_state_machine::EffectKind::GenerationAdvanced))
        return true;
    if (GBE_AdvanceDotaLobbyGeneration(gbe::dota_lobby_generation::Boundary::Join, "7044_join") == GBE_DotaGenerationAdvanceResult::Exhausted)
        return true;
    GBE_local_lobby = join_plan.lobby;
    gbe::dota_lobby_state::apply_lobby_generation(GBE_local_lobby, GBE_CurrentDotaLobbyGeneration());

    const GBE_DotaActionList join_actions = gbe::dota_lobby_flow::join_lobby_action_list(
        gbe::dota_lobby_flow::join_lobby_action_plan_from_context(join_context),
        wrapped);
    std::size_t join_action_index = 0u;

    if (matched_generic_lobby) {
        for (; join_action_index < join_actions.size(); ++join_action_index) {
            const GBE_DotaAction &action = join_actions[join_action_index];
            if (action.type == GBE_DotaActionType::LobbyLocalMemberData)
                break;
            switch (action.type) {
                case GBE_DotaActionType::GenericLobbyJoin:
                    if (steam_client && steam_client->steam_matchmaking)
                        steam_client->steam_matchmaking->JoinLobby(CSteamID(static_cast<uint64>(action.item_id)));
                    break;
                case GBE_DotaActionType::SettingsLobbySync:
                    GBE_SyncSettingsLobbyFromGenericLobby(action.reason.c_str());
                    break;
                default:
                    break;
            }
        }
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] 7044 matched generic custom metadata lobby_id=%llu generic_lobby_id=%llu custom_game_id=%llu custom_mode=%s custom_map=%s state=%u game_state=%u match_id=%llu server_id=%llu connect=%s",
            static_cast<unsigned long long>(matched_lobby.lobby_id),
            static_cast<unsigned long long>(matched_lobby.generic_lobby_id),
            static_cast<unsigned long long>(matched_lobby.custom_game.game_id),
            matched_lobby.custom_game.mode.c_str(),
            matched_lobby.custom_game.map_name.c_str(),
            matched_lobby.state,
            matched_lobby.game_state,
            static_cast<unsigned long long>(matched_lobby.match_id),
            static_cast<unsigned long long>(matched_lobby.server_id),
            matched_lobby.connect.c_str());
    }

    const GBE_DotaLobbyMemberState &local_member = join_plan.local_member;

    for (; join_action_index < join_actions.size(); ++join_action_index) {
        const GBE_DotaAction &action = join_actions[join_action_index];
        if (action.type == GBE_DotaActionType::LobbyCacheSubscriptionRecord)
            break;
        switch (action.type) {
            case GBE_DotaActionType::LobbyLocalMemberData:
                GBE_PublishDotaPracticeLobbyLocalMemberData(action.reason.c_str());
                break;
            case GBE_DotaActionType::LobbySnapshotRefresh:
                GBE_PublishSharedDotaLobbyState(action.reason.c_str());
                break;
            default:
                break;
        }
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Preparing 7044 cache update LobbyID=%llu owner_steam=%llu local_steam=%llu members=%zu matched_generic_members=%zu custom_game_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu connect=%s",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.owner_steam_id),
        static_cast<unsigned long long>(local_member.steam_id),
        GBE_local_lobby.members.size(),
        matched_lobby.members.size(),
        static_cast<unsigned long long>(GBE_local_lobby.custom_game.game_id),
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id),
        GBE_local_lobby.connect.c_str()
    );

    std::string response_24;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed(GBE_local_lobby, GBE_GetDotaLobbyOwnerName(), response_24)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 24 cache update for 7044 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    std::string response_7113;
    if (send_join_response) {
        if (!gbe::gc_message::build_dota_practice_lobby_join_response_payload(has_request_job, request_job_id, 0u, response_7113)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7113 join response LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
            return true;
        }
    }

    gbe::dota_gc_router::DotaGcOutboundMessage outbound_24{};
    gbe::dota_gc_router::DotaGcOutboundMessage outbound_7113{};
    if (!gbe::dota_gc_router::build_outbound_message(
            GBE_kDotaCacheSubscribed,
            response_24,
            wrapped,
            outer_session_field_raw,
            settings->get_local_steam_id().ConvertToUint64(),
            GBE_kEMsgClientFromGC,
            GBE_kDotaAppId,
            outbound_24) ||
        (send_join_response && !gbe::dota_gc_router::build_outbound_message(
            GBE_kDotaPracticeLobbyJoinResponse,
            response_7113,
            wrapped,
            outer_session_field_raw,
            settings->get_local_steam_id().ConvertToUint64(),
            GBE_kEMsgClientFromGC,
            GBE_kDotaAppId,
            outbound_7113))) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building outbound 7044 responses LobbyID=%llu wrapped=%d", static_cast<unsigned long long>(GBE_local_lobby.lobby_id), wrapped ? 1 : 0);
        return true;
    }

    for (; join_action_index < join_actions.size(); ++join_action_index) {
        const GBE_DotaAction &action = join_actions[join_action_index];
        if (action.type == GBE_DotaActionType::PushIncomingNow &&
                action.emsg == (GBE_kDotaPracticeLobbyJoinResponse | GBE_kProtoMask))
            break;
        switch (action.type) {
            case GBE_DotaActionType::LobbyCacheSubscriptionRecord:
                GBE_RecordDotaLobbyCacheSubscriptionState(response_24, action.reason.c_str());
                break;
            case GBE_DotaActionType::PushIncomingNow:
                if (action.emsg == (GBE_kDotaCacheSubscribed | GBE_kProtoMask))
                    push_incoming_now(outbound_24.emsg, outbound_24.payload);
                break;
            default:
                break;
        }
    }
    GBE_LogDotaResponsePacket("7044_join_24", GBE_kDotaCacheSubscribed, wrapped, response_24, outbound_24.payload, GBE_local_lobby.lobby_id, GBE_local_lobby.state, GBE_local_lobby.game_state);
    if (join_action_index < join_actions.size()) {
        const GBE_DotaAction &action = join_actions[join_action_index];
        if (action.type == GBE_DotaActionType::PushIncomingNow &&
                action.emsg == (GBE_kDotaPracticeLobbyJoinResponse | GBE_kProtoMask)) {
            push_incoming_now(outbound_7113.emsg, outbound_7113.payload);
            ++join_action_index;
        }
    }
    if (send_join_response)
        GBE_LogDotaResponsePacket("7044_join_7113", GBE_kDotaPracticeLobbyJoinResponse, wrapped, response_7113, outbound_7113.payload, GBE_local_lobby.lobby_id, GBE_local_lobby.state, GBE_local_lobby.game_state);

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Joined practice lobby via 7044 LobbyID=%llu generic_lobby_id=%llu matched_generic=%u request_job=%llu has_job=%u pass_len=%zu wrapped=%d",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        matched_generic_lobby ? 1u : 0u,
        static_cast<unsigned long long>(request_job_id),
        has_request_job ? 1u : 0u,
        GBE_local_lobby.pass_key.size(),
        wrapped ? 1 : 0);
    return true;
}
