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

bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbySetTeamSlotRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7047 because no local lobby is active");
        return true;
    }

    GBE_DotaPracticeLobbySetTeamSlotRequest request{};
    if (!gbe::proto_wire::parse_dota_practice_lobby_set_team_slot_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7047 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    const uint64 local_steam_id = settings->get_local_steam_id().ConvertToUint64();
    const bool local_is_owner = local_steam_id != 0ull && local_steam_id == GBE_local_lobby.owner_steam_id;
    if (local_is_owner) {
        if (request.has_team)
            GBE_local_lobby.owner_team = request.team;
        if (request.has_slot)
            GBE_local_lobby.owner_slot = request.slot;
    }
    gbe::dota_lobby_flow::apply_lobby_member_team_slot_update(
        GBE_local_lobby.members,
        local_steam_id,
        settings->get_local_steam_id().GetAccountID(),
        request.has_team,
        request.team,
        request.has_slot,
        request.slot,
        GBE_kDotaTeamPlayerPool,
        GBE_local_lobby.state == 3u);
    if (request.has_bot_difficulty) {
        const uint32 bot_team = request.has_team ? request.team : GBE_local_lobby.owner_team;
        if (gbe::proto_wire::dota_is_dire_team(bot_team))
            GBE_local_lobby.bot_difficulty_dire = request.bot_difficulty;
        else
            GBE_local_lobby.bot_difficulty_radiant = request.bot_difficulty;
    }
    GBE_NormalizeDotaArcadeLobbyMemberSlots(GBE_local_lobby);
    GBE_PublishDotaPracticeLobbyLocalMemberData("7047_set_team_slot");
    GBE_PublishSharedDotaLobbyState("7047_set_team_slot");

    if (!GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7047"))
        return true;

    if (has_request_job) {
        std::string response_7055;
        if (!gbe::gc_message::build_dota_practice_lobby_response_payload(request_job_id, true, response_7055)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7055 payload for 7047 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
            return true;
        }

        std::string wrapped_7055;
        if (!GBE_PushDotaPracticeLobbyResponse(response_7055, wrapped, outer_session_field_raw, "7047_7055", &wrapped_7055))
            return true;

        if (wrapped) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Sent wrapped 7055 ack for 7047 LobbyID=%llu request_job=%llu size=%zu body_prefix=%s packet_prefix=%s",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(request_job_id),
                wrapped_7055.size(),
                gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(response_7055.data()), response_7055.size(), 32).c_str(),
                gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(wrapped_7055.data()), wrapped_7055.size(), 32).c_str()
            );
        } else {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Sent direct 7055 ack for 7047 LobbyID=%llu request_job=%llu size=%zu body_prefix=%s",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(request_job_id),
                response_7055.size(),
                gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(response_7055.data()), response_7055.size(), 32).c_str()
            );
        }
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Team slot updated. local_team=%u local_slot=%u owner_team=%u owner_slot=%u bot_diff_req=%u has_bot_diff=%d",
        request.has_team ? request.team : 0u,
        request.has_slot ? request.slot : 0u,
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot,
        request.bot_difficulty,
        request.has_bot_difficulty ? 1 : 0
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyKickRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.generic_lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7081 because no local lobby is active");
        return true;
    }

    const uint64 local_steam_id = settings->get_local_steam_id().ConvertToUint64();
    if (local_steam_id != GBE_local_lobby.owner_steam_id) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7081 because local user is not Dota lobby owner LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    GBE_DotaPracticeLobbyKickRequest request{};
    if (!gbe::proto_wire::parse_dota_practice_lobby_kick_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7081 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    if (request.account_id == 0u || request.account_id == settings->get_local_steam_id().GetAccountID()) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7081 invalid target account_id=%u LobbyID=%llu", request.account_id, static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    const uint64 kicked_steam_id = gbe::dota_lobby_flow::find_lobby_member_steam_id_by_account_id(GBE_local_lobby.members, request.account_id);

    if (kicked_steam_id == 0ull) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7081 because target account_id=%u is not in Dota lobby LobbyID=%llu", request.account_id, static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7081 because matchmaking is unavailable LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    GBE_LocalLobby before_lobby = GBE_local_lobby;
    gbe::dota_lobby_flow::clear_lobby_member_by_account_id(before_lobby.members, request.account_id);

    CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
    const bool kicked = steam_client->steam_matchmaking->KickLobbyMemberForDota(generic_lobby_id, CSteamID((uint64)kicked_steam_id));
    if (!kicked) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed kicking generic lobby member for 7081 LobbyID=%llu generic_lobby_id=%llu target_account=%u target_steam=%llu",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
            request.account_id,
            static_cast<unsigned long long>(kicked_steam_id)
        );
        return true;
    }

    GBE_local_lobby = before_lobby;
    GBE_PublishSharedDotaLobbyState("7081_kick_member");

    if (!GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7081"))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Kicked practice lobby member target_account=%u target_steam=%llu LobbyID=%llu generic_lobby_id=%llu wrapped=%d",
        request.account_id,
        static_cast<unsigned long long>(kicked_steam_id),
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        wrapped ? 1 : 0
    );
    return true;
}


