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
#include "dll/callsystem.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_request_router.h"
#include "gbe_proto_buf_header.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_custom_lobby_http.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_gc_wire.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_dota_lobby_state_store.h"
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

namespace {

struct GBE_DotaLobbyCallbackGenerationGuardContext {
    const Steam_Game_Coordinator *coordinator{};
    std::uint64_t generation{};
};

} // namespace

// --- List X: lobby-flow-only static constants (moved from steam_game_coordinator.cpp) ---

static constexpr const char *GBE_kDotaLaunchPersonaStateInitServerSetupHex =
    "fe0200800f00000009911ddf050100100110c9dbfdd20408dfe60112c30309911ddf0501001001100118ba04300138017a0a636c6f7665726c6f7665c9010000000000000000fa01140000000000000000000000000000000000000000e802a6c7dacf06f00281c8dacf06f802a6c7dacf06ba0300c1033a02000000000000e20300ba04170a06737461747573120d23444f54415f52505f494e4954ba041e0a0d737465616d5f646973706c6179120d23444f54415f52505f494e4954ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131ba049a010a056c6f6262791290016c6f6262795f69643a203239383232343938363432383535303930206c6f6262795f73746174653a2053455256455253455455502067616d655f6d6f64653a20444f54415f47414d454d4f44455f4150206d656d6265725f636f756e743a2031206d61785f6d656d6265725f636f756e743a203130206e616d653a20226565656522206c6f6262795f747970653a2031c1040000000000000000c9040000000000000000f80400800500880500980501";


static constexpr const char *GBE_kDotaLaunchPersonaStateFindingMatchServerSetupHex =
    "fe0200800f00000009911ddf050100100110c9dbfdd204080812f20309911ddf0501001001100118ba04300138017a0a636c6f7665726c6f7665c9010e14ce4bfcc14001fa01140000000000000000000000000000000000000000e802a6c7dacf06f00281c8dacf06f80200ba0300c1033a02000000000000e20300ba04200a06737461747573121623444f54415f52505f46494e44494e475f4d41544348ba04270a0d737465616d5f646973706c6179121623444f54415f52505f46494e44494e475f4d41544348ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131ba041e0a057061727479121570617274795f73746174653a20494e5f4d41544348ba049a010a056c6f6262791290016c6f6262795f69643a203239383232343938363432383535303930206c6f6262795f73746174653a2053455256455253455455502067616d655f6d6f64653a20444f54415f47414d454d4f44455f4150206d656d6265725f636f756e743a2031206d61785f6d656d6265725f636f756e743a203130206e616d653a20226565656522206c6f6262795f747970653a2031c1040000000000000000c9040000000000000000f80400800500880500980501";


static constexpr const char *GBE_kDotaLaunchPersonaStateFindingMatchRunHex =
    "fe0200800f00000009911ddf050100100110c9dbfdd20408dfe60112ee0309911ddf0501001001100118ba04300138017a0a636c6f7665726c6f7665c9010000000000000000fa01140000000000000000000000000000000000000000e802a6c7dacf06f00281c8dacf06f802a6c7dacf06ba0300c1033a02000000000000e20300ba04200a06737461747573121623444f54415f52505f46494e44494e475f4d41544348ba04270a0d737465616d5f646973706c6179121623444f54415f52505f46494e44494e475f4d41544348ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131ba041e0a057061727479121570617274795f73746174653a20494e5f4d41544348ba0492010a056c6f6262791288016c6f6262795f69643a203239383232343938363432383535303930206c6f6262795f73746174653a2052554e2067616d655f6d6f64653a20444f54415f47414d454d4f44455f4150206d656d6265725f636f756e743a2031206d61785f6d656d6265725f636f756e743a203130206e616d653a20226565656522206c6f6262795f747970653a2031c1040000000000000000c9040000000000000000f80400800500880500980501";


static constexpr const char *GBE_kDotaLaunchPersonaStatePrivateLobbyRunHex =
    "fe0200800f00000009911ddf050100100110c9dbfdd20408dfe60112ee0309911ddf0501001001100118ba04300138017a0a636c6f7665726c6f7665c9010000000000000000fa01140000000000000000000000000000000000000000e802a6c7dacf06f00281c8dacf06f802a6c7dacf06ba0300c1033a02000000000000e20300ba04200a06737461747573121623444f54415f52505f505249564154455f4c4f424259ba04270a0d737465616d5f646973706c6179121623444f54415f52505f505249564154455f4c4f424259ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131ba041e0a057061727479121570617274795f73746174653a20494e5f4d41544348ba0492010a056c6f6262791288016c6f6262795f69643a203239383232343938363432383535303930206c6f6262795f73746174653a2052554e2067616d655f6d6f64653a20444f54415f47414d454d4f44455f4150206d656d6265725f636f756e743a2031206d61785f6d656d6265725f636f756e743a203130206e616d653a20226565656522206c6f6262795f747970653a2031c1040000000000000000c9040000000000000000f80400800500880500980501";


// --- Lobby-flow helper member functions (moved from steam_game_coordinator.cpp) ---

void Steam_Game_Coordinator::GBE_ResetDotaPracticeLobbyLaunchPeripheralState()
{
    GBE_ClearLastDotaDirectConnectCallbackKey();
}


bool Steam_Game_Coordinator::GBE_IsCurrentDotaLobbyCallbackGeneration(
    const void *context,
    unsigned int context_size)
{
    if (!context || context_size != sizeof(GBE_DotaLobbyCallbackGenerationGuardContext))
        return false;

    GBE_DotaLobbyCallbackGenerationGuardContext guard_context{};
    std::memcpy(&guard_context, context, sizeof(guard_context));
    return guard_context.coordinator
        && guard_context.coordinator->GBE_CurrentDotaLobbyGeneration() == guard_context.generation;
}


bool Steam_Game_Coordinator::GBE_ShouldTrackDotaPracticeLobbyLateSteamChain() const
{
    return
        GBE_local_lobby.active &&
        GBE_local_lobby.lobby_id != 0;
}


bool Steam_Game_Coordinator::GBE_MaybeQueueDotaPracticeLobbySteamAuthAck(const char *reason, uint64 request_job_id)
{
    if (gc_profile != GC_PROFILE_DOTA2 || settings->get_local_game_id().AppID() != GBE_kDotaAppId)
        return false;

    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.match_id == 0)
        return false;

    if (!gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game))
        return false;

    if (GBE_local_lobby.state != 1u || GBE_local_lobby.game_state != 0u)
        return false;

    if (GBE_local_lobby.launch_steam_auth_ack_queued)
        return false;

    const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    const uint64 owner_steam_id = GBE_GetDotaLobbyOwnerSteamId() != 0ull ? GBE_GetDotaLobbyOwnerSteamId() : steam_id;
    const uint32 message_sequence = GBE_local_lobby.launch_steam_auth_message_sequence != 0u
        ? GBE_local_lobby.launch_steam_auth_message_sequence
        : 1u;
    uint32 ticket_crc = GBE_local_lobby.launch_steam_auth_ticket_crc;
    if (ticket_crc == 0u) {
        uint64 seed = steam_id ^ (GBE_local_lobby.match_id << 1) ^ (GBE_local_lobby.server_id << 7) ^ 0x4409f3a5u;
        ticket_crc = static_cast<uint32>(seed) ^ static_cast<uint32>(seed >> 32);
        if (ticket_crc == 0u)
            ticket_crc = 1u;
        GBE_local_lobby.launch_steam_auth_ticket_crc = ticket_crc;
    }
    GBE_local_lobby.launch_steam_auth_message_sequence = message_sequence;

    std::string auth_complete_body;
    gbe::proto_wire::append_fixed64_field(auth_complete_body, 1u, steam_id);
    gbe::proto_wire::append_fixed64_field(auth_complete_body, 2u, GBE_kDotaAppId);
    gbe::proto_wire::append_varint_field(auth_complete_body, 3u, 3u);
    gbe::proto_wire::append_varint_field(auth_complete_body, 4u, 0u);
    gbe::proto_wire::append_varint_field(auth_complete_body, 6u, ticket_crc);
    gbe::proto_wire::append_fixed64_field(auth_complete_body, 8u, owner_steam_id);

    std::string auth_complete_message = build_protomsg_header(GBE_kSteamTicketAuthComplete | GBE_kProtoMask, request_job_id, k_GIDNil);
    auth_complete_message.append(auth_complete_body);

    std::string auth_ack_body;
    gbe::proto_wire::append_varint_field(auth_ack_body, 1u, ticket_crc);
    gbe::proto_wire::append_varint_field(auth_ack_body, 2u, GBE_kDotaAppId);
    gbe::proto_wire::append_varint_field(auth_ack_body, 3u, message_sequence);

    std::string auth_ack_message = build_protomsg_header(5575u | GBE_kProtoMask, request_job_id, k_GIDNil);
    auth_ack_message.append(auth_ack_body);

    GBE_local_lobby.launch_steam_auth_ack_queued = true;
    GBE_PublishSharedDotaLobbyState(reason ? reason : "steam_auth_ack");

    push_incoming_now(GBE_kSteamTicketAuthComplete | GBE_kProtoMask, auth_complete_message);
    push_incoming_now(5575u | GBE_kProtoMask, auth_ack_message);

    GBE_GC_DebugLog(
        "GC_DOTA_AUTH",
        "queued synthetic steam auth ack reason=%s request_job=%llu ticket_crc=%u sequence=%u steam_id=%llu owner_steam_id=%llu lobby_id=%llu match_id=%llu server_id=%llu state=%u game_state=%u sizes=%zu/%zu",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(request_job_id),
        ticket_crc,
        message_sequence,
        static_cast<unsigned long long>(steam_id),
        static_cast<unsigned long long>(owner_steam_id),
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id),
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        auth_complete_message.size(),
        auth_ack_message.size()
    );

    return true;
}


void Steam_Game_Coordinator::GBE_UpdateDotaPracticeLobbyLaunchRichPresence(const char *status, const char *lobby_state, bool include_party, bool include_lobby)
{
    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_friends)
        return;

    char lobby_value[512] = {};
    if (include_lobby) {
        const char *room_name = GBE_local_lobby.room_name.empty() ? "" : GBE_local_lobby.room_name.c_str();
        const bool is_custom_game = gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game);
        const uint32 max_member_count = is_custom_game && GBE_local_lobby.custom_game.max_players != 0u
            ? GBE_local_lobby.custom_game.max_players
            : 10u;
        if (is_custom_game) {
            std::snprintf(
                lobby_value,
                sizeof(lobby_value),
                "lobby_id: %llu lobby_state: %s game_mode: DOTA_GAMEMODE_CUSTOM custom_game_id: %llu member_count: 1 max_member_count: %u name: \"%s\" lobby_type: 1",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                lobby_state ? lobby_state : "SERVERSETUP",
                static_cast<unsigned long long>(GBE_local_lobby.custom_game.game_id),
                max_member_count,
                room_name
            );
        } else {
            std::snprintf(
                lobby_value,
                sizeof(lobby_value),
                "lobby_id: %llu lobby_state: %s game_mode: DOTA_GAMEMODE_AP member_count: 1 max_member_count: %u name: \"%s\" lobby_type: 1",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                lobby_state ? lobby_state : "SERVERSETUP",
                max_member_count,
                room_name
            );
        }
    }

    steam_client->steam_friends->SetRichPresence("status", status ? status : "");
    steam_client->steam_friends->SetRichPresence("steam_display", status ? status : "");
    steam_client->steam_friends->SetRichPresence("num_params", "0");
    // Declare event levels for all known Battle Pass/event IDs
    for (int eid = 7; eid <= 60; eid++) {
        char key[32];
        snprintf(key, sizeof(key), "EventLevel_%d", eid);
        steam_client->steam_friends->SetRichPresence(key, "1");
    }
    const uint64 local_steam_id = settings ? settings->get_local_steam_id().ConvertToUint64() : 0ull;
    const uint64 owner_steam_id = GBE_local_lobby.owner_steam_id != 0 ? GBE_local_lobby.owner_steam_id : GBE_GetDotaLobbyOwnerSteamId();
    const bool arcade_custom_launch = GBE_local_lobby.custom_game.game_id != 0ull;
    const std::string direct_connect_raw_endpoint = include_party
        ? gbe::proto_wire::get_dota_practice_lobby_first_connect_endpoint(GBE_local_lobby.connect)
        : std::string();
    const std::string direct_connect_endpoint = arcade_custom_launch
        ? gbe::proto_wire::select_dota_arcade_connect_endpoint_for_local_player(direct_connect_raw_endpoint, local_steam_id, owner_steam_id)
        : direct_connect_raw_endpoint;
    if (!direct_connect_endpoint.empty()) {
        const std::string connect_command = std::string("+connect ") + direct_connect_endpoint;
        steam_client->steam_friends->SetRichPresence("connect", connect_command.c_str());
    } else {
        steam_client->steam_friends->SetRichPresence("connect", nullptr);
    }
    if (include_lobby) {
        steam_client->steam_friends->SetRichPresence("lobby", lobby_value);
    } else {
        steam_client->steam_friends->SetRichPresence("lobby", nullptr);
    }
    if (include_party) {
        steam_client->steam_friends->SetRichPresence("party", "party_state: IN_MATCH");
    } else {
        steam_client->steam_friends->SetRichPresence("party", nullptr);
    }

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "updated local launch rich presence status=%s lobby_state=%s include_party=%u include_lobby=%u lobby_id=%llu connect=%s connect_raw=%s local_is_owner=%u",
        status ? status : "",
        lobby_state ? lobby_state : "",
        include_party ? 1u : 0u,
        include_lobby ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        direct_connect_endpoint.c_str(),
        direct_connect_raw_endpoint.c_str(),
        local_steam_id != 0ull && local_steam_id == owner_steam_id ? 1u : 0u
    );
}


void Steam_Game_Coordinator::GBE_ResetDotaPracticeLobbyLaunchRichPresenceToServerSetup()
{
    GBE_UpdateDotaPracticeLobbyLaunchRichPresence("#DOTA_RP_INIT", "SERVERSETUP", false, false);
}


void Steam_Game_Coordinator::GBE_ClearDotaPracticeLobbyLaunchRichPresence()
{
    GBE_ClearLastDotaLaunchPersonaSignature();
    GBE_ClearLastDotaDirectConnectCallbackKey();

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_friends)
        return;

    steam_client->steam_friends->SetRichPresence("status", nullptr);
    steam_client->steam_friends->SetRichPresence("steam_display", nullptr);
    steam_client->steam_friends->SetRichPresence("num_params", nullptr);
    // Clear event level keys for all known event IDs
    for (int eid = 7; eid <= 60; eid++) {
        char key[32];
        snprintf(key, sizeof(key), "EventLevel_%d", eid);
        steam_client->steam_friends->SetRichPresence(key, nullptr);
    }
    steam_client->steam_friends->SetRichPresence("connect", nullptr);
    steam_client->steam_friends->SetRichPresence("lobby", nullptr);
    steam_client->steam_friends->SetRichPresence("party", nullptr);
}


void Steam_Game_Coordinator::GBE_MaybeQueueDotaPracticeLobbyDirectConnectCallback(const char *reason)
{
    if (is_server || gc_profile != GC_PROFILE_DOTA2 || !callbacks)
        return;

    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || !GBE_local_lobby.lan)
        return;

    if (GBE_local_lobby.state != 2u || GBE_local_lobby.match_id == 0)
        return;

    const std::string raw_endpoint = gbe::proto_wire::get_dota_practice_lobby_first_connect_endpoint(GBE_local_lobby.connect);
    const uint32 endpoint_ip = gbe::proto_wire::parse_dota_practice_lobby_connect_ipv4(raw_endpoint);
    if (raw_endpoint.empty() || endpoint_ip == 0u)
        return;

    const uint64 local_steam_id = settings ? settings->get_local_steam_id().ConvertToUint64() : 0ull;
    const uint64 owner_steam_id = GBE_local_lobby.owner_steam_id != 0 ? GBE_local_lobby.owner_steam_id : GBE_GetDotaLobbyOwnerSteamId();
    const bool local_is_owner = local_steam_id != 0ull && local_steam_id == owner_steam_id;
    const bool arcade_custom_launch = GBE_local_lobby.custom_game.game_id != 0ull;

    if (local_is_owner) {
        GBE_GC_DebugLog(
            "GC_DOTA_CONNECT_DIAG",
            "skipping direct connect callbacks for local owner reason=%s lobby_id=%llu match_id=%llu custom_game_id=%llu endpoint_raw=%s server_id=%llu",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.custom_game.game_id),
            raw_endpoint.c_str(),
            static_cast<unsigned long long>(GBE_local_lobby.server_id)
        );
        return;
    }

    const std::string endpoint = gbe::proto_wire::select_dota_arcade_connect_endpoint_for_local_player(raw_endpoint, local_steam_id, owner_steam_id);

    const std::uint64_t generation = GBE_CurrentDotaLobbyGeneration();
    const gbe::dota_connection::DedupKey callback_key{
        gbe::dota_lobby_generation::Generation{generation},
        GBE_local_lobby.server_id,
        endpoint};
    if (callback_key == GBE_GetLastDotaDirectConnectCallbackKey()) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipping duplicate direct connect callback reason=%s lobby_id=%llu endpoint=%s",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            endpoint.c_str()
        );
        return;
    }

    const std::string connect_command = std::string("+connect ") + endpoint;
    const GBE_DotaLobbyCallbackGenerationGuardContext guard_context{this, generation};
    const SteamCallExecutionGuard execution_guard(
        GBE_IsCurrentDotaLobbyCallbackGeneration,
        &guard_context,
        sizeof(guard_context));
    GameServerChangeRequested_t server_change{};
    std::strncpy(server_change.m_rgchServer, endpoint.c_str(), sizeof(server_change.m_rgchServer) - 1);
    callbacks->addCBResult(server_change.k_iCallback, &server_change, sizeof(server_change), 0.0, false, execution_guard);

    GBE_GC_DebugLog(
        "GC_DOTA_CONNECT_DIAG",
        "queued callback id=%d type=GameServerChangeRequested delay=0.00 reason=%s lobby_id=%llu match_id=%llu owner=%llu local=%llu local_is_owner=%u state=%u game_state=%u endpoint=%s endpoint_raw=%s server_id=%llu arcade=%u",
        server_change.k_iCallback,
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(owner_steam_id),
        static_cast<unsigned long long>(local_steam_id),
        local_is_owner ? 1u : 0u,
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        endpoint.c_str(),
        raw_endpoint.c_str(),
        static_cast<unsigned long long>(GBE_local_lobby.server_id),
        arcade_custom_launch ? 1u : 0u
    );

    if (!arcade_custom_launch) {
        GameRichPresenceJoinRequested_t rich_join{};
        rich_join.m_steamIDFriend = CSteamID(owner_steam_id);
        std::strncpy(rich_join.m_rgchConnect, connect_command.c_str(), sizeof(rich_join.m_rgchConnect) - 1);
        callbacks->addCBResult(rich_join.k_iCallback, &rich_join, sizeof(rich_join), 0.25, false, execution_guard);

        GBE_GC_DebugLog(
            "GC_DOTA_CONNECT_DIAG",
            "queued callback id=%d type=GameRichPresenceJoinRequested delay=0.25 reason=%s lobby_id=%llu match_id=%llu owner=%llu local=%llu state=%u game_state=%u command=%s server_id=%llu",
            rich_join.k_iCallback,
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(owner_steam_id),
            static_cast<unsigned long long>(local_steam_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            connect_command.c_str(),
            static_cast<unsigned long long>(GBE_local_lobby.server_id)
        );
    } else {
        GBE_GC_DebugLog(
            "GC_DOTA_CONNECT_DIAG",
            "skipping rich presence join and launch command line for arcade server change reason=%s lobby_id=%llu match_id=%llu owner=%llu local=%llu command=%s server_id=%llu",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(owner_steam_id),
            static_cast<unsigned long long>(local_steam_id),
            connect_command.c_str(),
            static_cast<unsigned long long>(GBE_local_lobby.server_id)
        );
    }

    const bool reconnect_eligible_after_trigger = arcade_custom_launch;
    if (!reconnect_eligible_after_trigger)
        GBE_SetDotaReconnectEligible(false);
    GBE_SetLastDotaDirectConnectCallbackKey(callback_key);
    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "queued direct connect trigger reason=%s lobby_id=%llu match_id=%llu endpoint=%s endpoint_raw=%s command=%s local_is_owner=%u arcade=%u reconnect_eligible=%u",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        endpoint.c_str(),
        raw_endpoint.c_str(),
        connect_command.c_str(),
        local_is_owner ? 1u : 0u,
        arcade_custom_launch ? 1u : 0u,
        reconnect_eligible_after_trigger ? 1u : 0u
    );
}


void Steam_Game_Coordinator::GBE_MaybeQueueDotaPracticeLobbyLaunchPersonaState(const char *status, const char *lobby_state, bool include_party, bool include_lobby, const char *reason)
{
    if (is_server || gc_profile != GC_PROFILE_DOTA2 || !status || !lobby_state || !include_lobby || GBE_local_lobby.lobby_id == 0) {
        if (!include_lobby)
            GBE_ClearLastDotaLaunchPersonaSignature();
        return;
    }

    const char *template_hex = nullptr;
    if (std::strcmp(status, "#DOTA_RP_INIT") == 0 && std::strcmp(lobby_state, "SERVERSETUP") == 0 && !include_party) {
        template_hex = GBE_kDotaLaunchPersonaStateInitServerSetupHex;
    } else if (std::strcmp(status, "#DOTA_RP_FINDING_MATCH") == 0 && std::strcmp(lobby_state, "SERVERSETUP") == 0 && include_party) {
        template_hex = GBE_kDotaLaunchPersonaStateFindingMatchServerSetupHex;
    } else if (std::strcmp(status, "#DOTA_RP_FINDING_MATCH") == 0 && std::strcmp(lobby_state, "RUN") == 0 && include_party) {
        template_hex = GBE_kDotaLaunchPersonaStateFindingMatchRunHex;
    } else if (std::strcmp(status, "#DOTA_RP_PRIVATE_LOBBY") == 0 && std::strcmp(lobby_state, "RUN") == 0 && include_party) {
        template_hex = GBE_kDotaLaunchPersonaStatePrivateLobbyRunHex;
    }

    if (!template_hex) {
        GBE_ClearLastDotaLaunchPersonaSignature();
        return;
    }

    std::string signature;
    signature.reserve(96);
    signature.append(status);
    signature.push_back('|');
    signature.append(lobby_state);
    signature.push_back('|');
    signature.push_back(include_party ? '1' : '0');
    signature.push_back('|');
    signature.append(std::to_string(GBE_local_lobby.lobby_id));

    if (signature == GBE_GetLastDotaLaunchPersonaSignature()) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipping duplicate launch persona reason=%s lobby_id=%llu signature=%s",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            signature.c_str()
        );
        return;
    }

    std::string persona_message;
    const uint64 steam_id = settings ? settings->get_local_steam_id().ConvertToUint64() : 0ull;
    if (steam_id == 0 || !GBE_PrepareDotaPersonaStatePeripheralMessage(template_hex, steam_id, GBE_local_lobby.lobby_id, persona_message)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "failed building launch persona reason=%s lobby_id=%llu status=%s lobby_state=%s",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            status,
            lobby_state
        );
        return;
    }

    // Rich Presence is already updated via ISteamFriends::SetRichPresence
    // (called by GBE_UpdateDotaPracticeLobbyLaunchRichPresence before this point).
    // Do not push 766 (CMsgClientPersonaState) into GC queue -- Dota does not handle it.
    GBE_SetLastDotaLaunchPersonaSignature(signature);
    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "built launch persona reason=%s lobby_id=%llu status=%s lobby_state=%s size=%zu (not queued, using SetRichPresence)",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        status,
        lobby_state,
        persona_message.size()
    );
}


void Steam_Game_Coordinator::GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(const char *reason)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0)
        return;

    if (GBE_ShouldSuppressDotaAbandonedLobby(GBE_local_lobby.lobby_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipped reapplying launch rich presence for suppressed abandoned lobby reason=%s lobby_id=%llu state=%u game_state=%u",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state
        );
        return;
    }

    // Once the match is in HERO_SELECTION or later (state=2, game_state>=2),
    // the game engine takes over rich presence (showing hero name, level, etc.).
    // GC must not overwrite it.  This applies to BOTH the client GC (is_server=0)
    // and the server GC (is_server=1) in the host process, since they share the
    // same ISteamFriends interface and either one calling SetRichPresence will
    // clobber the engine's values.
    // Direct connect callback is still needed for reconnect scenarios, so we
    // call it unconditionally below (only for client GC).
    const bool engine_owns_rich_presence =
        !GBE_local_lobby.abandon_postgame_active &&
        GBE_local_lobby.state == 2u &&
        GBE_local_lobby.game_state >= 2u;

    if (engine_owns_rich_presence) {
        if (GBE_local_lobby.custom_game.game_id != 0ull) {
            Steam_Client *steam_client = get_steam_client();
            const std::string endpoint = gbe::proto_wire::get_dota_practice_lobby_first_connect_endpoint(GBE_local_lobby.connect);
            if (steam_client && steam_client->steam_friends && !endpoint.empty()) {
                const std::string connect_command = std::string("+connect ") + endpoint;
                steam_client->steam_friends->SetRichPresence("connect", connect_command.c_str());
                GBE_GC_DebugLog(
                    "GC_DOTA_SYNC",
                    "refreshed arcade active-match connect rich presence reason=%s lobby_id=%llu endpoint=%s",
                    reason ? reason : "unknown",
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    endpoint.c_str()
                );
            }
        }
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipping rich presence update (engine owns it during active match) reason=%s lobby_id=%llu state=%u game_state=%u",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state
        );
        if (GBE_local_lobby.custom_game.game_id != 0ull)
            GBE_MaybeQueueDotaPracticeLobbyDirectConnectCallback(reason);
        return;
    }

    const char *status = nullptr;
    const char *lobby_state = nullptr;
    bool include_party = false;
    const bool should_present_private_lobby =
        !GBE_local_lobby.abandon_postgame_active &&
        GBE_local_lobby.state == 2u &&
        GBE_local_lobby.game_state >= 1u;

    if (GBE_local_lobby.abandon_postgame_active) {
        status = "#DOTA_RP_PRIVATE_LOBBY";
        lobby_state = "RUN";
        include_party = true;
    } else if (GBE_local_lobby.state == 2u) {
        if (should_present_private_lobby) {
            status = "#DOTA_RP_PRIVATE_LOBBY";
        } else {
            status = "#DOTA_RP_FINDING_MATCH";
        }
        lobby_state = "RUN";
        include_party = true;
    } else if (GBE_local_lobby.state == 1u && GBE_local_lobby.game_state == 0u) {
        if (GBE_local_lobby.server_id != 0) {
            status = "#DOTA_RP_FINDING_MATCH";
            lobby_state = "SERVERSETUP";
            include_party = true;
        } else {
            status = "#DOTA_RP_INIT";
            lobby_state = "SERVERSETUP";
            include_party = false;
        }
    }

    if (!status || !lobby_state)
        return;

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "reapplying launch rich presence reason=%s lobby_id=%llu state=%u game_state=%u server_id=%llu status=%s lobby_state=%s include_party=%u",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        static_cast<unsigned long long>(GBE_local_lobby.server_id),
        status,
        lobby_state,
        include_party ? 1u : 0u
    );

    GBE_UpdateDotaPracticeLobbyLaunchRichPresence(status, lobby_state, include_party, !GBE_local_lobby.abandon_postgame_active);
    GBE_MaybeQueueDotaPracticeLobbyLaunchPersonaState(status, lobby_state, include_party, !GBE_local_lobby.abandon_postgame_active, reason);
    GBE_MaybeQueueDotaPracticeLobbyDirectConnectCallback(reason);
}


void Steam_Game_Coordinator::GBE_FinalizeDotaAbandonAfterOtherLeftChannel(uint64 consumed_lobby_id, const char *reason)
{
    if (is_server || gc_profile != GC_PROFILE_DOTA2 || !GBE_local_lobby.abandon_postgame_active)
        return;
    if (GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.lobby_id != consumed_lobby_id)
        return;

    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Resetting abandon teardown state after 7014 retrieval LobbyID=%llu reason=%s",
        static_cast<unsigned long long>(lobby_id),
        reason ? reason : "unknown"
    );
    GBE_ExecuteDotaLifecycleActions(gbe::dota_lobby_flow::abandon_finalize_action_list(reason));
}


void Steam_Game_Coordinator::GBE_FinalizeDotaNormalSignoutAfterCacheUnsubscribed(uint64 consumed_lobby_id, const char *reason)
{
    if (gc_profile != GC_PROFILE_DOTA2)
        return;

    if (!settings)
        return;

    const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    const auto shared_lobby = GBE_SharedLobbyStore().snapshot();
    const uint64 shared_lobby_id = shared_lobby.lobby_id;
    const uint64 lobby_id = consumed_lobby_id != 0 ? consumed_lobby_id : shared_lobby_id;
    const GBE_LocalLobby postgame_lobby = GBE_local_lobby;
    Steam_Game_Coordinator *client_target = this;
    if (is_server) {
        Steam_Client *steam_client = get_steam_client();
        if (steam_client && steam_client->steam_game_coordinator)
            client_target = steam_client->steam_game_coordinator;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Finalizing normal signout after 25 retrieval LobbyID=%llu shared_lobby=%llu reason=%s",
        static_cast<unsigned long long>(lobby_id),
        static_cast<unsigned long long>(shared_lobby_id),
        reason ? reason : "unknown"
    );

    const bool push_client_cache_unsubscribed = client_target && client_target->gc_profile == GC_PROFILE_DOTA2 && client_target != this && lobby_id != 0;
    std::string client_response_25;
    const bool built_client_response_25 = !push_client_cache_unsubscribed ||
        gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(lobby_id, client_response_25);
    if (push_client_cache_unsubscribed && !built_client_response_25) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "failed building normal signout client cache unsubscribe label=25 lobby_id=%llu",
            static_cast<unsigned long long>(lobby_id)
        );
    }

    gbe::dota_lifecycle::ExecutionOptions finalize_options;
    finalize_options.client_target = client_target;
    finalize_options.client_lobby_restore = &postgame_lobby;
    finalize_options.mirror_launch_peripheral_to_client_target = true;
    finalize_options.route_rich_presence_to_client_target = true;
    finalize_options.route_push_to_client_target = true;
    GBE_ExecuteDotaLifecycleActions(
        gbe::dota_lobby_flow::normal_signout_finalize_action_list(
            lobby_id,
            client_response_25,
            push_client_cache_unsubscribed && built_client_response_25,
            reason ? reason : "unknown"),
        finalize_options);
    if (push_client_cache_unsubscribed && built_client_response_25 && client_target && client_target->gc_profile == GC_PROFILE_DOTA2) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "queued normal signout client cache unsubscribe label=25 lobby_id=%llu size=%zu",
            static_cast<unsigned long long>(lobby_id),
            client_response_25.size()
        );
    }

    std::string persona_message;
    if (GBE_PrepareDotaPersonaStatePeripheralMessage(GBE_kDotaAbandonPersonaStateInitHex, steam_id, lobby_id, persona_message)) {
        // Rich Presence handled via ISteamFriends::SetRichPresence.
        // Do not push 766 into GC queue -- Dota does not handle it.
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "built normal signout persona label=25_init lobby_id=%llu size=%zu (not queued, using SetRichPresence)",
            static_cast<unsigned long long>(lobby_id),
            persona_message.size()
        );
    } else {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "failed building normal signout persona label=25_init lobby_id=%llu",
            static_cast<unsigned long long>(lobby_id)
        );
    }
}
