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

bool Steam_Game_Coordinator::GBE_NormalizeDotaArcadeLobbyMemberSlots(GBE_LocalLobby &lobby)
{
    return gbe::dota_lobby_flow::normalize_arcade_lobby_member_slots(
        lobby.custom_game.game_id != 0ull,
        lobby.owner_steam_id,
        lobby.owner_team,
        lobby.owner_slot,
        lobby.members,
        GBE_kDotaTeamGoodGuys);
}

bool Steam_Game_Coordinator::GBE_CaptureCurrentDotaLobbyState(const char *reason, GBE_LocalLobby &snapshot, bool restore_shared)
{
    if (restore_shared)
        GBE_RestoreSharedDotaLobbyState(reason);

    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0)
        return false;

    if (GBE_local_lobby.generic_lobby_id != 0) {
        Steam_Client *steam_client = get_steam_client();
        if (steam_client && steam_client->steam_matchmaking) {
            CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
            if (generic_lobby_id.IsLobby()) {
                const std::string generic_lobby_state_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyStateKey);
                const std::string generic_lobby_game_state_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyGameStateKey);
                const std::string generic_room_name = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyRoomNameKey);
                const std::string generic_match_id_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyMatchIdKey);
                const std::string generic_server_id_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyServerIdKey);
                const std::string generic_connect = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyConnectKey);
                const std::string generic_game_start_time_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyGameStartTimeKey);
                const std::string generic_allow_cheats_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyAllowCheatsKey);
                const std::string generic_fill_with_bots_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyFillWithBotsKey);
                const std::string generic_allow_spectating_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyAllowSpectatingKey);
                const std::string generic_visibility_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyVisibilityKey);
                const std::string generic_bot_difficulty_radiant_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyBotDifficultyRadiantKey);
                const std::string generic_bot_difficulty_dire_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyBotDifficultyDireKey);
                const std::string generic_bot_radiant_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyBotRadiantKey);
                const std::string generic_bot_dire_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyBotDireKey);
                const std::string generic_custom_game_mode = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomGameModeKey);
                const std::string generic_custom_map_name = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomMapNameKey);
                const std::string generic_custom_difficulty_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomDifficultyKey);
                const std::string generic_custom_game_id_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomGameIdKey);
                const std::string generic_custom_min_players_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomMinPlayersKey);
                const std::string generic_custom_max_players_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomMaxPlayersKey);
                const std::string generic_custom_game_crc_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomGameCrcKey);
                const std::string generic_custom_game_timestamp_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomGameTimestampKey);
                const std::string generic_custom_game_penalties_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomGamePenaltiesKey);
                if (!generic_lobby_state_raw.empty()) {
                    const uint32 generic_lobby_state = gbe::proto_wire::parse_uint32_or_zero(generic_lobby_state_raw.c_str());
                    const bool stale_launch_regression =
                        GBE_local_lobby.custom_game.game_id != 0ull &&
                        generic_lobby_state < GBE_local_lobby.state &&
                        GBE_local_lobby.match_id != 0ull &&
                        GBE_local_lobby.launch_phase >= GBE_kDotaLaunchPhaseSetupSynced;
                    if (stale_launch_regression) {
                        GBE_GC_DebugLog(
                            "GC_DOTA_LOBBY",
                            "[LOBBY] Ignored stale generic lobby state reason=%s lobby_id=%llu generic_lobby_id=%llu local_state=%u generic_state=%u launch_phase=%s",
                            reason ? reason : "capture_current_lobby_state",
                            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                            static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
                            GBE_local_lobby.state,
                            generic_lobby_state,
                            GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase)
                        );
                    } else {
                        GBE_local_lobby.state = generic_lobby_state;
                    }
                }
                if (!generic_lobby_game_state_raw.empty()) {
                    const uint32 generic_lobby_game_state = gbe::proto_wire::parse_uint32_or_zero(generic_lobby_game_state_raw.c_str());
                    const bool stale_launch_game_regression =
                        GBE_local_lobby.custom_game.game_id != 0ull &&
                        generic_lobby_game_state < GBE_local_lobby.game_state &&
                        GBE_local_lobby.match_id != 0ull &&
                        GBE_local_lobby.launch_phase >= GBE_kDotaLaunchPhaseSetupSynced;
                    if (!stale_launch_game_regression)
                        GBE_local_lobby.game_state = generic_lobby_game_state;
                }
                if (!generic_room_name.empty())
                    GBE_local_lobby.room_name = generic_room_name;
                const uint64 generic_match_id = gbe::proto_wire::parse_uint64_or_zero(generic_match_id_raw.c_str());
                if (!generic_match_id_raw.empty() && (generic_match_id != 0ull || GBE_local_lobby.match_id == 0ull))
                    GBE_local_lobby.match_id = generic_match_id;
                const uint64 generic_server_id = gbe::proto_wire::parse_uint64_or_zero(generic_server_id_raw.c_str());
                const bool preserve_existing_lan_runtime =
                    GBE_local_lobby.custom_game.game_id == 0ull &&
                    GBE_local_lobby.lan &&
                    GBE_local_lobby.match_id != 0ull &&
                    GBE_local_lobby.server_id != 0ull &&
                    gbe::proto_wire::parse_dota_practice_lobby_connect_ipv4(GBE_local_lobby.connect) != 0u;
                if (!generic_server_id_raw.empty() &&
                    !preserve_existing_lan_runtime &&
                    (generic_server_id != 0ull || GBE_local_lobby.server_id == 0ull || GBE_local_lobby.match_id == 0ull))
                    GBE_local_lobby.server_id = generic_server_id;

                if (!generic_connect.empty() && !preserve_existing_lan_runtime)
                    GBE_local_lobby.connect = gbe::proto_wire::normalize_dota_practice_lobby_connect(generic_connect);
                if (!generic_game_start_time_raw.empty())
                    GBE_local_lobby.game_start_time = gbe::proto_wire::parse_uint32_or_zero(generic_game_start_time_raw.c_str());
                if (!generic_allow_cheats_raw.empty())
                    GBE_local_lobby.allow_cheats = gbe::proto_wire::parse_uint32_or_zero(generic_allow_cheats_raw.c_str()) != 0u;
                if (!generic_fill_with_bots_raw.empty())
                    GBE_local_lobby.fill_with_bots = gbe::proto_wire::parse_uint32_or_zero(generic_fill_with_bots_raw.c_str()) != 0u;
                if (!generic_allow_spectating_raw.empty())
                    GBE_local_lobby.allow_spectating = gbe::proto_wire::parse_uint32_or_zero(generic_allow_spectating_raw.c_str()) != 0u;
                if (!generic_visibility_raw.empty())
                    GBE_local_lobby.visibility = gbe::proto_wire::parse_uint32_or_zero(generic_visibility_raw.c_str());
                if (!generic_bot_difficulty_radiant_raw.empty())
                    GBE_local_lobby.bot_difficulty_radiant = gbe::proto_wire::parse_uint32_or_zero(generic_bot_difficulty_radiant_raw.c_str());
                if (!generic_bot_difficulty_dire_raw.empty())
                    GBE_local_lobby.bot_difficulty_dire = gbe::proto_wire::parse_uint32_or_zero(generic_bot_difficulty_dire_raw.c_str());
                if (!generic_bot_radiant_raw.empty())
                    GBE_local_lobby.bot_radiant = gbe::proto_wire::parse_uint64_or_zero(generic_bot_radiant_raw.c_str());
                if (!generic_bot_dire_raw.empty())
                    GBE_local_lobby.bot_dire = gbe::proto_wire::parse_uint64_or_zero(generic_bot_dire_raw.c_str());
                if (!generic_custom_game_mode.empty())
                    GBE_local_lobby.custom_game.mode = generic_custom_game_mode;
                if (!generic_custom_map_name.empty())
                    GBE_local_lobby.custom_game.map_name = generic_custom_map_name;
                if (!generic_custom_difficulty_raw.empty())
                    GBE_local_lobby.custom_game.difficulty = gbe::proto_wire::parse_uint32_or_zero(generic_custom_difficulty_raw.c_str());
                if (!generic_custom_game_id_raw.empty())
                    GBE_local_lobby.custom_game.game_id = gbe::proto_wire::parse_uint64_or_zero(generic_custom_game_id_raw.c_str());
                if (!generic_custom_min_players_raw.empty())
                    GBE_local_lobby.custom_game.min_players = gbe::proto_wire::parse_uint32_or_zero(generic_custom_min_players_raw.c_str());
                if (!generic_custom_max_players_raw.empty())
                    GBE_local_lobby.custom_game.max_players = gbe::proto_wire::parse_uint32_or_zero(generic_custom_max_players_raw.c_str());
                if (!generic_custom_game_crc_raw.empty())
                    GBE_local_lobby.custom_game.crc = gbe::proto_wire::parse_uint64_or_zero(generic_custom_game_crc_raw.c_str());
                if (!generic_custom_game_timestamp_raw.empty())
                    GBE_local_lobby.custom_game.timestamp = gbe::proto_wire::parse_uint32_or_zero(generic_custom_game_timestamp_raw.c_str());
                if (!generic_custom_game_penalties_raw.empty())
                    GBE_local_lobby.custom_game.penalties = gbe::proto_wire::parse_uint32_or_zero(generic_custom_game_penalties_raw.c_str()) != 0u;

                const bool custom_runtime_member_refresh = reason && std::strcmp(reason, "7034_custom_runtime_member_refresh") == 0;
                const bool repaired_owner = custom_runtime_member_refresh ? false : steam_client->steam_matchmaking->RepairLobbyOwnerIfMissing(generic_lobby_id, reason ? reason : "capture_current_lobby_state");
                if (repaired_owner) {
                    GBE_GC_DebugLog(
                        "GC_DOTA_LOBBY",
                        "[LOBBY] Repaired missing generic lobby owner before Dota snapshot reason=%s dota_lobby_id=%llu generic_lobby_id=%llu",
                        reason ? reason : "capture_current_lobby_state",
                        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id)
                    );
                }
                if (!custom_runtime_member_refresh)
                    GBE_AdoptDotaGenericLobbyOwnerIfNeeded(reason ? reason : "capture_current_lobby_state");

                std::vector<GBE_DotaLobbyMemberState> members;
                const uint64 local_steam_id = settings->get_local_steam_id().ConvertToUint64();
                const std::vector<CSteamID> generic_members = steam_client->steam_matchmaking->GetLobbyMemberListSnapshot(generic_lobby_id);
                const bool preserve_custom_game_runtime_members =
                    gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game) &&
                    GBE_local_lobby.match_id != 0ull &&
                    GBE_local_lobby.state == 2u &&
                    GBE_local_lobby.game_state >= 1u &&
                    !generic_members.empty();
                const bool preserve_launched_lan_members =
                    (GBE_local_lobby.lan || preserve_custom_game_runtime_members) &&
                    GBE_local_lobby.match_id != 0ull &&
                    GBE_local_lobby.state >= 1u &&
                    !generic_members.empty();
                bool owner_in_generic_members = false;
                for (const CSteamID &member_id : generic_members) {
                    if (!member_id.IsValid())
                        continue;
                    if (member_id.ConvertToUint64() == GBE_local_lobby.owner_steam_id)
                        owner_in_generic_members = true;

                    const uint64 member_steam_id = member_id.ConvertToUint64();
                    const bool owner_member = member_steam_id == GBE_local_lobby.owner_steam_id;
                    const bool read_member_raw_state = !owner_member || member_steam_id != local_steam_id;
                    const char *member_team_raw = read_member_raw_state
                        ? steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberTeamKey)
                        : nullptr;
                    const char *member_slot_raw = read_member_raw_state
                        ? steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberSlotKey)
                        : nullptr;
                    const bool has_member_team = !std::string(member_team_raw ? member_team_raw : "").empty();
                    const bool has_member_slot = !std::string(member_slot_raw ? member_slot_raw : "").empty();
                    const GBE_DotaLobbyMemberSnapshotData member_snapshot = gbe::dota_lobby_flow::compose_lobby_member_snapshot_data(
                        member_steam_id,
                        member_id.GetAccountID(),
                        GBE_local_lobby.owner_steam_id,
                        GBE_local_lobby.owner_account_id,
                        GBE_local_lobby.owner_team,
                        GBE_local_lobby.owner_slot,
                        GBE_local_lobby.owner_hero_id,
                        GBE_local_lobby.owner_connected || GBE_local_lobby.state == 3u,
                        has_member_team,
                        gbe::proto_wire::parse_uint32_or_zero(member_team_raw),
                        has_member_slot,
                        gbe::proto_wire::parse_uint32_or_zero(member_slot_raw),
                        read_member_raw_state ? gbe::proto_wire::parse_uint32_or_zero(steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberHeroKey)) : GBE_local_lobby.owner_hero_id,
                        read_member_raw_state ? gbe::proto_wire::parse_uint32_or_zero(steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberConnectedKey)) != 0u : (GBE_local_lobby.owner_connected || GBE_local_lobby.state == 3u),
                        GBE_kDotaTeamPlayerPool);
                    GBE_DotaLobbyMemberState member = member_snapshot.member;
                    if (owner_member && read_member_raw_state) {
                        GBE_local_lobby.owner_team = member_snapshot.owner_team;
                        GBE_local_lobby.owner_slot = member_snapshot.owner_slot;
                        GBE_local_lobby.owner_hero_id = member_snapshot.owner_hero_id;
                        GBE_local_lobby.owner_connected = member_snapshot.owner_connected;
                    }
                    gbe::dota_lobby_flow::update_generic_lobby_member_snapshot_state(
                        member,
                        GBE_local_lobby.members,
                        gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game),
                        GBE_local_lobby.owner_steam_id,
                        GBE_local_lobby.owner_slot,
                        members,
                        preserve_launched_lan_members,
                        preserve_custom_game_runtime_members,
                        GBE_kDotaTeamGoodGuys,
                        GBE_kDotaTeamPlayerPool);
                    gbe::dota_lobby_flow::upsert_lobby_member(members, member);
                }

                gbe::dota_lobby_flow::merge_existing_lobby_members_for_generic_snapshot(
                    members,
                    GBE_local_lobby.members,
                    generic_members.empty(),
                    local_steam_id,
                    GBE_local_lobby.owner_steam_id,
                    preserve_launched_lan_members,
                    preserve_custom_game_runtime_members);

                gbe::dota_lobby_flow::upsert_owner_member_for_generic_snapshot(
                    members,
                    owner_in_generic_members,
                    generic_members.empty(),
                    GBE_local_lobby.owner_steam_id,
                    GBE_local_lobby.owner_account_id,
                    GBE_local_lobby.owner_team,
                    GBE_local_lobby.owner_slot,
                    GBE_local_lobby.owner_hero_id,
                    GBE_local_lobby.owner_connected || GBE_local_lobby.state == 3u);

                GBE_local_lobby.members = members;
                GBE_NormalizeDotaArcadeLobbyMemberSlots(GBE_local_lobby);
            }
        }
    }

    snapshot = GBE_local_lobby;
    return true;
}

void Steam_Game_Coordinator::GBE_RecordDotaLobbyCacheSubscriptionState(const std::string &message, const char *reason)
{
    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return;

    CMsgSOCacheSubscribed protomsg;
    if (!protomsg.ParseFromArray(proto_context.body, static_cast<int>(proto_context.body_size)))
        return;

    if (!protomsg.has_owner_soid() || protomsg.owner_soid().type() != 3u || protomsg.owner_soid().id() == 0)
        return;

    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && protomsg.owner_soid().id() != GBE_local_lobby.lobby_id)
        return;

    GBE_local_lobby.has_cache_version = protomsg.has_version();
    GBE_local_lobby.cache_version = protomsg.has_version() ? protomsg.version() : 0ull;
    GBE_local_lobby.has_cache_service_id = protomsg.has_service_id();
    GBE_local_lobby.cache_service_id = protomsg.has_service_id() ? protomsg.service_id() : 0u;
    GBE_local_lobby.cache_service_list.assign(protomsg.service_list().begin(), protomsg.service_list().end());
    GBE_local_lobby.has_cache_sync_version = protomsg.has_sync_version();
    GBE_local_lobby.cache_sync_version = protomsg.has_sync_version() ? protomsg.sync_version() : 0ull;

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "recorded lobby CacheSubscribed metadata reason=%s owner_id=%llu version_present=%u version=%llu service_id_present=%u service_id=%u service_list_count=%zu sync_version_present=%u sync_version=%llu",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(protomsg.owner_soid().id()),
        GBE_local_lobby.has_cache_version ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.cache_version),
        GBE_local_lobby.has_cache_service_id ? 1u : 0u,
        GBE_local_lobby.cache_service_id,
        GBE_local_lobby.cache_service_list.size(),
        GBE_local_lobby.has_cache_sync_version ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.cache_sync_version)
    );

    GBE_LogDotaSOCacheSubscribedSummary("GC_DOTA_SYNC", reason ? reason : "record_cache_subscribed_metadata", message);

    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0)
        GBE_PublishSharedDotaLobbyState(reason ? reason : "record_cache_subscribed_metadata");
}

void Steam_Game_Coordinator::GBE_PublishSharedDotaLobbyState(const char *reason)
{
    if (GBE_ShouldSuppressDotaAbandonedLobby(GBE_local_lobby.lobby_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "publish skipped for suppressed abandoned lobby reason=%s this=%p is_server=%u lobby_id=%llu active=%u state=%u game_state=%u",
            reason ? reason : "unknown",
            static_cast<void *>(this),
            is_server ? 1u : 0u,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.active ? 1u : 0u,
            GBE_local_lobby.state,
            GBE_local_lobby.game_state
        );
        return;
    }

    gbe::dota_lobby_state::publish_local_lobby_to_shared(GBE_local_lobby, is_server, GBE_shared_dota_lobby_state);

    GBE_DotaReconnectContext reconnect_context{};
    if (gbe::dota_lobby_state::build_reconnect_context(GBE_local_lobby, reconnect_context)) {
        GBE_SetRecentDotaReconnectContext(reconnect_context);
        GBE_ReconnectLog(
            "GBE_RECONNECT_DIAG",
            "cached recent dota reconnect context reason=%s server_id=%llu lobby_state=%u game_state=%u custom_game_id=%llu endpoint=%s",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(reconnect_context.server_id),
            reconnect_context.lobby_state,
            reconnect_context.game_state,
            static_cast<unsigned long long>(reconnect_context.custom_game_id),
            reconnect_context.connect
        );
    }

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "published shared lobby this=%p shared_lobby=%p reason=%s active=%u lobby_id=%llu generic_lobby_id=%llu match_id=%llu owner_steam_id=%llu owner_account_id=%u state=%u game_state=%u launch_phase=%s team=%u slot=%u connect=%s",
        static_cast<void *>(this),
        static_cast<void *>(&GBE_shared_dota_lobby_state),
        reason ? reason : "unknown",
        GBE_local_lobby.active ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.owner_steam_id),
        GBE_local_lobby.owner_account_id,
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase),
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot,
        GBE_local_lobby.connect.c_str()
    );

    const bool local_is_owner =
        settings &&
        GBE_local_lobby.owner_steam_id != 0ull &&
        settings->get_local_steam_id().ConvertToUint64() == GBE_local_lobby.owner_steam_id;
    if (is_server || (!is_server && local_is_owner && GBE_local_lobby.custom_game.game_id != 0ull && GBE_local_lobby.match_id != 0ull && GBE_local_lobby.launch_phase >= GBE_kDotaLaunchPhaseSetupSynced))
        GBE_PublishDotaPracticeLobbyMetadata(reason ? reason : "shared_lobby_state");
}

bool Steam_Game_Coordinator::GBE_MaybeNotifyDotaPracticeLobbyMembersChanged(const char *reason)
{
    if (is_server || gc_profile != GC_PROFILE_DOTA2)
        return false;
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.generic_lobby_id == 0)
        return false;
    if (GBE_local_lobby.state > 2u)
        return false;

    const std::vector<GBE_DotaLobbyMemberState> previous_members = GBE_local_lobby.members;
    const uint64 previous_owner_steam_id = GBE_local_lobby.owner_steam_id;
    const uint32 previous_owner_account_id = GBE_local_lobby.owner_account_id;
    const std::string previous_owner_name = GBE_local_lobby.owner_name;
    const uint32 previous_state = GBE_local_lobby.state;
    const uint32 previous_game_state = GBE_local_lobby.game_state;
    const uint64 previous_match_id = GBE_local_lobby.match_id;
    const uint64 previous_server_id = GBE_local_lobby.server_id;
    const std::string previous_connect = GBE_local_lobby.connect;
    const uint32 previous_game_start_time = GBE_local_lobby.game_start_time;
    const std::string previous_room_name = GBE_local_lobby.room_name;
    const uint32 previous_game_mode = GBE_local_lobby.game_mode;
    const uint32 previous_server_region = GBE_local_lobby.server_region;
    const std::string previous_lan_host_ping_location = GBE_local_lobby.lan_host_ping_location;
    const bool previous_allow_cheats = GBE_local_lobby.allow_cheats;
    const bool previous_fill_with_bots = GBE_local_lobby.fill_with_bots;
    const bool previous_allow_spectating = GBE_local_lobby.allow_spectating;
    const std::string previous_pass_key = GBE_local_lobby.pass_key;
    const uint32 previous_visibility = GBE_local_lobby.visibility;
    const uint32 previous_bot_difficulty_radiant = GBE_local_lobby.bot_difficulty_radiant;
    const uint32 previous_bot_difficulty_dire = GBE_local_lobby.bot_difficulty_dire;
    const uint64 previous_bot_radiant = GBE_local_lobby.bot_radiant;
    const uint64 previous_bot_dire = GBE_local_lobby.bot_dire;
    GBE_LocalLobby lobby{};
    if (!GBE_CaptureCurrentDotaLobbyStateWithPreviousSlots(reason ? reason : "generic_lobby_members_changed", previous_members, previous_owner_steam_id, lobby))
        return false;
    const bool owner_changed =
        previous_owner_steam_id != GBE_local_lobby.owner_steam_id ||
        previous_owner_account_id != GBE_local_lobby.owner_account_id ||
        previous_owner_name != GBE_local_lobby.owner_name;
    const bool runtime_changed =
        previous_state != GBE_local_lobby.state ||
        previous_game_state != GBE_local_lobby.game_state ||
        previous_match_id != GBE_local_lobby.match_id ||
        previous_server_id != GBE_local_lobby.server_id ||
        previous_connect != GBE_local_lobby.connect ||
        previous_game_start_time != GBE_local_lobby.game_start_time;
    const bool settings_changed =
        previous_room_name != GBE_local_lobby.room_name ||
        previous_game_mode != GBE_local_lobby.game_mode ||
        previous_server_region != GBE_local_lobby.server_region ||
        previous_lan_host_ping_location != GBE_local_lobby.lan_host_ping_location ||
        previous_allow_cheats != GBE_local_lobby.allow_cheats ||
        previous_fill_with_bots != GBE_local_lobby.fill_with_bots ||
        previous_allow_spectating != GBE_local_lobby.allow_spectating ||
        previous_pass_key != GBE_local_lobby.pass_key ||
        previous_visibility != GBE_local_lobby.visibility ||
        previous_bot_difficulty_radiant != GBE_local_lobby.bot_difficulty_radiant ||
        previous_bot_difficulty_dire != GBE_local_lobby.bot_difficulty_dire ||
        previous_bot_radiant != GBE_local_lobby.bot_radiant ||
        previous_bot_dire != GBE_local_lobby.bot_dire;
    const std::vector<GBE_DotaLobbyMemberState> joined_members = gbe::dota_lobby_flow::find_joined_lobby_members(previous_members, GBE_local_lobby.members);
    if (!owner_changed && !runtime_changed && !settings_changed && gbe::dota_lobby_flow::lobby_members_equal(previous_members, GBE_local_lobby.members))
        return false;

    // When lobby member state changes on the client during pre-game
    // (game_state == 0), clear the direct-connect callback dedup signature
    // and allow the private lobby snapshot to be replayed.  This ensures that
    // GameServerChangeRequested_t is re-sent with the real IP address when the
    // lobby state transitions during initial match setup.
    //
    // During an active match (game_state >= 1), do NOT clear the signature.
    // The host sends member-change updates continuously (e.g. when a player
    // disconnects), and clearing the signature would allow every subsequent
    // queued_state apply to re-fire GameServerChangeRequested_t, causing an
    // automatic reconnect loop.  Manual reconnect uses restore_client_runtime
    // which has its own signature-clearing logic (line ~11336-11350).
    if (!is_server && !gbe::dota_lobby_flow::lobby_members_equal(previous_members, GBE_local_lobby.members) && GBE_local_lobby.game_state == 0u) {
        GBE_last_dota_direct_connect_callback_signature.clear();
        GBE_dota_private_lobby_snapshot_replayed = false;
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "cleared direct connect signature and snapshot replay flag on member change reason=%s lobby_id=%llu state=%u game_state=%u",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state
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
    bool host_has_active_server_gc = false;
    {
        Steam_Client *sc = get_steam_client();
        if (sc && sc->steam_gameserver_game_coordinator) {
            host_has_active_server_gc = sc->steam_gameserver_game_coordinator->GBE_HasActiveServerLobby(GBE_local_lobby.lobby_id);
        }
    }
    if (!is_server && host_has_active_server_gc && previous_state < 3u && GBE_local_lobby.state >= 3u) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Skipping PLAYER PostGame cleanup on HOST client GC: server GC owns lobby LobbyID=%llu state=%u reason=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            reason ? reason : "generic_lobby_members_changed"
        );
    }
    const bool arcade_active_match =
        gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game) &&
        GBE_local_lobby.match_id != 0ull &&
        GBE_local_lobby.game_state >= 2u &&
        GBE_local_lobby.launch_phase >= GBE_kDotaLaunchPhaseRunQueued;
    if (!is_server && !host_has_active_server_gc && arcade_active_match && previous_state < 3u && GBE_local_lobby.state >= 3u) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Skipping PLAYER PostGame cleanup during arcade active match LobbyID=%llu state=%u game_state=%u launch_phase=%s reason=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase),
            reason ? reason : "generic_lobby_members_changed"
        );
    }
    if (!is_server && !host_has_active_server_gc && !arcade_active_match && previous_state < 3u && GBE_local_lobby.state >= 3u && GBE_local_lobby.lobby_id != 0) {
        const uint64 cleaning_lobby_id = GBE_local_lobby.lobby_id;
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] PLAYER PostGame cleanup: generic lobby transitioned to state=%u game_state=%u LobbyID=%llu reason=%s",
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            static_cast<unsigned long long>(cleaning_lobby_id),
            reason ? reason : "generic_lobby_members_changed"
        );

        // Send a final msg 26 with PostGame state so Dota sees the transition
        std::string postgame_response_26;
        if (GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(lobby, GBE_GetDotaLobbyOwnerName(), postgame_response_26, true)) {
            push_incoming_now(GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask, postgame_response_26);
        }

        // Clear Rich Presence
        GBE_ClearDotaPracticeLobbyLaunchRichPresence();

        // Reset local lobby and invalidate shared state
        GBE_ResetDotaPracticeLobbyLaunchPeripheralState();
        GBE_local_lobby = GBE_LocalLobby{};
        GBE_shared_dota_lobby_state = GBE_SharedDotaLobbyState{};
        GBE_last_dota_launch_state_pushed_game_state = 0;

        // Push CacheUnsubscribed (msg 25) so Dota knows the lobby SO is gone
        std::string response_25;
        if (gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(cleaning_lobby_id, response_25)) {
            push_incoming_now(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25);
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "PLAYER PostGame: pushed CacheUnsubscribed lobby_id=%llu size=%zu",
                static_cast<unsigned long long>(cleaning_lobby_id),
                response_25.size()
            );
        }

        if (settings && settings->get_lobby().ConvertToUint64() != 0)
            settings->set_lobby(k_steamIDNil);

        return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Detected generic lobby member/owner/runtime/settings change LobbyID=%llu generic_lobby_id=%llu old_members=%zu new_members=%zu old_owner=%llu new_owner=%llu old_state=%u new_state=%u old_game_state=%u new_game_state=%u old_server_id=%llu new_server_id=%llu old_connect=%s new_connect=%s settings_changed=%u old_cheats=%u new_cheats=%u reason=%s",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        previous_members.size(),
        GBE_local_lobby.members.size(),
        static_cast<unsigned long long>(previous_owner_steam_id),
        static_cast<unsigned long long>(GBE_local_lobby.owner_steam_id),
        previous_state,
        GBE_local_lobby.state,
        previous_game_state,
        GBE_local_lobby.game_state,
        static_cast<unsigned long long>(previous_server_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id),
        previous_connect.c_str(),
        GBE_local_lobby.connect.c_str(),
        settings_changed ? 1u : 0u,
        previous_allow_cheats ? 1u : 0u,
        GBE_local_lobby.allow_cheats ? 1u : 0u,
        reason ? reason : "generic_lobby_members_changed"
    );

    const uint64 local_steam_id = settings ? settings->get_local_steam_id().ConvertToUint64() : 0ull;
    const bool lan_launch_active =
        GBE_local_lobby.lan &&
        GBE_local_lobby.match_id != 0ull;

    std::string response_26;
    const bool sent_details_update = GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(lobby, GBE_GetDotaLobbyOwnerName(), response_26, is_server || lan_launch_active);
    if (sent_details_update) {
        const bool arcade_runtime_member_change =
            gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game) &&
            GBE_local_lobby.match_id != 0ull &&
            GBE_local_lobby.launch_phase >= GBE_kDotaLaunchPhaseRunQueued &&
            GBE_local_lobby.state >= 2u;
        const bool peer_lan_direct_launch =
            local_steam_id != 0ull &&
            GBE_local_lobby.owner_steam_id != 0ull &&
            local_steam_id != GBE_local_lobby.owner_steam_id &&
            GBE_local_lobby.lan &&
            GBE_local_lobby.match_id != 0ull &&
            GBE_local_lobby.state == 2u &&
            GBE_local_lobby.server_id == 0ull &&
            gbe::proto_wire::parse_dota_practice_lobby_connect_ipv4(gbe::proto_wire::get_dota_practice_lobby_first_connect_endpoint(GBE_local_lobby.connect)) != 0u;

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

    if (GBE_local_lobby.has_chat_channel && GBE_local_lobby.chat_channel_id != 0) {
        for (const GBE_DotaLobbyMemberState &joined_member : joined_members) {
            std::string joined_name;
            Steam_Client *steam_client = get_steam_client();
            if (steam_client && steam_client->steam_matchmaking && GBE_local_lobby.generic_lobby_id != 0ull) {
                const char *generic_name = steam_client->steam_matchmaking->GetLobbyMemberData(
                    CSteamID((uint64)GBE_local_lobby.generic_lobby_id),
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
            if (gbe::gc_message::build_dota_other_joined_channel_payload(GBE_local_lobby.chat_channel_id, joined_name, joined_member.steam_id, response_7013)) {
                push_incoming_now(GBE_kDotaOtherJoinedChannel | GBE_kProtoMask, response_7013);
                GBE_GC_DebugLog(
                    "GC_DOTA_LOBBY",
                    "[LOBBY] Sent 7013 other joined channel LobbyID=%llu channel_id=%llu steam_id=%llu name=%s reason=%s",
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    static_cast<unsigned long long>(GBE_local_lobby.chat_channel_id),
                    static_cast<unsigned long long>(joined_member.steam_id),
                    joined_name.c_str(),
                    reason ? reason : "generic_lobby_members_changed"
                );
            }
        }
    }

    if (GBE_local_lobby.has_chat_channel && GBE_local_lobby.chat_channel_id != 0) {
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
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(GBE_local_lobby.chat_channel_id),
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
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.generic_lobby_id == 0)
        return false;
    const bool after_chat_leave = std::strcmp(reason ? reason : "", "7272_leave_chat") == 0;
    if (!GBE_local_lobby.has_chat_channel && !after_chat_leave)
        return false;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking)
        return false;

    CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
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
        GBE_local_lobby.seen_local_in_generic_lobby = true;
        GBE_local_lobby.kicked_suppressed_logged = false;
        GBE_local_lobby.owner_adoption_suppressed_logged = false;
        return false;
    }

    if (!GBE_local_lobby.seen_local_in_generic_lobby) {
        if (!GBE_local_lobby.waiting_join_confirmation_logged) {
            GBE_local_lobby.waiting_join_confirmation_logged = true;
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Waiting for generic lobby join confirmation before treating local user as kicked. LobbyID=%llu generic_lobby_id=%llu reason=%s",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
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
    if (GBE_local_lobby.lan &&
        GBE_local_lobby.state >= 2u &&
        GBE_local_lobby.state < 3u &&
        GBE_local_lobby.match_id != 0ull &&
        !GBE_local_lobby.connect.empty()) {
        if (!GBE_local_lobby.kicked_suppressed_logged) {
            GBE_local_lobby.kicked_suppressed_logged = true;
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Suppressed kicked detection during launched LAN game (generic lobby member loss from P2P disconnect). "
                "LobbyID=%llu generic_lobby_id=%llu state=%u game_state=%u match_id=%llu connect=%s reason=%s",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64()),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state,
                static_cast<unsigned long long>(GBE_local_lobby.match_id),
                GBE_local_lobby.connect.c_str(),
                reason ? reason : "generic_lobby_members_changed"
            );
        }
        return false;
    }

    const uint64 lobby_id = GBE_local_lobby.lobby_id;
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
    ResetGCMemory("7081_kicked_from_lobby", false, false);
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
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.generic_lobby_id == 0)
        return false;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking)
        return false;

    CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
    if (!generic_lobby_id.IsLobby())
        return false;

    CSteamID generic_owner = steam_client->steam_matchmaking->GetLobbyOwner(generic_lobby_id);
    if (!generic_owner.IsValid() || generic_owner.ConvertToUint64() == 0 || generic_owner.ConvertToUint64() == GBE_local_lobby.owner_steam_id)
        return false;

    const uint64 previous_owner_steam_id = GBE_local_lobby.owner_steam_id;
    const uint64 new_owner_steam_id = generic_owner.ConvertToUint64();
    const uint64 local_steam_id = settings->get_local_steam_id().ConvertToUint64();

    const bool preserve_custom_game_launch_owner =
        gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game) &&
        GBE_local_lobby.match_id != 0ull &&
        GBE_local_lobby.launch_phase >= GBE_kDotaLaunchPhaseSetupSynced;
    if (preserve_custom_game_launch_owner) {
        if (!GBE_local_lobby.owner_adoption_suppressed_logged) {
            GBE_local_lobby.owner_adoption_suppressed_logged = true;
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Ignored generic lobby owner adoption during custom game launch reason=%s dota_lobby_id=%llu generic_lobby_id=%llu dota_owner=%llu generic_owner=%llu state=%u game_state=%u launch_phase=%s team=%u slot=%u",
                reason ? reason : "unknown",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
                static_cast<unsigned long long>(previous_owner_steam_id),
                static_cast<unsigned long long>(new_owner_steam_id),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state,
                GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase),
                GBE_local_lobby.owner_team,
                GBE_local_lobby.owner_slot
            );
        }
        return false;
    }

    if (!is_server &&
        local_steam_id != 0ull &&
        previous_owner_steam_id != 0ull &&
        previous_owner_steam_id != local_steam_id &&
        new_owner_steam_id == local_steam_id &&
        GBE_local_lobby.lan &&
        GBE_local_lobby.state == 2u &&
        GBE_local_lobby.match_id != 0ull &&
        !GBE_local_lobby.connect.empty()) {
        if (!GBE_local_lobby.owner_adoption_suppressed_logged) {
            GBE_local_lobby.owner_adoption_suppressed_logged = true;
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Ignored generic lobby owner adoption during launched LAN peer disconnect reason=%s dota_lobby_id=%llu generic_lobby_id=%llu dota_owner=%llu generic_owner=%llu local=%llu state=%u game_state=%u match_id=%llu connect=%s",
                reason ? reason : "unknown",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
                static_cast<unsigned long long>(previous_owner_steam_id),
                static_cast<unsigned long long>(new_owner_steam_id),
                static_cast<unsigned long long>(local_steam_id),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state,
                static_cast<unsigned long long>(GBE_local_lobby.match_id),
                GBE_local_lobby.connect.c_str()
            );
        }
        return false;
    }

    gbe::dota_lobby_flow::adopt_lobby_owner_member(
        GBE_local_lobby.members,
        new_owner_steam_id,
        generic_owner.GetAccountID(),
        GBE_kDotaTeamGoodGuys,
        GBE_local_lobby.state == 3u,
        GBE_local_lobby.owner_steam_id,
        GBE_local_lobby.owner_account_id,
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot,
        GBE_local_lobby.owner_hero_id,
        GBE_local_lobby.owner_connected);
    if (new_owner_steam_id == local_steam_id) {
        GBE_local_lobby.owner_name = std::string(settings->get_local_name());
    } else {
        const char *owner_name = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerNameKey);
        GBE_local_lobby.owner_name = std::string(owner_name ? owner_name : "Lobby Host");
    }

    if (new_owner_steam_id == local_steam_id) {
        GBE_PublishDotaPracticeLobbyLocalMemberData(reason ? reason : "adopt_generic_owner");
        GBE_PublishDotaPracticeLobbyMetadata(reason ? reason : "adopt_generic_owner");
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Adopted generic lobby owner reason=%s dota_lobby_id=%llu generic_lobby_id=%llu old_owner=%llu new_owner=%llu local_is_owner=%u owner_name=%s team=%u slot=%u",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        static_cast<unsigned long long>(previous_owner_steam_id),
        static_cast<unsigned long long>(new_owner_steam_id),
        new_owner_steam_id == local_steam_id ? 1u : 0u,
        GBE_local_lobby.owner_name.c_str(),
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot
    );
    return true;
}

void Steam_Game_Coordinator::GBE_PublishDotaPracticeLobbyLocalMemberData(const char *reason)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.generic_lobby_id == 0)
        return;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking)
        return;

    CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
    if (!generic_lobby_id.IsLobby())
        return;

    const uint64 local_steam_id = settings->get_local_steam_id().ConvertToUint64();
    const GBE_DotaLobbyMemberState *local_member = nullptr;
    for (const GBE_DotaLobbyMemberState &member : GBE_local_lobby.members) {
        if (member.steam_id == local_steam_id) {
            local_member = &member;
            break;
        }
    }
    if (!local_member)
        return;

    const GBE_DotaLobbyMemberPublishData publish_data = gbe::dota_lobby_flow::compose_lobby_member_publish_data(*local_member, std::string(settings->get_local_name()));
    steam_client->steam_matchmaking->SetLobbyMemberData(generic_lobby_id, GBE_kDotaGenericLobbyMemberTeamKey, std::to_string(publish_data.team).c_str());
    steam_client->steam_matchmaking->SetLobbyMemberData(generic_lobby_id, GBE_kDotaGenericLobbyMemberSlotKey, std::to_string(publish_data.slot).c_str());
    steam_client->steam_matchmaking->SetLobbyMemberData(generic_lobby_id, GBE_kDotaGenericLobbyMemberHeroKey, std::to_string(publish_data.hero_id).c_str());
    steam_client->steam_matchmaking->SetLobbyMemberData(generic_lobby_id, GBE_kDotaGenericLobbyMemberConnectedKey, publish_data.connected ? "1" : "0");
    steam_client->steam_matchmaking->SetLobbyMemberData(generic_lobby_id, GBE_kDotaGenericLobbyMemberNameKey, publish_data.name.c_str());

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Published generic lobby member data reason=%s dota_lobby_id=%llu generic_lobby_id=%llu steam_id=%llu team=%u slot=%u hero=%u connected=%u name=%s",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        static_cast<unsigned long long>(local_member->steam_id),
        publish_data.team,
        publish_data.slot,
        publish_data.hero_id,
        publish_data.connected ? 1u : 0u,
        publish_data.name.c_str()
    );
}

void Steam_Game_Coordinator::GBE_PublishDotaPracticeLobbyMetadata(const char *reason)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.generic_lobby_id == 0)
        return;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking)
        return;

    CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
    if (!generic_lobby_id.IsLobby())
        return;

    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyMarkerKey, GBE_kDotaGenericLobbyMarkerValue);
    const GBE_DotaLobbyScalarPublishData scalar_publish_data = gbe::dota_lobby_flow::compose_lobby_scalar_publish_data(
        GBE_local_lobby.lobby_id,
        GBE_local_lobby.owner_steam_id,
        GBE_local_lobby.owner_account_id,
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        GBE_local_lobby.match_id,
        GBE_local_lobby.game_start_time,
        GBE_local_lobby.tv_secret_code,
        GBE_local_lobby.tv_port);
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyDotaLobbyIdKey, scalar_publish_data.dota_lobby_id.c_str());
    const std::string normalized_connect = gbe::proto_wire::normalize_dota_practice_lobby_connect(GBE_local_lobby.connect);
    const GBE_DotaLobbyMetadataPublishData publish_data = gbe::dota_lobby_flow::compose_lobby_metadata_publish_data(
        GBE_local_lobby.room_name,
        GBE_local_lobby.owner_name,
        std::string(settings->get_local_name()),
        GBE_local_lobby.match_id,
        GBE_local_lobby.server_id,
        normalized_connect);
    const GBE_DotaLobbyOptionsPublishData options_publish_data = gbe::dota_lobby_flow::compose_lobby_options_publish_data(
        GBE_local_lobby.game_mode,
        GBE_local_lobby.server_region,
        GBE_local_lobby.lan_host_ping_location,
        GBE_local_lobby.pass_key,
        GBE_local_lobby.allow_cheats,
        GBE_local_lobby.fill_with_bots,
        GBE_local_lobby.allow_spectating,
        GBE_local_lobby.visibility,
        GBE_local_lobby.bot_difficulty_radiant,
        GBE_local_lobby.bot_difficulty_dire,
        GBE_local_lobby.bot_radiant,
        GBE_local_lobby.bot_dire);
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyRoomNameKey, publish_data.room_name.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyGameModeKey, options_publish_data.game_mode.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyServerRegionKey, options_publish_data.server_region.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyLanPingKey, options_publish_data.lan_host_ping_location.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyPassKeyKey, options_publish_data.pass_key.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyAllowCheatsKey, options_publish_data.allow_cheats.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyFillWithBotsKey, options_publish_data.fill_with_bots.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyAllowSpectatingKey, options_publish_data.allow_spectating.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyVisibilityKey, options_publish_data.visibility.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyBotDifficultyRadiantKey, options_publish_data.bot_difficulty_radiant.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyBotDifficultyDireKey, options_publish_data.bot_difficulty_dire.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyBotRadiantKey, options_publish_data.bot_radiant.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyBotDireKey, options_publish_data.bot_dire.c_str());
    const GBE_DotaCustomGamePublishData custom_game_publish_data = gbe::dota_custom_game::compose_custom_game_publish_data(GBE_local_lobby.custom_game);
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomGameModeKey, custom_game_publish_data.mode.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomMapNameKey, custom_game_publish_data.map_name.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomDifficultyKey, custom_game_publish_data.difficulty.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomGameIdKey, custom_game_publish_data.game_id.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomMinPlayersKey, custom_game_publish_data.min_players.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomMaxPlayersKey, custom_game_publish_data.max_players.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomGameCrcKey, custom_game_publish_data.crc.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomGameTimestampKey, custom_game_publish_data.timestamp.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomGamePenaltiesKey, custom_game_publish_data.penalties.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerSteamIdKey, scalar_publish_data.owner_steam_id.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerAccountIdKey, scalar_publish_data.owner_account_id.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerNameKey, publish_data.owner_name.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyStateKey, scalar_publish_data.state.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyGameStateKey, scalar_publish_data.game_state.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyMatchIdKey, scalar_publish_data.match_id.c_str());
    GBE_local_lobby.connect = publish_data.connect;
    GBE_local_lobby.server_id = publish_data.server_id;
    if (GBE_shared_dota_lobby_state.valid && GBE_shared_dota_lobby_state.lobby_id == GBE_local_lobby.lobby_id) {
        GBE_shared_dota_lobby_state.connect = GBE_local_lobby.connect;
        if (gbe::dota_lobby_flow::should_clear_lobby_server_id_for_metadata_publish(GBE_local_lobby.match_id)) {
            GBE_shared_dota_lobby_state.server_id = 0ull;
        }
    }

    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyServerIdKey, std::to_string(publish_data.server_id).c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyConnectKey, publish_data.connect.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyGameStartTimeKey, scalar_publish_data.game_start_time.c_str());
    if (GBE_local_lobby.tv_secret_code != 0)
        steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyTvSecretCodeKey, scalar_publish_data.tv_secret_code.c_str());
    if (GBE_local_lobby.tv_port != 0)
        steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyTvPortKey, scalar_publish_data.tv_port.c_str());

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Published generic lobby metadata reason=%s dota_lobby_id=%llu generic_lobby_id=%llu room=%s mode=%u region=%u pass_len=%zu cheats=%u bots=%u spectating=%u visibility=%u bot_diff_r=%u bot_diff_d=%u state=%u game_state=%u match_id=%llu server_id=%llu connect=%s",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        GBE_local_lobby.room_name.c_str(),
        GBE_local_lobby.game_mode,
        GBE_local_lobby.server_region,
        GBE_local_lobby.pass_key.size(),
        GBE_local_lobby.allow_cheats ? 1u : 0u,
        GBE_local_lobby.fill_with_bots ? 1u : 0u,
        GBE_local_lobby.allow_spectating ? 1u : 0u,
        GBE_local_lobby.visibility,
        GBE_local_lobby.bot_difficulty_radiant,
        GBE_local_lobby.bot_difficulty_dire,
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id),
        publish_data.connect.c_str()
    );
}

bool Steam_Game_Coordinator::GBE_TryRecoverDotaReconnectContextFromGenericLobbies(uint64 local_steam_id, GBE_DotaReconnectContext *out)
{
    if (!out || gc_profile != GC_PROFILE_DOTA2)
        return false;

    const std::vector<GBE_LocalLobby> snapshots = GBE_GetDotaGenericLobbySnapshots("recover_reconnect_context");
    for (const GBE_LocalLobby &snapshot : snapshots) {
        if (!snapshot.active ||
            snapshot.custom_game.game_id == 0ull ||
            snapshot.server_id == 0ull ||
            snapshot.connect.empty() ||
            (snapshot.state < 2u && snapshot.game_state < 2u))
            continue;
        if (local_steam_id != 0ull && snapshot.owner_steam_id == local_steam_id)
            continue;

        bool local_in_lobby = local_steam_id == 0ull;
        for (const GBE_DotaLobbyMemberState &member : snapshot.members) {
            if (member.steam_id == local_steam_id) {
                local_in_lobby = true;
                break;
            }
        }
        if (!local_in_lobby)
            continue;

        *out = GBE_DotaReconnectContext{};
        out->server_id = snapshot.server_id;
        out->lobby_state = snapshot.state;
        out->game_state = snapshot.game_state;
        out->custom_game_id = snapshot.custom_game.game_id;
        const std::string endpoint = gbe::proto_wire::get_dota_practice_lobby_first_connect_endpoint(snapshot.connect);
        std::strncpy(out->connect, endpoint.c_str(), sizeof(out->connect) - 1);
        out->connect[sizeof(out->connect) - 1] = '\0';
        out->owner_steam_id = snapshot.owner_steam_id;
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "recovered arcade reconnect context from generic lobby local=%llu dota_lobby_id=%llu server_id=%llu state=%u game_state=%u custom_game_id=%llu endpoint=%s owner=%llu",
            static_cast<unsigned long long>(local_steam_id),
            static_cast<unsigned long long>(snapshot.lobby_id),
            static_cast<unsigned long long>(out->server_id),
            out->lobby_state,
            out->game_state,
            static_cast<unsigned long long>(out->custom_game_id),
            out->connect,
            static_cast<unsigned long long>(out->owner_steam_id)
        );
        return true;
    }

    return false;
}

bool Steam_Game_Coordinator::GBE_FindDotaGenericLobbyByDotaLobbyId(uint64 dota_lobby_id, CSteamID &generic_lobby_id, GBE_LocalLobby *lobby_snapshot, const char *reason)
{
    const std::vector<GBE_LocalLobby> snapshots = GBE_GetDotaGenericLobbySnapshots(reason ? reason : "find_generic_by_dota_lobby_id");
    for (const GBE_LocalLobby &snapshot : snapshots) {
        if (snapshot.lobby_id != dota_lobby_id)
            continue;
        generic_lobby_id = CSteamID((uint64)snapshot.generic_lobby_id);
        if (lobby_snapshot)
            *lobby_snapshot = snapshot;
        return generic_lobby_id.IsLobby();
    }
    return false;
}

void Steam_Game_Coordinator::GBE_SyncSettingsLobbyFromGenericLobby(const char *reason)
{
    if (!settings)
        return;

    CSteamID target_lobby_id = k_steamIDNil;
    if (GBE_local_lobby.active && GBE_local_lobby.generic_lobby_id != 0) {
        CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
        if (generic_lobby_id.IsLobby())
            target_lobby_id = generic_lobby_id;
    }

    const CSteamID previous_lobby_id = settings->get_lobby();
    if (previous_lobby_id == target_lobby_id)
        return;

    settings->set_lobby(target_lobby_id);
    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "synced settings lobby from generic reason=%s active=%u dota_lobby_id=%llu generic_lobby_id=%llu old_settings_lobby=%llu new_settings_lobby=%llu",
        reason ? reason : "unknown",
        GBE_local_lobby.active ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        static_cast<unsigned long long>(previous_lobby_id.ConvertToUint64()),
        static_cast<unsigned long long>(target_lobby_id.ConvertToUint64())
    );
}

void Steam_Game_Coordinator::GBE_RestoreSharedDotaLobbyState(const char *reason)
{
    if (!GBE_shared_dota_lobby_state.valid) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "restore skipped this=%p shared_lobby=%p reason=%s valid=0",
            static_cast<void *>(this),
            static_cast<void *>(&GBE_shared_dota_lobby_state),
            reason ? reason : "unknown"
        );
        return;
    }

    if (GBE_ShouldSuppressDotaAbandonedLobby(GBE_shared_dota_lobby_state.lobby_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "restore skipped for suppressed abandoned lobby this=%p shared_lobby=%p reason=%s lobby_id=%llu active=%u state=%u game_state=%u",
            static_cast<void *>(this),
            static_cast<void *>(&GBE_shared_dota_lobby_state),
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_shared_dota_lobby_state.lobby_id),
            GBE_shared_dota_lobby_state.active ? 1u : 0u,
            GBE_shared_dota_lobby_state.state,
            GBE_shared_dota_lobby_state.game_state
        );
        return;
    }

    if (!is_server) {
        if (!GBE_shared_dota_lobby_state.active)
            return;
        if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && GBE_local_lobby.lobby_id != GBE_shared_dota_lobby_state.lobby_id)
            return;

        if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
            // Only adopt if this GC instance belongs to a lobby member (or owner).
            // Bot/fake-player GC instances have steam_ids that are not lobby members
            // and should not adopt the shared lobby state (avoids hundreds of redundant
            // adopt cycles during post-game GC re-initialization).
            const uint64 local_sid = settings ? settings->get_local_steam_id().ConvertToUint64() : 0;
            if (local_sid != 0 && local_sid != GBE_shared_dota_lobby_state.owner_steam_id) {
                bool is_member = false;
                for (const auto &m : GBE_shared_dota_lobby_state.members) {
                    if (m.steam_id == local_sid) { is_member = true; break; }
                }
                if (!is_member) {
                    return;  // Not a lobby participant; skip adopt
                }
            }

            gbe::dota_lobby_state::adopt_shared_lobby_to_local(GBE_shared_dota_lobby_state, false, true, GBE_local_lobby);

            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "adopted full shared lobby on client this=%p shared_lobby=%p reason=%s lobby_id=%llu state=%u game_state=%u server_id=%llu",
                static_cast<void *>(this),
                static_cast<void *>(&GBE_shared_dota_lobby_state),
                reason ? reason : "unknown",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state,
                static_cast<unsigned long long>(GBE_local_lobby.server_id)
            );
            GBE_SyncSettingsLobbyFromGenericLobby(reason ? reason : "restore_client_full_adopt");
            GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(reason ? reason : "restore_client_full_adopt");
            GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot(reason ? reason : "restore_client_full_adopt");
            GBE_PushDotaLoginSyncMessages();
            return;
        }

        bool changed = false;
        if (GBE_local_lobby.generic_lobby_id != GBE_shared_dota_lobby_state.generic_lobby_id) {
            GBE_local_lobby.generic_lobby_id = GBE_shared_dota_lobby_state.generic_lobby_id;
            changed = true;
        }
        const uint64 previous_server_id = GBE_local_lobby.server_id;
        const std::string previous_connect = GBE_local_lobby.connect;

        const std::string shared_connect = gbe::proto_wire::normalize_dota_practice_lobby_connect(GBE_shared_dota_lobby_state.connect);
        if (!shared_connect.empty() && GBE_local_lobby.connect != shared_connect) {
            GBE_local_lobby.connect = shared_connect;
            changed = true;
        }

        if (GBE_shared_dota_lobby_state.match_id != 0 && GBE_local_lobby.match_id != GBE_shared_dota_lobby_state.match_id) {
            GBE_local_lobby.match_id = GBE_shared_dota_lobby_state.match_id;
            changed = true;
        }

        const bool ignore_shared_readyup_regression =
            gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game) &&
            GBE_local_lobby.match_id != 0ull &&
            GBE_local_lobby.launch_phase >= GBE_kDotaLaunchPhaseRunQueued &&
            GBE_local_lobby.state == 2u &&
            GBE_local_lobby.game_state >= 2u &&
            GBE_shared_dota_lobby_state.state == 4u;
        if (GBE_local_lobby.state != GBE_shared_dota_lobby_state.state && !ignore_shared_readyup_regression) {
            GBE_local_lobby.state = GBE_shared_dota_lobby_state.state;
            changed = true;
        } else if (ignore_shared_readyup_regression) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Ignored shared READYUP regression reason=%s lobby_id=%llu local_state=%u local_game_state=%u shared_state=%u shared_game_state=%u launch_phase=%s",
                reason ? reason : "restore_shared_lobby_state",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state,
                GBE_shared_dota_lobby_state.state,
                GBE_shared_dota_lobby_state.game_state,
                GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase)
            );
        }

        if (GBE_local_lobby.game_state != GBE_shared_dota_lobby_state.game_state) {
            GBE_local_lobby.game_state = GBE_shared_dota_lobby_state.game_state;
            changed = true;
        }

        if (GBE_shared_dota_lobby_state.game_start_time != 0 && GBE_local_lobby.game_start_time != GBE_shared_dota_lobby_state.game_start_time) {
            GBE_local_lobby.game_start_time = GBE_shared_dota_lobby_state.game_start_time;
            changed = true;
        }

        if (GBE_local_lobby.room_name != GBE_shared_dota_lobby_state.room_name) {
            GBE_local_lobby.room_name = GBE_shared_dota_lobby_state.room_name;
            changed = true;
        }

        if (GBE_local_lobby.game_mode != GBE_shared_dota_lobby_state.game_mode) {
            GBE_local_lobby.game_mode = GBE_shared_dota_lobby_state.game_mode;
            changed = true;
        }

        if (GBE_local_lobby.server_region != GBE_shared_dota_lobby_state.server_region) {
            GBE_local_lobby.server_region = GBE_shared_dota_lobby_state.server_region;
            changed = true;
        }

        if (GBE_local_lobby.lan != GBE_shared_dota_lobby_state.lan) {
            GBE_local_lobby.lan = GBE_shared_dota_lobby_state.lan;
            changed = true;
        }

        if (GBE_local_lobby.lan_host_ping_location != GBE_shared_dota_lobby_state.lan_host_ping_location) {
            GBE_local_lobby.lan_host_ping_location = GBE_shared_dota_lobby_state.lan_host_ping_location;
            changed = true;
        }

        if (GBE_local_lobby.allow_cheats != GBE_shared_dota_lobby_state.allow_cheats) {
            GBE_local_lobby.allow_cheats = GBE_shared_dota_lobby_state.allow_cheats;
            changed = true;
        }

        if (GBE_local_lobby.fill_with_bots != GBE_shared_dota_lobby_state.fill_with_bots) {
            GBE_local_lobby.fill_with_bots = GBE_shared_dota_lobby_state.fill_with_bots;
            changed = true;
        }

        if (GBE_local_lobby.allow_spectating != GBE_shared_dota_lobby_state.allow_spectating) {
            GBE_local_lobby.allow_spectating = GBE_shared_dota_lobby_state.allow_spectating;
            changed = true;
        }

        if (GBE_local_lobby.pass_key != GBE_shared_dota_lobby_state.pass_key) {
            GBE_local_lobby.pass_key = GBE_shared_dota_lobby_state.pass_key;
            changed = true;
        }

        if (GBE_local_lobby.visibility != GBE_shared_dota_lobby_state.visibility) {
            GBE_local_lobby.visibility = GBE_shared_dota_lobby_state.visibility;
            changed = true;
        }

        if (GBE_local_lobby.bot_difficulty_radiant != GBE_shared_dota_lobby_state.bot_difficulty_radiant) {
            GBE_local_lobby.bot_difficulty_radiant = GBE_shared_dota_lobby_state.bot_difficulty_radiant;
            changed = true;
        }

        if (GBE_local_lobby.bot_difficulty_dire != GBE_shared_dota_lobby_state.bot_difficulty_dire) {
            GBE_local_lobby.bot_difficulty_dire = GBE_shared_dota_lobby_state.bot_difficulty_dire;
            changed = true;
        }

        if (GBE_local_lobby.bot_radiant != GBE_shared_dota_lobby_state.bot_radiant) {
            GBE_local_lobby.bot_radiant = GBE_shared_dota_lobby_state.bot_radiant;
            changed = true;
        }

        if (GBE_local_lobby.bot_dire != GBE_shared_dota_lobby_state.bot_dire) {
            GBE_local_lobby.bot_dire = GBE_shared_dota_lobby_state.bot_dire;
            changed = true;
        }

        if (!gbe::dota_custom_game::custom_game_details_equal(GBE_local_lobby.custom_game, GBE_shared_dota_lobby_state.custom_game)) {
            GBE_local_lobby.custom_game = GBE_shared_dota_lobby_state.custom_game;
            changed = true;
        }

        if (GBE_local_lobby.owner_connected != GBE_shared_dota_lobby_state.owner_connected) {
            GBE_local_lobby.owner_connected = GBE_shared_dota_lobby_state.owner_connected;
            changed = true;
        }

        if (GBE_local_lobby.launch_phase != GBE_shared_dota_lobby_state.launch_phase) {
            GBE_local_lobby.launch_phase = GBE_shared_dota_lobby_state.launch_phase;
            changed = true;
        }

        if (GBE_local_lobby.launch_4511_seen != GBE_shared_dota_lobby_state.launch_4511_seen) {
            GBE_local_lobby.launch_4511_seen = GBE_shared_dota_lobby_state.launch_4511_seen;
            changed = true;
        }

        if (GBE_local_lobby.owner_team != GBE_shared_dota_lobby_state.owner_team) {
            GBE_local_lobby.owner_team = GBE_shared_dota_lobby_state.owner_team;
            changed = true;
        }

        if (GBE_local_lobby.owner_slot != GBE_shared_dota_lobby_state.owner_slot) {
            GBE_local_lobby.owner_slot = GBE_shared_dota_lobby_state.owner_slot;
            changed = true;
        }

        if (GBE_shared_dota_lobby_state.owner_hero_id != 0 && GBE_local_lobby.owner_hero_id != GBE_shared_dota_lobby_state.owner_hero_id) {
            GBE_local_lobby.owner_hero_id = GBE_shared_dota_lobby_state.owner_hero_id;
            changed = true;
        }

        if (!gbe::dota_lobby_flow::lobby_members_equal(GBE_local_lobby.members, GBE_shared_dota_lobby_state.members)) {
            GBE_local_lobby.members = GBE_shared_dota_lobby_state.members;
            changed = true;
        }

        if (GBE_local_lobby.has_cache_version != GBE_shared_dota_lobby_state.has_cache_version ||
                GBE_local_lobby.cache_version != GBE_shared_dota_lobby_state.cache_version) {
            GBE_local_lobby.has_cache_version = GBE_shared_dota_lobby_state.has_cache_version;
            GBE_local_lobby.cache_version = GBE_shared_dota_lobby_state.cache_version;
            changed = true;
        }

        if (GBE_local_lobby.has_cache_service_id != GBE_shared_dota_lobby_state.has_cache_service_id ||
                GBE_local_lobby.cache_service_id != GBE_shared_dota_lobby_state.cache_service_id) {
            GBE_local_lobby.has_cache_service_id = GBE_shared_dota_lobby_state.has_cache_service_id;
            GBE_local_lobby.cache_service_id = GBE_shared_dota_lobby_state.cache_service_id;
            changed = true;
        }

        if (GBE_local_lobby.cache_service_list != GBE_shared_dota_lobby_state.cache_service_list) {
            GBE_local_lobby.cache_service_list = GBE_shared_dota_lobby_state.cache_service_list;
            changed = true;
        }

        if (GBE_local_lobby.has_cache_sync_version != GBE_shared_dota_lobby_state.has_cache_sync_version ||
                GBE_local_lobby.cache_sync_version != GBE_shared_dota_lobby_state.cache_sync_version) {
            GBE_local_lobby.has_cache_sync_version = GBE_shared_dota_lobby_state.has_cache_sync_version;
            GBE_local_lobby.cache_sync_version = GBE_shared_dota_lobby_state.cache_sync_version;
            changed = true;
        }

        if (changed) {
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "adopted shared runtime on client this=%p shared_lobby=%p reason=%s lobby_id=%llu generic_lobby_id=%llu old_server_id=%llu new_server_id=%llu old_connect=%s new_connect=%s",
                static_cast<void *>(this),
                static_cast<void *>(&GBE_shared_dota_lobby_state),
                reason ? reason : "unknown",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
                static_cast<unsigned long long>(previous_server_id),
                static_cast<unsigned long long>(GBE_local_lobby.server_id),
                previous_connect.c_str(),
                GBE_local_lobby.connect.c_str()
            );
        }

        // NOTE: Signature clearing was previously done here for LAN reconnect,
        // but it caused every restore_client_runtime invocation (including
        // routine lobby state syncs while the player is still connected) to
        // re-fire GameServerChangeRequested_t, producing an automatic
        // reconnect loop.  Manual reconnect works through Dota's own rich
        // presence "connect" field (+connect <LAN IP>:<port>) which is kept
        // up-to-date by GBE_ReapplyDotaPracticeLobbyLaunchRichPresence below,
        // so GameServerChangeRequested_t does not need to fire again.

        GBE_SyncSettingsLobbyFromGenericLobby(reason ? reason : "restore_client_runtime");
        GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(reason ? reason : "restore_client_runtime");
        GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot(reason ? reason : "restore_client_runtime");
        GBE_PushDotaLoginSyncMessages();
        return;
    }

    gbe::dota_lobby_state::adopt_shared_lobby_to_local(GBE_shared_dota_lobby_state, true, false, GBE_local_lobby);

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "restored shared lobby this=%p shared_lobby=%p reason=%s active=%u lobby_id=%llu generic_lobby_id=%llu match_id=%llu owner_steam_id=%llu owner_account_id=%u state=%u game_state=%u team=%u slot=%u connect=%s",
        static_cast<void *>(this),
        static_cast<void *>(&GBE_shared_dota_lobby_state),
        reason ? reason : "unknown",
        GBE_local_lobby.active ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.owner_steam_id),
        GBE_local_lobby.owner_account_id,
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot,
        GBE_local_lobby.connect.c_str()
    );
    GBE_SyncSettingsLobbyFromGenericLobby(reason ? reason : "restore_server_or_full");
    GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(reason ? reason : "restore_server_or_full");
}

uint64 Steam_Game_Coordinator::GBE_GetDotaLobbyOwnerSteamId() const
{
    if (GBE_local_lobby.owner_steam_id != 0)
        return GBE_local_lobby.owner_steam_id;

    if (GBE_shared_dota_lobby_state.owner_steam_id != 0)
        return GBE_shared_dota_lobby_state.owner_steam_id;

    return settings->get_local_steam_id().ConvertToUint64();
}

uint32 Steam_Game_Coordinator::GBE_GetDotaLobbyOwnerAccountId() const
{
    if (GBE_local_lobby.owner_account_id != 0)
        return GBE_local_lobby.owner_account_id;

    if (GBE_shared_dota_lobby_state.owner_account_id != 0)
        return GBE_shared_dota_lobby_state.owner_account_id;

    return settings->get_local_steam_id().GetAccountID();
}

void Steam_Game_Coordinator::GBE_LeaveGenericLobby()
{
    GBE_ResetDotaPracticeLobbyLaunchPeripheralState();
    if (GBE_local_lobby.generic_lobby_id == 0) {
        GBE_SyncSettingsLobbyFromGenericLobby("leave_generic_lobby_noop");
        return;
    }

    Steam_Client *steam_client = get_steam_client();
    if (steam_client && steam_client->steam_matchmaking) {
        CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
        if (generic_lobby_id.IsLobby())
            steam_client->steam_matchmaking->LeaveLobby(generic_lobby_id);
    }

    GBE_local_lobby.generic_lobby_id = 0;
    GBE_SyncSettingsLobbyFromGenericLobby("leave_generic_lobby");
}

bool Steam_Game_Coordinator::GBE_SyncGenericLobbyGameServer(const char *reason)
{
    if (!is_server)
        return false;

    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.generic_lobby_id == 0 || GBE_local_lobby.server_id == 0)
        return false;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking || !steam_client->steam_gameserver)
        return false;

    Steam_GameServer *game_server = steam_client->steam_gameserver;
    if (!game_server->BLoggedOn())
        return false;

    uint32 lobby_ip = game_server->GetPublicIP_old();
    if (GBE_local_lobby.lan) {
        const uint32 connect_ip = gbe::proto_wire::parse_dota_practice_lobby_connect_ipv4(GBE_local_lobby.connect);
        if (connect_ip != 0)
            lobby_ip = connect_ip;
        else if (network)
            lobby_ip = network->getOwnIP();
    }
    constexpr uint16 lobby_port = 27015u;
    CSteamID lobby_steam_id((uint64)GBE_local_lobby.generic_lobby_id);
    CSteamID gameserver_steam_id((uint64)GBE_local_lobby.server_id);
    const bool has_ip_server_id = GBE_local_lobby.server_id != 0ull && !gameserver_steam_id.IsValid() && gbe::proto_wire::parse_dota_practice_lobby_connect_ipv4(GBE_local_lobby.connect) != 0u;
    if (!lobby_steam_id.IsLobby() || (!gameserver_steam_id.IsValid() && !has_ip_server_id))
        return false;

    uint32 previous_ip = 0;
    uint16 previous_port = 0;
    CSteamID previous_server_id = k_steamIDNil;
    const bool had_previous_gameserver = steam_client->steam_matchmaking->GetLobbyGameServer(
        lobby_steam_id,
        &previous_ip,
        &previous_port,
        &previous_server_id);

    if (had_previous_gameserver &&
            previous_server_id == gameserver_steam_id &&
            previous_ip == lobby_ip &&
            previous_port == lobby_port) {
        return false;
    }

    steam_client->steam_matchmaking->SetLobbyGameServer(
        lobby_steam_id,
        lobby_ip,
        lobby_port,
        gameserver_steam_id);

    if (steam_client->steam_user) {
        steam_client->steam_user->AdvertiseGame(gameserver_steam_id, lobby_ip, lobby_port);
    }

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "synced generic lobby gameserver reason=%s dota_lobby_id=%llu generic_lobby_id=%llu server_id=%llu ip=%s port=%u had_previous=%u previous_server_id=%llu previous_ip=%s previous_port=%u",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id),
        gbe::proto_wire::format_ipv4(lobby_ip).c_str(),
        static_cast<unsigned>(lobby_port),
        had_previous_gameserver ? 1u : 0u,
        static_cast<unsigned long long>(previous_server_id.ConvertToUint64()),
        gbe::proto_wire::format_ipv4(previous_ip).c_str(),
        static_cast<unsigned>(previous_port)
    );

    return true;
}

bool Steam_Game_Coordinator::GBE_TrySyncDotaLobbyServerIdFromGameServer(const char *reason)
{
    if (!is_server)
        return false;

    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.match_id == 0)
        return false;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_gameserver)
        return false;

    Steam_GameServer *game_server = steam_client->steam_gameserver;
    if (!game_server->BLoggedOn())
        return false;

    uint32 connect_server_ip = gbe::proto_wire::parse_dota_practice_lobby_connect_ipv4(GBE_local_lobby.connect);
    if (connect_server_ip == 0u && GBE_local_lobby.lan && network)
        connect_server_ip = network->getOwnIP();

    const uint64 derived_server_id = gbe::dota_custom_game::derive_practice_lobby_ip_server_id(connect_server_ip);
    if (GBE_local_lobby.server_id == 0ull)
        return false;

    const uint64 previous_server_id = GBE_local_lobby.server_id;
    GBE_local_lobby.server_id = derived_server_id;
    if (GBE_shared_dota_lobby_state.valid && GBE_shared_dota_lobby_state.lobby_id == GBE_local_lobby.lobby_id)
        GBE_shared_dota_lobby_state.server_id = derived_server_id;

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "synced lobby server_id from connect endpoint reason=%s lobby_id=%llu match_id=%llu old=%llu derived=%llu lan_ip=%s custom_game_id=%llu",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(previous_server_id),
        static_cast<unsigned long long>(derived_server_id),
        gbe::proto_wire::format_ipv4(connect_server_ip).c_str(),
        static_cast<unsigned long long>(GBE_local_lobby.custom_game.game_id)
    );

    GBE_PublishSharedDotaLobbyState(reason ? reason : "server_id_clear");
    GBE_PushDotaLaunchStateToClientPeer(reason ? reason : "server_id_clear");
    return true;
}
