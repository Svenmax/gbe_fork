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

// Split from gbe_dota_lobby_state_coordinator.cpp (stage D.10.2). Behavior unchanged.

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_gc_diagnostics.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_request_router.h"
#include "gbe_proto_buf_header.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_custom_lobby_http.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_gc_wire.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_dota_lifecycle_state_machine.h"
#include "gbe_dota_lobby_state_store.h"
#include "gbe_dota_reconnect_context.h"
#include "gbe_gc_config.h"
#include "gbe_gc_message_utils.h"
#include <algorithm>
#include "gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_payload_lobby_helpers.h"
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
#include <utility>
#include <vector>
#include <steammessages.pb.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/econ_gcmessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>
#include <tf2/gcsystemmsgs.pb.h>
#include <tf2/tf_gcmessages.pb.h>

using namespace gamecoordinator::tf2;

bool Steam_Game_Coordinator::GBE_MaybeNotifyDotaPracticeLobbyMembersChanged(const char *reason)
{
    if (is_server || gc_profile != GC_PROFILE_DOTA2)
        return false;
    gbe::dota_lobby_state::LocalLobbyOwner local_lobby(GBE_local_lobby);
    const GBE_LocalLobby previous_lobby_snapshot = local_lobby.snapshot();

    if (!previous_lobby_snapshot.active || previous_lobby_snapshot.lobby_id == 0 || previous_lobby_snapshot.generic_lobby_id == 0)
        return false;
    if (previous_lobby_snapshot.state > 2u)
        return false;

    const std::vector<GBE_DotaLobbyMemberState> previous_members = previous_lobby_snapshot.members;
    const uint64 previous_owner_steam_id = previous_lobby_snapshot.owner_steam_id;
    const uint32 previous_owner_account_id = previous_lobby_snapshot.owner_account_id;
    const std::string previous_owner_name = previous_lobby_snapshot.owner_name;
    const uint32 previous_state = previous_lobby_snapshot.state;
    const uint32 previous_game_state = previous_lobby_snapshot.game_state;
    const uint64 previous_match_id = previous_lobby_snapshot.match_id;
    const uint64 previous_server_id = previous_lobby_snapshot.server_id;
    const std::string previous_connect = previous_lobby_snapshot.connect;
    const uint32 previous_game_start_time = previous_lobby_snapshot.game_start_time;
    const std::string previous_room_name = previous_lobby_snapshot.room_name;
    const uint32 previous_game_mode = previous_lobby_snapshot.game_mode;
    const uint32 previous_server_region = previous_lobby_snapshot.server_region;
    const std::string previous_lan_host_ping_location = previous_lobby_snapshot.lan_host_ping_location;
    const bool previous_allow_cheats = previous_lobby_snapshot.allow_cheats;
    const bool previous_fill_with_bots = previous_lobby_snapshot.fill_with_bots;
    const bool previous_allow_spectating = previous_lobby_snapshot.allow_spectating;
    const std::string previous_pass_key = previous_lobby_snapshot.pass_key;
    const uint32 previous_visibility = previous_lobby_snapshot.visibility;
    const uint32 previous_bot_difficulty_radiant = previous_lobby_snapshot.bot_difficulty_radiant;
    const uint32 previous_bot_difficulty_dire = previous_lobby_snapshot.bot_difficulty_dire;
    const uint64 previous_bot_radiant = previous_lobby_snapshot.bot_radiant;
    const uint64 previous_bot_dire = previous_lobby_snapshot.bot_dire;
    GBE_LocalLobby lobby{};
    if (!GBE_CaptureCurrentDotaLobbyStateWithPreviousSlots(reason ? reason : "generic_lobby_members_changed", previous_members, previous_owner_steam_id, lobby))
        return false;
    const GBE_LocalLobby current_lobby_snapshot = local_lobby.snapshot();
    const bool owner_changed =
        previous_owner_steam_id != current_lobby_snapshot.owner_steam_id ||
        previous_owner_account_id != current_lobby_snapshot.owner_account_id ||
        previous_owner_name != current_lobby_snapshot.owner_name;
    const bool runtime_changed =
        previous_state != current_lobby_snapshot.state ||
        previous_game_state != current_lobby_snapshot.game_state ||
        previous_match_id != current_lobby_snapshot.match_id ||
        previous_server_id != current_lobby_snapshot.server_id ||
        previous_connect != current_lobby_snapshot.connect ||
        previous_game_start_time != current_lobby_snapshot.game_start_time;
    const bool settings_changed =
        previous_room_name != current_lobby_snapshot.room_name ||
        previous_game_mode != current_lobby_snapshot.game_mode ||
        previous_server_region != current_lobby_snapshot.server_region ||
        previous_lan_host_ping_location != current_lobby_snapshot.lan_host_ping_location ||
        previous_allow_cheats != current_lobby_snapshot.allow_cheats ||
        previous_fill_with_bots != current_lobby_snapshot.fill_with_bots ||
        previous_allow_spectating != current_lobby_snapshot.allow_spectating ||
        previous_pass_key != current_lobby_snapshot.pass_key ||
        previous_visibility != current_lobby_snapshot.visibility ||
        previous_bot_difficulty_radiant != current_lobby_snapshot.bot_difficulty_radiant ||
        previous_bot_difficulty_dire != current_lobby_snapshot.bot_difficulty_dire ||
        previous_bot_radiant != current_lobby_snapshot.bot_radiant ||
        previous_bot_dire != current_lobby_snapshot.bot_dire;
    const std::vector<GBE_DotaLobbyMemberState> joined_members = gbe::dota_lobby_flow::find_joined_lobby_members(previous_members, current_lobby_snapshot.members);
    if (!owner_changed && !runtime_changed && !settings_changed && gbe::dota_lobby_flow::lobby_members_equal(previous_members, current_lobby_snapshot.members))
        return false;

    // When lobby member state changes on the client during pre-game
    // (game_state == 0), clear the direct-connect callback dedup key
    // and allow the private lobby snapshot to be replayed.  This ensures that
    // GameServerChangeRequested_t is re-sent with the real IP address when the
    // lobby state transitions during initial match setup.
    //
    // During an active match (game_state >= 1), keep the key stable.
    // The host sends member-change updates continuously (e.g. when a player
    // disconnects), and clearing the signature would allow every subsequent
    // queued_state apply to re-fire GameServerChangeRequested_t, causing an
    // automatic reconnect loop. Manual reconnect has its own key-clearing path.
    if (!is_server && !gbe::dota_lobby_flow::lobby_members_equal(previous_members, current_lobby_snapshot.members) && current_lobby_snapshot.game_state == 0u) {
        GBE_ClearLastDotaDirectConnectCallbackKey();
        GBE_ClearDotaPrivateLobbySnapshotReplayed();
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "cleared direct connect key and snapshot replay flag on member change reason=%s lobby_id=%llu state=%u game_state=%u",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(current_lobby_snapshot.lobby_id),
            current_lobby_snapshot.state,
            current_lobby_snapshot.game_state
        );
    }

    if (is_server)
        GBE_PublishSharedDotaLobbyState(reason ? reason : "generic_lobby_members_changed");

    // PLAYER-side PostGame cleanup: when a non-server client GC detects that the
    // generic lobby state transitioned from in-game (state < 3) to PostGame
    // (state >= 3), clear shared state, Rich Presence, and push CacheUnsubscribed.
    // The HOST's server GC handles this via 7004 signout; the PLAYER has no such
    // path and must rely on observing the generic lobby metadata change.
    //
    // IMPORTANT: On the HOST machine, the client GC is also !is_server and will
    // see this same state transition (because the server GC just published state=3
    // to generic lobby metadata).  We must NOT run PLAYER cleanup on the HOST's
    // client GC -- that would invalidate shared state before the server GC's
    // normal signout finalize path can use it.  Detect this by checking whether
    // a server GC exists in this process and owns the same lobby.
    const bool host_has_active_server_gc = GBE_HostHasActiveDotaServerLobby(current_lobby_snapshot.lobby_id);
    const bool arcade_active_match =
        gbe::dota_custom_game::has_custom_game_details(current_lobby_snapshot.custom_game) &&
        current_lobby_snapshot.match_id != 0ull &&
        current_lobby_snapshot.game_state >= 2u &&
        current_lobby_snapshot.launch_phase >= GBE_kDotaLaunchPhaseRunQueued;
    const auto postgame_observation = gbe::dota_lobby_state::compute_postgame_observation_decision(
        is_server,
        host_has_active_server_gc,
        arcade_active_match,
        previous_state,
        current_lobby_snapshot.state,
        current_lobby_snapshot.lobby_id);
    if (postgame_observation.skip_for_host_client) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Skipping PLAYER PostGame cleanup on HOST client GC: server GC owns lobby LobbyID=%llu state=%u reason=%s",
            static_cast<unsigned long long>(current_lobby_snapshot.lobby_id),
            current_lobby_snapshot.state,
            reason ? reason : "generic_lobby_members_changed"
        );
    }
    if (postgame_observation.skip_for_arcade_active_match) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Skipping PLAYER PostGame cleanup during arcade active match LobbyID=%llu state=%u game_state=%u launch_phase=%s reason=%s",
            static_cast<unsigned long long>(current_lobby_snapshot.lobby_id),
            current_lobby_snapshot.state,
            current_lobby_snapshot.game_state,
            GBE_DescribeDotaLaunchPhase(current_lobby_snapshot.launch_phase),
            reason ? reason : "generic_lobby_members_changed"
        );
    }
    if (postgame_observation.run_player_cleanup) {
        gbe::dota_lifecycle_state_machine::MachineState machine_state{};
        machine_state.generation = GBE_CurrentDotaLobbyGeneration();
        const auto teardown = gbe::dota_lifecycle_state_machine::transition_teardown(
            machine_state,
            { { gbe::dota_lifecycle_state_machine::EventKind::PostGame,
                gbe::dota_lifecycle_state_machine::EventSource::Internal,
                0u,
                machine_state.generation },
              gbe::dota_lifecycle_state_machine::TeardownStage::Initiate,
              true,
              false });
        if (!teardown.accepted() || !teardown.effects.contains(
                gbe::dota_lifecycle_state_machine::EffectKind::TeardownPostGameInitiateRequested))
            return true;
        const uint64 cleaning_lobby_id = current_lobby_snapshot.lobby_id;
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] PLAYER PostGame cleanup: generic lobby transitioned to state=%u game_state=%u LobbyID=%llu reason=%s",
            current_lobby_snapshot.state,
            current_lobby_snapshot.game_state,
            static_cast<unsigned long long>(cleaning_lobby_id),
            reason ? reason : "generic_lobby_members_changed"
        );

        // Send a final msg 26 with PostGame state so Dota sees the transition
        std::string postgame_response_26;
        if (GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(lobby, GBE_GetDotaLobbyOwnerName(), postgame_response_26, true)) {
            push_incoming_now(GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask, postgame_response_26);
        }

        std::string response_25;
        const bool push_cache_unsubscribed = gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(cleaning_lobby_id, response_25);
        GBE_ExecuteDotaLifecycleActions(gbe::dota_lobby_flow::player_postgame_cleanup_action_list(
            cleaning_lobby_id,
            response_25,
            push_cache_unsubscribed,
            reason ? reason : "generic_lobby_members_changed"));
        if (push_cache_unsubscribed) {
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "PLAYER PostGame: pushed CacheUnsubscribed lobby_id=%llu size=%zu",
                static_cast<unsigned long long>(cleaning_lobby_id),
                response_25.size()
            );
        }

        return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Detected generic lobby member/owner/runtime/settings change LobbyID=%llu generic_lobby_id=%llu old_members=%zu new_members=%zu old_owner=%llu new_owner=%llu old_state=%u new_state=%u old_game_state=%u new_game_state=%u old_server_id=%llu new_server_id=%llu old_connect=%s new_connect=%s settings_changed=%u old_cheats=%u new_cheats=%u reason=%s",
        static_cast<unsigned long long>(current_lobby_snapshot.lobby_id),
        static_cast<unsigned long long>(current_lobby_snapshot.generic_lobby_id),
        previous_members.size(),
        current_lobby_snapshot.members.size(),
        static_cast<unsigned long long>(previous_owner_steam_id),
        static_cast<unsigned long long>(current_lobby_snapshot.owner_steam_id),
        previous_state,
        current_lobby_snapshot.state,
        previous_game_state,
        current_lobby_snapshot.game_state,
        static_cast<unsigned long long>(previous_server_id),
        static_cast<unsigned long long>(current_lobby_snapshot.server_id),
        previous_connect.c_str(),
        current_lobby_snapshot.connect.c_str(),
        settings_changed ? 1u : 0u,
        previous_allow_cheats ? 1u : 0u,
        current_lobby_snapshot.allow_cheats ? 1u : 0u,
        reason ? reason : "generic_lobby_members_changed"
    );

    const uint64 local_steam_id = settings ? settings->get_local_steam_id().ConvertToUint64() : 0ull;
    const bool lan_launch_active =
        current_lobby_snapshot.lan &&
        current_lobby_snapshot.match_id != 0ull;

    std::string response_26;
    const bool sent_details_update = GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(lobby, GBE_GetDotaLobbyOwnerName(), response_26, is_server || lan_launch_active);
    if (sent_details_update) {
        const bool arcade_runtime_member_change =
            gbe::dota_custom_game::has_custom_game_details(current_lobby_snapshot.custom_game) &&
            current_lobby_snapshot.match_id != 0ull &&
            current_lobby_snapshot.launch_phase >= GBE_kDotaLaunchPhaseRunQueued &&
            current_lobby_snapshot.state >= 2u;
        const bool peer_lan_direct_launch =
            local_steam_id != 0ull &&
            current_lobby_snapshot.owner_steam_id != 0ull &&
            local_steam_id != current_lobby_snapshot.owner_steam_id &&
            current_lobby_snapshot.lan &&
            current_lobby_snapshot.match_id != 0ull &&
            current_lobby_snapshot.state == 2u &&
            current_lobby_snapshot.server_id == 0ull &&
            gbe::proto_wire::parse_dota_practice_lobby_connect_ipv4(gbe::proto_wire::get_dota_practice_lobby_first_connect_endpoint(current_lobby_snapshot.connect)) != 0u;

        if (arcade_runtime_member_change) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Skipped arcade runtime direct 26 details update from preserved member snapshot LobbyID=%llu reason=%s size=%zu",
                static_cast<unsigned long long>(lobby.lobby_id),
                reason ? reason : "generic_lobby_members_changed",
                response_26.size()
            );
        } else if (peer_lan_direct_launch) {
            GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(reason ? reason : "generic_lobby_peer_lan_direct_launch");
        } else {
            push_incoming_now(GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask, response_26);
            if (runtime_changed)
                GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(reason ? reason : "generic_lobby_runtime_changed");
        }
        if (!arcade_runtime_member_change) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] %s direct 26 details update from preserved member snapshot LobbyID=%llu reason=%s size=%zu body_prefix=%s",
                peer_lan_direct_launch ? "Suppressed peer LAN" : "Sent",
                static_cast<unsigned long long>(lobby.lobby_id),
                reason ? reason : "generic_lobby_members_changed",
                response_26.size(),
                gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(response_26.data()), response_26.size(), 32).c_str()
            );
        }
    } else {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed building preserved member snapshot 26 details update for LobbyID=%llu reason=%s",
            static_cast<unsigned long long>(lobby.lobby_id),
            reason ? reason : "generic_lobby_members_changed"
        );
    }

    if (current_lobby_snapshot.has_chat_channel && current_lobby_snapshot.chat_channel_id != 0) {
        for (const GBE_DotaLobbyMemberState &joined_member : joined_members) {
            std::string joined_name;
            Steam_Client *steam_client = get_steam_client();
            if (steam_client && steam_client->steam_matchmaking && current_lobby_snapshot.generic_lobby_id != 0ull) {
                const char *generic_name = steam_client->steam_matchmaking->GetLobbyMemberData(
                    CSteamID((uint64)current_lobby_snapshot.generic_lobby_id),
                    CSteamID((uint64)joined_member.steam_id),
                    GBE_kDotaGenericLobbyMemberNameKey);
                if (generic_name && generic_name[0] != '\0')
                    joined_name = generic_name;
            }
            if (joined_name.empty() && steam_client && steam_client->steam_friends) {
                const char *friend_name = steam_client->steam_friends->GetFriendPersonaName(CSteamID((uint64)joined_member.steam_id));
                if (friend_name && friend_name[0] != '\0' && std::string(friend_name) != "Unknown User")
                    joined_name = friend_name;
            }
            if (joined_name.empty())
                joined_name = "Lobby Member";

            std::string response_7013;
            if (gbe::gc_message::build_dota_other_joined_channel_payload(current_lobby_snapshot.chat_channel_id, joined_name, joined_member.steam_id, response_7013)) {
                push_incoming_now(GBE_kDotaOtherJoinedChannel | GBE_kProtoMask, response_7013);
                GBE_GC_DebugLog(
                    "GC_DOTA_LOBBY",
                    "[LOBBY] Sent 7013 other joined channel LobbyID=%llu channel_id=%llu steam_id=%llu name=%s reason=%s",
                    static_cast<unsigned long long>(current_lobby_snapshot.lobby_id),
                    static_cast<unsigned long long>(current_lobby_snapshot.chat_channel_id),
                    static_cast<unsigned long long>(joined_member.steam_id),
                    joined_name.c_str(),
                    reason ? reason : "generic_lobby_members_changed"
                );
            }
        }
    }

    if (current_lobby_snapshot.has_chat_channel && current_lobby_snapshot.chat_channel_id != 0) {
        GBE_LocalLobby chat_snapshot = lobby;
        chat_snapshot.members = gbe::dota_lobby_flow::filter_nonzero_lobby_members(lobby.members);

        std::string response_7010;
        if (GBE_AdaptDotaJoinChatChannelResponsePayload(
                settings->get_local_steam_id().ConvertToUint64(),
                chat_snapshot.generic_lobby_id,
                chat_snapshot.chat_channel_id,
                chat_snapshot.chat_channel_name,
                std::string(settings->get_local_name()),
                chat_snapshot.members,
                chat_snapshot.owner_steam_id,
                chat_snapshot.owner_name,
                chat_snapshot.chat_channel_type,
                response_7010)) {
            push_incoming_now(GBE_kDotaJoinChatChannelResponse | GBE_kProtoMask, response_7010);
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Refreshed chat channel members after lobby member change LobbyID=%llu channel_id=%llu members=%zu reason=%s",
                static_cast<unsigned long long>(chat_snapshot.lobby_id),
                static_cast<unsigned long long>(chat_snapshot.chat_channel_id),
                chat_snapshot.members.size(),
                reason ? reason : "generic_lobby_members_changed"
            );
        } else {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Failed refreshing chat channel members after lobby member change LobbyID=%llu channel_id=%llu reason=%s",
                static_cast<unsigned long long>(current_lobby_snapshot.lobby_id),
                static_cast<unsigned long long>(current_lobby_snapshot.chat_channel_id),
                reason ? reason : "generic_lobby_members_changed"
            );
        }
    }

    return sent_details_update;
}

bool Steam_Game_Coordinator::GBE_MaybeHandleDotaPracticeLobbyKicked(const char *reason)
{
    if (is_server || gc_profile != GC_PROFILE_DOTA2)
        return false;
    gbe::dota_lobby_state::LocalLobbyOwner local_lobby(GBE_local_lobby);
    const GBE_LocalLobby kicked_lobby_snapshot = local_lobby.snapshot();

    if (!kicked_lobby_snapshot.active || kicked_lobby_snapshot.lobby_id == 0 || kicked_lobby_snapshot.generic_lobby_id == 0)
        return false;
    const bool after_chat_leave =
        gbe::dota_diagnostic::reason_from_string(reason ? reason : "") ==
        gbe::dota_diagnostic::Reason::LeaveChat;
    if (!kicked_lobby_snapshot.has_chat_channel && !after_chat_leave)
        return false;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking)
        return false;

    CSteamID generic_lobby_id((uint64)kicked_lobby_snapshot.generic_lobby_id);
    if (!generic_lobby_id.IsLobby())
        return false;

    const uint64 local_steam_id = settings->get_local_steam_id().ConvertToUint64();
    const std::vector<CSteamID> generic_members = steam_client->steam_matchmaking->GetLobbyMemberListSnapshot(generic_lobby_id);
    bool still_in_generic_lobby = false;
    for (const CSteamID &member_id : generic_members) {
        if (member_id.ConvertToUint64() == local_steam_id) {
            still_in_generic_lobby = true;
            break;
        }
    }
    if (still_in_generic_lobby) {
        local_lobby.apply("generic_lobby_local_member_seen", [](GBE_LocalLobby &lobby) {
            gbe::dota_lobby_state::note_generic_lobby_local_member_seen(lobby);
        });
        return false;
    }

    if (!kicked_lobby_snapshot.seen_local_in_generic_lobby) {
        if (local_lobby.apply("generic_lobby_waiting_join_confirmation", [](GBE_LocalLobby &lobby) {
            return gbe::dota_lobby_state::mark_generic_lobby_waiting_join_confirmation_logged(lobby);
        })) {
            const GBE_LocalLobby waiting_lobby_snapshot = local_lobby.snapshot();
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Waiting for generic lobby join confirmation before treating local user as kicked. LobbyID=%llu generic_lobby_id=%llu reason=%s",
                static_cast<unsigned long long>(waiting_lobby_snapshot.lobby_id),
                static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64()),
                reason ? reason : "generic_lobby_members_changed"
            );
        }
        return false;
    }

    // During a launched LAN game, the generic lobby member list can become empty
    // because the underlying P2P connection was lost (e.g. sleep/hibernate).
    // This does NOT mean the user was actually kicked by the lobby leader.
    // The real game connection uses steamnetworkingsockets.dll and can survive
    // or reconnect independently. Suppress the kicked detection here.
    // EXCEPTION: If the lobby has reached PostGame (state >= 3), the game is over
    // and the host legitimately destroyed the lobby. Do NOT suppress in that case.
    if (kicked_lobby_snapshot.lan &&
        kicked_lobby_snapshot.state >= 2u &&
        kicked_lobby_snapshot.state < 3u &&
        kicked_lobby_snapshot.match_id != 0ull &&
        !kicked_lobby_snapshot.connect.empty()) {
        if (local_lobby.apply("generic_lobby_kicked_suppressed", [](GBE_LocalLobby &lobby) {
            return gbe::dota_lobby_state::mark_generic_lobby_kicked_suppressed_logged(lobby);
        })) {
            const GBE_LocalLobby suppressed_lobby_snapshot = local_lobby.snapshot();
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Suppressed kicked detection during launched LAN game (generic lobby member loss from P2P disconnect). "
                "LobbyID=%llu generic_lobby_id=%llu state=%u game_state=%u match_id=%llu connect=%s reason=%s",
                static_cast<unsigned long long>(suppressed_lobby_snapshot.lobby_id),
                static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64()),
                suppressed_lobby_snapshot.state,
                suppressed_lobby_snapshot.game_state,
                static_cast<unsigned long long>(suppressed_lobby_snapshot.match_id),
                suppressed_lobby_snapshot.connect.c_str(),
                reason ? reason : "generic_lobby_members_changed"
            );
        }
        return false;
    }

    const uint64 lobby_id = kicked_lobby_snapshot.lobby_id;
    std::string response_25;
    if (!gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(lobby_id, response_25)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 payload after practice lobby kick LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
        return true;
    }

    std::string response_7102;
    if (!gbe::gc_message::build_dota_practice_lobby_kicked_popup_payload(response_7102)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7102 popup after practice lobby kick LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
        return true;
    }

    push_incoming_now(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25);
    push_incoming_now(GBE_kDotaPopup | GBE_kProtoMask, response_7102, 0.01);
    ResetGCMemory("7081_kicked_from_lobby", false, false, gbe::dota_lobby_generation::Boundary::Leave);
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Detected local user kicked from generic Dota lobby. queued 25 and 7102 LobbyID=%llu generic_lobby_id=%llu reason=%s",
        static_cast<unsigned long long>(lobby_id),
        static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64()),
        reason ? reason : "generic_lobby_members_changed"
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_AdoptDotaGenericLobbyOwnerIfNeeded(const char *reason)
{
    gbe::dota_lobby_state::LocalLobbyOwner local_lobby(GBE_local_lobby);
    const GBE_LocalLobby adoption_lobby_snapshot = local_lobby.snapshot();

    if (!adoption_lobby_snapshot.active || adoption_lobby_snapshot.lobby_id == 0 || adoption_lobby_snapshot.generic_lobby_id == 0)
        return false;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking)
        return false;

    CSteamID generic_lobby_id((uint64)adoption_lobby_snapshot.generic_lobby_id);
    if (!generic_lobby_id.IsLobby())
        return false;

    CSteamID generic_owner = steam_client->steam_matchmaking->GetLobbyOwner(generic_lobby_id);
    if (!generic_owner.IsValid() || generic_owner.ConvertToUint64() == 0 || generic_owner.ConvertToUint64() == adoption_lobby_snapshot.owner_steam_id)
        return false;

    const uint64 previous_owner_steam_id = adoption_lobby_snapshot.owner_steam_id;
    const uint64 new_owner_steam_id = generic_owner.ConvertToUint64();
    const uint64 local_steam_id = settings->get_local_steam_id().ConvertToUint64();

    const bool preserve_custom_game_launch_owner =
        gbe::dota_custom_game::has_custom_game_details(adoption_lobby_snapshot.custom_game) &&
        adoption_lobby_snapshot.match_id != 0ull &&
        adoption_lobby_snapshot.launch_phase >= GBE_kDotaLaunchPhaseSetupSynced;
    if (preserve_custom_game_launch_owner) {
        if (local_lobby.apply("generic_lobby_owner_adoption_suppressed", [](GBE_LocalLobby &lobby) {
            return gbe::dota_lobby_state::mark_generic_lobby_owner_adoption_suppressed_logged(lobby);
        })) {
            const GBE_LocalLobby suppressed_lobby_snapshot = local_lobby.snapshot();
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Ignored generic lobby owner adoption during custom game launch reason=%s dota_lobby_id=%llu generic_lobby_id=%llu dota_owner=%llu generic_owner=%llu state=%u game_state=%u launch_phase=%s team=%u slot=%u",
                reason ? reason : "unknown",
                static_cast<unsigned long long>(suppressed_lobby_snapshot.lobby_id),
                static_cast<unsigned long long>(suppressed_lobby_snapshot.generic_lobby_id),
                static_cast<unsigned long long>(previous_owner_steam_id),
                static_cast<unsigned long long>(new_owner_steam_id),
                suppressed_lobby_snapshot.state,
                suppressed_lobby_snapshot.game_state,
                GBE_DescribeDotaLaunchPhase(suppressed_lobby_snapshot.launch_phase),
                suppressed_lobby_snapshot.owner_team,
                suppressed_lobby_snapshot.owner_slot
            );
        }
        return false;
    }

    if (!is_server &&
        local_steam_id != 0ull &&
        previous_owner_steam_id != 0ull &&
        previous_owner_steam_id != local_steam_id &&
        new_owner_steam_id == local_steam_id &&
        adoption_lobby_snapshot.lan &&
        adoption_lobby_snapshot.state == 2u &&
        adoption_lobby_snapshot.match_id != 0ull &&
        !adoption_lobby_snapshot.connect.empty()) {
        if (local_lobby.apply("generic_lobby_owner_adoption_suppressed", [](GBE_LocalLobby &lobby) {
            return gbe::dota_lobby_state::mark_generic_lobby_owner_adoption_suppressed_logged(lobby);
        })) {
            const GBE_LocalLobby suppressed_lobby_snapshot = local_lobby.snapshot();
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Ignored generic lobby owner adoption during launched LAN peer disconnect reason=%s dota_lobby_id=%llu generic_lobby_id=%llu dota_owner=%llu generic_owner=%llu local=%llu state=%u game_state=%u match_id=%llu connect=%s",
                reason ? reason : "unknown",
                static_cast<unsigned long long>(suppressed_lobby_snapshot.lobby_id),
                static_cast<unsigned long long>(suppressed_lobby_snapshot.generic_lobby_id),
                static_cast<unsigned long long>(previous_owner_steam_id),
                static_cast<unsigned long long>(new_owner_steam_id),
                static_cast<unsigned long long>(local_steam_id),
                suppressed_lobby_snapshot.state,
                suppressed_lobby_snapshot.game_state,
                static_cast<unsigned long long>(suppressed_lobby_snapshot.match_id),
                suppressed_lobby_snapshot.connect.c_str()
            );
        }
        return false;
    }

    std::string new_owner_name;
    if (new_owner_steam_id == local_steam_id) {
        new_owner_name = settings->get_local_name();
    } else {
        const char *owner_name = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerNameKey);
        new_owner_name = owner_name ? owner_name : "Lobby Host";
    }
    {
        local_lobby.apply(reason ? reason : "adopt_generic_owner", [new_owner_steam_id, generic_owner, &new_owner_name](GBE_LocalLobby &lobby) {
            gbe::dota_lobby_flow::adopt_lobby_owner_member(
                lobby.members,
                new_owner_steam_id,
                generic_owner.GetAccountID(),
                GBE_kDotaTeamGoodGuys,
                lobby.state == 3u,
                lobby.owner_steam_id,
                lobby.owner_account_id,
                lobby.owner_team,
                lobby.owner_slot,
                lobby.owner_hero_id,
                lobby.owner_connected);
            gbe::dota_lobby_state::apply_lobby_owner_name(lobby, new_owner_name);
        });
    }
    const GBE_LocalLobby adopted_lobby_snapshot = local_lobby.snapshot();

    if (new_owner_steam_id == local_steam_id) {
        GBE_PublishDotaPracticeLobbyLocalMemberData(reason ? reason : "adopt_generic_owner");
        GBE_PublishDotaPracticeLobbyMetadata(reason ? reason : "adopt_generic_owner");
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Adopted generic lobby owner reason=%s dota_lobby_id=%llu generic_lobby_id=%llu old_owner=%llu new_owner=%llu local_is_owner=%u owner_name=%s team=%u slot=%u",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(adopted_lobby_snapshot.lobby_id),
        static_cast<unsigned long long>(adopted_lobby_snapshot.generic_lobby_id),
        static_cast<unsigned long long>(previous_owner_steam_id),
        static_cast<unsigned long long>(new_owner_steam_id),
        new_owner_steam_id == local_steam_id ? 1u : 0u,
        adopted_lobby_snapshot.owner_name.c_str(),
        adopted_lobby_snapshot.owner_team,
        adopted_lobby_snapshot.owner_slot
    );
    return true;
}
