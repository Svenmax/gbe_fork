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
#include "gbe_dota_gc_internal.h"
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

bool Steam_Game_Coordinator::GBE_HandleDotaLobbyListRequest(bool has_request_job, uint64 request_job_id, bool wrapped, const std::string *outer_session_field_raw)
{
    const bool finishing_leave = GBE_local_lobby.pending_leave_after_7040 && GBE_local_lobby.pending_leave_lobby_id != 0;
    const uint64 leaving_lobby_id = GBE_local_lobby.pending_leave_lobby_id;

    std::string response_25;
    if (finishing_leave && !gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(leaving_lobby_id, response_25)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building delayed 25 for 7040 LobbyID=%llu", static_cast<unsigned long long>(leaving_lobby_id));
        return true;
    }

    std::vector<GBE_LocalLobby> lobby_snapshots;
    if (!finishing_leave)
        lobby_snapshots = GBE_GetDotaGenericLobbySnapshots("8011_lobby_list");
    if (!finishing_leave && GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0) {
        const uint64 local_lobby_id = GBE_local_lobby.lobby_id;
        const bool already_included = std::any_of(lobby_snapshots.begin(), lobby_snapshots.end(), [local_lobby_id](const GBE_LocalLobby &snapshot) {
            return snapshot.lobby_id == local_lobby_id;
        });
        if (!already_included)
            lobby_snapshots.push_back(GBE_local_lobby);
    }

    std::vector<std::string> entries;
    for (const GBE_LocalLobby &snapshot : lobby_snapshots) {
        if (GBE_ShouldSuppressDotaAbandonedLobby(snapshot.lobby_id))
            continue;
        entries.push_back(gbe::gc_message::build_dota_practice_lobby_list_entry_body(
            snapshot.lobby_id,
            snapshot.owner_account_id != 0u ? snapshot.owner_account_id : settings->get_local_steam_id().GetAccountID(),
            snapshot.owner_name.empty() ? std::string(settings->get_local_name()) : snapshot.owner_name,
            snapshot.room_name,
            snapshot.game_mode,
            snapshot.server_region,
            !snapshot.pass_key.empty(),
            1u,
            10u,
            snapshot.lan_host_ping_location));
    }

    std::string response_8012;
    if (!gbe::gc_message::build_dota_lobby_list_response_payload(entries, response_8012)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 8012 lobby list response active=%u finishing_leave=%u", GBE_local_lobby.active ? 1u : 0u, finishing_leave ? 1u : 0u);
        return true;
    }

    if (wrapped && !outer_session_field_raw) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 8012 lobby list");
        return true;
    }

    if (finishing_leave) {
        gbe::dota_lifecycle_state_machine::MachineState machine_state{};
        machine_state.generation = GBE_CurrentDotaLobbyGeneration();
        const auto teardown = gbe::dota_lifecycle_state_machine::transition_teardown(
            machine_state,
            { { gbe::dota_lifecycle_state_machine::EventKind::Leave,
                gbe::dota_lifecycle_state_machine::transport_source(wrapped),
                GBE_kDotaPracticeLobbyLeave,
                machine_state.generation },
              gbe::dota_lifecycle_state_machine::TeardownStage::Finalize,
              true,
              true });
        if (!teardown.accepted() || !teardown.effects.contains(
                gbe::dota_lifecycle_state_machine::EffectKind::TeardownActionsRequested))
            return true;
        GBE_PushDotaCacheUnsubscribedResponse(response_25, wrapped, outer_session_field_raw, "7040_leave_after_lobby_list_25");
        ResetGCMemory("7040_leave_after_lobby_list", true, false, gbe::dota_lobby_generation::Boundary::Leave);
    }

    if (!GBE_PushDotaResponse(GBE_kDotaLobbyListResponse, response_8012, wrapped, outer_session_field_raw, "8012_lobby_list"))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Sent lobby list response 8012 entries=%zu finishing_leave=%u request_job=%llu has_job=%u wrapped=%d",
        entries.size(),
        finishing_leave ? 1u : 0u,
        static_cast<unsigned long long>(request_job_id),
        has_request_job ? 1u : 0u,
        wrapped ? 1 : 0);
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaCustomLobbyListRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    const uint8 *request_data = reinterpret_cast<const uint8 *>(request_body.data());
    std::uint64_t request_list_job_id = 0ull;
    std::string requested_pass_key;
    gbe::proto_wire::read_uint64_field(request_data, request_body.size(), 10u, request_list_job_id);
    gbe::proto_wire::read_bytes_field(request_data, request_body.size(), 2u, requested_pass_key);

    std::vector<GBE_LocalLobby> lobby_snapshots = GBE_GetDotaGenericLobbySnapshots("7042_custom_lobby_list");
    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0ull) {
        const uint64 local_lobby_id = GBE_local_lobby.lobby_id;
        const bool already_included = std::any_of(lobby_snapshots.begin(), lobby_snapshots.end(), [local_lobby_id](const GBE_LocalLobby &snapshot) {
            return snapshot.lobby_id == local_lobby_id;
        });
        if (!already_included)
            lobby_snapshots.push_back(GBE_local_lobby);
    }

    std::vector<uint64> seen_lobby_ids;
    std::vector<std::string> entries;
    for (const GBE_LocalLobby &snapshot : lobby_snapshots) {
        if (!snapshot.active || snapshot.lobby_id == 0ull || snapshot.custom_game.game_id == 0ull)
            continue;
        if (GBE_ShouldSuppressDotaAbandonedLobby(snapshot.lobby_id))
            continue;
        if (!requested_pass_key.empty() && snapshot.pass_key != requested_pass_key)
            continue;
        if (std::find(seen_lobby_ids.begin(), seen_lobby_ids.end(), snapshot.lobby_id) != seen_lobby_ids.end())
            continue;

        entries.push_back(gbe::gc_message::build_dota_custom_lobby_list_entry_body(
            snapshot.lobby_id,
            snapshot.owner_account_id != 0u ? snapshot.owner_account_id : settings->get_local_steam_id().GetAccountID(),
            snapshot.owner_name.empty() ? std::string(settings->get_local_name()) : snapshot.owner_name,
            !snapshot.pass_key.empty(),
            snapshot.lan_host_ping_location));
        seen_lobby_ids.push_back(snapshot.lobby_id);
    }

    std::string response_7043;
    if (!gbe::gc_message::build_dota_custom_lobby_list_response_payload(request_list_job_id, entries, response_7043)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7043 custom lobby list response entries=%zu", entries.size());
        return true;
    }

    if (!GBE_PushDotaResponse(GBE_kDotaCustomLobbyListResponse, response_7043, wrapped, outer_session_field_raw, "7043_custom_lobby_list"))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Sent custom lobby list response 7043 entries=%zu request_list_job=%llu pass_key_filter=%u wrapped=%d",
        entries.size(),
        static_cast<unsigned long long>(request_list_job_id),
        requested_pass_key.empty() ? 0u : 1u,
        wrapped ? 1 : 0);
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaFriendPracticeLobbyListRequest(bool wrapped, const std::string *outer_session_field_raw)
{
    std::vector<std::string> entries;
    const std::vector<GBE_LocalLobby> lobby_snapshots = GBE_GetDotaGenericLobbySnapshots("7111_friend_lobby_list");
    for (const GBE_LocalLobby &snapshot : lobby_snapshots) {
        if (GBE_ShouldSuppressDotaAbandonedLobby(snapshot.lobby_id))
            continue;
        entries.push_back(gbe::gc_message::build_dota_practice_lobby_list_entry_body(
            snapshot.lobby_id,
            snapshot.owner_account_id != 0u ? snapshot.owner_account_id : settings->get_local_steam_id().GetAccountID(),
            snapshot.owner_name.empty() ? std::string(settings->get_local_name()) : snapshot.owner_name,
            snapshot.room_name,
            snapshot.game_mode,
            snapshot.server_region,
            !snapshot.pass_key.empty(),
            1u,
            10u,
            snapshot.lan_host_ping_location));
    }

    std::string response_7112;
    if (!gbe::gc_message::build_dota_friend_practice_lobby_list_response_payload(entries, response_7112)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7112 friend practice lobby list response");
        return true;
    }

    if (!GBE_PushDotaResponse(GBE_kDotaFriendPracticeLobbyListResponse, response_7112, wrapped, outer_session_field_raw, "7112_friend_lobby_list"))
        return true;

    GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Sent friend practice lobby list response 7112 entries=%zu wrapped=%d", entries.size(), wrapped ? 1 : 0);
    return true;
}
