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

bool Steam_Game_Coordinator::GBE_HandleDotaInviteToLobbyRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    GBE_DotaInviteToLobbyRequest request{};
    if (!gbe::proto_wire::parse_dota_invite_to_lobby_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 4512 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str());
        return true;
    }

    uint64 dota_lobby_id = GBE_local_lobby.lobby_id;
    CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
    const auto shared_lobby = GBE_SharedLobbyStore().snapshot();
    if ((!GBE_local_lobby.active || dota_lobby_id == 0 || !generic_lobby_id.IsLobby()) && shared_lobby.valid) {
        dota_lobby_id = shared_lobby.lobby_id;
        generic_lobby_id = CSteamID((uint64)shared_lobby.generic_lobby_id);
    }

    Steam_Client *steam_client = get_steam_client();
    bool sent_invite = false;
    bool sent_lobby_snapshot = false;
    if (steam_client && steam_client->steam_matchmaking && generic_lobby_id.IsLobby() && request.steam_id != 0) {
        sent_lobby_snapshot = steam_client->steam_matchmaking->SendLobbySnapshotToUserForDotaInvite(generic_lobby_id, CSteamID((uint64)request.steam_id));
        sent_invite = steam_client->steam_matchmaking->InviteUserToLobby(generic_lobby_id, CSteamID((uint64)request.steam_id));
    }

    bool sent_dota_invite = false;
    if (sent_invite && sent_lobby_snapshot && dota_lobby_id != 0)
        sent_dota_invite = true;

    std::string response_4502;
    if (!gbe::gc_message::build_dota_invitation_created_payload(dota_lobby_id, request.steam_id, false, response_4502)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 4502 invite response dota_lobby_id=%llu", static_cast<unsigned long long>(dota_lobby_id));
        return true;
    }

    if (!GBE_PushDotaResponse(GBE_kGCInvitationCreated, response_4502, wrapped, outer_session_field_raw, "4512_invitation_created"))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Processed 4512 invite dota_lobby_id=%llu generic_lobby_id=%llu invitee=%llu client_version=%u sent_invite=%u sent_lobby_snapshot=%u sent_dota_invite=%u wrapped=%d",
        static_cast<unsigned long long>(dota_lobby_id),
        static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64()),
        static_cast<unsigned long long>(request.steam_id),
        request.has_client_version ? request.client_version : 0u,
        sent_invite ? 1u : 0u,
        sent_lobby_snapshot ? 1u : 0u,
        sent_dota_invite ? 1u : 0u,
        wrapped ? 1 : 0);
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaLobbyInviteResponseRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    GBE_DotaLobbyInviteResponseRequest request{};
    if (!gbe::proto_wire::parse_dota_lobby_invite_response_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 4513 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str());
        return true;
    }

    CSteamID matched_generic_lobby_id = k_steamIDNil;
    GBE_LocalLobby matched_lobby{};
    bool matched_generic_lobby = request.has_lobby_id && request.lobby_id != 0 &&
        GBE_FindDotaGenericLobbyByDotaLobbyId(request.lobby_id, matched_generic_lobby_id, &matched_lobby, "4513_invite_accept");

    Steam_Client *steam_client = get_steam_client();
    if (!matched_generic_lobby && steam_client && steam_client->steam_matchmaking && request.has_lobby_id && request.lobby_id != 0) {
        matched_generic_lobby_id = steam_client->steam_matchmaking->FindLobbyByDotaLobbyIdForInvite(
            request.lobby_id,
            GBE_kDotaGenericLobbyMarkerKey,
            GBE_kDotaGenericLobbyMarkerValue,
            GBE_kDotaGenericLobbyDotaLobbyIdKey);
        if (matched_generic_lobby_id.IsLobby()) {
            steam_client->steam_matchmaking->JoinLobby(matched_generic_lobby_id);
            steam_client->steam_matchmaking->RefreshLobbyCallbacksForDota();
            matched_generic_lobby = GBE_FindDotaGenericLobbyByDotaLobbyId(request.lobby_id, matched_generic_lobby_id, &matched_lobby, "4513_invite_accept_local_find");
        }
    }

    if (request.has_accept && !request.accept) {
        std::string response_remove_2011;
        if (request.has_lobby_id && request.lobby_id != 0 && gbe::gc_message::build_dota_remove_lobby_invite_payload(request.lobby_id, settings->get_local_steam_id().ConvertToUint64(), response_remove_2011)) {
            if (!wrapped || outer_session_field_raw)
                GBE_PushDotaResponse(GBE_kDotaPracticeLobbyDetailsUpdate, response_remove_2011, wrapped, outer_session_field_raw, "4513_decline_remove_2011");
        }

        std::string response_25;
        if (gbe::gc_message::build_dota_so_owner_cache_unsubscribed_payload(4u, settings->get_local_steam_id().ConvertToUint64(), response_25)) {
            if (!wrapped || outer_session_field_raw)
                GBE_PushDotaResponse(GBE_kDotaCacheUnsubscribed, response_25, wrapped, outer_session_field_raw, "4513_decline_25");
        }

        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Processed declined 4513 invite response lobby_id=%llu matched_generic=%u generic_lobby_id=%llu client_version=%u wrapped=%d",
            static_cast<unsigned long long>(request.lobby_id),
            matched_generic_lobby ? 1u : 0u,
            static_cast<unsigned long long>(matched_generic_lobby_id.ConvertToUint64()),
            request.has_client_version ? request.client_version : 0u,
            wrapped ? 1 : 0);
        return true;
    }

    std::string join_body;
    if (request.has_lobby_id)
        gbe::proto_wire::append_varint_field(join_body, 1u, request.lobby_id);

    if (!GBE_HandleDotaPracticeLobbyJoinRequest(join_body, 0u, false, wrapped, outer_session_field_raw, false))
        return false;

    std::string response_remove_2011;
    if (request.has_lobby_id && request.lobby_id != 0 && gbe::gc_message::build_dota_remove_lobby_invite_payload(request.lobby_id, settings->get_local_steam_id().ConvertToUint64(), response_remove_2011)) {
        if (!wrapped || outer_session_field_raw)
            GBE_PushDotaResponse(GBE_kDotaPracticeLobbyDetailsUpdate, response_remove_2011, wrapped, outer_session_field_raw, "4513_accept_remove_2011");
    }

    std::string response_25;
    if (gbe::gc_message::build_dota_so_owner_cache_unsubscribed_payload(4u, settings->get_local_steam_id().ConvertToUint64(), response_25)) {
        if (!wrapped || outer_session_field_raw)
            GBE_PushDotaResponse(GBE_kDotaCacheUnsubscribed, response_25, wrapped, outer_session_field_raw, "4513_accept_25");
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Processed 4513 invite response lobby_id=%llu accept=%u matched_generic=%u generic_lobby_id=%llu client_version=%u wrapped=%d",
        static_cast<unsigned long long>(request.lobby_id),
        (!request.has_accept || request.accept) ? 1u : 0u,
        matched_generic_lobby ? 1u : 0u,
        static_cast<unsigned long long>(matched_generic_lobby_id.ConvertToUint64()),
        request.has_client_version ? request.client_version : 0u,
        wrapped ? 1 : 0);
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaFriendLobbyInviteMessage(Common_Message *msg)
{
    if (!msg || !msg->has_friend_messages() || !gc_initialized || gc_profile != GC_PROFILE_DOTA2)
        return false;

    if (msg->friend_messages().type() != Friend_Messages::LOBBY_INVITE)
        return false;

    CSteamID generic_lobby_id((uint64)msg->friend_messages().lobby_id());
    if (!generic_lobby_id.IsLobby())
        return false;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking)
        return false;

    steam_client->steam_matchmaking->RefreshLobbyCallbacksForDota();

    const char *marker = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyMarkerKey);
    if (!marker || std::strcmp(marker, GBE_kDotaGenericLobbyMarkerValue) != 0)
        return false;

    const uint64 dota_lobby_id = gbe::proto_wire::parse_uint64_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyDotaLobbyIdKey));
    if (dota_lobby_id == 0) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Ignoring friend lobby invite without Dota lobby id generic_lobby_id=%llu source=%llu",
            static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64()),
            static_cast<unsigned long long>(msg->source_id())
        );
        return true;
    }

    uint64 owner_steam_id = gbe::proto_wire::parse_uint64_or_zero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerSteamIdKey));
    if (owner_steam_id == 0)
        owner_steam_id = msg->source_id();
    const uint64 inviter_steam_id = msg->source_id() != 0 ? msg->source_id() : owner_steam_id;

    const char *owner_name_value = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerNameKey);
    const std::string owner_name = owner_name_value && owner_name_value[0] != '\0'
        ? std::string(owner_name_value)
        : std::string("Lobby Host");

    auto get_invite_member_name = [&](uint64 steam_id) -> std::string {
        CSteamID member_id((uint64)steam_id);
        const char *member_name = steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberNameKey);
        if (member_name && member_name[0] != '\0')
            return std::string(member_name);
        if (steam_id == owner_steam_id || steam_id == inviter_steam_id)
            return owner_name;
        return std::string("Lobby Host");
    };

    std::vector<std::pair<uint64, std::string>> invite_members;
    const std::string inviter_name = get_invite_member_name(inviter_steam_id);
    if (inviter_steam_id != 0)
        invite_members.emplace_back(inviter_steam_id, inviter_name);
    for (const auto &member_id : steam_client->steam_matchmaking->GetLobbyMemberListSnapshot(generic_lobby_id)) {
        const uint64 member_steam_id = member_id.ConvertToUint64();
        if (member_steam_id == 0 || member_steam_id == inviter_steam_id)
            continue;
        invite_members.emplace_back(member_steam_id, get_invite_member_name(member_steam_id));
    }

    const char *room_name_value = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyRoomNameKey);
    const std::string room_name = room_name_value && room_name_value[0] != '\0'
        ? std::string(room_name_value)
        : owner_name;

    std::string invite_24;
    if (!GBE_AdaptDotaLobbyInviteCacheSubscribedPayload(
            dota_lobby_id,
            inviter_steam_id,
            settings->get_local_steam_id().ConvertToUint64(),
            inviter_name,
            invite_members,
            invite_24)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed building 2011 from friend lobby invite dota_lobby_id=%llu generic_lobby_id=%llu source=%llu",
            static_cast<unsigned long long>(dota_lobby_id),
            static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64()),
            static_cast<unsigned long long>(msg->source_id())
        );
        return true;
    }

    push_incoming_now(GBE_kDotaCacheSubscribed | GBE_kProtoMask, invite_24);
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Created 2011 lobby invite from friend invite dota_lobby_id=%llu generic_lobby_id=%llu source=%llu owner=%llu members=%zu room='%s'",
        static_cast<unsigned long long>(dota_lobby_id),
        static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64()),
        static_cast<unsigned long long>(msg->source_id()),
        static_cast<unsigned long long>(owner_steam_id),
        invite_members.size(),
        room_name.c_str()
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaNetworkLobbyInviteMessage(Common_Message *msg)
{
    if (!msg || !msg->has_steam_messages() || !gc_initialized || gc_profile != GC_PROFILE_DOTA2)
        return false;

    if (msg->steam_messages().type() != Steam_Messages::FRIEND_CHAT)
        return false;

    const std::string &message = msg->steam_messages().message();
    if (!GBE_IsDotaLobbyInviteCacheSubscribedPayload(message))
        return false;

    push_incoming_now(GBE_kDotaCacheSubscribed | GBE_kProtoMask, message);
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Received network 2011 lobby invite source=%llu size=%zu",
        static_cast<unsigned long long>(msg->source_id()),
        message.size()
    );
    return true;
}
