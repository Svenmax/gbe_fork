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
#include <vector>
#include <steammessages.pb.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/econ_gcmessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>
#include <tf2/gcsystemmsgs.pb.h>
#include <tf2/tf_gcmessages.pb.h>

using namespace gamecoordinator::tf2;

// --- Lobby-snapshot/build helper member functions (moved from steam_game_coordinator.cpp) ---

bool Steam_Game_Coordinator::GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay(const std::string &player_name, std::string &message)
{
    GBE_LocalLobby lobby{};
    if (!GBE_CaptureCurrentDotaLobbyState("cache_template_replay", lobby))
        return false;

    return GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay(lobby, player_name, message);
}


bool Steam_Game_Coordinator::GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay(const GBE_LocalLobby &lobby, const std::string &player_name, std::string &message)
{
    const uint64 owner_steam_id = lobby.owner_steam_id != 0 ? lobby.owner_steam_id : GBE_GetDotaLobbyOwnerSteamId();
    const uint32 owner_account_id = lobby.owner_account_id != 0 ? lobby.owner_account_id : GBE_GetDotaLobbyOwnerAccountId();

    return GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl(
        owner_steam_id,
        owner_account_id,
        lobby.lobby_id,
        lobby.state,
        lobby.game_state,
        lobby.server_id,
        lobby.match_id,
        lobby.game_start_time,
        lobby.connect,
        player_name,
        lobby.room_name,
        lobby.game_mode,
        lobby.server_region,
        lobby.lan,
        lobby.lan_host_ping_location,
        lobby.allow_cheats,
        lobby.fill_with_bots,
        lobby.allow_spectating,
        lobby.visibility,
        lobby.bot_difficulty_radiant,
        lobby.bot_difficulty_dire,
        lobby.bot_radiant,
        lobby.bot_dire,
        lobby.owner_team,
        lobby.owner_slot,
        lobby.owner_hero_id,
        lobby.pass_key,
        lobby.members,
        false,
        false,
        owner_account_id,
        &lobby.custom_game,
        message);
}


bool Steam_Game_Coordinator::GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayload(const std::string &player_name, std::string &message)
{
    GBE_LocalLobby lobby{};
    if (!GBE_CaptureCurrentDotaLobbyState("cache_payload", lobby))
        return false;

    return GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayload(lobby, player_name, message);
}


bool Steam_Game_Coordinator::GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayload(const GBE_LocalLobby &lobby, const std::string &player_name, std::string &message)
{
    const uint64 owner_steam_id = lobby.owner_steam_id != 0 ? lobby.owner_steam_id : GBE_GetDotaLobbyOwnerSteamId();
    const uint32 owner_account_id = lobby.owner_account_id != 0 ? lobby.owner_account_id : GBE_GetDotaLobbyOwnerAccountId();

    return GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl(
        owner_steam_id,
        lobby.lobby_id,
        lobby.state,
        lobby.game_state,
        lobby.server_id,
        lobby.match_id,
        lobby.game_start_time,
        lobby.connect,
        player_name,
        lobby.room_name,
        lobby.game_mode,
        lobby.server_region,
        lobby.lan,
        lobby.lan_host_ping_location,
        lobby.allow_cheats,
        lobby.fill_with_bots,
        lobby.allow_spectating,
        lobby.visibility,
        lobby.bot_difficulty_radiant,
        lobby.bot_difficulty_dire,
        lobby.bot_radiant,
        lobby.bot_dire,
        lobby.owner_team,
        lobby.owner_slot,
        lobby.owner_hero_id,
        lobby.members,
        lobby.has_broadcast_channel,
        lobby.broadcast_channel_id,
        lobby.broadcast_country_code,
        lobby.broadcast_description,
        lobby.broadcast_language_code,
        lobby.pass_key,
        owner_account_id,
        &lobby.custom_game,
        message);
}


bool Steam_Game_Coordinator::GBE_BuildCurrentDotaPracticeLobbyDetailsUpdate(const GBE_LocalLobby &lobby, const std::string &player_name, std::string &message)
{
    const uint64 owner_steam_id = lobby.owner_steam_id != 0 ? lobby.owner_steam_id : GBE_GetDotaLobbyOwnerSteamId();
    const uint32 owner_account_id = lobby.owner_account_id != 0 ? lobby.owner_account_id : GBE_GetDotaLobbyOwnerAccountId();

    return GBE_AdaptDotaPracticeLobbyDetailsUpdatePayload(
        owner_steam_id,
        owner_account_id,
        lobby.lobby_id,
        lobby.state,
        lobby.game_state,
        lobby.server_id,
        lobby.match_id,
        lobby.game_start_time,
        lobby.connect,
        player_name,
        lobby.room_name,
        lobby.game_mode,
        lobby.server_region,
        lobby.lan,
        lobby.lan_host_ping_location,
        lobby.allow_cheats,
        lobby.fill_with_bots,
        lobby.allow_spectating,
        lobby.visibility,
        lobby.bot_difficulty_radiant,
        lobby.bot_difficulty_dire,
        lobby.bot_radiant,
        lobby.bot_dire,
        lobby.owner_team,
        lobby.owner_slot,
        lobby.owner_hero_id,
        lobby.members,
        lobby.has_broadcast_channel,
        lobby.broadcast_channel_id,
        lobby.broadcast_country_code,
        lobby.broadcast_description,
        lobby.broadcast_language_code,
        lobby.pass_key,
        message,
        &lobby.custom_game);
}


bool Steam_Game_Coordinator::GBE_CaptureCurrentDotaLobbyStateWithPreviousSlots(
    const char *reason,
    const std::vector<GBE_DotaLobbyMemberState> &previous_members,
    uint64 previous_owner_steam_id,
    GBE_LocalLobby &snapshot)
{
    if (!GBE_CaptureCurrentDotaLobbyState(reason, snapshot, false))
        return false;

    const uint64 new_owner_steam_id = GBE_local_lobby.owner_steam_id;
    const size_t before_count = GBE_local_lobby.members.size();
    gbe::dota_lobby_flow::preserve_lobby_owner_transfer_slots(GBE_local_lobby.members, previous_members, previous_owner_steam_id, new_owner_steam_id);
    if (!gbe::dota_lobby_flow::lobby_members_equal(snapshot.members, GBE_local_lobby.members)) {
        snapshot.members = GBE_local_lobby.members;
        if (is_server)
            GBE_PublishSharedDotaLobbyState(reason ? reason : "owner_transfer_preserve_slots");

        size_t previous_owner_index = 0;
        size_t new_owner_index = 0;
        const bool has_previous_owner_index = gbe::dota_lobby_flow::find_lobby_member_index(previous_members, previous_owner_steam_id, previous_owner_index);
        const bool has_new_owner_index = gbe::dota_lobby_flow::find_lobby_member_index(GBE_local_lobby.members, new_owner_steam_id, new_owner_index);
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Preserved owner transfer member slots reason=%s lobby_id=%llu old_owner=%llu new_owner=%llu old_owner_index=%lld new_owner_index=%lld before_members=%zu after_members=%zu",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(previous_owner_steam_id),
            static_cast<unsigned long long>(new_owner_steam_id),
            has_previous_owner_index ? static_cast<long long>(previous_owner_index) : -1ll,
            has_new_owner_index ? static_cast<long long>(new_owner_index) : -1ll,
            before_count,
            GBE_local_lobby.members.size()
        );
    }

    return true;
}


std::vector<GBE_LocalLobby> Steam_Game_Coordinator::GBE_GetDotaGenericLobbySnapshots(const char *reason)
{
    std::vector<GBE_LocalLobby> snapshots;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking)
        return snapshots;

    steam_client->steam_matchmaking->RefreshLobbyCallbacksForDota();

    const std::vector<CSteamID> generic_lobbies = steam_client->steam_matchmaking->GetLobbyListSnapshot();
    for (const CSteamID &generic_lobby_id : generic_lobbies) {
        if (!generic_lobby_id.IsLobby())
            continue;

        const char *marker = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyMarkerKey);
        if (std::string(marker ? marker : "") != GBE_kDotaGenericLobbyMarkerValue)
            continue;

        const uint64 dota_lobby_id = gbe::proto_wire::parse_uint64_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyDotaLobbyIdKey));
        if (dota_lobby_id == 0)
            continue;

        const bool repaired_owner = steam_client->steam_matchmaking->RepairLobbyOwnerIfMissing(generic_lobby_id, reason ? reason : "generic_lobby_snapshot");
        if (repaired_owner) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Repaired missing generic lobby owner while reading snapshot reason=%s dota_lobby_id=%llu generic_lobby_id=%llu",
                reason ? reason : "generic_lobby_snapshot",
                static_cast<unsigned long long>(dota_lobby_id),
                static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64())
            );
        }

        CSteamID generic_owner_id = steam_client->steam_matchmaking->GetLobbyOwner(generic_lobby_id);

        GBE_LocalLobby snapshot{};
        snapshot.active = true;
        snapshot.lobby_id = dota_lobby_id;
        snapshot.generic_lobby_id = generic_lobby_id.ConvertToUint64();
        const GBE_DotaLobbyOwnerSnapshotData owner_snapshot = gbe::dota_lobby_flow::compose_lobby_owner_snapshot_data(
            gbe::proto_wire::parse_uint64_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerSteamIdKey)),
            gbe::proto_wire::parse_uint32_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerAccountIdKey)),
            steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerNameKey),
            generic_owner_id.ConvertToUint64(),
            generic_owner_id.GetAccountID(),
            generic_owner_id.IsValid(),
            settings->get_local_steam_id().ConvertToUint64(),
            std::string(settings->get_local_name()),
            settings->get_local_steam_id().GetAccountID(),
            "Lobby Host");
        const GBE_DotaLobbyOwnerPublishData owner_publish_data = gbe::dota_lobby_flow::compose_lobby_owner_publish_data(owner_snapshot, std::string(settings->get_local_name()));
        snapshot.owner_steam_id = owner_publish_data.owner_steam_id;
        snapshot.owner_account_id = owner_publish_data.owner_account_id;
        snapshot.owner_name = owner_publish_data.owner_name;
        if (owner_publish_data.should_publish_local_owner) {
            steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerSteamIdKey, std::to_string(snapshot.owner_steam_id).c_str());
            steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerAccountIdKey, std::to_string(snapshot.owner_account_id).c_str());
            steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerNameKey, snapshot.owner_name.c_str());
        }
        const GBE_DotaGenericLobbySnapshotScalarData snapshot_scalar = gbe::dota_lobby_flow::compose_generic_lobby_snapshot_scalar_data(
            steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyRoomNameKey),
            gbe::proto_wire::parse_uint32_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyGameModeKey)),
            gbe::proto_wire::parse_uint32_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyServerRegionKey)),
            gbe::proto_wire::parse_uint32_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyStateKey)),
            gbe::proto_wire::parse_uint32_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyGameStateKey)),
            gbe::proto_wire::parse_uint64_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyMatchIdKey)),
            gbe::proto_wire::parse_uint64_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyServerIdKey)),
            gbe::proto_wire::normalize_dota_practice_lobby_connect(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyConnectKey)),
            gbe::proto_wire::parse_uint32_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyGameStartTimeKey)),
            steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyLanPingKey),
            steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyPassKeyKey),
            gbe::proto_wire::parse_uint32_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyAllowCheatsKey)) != 0u,
            gbe::proto_wire::parse_uint32_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyFillWithBotsKey)) != 0u,
            gbe::proto_wire::parse_uint32_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyAllowSpectatingKey)) != 0u,
            gbe::proto_wire::parse_uint32_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyVisibilityKey)),
            gbe::proto_wire::parse_uint32_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyBotDifficultyRadiantKey)),
            gbe::proto_wire::parse_uint32_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyBotDifficultyDireKey)),
            gbe::proto_wire::parse_uint64_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyBotRadiantKey)),
            gbe::proto_wire::parse_uint64_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyBotDireKey)),
            gbe::proto_wire::parse_uint64_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyTvSecretCodeKey)),
            gbe::proto_wire::parse_uint32_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyTvPortKey)));
        snapshot.room_name = snapshot_scalar.room_name;
        snapshot.game_mode = snapshot_scalar.game_mode;
        snapshot.server_region = snapshot_scalar.server_region;
        snapshot.lan = snapshot_scalar.lan;
        snapshot.lan_host_ping_location = snapshot_scalar.lan_host_ping_location;
        snapshot.allow_cheats = snapshot_scalar.allow_cheats;
        snapshot.fill_with_bots = snapshot_scalar.fill_with_bots;
        snapshot.allow_spectating = snapshot_scalar.allow_spectating;
        snapshot.visibility = snapshot_scalar.visibility;
        snapshot.bot_difficulty_radiant = snapshot_scalar.bot_difficulty_radiant;
        snapshot.bot_difficulty_dire = snapshot_scalar.bot_difficulty_dire;
        snapshot.bot_radiant = snapshot_scalar.bot_radiant;
        snapshot.bot_dire = snapshot_scalar.bot_dire;
        snapshot.state = snapshot_scalar.state;
        snapshot.game_state = snapshot_scalar.game_state;
        snapshot.match_id = snapshot_scalar.match_id;
        snapshot.server_id = snapshot_scalar.server_id;
        snapshot.connect = snapshot_scalar.connect;
        snapshot.game_start_time = snapshot_scalar.game_start_time;
        snapshot.pass_key = snapshot_scalar.pass_key;
        snapshot.tv_secret_code = snapshot_scalar.tv_secret_code;
        snapshot.tv_port = snapshot_scalar.tv_port;
        snapshot.custom_game = gbe::dota_custom_game::compose_snapshot_custom_game_details(
            steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomGameModeKey),
            steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomMapNameKey),
            steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomDifficultyKey),
            steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomGameIdKey),
            steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomMinPlayersKey),
            steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomMaxPlayersKey),
            steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomGameCrcKey),
            steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomGameTimestampKey),
            steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyCustomGamePenaltiesKey));
        snapshot.owner_team = GBE_kDotaTeamGoodGuys;
        snapshot.owner_slot = 1u;
        snapshot.owner_connected = false;
        const std::vector<CSteamID> generic_members = steam_client->steam_matchmaking->GetLobbyMemberListSnapshot(generic_lobby_id);
        bool owner_in_generic_members = false;
        for (const CSteamID &member_id : generic_members) {
            if (!member_id.IsValid())
                continue;
            if (member_id.ConvertToUint64() == snapshot.owner_steam_id)
                owner_in_generic_members = true;

            const char *member_team_raw = steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberTeamKey);
            const char *member_slot_raw = steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberSlotKey);
            const GBE_DotaGenericLobbyMemberSnapshotInput member_snapshot_input{
                member_id.ConvertToUint64(),
                member_id.GetAccountID(),
                snapshot.owner_steam_id,
                snapshot.owner_account_id,
                snapshot.owner_team,
                snapshot.owner_slot,
                snapshot.owner_hero_id,
                snapshot.owner_connected,
                std::string(member_team_raw ? member_team_raw : ""),
                std::string(member_slot_raw ? member_slot_raw : ""),
                steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberHeroKey),
                steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberConnectedKey),
                GBE_kDotaTeamPlayerPool};
            const GBE_DotaLobbyMemberSnapshotData member_snapshot = gbe::dota_lobby_flow::compose_generic_lobby_member_snapshot_data(member_snapshot_input);
            gbe::dota_lobby_flow::apply_generic_lobby_member_snapshot(
                snapshot.members,
                member_snapshot,
                gbe::dota_custom_game::has_custom_game_details(snapshot.custom_game),
                snapshot.owner_steam_id,
                snapshot.owner_team,
                snapshot.owner_slot,
                snapshot.owner_hero_id,
                snapshot.owner_connected,
                GBE_kDotaTeamGoodGuys,
                GBE_kDotaTeamPlayerPool);
        }
        GBE_DotaLobbyMemberState owner{};
        owner.steam_id = snapshot.owner_steam_id;
        owner.account_id = snapshot.owner_account_id;
        owner.team = snapshot.owner_team;
        owner.slot = snapshot.owner_slot;
        owner.hero_id = snapshot.owner_hero_id;
        owner.connected = snapshot.owner_connected;
        if (owner_in_generic_members || generic_members.empty())
            gbe::dota_lobby_flow::upsert_lobby_member(snapshot.members, owner);

        snapshots.push_back(snapshot);
    }

    GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Read generic lobby snapshots reason=%s count=%zu", reason ? reason : "unknown", snapshots.size());
    return snapshots;
}


void Steam_Game_Coordinator::GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot(const char *reason)
{
    if (is_server) {
        GBE_ClearDotaPrivateLobbySnapshotReplayed();
        return;
    }

    GBE_LocalLobby lobby{};
    if (!GBE_CaptureCurrentDotaLobbyState(reason ? reason : "replay_current_private_lobby_snapshot", lobby, false)) {
        GBE_ClearDotaPrivateLobbySnapshotReplayed();
        return;
    }

    if (GBE_ShouldSuppressDotaAbandonedLobby(lobby.lobby_id)) {
        GBE_ClearDotaPrivateLobbySnapshotReplayed();
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipped replaying current private lobby snapshot for suppressed abandoned lobby reason=%s lobby_id=%llu state=%u game_state=%u",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(lobby.lobby_id),
            lobby.state,
            lobby.game_state
        );
        return;
    }

    if (gbe::dota_custom_game::has_custom_game_details(lobby.custom_game) &&
        lobby.match_id != 0ull &&
        lobby.launch_phase >= GBE_kDotaLaunchPhaseRunQueued) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipped replaying current private lobby snapshot during arcade run reason=%s lobby_id=%llu state=%u game_state=%u launch_phase=%s",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(lobby.lobby_id),
            lobby.state,
            lobby.game_state,
            GBE_DescribeDotaLaunchPhase(lobby.launch_phase)
        );
        return;
    }

    const bool ready_for_private_lobby_snapshot =
        lobby.state == 2u &&
        lobby.game_state >= 2u;

    if (!ready_for_private_lobby_snapshot) {
        GBE_ClearDotaPrivateLobbySnapshotReplayed();
        return;
    }

    if (GBE_HasReplayedDotaPrivateLobbySnapshot())
        return;

    const bool lan_launch_active =
        lobby.lan &&
        lobby.match_id != 0ull;

    std::string response_24;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed(lobby, GBE_GetDotaLobbyOwnerName(), response_24, is_server || lan_launch_active)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "failed building current private lobby snapshot 24 reason=%s lobby_id=%llu state=%u game_state=%u server_id=%llu",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(lobby.lobby_id),
            lobby.state,
            lobby.game_state,
            static_cast<unsigned long long>(lobby.server_id)
        );
        return;
    }

    std::string response_26;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(lobby, GBE_GetDotaLobbyOwnerName(), response_26, is_server || lan_launch_active)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "failed building current private lobby snapshot 26 reason=%s lobby_id=%llu state=%u game_state=%u server_id=%llu",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(lobby.lobby_id),
            lobby.state,
            lobby.game_state,
            static_cast<unsigned long long>(lobby.server_id)
        );
        return;
    }

    GBE_RecordDotaLobbyCacheSubscriptionState(response_24, reason ? reason : "replay_current_private_lobby_snapshot");
    push_incoming_now(GBE_kDotaCacheSubscribed | GBE_kProtoMask, response_24);
    push_incoming_now(
        GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask,
        response_26,
        true,
        lobby.state,
        lobby.game_state
    );
    GBE_MarkDotaPrivateLobbySnapshotReplayed();

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "replayed current private lobby snapshot reason=%s lobby_id=%llu state=%u game_state=%u server_id=%llu size24=%zu size26=%zu",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(lobby.lobby_id),
        lobby.state,
        lobby.game_state,
        static_cast<unsigned long long>(lobby.server_id),
        response_24.size(),
        response_26.size()
    );
}


bool Steam_Game_Coordinator::GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed(const GBE_LocalLobby &lobby, const std::string &player_name, std::string &message, bool preserve_server_id)
{
    const uint64 owner_steam_id = lobby.owner_steam_id != 0 ? lobby.owner_steam_id : GBE_GetDotaLobbyOwnerSteamId();
    const uint32 owner_account_id = lobby.owner_account_id != 0 ? lobby.owner_account_id : GBE_GetDotaLobbyOwnerAccountId();
    const bool launch_started = lobby.match_id != 0;
    const std::string effective_player_name = player_name.empty() ? GBE_GetDotaLobbyOwnerName() : player_name;
    const std::string effective_connect = gbe::dota_custom_game::format_practice_lobby_connect_for_custom_game(lobby.connect, &lobby.custom_game);
    uint64 server_candidate_id = 0ull;
    if (preserve_server_id && launch_started) {
        if (lobby.server_id == 0ull) {
            if (is_server && settings)
                server_candidate_id = settings->get_local_steam_id().ConvertToUint64();
            if (server_candidate_id == 0ull && !is_server && settings && owner_steam_id == settings->get_local_steam_id().ConvertToUint64()) {
                Steam_Client *steam_client = get_steam_client();
                if (steam_client && steam_client->settings_server)
                    server_candidate_id = steam_client->settings_server->get_local_steam_id().ConvertToUint64();
            }
        }
    }
    const GBE_DotaAuthoritativeLobbyPayloadData payload_data = gbe::dota_lobby_flow::compose_authoritative_lobby_payload_data(
        preserve_server_id,
        launch_started,
        lobby.server_id,
        server_candidate_id,
        effective_connect);
    GBE_LocalLobby effective_lobby = lobby;
    effective_lobby.server_id = payload_data.server_id;
    effective_lobby.connect = payload_data.connect;

    if (gbe::dota_lobby_flow::should_use_current_practice_lobby_payload_for_cache_subscribed(
            launch_started,
            lobby.lan,
            lobby.members.size()))
        return GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayload(effective_lobby, effective_player_name, message);

    if (launch_started && owner_steam_id != 0 && owner_account_id != 0) {
        if (GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedTemplate(
                owner_account_id,
                owner_steam_id,
                lobby.lobby_id,
                lobby.state,
                lobby.game_state,
                payload_data.server_id,
                lobby.match_id,
                lobby.game_start_time,
                payload_data.connect,
                effective_player_name,
                lobby.room_name,
                lobby.game_mode,
                lobby.server_region,
                lobby.lan,
                lobby.lan_host_ping_location,
                lobby.allow_cheats,
                lobby.fill_with_bots,
                lobby.allow_spectating,
                lobby.visibility,
                lobby.bot_difficulty_radiant,
                lobby.bot_difficulty_dire,
                lobby.bot_radiant,
                lobby.bot_dire,
                lobby.owner_team,
                lobby.owner_slot,
                lobby.owner_hero_id,
                lobby.pass_key,
                owner_account_id,
                message,
                &lobby.custom_game)) {
            return true;
        }
    }

    return GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay(effective_lobby, effective_player_name, message);
}


bool Steam_Game_Coordinator::GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(const GBE_LocalLobby &lobby, const std::string &player_name, std::string &message, bool preserve_server_id)
{
    const uint64 owner_steam_id = lobby.owner_steam_id != 0 ? lobby.owner_steam_id : GBE_GetDotaLobbyOwnerSteamId();
    const uint32 owner_account_id = lobby.owner_account_id != 0 ? lobby.owner_account_id : GBE_GetDotaLobbyOwnerAccountId();
    const std::string effective_player_name = player_name.empty() ? GBE_GetDotaLobbyOwnerName() : player_name;
    const std::string effective_connect = gbe::dota_custom_game::format_practice_lobby_connect_for_custom_game(lobby.connect, &lobby.custom_game);
    const uint32 startup_account_id = gbe::dota_gc_wire::get_dota_practice_lobby_startup_account_id_for_state(owner_account_id, lobby.state, lobby.game_state);
    uint64 server_candidate_id = 0ull;
    if (preserve_server_id && lobby.match_id != 0) {
        if (lobby.server_id == 0ull) {
            if (is_server && settings)
                server_candidate_id = settings->get_local_steam_id().ConvertToUint64();
            if (server_candidate_id == 0ull && !is_server && settings && owner_steam_id == settings->get_local_steam_id().ConvertToUint64()) {
                Steam_Client *steam_client = get_steam_client();
                if (steam_client && steam_client->settings_server)
                    server_candidate_id = steam_client->settings_server->get_local_steam_id().ConvertToUint64();
            }
        }
    }
    const GBE_DotaAuthoritativeLobbyPayloadData payload_data = gbe::dota_lobby_flow::compose_authoritative_lobby_payload_data(
        preserve_server_id,
        lobby.match_id != 0,
        lobby.server_id,
        server_candidate_id,
        effective_connect);
    GBE_LocalLobby effective_lobby = lobby;
    effective_lobby.server_id = payload_data.server_id;
    effective_lobby.connect = payload_data.connect;

    if (gbe::dota_lobby_flow::should_use_current_practice_lobby_payload_for_details_update(
            lobby.match_id,
            lobby.lan,
            lobby.members.size(),
            lobby.custom_game.game_id))
        return GBE_BuildCurrentDotaPracticeLobbyDetailsUpdate(effective_lobby, effective_player_name, message);

    if (owner_steam_id != 0 && owner_account_id != 0) {
        if (GBE_ReplayDotaPracticeLobbyOfficial26Payload(
                GBE_kDotaOfficial032PracticeLobby26Hex,
                "authoritative practice lobby 26",
                owner_account_id,
                owner_steam_id,
                lobby.lobby_id,
                payload_data.server_id,
                lobby.match_id,
                lobby.game_start_time,
                payload_data.connect,
                effective_player_name,
                lobby.room_name,
                lobby.game_mode,
                lobby.server_region,
                lobby.lan,
                lobby.lan_host_ping_location,
                lobby.allow_cheats,
                lobby.fill_with_bots,
                lobby.allow_spectating,
                lobby.visibility,
                lobby.bot_difficulty_radiant,
                lobby.bot_difficulty_dire,
                lobby.bot_radiant,
                lobby.bot_dire,
                lobby.owner_team,
                lobby.owner_slot,
                lobby.owner_hero_id,
                lobby.pass_key,
                lobby.state,
                lobby.game_state,
                startup_account_id != 0u,
                startup_account_id,
                message,
                &lobby.custom_game)) {
            return true;
        }
    }

    return GBE_BuildCurrentDotaPracticeLobbyDetailsUpdate(effective_lobby, effective_player_name, message);
}
