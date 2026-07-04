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

// Custom-game loading flow handlers for the Dota Game Coordinator.
// Extracted from gbe_dota_handlers.cpp (Phase 3.1.5) to group the 7070/8052/8053
// custom game ready-up / started-loading / finished-loading requests into one
// domain file.
//
// Responsibility boundary: owns the local lobby launch-phase advancement and
// state publication triggered by the custom game loading lifecycle. Side-effect
// ownership and ordering are unchanged from the prior monolithic handler file;
// only the file location moved. No handler-local statics moved with this group
// (List X = 0) and no cross-TU symbols needed externalization (List Y = 0).

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

using GBE_Dota8053Result = gbe::proto_wire::Dota8053Result;


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
        uint32 ready_state = 0u;
        gbe::proto_wire::read_uint32_field(body, body_size, 1u, ready_state);
        std::string response_7170;
        if (gbe::gc_message::build_dota_ready_up_status_payload(has_source_job, source_job, GBE_local_lobby.lobby_id, 0u, ready_state != 0u ? ready_state : 1u, response_7170))
            push_incoming_now(7170u | GBE_kProtoMask, response_7170);

        if (ready_state == 1u && GBE_local_lobby.state == 2u && GBE_local_lobby.game_state < 1u && GBE_local_lobby.launch_phase >= GBE_kDotaLaunchPhaseRunQueued) {
            GBE_local_lobby.game_state = 1u;
            GBE_PublishSharedDotaLobbyState("7070_custom_game_ready_up_run_ack");
            GBE_SendDotaPracticeLobbyDetailsUpdate(false, nullptr, "7070_custom_game_ready_up_run_ack");
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
        uint64 lobby_id = 0;
        uint64 custom_game_id = 0;
        uint64 start_time = 0;
        gbe::proto_wire::read_uint64_field(body, body_size, 1u, lobby_id);
        gbe::proto_wire::read_uint64_field(body, body_size, 2u, custom_game_id);
        gbe::proto_wire::read_uint64_field(body, body_size, 4u, start_time);

        if (lobby_id == 0 || lobby_id == GBE_local_lobby.lobby_id) {
            if (custom_game_id != 0)
                GBE_local_lobby.custom_game.game_id = custom_game_id;
            if (start_time != 0)
                GBE_local_lobby.game_start_time = static_cast<uint32>(start_time);
            if (GBE_TryAdvanceDotaLaunchToRun("custom game 8052 started loading", 8052u, source_job, "8052_started_loading", 0u)) {
                GBE_GC_DebugLog(
                    "GC_DOTA_LOBBY",
                    "[LOBBY] Advanced custom game RUN after 8052 lobby_id=%llu custom_game_id=%llu start_time=%llu state=%u game_state=%u",
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    static_cast<unsigned long long>(custom_game_id),
                    static_cast<unsigned long long>(start_time),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state
                );
            } else {
                GBE_PublishSharedDotaLobbyState("8052_started_loading");
                GBE_SendDotaPracticeLobbyDetailsUpdate(false, nullptr, "8052_started_loading");
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
        const GBE_Dota8053Result load_result = gbe::proto_wire::parse_dota8053_result(body, body_size);

        if (load_result.lobby_id == 0 || load_result.lobby_id == GBE_local_lobby.lobby_id) {
            if (GBE_local_lobby.launch_phase >= GBE_kDotaLaunchPhaseRunQueued) {
                GBE_local_lobby.state = 2u;
                if (GBE_local_lobby.game_state < 1u)
                    GBE_local_lobby.game_state = 1u;
            } else if (GBE_local_lobby.state < 2u) {
                GBE_local_lobby.state = 2u;
            }

            const bool load_failed = gbe::proto_wire::dota8053_indicates_load_failure(load_result.result_code, load_result.result_text);
            const char *reason = load_failed ? "8053_load_failed" : "8053_finished_loading";
            if (!load_failed) {
                const uint64 local_steam_id = settings ? settings->get_local_steam_id().ConvertToUint64() : 0ull;
                if (local_steam_id != 0ull)
                    GBE_SetDotaLobbyMemberRuntimeState(local_steam_id, true, 0u, false);
                GBE_MarkDotaLaunchPhase(GBE_kDotaLaunchPhaseLoaded, reason);
                GBE_PublishDotaPracticeLobbyLocalMemberData(reason);
            }
            GBE_PublishSharedDotaLobbyState(reason);
            GBE_SendDotaPracticeLobbyDetailsUpdate(false, nullptr, reason);
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Applied direct 8053 lobby_id=%llu loading_duration=%llu result_code=%llu signon_states=%llu load_failed=%u result_text=%s",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(load_result.loading_duration),
                static_cast<unsigned long long>(load_result.result_code),
                static_cast<unsigned long long>(load_result.signon_states),
                load_failed ? 1u : 0u,
                load_result.result_text.c_str()
            );
        }
    }
    return true;
}
