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
#include "gbe_dota_lobby_state.h"
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

bool Steam_Game_Coordinator::GBE_TryRecoverDotaReconnectContextFromGenericLobbies(uint64 local_steam_id, GBE_DotaReconnectContext *out)
{
    if (!out || gc_profile != GC_PROFILE_DOTA2)
        return false;

    const std::vector<GBE_LocalLobby> snapshots = GBE_GetDotaGenericLobbySnapshots("recover_reconnect_context");
    for (const GBE_LocalLobby &snapshot : snapshots) {
        const gbe::dota_reconnect::Source source =
            gbe::dota_reconnect::source_from_generic_lobby(snapshot, local_steam_id);
        GBE_DotaReconnectContext recovered{};
        if (gbe::dota_reconnect::build_context(source, recovered) != gbe::dota_reconnect::RejectReason::None)
            continue;
        *out = recovered;
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

    gbe::dota_lobby_state::LocalLobbyOwner local_lobby(GBE_local_lobby);
    local_lobby.apply("leave_generic_lobby", [](GBE_LocalLobby &lobby) {
        gbe::dota_lobby_state::apply_lobby_generic_lobby_id(lobby, 0ull);
    });
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
    gbe::dota_lobby_state::LocalLobbyOwner local_lobby(GBE_local_lobby);
    local_lobby.apply(reason ? reason : "server_id_clear", [derived_server_id](GBE_LocalLobby &lobby) {
        gbe::dota_lobby_state::apply_lobby_server_id(lobby, derived_server_id);
    });
    const auto shared_update_result = GBE_SharedLobbyStore().compare_update(
        GBE_local_lobby.generation,
        [&](GBE_SharedDotaLobbyState &shared_lobby) {
            if (shared_lobby.valid && shared_lobby.lobby_id == GBE_local_lobby.lobby_id)
                shared_lobby.server_id = derived_server_id;
        });
    if (shared_update_result == gbe::dota_lobby_state::StoreUpdateResult::StaleGeneration) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipped stale shared lobby server_id update reason=%s lobby_id=%llu generation=%llu derived=%llu",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.generation),
            static_cast<unsigned long long>(derived_server_id));
    }

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
