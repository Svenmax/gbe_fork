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

void Steam_Game_Coordinator::on_client_connected(CSteamID steam_id)
{
    if (!steam_id.BIndividualAccount())
        return;

    if (is_server && gc_profile == GC_PROFILE_DOTA2) {
        GBE_RestoreSharedDotaLobbyState("on_client_connected");

        const uint64 connected_steam_id = steam_id.ConvertToUint64();
        const uint64 owner_steam_id = GBE_GetDotaLobbyOwnerSteamId();
        if (gc_initialized && GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && connected_steam_id != 0 && connected_steam_id == owner_steam_id) {
            if (GBE_ShouldSuppressDotaAbandonedLobby(GBE_local_lobby.lobby_id)) {
                GBE_GC_DebugLog(
                    "GC_DOTA_SYNC",
                    "ignoring owner reconnect for suppressed abandoned lobby steam_id=%llu lobby_id=%llu state=%u game_state=%u",
                    static_cast<unsigned long long>(connected_steam_id),
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state
                );
                return;
            }
            if (!GBE_local_lobby.owner_connected) {
                GBE_local_lobby.owner_connected = true;
                GBE_PublishSharedDotaLobbyState("owner_connected");
            }
        } else if (gc_initialized && GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && connected_steam_id != 0) {
            if (GBE_SetDotaLobbyMemberConnected(connected_steam_id, true)) {
                GBE_PublishSharedDotaLobbyState("member_connected");
                GBE_GC_DebugLog(
                    "GC_DOTA_SYNC",
                    "marked Dota lobby member connected steam_id=%llu lobby_id=%llu state=%u game_state=%u members=%zu",
                    static_cast<unsigned long long>(connected_steam_id),
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state,
                    GBE_local_lobby.members.size()
                );
            }
        }
    }

    if (gc_initialized) {
        request_user_items(steam_id, generate_steam_api_call_id(), true);
    }
}

void Steam_Game_Coordinator::on_client_disconnected(CSteamID steam_id)
{
    if (!steam_id.BIndividualAccount())
        return;

    bool suppress_user_item_unsubscribe = false;
    if (is_server && gc_profile == GC_PROFILE_DOTA2) {
        const uint64 disconnected_steam_id = steam_id.ConvertToUint64();
        const uint64 owner_steam_id = GBE_GetDotaLobbyOwnerSteamId();

        // In PostGame (state >= 3), do NOT publish shared state for disconnect
        // events.  The 7004 signout finalize path handles PostGame cleanup.
        // Re-publishing state=3 here would race with finalize and leave stale
        // shared state that causes an initialize_gc adopt loop.
        const bool postgame_suppress_publish = GBE_local_lobby.state >= 3u;

        if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && disconnected_steam_id != 0 && disconnected_steam_id == owner_steam_id) {
            GBE_local_lobby.owner_connected = false;
            if (!postgame_suppress_publish)
                GBE_PublishSharedDotaLobbyState("owner_disconnected");

            suppress_user_item_unsubscribe = true;
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "preserving owner inventory cache across dota reconnect steam_id=%llu lobby_id=%llu state=%u game_state=%u launch_phase=%s abandon_postgame=%u postgame_suppress=%u",
                static_cast<unsigned long long>(disconnected_steam_id),
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state,
                GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase),
                GBE_local_lobby.abandon_postgame_active ? 1u : 0u,
                postgame_suppress_publish ? 1u : 0u
            );
        } else if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && disconnected_steam_id != 0) {
            if (GBE_SetDotaLobbyMemberConnected(disconnected_steam_id, false)) {
                if (!postgame_suppress_publish)
                    GBE_PublishSharedDotaLobbyState("member_disconnected");
                GBE_GC_DebugLog(
                    "GC_DOTA_SYNC",
                    "marked Dota lobby member disconnected steam_id=%llu lobby_id=%llu state=%u game_state=%u members=%zu postgame_suppress=%u",
                    static_cast<unsigned long long>(disconnected_steam_id),
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state,
                    GBE_local_lobby.members.size(),
                    postgame_suppress_publish ? 1u : 0u
                );
            }
        }
    }

    if (!suppress_user_item_unsubscribe)
        remove_user_items(steam_id);
}
