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
#include "gbe_dota_gc_diagnostics.h"
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
                const uint32 generic_lobby_state = gbe::proto_wire::parse_uint32_or_zero(generic_lobby_state_raw.c_str());
                const uint32 generic_lobby_game_state = gbe::proto_wire::parse_uint32_or_zero(generic_lobby_game_state_raw.c_str());
                const gbe::dota_lobby_state::GenericLobbyStateCapturePlan generic_state_plan =
                    gbe::dota_lobby_state::compose_generic_lobby_state_capture_plan(
                        GBE_local_lobby,
                        !generic_lobby_state_raw.empty(),
                        generic_lobby_state,
                        !generic_lobby_game_state_raw.empty(),
                        generic_lobby_game_state,
                        GBE_kDotaLaunchPhaseSetupSynced);
                if (generic_state_plan.ignored_stale_state) {
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
                }
                gbe::dota_lobby_state::apply_generic_lobby_state_capture_plan(GBE_local_lobby, generic_state_plan);
                gbe::dota_lobby_state::apply_generic_lobby_runtime_identity_capture_plan(
                    GBE_local_lobby,
                    gbe::dota_lobby_state::compose_generic_lobby_runtime_identity_capture_plan(
                        GBE_local_lobby,
                        generic_room_name,
                        generic_match_id_raw,
                        generic_server_id_raw,
                        generic_connect,
                        generic_game_start_time_raw));
                gbe::dota_lobby_state::apply_generic_lobby_options_capture_plan(
                    GBE_local_lobby,
                    gbe::dota_lobby_state::compose_generic_lobby_options_capture_plan(
                        generic_allow_cheats_raw,
                        generic_fill_with_bots_raw,
                        generic_allow_spectating_raw,
                        generic_visibility_raw,
                        generic_bot_difficulty_radiant_raw,
                        generic_bot_difficulty_dire_raw,
                        generic_bot_radiant_raw,
                        generic_bot_dire_raw));
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

                const bool custom_runtime_member_refresh =
                    gbe::dota_diagnostic::reason_from_string(reason ? reason : "") ==
                    gbe::dota_diagnostic::Reason::CustomRuntimeMemberRefresh;
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
                    const char *member_hero_raw = read_member_raw_state
                        ? steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberHeroKey)
                        : nullptr;
                    const bool has_member_hero = member_hero_raw && member_hero_raw[0] != '\0';
                    const uint32 member_hero_id = has_member_hero
                        ? gbe::proto_wire::parse_uint32_or_zero(member_hero_raw)
                        : (owner_member ? GBE_local_lobby.owner_hero_id : 0u);
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
                        member_hero_id,
                        read_member_raw_state ? gbe::proto_wire::parse_uint32_or_zero(steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberConnectedKey)) != 0u : (GBE_local_lobby.owner_connected || GBE_local_lobby.state == 3u),
                        GBE_kDotaTeamPlayerPool);
                    GBE_DotaLobbyMemberState member = member_snapshot.member;
                    if (owner_member && read_member_raw_state) {
                        GBE_local_lobby.owner_team = member_snapshot.owner_team;
                        GBE_local_lobby.owner_slot = member_snapshot.owner_slot;
                        if (member_snapshot.owner_hero_id != 0u)
                            GBE_ApplyOwnerHeroId(member_snapshot.owner_hero_id, "generic_member_snapshot_owner");
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
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

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

    const auto publish_result = GBE_SharedLobbyStore().update_if_generation_current_or_newer(
        GBE_local_lobby.generation,
        [&](GBE_SharedDotaLobbyState &shared_lobby) {
            gbe::dota_lobby_state::publish_local_lobby_to_shared(
                GBE_local_lobby,
                is_server,
                shared_lobby);
        });
    if (publish_result == gbe::dota_lobby_state::StoreUpdateResult::StaleGeneration) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "publish skipped for stale lobby generation reason=%s lobby_id=%llu generation=%llu",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.generation));
        return;
    }

    GBE_DotaReconnectContext reconnect_context{};
    const auto reconnect_source = gbe::dota_reconnect::source_from_local_lobby(GBE_local_lobby);
    if (gbe::dota_reconnect::build_context(reconnect_source, reconnect_context) ==
        gbe::dota_reconnect::RejectReason::None) {
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
        static_cast<void *>(&GBE_SharedLobbyStore()),
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
    const auto shared_update_result = GBE_SharedLobbyStore().compare_update(
        GBE_local_lobby.generation,
        [&](GBE_SharedDotaLobbyState &shared_lobby) {
            if (!shared_lobby.valid || shared_lobby.lobby_id != GBE_local_lobby.lobby_id)
                return;

            shared_lobby.connect = GBE_local_lobby.connect;
            if (gbe::dota_lobby_flow::should_clear_lobby_server_id_for_metadata_publish(GBE_local_lobby.match_id))
                shared_lobby.server_id = 0ull;
        });
    if (shared_update_result == gbe::dota_lobby_state::StoreUpdateResult::StaleGeneration) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipped stale shared lobby metadata update reason=%s lobby_id=%llu generation=%llu",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.generation));
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
