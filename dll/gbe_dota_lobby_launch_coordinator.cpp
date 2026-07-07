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

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_request_router.h"
#include "gbe_proto_buf_header.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_custom_lobby_http.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_gc_wire.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_gc_config.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_gc_internal.h"
#include <atomic>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <sstream>
#include <iomanip>
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

// --- List X: launch/teardown-only static symbols (moved from steam_game_coordinator.cpp) ---

static constexpr const char *GBE_kDotaAbandonPersonaStatePrivateLobbyNoLobbyHex =
    "fe0200800f00000009911ddf050100100110c9dbfdd20408dfe60112d80209911ddf0501001001100118ba04300138017a0a636c6f7665726c6f7665c9010000000000000000fa01140000000000000000000000000000000000000000e802a6c7dacf06f00281c8dacf06f802a6c7dacf06ba0300c1033a02000000000000e20300ba04200a06737461747573121623444f54415f52505f505249564154455f4c4f424259ba04270a0d737465616d5f646973706c6179121623444f54415f52505f505249564154455f4c4f424259ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131ba041e0a057061727479121570617274795f73746174653a20494e5f4d41544348c1040000000000000000c9040000000000000000f80400800500880500980501";


static uint64 GBE_GenerateDotaPostGameChatChannelId()
{
    std::random_device device;
    std::mt19937_64 generator(
        (static_cast<uint64>(device()) << 32) ^
        static_cast<uint64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );

    const uint64 candidate = 0x62F000ull + (generator() & 0x0000000000000FFFull);
    return candidate != 0 ? candidate : 0x62FEB8ull;
}


// --- Lobby launch/teardown flow member functions (moved from steam_game_coordinator.cpp) ---

bool Steam_Game_Coordinator::GBE_TryQueueDotaPrelaunch021(const char *note, uint32 trigger_emsg, uint64 source_job)
{
    GBE_LocalLobby wait_for_players_lobby = GBE_local_lobby;
    wait_for_players_lobby.state = 2u;
    wait_for_players_lobby.game_state = 1u;

    std::string wait_for_players_message;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(wait_for_players_lobby, GBE_local_lobby.owner_name, wait_for_players_message, is_server))
        return false;

    push_incoming_now(
        GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask,
        wait_for_players_message,
        true,
        wait_for_players_lobby.state,
        wait_for_players_lobby.game_state);
    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=%u resp=%u source_job=%llu size=%zu note=%s apply_state=%u apply_game_state=%u source=runtime",
        trigger_emsg,
        GBE_kDotaPracticeLobbyDetailsUpdate,
        static_cast<unsigned long long>(source_job),
        wait_for_players_message.size(),
        note ? note : "unknown",
        wait_for_players_lobby.state,
        wait_for_players_lobby.game_state
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_SetDotaLobbyMemberConnected(uint64 steam_id, bool connected)
{
    if (steam_id == 0ull)
        return false;

    bool changed = false;
    if (steam_id == GBE_local_lobby.owner_steam_id && GBE_local_lobby.owner_connected != connected) {
        GBE_local_lobby.owner_connected = connected;
        changed = true;
    }

    const bool has_custom_game = gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game);
    const bool should_mark_leaver =
        GBE_local_lobby.state == 2u &&
        GBE_local_lobby.game_state >= 1u &&
        GBE_local_lobby.match_id != 0ull;
    changed = gbe::dota_lobby_flow::set_lobby_member_connected(
        GBE_local_lobby.members,
        steam_id,
        CSteamID((uint64)steam_id).GetAccountID(),
        connected,
        has_custom_game,
        should_mark_leaver,
        GBE_local_lobby.owner_steam_id,
        GBE_local_lobby.owner_slot,
        GBE_kDotaTeamGoodGuys,
        GBE_kDotaTeamPlayerPool) || changed;

    return changed;
}


bool Steam_Game_Coordinator::GBE_SetDotaLobbyMemberRuntimeState(uint64 steam_id, bool connected, uint32 hero_id, bool has_hero_id)
{
    if (steam_id == 0ull)
        return false;

    bool changed = GBE_SetDotaLobbyMemberConnected(steam_id, connected);
    if (has_hero_id && hero_id != 0u) {
        if (steam_id == GBE_local_lobby.owner_steam_id && GBE_local_lobby.owner_hero_id != hero_id) {
            GBE_local_lobby.owner_hero_id = hero_id;
            changed = true;
        }
        changed = gbe::dota_lobby_flow::set_lobby_member_hero(GBE_local_lobby.members, steam_id, hero_id) || changed;
    }

    return changed;
}


bool Steam_Game_Coordinator::GBE_ShouldHoldDotaLanLaunchForRemoteMembers(uint32 next_game_state, uint32 *remote_count_out, uint32 *connected_remote_count_out) const
{
    if (remote_count_out)
        *remote_count_out = 0u;
    if (connected_remote_count_out)
        *connected_remote_count_out = 0u;

    if (next_game_state < 2u)
        return false;
    if (!GBE_local_lobby.active || GBE_local_lobby.state != 2u || GBE_local_lobby.match_id == 0ull)
        return false;
    if (gbe::proto_wire::parse_dota_practice_lobby_connect_ipv4(GBE_local_lobby.connect) == 0u && !GBE_local_lobby.lan)
        return false;

    uint32 remote_count = 0u;
    uint32 connected_remote_count = 0u;
    const uint64 owner_steam_id = GBE_GetDotaLobbyOwnerSteamId();
    const bool should_hold = gbe::dota_lobby_flow::should_hold_lan_launch_for_remote_members(GBE_local_lobby.members, owner_steam_id, remote_count, connected_remote_count);

    if (remote_count_out)
        *remote_count_out = remote_count;
    if (connected_remote_count_out)
        *connected_remote_count_out = connected_remote_count;

    return should_hold;
}


bool Steam_Game_Coordinator::GBE_TryQueueDotaRuntimeLobbyDetailsUpdate(const char *note, uint32 trigger_emsg, uint64 source_job, uint32 next_state, uint32 next_game_state, double delay)
{
    if (GBE_ShouldSuppressDotaAbandonedLobby(GBE_local_lobby.lobby_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "skipping runtime lobby update for suppressed abandoned lobby req=%u source_job=%llu note=%s lobby_id=%llu state=%u game_state=%u next_state=%u next_game_state=%u",
            trigger_emsg,
            static_cast<unsigned long long>(source_job),
            note ? note : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            next_state,
            next_game_state
        );
        return false;
    }

    GBE_LocalLobby next_lobby = GBE_local_lobby;
    next_lobby.state = next_state;
    next_lobby.game_state = next_game_state;

    auto queue_details_update = [&](Steam_Game_Coordinator *target) -> bool {
        if (!target || target->gc_profile != GC_PROFILE_DOTA2)
            return false;

        std::string response_message;
        if (!target->GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(next_lobby, target->GBE_GetDotaLobbyOwnerName(), response_message, target->is_server))
            return false;

        if (delay > 0.0) {
            target->push_incoming(
                GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask,
                response_message,
                delay,
                true,
                next_lobby.state,
                next_lobby.game_state);
        } else {
            target->push_incoming_now(
                GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask,
                response_message,
                true,
                next_lobby.state,
                next_lobby.game_state);
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu size=%zu note=%s apply_state=%u apply_game_state=%u delay=%.3f source=runtime target=%p",
            trigger_emsg,
            GBE_kDotaPracticeLobbyDetailsUpdate,
            static_cast<unsigned long long>(source_job),
            response_message.size(),
            note ? note : "unknown",
            next_lobby.state,
            next_lobby.game_state,
            delay,
            static_cast<void *>(target)
        );

        return true;
    };

    if (!queue_details_update(this))
        return false;

    if (is_server && GBE_local_lobby.state == 2u && GBE_local_lobby.game_state >= 1u) {
        if (delay > 0.0) {
            Steam_Client *steam_client = get_steam_client();
            Steam_Game_Coordinator *client_target = steam_client ? steam_client->steam_game_coordinator : nullptr;
            if (client_target && client_target != this)
                queue_details_update(client_target);
        } else {
            GBE_PushDotaLaunchStateToClientPeer(note ? note : "runtime_lobby_update");
        }
    }

    return true;
}


bool Steam_Game_Coordinator::GBE_HasDotaLaunchServerSetupSync() const
{
    return gbe::dota_lobby_state::has_launch_server_setup_sync(GBE_local_lobby);
}


void Steam_Game_Coordinator::GBE_MarkDotaLaunchPhase(uint32 phase, const char *reason)
{
    if (GBE_local_lobby.launch_phase >= phase)
        return;

    const uint32 previous_phase = GBE_local_lobby.launch_phase;
    GBE_local_lobby.launch_phase = phase;
    GBE_PublishSharedDotaLobbyState(reason ? reason : "launch_phase");
    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "advanced launch phase reason=%s lobby_id=%llu state=%u game_state=%u previous=%s next=%s",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        GBE_DescribeDotaLaunchPhase(previous_phase),
        GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase)
    );
}


bool Steam_Game_Coordinator::GBE_TryAdvanceDotaLaunchToRun(const char *note, uint32 trigger_emsg, uint64 source_job, const char *reason, uint32 next_game_state)
{
    const gbe::dota_lobby_state::LaunchRunPlan launch_plan = gbe::dota_lobby_state::compose_launch_run_plan(
        GBE_local_lobby,
        GBE_kDotaLaunchPhaseSetupSynced,
        GBE_kDotaLaunchPhaseRunQueued,
        next_game_state);
    if (!launch_plan.can_advance)
        return false;

    GBE_MarkDotaLaunchPhase(launch_plan.launch_phase, reason ? reason : "launch_run_queued");

    return GBE_TryQueueDotaRuntimeLobbyDetailsUpdate(
        note ? note : "runtime packet after launch run gate",
        trigger_emsg,
        source_job,
        launch_plan.next_state,
        launch_plan.next_game_state);
}


void Steam_Game_Coordinator::GBE_PushDotaLaunchStateToClientPeer(const char *reason)
{
    if (GBE_ShouldSuppressDotaAbandonedLobby(GBE_local_lobby.lobby_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipped pushing launch state from suppressed abandoned lobby reason=%s source=%p is_server=%u lobby_id=%llu",
            reason ? reason : "unknown",
            static_cast<void *>(this),
            is_server ? 1u : 0u,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id)
        );
        return;
    }

    Steam_Game_Coordinator *target = this;
    if (is_server) {
        Steam_Client *steam_client = get_steam_client();
        if (gbe::dota_lobby_flow::should_use_client_peer_for_launch_state_push(
                is_server,
                steam_client && steam_client->steam_game_coordinator))
            target = steam_client->steam_game_coordinator;
    }

    if (!gbe::dota_lobby_flow::is_valid_launch_state_push_target(
            target != nullptr,
            target ? target->is_server : false,
            target ? target->gc_profile == GC_PROFILE_DOTA2 : false))
        return;

    target->GBE_RestoreSharedDotaLobbyState(reason ? reason : "push_launch_state_to_client");

    const GBE_DotaSharedLobbyScalarSnapshot shared_snapshot = GBE_GetSharedDotaLobbyScalarSnapshot();
    if (target->GBE_ShouldSuppressDotaAbandonedLobby(shared_snapshot.lobby_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipped pushing launch state to client for suppressed abandoned lobby reason=%s target=%p lobby_id=%llu",
            reason ? reason : "unknown",
            static_cast<void *>(target),
            static_cast<unsigned long long>(shared_snapshot.lobby_id)
        );
        return;
    }

    GBE_LocalLobby lobby{};
    if (!target->GBE_CaptureCurrentDotaLobbyState(reason ? reason : "push_launch_state_to_client", lobby, false)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipped pushing launch state to client reason=%s target=%p active=0",
            reason ? reason : "unknown",
            static_cast<void *>(target)
        );
        return;
    }

    if (lobby.state != 2u || lobby.game_state < 1u || (lobby.server_id == 0 && lobby.connect.empty())) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipped pushing launch state to client reason=%s target=%p lobby_id=%llu state=%u game_state=%u server_id=%llu",
            reason ? reason : "unknown",
            static_cast<void *>(target),
            static_cast<unsigned long long>(lobby.lobby_id),
            lobby.state,
            lobby.game_state,
            static_cast<unsigned long long>(lobby.server_id)
        );
        return;
    }

    if (target->GBE_GetLastDotaLaunchStatePushedGameState() >= lobby.game_state) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipping duplicate launch state push to client reason=%s target=%p lobby_id=%llu state=%u game_state=%u last_game_state=%u server_id=%llu",
            reason ? reason : "unknown",
            static_cast<void *>(target),
            static_cast<unsigned long long>(lobby.lobby_id),
            lobby.state,
            lobby.game_state,
            target->GBE_GetLastDotaLaunchStatePushedGameState(),
            static_cast<unsigned long long>(lobby.server_id)
        );
        return;
    }

    const uint64 target_local_steam_id = target->settings ? target->settings->get_local_steam_id().ConvertToUint64() : 0ull;
    const bool target_owner_lan_launch = gbe::dota_lobby_flow::should_preserve_server_id_for_launch_state_push_target(
        target_local_steam_id,
        lobby.owner_steam_id,
        lobby.lan,
        lobby.match_id);

    std::string response_24;
    if (!target->GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed(lobby, target->GBE_GetDotaLobbyOwnerName(), response_24, target_owner_lan_launch)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "failed building launch state 24 for client reason=%s target=%p lobby_id=%llu state=%u game_state=%u server_id=%llu",
            reason ? reason : "unknown",
            static_cast<void *>(target),
            static_cast<unsigned long long>(lobby.lobby_id),
            lobby.state,
            lobby.game_state,
            static_cast<unsigned long long>(lobby.server_id)
        );
        return;
    }

    std::string response_26;
    if (!target->GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(lobby, target->GBE_GetDotaLobbyOwnerName(), response_26, target_owner_lan_launch)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "failed building launch state 26 for client reason=%s target=%p lobby_id=%llu state=%u game_state=%u server_id=%llu",
            reason ? reason : "unknown",
            static_cast<void *>(target),
            static_cast<unsigned long long>(lobby.lobby_id),
            lobby.state,
            lobby.game_state,
            static_cast<unsigned long long>(lobby.server_id)
        );
        return;
    }

    target->GBE_RecordDotaLobbyCacheSubscriptionState(response_24, reason ? reason : "push_launch_state_to_client");
    target->push_incoming_now(GBE_kDotaCacheSubscribed | GBE_kProtoMask, response_24);
    target->push_incoming_now(
        GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask,
        response_26,
        true,
        lobby.state,
        lobby.game_state
    );
    target->GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(reason ? reason : "push_launch_state_to_client");
    target->GBE_SetLastDotaLaunchStatePushedGameState(lobby.game_state);

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "pushed launch state to client reason=%s target=%p lobby_id=%llu state=%u game_state=%u server_id=%llu size24=%zu size26=%zu",
        reason ? reason : "unknown",
        static_cast<void *>(target),
        static_cast<unsigned long long>(lobby.lobby_id),
        lobby.state,
        lobby.game_state,
        static_cast<unsigned long long>(lobby.server_id),
        response_24.size(),
        response_26.size()
    );
}


bool Steam_Game_Coordinator::GBE_QueueDotaPostGameTeardown(const char *reason, bool wrapped, const std::string *outer_session_field_raw, bool suppress_previous_chat_channel, bool push_cache_unsubscribed, bool push_postgame_join)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring postgame teardown because no local lobby is active reason=%s", reason ? reason : "unknown");
        return true;
    }

    if (wrapped && !outer_session_field_raw) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for postgame teardown reason=%s LobbyID=%llu", reason ? reason : "unknown", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return false;
    }

    const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    const uint64 pre_postgame_chat_channel_id = suppress_previous_chat_channel ? GBE_local_lobby.chat_channel_id : 0u;

    std::string response_25;
    if (!gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(lobby_id, response_25)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 payload for postgame teardown LobbyID=%llu reason=%s", static_cast<unsigned long long>(lobby_id), reason ? reason : "unknown");
        return false;
    }

    GBE_ResetDotaPracticeLobbyLaunchPeripheralState();

    GBE_local_lobby.state = 3u;
    GBE_local_lobby.game_state = 6u;
    GBE_local_lobby.has_chat_channel = true;
    GBE_local_lobby.chat_channel_id = GBE_GenerateDotaPostGameChatChannelId();
    GBE_local_lobby.chat_channel_name = "PostGame_" + std::to_string(lobby_id);
    GBE_local_lobby.chat_channel_type = 18u;
    GBE_local_lobby.abandon_pre_postgame_chat_channel_id = pre_postgame_chat_channel_id;
    GBE_local_lobby.has_cache_version = false;
    GBE_local_lobby.cache_version = 0;
    GBE_local_lobby.has_cache_service_id = false;
    GBE_local_lobby.cache_service_id = 0;
    GBE_local_lobby.cache_service_list.clear();
    GBE_local_lobby.has_cache_sync_version = false;
    GBE_local_lobby.cache_sync_version = 0;
    GBE_local_lobby.abandon_postgame_active = true;
    GBE_PublishSharedDotaLobbyState(reason ? reason : "postgame_teardown");

    std::string response_7010_postgame;
    if (!gbe::gc_message::build_dota_post_game_join_chat_channel_response_payload(
            steam_id,
            GBE_local_lobby.chat_channel_id,
            GBE_local_lobby.chat_channel_name,
            std::string(settings->get_local_name()),
            response_7010_postgame)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed building postgame 7010 payload LobbyID=%llu channel=%llu reason=%s",
            static_cast<unsigned long long>(lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.chat_channel_id),
            reason ? reason : "unknown"
        );
        return false;
    }

    auto push_reply = [&](const std::string &payload, uint32 direct_emsg, const char *label) -> bool {
        return GBE_PushDotaResponse(direct_emsg, payload, wrapped, outer_session_field_raw, label ? label : reason);
    };

    if (push_cache_unsubscribed && !push_reply(response_25, GBE_kDotaCacheUnsubscribed, "25"))
        return false;

    if (push_postgame_join && !push_reply(response_7010_postgame, GBE_kDotaJoinChatChannelResponse, "7010_postgame"))
        return false;

    GBE_ClearPendingResetAfterCacheUnsubscribed(lobby_id);
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Queued postgame teardown cache_unsub=%u postgame_join=%u; deferring reset until 7272/7014 LobbyID=%llu pre_channel=%llu post_channel=%llu reason=%s",
        push_cache_unsubscribed ? 1u : 0u,
        push_postgame_join ? 1u : 0u,
        static_cast<unsigned long long>(lobby_id),
        static_cast<unsigned long long>(pre_postgame_chat_channel_id),
        static_cast<unsigned long long>(GBE_local_lobby.chat_channel_id),
        reason ? reason : "unknown"
    );

    GBE_UpdateDotaPracticeLobbyLaunchRichPresence("#DOTA_RP_PRIVATE_LOBBY", "RUN", true, false);

    std::string no_lobby_persona;
    if (GBE_PrepareDotaPersonaStatePeripheralMessage(GBE_kDotaAbandonPersonaStatePrivateLobbyNoLobbyHex, steam_id, lobby_id, no_lobby_persona)) {
        // Rich Presence is already updated via ISteamFriends::SetRichPresence above.
        // No need to push 766 (CMsgClientPersonaState) into GC queue -- Dota ignores it.
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "built postgame persona label=private_lobby_no_lobby lobby_id=%llu size=%zu reason=%s (not queued, using SetRichPresence)",
            static_cast<unsigned long long>(lobby_id),
            no_lobby_persona.size(),
            reason ? reason : "unknown"
        );
    } else {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "failed building postgame persona label=private_lobby_no_lobby lobby_id=%llu reason=%s",
            static_cast<unsigned long long>(lobby_id),
            reason ? reason : "unknown"
        );
    }

    return true;
}


bool Steam_Game_Coordinator::GBE_SendDotaPracticeLobbyDetailsUpdate(bool wrapped, const std::string *outer_session_field_raw, const char *reason)
{
    GBE_LocalLobby lobby{};
    if (!GBE_CaptureCurrentDotaLobbyState(reason ? reason : "details_update", lobby))
        return false;

    if (GBE_ShouldSuppressDotaAbandonedLobby(lobby.lobby_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Skipped 26 details update for suppressed abandoned lobby LobbyID=%llu reason=%s state=%u game_state=%u",
            static_cast<unsigned long long>(lobby.lobby_id),
            reason ? reason : "unknown",
            lobby.state,
            lobby.game_state
        );
        return false;
    }

    const char *details_reason = reason ? reason : "";
    const bool arcade_runtime_loading_result =
        !wrapped &&
        lobby.custom_game.game_id != 0ull &&
        lobby.match_id != 0ull &&
        lobby.launch_phase >= GBE_kDotaLaunchPhaseLoaded &&
        (std::strcmp(details_reason, "8053_finished_loading") == 0 ||
            std::strcmp(details_reason, "8053_load_failed") == 0);
    const bool arcade_runtime_poll =
        !wrapped &&
        lobby.custom_game.game_id != 0ull &&
        lobby.match_id != 0ull &&
        lobby.launch_phase >= GBE_kDotaLaunchPhaseRunQueued &&
        std::strcmp(details_reason, "7034_launch_poll") == 0;
    if (arcade_runtime_loading_result || arcade_runtime_poll) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Skipped arcade runtime 26 details update LobbyID=%llu reason=%s state=%u game_state=%u launch_phase=%s",
            static_cast<unsigned long long>(lobby.lobby_id),
            reason ? reason : "unknown",
            lobby.state,
            lobby.game_state,
            GBE_DescribeDotaLaunchPhase(lobby.launch_phase)
        );
        return false;
    }

    const bool preserve_server_id =
        lobby.custom_game.game_id != 0ull &&
        lobby.match_id != 0ull &&
        (lobby.state >= 2u || lobby.game_state >= 1u);

    std::string response_26;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(lobby, GBE_GetDotaLobbyOwnerName(), response_26, preserve_server_id)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 26 details update for LobbyID=%llu reason=%s", static_cast<unsigned long long>(lobby.lobby_id), reason ? reason : "unknown");
        return false;
    }

    if (!GBE_PushDotaResponse(GBE_kDotaPracticeLobbyDetailsUpdate, response_26, wrapped, outer_session_field_raw, reason, false, 0u, 0u))
        return false;

    return true;
}


bool Steam_Game_Coordinator::GBE_PushDotaResponse(uint32 inner_emsg, const std::string &inner_message, bool wrapped, const std::string *outer_session_field_raw, const char *reason, bool apply_lobby_state, uint32 lobby_state, uint32 lobby_game_state, std::string *out_wrapped_message)
{
    gbe::dota_gc_router::DotaGcOutboundMessage outbound{};
    if (!gbe::dota_gc_router::build_outbound_message(
            inner_emsg,
            inner_message,
            wrapped,
            outer_session_field_raw,
            settings->get_local_steam_id().ConvertToUint64(),
            GBE_kEMsgClientFromGC,
            GBE_kDotaAppId,
            outbound)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed response reason=%s path=%s inner_emsg=%u wrapped=%u payload_size=%zu lobby_id=%llu state=%u game_state=%u has_session=%u",
            reason ? reason : "unknown",
            wrapped ? "wrapped" : "direct",
            inner_emsg,
            wrapped ? 1u : 0u,
            inner_message.size(),
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            outer_session_field_raw ? 1u : 0u
        );
        return false;
    }

    push_incoming_now(outbound.emsg, outbound.payload, apply_lobby_state, lobby_state, lobby_game_state);
    GBE_LogDotaResponsePacket(
        reason,
        inner_emsg,
        wrapped,
        inner_message,
        outbound.payload,
        GBE_local_lobby.lobby_id,
        apply_lobby_state ? lobby_state : GBE_local_lobby.state,
        apply_lobby_state ? lobby_game_state : GBE_local_lobby.game_state);
    if (wrapped && out_wrapped_message)
        *out_wrapped_message = outbound.payload;
    return true;
}


bool Steam_Game_Coordinator::GBE_PushDotaCacheUnsubscribedResponse(const std::string &message, bool wrapped, const std::string *outer_session_field_raw, const char *reason)
{
    return GBE_PushDotaResponse(GBE_kDotaCacheUnsubscribed, message, wrapped, outer_session_field_raw, reason);
}


bool Steam_Game_Coordinator::GBE_PushDotaOtherLeftChannelResponse(const std::string &message, bool wrapped, const std::string *outer_session_field_raw, const char *reason)
{
    return GBE_PushDotaResponse(GBE_kDotaOtherLeftChannel, message, wrapped, outer_session_field_raw, reason);
}


bool Steam_Game_Coordinator::GBE_SendDotaCustomGameLaunchSetupFlow(bool wrapped, const std::string *outer_session_field_raw, bool has_request_job, uint64 request_job_id)
{
    const gbe::dota_lobby_state::CustomGameLaunchSetupPlan launch_plan = gbe::dota_lobby_state::compose_custom_game_launch_setup_plan(
        GBE_local_lobby,
        GBE_kDotaLaunchPhaseSetupSynced);
    const gbe::dota_lobby_state::CustomGameLaunchSetupEventPlan event_plan = gbe::dota_lobby_state::compose_custom_game_launch_setup_event_plan(GBE_kDotaPracticeLobbyDetailsUpdate);
    const GBE_LocalLobby &readyup_lobby = launch_plan.readyup_lobby;
    const gbe::dota_lobby_state::LaunchDetailsEvent *readyup_event = event_plan.details_events.size() > 0 ? &event_plan.details_events[0] : nullptr;
    const gbe::dota_lobby_state::LaunchDetailsEvent *serversetup_event = event_plan.details_events.size() > 1 ? &event_plan.details_events[1] : nullptr;

    std::string readyup_26;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(readyup_lobby, GBE_local_lobby.owner_name, readyup_26, true)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building custom game READYUP 26 after 7041 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return false;
    }

    if (!readyup_event || !GBE_PushDotaResponse(readyup_event->emsg, readyup_26, wrapped, outer_session_field_raw, readyup_event->reason.c_str(), readyup_event->apply_lobby_state, readyup_event->lobby_state, readyup_event->lobby_game_state))
        return false;

    GBE_local_lobby = launch_plan.serversetup_lobby;
    GBE_PublishSharedDotaLobbyState("7041_custom_game_serversetup");

    std::string serversetup_26;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(GBE_local_lobby, GBE_local_lobby.owner_name, serversetup_26, true)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building custom game SERVERSETUP 26 after 7041 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return false;
    }

    if (!serversetup_event || !GBE_PushDotaResponse(serversetup_event->emsg, serversetup_26, wrapped, outer_session_field_raw, serversetup_event->reason.c_str(), serversetup_event->apply_lobby_state, serversetup_event->lobby_state, serversetup_event->lobby_game_state))
        return false;

    GBE_MarkDotaLaunchPhase(launch_plan.synced_launch_phase, event_plan.mark_phase_reason.c_str());
    if (event_plan.steam_auth_ack.queue)
        GBE_MaybeQueueDotaPracticeLobbySteamAuthAck(event_plan.steam_auth_ack.reason.c_str(), has_request_job ? request_job_id : 0ull);
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Sent custom game launch setup flow after 7041 path=%s has_request_job=%d request_job=%llu LobbyID=%llu match_id=%llu server_id=%llu custom_id=%llu custom_map=%s",
        wrapped ? "wrapped" : "direct",
        has_request_job ? 1 : 0,
        static_cast<unsigned long long>(request_job_id),
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id),
        static_cast<unsigned long long>(GBE_local_lobby.custom_game.game_id),
        GBE_local_lobby.custom_game.map_name.c_str()
    );
    return true;
}
