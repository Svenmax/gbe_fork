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

// Type aliases for proto-wire request/response shapes. These mirror the
// aliases defined in steam_game_coordinator.cpp and are needed by the
// handlers below. They are alias-declarations (no linkage), so duplicating
// them across TUs is safe.
using GBE_DotaPracticeLobbyDetailsRequest = gbe::proto_wire::DotaPracticeLobbyDetailsRequest;
using GBE_DotaPracticeLobbyCreateRequest = gbe::proto_wire::DotaPracticeLobbyCreateRequest;
using GBE_DotaPracticeLobbyJoinRequest = gbe::proto_wire::DotaPracticeLobbyJoinRequest;
using GBE_DotaInviteToLobbyRequest = gbe::proto_wire::DotaInviteToLobbyRequest;
using GBE_DotaLobbyInviteResponseRequest = gbe::proto_wire::DotaLobbyInviteResponseRequest;
using GBE_DotaPracticeLobbySetTeamSlotRequest = gbe::proto_wire::DotaPracticeLobbySetTeamSlotRequest;
using GBE_DotaPracticeLobbyKickRequest = gbe::proto_wire::DotaPracticeLobbyKickRequest;

using GBE_DotaPracticeLobbyBroadcastChannelRequest = gbe::proto_wire::DotaPracticeLobbyBroadcastChannelRequest;

using GBE_DotaJoinChatChannelRequest = gbe::proto_wire::DotaJoinChatChannelRequest;
using GBE_DotaLeaveChatChannelRequest = gbe::proto_wire::DotaLeaveChatChannelRequest;
using GBE_DotaChatMessageRequest = gbe::proto_wire::DotaChatMessageRequest;
using GBE_Dota8053Result = gbe::proto_wire::Dota8053Result;

using GBE_DotaEmptyRequestShape = gbe::proto_wire::DotaEmptyRequestShape;
using GBE_DotaRankRequestShape = gbe::proto_wire::DotaRankRequestShape;
using GBE_Dota7034ConnectedPlayer = gbe::proto_wire::Dota7034ConnectedPlayer;
using GBE_Dota7034DisconnectedPlayer = gbe::proto_wire::Dota7034DisconnectedPlayer;
using GBE_Dota7034RequestShape = gbe::proto_wire::Dota7034RequestShape;

// Proto field locator used by handler template-replay patching.
// Moved here from steam_game_coordinator.cpp (now handler-only).
struct GBE_ProtoField
{
    bool found{};
    uint32 field_number{};
    uint32 wire_type{};
    size_t value_offset{};
    size_t value_size{};

    gbe::proto_wire::Field as_proto_wire_field() const
    {
        gbe::proto_wire::Field field{};
        field.number = field_number;
        field.wire_type = wire_type;
        field.value_offset = value_offset;
        field.value_size = value_size;
        return field;
    }
};

// --- List A: handler-only static helpers (moved from steam_game_coordinator.cpp) ---

static constexpr uint32 GBE_kSteamGamesPlayedWithDataBlob = 5410u;


static constexpr uint32 GBE_kSteamAuthList = 5432u;


static constexpr const char *GBE_kDota8730TemplateHex =
    "1a22008009000000594600000000000000080112100a0e088ea83d1206cfb9cfb9d48c18001200"
    "120f0a0d08e0d61f1205313773656318001200120d0a0b0884b0331203534b2b180012120a1008"
    "f7a9021208d09fd090d09fd090180012160a1408bfcb03120ce58fb2e4b88ae69c80e5bcba1800"
    "12100a0e08ded42a120632306d6dd1801800120012100a0e08f3b03a1206e6a097e889b2180012"
    "0f0a0d08a9b63b12055b3939395d1800120e0a0c088ca4011204636963611800120f0a0d0884f0"
    "19120575796b75791800120d0a0b08d9d20612035246351800120012160a14088d851b120ce981"
    "97e8bfb9e58fb2e8af971800120e0a0c08b6e8261204486c6c45180012120a1008d2fe1e1208e2"
    "969a5450e29784180012140a1208ca8737120ad184d188d0b8d181d0bf1800120f0a0d08e1fe3e"
    "12055f6e415353180012190a170888ba28120fe99693e98195e38184e381aae381841800120012"
    "190a170884c206120fe4bda0e694bee5ada6e588abe8b5b0180012110a0f08dcb60f1207e299a5"
    "20e299821800120f0a0d08b3b73e12056e756d6239180012120a1008b1d5281208d0a1d09bd090"
    "d0911800120f0a0d08b1940112054d4f4c43481800120012120a1008acc12912052d5552412d18"
    "8080800112130a1108e9c9031209e385a4e29885e385a41800120f0a0d08c9b03c120557455853"
    "531800120f0a0d08d4e223120553742e203118001200120f0a0d08bcc70d120543687672731800"
    "120e0a0c08c2d2171204c2a17a2118001200120f0a0d08dfb91a1205545072737218001200";


static constexpr const char *GBE_kDota8331TemplateHex =
    "4d15008014000000090eac2b7cdec1400110dbc3fdeafdffffffff0108ba04108bc18080081a138b200080090000005917000000000000000804";


static constexpr const char *GBE_kDotaOfficial8745TemplateHex =
    "4d15008014000000090eac2b7cdec1400110dbc3fdeafdffffffff0108ba0410a9c48080081a112922008009000000592100000000000000";


static const uint8 GBE_kDota8678Template[] = {
    0xE6, 0x21, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00,
};


static const uint8 GBE_kDota8136Template[] = {
    0xC8, 0x1F, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x59, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00,
};


static const uint8 GBE_kDota2538Template[] = {
    0xEA, 0x09, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x10, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F,
};


static const uint8 GBE_kDota2618Template[] = {
    0x3A, 0x0A, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x59, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x01,
};


static const uint8 GBE_kDota8674Template[] = {
    0xE2, 0x21, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x59, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x05,
};


static const uint8 GBE_kDota8677Template[] = {
    0xE5, 0x21, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x59, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x01, 0x12, 0x00,
};


static const uint8 GBE_kDota7198Template[] = {
    0x1E, 0x1C, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x38, 0x95, 0x03, 0x38, 0xE3, 0x04, 0x38, 0xA1,
    0x1F, 0x38, 0xB4, 0x16, 0x38, 0x00, 0x38, 0xBF, 0x03, 0x38, 0xD1, 0x03, 0x38, 0xAA, 0x36, 0x38,
    0xF3, 0x1C, 0x38, 0xA5, 0x01, 0x38, 0x30, 0x38, 0xEB, 0x08, 0x38, 0xB5, 0x02, 0x38, 0x4E, 0x38,
    0xE1, 0x03, 0x38, 0xCF, 0x07, 0x38, 0x6B, 0x38, 0xB5, 0x04, 0x38, 0xE2, 0x07, 0x38, 0xDE, 0x06,
    0x38, 0xB7, 0x09, 0x38, 0x00, 0x38, 0x00, 0x38, 0x00, 0x38, 0x00, 0x38, 0xD8, 0x02, 0x42, 0x09,
    0x08, 0x95, 0x03, 0x10, 0x00, 0x18, 0x00, 0x20, 0x00, 0x42, 0x09, 0x08, 0xE3, 0x04, 0x10, 0x00,
    0x18, 0x00, 0x20, 0x00, 0x42, 0x09, 0x08, 0xA1, 0x1F, 0x10, 0x00, 0x18, 0x00, 0x20, 0x22, 0x42,
    0x0A, 0x08, 0xB4, 0x16, 0x10, 0x06, 0x18, 0x00, 0x20, 0xBC, 0x01, 0x42, 0x00, 0x42, 0x09, 0x08,
    0xBF, 0x03, 0x10, 0x00, 0x18, 0x00, 0x20, 0x00, 0x42, 0x09, 0x08, 0xD1, 0x03, 0x10, 0x00, 0x18,
    0x00, 0x20, 0x00, 0x42, 0x09, 0x08, 0xAA, 0x36, 0x10, 0x00, 0x18, 0x00, 0x20, 0x6C, 0x42, 0x09,
    0x08, 0xF3, 0x1C, 0x10, 0x00, 0x18, 0x00, 0x20, 0x00, 0x42, 0x09, 0x08, 0xA5, 0x01, 0x10, 0x00,
    0x18, 0x00, 0x20, 0x0E, 0x42, 0x08, 0x08, 0x30, 0x10, 0x00, 0x18, 0x00, 0x20, 0x00, 0x42, 0x0A,
    0x08, 0xEB, 0x08, 0x10, 0x00, 0x18, 0x00, 0x20, 0xA0, 0x02, 0x42, 0x0A, 0x08, 0xB5, 0x02, 0x10,
    0x00, 0x18, 0x00, 0x20, 0x80, 0x02, 0x42, 0x08, 0x08, 0x4E, 0x10, 0x00, 0x18, 0x00, 0x20, 0x3E,
    0x42, 0x09, 0x08, 0xE1, 0x03, 0x10, 0x00, 0x18, 0x00, 0x20, 0x00, 0x42, 0x09, 0x08, 0xCF, 0x07,
    0x10, 0x00, 0x18, 0x00, 0x20, 0x00, 0x42, 0x09, 0x08, 0x6B, 0x10, 0x00, 0x18, 0x00, 0x20, 0x92,
    0x01, 0x42, 0x0A, 0x08, 0xB5, 0x04, 0x10, 0x00, 0x18, 0x00, 0x20, 0x9A, 0x02, 0x42, 0x0A, 0x08,
    0xE2, 0x07, 0x10, 0x00, 0x18, 0x00, 0x20, 0x9C, 0x02, 0x42, 0x0A, 0x08, 0xDE, 0x06, 0x10, 0x00,
    0x18, 0x00, 0x20, 0xBC, 0x01, 0x42, 0x0A, 0x08, 0xB7, 0x09, 0x10, 0x00, 0x18, 0x00, 0x20, 0xA2,
    0x02, 0x42, 0x08, 0x08, 0x00, 0x10, 0x01, 0x18, 0x02, 0x20, 0x01, 0x42, 0x00, 0x42, 0x00, 0x42,
    0x00, 0x42, 0x0A, 0x08, 0xD8, 0x02, 0x10, 0x00, 0x18, 0x00, 0x20, 0x82, 0x02,
};


static const uint8 GBE_kDota8079Template[] = {
    0x8F, 0x1F, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x59, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x01,
};


static const uint8 GBE_kDota8854Template[] = {
    0x96, 0x22, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x59, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x01, 0x12, 0x20, 0x0A, 0x0E, 0x08, 0x51, 0x10, 0xFF, 0xB9, 0x01, 0x18, 0x04, 0x20,
    0xCD, 0x87, 0xD5, 0xB8, 0x5F, 0x0A, 0x0E, 0x08, 0x54, 0x10, 0xFE, 0xB9, 0x01, 0x18, 0x04, 0x20,
    0xBD, 0xFA, 0xD4, 0xB8, 0x5F,
};


static const uint8 GBE_kDota9024Template[] = {
    0x40, 0x23, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x59, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x01, 0x12, 0x02, 0x18, 0x00,
};


static void GBE_ApplyDotaCustomGameDetailsRequest(const GBE_DotaPracticeLobbyDetailsRequest &request, GBE_DotaCustomGameDetails &custom_game)
{
    if (request.has_custom_game_mode)
        custom_game.mode = request.custom_game_mode;
    if (request.has_custom_map_name)
        custom_game.map_name = request.custom_map_name;
    if (request.has_custom_difficulty)
        custom_game.difficulty = request.custom_difficulty;
    if (request.has_custom_game_id)
        custom_game.game_id = request.custom_game_id;
    if (request.has_custom_min_players)
        custom_game.min_players = request.custom_min_players;
    if (request.has_custom_max_players)
        custom_game.max_players = request.custom_max_players;
    if (request.has_custom_game_crc)
        custom_game.crc = request.custom_game_crc;
    if (request.has_custom_game_timestamp)
        custom_game.timestamp = request.custom_game_timestamp;
    if (request.has_custom_game_penalties)
        custom_game.penalties = request.custom_game_penalties;
}


static void GBE_NormalizeDotaCustomGameDetailsFromInstalledMod(class Settings *settings, GBE_DotaCustomGameDetails &custom_game)
{
    if (!settings || custom_game.game_id == 0ull || !settings->isModInstalled(static_cast<PublishedFileId_t>(custom_game.game_id)))
        return;

    Mod_entry mod = settings->getMod(static_cast<PublishedFileId_t>(custom_game.game_id));
    const std::string addon_name = gbe::dota_custom_game::metadata_value_for_gc(mod.metadata, "addon_name", mod.title);
    const std::string metadata_map_name = gbe::dota_custom_game::metadata_value_for_gc(mod.metadata, "map_name", addon_name);

    if (gbe::proto_wire::dota_is_readable_custom_game_name(addon_name) && (custom_game.mode.empty() || gbe::proto_wire::dota_string_is_unsigned_integer(custom_game.mode)))
        custom_game.mode = addon_name;
    if (gbe::proto_wire::dota_is_readable_custom_game_name(metadata_map_name) && (custom_game.map_name.empty() || custom_game.map_name == "dota" || gbe::proto_wire::dota_string_is_unsigned_integer(custom_game.map_name)))
        custom_game.map_name = metadata_map_name;
}


static uint64 GBE_GenerateDotaLobbyId()
{
    std::random_device device;
    std::mt19937_64 generator(
        (static_cast<uint64>(device()) << 32) ^
        static_cast<uint64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );

    for (int attempt = 0; attempt < 64; ++attempt) {
        const uint64 candidate = (generator() & 0x00FFFFFFFFFFFFFFull) | 0x0002000000000000ull;
        std::vector<uint8> encoded;
        if (candidate != 0 && gbe::proto_wire::encode_varuint_with_expected_size(candidate, GBE_kOldDotaLobbyIdVarint.size(), encoded))
            return candidate;
    }

    return 29799760111995806ull;
}


static uint64 GBE_GenerateDotaMatchId()
{
    std::random_device device;
    std::mt19937_64 generator(
        (static_cast<uint64>(device()) << 32) ^
        static_cast<uint64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );

    for (int attempt = 0; attempt < 128; ++attempt) {
        const uint64 candidate = 0x100000000ull + (generator() & 0x00000003FFFFFFFFull);
        std::vector<uint8> encoded;
        if (candidate != 0 && gbe::proto_wire::encode_varuint_with_expected_size(candidate, GBE_kOldDotaPracticeLobbyMatchIdVarint.size(), encoded))
            return candidate;
    }

    return 8781757536ull;
}


static bool GBE_AdaptDotaLobbyInviteCacheSubscribedPayload(
    uint64 lobby_id,
    uint64 inviter_steam_id,
    uint64 invitee_steam_id,
    const std::string &inviter_name,
    const std::vector<std::pair<uint64, std::string>> &members,
    std::string &message)
{
    uint64 cache_version = 0;
    uint64 invite_gid = 0;
    std::vector<std::pair<std::uint64_t, std::string>> parsed_members;
    parsed_members.reserve(members.size());
    for (const auto &member : members)
        parsed_members.push_back({ member.first, member.second });
    std::uint64_t parsed_invite_gid = 0;
    std::uint64_t parsed_cache_version = 0;
    if (!gbe::gc_message::build_dota_lobby_invite_cache_subscribed_payload(lobby_id, inviter_steam_id, invitee_steam_id, inviter_name, parsed_members, message, &parsed_invite_gid, &parsed_cache_version))
        return false;
    invite_gid = static_cast<uint64>(parsed_invite_gid);
    cache_version = static_cast<uint64>(parsed_cache_version);

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Built 2011 lobby invite lobby_id=%llu invitee=%llu inviter=%llu invite_gid=%llu cache_version=%llu members=%zu",
        static_cast<unsigned long long>(lobby_id),
        static_cast<unsigned long long>(invitee_steam_id),
        static_cast<unsigned long long>(inviter_steam_id),
        static_cast<unsigned long long>(invite_gid),
        static_cast<unsigned long long>(cache_version),
        members.size());
    return true;
}


static bool GBE_IsDotaLobbyInviteCacheSubscribedPayload(const std::string &message)
{
    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return false;

    CMsgSOCacheSubscribed protomsg;
    if (!protomsg.ParseFromArray(proto_context.body, static_cast<int>(proto_context.body_size)))
        return false;

    for (int object_index = 0; object_index < protomsg.objects_size(); ++object_index) {
        const auto &object = protomsg.objects(object_index);
        if (object.type_id() == 2011 && object.object_data_size() > 0)
            return true;
    }

    return false;
}


static bool GBE_AdaptDota7034ConnectedPlayersResponsePayload(
    uint64 steam_id,
    uint32 lobby_state,
    uint32 game_state,
    uint32 owner_team,
    uint32 owner_slot,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    const GBE_Dota7034RequestShape &request_shape,
    bool compact_member_slots,
    bool include_draft_players,
    bool has_request_job,
    uint64 request_job_id,
    std::string &message)
{
    const bool include_draft = include_draft_players && (lobby_state >= 2u && game_state >= 2u);
    std::vector<uint64> connected_steam_ids;
    std::vector<uint64> disconnected_steam_ids;
    std::vector<gbe::gc_message::Dota7034Player> connected_players;
    std::vector<gbe::gc_message::Dota7034Player> disconnected_players;
    std::vector<gbe::gc_message::Dota7034Player> draft_players;

    auto resolve_draft_slot = [&](uint64 player_steam_id, uint32 slot) -> uint32 {
        if (compact_member_slots) {
            for (size_t index = 0; index < members.size(); ++index) {
                if (members[index].steam_id == player_steam_id)
                    return static_cast<uint32>(index);
            }
        }
        return slot > 0u ? (slot - 1u) : 0u;
    };

    auto append_connected_player = [&](uint64 player_steam_id, uint32 hero_id, uint32 team, uint32 slot) {
        if (player_steam_id == 0ull || std::find(connected_steam_ids.begin(), connected_steam_ids.end(), player_steam_id) != connected_steam_ids.end())
            return;

        connected_players.push_back({ player_steam_id, hero_id, team, include_draft ? resolve_draft_slot(player_steam_id, slot) : slot, lobby_state, game_state, include_draft });
        connected_steam_ids.push_back(player_steam_id);
    };

    auto append_disconnected_player = [&](uint64 player_steam_id, uint32 disconnected_lobby_state, uint32 disconnected_game_state) {
        if (player_steam_id == 0ull || std::find(disconnected_steam_ids.begin(), disconnected_steam_ids.end(), player_steam_id) != disconnected_steam_ids.end())
            return;

        disconnected_players.push_back({ player_steam_id, 0u, 0u, 0u, disconnected_lobby_state, disconnected_game_state });
        disconnected_steam_ids.push_back(player_steam_id);
    };

    if (request_shape.has_connected_player || request_shape.has_disconnected_player) {
        for (const GBE_Dota7034ConnectedPlayer &connected_player : request_shape.connected_players) {
            if (!connected_player.has_steam_id || connected_player.steam_id == 0ull)
                continue;
            uint32 hero_id = 0u;
            uint32 team = owner_team;
            uint32 slot = owner_slot;
            for (const GBE_DotaLobbyMemberState &member : members) {
                if (member.steam_id == connected_player.steam_id) {
                    hero_id = member.hero_id;
                    team = member.team;
                    slot = member.slot;
                    break;
                }
            }
            if (connected_player.has_hero_id && connected_player.hero_id != 0u)
                hero_id = connected_player.hero_id;
            append_connected_player(connected_player.steam_id, hero_id, team, slot);
        }

        for (const GBE_Dota7034DisconnectedPlayer &disconnected_player : request_shape.disconnected_players) {
            if (!disconnected_player.has_steam_id || disconnected_player.steam_id == 0ull)
                continue;
            const uint32 disconnected_lobby_state = disconnected_player.has_lobby_state ? disconnected_player.lobby_state : lobby_state;
            const uint32 disconnected_game_state = disconnected_player.has_game_state ? disconnected_player.game_state : game_state;
            append_disconnected_player(disconnected_player.steam_id, disconnected_lobby_state, disconnected_game_state);
        }
    } else {
        append_connected_player(steam_id, 0u, owner_team, owner_slot);
        for (const GBE_DotaLobbyMemberState &member : members) {
            if (member.steam_id == 0ull || member.steam_id == steam_id)
                continue;

            if (member.connected)
                append_connected_player(member.steam_id, member.hero_id, member.team, member.slot);
            else
                append_disconnected_player(member.steam_id, lobby_state, game_state);
        }
    }

    gbe::gc_message::Dota7034ExtraState extra_state{};
    extra_state.has_first_blood_happened = request_shape.has_first_blood_happened;
    extra_state.first_blood_happened = request_shape.first_blood_happened;
    extra_state.has_send_reason = request_shape.has_send_reason;
    extra_state.send_reason = request_shape.send_reason;
    extra_state.has_radiant_kills = request_shape.has_radiant_kills;
    extra_state.radiant_kills = request_shape.radiant_kills;
    extra_state.has_dire_kills = request_shape.has_dire_kills;
    extra_state.dire_kills = request_shape.dire_kills;
    extra_state.has_radiant_lead = request_shape.has_radiant_lead;
    extra_state.radiant_lead = request_shape.radiant_lead;
    extra_state.has_building_state = request_shape.has_building_state;
    extra_state.building_state = request_shape.building_state;
    bool disconnected_request_has_steam_id = false;
    bool disconnected_request_includes_local = false;
    for (const GBE_Dota7034DisconnectedPlayer &disconnected_player : request_shape.disconnected_players) {
        if (!disconnected_player.has_steam_id)
            continue;
        disconnected_request_has_steam_id = true;
        if (disconnected_player.steam_id == steam_id) {
            disconnected_request_includes_local = true;
            break;
        }
    }
    if (request_shape.has_disconnected_player && (!disconnected_request_has_steam_id || disconnected_request_includes_local)) {
        uint32 disconnected_lobby_state = lobby_state;
        uint32 disconnected_game_state = game_state;
        for (const GBE_Dota7034DisconnectedPlayer &disconnected_player : request_shape.disconnected_players) {
            if (disconnected_request_includes_local && disconnected_player.steam_id != steam_id)
                continue;
            if (disconnected_player.has_lobby_state)
                disconnected_lobby_state = disconnected_player.lobby_state;
            if (disconnected_player.has_game_state)
                disconnected_game_state = disconnected_player.game_state;
            if (!disconnected_request_includes_local || disconnected_player.steam_id == steam_id)
                break;
        }
        if (disconnected_lobby_state < lobby_state)
            disconnected_lobby_state = lobby_state;
        if (disconnected_game_state < game_state)
            disconnected_game_state = game_state;

        append_disconnected_player(steam_id, disconnected_lobby_state, disconnected_game_state);
    }

    return gbe::gc_message::build_dota_7034_connected_players_response_payload(connected_players, disconnected_players, draft_players, game_state, extra_state, has_request_job, request_job_id, message);
}


bool Steam_Game_Coordinator::GBE_HandleDotaDirect7034Request(
    uint32 request_emsg,
    const uint8 *body,
    size_t body_size,
    bool has_source_job,
    uint64 source_job)
{
    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && GBE_local_lobby.match_id != 0 && (GBE_local_lobby.server_id != 0 || !GBE_local_lobby.connect.empty())) {
        const GBE_Dota7034RequestShape request_shape = gbe::proto_wire::parse_dota7034_request_shape(body, body_size);
        bool queued_runtime_lobby_update = false;
        const bool custom_game_launch = GBE_local_lobby.custom_game.game_id != 0ull;

        bool updated_owner_team_or_slot_from_7034 = false;
        if (!custom_game_launch && request_shape.has_draft_steam_id && request_shape.draft_steam_id == GBE_GetDotaLobbyOwnerSteamId()) {
            if (request_shape.has_draft_team && GBE_local_lobby.owner_team != request_shape.draft_team) {
                GBE_local_lobby.owner_team = request_shape.draft_team;
                updated_owner_team_or_slot_from_7034 = true;
            }

            const uint32 draft_owner_slot = request_shape.has_draft_team_slot ? (request_shape.draft_team_slot + 1u) : 0u;
            if (draft_owner_slot != 0u && GBE_local_lobby.owner_slot != draft_owner_slot) {
                GBE_local_lobby.owner_slot = draft_owner_slot;
                updated_owner_team_or_slot_from_7034 = true;
            }
        }

        if (updated_owner_team_or_slot_from_7034) {
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "updated owner team/slot from 7034 draft team=%u slot=%u source_job=%llu state=%u game_state=%u",
                GBE_local_lobby.owner_team,
                GBE_local_lobby.owner_slot,
                static_cast<unsigned long long>(source_job),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state
            );
            GBE_PublishSharedDotaLobbyState("7034_draft_team_slot");
        }

        bool owner_hero_updated_from_7034 = false;
        const uint64 owner_steam_id = GBE_GetDotaLobbyOwnerSteamId();
        for (const GBE_Dota7034ConnectedPlayer &connected_player : request_shape.connected_players) {
            if (!connected_player.has_steam_id || connected_player.steam_id == 0ull)
                continue;
            const uint32 previous_owner_hero_id = GBE_local_lobby.owner_hero_id;
            if (GBE_SetDotaLobbyMemberRuntimeState(connected_player.steam_id, true, connected_player.hero_id, connected_player.has_hero_id)) {
                GBE_PublishSharedDotaLobbyState("7034_connected_player");
                GBE_GC_DebugLog(
                    "GC_DOTA_DIRECT",
                    "marked connected player from 7034 steam_id=%llu hero_id=%u has_hero=%u source_job=%llu state=%u game_state=%u",
                    static_cast<unsigned long long>(connected_player.steam_id),
                    connected_player.hero_id,
                    connected_player.has_hero_id ? 1u : 0u,
                    static_cast<unsigned long long>(source_job),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state
                );
            }
            if (connected_player.steam_id == owner_steam_id && connected_player.has_hero_id && connected_player.hero_id != 0u && previous_owner_hero_id != GBE_local_lobby.owner_hero_id)
                owner_hero_updated_from_7034 = true;
        }

        if (custom_game_launch) {
            GBE_LocalLobby refreshed_lobby{};
            GBE_CaptureCurrentDotaLobbyState("7034_custom_runtime_member_refresh", refreshed_lobby, false);
            if (GBE_NormalizeDotaArcadeLobbyMemberSlots(GBE_local_lobby))
                GBE_PublishSharedDotaLobbyState("7034_custom_runtime_slot_normalize");
        }

        if (owner_hero_updated_from_7034) {
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "updated owner hero from 7034 request hero_id=%u source_job=%llu state=%u game_state=%u",
                GBE_local_lobby.owner_hero_id,
                static_cast<unsigned long long>(source_job),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state
            );

            GBE_HandleDotaDirectOwnerHeroKnownEquipReplay(owner_steam_id, source_job);
        }

        GBE_HandleDotaDirect7034DisconnectedPlayers(request_shape.disconnected_players, source_job);

        if (GBE_local_lobby.state == 1u &&
            GBE_local_lobby.game_state == 0u &&
            GBE_local_lobby.launch_phase >= GBE_kDotaLaunchPhaseSetupSynced &&
            GBE_local_lobby.launch_4511_seen) {
            if (GBE_TryAdvanceDotaLaunchToRun("runtime packet after matched 4511/7034", request_emsg, source_job, "7034_launch_run_after_4511"))
                queued_runtime_lobby_update = true;
        }

        if (GBE_local_lobby.state == 1u && GBE_local_lobby.game_state == 0u) {
            const std::string request_summary = gbe::proto_wire::format_dota7034_summary(body, body_size);
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "consumed req=%u source_job=%llu note=prelaunch 7034 observed before RUN apply active=%u lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu launch_phase=%s summary=%s",
                request_emsg,
                static_cast<unsigned long long>(source_job),
                GBE_local_lobby.active ? 1u : 0u,
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state,
                static_cast<unsigned long long>(GBE_local_lobby.match_id),
                static_cast<unsigned long long>(GBE_local_lobby.server_id),
                GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase),
                request_summary.c_str()
            );
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "consumed req=%u source_job=%llu note=runtime 7034 active=%u lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu summary=%s",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            GBE_local_lobby.active ? 1u : 0u,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id),
            gbe::proto_wire::format_dota7034_summary(body, body_size).c_str()
        );

        GBE_HandleDotaDirect7034RuntimeUpdates(request_emsg, body, body_size, request_shape, custom_game_launch, source_job, queued_runtime_lobby_update);

        if (custom_game_launch)
            return true;
    }

    const GBE_Dota7034RequestShape request_shape = gbe::proto_wire::parse_dota7034_request_shape(body, body_size);
    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "parsed req=%u source_job=%llu body_size=%zu summary=%s",
        request_emsg,
        static_cast<unsigned long long>(source_job),
        body_size,
        gbe::proto_wire::format_dota7034_summary(body, body_size).c_str()
    );

    return GBE_HandleDotaDirect7034Response(request_emsg, request_shape, body, body_size, has_source_job, source_job);
}


void Steam_Game_Coordinator::GBE_HandleDotaDirectOwnerHeroKnownEquipReplay(
    uint64 owner_steam_id,
    uint64 source_job)
{
    if (!is_server || gc_profile != GC_PROFILE_DOTA2)
        return;

    Steam_Client *steam_client = get_steam_client();
    Steam_Game_Coordinator *client_gc = steam_client ? steam_client->steam_game_coordinator : nullptr;
    if (!client_gc || owner_steam_id == 0ull)
        return;

    const CSteamID owner_id(owner_steam_id);
    const auto &client_items = client_gc->get_items();
    if (GBE_PushDotaPlayerEquippedItemsCacheToGC(this, owner_id, client_items, true, "7034_owner_hero_known_server")) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replayed host equipped items to server GC after owner hero became known: steam64=%llu hero_id=%u source_job=%llu",
            static_cast<unsigned long long>(owner_steam_id),
            GBE_local_lobby.owner_hero_id,
            static_cast<unsigned long long>(source_job)
        );
    }
}


void Steam_Game_Coordinator::GBE_HandleDotaDirect7034DisconnectedPlayers(
    const std::vector<GBE_Dota7034DisconnectedPlayer> &disconnected_players,
    uint64 source_job)
{
    for (const GBE_Dota7034DisconnectedPlayer &disconnected_player : disconnected_players) {
        if (!disconnected_player.has_steam_id || disconnected_player.steam_id == 0ull)
            continue;
        if (GBE_SetDotaLobbyMemberRuntimeState(disconnected_player.steam_id, false, 0u, false)) {
            GBE_PublishSharedDotaLobbyState("7034_disconnected_player");
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "marked disconnected player from 7034 steam_id=%llu source_job=%llu state=%u game_state=%u",
                static_cast<unsigned long long>(disconnected_player.steam_id),
                static_cast<unsigned long long>(source_job),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state
            );
        }
    }
}


void Steam_Game_Coordinator::GBE_HandleDotaDirect7034RuntimeUpdates(
    uint32 request_emsg,
    const uint8 *body,
    size_t body_size,
    const GBE_Dota7034RequestShape &request_shape,
    bool custom_game_launch,
    uint64 source_job,
    bool &queued_runtime_lobby_update)
{
    const bool request_advances_to_hero_selection = request_shape.has_game_state && request_shape.game_state >= 2u;

    if (custom_game_launch &&
            GBE_local_lobby.state == 2u &&
            GBE_local_lobby.launch_phase >= GBE_kDotaLaunchPhaseRunQueued &&
            request_shape.has_game_state &&
            request_shape.game_state > GBE_local_lobby.game_state) {
        if (GBE_TryQueueDotaRuntimeLobbyDetailsUpdate("custom game 7034 game_state", request_emsg, source_job, 2u, request_shape.game_state))
            queued_runtime_lobby_update = true;
    }

    GBE_HandleDotaDirect7034WaitForPlayers(request_emsg, body, body_size, custom_game_launch, request_advances_to_hero_selection, source_job, queued_runtime_lobby_update);

    if (!custom_game_launch && GBE_local_lobby.state == 2u && GBE_local_lobby.game_state == 0u &&
            GBE_local_lobby.launch_phase >= GBE_kDotaLaunchPhaseRunQueued) {
        if (GBE_TryQueueDotaPrelaunch021("runtime wait_for_players after 7034", request_emsg, source_job))
            queued_runtime_lobby_update = true;
    }

    GBE_HandleDotaDirect7034StrategyTime(request_emsg, body, body_size, request_shape, custom_game_launch, source_job, queued_runtime_lobby_update);

    GBE_HandleDotaDirect7034LaunchPoll(request_emsg, source_job, queued_runtime_lobby_update);

    if (queued_runtime_lobby_update && !custom_game_launch) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "continuing req=%u source_job=%llu with connected players reply after runtime 26 updates state=%u game_state=%u",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state
        );
    }

    if (custom_game_launch) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "consumed req=%u source_job=%llu note=custom game 7034 handled by runtime 26 update state=%u game_state=%u summary=%s",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            gbe::proto_wire::format_dota7034_summary(body, body_size).c_str()
        );
    }
}


void Steam_Game_Coordinator::GBE_HandleDotaDirect7034StrategyTime(
    uint32 request_emsg,
    const uint8 *body,
    size_t body_size,
    const GBE_Dota7034RequestShape &request_shape,
    bool custom_game_launch,
    uint64 source_job,
    bool &queued_runtime_lobby_update)
{
    GBE_HandleDotaDirect7034StrategyTimeFallback(request_emsg, body, body_size, request_shape, custom_game_launch, source_job, queued_runtime_lobby_update);
    GBE_HandleDotaDirect7034StrategyTimePreserve(request_emsg, source_job, queued_runtime_lobby_update);
}


void Steam_Game_Coordinator::GBE_HandleDotaDirect7034StrategyTimeFallback(
    uint32 request_emsg,
    const uint8 *body,
    size_t body_size,
    const GBE_Dota7034RequestShape &request_shape,
    bool custom_game_launch,
    uint64 source_job,
    bool &queued_runtime_lobby_update)
{
    if (!custom_game_launch &&
        GBE_local_lobby.state == 2u &&
        GBE_local_lobby.game_state == 2u &&
        request_shape.has_game_state && request_shape.game_state == 2u &&
        request_shape.has_send_reason && request_shape.send_reason == 2u &&
        GBE_local_lobby.game_mode == 1u) {
        uint32 remote_count = 0u;
        uint32 connected_remote_count = 0u;
        if (GBE_ShouldHoldDotaLanLaunchForRemoteMembers(3u, &remote_count, &connected_remote_count)) {
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "holding LAN AP fallback strategy_time req=%u source_job=%llu remote_connected=%u remote_total=%u state=%u game_state=%u summary=%s",
                request_emsg,
                static_cast<unsigned long long>(source_job),
                connected_remote_count,
                remote_count,
                GBE_local_lobby.state,
                GBE_local_lobby.game_state,
                gbe::proto_wire::format_dota7034_summary(body, body_size).c_str()
            );
        } else if (GBE_TryQueueDotaRuntimeLobbyDetailsUpdate("runtime AP hero_selection fallback strategy_time", request_emsg, source_job, 2u, 3u, 1.0)) {
            queued_runtime_lobby_update = true;
        }
    }
}


void Steam_Game_Coordinator::GBE_HandleDotaDirect7034StrategyTimePreserve(
    uint32 request_emsg,
    uint64 source_job,
    bool &queued_runtime_lobby_update)
{
    if (GBE_local_lobby.state == 2u && GBE_local_lobby.game_state == 3u) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "consumed req=%u source_job=%llu note=skip synthetic official 032 follow-up and preserve strategy_time state=%u game_state=%u",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state
        );
        queued_runtime_lobby_update = true;
    }
}


bool Steam_Game_Coordinator::GBE_HandleDotaDirect7034Response(
    uint32 request_emsg,
    const GBE_Dota7034RequestShape &request_shape,
    const uint8 *body,
    size_t body_size,
    bool has_source_job,
    uint64 source_job)
{
    // [FIX] Re-push host equipped items when game_state reaches TEAM_SHOWCASE (4).
    // On a listen server the login CacheSubscribed establishes the host's SO cache
    // with 27k items (no equipped_state) in the shared cache. The equip-forward
    // CacheSubscribed arrives during STRATEGY_TIME before hero spawn, so the server
    // engine sees [in cache] and does not create wearables. By re-pushing at
    // TEAM_SHOWCASE (when the server engine is about to spawn heroes), we give it
    // a fresh CacheSubscribed with only equipped items so wearables are created.
    if (is_server && !GBE_dota_host_showcase_equip_pushed &&
        request_shape.has_game_state && request_shape.game_state >= 4u &&
        GBE_local_lobby.active && GBE_local_lobby.state == 2u) {
        Steam_Client *steam_client_ptr = get_steam_client();
        Steam_Game_Coordinator *client_gc_ptr = steam_client_ptr ? steam_client_ptr->steam_game_coordinator : nullptr;
        const uint64 owner_steam64 = GBE_GetDotaLobbyOwnerSteamId();
        if (client_gc_ptr && owner_steam64 != 0ull) {
            const CSteamID owner_steam_id(owner_steam64);
            const auto &client_items = client_gc_ptr->get_items();
            if (GBE_PushDotaPlayerEquippedItemsCacheToGC(this, owner_steam_id, client_items, true, "7034_showcase_host_equip_repush")) {
                GBE_dota_host_showcase_equip_pushed = true;
                GBE_GC_DebugLog(
                    "GC_DOTA_DIRECT",
                    "re-pushed host equipped items at TEAM_SHOWCASE: steam64=%llu request_game_state=%u lobby_game_state=%u",
                    static_cast<unsigned long long>(owner_steam64),
                    request_shape.game_state,
                    GBE_local_lobby.game_state
                );
            }
        }
    }

    std::string response_message;
    const bool built_response = GBE_AdaptDota7034ConnectedPlayersResponsePayload(
        GBE_GetDotaLobbyOwnerSteamId(),
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot,
        GBE_local_lobby.members,
        request_shape,
        true,
        true,
        has_source_job,
        source_job,
        response_message);
    if (!built_response) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, 7034u);
        return true;
    }

    size_t response_body_offset = 8u;
    if (response_message.size() >= 8u) {
        uint32 response_header_length = 0;
        std::memcpy(&response_header_length, response_message.data() + 4, sizeof(response_header_length));
        response_body_offset += response_header_length;
    }
    const uint8 *response_body = response_body_offset <= response_message.size()
        ? reinterpret_cast<const uint8 *>(response_message.data() + response_body_offset)
        : nullptr;
    const size_t response_body_size = response_body_offset <= response_message.size()
        ? (response_message.size() - response_body_offset)
        : 0u;

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=%u resp=%u source_job=%llu size=%zu note=7034 connected players game_state=%u team=%u slot=%u summary=%s",
        request_emsg,
        7034u,
        static_cast<unsigned long long>(source_job),
        response_message.size(),
        GBE_local_lobby.game_state,
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot,
        gbe::proto_wire::format_dota7034_summary(response_body, response_body_size).c_str()
    );
    push_incoming_now(7034u | GBE_kProtoMask, response_message);
    return true;
}


void Steam_Game_Coordinator::GBE_HandleDotaDirect7034LaunchPoll(
    uint32 request_emsg,
    uint64 source_job,
    bool &queued_runtime_lobby_update)
{
    if (GBE_local_lobby.state == 2u && GBE_local_lobby.game_state == 10u)
        return;

    if (GBE_SendDotaPracticeLobbyDetailsUpdate(false, nullptr, "7034_launch_poll")) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu note=7034 direct poll uses runtime 26 fallback state=%u game_state=%u",
            request_emsg,
            GBE_kDotaPracticeLobbyDetailsUpdate,
            static_cast<unsigned long long>(source_job),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state
        );
        queued_runtime_lobby_update = true;
    }
}


void Steam_Game_Coordinator::GBE_HandleDotaDirect7034WaitForPlayers(
    uint32 request_emsg,
    const uint8 *body,
    size_t body_size,
    bool custom_game_launch,
    bool request_advances_to_hero_selection,
    uint64 source_job,
    bool &queued_runtime_lobby_update)
{
    if (custom_game_launch || GBE_local_lobby.state != 2u || GBE_local_lobby.game_state != 1u)
        return;

    if (!GBE_TryQueueDotaRuntimeLobbyDetailsUpdate("runtime packet after 8870/7034 wait_for_players", request_emsg, source_job, 2u, 1u))
        return;

    queued_runtime_lobby_update = true;
    if (!request_advances_to_hero_selection)
        return;

    uint32 remote_count = 0u;
    uint32 connected_remote_count = 0u;
    if (GBE_ShouldHoldDotaLanLaunchForRemoteMembers(2u, &remote_count, &connected_remote_count)) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "holding LAN hero_selection req=%u source_job=%llu remote_connected=%u remote_total=%u state=%u game_state=%u summary=%s",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            connected_remote_count,
            remote_count,
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            gbe::proto_wire::format_dota7034_summary(body, body_size).c_str()
        );
    } else if (GBE_TryQueueDotaRuntimeLobbyDetailsUpdate("runtime packet after 8870/7034 hero_selection", request_emsg, source_job, 2u, 2u)) {
        queued_runtime_lobby_update = true;
    }
}


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


bool Steam_Game_Coordinator::GBE_HandleDotaMinimalVarintSuccessRequest(uint32 request_emsg, uint32 response_emsg, const char *log_note, const char *push_note, bool has_source_job, uint64 source_job)
{
    std::string response_message;
    if (!gbe::gc_message::build_dota_varint_response_payload(response_emsg, 1u, 1u, has_source_job, source_job, response_message)) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, response_emsg);
        return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=%u resp=%u source_job=%llu size=%zu note=%s",
        request_emsg,
        response_emsg,
        static_cast<unsigned long long>(source_job),
        response_message.size(),
        log_note
    );
    GBE_PushDotaResponse(response_emsg, response_message, false, nullptr, push_note);
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDota7427NotificationsRequest(bool has_source_job, uint64 source_job)
{
    std::string response_message;
    if (!gbe::gc_message::build_dota_7428_response_payload(has_source_job, source_job, response_message)) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", 7427u, 7428u);
        return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=%u resp=%u source_job=%llu size=%zu note=%s",
        7427u,
        7428u,
        static_cast<unsigned long long>(source_job),
        response_message.size(),
        "7427->7428 minimal notifications response"
    );
    GBE_PushDotaResponse(7428u, response_message, false, nullptr, "7427_7428");
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaUploadRateRequest(bool has_source_job, uint64 source_job)
{
    std::string response_message;
    if (!gbe::gc_message::build_dota_4524_response_payload(has_source_job, source_job, response_message)) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", 4523u, 4524u);
        return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=%u resp=%u source_job=%llu size=%zu note=%s",
        4523u,
        4524u,
        static_cast<unsigned long long>(source_job),
        response_message.size(),
        "4523->4524 minimal upload_rate_modifier=1.0"
    );
    GBE_PushDotaResponse(4524u, response_message, false, nullptr, "4523_4524");
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaProfileCardRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job)
{
    uint64 account_id_field = settings->get_local_steam_id().GetAccountID();
    gbe::proto_wire::read_uint64_field(body, body_size, 1u, account_id_field);

    std::string response_message;
    if (!gbe::gc_message::build_dota_varint_response_payload(7535u, 1u, static_cast<uint32>(account_id_field), has_source_job, source_job, response_message)) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", 7534u, 7535u);
        return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=%u resp=%u source_job=%llu size=%zu note=7534->7535 minimal profile card account_id=%u",
        7534u,
        7535u,
        static_cast<unsigned long long>(source_job),
        response_message.size(),
        static_cast<unsigned>(account_id_field)
    );
    GBE_PushDotaResponse(7535u, response_message, false, nullptr, "7534_7535");
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaLookupAccountNameRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job)
{
    uint64 account_id_field = settings->get_local_steam_id().GetAccountID();
    gbe::proto_wire::read_uint64_field(body, body_size, 1u, account_id_field);

    std::string response_message;
    if (!gbe::gc_message::build_dota_2582_lookup_account_name_response_payload(
            static_cast<uint32>(account_id_field),
            std::string(settings->get_local_name()),
            has_source_job,
            source_job,
            response_message)) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", 2581u, 2582u);
        return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=%u resp=%u source_job=%llu size=%zu note=2581->2582 lookup account name account_id=%u",
        2581u,
        2582u,
        static_cast<unsigned long long>(source_job),
        response_message.size(),
        static_cast<unsigned>(account_id_field)
    );
    GBE_PushDotaResponse(2582u, response_message, false, nullptr, "2581_2582");
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaEmoticonDataRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job)
{
    const GBE_DotaEmptyRequestShape request_shape = gbe::proto_wire::parse_dota_empty_request_shape(body, body_size);
    const uint32 account_id = settings->get_local_steam_id().GetAccountID();
    std::string response_message;
    if (!gbe::gc_message::build_dota_7504_response_payload(account_id, has_source_job, source_job, response_message)) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", 7503u, 7504u);
        return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=%u resp=%u source_job=%llu size=%zu note=7503->7504 parsed valid=%u fields=%u emoticon data account_id=%u",
        7503u,
        7504u,
        static_cast<unsigned long long>(source_job),
        response_message.size(),
        request_shape.valid ? 1u : 0u,
        request_shape.field_count,
        account_id
    );
    GBE_PushDotaResponse(7504u, response_message, false, nullptr, "7503_7504");
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaConductScorecardRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job)
{
    const GBE_DotaEmptyRequestShape request_shape = gbe::proto_wire::parse_dota_empty_request_shape(body, body_size);
    const uint32 account_id = settings->get_local_steam_id().GetAccountID();
    std::string response_message;
    if (!gbe::gc_message::build_dota_8096_response_payload(account_id, has_source_job, source_job, response_message)) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", 8095u, 8096u);
        return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=%u resp=%u source_job=%llu size=%zu note=8095->8096 parsed valid=%u fields=%u conduct scorecard account_id=%u",
        8095u,
        8096u,
        static_cast<unsigned long long>(source_job),
        response_message.size(),
        request_shape.valid ? 1u : 0u,
        request_shape.field_count,
        account_id
    );
    GBE_PushDotaResponse(8096u, response_message, false, nullptr, "8095_8096");
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaCoachingSummaryRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job)
{
    const GBE_DotaEmptyRequestShape request_shape = gbe::proto_wire::parse_dota_empty_request_shape(body, body_size);
    std::string response_message;
    if (!gbe::gc_message::build_dota_varint_response_payload(8801u, 1u, 1u, has_source_job, source_job, response_message)) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", 8800u, 8801u);
        return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=%u resp=%u source_job=%llu size=%zu note=8800->8801 parsed valid=%u fields=%u coaching summary success",
        8800u,
        8801u,
        static_cast<unsigned long long>(source_job),
        response_message.size(),
        request_shape.valid ? 1u : 0u,
        request_shape.field_count
    );
    GBE_PushDotaResponse(8801u, response_message, false, nullptr, "8800_8801");
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaRankRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job)
{
    const GBE_DotaRankRequestShape request_shape = gbe::proto_wire::parse_dota_rank_request_shape(body, body_size);
    std::string response_message;
    if (!gbe::gc_message::build_dota_8880_response_payload(
            request_shape.valid,
            request_shape.has_rank_type,
            gbe::proto_wire::dota_is_rank_type_supported(request_shape.rank_type),
            has_source_job,
            source_job,
            response_message)) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", 8879u, 8880u);
        return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=%u resp=%u source_job=%llu size=%zu note=8879->8880 parsed valid=%u fields=%u has_rank_type=%u rank_type=%u",
        8879u,
        8880u,
        static_cast<unsigned long long>(source_job),
        response_message.size(),
        request_shape.valid ? 1u : 0u,
        request_shape.field_count,
        request_shape.has_rank_type ? 1u : 0u,
        request_shape.rank_type
    );
    GBE_PushDotaResponse(8880u, response_message, false, nullptr, "8879_8880");
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaLaunchAdvanceOrConsume(
    uint32 request_emsg,
    const char *advance_reason,
    const char *advance_phase,
    const char *consume_note,
    uint64 source_job,
    size_t body_size)
{
    // If we are stuck at state=1 (SERVERSETUP) because 4508 never arrived
    // (dedicated server did not restart between matches), use 4506/5429 as the
    // signal to advance the launch to RUN.  This is safe because:
    //  - If 4508 already advanced us to state=2, the state==1 check fails.
    //  - 4506 is "server available acknowledgement" / 5429 is "ticket auth
    //    complete", so the server IS ready.
    if (GBE_local_lobby.state == 1u && GBE_local_lobby.game_state == 0u && GBE_HasDotaLaunchServerSetupSync()) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "req=%u advancing stalled launch: lobby_id=%llu state=%u launch_phase=%s",
            request_emsg,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase)
        );
        if (GBE_TryAdvanceDotaLaunchToRun(advance_reason, request_emsg, source_job, advance_phase))
            return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "consumed req=%u source_job=%llu note=%s body_size=%zu active=%u lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu",
        request_emsg,
        static_cast<unsigned long long>(source_job),
        consume_note,
        body_size,
        GBE_local_lobby.active ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id)
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDota8870LaunchMarkerRequest(uint32 request_emsg, uint64 source_job)
{
    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "consumed req=%u source_job=%llu note=official 8870 launch marker without pending gate active=%u lobby_id=%llu state=%u game_state=%u",
        request_emsg,
        static_cast<unsigned long long>(source_job),
        GBE_local_lobby.active ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        GBE_local_lobby.state,
        GBE_local_lobby.game_state
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaLanServerAvailableRequest(uint32 request_emsg, const uint8 *body, size_t body_size, uint64 source_job)
{
    uint64 lobby_id = 0;
    gbe::proto_wire::read_uint64_field(body, body_size, 1u, lobby_id);

    const bool matches_local_lobby = (lobby_id != 0 && lobby_id == GBE_local_lobby.lobby_id);
    if (matches_local_lobby) {
        if (!GBE_local_lobby.launch_4511_seen) {
            GBE_local_lobby.launch_4511_seen = true;
            GBE_PublishSharedDotaLobbyState("4511_lan_server_available_seen");
        }
        GBE_TrySyncDotaLobbyServerIdFromGameServer("4511_lan_server_available");
    }

    if (matches_local_lobby && !incoming_messages.empty()) {
        GCMessageAvailable_t data{};
        data.m_nMessageSize = static_cast<uint32>(incoming_messages.front().msg_body.size());
        callbacks->addCBResult(data.k_iCallback, &data, sizeof(data), 0.0);
        GBE_GC_DebugLog(
            "GC_CALLBACK",
            "reposted GCMessageAvailable_t after 4511 lobby_id=%llu queued_emsg=%u queue_size=%zu size=%u",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_GC_MaskedEMsg(incoming_messages.front().msg_type),
            incoming_messages.size(),
            data.m_nMessageSize
        );
    }

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "consumed req=%u source_job=%llu note=lan server available notification without launch gating lobby_id=%llu local_lobby_id=%llu matches_local=%u",
        request_emsg,
        static_cast<unsigned long long>(source_job),
        static_cast<unsigned long long>(lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        matches_local_lobby ? 1u : 0u
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaBatchPlayerResourcesRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job)
{
    std::vector<uint32> account_ids;
    gbe::proto_wire::Field account_ids_field{};
    GBE_ProtoField account_ids_view{};
    if (gbe::proto_wire::find_field(body, body_size, 1u, account_ids_field))
        account_ids_view = GBE_ProtoField{
            true,
            account_ids_field.number,
            account_ids_field.wire_type,
            account_ids_field.value_offset,
            account_ids_field.value_size
        };
    if (!gbe::proto_wire::extract_packed_uint32_field(body, body_size, account_ids_view.as_proto_wire_field(), account_ids) || account_ids.empty())
        account_ids.push_back(settings->get_local_steam_id().GetAccountID());

    std::string response_message;
    const std::vector<std::uint32_t> resource_account_ids(account_ids.begin(), account_ids.end());
    if (!gbe::gc_message::build_dota_7451_batch_player_resources_response_payload(resource_account_ids, has_source_job, source_job, response_message)) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", 7450u, 7451u);
        return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=%u resp=%u source_job=%llu size=%zu note=7450->7451 minimal batch player resources accounts=%zu",
        7450u,
        7451u,
        static_cast<unsigned long long>(source_job),
        response_message.size(),
        account_ids.size()
    );
    GBE_PushDotaResponse(7451u, response_message, false, nullptr, "7450_7451");

    // After 7451, send per-player item CacheSubscribed so the dedicated
    // server knows each player's equipped cosmetics (loadout).
    // In Valve's system the GC pushes a CMsgSOCacheSubscribed (owner type=1)
    // containing each player's CSOEconItem list.  GBE's server GC does not
    // have this data, so we read it from the client GC (same process) for the
    // local player, and from all_user_items for remote players (populated via
    // network inventory exchange).
    if (is_server && gc_profile == GC_PROFILE_DOTA2) {
        Steam_Client *steam_client = get_steam_client();
        Steam_Game_Coordinator *client_gc = steam_client ? steam_client->steam_game_coordinator : nullptr;

        for (uint32 target_account_id : account_ids) {
            const uint64 player_steam64 = static_cast<uint64>(target_account_id) + 76561197960265728ull;
            const CSteamID player_steam_id(player_steam64);

            // Determine which item source to use for this player.
            // [FIX] On a listen server the server GC's
            // settings->get_local_steam_id() returns the game-server
            // steam ID (90071999...), NOT the lobby owner's personal
            // steam ID.  So we also check against the lobby owner's
            // steam ID to correctly identify the host player and read
            // their items from the client GC.
            std::vector<const Econ_Item *> equipped_items;

            const uint64 local_steam64 = settings->get_local_steam_id().ConvertToUint64();
            const uint64 owner_steam64 = GBE_GetDotaLobbyOwnerSteamId();
            const bool is_host_player = (player_steam64 == local_steam64 || player_steam64 == owner_steam64);

            if (is_host_player && client_gc) {
                // Host player: read from client GC (same process)
                const auto &client_items = client_gc->get_items();
                for (const auto &item : client_items) {
                    if (!item.equip_states.empty())
                        equipped_items.push_back(&item);
                }
            } else if (all_user_items.count(player_steam64)) {
                // Remote player: read from all_user_items (received via network)
                const auto &remote_items = all_user_items.at(player_steam64);
                for (const auto &item : remote_items) {
                    if (!item.equip_states.empty())
                        equipped_items.push_back(&item);
                }
            }

            if (equipped_items.empty()) {
                GBE_GC_DebugLog(
                    "GC_DOTA_DIRECT",
                    "no equipped items for player at 7450 time: account_id=%u steam64=%llu is_local=%d is_host=%d has_remote_data=%d",
                    target_account_id,
                    static_cast<unsigned long long>(player_steam64),
                    (player_steam64 == local_steam64) ? 1 : 0,
                    is_host_player ? 1 : 0,
                    all_user_items.count(player_steam64) ? 1 : 0
                );
                continue;
            }

            // Build a CMsgSOCacheSubscribed with owner type=1 (player)
            // containing type_id=1 (CSOEconItem) objects.
            std::string owner_soid;
            gbe::proto_wire::append_varint_field(owner_soid, 1u, 1u);
            gbe::proto_wire::append_varint_field(owner_soid, 2u, player_steam64);

            std::string subscribed_type;
            gbe::proto_wire::append_varint_field(subscribed_type, 1u, 1u);
            for (const Econ_Item *item_ptr : equipped_items) {
                const std::string serialized = client_gc ?
                    client_gc->serialize_item_to_gcprotobuf(*item_ptr, player_steam_id) :
                    serialize_item_to_gcprotobuf(*item_ptr, player_steam_id);
                gbe::proto_wire::append_bytes_field(subscribed_type, 2u, serialized);
            }

            std::string cache_body;
            gbe::proto_wire::append_bytes_field(cache_body, 2u, subscribed_type);
            gbe::proto_wire::append_fixed64_field(cache_body, 3u, 1ull);
            gbe::proto_wire::append_bytes_field(cache_body, 4u, owner_soid);

            std::string cache_message;
            gbe::gc_message::build_dota_zero_header_payload(GBE_kDotaCacheSubscribed, cache_body, cache_message);
            push_incoming_now(GBE_kDotaCacheSubscribed | GBE_kProtoMask, cache_message);

            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "pushed player item CacheSubscribed for server: account_id=%u steam64=%llu equipped_items=%zu message_size=%zu",
                target_account_id,
                static_cast<unsigned long long>(player_steam64),
                equipped_items.size(),
                cache_message.size()
            );
        }
    }

    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaCacheSubscriptionRefreshRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job)
{
    uint64 requested_owner_type = 0;
    uint64 requested_owner_id = 0;
    std::string owner_soid;
    if (gbe::proto_wire::read_bytes_field(body, body_size, 2u, owner_soid)) {
        gbe::proto_wire::read_uint64_field(reinterpret_cast<const uint8 *>(owner_soid.data()), owner_soid.size(), 1u, requested_owner_type);
        gbe::proto_wire::read_uint64_field(reinterpret_cast<const uint8 *>(owner_soid.data()), owner_soid.size(), 2u, requested_owner_id);
    }

    const bool matches_lobby_owner =
        GBE_local_lobby.active &&
        requested_owner_type == 3u &&
        requested_owner_id != 0 &&
        requested_owner_id == GBE_local_lobby.lobby_id;

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "observed req=%u source_job=%llu note=cache subscription refresh owner_type=%llu owner_id=%llu active=%u lobby_id=%llu state=%u game_state=%u body_prefix=%s",
        GBE_kDotaCacheSubscriptionRefresh,
        static_cast<unsigned long long>(source_job),
        static_cast<unsigned long long>(requested_owner_type),
        static_cast<unsigned long long>(requested_owner_id),
        GBE_local_lobby.active ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(body), body_size, 32).c_str()
    );

    if (matches_lobby_owner) {
        std::string response_message;
        if (!gbe::gc_message::build_dota_lobby_cache_subscribed_up_to_date_payload(
                GBE_local_lobby.lobby_id,
                GBE_local_lobby.has_cache_version,
                GBE_local_lobby.cache_version,
                GBE_local_lobby.has_cache_service_id,
                GBE_local_lobby.cache_service_id,
                GBE_local_lobby.cache_service_list,
                GBE_local_lobby.has_cache_sync_version,
                GBE_local_lobby.cache_sync_version,
                response_message)) {
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "failed building reply req=%u resp=%u lobby_id=%llu",
                GBE_kDotaCacheSubscriptionRefresh,
                GBE_kDotaCacheSubscribedUpToDate,
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id)
            );
            return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu size=%zu note=cache subscription refresh acknowledged owner_type=%llu owner_id=%llu version_present=%u service_id_present=%u service_list_count=%zu sync_version_present=%u",
            GBE_kDotaCacheSubscriptionRefresh,
            GBE_kDotaCacheSubscribedUpToDate,
            static_cast<unsigned long long>(source_job),
            response_message.size(),
            static_cast<unsigned long long>(requested_owner_type),
            static_cast<unsigned long long>(requested_owner_id),
            GBE_local_lobby.has_cache_version ? 1u : 0u,
            GBE_local_lobby.has_cache_service_id ? 1u : 0u,
            GBE_local_lobby.cache_service_list.size(),
            GBE_local_lobby.has_cache_sync_version ? 1u : 0u
        );
        GBE_PushDotaResponse(GBE_kDotaCacheSubscribedUpToDate, response_message, false, nullptr, "cache_subscribed_up_to_date_refresh");
    }

    (void)has_source_job;
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaLeaverDetectedRequest(const uint8 *body, size_t body_size, uint64 source_job)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "ignoring 7072 because no local lobby is active");
        return true;
    }

    // Parse steam_id (field 1, fixed64) and leaver_status (field 2, varint)
    uint64 leaver_steam_id = 0ull;
    uint32 leaver_status = 0u;
    uint32 disconnect_reason = 0u;
    gbe::proto_wire::read_uint64_field(body, body_size, 1u, leaver_steam_id);
    gbe::proto_wire::read_uint32_field(body, body_size, 2u, leaver_status);
    gbe::proto_wire::read_uint32_field(body, body_size, 6u, disconnect_reason);

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "handling req=7072 LeaverDetected steam_id=%llu leaver_status=%u disconnect_reason=%u lobby_id=%llu state=%u game_state=%u",
        static_cast<unsigned long long>(leaver_steam_id),
        leaver_status,
        disconnect_reason,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        GBE_local_lobby.state,
        GBE_local_lobby.game_state
    );

    if (leaver_steam_id != 0ull && leaver_status != 0u) {
        bool updated = false;
        for (GBE_DotaLobbyMemberState &member : GBE_local_lobby.members) {
            if (member.steam_id == leaver_steam_id) {
                if (member.leaver_status != leaver_status) {
                    member.leaver_status = leaver_status;
                    member.connected = false;
                    updated = true;
                    GBE_GC_DebugLog(
                        "GC_DOTA_DIRECT",
                        "updated member leaver_status steam_id=%llu leaver_status=%u",
                        static_cast<unsigned long long>(leaver_steam_id),
                        leaver_status
                    );
                }
                break;
            }
        }

        if (updated) {
            GBE_PublishSharedDotaLobbyState("7072_leaver_detected");
            GBE_SendDotaPracticeLobbyDetailsUpdate(false, nullptr, "7072_leaver_detected");
        }
    }

    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaSignOutPermissionRequest(bool has_source_job, uint64 source_job)
{
    std::string response_message;
    if (!gbe::gc_message::build_dota_varint_response_payload(GBE_kDotaGameMatchSignOutPermissionResponse, 1u, 1u, has_source_job, source_job, response_message)) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", GBE_kDotaGameMatchSignOutPermissionRequest, GBE_kDotaGameMatchSignOutPermissionResponse);
        return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=%u resp=%u source_job=%llu size=%zu note=signout permission granted",
        GBE_kDotaGameMatchSignOutPermissionRequest,
        GBE_kDotaGameMatchSignOutPermissionResponse,
        static_cast<unsigned long long>(source_job),
        response_message.size()
    );
    GBE_PushDotaResponse(GBE_kDotaGameMatchSignOutPermissionResponse, response_message, false, nullptr, "signout_permission");
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaSubmitPlayerReportV2Request(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job)
{
    std::string response_message;
    if (!gbe::gc_message::build_dota_submit_player_report_response_v2_payload(body, body_size, has_source_job, source_job, response_message)) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", GBE_kDotaSubmitPlayerReportV2, GBE_kDotaSubmitPlayerReportResponseV2);
        return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=%u resp=%u source_job=%llu size=%zu note=submit player report v2 success",
        GBE_kDotaSubmitPlayerReportV2,
        GBE_kDotaSubmitPlayerReportResponseV2,
        static_cast<unsigned long long>(source_job),
        response_message.size()
    );
    GBE_PushDotaResponse(GBE_kDotaSubmitPlayerReportResponseV2, response_message, false, nullptr, "submit_player_report_v2");
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaTemplateReplayRequest(uint32 request_emsg, const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job) {
    const uint8 *template_bytes = nullptr;
    size_t template_size = 0;
    const char *template_hex = nullptr;
    uint32 response_emsg = 0;
    bool replace_account = false;
    bool replace_steam_id = false;
    const char *response_note = "";

    switch (request_emsg) {
        case 2536:
            template_bytes = GBE_kDota2538Template;
            template_size = sizeof(GBE_kDota2538Template);
            response_emsg = 2538;
            response_note = "2536->2538";
            break;
        case 2617:
            template_bytes = GBE_kDota2618Template;
            template_size = sizeof(GBE_kDota2618Template);
            response_emsg = 2618;
            response_note = "2617->2618";
            break;
        case 8137:
            template_bytes = GBE_kDota8136Template;
            template_size = sizeof(GBE_kDota8136Template);
            response_emsg = 8136;
            response_note = "8137->8136";
            break;
        case 8673:
            template_bytes = GBE_kDota8674Template;
            template_size = sizeof(GBE_kDota8674Template);
            response_emsg = 8674;
            response_note = "8673->8674";
            break;
        case 7197:
            template_bytes = GBE_kDota7198Template;
            template_size = sizeof(GBE_kDota7198Template);
            response_emsg = 7198;
            response_note = "7197->7198";
            break;
        case 8729:
            template_hex = GBE_kDota8730TemplateHex;
            response_emsg = 8730;
            response_note = "8729->8730";
            break;
        case 8744:
            template_hex = GBE_kDotaOfficial8745TemplateHex;
            response_emsg = 8745;
            response_note = "8744->8745";
            break;
        case 8330:
            template_hex = GBE_kDota8331TemplateHex;
            response_emsg = 8331;
            response_note = "8330->8331";
            break;
        case 8676:
            template_bytes = GBE_kDota8677Template;
            template_size = sizeof(GBE_kDota8677Template);
            response_emsg = 8677;
            response_note = "8676->8677 + 8678 update";
            break;
        case 7387: {
            uint64 profile_selector = 0;
            if (!gbe::proto_wire::read_uint64_field(body, body_size, 1u, profile_selector)) {
                GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed reading 7387 selector body_size=%zu", body_size);
                return true;
            }

            uint64 account_id_field = settings->get_local_steam_id().GetAccountID();
            gbe::proto_wire::read_uint64_field(body, body_size, 2u, account_id_field);

            // Always use minimal response with owned=true for ALL event selectors.
            // Previous code had hardcoded templates for 0x20 (owned=0) and 0x37 (owned=1)
            // which caused items bound to event 0x20 to show "Unavailable".
            {
                std::string response_message;
                if (!gbe::gc_message::build_dota_7388_minimal_response_payload(
                        static_cast<uint32>(profile_selector),
                        static_cast<uint32>(account_id_field),
                        has_source_job,
                        source_job,
                        response_message)) {
                    GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building minimal 7388 selector=%llu", static_cast<unsigned long long>(profile_selector));
                    return true;
                }

                GBE_GC_DebugLog(
                    "GC_DOTA_DIRECT",
                    "replying req=%u resp=%u source_job=%llu size=%zu note=7387 selector=%llu owned=true level=1000 account_id=%u",
                    request_emsg,
                    7388u,
                    static_cast<unsigned long long>(source_job),
                    response_message.size(),
                    static_cast<unsigned long long>(profile_selector),
                    static_cast<unsigned>(account_id_field)
                );
                GBE_PushDotaResponse(7388u, response_message, false, nullptr, "7387_7388");
                return true;
            }
        }
        case 8078:
            template_bytes = GBE_kDota8079Template;
            template_size = sizeof(GBE_kDota8079Template);
            response_emsg = 8079;
            response_note = "8078->8079";
            break;
        case 8853:
            template_bytes = GBE_kDota8854Template;
            template_size = sizeof(GBE_kDota8854Template);
            response_emsg = 8854;
            response_note = "8853->8854";
            break;
        case 9023:
            template_bytes = GBE_kDota9024Template;
            template_size = sizeof(GBE_kDota9024Template);
            response_emsg = 9024;
            response_note = "9023->9024";
            break;
        case 8218: {
            // CMsgClientToGCGiveTip -> CMsgClientToGCGiveTipResponse
            // Return result=0 (success) so tipping works in-game.
            std::string tip_response;
            gbe::gc_message::build_dota_give_tip_response_payload(has_source_job, source_job, tip_response);
            GBE_PushDotaResponse(8219u, tip_response, false, nullptr, "8218_8219");

            GBE_GC_DebugLog("GC_DOTA_DIRECT", "tip request -> success response source_job=%llu", static_cast<unsigned long long>(source_job));
            return true;
        }
        case 8879: {
            // CMsgClientToGCRankRequest -> CMsgGCToClientRankResponse
            // Return empty first, then with rank data (field 2=10000, field 3=10000)
            std::string rank_response;
            gbe::gc_message::build_dota_rank_request_response_payload(has_source_job, source_job, rank_response);
            GBE_PushDotaResponse(8880u, rank_response, false, nullptr, "8879_8880_switch");

            GBE_GC_DebugLog("GC_DOTA_DIRECT", "rank request -> response source_job=%llu", static_cast<unsigned long long>(source_job));
            return true;
        }
        case 8095: {
            // CMsgPlayerConductScorecardRequest -> suppress (don't reply)
            // Not replying prevents misleading conduct scorecard popup.
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "conduct scorecard request suppressed source_job=%llu", static_cast<unsigned long long>(source_job));
            return true;
        }
        case GBE_kDotaCustomGameInfoRequest: {
            uint64 custom_game_id = 0ull;
            gbe::proto_wire::read_uint64_field(body, body_size, 1u, custom_game_id);

            std::string response_message;
            if (!gbe::gc_message::build_dota_custom_game_info_response_payload(custom_game_id, has_source_job, source_job, response_message)) {
                GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, GBE_kDotaCustomGameInfoResponse);
                return true;
            }

            GBE_PushDotaResponse(GBE_kDotaCustomGameInfoResponse, response_message, false, nullptr, "custom_game_info");
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "custom game info -> response custom_game_id=%llu source_job=%llu",
                static_cast<unsigned long long>(custom_game_id),
                static_cast<unsigned long long>(source_job));
            return true;
        }
        case GBE_kDotaJoinableCustomGameModesRequest: {
            const std::vector<GBE_LocalLobby> snapshots = GBE_GetDotaGenericLobbySnapshots("7466_joinable_custom_modes");
            std::vector<gbe::gc_message::DotaJoinableCustomGameMode> modes;

            for (const GBE_LocalLobby &snapshot : snapshots) {
                if (!snapshot.active)
                    continue;
                modes.push_back(gbe::gc_message::DotaJoinableCustomGameMode{snapshot.custom_game.game_id, static_cast<uint32>(snapshot.members.size())});
            }
            if (GBE_local_lobby.active)
                modes.push_back(gbe::gc_message::DotaJoinableCustomGameMode{GBE_local_lobby.custom_game.game_id, static_cast<uint32>(GBE_local_lobby.members.size())});
            if (GBE_shared_dota_lobby_state.valid && GBE_shared_dota_lobby_state.active)
                modes.push_back(gbe::gc_message::DotaJoinableCustomGameMode{GBE_shared_dota_lobby_state.custom_game.game_id, static_cast<uint32>(GBE_shared_dota_lobby_state.members.size())});

            std::string response_body;
            const size_t mode_count = gbe::gc_message::build_dota_joinable_custom_game_modes_body(modes, response_body);

            std::string response_message;
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(
                GBE_kDotaJoinableCustomGameModesResponse,
                has_source_job,
                source_job,
                response_body,
                response_message);
            GBE_PushDotaResponse(GBE_kDotaJoinableCustomGameModesResponse, response_message, false, nullptr, "joinable_custom_game_modes");
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "joinable custom game modes -> response modes=%zu source_job=%llu",
                mode_count,
                static_cast<unsigned long long>(source_job));
            return true;
        }
        case GBE_kDotaJoinableCustomLobbiesRequest: {
            uint64 requested_custom_game_id = 0ull;
            gbe::proto_wire::read_uint64_field(body, body_size, 2u, requested_custom_game_id);

            std::vector<GBE_LocalLobby> lobbies = GBE_GetDotaGenericLobbySnapshots("7468_joinable_custom_lobbies");
            if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0ull)
                lobbies.push_back(GBE_local_lobby);

            std::vector<gbe::gc_message::DotaJoinableCustomLobby> joinable_lobbies;
            const uint32 now = static_cast<uint32>(std::time(nullptr));
            for (const GBE_LocalLobby &lobby : lobbies) {
                if (!lobby.active || lobby.lobby_id == 0ull || lobby.custom_game.game_id == 0ull)
                    continue;

                const std::string room_name = lobby.room_name.empty() ? std::string("Lobby") : lobby.room_name;
                const std::string display_name = GBE_DotaCustomGameDisplayName(settings, lobby.custom_game, room_name);
                gbe::gc_message::DotaJoinableCustomLobby parsed_lobby{};
                parsed_lobby.lobby_id = lobby.lobby_id;
                parsed_lobby.custom_game_id = lobby.custom_game.game_id;
                parsed_lobby.display_name = display_name;
                parsed_lobby.member_count = static_cast<uint32>(lobby.members.empty() ? 1u : lobby.members.size());
                parsed_lobby.owner_account_id = lobby.owner_account_id != 0u ? lobby.owner_account_id : settings->get_local_steam_id().GetAccountID();
                parsed_lobby.owner_name = lobby.owner_name.empty() ? std::string(settings->get_local_name()) : lobby.owner_name;
                parsed_lobby.map_name = lobby.custom_game.map_name;
                parsed_lobby.max_players = lobby.custom_game.max_players;
                parsed_lobby.server_region = lobby.server_region;
                parsed_lobby.has_pass_key = !lobby.pass_key.empty();
                parsed_lobby.lan_host_ping_location = lobby.lan_host_ping_location;
                parsed_lobby.created_time = now;
                parsed_lobby.custom_game_timestamp = lobby.custom_game.timestamp;
                parsed_lobby.custom_game_crc = lobby.custom_game.crc;
                parsed_lobby.min_players = lobby.custom_game.min_players;
                parsed_lobby.penalties = lobby.custom_game.penalties;
                joinable_lobbies.push_back(parsed_lobby);
            }

            std::string response_body;
            const size_t lobby_count = gbe::gc_message::build_dota_joinable_custom_lobbies_body(joinable_lobbies, requested_custom_game_id, response_body);

            std::string response_message;
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(
                GBE_kDotaJoinableCustomLobbiesResponse,
                has_source_job,
                source_job,
                response_body,
                response_message);
            GBE_PushDotaResponse(GBE_kDotaJoinableCustomLobbiesResponse, response_message, false, nullptr, "joinable_custom_lobbies");
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "joinable custom lobbies -> response requested_game_id=%llu lobbies=%zu source_job=%llu",
                static_cast<unsigned long long>(requested_custom_game_id),
                lobby_count,
                static_cast<unsigned long long>(source_job));
            return true;
        }
        case GBE_kDotaFindTopSourceTVGames: {
            // CMsgClientToGCFindTopSourceTVGames -> CMsgGCToClientFindTopSourceTVGamesResponse
            // Build response from local shared lobby state OR remote generic lobbies.
            gbe::gc_message::DotaSourceTVGame source_tv_game{};
            bool found_game = false;

            // First: check local shared lobby state (we are the host)
            if (!found_game &&
                GBE_shared_dota_lobby_state.valid &&
                GBE_shared_dota_lobby_state.active &&
                GBE_shared_dota_lobby_state.game_state >= 1u &&
                GBE_shared_dota_lobby_state.server_id != 0) {
                source_tv_game.start_time = GBE_shared_dota_lobby_state.game_start_time != 0
                    ? GBE_shared_dota_lobby_state.game_start_time
                    : static_cast<uint32>(std::time(nullptr) - 300);
                source_tv_game.server_id = GBE_shared_dota_lobby_state.server_id;
                source_tv_game.lobby_id = GBE_shared_dota_lobby_state.lobby_id;
                source_tv_game.game_time = GBE_shared_dota_lobby_state.game_start_time != 0
                    ? static_cast<uint32>(std::time(nullptr)) - static_cast<uint32>(GBE_shared_dota_lobby_state.game_start_time)
                    : 300u;
                source_tv_game.game_mode = GBE_shared_dota_lobby_state.game_mode;
                source_tv_game.match_id = GBE_shared_dota_lobby_state.match_id;
                for (const auto &member : GBE_shared_dota_lobby_state.members) {
                    if (member.account_id == 0) continue;
                    source_tv_game.players.push_back(gbe::gc_message::DotaSourceTVPlayer{member.account_id, member.hero_id, member.slot, member.team});
                }
                found_game = true;
            }

            // Second: check remote generic lobbies via matchmaking
            if (!found_game) {
                const std::vector<GBE_LocalLobby> snapshots = GBE_GetDotaGenericLobbySnapshots("8009_find_top_source_tv");
                for (const auto &snap : snapshots) {
                    if (snap.game_state >= 1u && snap.server_id != 0 && snap.allow_spectating) {
                        source_tv_game.start_time = snap.game_start_time != 0
                            ? snap.game_start_time
                            : static_cast<uint32>(std::time(nullptr) - 300);
                        source_tv_game.server_id = snap.server_id;
                        source_tv_game.lobby_id = snap.lobby_id;
                        source_tv_game.game_time = snap.game_start_time != 0
                            ? static_cast<uint32>(std::time(nullptr)) - static_cast<uint32>(snap.game_start_time)
                            : 300u;
                        source_tv_game.game_mode = snap.game_mode;
                        source_tv_game.match_id = snap.match_id;
                        for (const auto &member : snap.members) {
                            if (member.account_id == 0) continue;
                            source_tv_game.players.push_back(gbe::gc_message::DotaSourceTVPlayer{member.account_id, member.hero_id, member.slot, member.team});
                        }
                        found_game = true;
                        break; // Use first matching game
                    }
                }
            }

            std::string response_body;
            if (found_game)
                gbe::gc_message::build_dota_find_top_source_tv_games_body(&source_tv_game, response_body);
            else
                gbe::gc_message::build_dota_find_top_source_tv_games_empty_body(response_body);

            std::string response_message;
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(
                GBE_kDotaFindTopSourceTVGamesResponse,
                has_source_job, source_job,
                response_body, response_message);
            GBE_PushDotaResponse(GBE_kDotaFindTopSourceTVGamesResponse, response_message, false, nullptr, "find_top_source_tv_games");

            GBE_GC_DebugLog("GC_DOTA_DIRECT",
                "FindTopSourceTVGames -> response found=%d source_job=%llu local_valid=%d",
                found_game ? 1 : 0,
                static_cast<unsigned long long>(source_job),
                GBE_shared_dota_lobby_state.valid ? 1 : 0);
            return true;
        }
        case 7073: {
            // CMsgSpectateFriendGame -> CMsgSpectateFriendGameResponse
            // Sent when a player clicks "Watch Game" on a friend's profile card.
            // Parse target steam_id (field 1, fixed64), find the game server they're on,
            // and return server_steamid + SUCCESS so the client can follow up with 7091.
            //
            // CMsgSpectateFriendGameResponse proto:
            //   field 4: server_steamid (fixed64)
            //   field 5: watch_live_result (varint enum, 0=SUCCESS)
            uint64 spectate_target_steamid = 0;
            {
                size_t pos = 0;
                while (pos < body_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end))
                        break;
                    if (field.number == 1u && field.wire_type == 1u && field.value_size == 8) {
                        memcpy(&spectate_target_steamid, body + field.value_offset, 8);
                    }
                }
            }

            // Find the server_steamid from local shared state or remote lobbies
            uint64 spectate_server_steamid = 0;

            if (GBE_shared_dota_lobby_state.valid && GBE_shared_dota_lobby_state.server_id != 0) {
                spectate_server_steamid = GBE_shared_dota_lobby_state.server_id;
            } else {
                const std::vector<GBE_LocalLobby> snapshots = GBE_GetDotaGenericLobbySnapshots("7073_spectate_friend");
                for (const auto &snap : snapshots) {
                    if (snap.game_state >= 1u && snap.server_id != 0) {
                        spectate_server_steamid = snap.server_id;
                        break;
                    }
                }
            }

            if (spectate_server_steamid == 0)
                spectate_server_steamid = GBE_local_lobby.server_id;

            // Build 7074 CMsgSpectateFriendGameResponse
            // Official GC always sends job_id_target = 0xFFFFFFFFFFFFFFFF in the header,
            // even when the request has no source_job. The client requires this to process
            // the response and trigger the follow-up 7091 WatchGame request.
            // Body contains only field 4 (server_steamid); field 5 (watch_live_result)
            // is omitted when SUCCESS (default value 0 is not serialized by official GC).
            {
                std::string resp_body;
                gbe::gc_message::build_dota_spectate_friend_game_response_body(spectate_server_steamid, resp_body);
                std::string response_msg;
                uint64 reply_job = has_source_job ? source_job : 0xFFFFFFFFFFFFFFFFULL;
                gbe::gc_message::build_dota_job_reply_payload(7074u, reply_job, resp_body, response_msg);
                GBE_PushDotaResponse(7074u, response_msg, false, nullptr, "7073_7074");
            }

            GBE_GC_DebugLog("GC_DOTA_DIRECT",
                "spectate friend game -> SUCCESS target=%llu server=0x%llx source_job=%llu",
                static_cast<unsigned long long>(spectate_target_steamid),
                static_cast<unsigned long long>(spectate_server_steamid),
                static_cast<unsigned long long>(source_job));
            return true;
        }
        case 7091: {
            // CMsgWatchGame -> CMsgWatchGameResponse
            // Parse server_steamid from request (field 1, fixed64).
            // Return READY with SourceTV connection info for LAN spectating.
            uint64 watch_server_steamid = 0;
            {
                size_t pos = 0;
                while (pos < body_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end))
                        break;
                    if (field.number == 1u && field.wire_type == 1u && field.value_size == 8) {
                        memcpy(&watch_server_steamid, body + field.value_offset, 8);
                    }
                }
            }

            // Parse SourceTV address from lobby connect string (ip:port -> ip, port+5)
            uint32 source_tv_addr = 0;
            uint32 source_tv_port = 27020; // default SourceTV port
            uint64 tv_secret_code = 0;
            std::string connect_str;
            if (GBE_shared_dota_lobby_state.valid && !GBE_shared_dota_lobby_state.connect.empty()) {
                connect_str = GBE_shared_dota_lobby_state.connect;
            } else {
                // Fallback: find connect from remote generic lobbies
                const std::vector<GBE_LocalLobby> snapshots = GBE_GetDotaGenericLobbySnapshots("7091_watch_game");
                for (const auto &snap : snapshots) {
                    if (snap.game_state >= 1u && snap.server_id != 0 && !snap.connect.empty()) {
                        connect_str = snap.connect;
                        if (snap.tv_secret_code != 0)
                            tv_secret_code = snap.tv_secret_code;
                        if (snap.tv_port != 0)
                            source_tv_port = snap.tv_port;
                        break;
                    }
                }
            }
            // If we have local lobby tv_secret_code (host side), use it
            if (tv_secret_code == 0 && GBE_local_lobby.tv_secret_code != 0)
                tv_secret_code = GBE_local_lobby.tv_secret_code;
            if (GBE_local_lobby.tv_port != 0)
                source_tv_port = GBE_local_lobby.tv_port;
            if (!connect_str.empty()) {
                size_t colon = connect_str.find(':');
                std::string ip_str = (colon != std::string::npos) ? connect_str.substr(0, colon) : connect_str;
                if (colon != std::string::npos) {
                    uint32 game_port = static_cast<uint32>(std::strtoul(connect_str.c_str() + colon + 1, nullptr, 10));
                    if (game_port > 0) source_tv_port = game_port + 5;
                }
                // Convert IP string to uint32 (network byte order as stored by Dota)
                unsigned int a = 0, b = 0, c = 0, d = 0;
                if (std::sscanf(ip_str.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
                    source_tv_addr = (a << 24) | (b << 16) | (c << 8) | d;
                }
            }

            // First response: PENDING (field 1 = 0)
            // Official GC sends job_id_target = 0xFFFFFFFFFFFFFFFF in the header.
            {
                std::string pending_body;
                gbe::gc_message::build_dota_watch_game_pending_response_body(pending_body);
                std::string pending_msg;
                uint64 pending_reply_job = has_source_job ? source_job : 0xFFFFFFFFFFFFFFFFULL;
                gbe::gc_message::build_dota_job_reply_payload(7092u, pending_reply_job, pending_body, pending_msg);
                push_incoming_now(7092u | GBE_kProtoMask, pending_msg);
            }

            const CSteamID local_steam_id = settings->get_local_steam_id();
            const uint32 local_account_id = local_steam_id.GetAccountID();

            // Second response: READY with server info
            {
                // Use real tv_secret_code from host's 4508 if available, otherwise fallback
                uint64 secret_code = (tv_secret_code != 0) ? tv_secret_code : (watch_server_steamid ^ 0x0514D449EDC24001ULL);
                std::string ready_body;
                gbe::gc_message::build_dota_watch_game_ready_response_body(source_tv_addr, source_tv_port, watch_server_steamid, secret_code, ready_body);
                std::string ready_msg;
                gbe::gc_message::build_dota_job_reply_or_zero_header_payload(7092u, false, 0, ready_body, ready_msg);
                push_incoming_now(7092u | GBE_kProtoMask, ready_msg);
            }

            GBE_GC_DebugLog("GC_DOTA_DIRECT", "watch game request -> READY server=0x%llx tv_addr=0x%x tv_port=%u local_account=%u local_steamid=%llu raw_tv_secret=0x%llx sent_secret=0x%llx source_job=%llu",
                static_cast<unsigned long long>(watch_server_steamid),
                source_tv_addr, source_tv_port,
                local_account_id,
                static_cast<unsigned long long>(local_steam_id.ConvertToUint64()),
                static_cast<unsigned long long>(tv_secret_code),
                static_cast<unsigned long long>(tv_secret_code != 0 ? tv_secret_code : (watch_server_steamid ^ 0x0514D449EDC24001ULL)),
                static_cast<unsigned long long>(source_job));
            return true;
        }
        case 8209: {
            // CMsgDOTAClaimEventAction -> CMsgDOTAClaimEventActionResponse
            // Parse event_id (field 1) and action_id (field 2) from request.
            // Return result=Success(0) with the action_id echoed back.
            uint32 claim_event_id = 0;
            uint32 claim_action_id = 0;
            {
                size_t pos = 0;
                while (pos < body_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end))
                        break;
                    if (field.number == 1u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        claim_event_id = static_cast<uint32>(v);
                    }
                    if (field.number == 2u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        claim_action_id = static_cast<uint32>(v);
                    }
                }
            }

            // Build CMsgDOTAClaimEventActionResponse:
            // field 1 = result (varint, 0=Success)
            // field 2 = reward_results (repeated, empty for now)
            // field 3 = action_id (varint)
            std::string claim_body;
            gbe::gc_message::build_dota_claim_event_action_response_body(claim_action_id, claim_body);

            std::string claim_response;
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(8210u, has_source_job, source_job, claim_body, claim_response);
            GBE_PushDotaResponse(8210u, claim_response, false, nullptr, "8209_8210");

            GBE_GC_DebugLog("GC_DOTA_DIRECT", "claim event action -> success event=%u action=%u source_job=%llu",
                claim_event_id, claim_action_id, static_cast<unsigned long long>(source_job));
            return true;
        }
        case 2510: {
            // StorePurchaseInit - client wants to buy an item from the store.
            // Parse the request to get item_def_id, then respond with success
            // and grant the item to the player's inventory.
            
            // Parse line_items (field 4) to get item_def_id
            uint32_t purchased_def = 0;
            uint32_t purchased_qty = 1;
            {
                size_t pos = 0;
                while (pos < body_size) {
                    uint8_t tag = body[pos];
                    uint8_t field_num = tag >> 3;
                    uint8_t wire_type = tag & 0x07;
                    pos++;
                    if (wire_type == 0) { // varint
                        while (pos < body_size && (body[pos] & 0x80)) pos++;
                        pos++;
                    } else if (wire_type == 2) { // length-delimited
                        uint32_t len = 0;
                        uint8_t shift = 0;
                        while (pos < body_size) {
                            uint8_t b = body[pos++];
                            len |= (b & 0x7F) << shift;
                            shift += 7;
                            if (!(b & 0x80)) break;
                        }
                        if (field_num == 4 && len > 0 && pos + len <= body_size) {
                            // Parse CGCStorePurchaseInit_LineItem sub-message
                            const uint8_t *sub = body + pos;
                            size_t sub_pos = 0;
                            while (sub_pos < len) {
                                uint8_t st = sub[sub_pos];
                                uint8_t sf = st >> 3;
                                uint8_t sw = st & 0x07;
                                sub_pos++;
                                if (sw == 0) {
                                    uint32_t val = 0;
                                    uint8_t s2 = 0;
                                    while (sub_pos < len) {
                                        uint8_t b = sub[sub_pos++];
                                        val |= (b & 0x7F) << s2;
                                        s2 += 7;
                                        if (!(b & 0x80)) break;
                                    }
                                    if (sf == 1) purchased_def = val;
                                    else if (sf == 2) purchased_qty = val;
                                } else if (sw == 2) {
                                    uint32_t sl = 0;
                                    uint8_t s2 = 0;
                                    while (sub_pos < len) {
                                        uint8_t b = sub[sub_pos++];
                                        sl |= (b & 0x7F) << s2;
                                        s2 += 7;
                                        if (!(b & 0x80)) break;
                                    }
                                    sub_pos += sl;
                                } else {
                                    break;
                                }
                            }
                        }
                        pos += len;
                    } else {
                        break;
                    }
                }
            }

            if (purchased_def == 0) {
                GBE_GC_DebugLog("GC_DOTA_DIRECT", "StorePurchaseInit failed to parse item_def_id body_size=%zu", body_size);
                return true;
            }

            // Build StorePurchaseInitResponse (msg 2511): result=1, txn_id=fake
            {
                std::string resp_body;
                uint64_t fake_txn = (static_cast<uint64_t>(0xBEEF0000u) | purchased_def);
                gbe::gc_message::build_dota_store_purchase_init_response_body(fake_txn, resp_body);

                std::string response_message;
                gbe::gc_message::build_dota_job_reply_or_zero_header_payload(2511u, has_source_job, source_job, resp_body, response_message);
                push_incoming_now(2511u | GBE_kProtoMask, response_message);
            }

            // Grant the purchased item to inventory
            {
                CSteamID player_steam_id = settings->get_local_steam_id();
                uint32_t account_id = player_steam_id.GetAccountID();

                for (uint32_t qi = 0; qi < purchased_qty; qi++) {
                    uint32_t seq = static_cast<uint32_t>(items.size()) + 1;
                    uint64_t new_item_id = (static_cast<uint64_t>(0x50000000u + seq) << 32ull) | static_cast<uint64_t>(account_id);

                    Econ_Item item;
                    item.id = new_item_id;
                    item.def = purchased_def;
                    item.level = 1;
                    item.quality = static_cast<EItemQuality>(4);
                    item.inv_pos = seq;
                    item.quantity = 1;
                    item.flags = 0;
                    item.origin = 2; // purchased
                    item.in_use = false;
                    item.original_id = new_item_id;
                    item.style = 0;
                    items.push_back(item);

                    // Notify client via SOCreate (msg 21 = k_ESOMsg_Create)
                    std::string create_body;
                    GBE_BuildSOSingleObjectFromItem(item, player_steam_id, create_body);
                    std::string create_response;
                    gbe::gc_message::build_dota_zero_header_payload(21u, create_body, create_response);
                    push_incoming_now(21u | GBE_kProtoMask, create_response);
                }

                save_items_to_file();
            }

            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "StorePurchaseInit success def=%u qty=%u source_job=%llu",
                purchased_def, purchased_qty,
                static_cast<unsigned long long>(source_job)
            );
            return true;
        }
        case 1092: {
            // k_EMsgGCRequestCrateItems -> k_EMsgGCRequestCrateItemsResponse (1093)
            // Client asks what items are in a crate. Parse crate_item_def, look up
            // loot list, return the def_indexes of items in the loot list.
            uint32 crate_def = 0;
            {
                size_t pos = 0;
                while (pos < body_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end)) break;
                    if (field.number == 1u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        crate_def = static_cast<uint32>(v);
                    }
                }
            }
            // Build response with item_defs from loot list
            std::vector<uint32> crate_item_defs;
            // Look up loot list for this treasure
            auto tll_it = GBE_vpk_loot_data.treasure_to_loot_list.find(crate_def);
            if (tll_it != GBE_vpk_loot_data.treasure_to_loot_list.end()) {
                auto ll_it = GBE_vpk_loot_data.loot_lists.find(tll_it->second);
                if (ll_it != GBE_vpk_loot_data.loot_lists.end()) {
                    for (const auto &entry_name : ll_it->second) {
                        auto def_it = GBE_vpk_loot_data.name_to_def.find(entry_name);
                        if (def_it != GBE_vpk_loot_data.name_to_def.end())
                            crate_item_defs.push_back(def_it->second);
                    }
                }
            }
            std::string resp_body;
            gbe::gc_message::build_dota_crate_items_response_body(crate_item_defs, resp_body);
            std::string response_message;
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(1093u, has_source_job, source_job, resp_body, response_message);
            GBE_PushDotaResponse(1093u, response_message, false, nullptr, "1092_1093_crate_items");
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "RequestCrateItems -> success crate_def=%u source_job=%llu",
                crate_def, static_cast<unsigned long long>(source_job));
            return true;
        }
        case 1025: {
            // k_EMsgGCUseItemRequest -> k_EMsgGCUseItemResponse (1026)
            // Client wants to "use" an item (open a treasure).
            // Find the item in inventory, get its def_index, look up loot list,
            // pick a random item, grant it, then respond.
            uint64 use_item_id = 0;
            {
                size_t pos = 0;
                while (pos < body_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end)) break;
                    if (field.number == 1u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        use_item_id = v;
                    } else if (field.number == 1u && field.wire_type == 1u) {
                        if (field.value_offset + 8 <= body_size) memcpy(&use_item_id, body + field.value_offset, 8);
                    }
                }
            }
            // Find item def_index from inventory
            uint32 item_def = 0;
            for (const auto &inv_item : items) {
                if (inv_item.id == use_item_id) { item_def = inv_item.def; break; }
            }
            // Try to open as treasure: look up loot list
            uint32 granted_def = 0;
            std::string resolved_loot_list;
            auto tll_it = GBE_vpk_loot_data.treasure_to_loot_list.find(item_def);
            if (tll_it != GBE_vpk_loot_data.treasure_to_loot_list.end()) {
                resolved_loot_list = tll_it->second;
            } else {
                // Try tool.usage.loot_list path (gem packs, gifts, etc.)
                auto tool_it = GBE_vpk_loot_data.tool_to_loot_list.find(item_def);
                if (tool_it != GBE_vpk_loot_data.tool_to_loot_list.end()) {
                    resolved_loot_list = tool_it->second;
                }
            }
            if (!resolved_loot_list.empty()) {
                auto ll_it = GBE_vpk_loot_data.loot_lists.find(resolved_loot_list);
                if (ll_it != GBE_vpk_loot_data.loot_lists.end() && !ll_it->second.empty()) {
                    // Pick random item from loot list
                    size_t idx = static_cast<size_t>(rand()) % ll_it->second.size();
                    const std::string &chosen_name = ll_it->second[idx];
                    auto def_it = GBE_vpk_loot_data.name_to_def.find(chosen_name);
                    if (def_it != GBE_vpk_loot_data.name_to_def.end()) {
                        granted_def = def_it->second;
                    }
                }
            }
            // Grant the item if we found one
            if (granted_def != 0) {
                CSteamID player_steam_id = settings->get_local_steam_id();
                uint32_t account_id = player_steam_id.GetAccountID();
                uint32_t seq = static_cast<uint32_t>(items.size()) + 1;
                uint64_t new_item_id = (static_cast<uint64_t>(0x60000000u + seq) << 32ull) | static_cast<uint64_t>(account_id);

                Econ_Item new_item;
                new_item.id = new_item_id;
                new_item.def = granted_def;
                new_item.level = 1;
                new_item.quality = static_cast<EItemQuality>(4);
                new_item.inv_pos = seq;
                new_item.quantity = 1;
                new_item.flags = 0;
                new_item.origin = 8; // opened from crate
                new_item.in_use = false;
                new_item.original_id = new_item_id;
                new_item.style = 0;
                items.push_back(new_item);

                std::string create_body;
                GBE_BuildSOSingleObjectFromItem(new_item, player_steam_id, create_body);
                std::string create_response;
                gbe::gc_message::build_dota_zero_header_payload(21u, create_body, create_response);
                push_incoming_now(21u | GBE_kProtoMask, create_response);

                save_items_to_file();
            }
            // Response
            std::string resp_body;
            gbe::gc_message::build_dota_use_item_response_body(granted_def != 0, resp_body);
            std::string response_message;
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(1026u, has_source_job, source_job, resp_body, response_message);
            push_incoming_now(1026u | GBE_kProtoMask, response_message);
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "UseItemRequest -> item_id=0x%llx def=%u granted_def=%u source_job=%llu",
                static_cast<unsigned long long>(use_item_id), item_def, granted_def,
                static_cast<unsigned long long>(source_job));
            return true;
        }
        case 2574: {
            // k_EMsgClientToGCUnlockCrate -> k_EMsgClientToGCUnlockCrateResponse (2575)
            // Open a treasure chest. Find crate in inventory, pick random from loot list, grant it.
            uint64 crate_item_id = 0;
            uint64 key_item_id = 0;
            {
                size_t pos = 0;
                while (pos < body_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end)) break;
                    if (field.number == 1u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        crate_item_id = v;
                    } else if (field.number == 2u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        key_item_id = v;
                    }
                }
            }
            // Find crate def_index
            uint32 crate_def = 0;
            for (const auto &inv_item : items) {
                if (inv_item.id == crate_item_id) { crate_def = inv_item.def; break; }
            }
            // Pick random item from loot list
            std::vector<uint32> granted_defs;
            auto tll_it = GBE_vpk_loot_data.treasure_to_loot_list.find(crate_def);
            if (tll_it != GBE_vpk_loot_data.treasure_to_loot_list.end()) {
                auto ll_it = GBE_vpk_loot_data.loot_lists.find(tll_it->second);
                if (ll_it != GBE_vpk_loot_data.loot_lists.end() && !ll_it->second.empty()) {
                    size_t idx = static_cast<size_t>(rand()) % ll_it->second.size();
                    const std::string &chosen_name = ll_it->second[idx];
                    auto def_it = GBE_vpk_loot_data.name_to_def.find(chosen_name);
                    if (def_it != GBE_vpk_loot_data.name_to_def.end()) {
                        granted_defs.push_back(def_it->second);
                    }
                }
            }
            // Grant items
            CSteamID player_steam_id = settings->get_local_steam_id();
            uint32_t account_id = player_steam_id.GetAccountID();
            for (uint32 gdef : granted_defs) {
                uint32_t seq = static_cast<uint32_t>(items.size()) + 1;
                uint64_t new_item_id = (static_cast<uint64_t>(0x61000000u + seq) << 32ull) | static_cast<uint64_t>(account_id);

                Econ_Item new_item;
                new_item.id = new_item_id;
                new_item.def = gdef;
                new_item.level = 1;
                new_item.quality = static_cast<EItemQuality>(4);
                new_item.inv_pos = seq;
                new_item.quantity = 1;
                new_item.flags = 0;
                new_item.origin = 8;
                new_item.in_use = false;
                new_item.original_id = new_item_id;
                new_item.style = 0;
                items.push_back(new_item);

                std::string create_body;
                GBE_BuildSOSingleObjectFromItem(new_item, player_steam_id, create_body);
                std::string create_response;
                gbe::gc_message::build_dota_zero_header_payload(21u, create_body, create_response);
                push_incoming_now(21u | GBE_kProtoMask, create_response);
            }
            if (!granted_defs.empty()) save_items_to_file();
            // Build response with granted items
            std::string resp_body;
            gbe::gc_message::build_dota_unlock_crate_response_body(granted_defs, resp_body);
            std::string response_message;
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(2575u, has_source_job, source_job, resp_body, response_message);
            push_incoming_now(2575u | GBE_kProtoMask, response_message);
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "UnlockCrate -> OK crate_id=0x%llx crate_def=%u granted=%zu source_job=%llu",
                static_cast<unsigned long long>(crate_item_id), crate_def,
                granted_defs.size(), static_cast<unsigned long long>(source_job));
            return true;
        }
        case 2576: {
            // k_EMsgClientToGCUnpackBundle -> k_EMsgClientToGCUnpackBundleResponse (2567)
            // Unpack a bundle: grant all contained items.
            uint64 bundle_item_id = 0;
            {
                size_t pos = 0;
                while (pos < body_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end)) break;
                    if (field.number == 1u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        bundle_item_id = v;
                    }
                }
            }
            // Find bundle def_index
            uint32 bundle_def = 0;
            for (const auto &inv_item : items) {
                if (inv_item.id == bundle_item_id) { bundle_def = inv_item.def; break; }
            }
            // Look up bundle contents and grant all items
            std::vector<uint32> granted_defs;
            auto bc_it = GBE_vpk_loot_data.bundle_contents.find(bundle_def);
            if (bc_it != GBE_vpk_loot_data.bundle_contents.end()) {
                for (const auto &item_name : bc_it->second) {
                    auto def_it = GBE_vpk_loot_data.name_to_def.find(item_name);
                    if (def_it != GBE_vpk_loot_data.name_to_def.end()) {
                        granted_defs.push_back(def_it->second);
                    }
                }
            }
            // Grant items
            CSteamID player_steam_id2 = settings->get_local_steam_id();
            uint32_t account_id2 = player_steam_id2.GetAccountID();
            for (uint32 gdef : granted_defs) {
                uint32_t seq = static_cast<uint32_t>(items.size()) + 1;
                uint64_t new_item_id = (static_cast<uint64_t>(0x62000000u + seq) << 32ull) | static_cast<uint64_t>(account_id2);

                Econ_Item new_item;
                new_item.id = new_item_id;
                new_item.def = gdef;
                new_item.level = 1;
                new_item.quality = static_cast<EItemQuality>(4);
                new_item.inv_pos = seq;
                new_item.quantity = 1;
                new_item.flags = 0;
                new_item.origin = 8;
                new_item.in_use = false;
                new_item.original_id = new_item_id;
                new_item.style = 0;
                items.push_back(new_item);

                std::string create_body;
                GBE_BuildSOSingleObjectFromItem(new_item, player_steam_id2, create_body);
                std::string create_response;
                gbe::gc_message::build_dota_zero_header_payload(21u, create_body, create_response);
                push_incoming_now(21u | GBE_kProtoMask, create_response);
            }
            if (!granted_defs.empty()) save_items_to_file();
            // Response
            std::string resp_body;
            gbe::gc_message::build_dota_unpack_bundle_response_body(granted_defs, resp_body);
            std::string response_message;
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(2567u, has_source_job, source_job, resp_body, response_message);
            push_incoming_now(2567u | GBE_kProtoMask, response_message);
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "UnpackBundle -> Succeeded bundle_id=0x%llx bundle_def=%u granted=%zu source_job=%llu",
                static_cast<unsigned long long>(bundle_item_id), bundle_def,
                granted_defs.size(), static_cast<unsigned long long>(source_job));
            return true;
        }
        case 8260: {
            // k_EMsgClientToGCClaimEventActionUsingItem -> k_EMsgClientToGCClaimEventActionUsingItemResponse (8261)
            // Similar to 8209 but uses an item. Parse event_id, action_id, item_id.
            uint32 claim_event_id = 0;
            uint32 claim_action_id = 0;
            uint64 claim_item_id = 0;
            {
                size_t pos = 0;
                while (pos < body_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end)) break;
                    if (field.number == 1u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        claim_event_id = static_cast<uint32>(v);
                    } else if (field.number == 2u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        claim_action_id = static_cast<uint32>(v);
                    } else if (field.number == 3u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        claim_item_id = v;
                    }
                }
            }
            // CMsgClientToGCClaimEventActionUsingItemResponse:
            // field 1 = action_results (CMsgDOTAClaimEventActionResponse sub-message)
            //   sub field 1 = result (0=Success)
            //   sub field 3 = action_id
            std::string resp_body;
            gbe::gc_message::build_dota_claim_event_action_using_item_response_body(claim_action_id, resp_body);
            std::string response_message;
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(8261u, has_source_job, source_job, resp_body, response_message);
            GBE_PushDotaResponse(8261u, response_message, false, nullptr, "8260_8261");
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "ClaimEventActionUsingItem -> success event=%u action=%u item=0x%llx source_job=%llu",
                claim_event_id, claim_action_id,
                static_cast<unsigned long long>(claim_item_id),
                static_cast<unsigned long long>(source_job));
            return true;
        }
        default:
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "no replay template req=%u source_job=%llu body_size=%zu body_prefix=%s",
                request_emsg,
                static_cast<unsigned long long>(source_job),
                body_size,
                gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(body), body_size, 32).c_str()
            );
            return false;
    }

    std::string decoded_template;
    if (template_hex != nullptr) {
        if (!gbe::proto_wire::decode_hex_string(template_hex, decoded_template)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed decoding replay template req=%u resp=%u", request_emsg, response_emsg);
            return true;
        }

        template_bytes = reinterpret_cast<const uint8 *>(decoded_template.data());
        template_size = decoded_template.size();
    }

    const bool mirror_source_job_to_target = has_source_job;

    std::string response_message;
    if (!GBE_PrepareDotaDirectReplayMessage(
            template_bytes,
            template_size,
            settings->get_local_steam_id().GetAccountID(),
            settings->get_local_steam_id().ConvertToUint64(),
            replace_account,
            replace_steam_id,
            mirror_source_job_to_target,
            source_job,
            request_emsg,
            response_emsg,
            body_size,
            response_note,
            response_message)) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building replay req=%u resp=%u", request_emsg, response_emsg);
        return true;
    }

    if (request_emsg == 8744u) {
        size_t response_body_offset = 8u;
        if (response_message.size() >= 8u) {
            uint32 response_header_length = 0;
            std::memcpy(&response_header_length, response_message.data() + 4, sizeof(response_header_length));
            response_body_offset += response_header_length;
        }
        const uint8 *response_body = response_body_offset <= response_message.size()
            ? reinterpret_cast<const uint8 *>(response_message.data() + response_body_offset)
            : nullptr;
        const size_t response_body_size = response_body_offset <= response_message.size()
            ? (response_message.size() - response_body_offset)
            : 0u;

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "observed resp=%u for req=%u source_job=%llu body_size=%zu fields=%s body_prefix=%s",
            response_emsg,
            request_emsg,
            static_cast<unsigned long long>(source_job),
            response_body_size,
            gbe::proto_wire::format_top_level_field_summary(response_body, response_body_size).c_str(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(response_body), response_body_size, 32).c_str()
        );
    }

    GBE_GC_DebugLog("GC_DOTA_DIRECT", "replying req=%u resp=%u source_job=%llu size=%zu note=%s", request_emsg, response_emsg, static_cast<unsigned long long>(source_job), response_message.size(), response_note);
    GBE_PushDotaResponse(response_emsg, response_message, false, nullptr, response_note);

    if (request_emsg == 8676) {
        std::string followup_message;
        if (!GBE_PrepareDotaDirectReplayMessage(
                GBE_kDota8678Template,
                sizeof(GBE_kDota8678Template),
                settings->get_local_steam_id().GetAccountID(),
                settings->get_local_steam_id().ConvertToUint64(),
                false,
                false,
                false,
                0,
                request_emsg,
                8678u,
                body_size,
                "8676 followup 8678",
                followup_message)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building 8678 followup after 8676");
            return true;
        }

        GBE_GC_DebugLog("GC_DOTA_DIRECT", "queueing followup req=%u resp=%u size=%zu note=member.zip unsolicited update", request_emsg, 8678u, followup_message.size());
        GBE_PushDotaResponse(8678u, followup_message, false, nullptr, "8676_followup_8678");
    }

    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaServerAssignmentRequest(uint32 request_emsg, const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job)
{
    uint32 public_ip = 0;
    uint32 private_ip = 0;
    uint32 server_port = 0;
    uint32 tv_port = 0;
    uint32 server_type = 0;
    uint32 server_region = 0;
    uint32 relay_slots_max = 0;
    uint32 server_version = 0;
    uint32 server_cluster = 0;
    uint32 assigned_tv_port = 0;
    uint32 allow_custom_games = 0;
    uint32 build_version = 0;

    gbe::proto_wire::read_uint32_field(body, body_size, 1u, public_ip);
    gbe::proto_wire::read_uint32_field(body, body_size, 2u, private_ip);
    gbe::proto_wire::read_uint32_field(body, body_size, 3u, server_port);
    gbe::proto_wire::read_uint32_field(body, body_size, 4u, tv_port);
    gbe::proto_wire::read_uint32_field(body, body_size, 7u, server_type);
    gbe::proto_wire::read_uint32_field(body, body_size, 8u, server_region);
    gbe::proto_wire::read_uint32_field(body, body_size, 13u, relay_slots_max);
    gbe::proto_wire::read_uint32_field(body, body_size, 19u, server_version);
    gbe::proto_wire::read_uint32_field(body, body_size, 20u, server_cluster);
    gbe::proto_wire::read_uint32_field(body, body_size, 22u, assigned_tv_port);
    gbe::proto_wire::read_uint32_field(body, body_size, 23u, allow_custom_games);
    gbe::proto_wire::read_uint32_field(body, body_size, 24u, build_version);

    // Extract tv_secret_code from field 18 - needed for SourceTV spectating
    // Try fixed64 first (wire type 1), then varint (wire type 0)
    uint64 tv_secret_code = 0;
    {
        gbe::proto_wire::Field f18{};
        const bool has_f18 = gbe::proto_wire::find_field(body, body_size, 18u, f18);
        if (has_f18) {
            if (f18.wire_type == 1 && f18.value_size == 8) {
                std::memcpy(&tv_secret_code, body + f18.value_offset, sizeof(tv_secret_code));
            } else if (f18.wire_type == 0) {
                size_t off = f18.value_offset;
                gbe::proto_wire::read_varuint(body, body_size, off, tv_secret_code);
            }
            GBE_GC_DebugLog("GC_DOTA_DIRECT",
                "4508 field_18 found wire_type=%u value_size=%zu tv_secret_code=0x%llx",
                f18.wire_type, f18.value_size,
                static_cast<unsigned long long>(tv_secret_code));
        } else {
            // Field 18 not found - dump all fields for diagnosis
            size_t pos = 0;
            std::string field_list;
            while (pos < body_size) {
                gbe::proto_wire::Field field{};
                size_t field_offset = 0;
                size_t field_end = 0;
                if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end))
                    break;
                if (!field_list.empty()) field_list += ",";
                field_list += std::to_string(field.number) + ":" + std::to_string(field.wire_type);
            }
            GBE_GC_DebugLog("GC_DOTA_DIRECT",
                "4508 field_18 NOT found body_size=%zu fields=[%s]",
                body_size, field_list.c_str());
        }
    }

    const std::string runtime_connect = gbe::proto_wire::normalize_dota_practice_lobby_connect(
        gbe::proto_wire::format_dota_practice_lobby_connect_from_ips(public_ip, private_ip, server_port));
    // 4508 reports the engine's listen address, but peers that already have a
    // working LAN endpoint must keep it to avoid a post-connect P2P redirect.
    const bool preserve_existing_lan_connect =
        GBE_local_lobby.active &&
        GBE_local_lobby.lobby_id != 0 &&
        GBE_local_lobby.custom_game.game_id == 0ull &&
        GBE_local_lobby.lan &&
        GBE_local_lobby.match_id != 0ull &&
        gbe::proto_wire::parse_dota_practice_lobby_connect_ipv4(GBE_local_lobby.connect) != 0u &&
        gbe::proto_wire::parse_dota_practice_lobby_connect_ipv4(runtime_connect) != 0u &&
        runtime_connect != GBE_local_lobby.connect;
    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 &&
        !runtime_connect.empty() && runtime_connect != GBE_local_lobby.connect &&
        !preserve_existing_lan_connect) {
        const std::string previous_connect = GBE_local_lobby.connect;
        GBE_local_lobby.connect = runtime_connect;
        if (GBE_shared_dota_lobby_state.valid && GBE_shared_dota_lobby_state.lobby_id == GBE_local_lobby.lobby_id)
            GBE_shared_dota_lobby_state.connect = runtime_connect;

        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "adopted game server address as lobby connect reason=4508_game_server_info lobby_id=%llu previous=%s new=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            previous_connect.c_str(),
            runtime_connect.c_str()
        );
    } else if (preserve_existing_lan_connect) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "preserved existing LAN lobby connect over 4508 runtime address lobby_id=%llu current=%s candidate=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.connect.c_str(),
            runtime_connect.c_str()
        );
    }

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "consumed req=%u source_job=%llu note=game server info notification public_ip=%s private_ip=%s port=%u tv_port=%u assigned_tv_port=%u type=%u region=%u relay_slots=%u version=%u build=%u cluster=%u custom_games=%u",
        request_emsg,
        static_cast<unsigned long long>(source_job),
        gbe::proto_wire::format_ipv4(public_ip).c_str(),
        gbe::proto_wire::format_ipv4(private_ip).c_str(),
        server_port,
        tv_port,
        assigned_tv_port,
        server_type,
        server_region,
        relay_slots_max,
        server_version,
        build_version,
        server_cluster,
        allow_custom_games
    );

    GBE_TrySyncDotaLobbyServerIdFromGameServer("4508_game_server_info");

    // Store tv_secret_code and tv_port for SourceTV spectating
    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0) {
        if (tv_secret_code != 0)
            GBE_local_lobby.tv_secret_code = tv_secret_code;
        if (tv_port != 0)
            GBE_local_lobby.tv_port = tv_port;
        GBE_PublishDotaPracticeLobbyMetadata("4508_game_server_info");
    }

    if (GBE_HasDotaLaunchServerSetupSync())
        GBE_MarkDotaLaunchPhase(GBE_kDotaLaunchPhaseSetupSynced, "4508_game_server_info");

    if (GBE_local_lobby.state == 1u && GBE_local_lobby.game_state == 0u && GBE_HasDotaLaunchServerSetupSync()) {
        if (GBE_TryAdvanceDotaLaunchToRun("runtime packet after 4508", request_emsg, source_job, "4508_launch_run"))
            return true;
    }

    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaDirectPostLoginRequest(uint32 unMsgType, const void *pubData, uint32 cubData)
{
    GBE_RestoreSharedDotaLobbyState("direct_post_login_request");

    const uint32 request_emsg = GBE_GC_MaskedEMsg(unMsgType);

    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(pubData, cubData, proto_context)) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed parsing direct request req=%u len=%u", request_emsg, cubData);
        return false;
    }

    const bool has_source_job = proto_context.protohdr.has_job_id_source();
    const uint64 source_job = has_source_job ? proto_context.protohdr.job_id_source() : 0ull;
    const uint8 *body = proto_context.body;
    const size_t body_size = proto_context.body_size;

    gbe::dota_gc_router::DotaGcRequestContext request_context{};
    request_context.valid = true;
    request_context.inner_emsg = request_emsg;
    request_context.body.assign(reinterpret_cast<const char *>(body), body_size);
    request_context.request_job_id = source_job;
    request_context.has_request_job = has_source_job;
    request_context.wrapped = false;
    if (GBE_DispatchDotaPostLoginRequest(request_context))
        return true;

    if (request_emsg == 7070u) {
        return GBE_HandleDotaCustomGameReadyUpRequest(proto_context.body, proto_context.body_size, has_source_job, source_job);
    }

    if (request_emsg == 8052u) {
        return GBE_HandleDotaCustomGameStartedLoadingRequest(proto_context.body, proto_context.body_size, has_source_job, source_job);
    }

    if (request_emsg == 8053u) {
        return GBE_HandleDotaCustomGameFinishedLoadingRequest(proto_context.body, proto_context.body_size, has_source_job, source_job);
    }

    if (request_emsg == GBE_kDotaChatMessage) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 7273 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            proto_context.body_size,
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(proto_context.body), proto_context.body_size, 48).c_str()
        );

        return GBE_HandleDotaChatMessageRequest(
            std::string(reinterpret_cast<const char *>(proto_context.body), proto_context.body_size),
            false,
            nullptr
        );
    }

    if (request_emsg == GBE_kDotaLeaveChatChannel) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 7272 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(body), body_size, 48).c_str()
        );

        return GBE_HandleDotaLeaveChatChannelRequest(
            std::string(reinterpret_cast<const char *>(body), body_size),
            false,
            nullptr
        );
    }

    if (request_emsg == GBE_kDotaDestroyLobbyRequest) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 8246 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(body), body_size, 48).c_str()
        );

        return GBE_HandleDotaDestroyLobbyRequest(source_job, has_source_job, false, nullptr);
    }

    if (request_emsg == GBE_kDotaAddSocket) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "received direct 1087 AddSocket source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(body), body_size, 48).c_str()
        );

        return GBE_HandleDotaAddSocketRequest(body, body_size, has_source_job, source_job);
    }

    if (request_emsg == GBE_kDotaUnlockItemStyle) {
        return GBE_HandleDotaUnlockItemStyleRequest(body, body_size, has_source_job, source_job);
    }

    if (request_emsg == GBE_kDotaSetItemStyle) {
        return GBE_HandleDotaSetItemStyleRequest(body, body_size, has_source_job, source_job);
    }

    if (request_emsg == 8727) {
        return GBE_HandleDotaMinimalVarintSuccessRequest(request_emsg, 8728u, "8727->8728 minimal success", "8727_8728", has_source_job, source_job);
    }

    if (request_emsg == 8886) {
        return GBE_HandleDotaMinimalVarintSuccessRequest(request_emsg, 8887u, "8886->8887 minimal success", "8886_8887", has_source_job, source_job);
    }

    if (request_emsg == 7427) {
        return GBE_HandleDota7427NotificationsRequest(has_source_job, source_job);
    }

    if (request_emsg == 8793) {
        return GBE_HandleDotaMinimalVarintSuccessRequest(request_emsg, 8794u, "8793->8794 minimal success", "8793_8794", has_source_job, source_job);
    }

    if (request_emsg == 4523) {
        return GBE_HandleDotaUploadRateRequest(has_source_job, source_job);
    }

    if (request_emsg == 7534) {
        return GBE_HandleDotaProfileCardRequest(body, body_size, has_source_job, source_job);
    }

    if (request_emsg == 2581) {
        return GBE_HandleDotaLookupAccountNameRequest(body, body_size, has_source_job, source_job);
    }

    if (request_emsg == 7503) {
        return GBE_HandleDotaEmoticonDataRequest(body, body_size, has_source_job, source_job);
    }

    if (request_emsg == 8095) {
        return GBE_HandleDotaConductScorecardRequest(body, body_size, has_source_job, source_job);
    }

    if (request_emsg == 8800) {
        return GBE_HandleDotaCoachingSummaryRequest(body, body_size, has_source_job, source_job);
    }

    if (request_emsg == 8879) {
        return GBE_HandleDotaRankRequest(body, body_size, has_source_job, source_job);
    }

    if (request_emsg == 7450) {
        return GBE_HandleDotaBatchPlayerResourcesRequest(body, body_size, has_source_job, source_job);
    }

    if (request_emsg == 7034)
        return GBE_HandleDotaDirect7034Request(request_emsg, body, body_size, has_source_job, source_job);

    if (request_emsg == 8744u) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "observed req=%u source_job=%llu body_size=%zu fields=%s body_prefix=%s",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            body_size,
            gbe::proto_wire::format_top_level_field_summary(body, body_size).c_str(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(body), body_size, 32).c_str()
        );
    }

    if (request_emsg == GBE_kDotaCacheSubscriptionRefresh) {
        return GBE_HandleDotaCacheSubscriptionRefreshRequest(body, body_size, has_source_job, source_job);
    }

    if (request_emsg == GBE_kDotaAbandonCurrentGame) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "handling req=%u source_job=%llu note=AbandonCurrentGame body_size=%zu active=%u lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_local_lobby.active ? 1u : 0u,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id)
        );
        return GBE_HandleDotaAbandonCurrentGameRequest(false, nullptr);
    }

    // Handle CMsgLeaverDetected (7072) from game server
    // When a player disconnects and later abandons (or the abandon timer expires),
    // the game server sends this message. We update the member's leaver_status
    // and push a lobby update so the Dota client sees the transition from
    // DISCONNECTED to ABANDONED (with left_member_indices updated).
    if (request_emsg == GBE_kDotaLeaverDetected) {
        return GBE_HandleDotaLeaverDetectedRequest(body, body_size, source_job);
    }

    if (request_emsg == GBE_kDotaGameMatchSignOutPermissionRequest) {
        return GBE_HandleDotaSignOutPermissionRequest(has_source_job, source_job);
    }

    if (request_emsg == GBE_kDotaGameMatchSignOut) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "handling req=%u source_job=%llu note=GameMatchSignOut body_size=%zu active=%u lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_local_lobby.active ? 1u : 0u,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id)
        );
        return GBE_HandleDotaGameMatchSignOutRequest(false, nullptr, has_source_job, source_job);
    }

    if (request_emsg == GBE_kDotaSubmitPlayerReportV2) {
        return GBE_HandleDotaSubmitPlayerReportV2Request(body, body_size, has_source_job, source_job);
    }

    if (request_emsg == 4506) {
        return GBE_HandleDotaLaunchAdvanceOrConsume(request_emsg, "runtime packet after 4506 stall recovery", "4506_launch_run", "server available acknowledgement", source_job, body_size);
    }

    if (request_emsg == GBE_kSteamTicketAuthComplete) {
        return GBE_HandleDotaLaunchAdvanceOrConsume(request_emsg, "runtime packet after 5429", "5429_launch_run", "ticket auth complete", source_job, body_size);
    }

    if (request_emsg == 8870) {
        return GBE_HandleDota8870LaunchMarkerRequest(request_emsg, source_job);
    }

    if (request_emsg == 4511) {
        return GBE_HandleDotaLanServerAvailableRequest(request_emsg, body, body_size, source_job);
    }

    if (request_emsg == 4508) {
        return GBE_HandleDotaServerAssignmentRequest(request_emsg, body, body_size, has_source_job, source_job);
    }

    if (request_emsg == GBE_kSteamGamesPlayedWithDataBlob && GBE_ShouldTrackDotaPracticeLobbyLateSteamChain()) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "consumed req=%u source_job=%llu note=late steam chain games played observed without synthetic followup active=%u lobby_id=%llu state=%u game_state=%u body_size=%zu body_prefix=%s",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            GBE_local_lobby.active ? 1u : 0u,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            body_size,
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(body), body_size, 48).c_str()
        );
        return true;
    }

    if (request_emsg == GBE_kSteamAuthList && GBE_ShouldTrackDotaPracticeLobbyLateSteamChain()) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "consumed req=%u source_job=%llu note=late steam chain auth list observed without synthetic followup active=%u lobby_id=%llu state=%u game_state=%u body_size=%zu body_prefix=%s",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            GBE_local_lobby.active ? 1u : 0u,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            body_size,
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(body), body_size, 48).c_str()
        );
        return true;
    }

    if (request_emsg == 2569u) {
        return GBE_HandleDotaEquipItemsRequest(body, body_size, has_source_job, source_job);
    }

    return GBE_HandleDotaTemplateReplayRequest(request_emsg, body, body_size, has_source_job, source_job);
}


bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyCreateRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw)
{
    GBE_DotaPracticeLobbyCreateRequest pre_reset_request{};
    GBE_DotaCustomGameDetails pre_reset_custom_game{};
    const bool parsed_pre_reset_request = gbe::proto_wire::parse_dota_practice_lobby_create_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), pre_reset_request);
    if (parsed_pre_reset_request && pre_reset_request.has_lobby_details) {
        GBE_ApplyDotaCustomGameDetailsRequest(pre_reset_request.lobby_details, pre_reset_custom_game);
        GBE_NormalizeDotaCustomGameDetailsFromInstalledMod(settings, pre_reset_custom_game);
    }

    const gbe::dota_lobby_state::CreateLobbyResetPlan reset_plan = gbe::dota_lobby_state::compose_create_lobby_reset_plan(GBE_local_lobby, pre_reset_custom_game);

    ResetGCMemory("7038_create", true, true);

    if (reset_plan.unsubscribe_previous_practice_lobby) {
        std::string response_25;
        if (gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(reset_plan.previous_lobby_id, response_25)) {
            push_incoming_now(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25);
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Unsubscribed previous practice lobby before arcade create previous_lobby_id=%llu previous_match_id=%llu previous_state=%u previous_game_state=%u previous_team=%u previous_slot=%u custom_game_id=%llu size=%zu",
                static_cast<unsigned long long>(reset_plan.previous_lobby_id),
                static_cast<unsigned long long>(reset_plan.previous_match_id),
                reset_plan.previous_state,
                reset_plan.previous_game_state,
                reset_plan.previous_owner_team,
                reset_plan.previous_owner_slot,
                static_cast<unsigned long long>(pre_reset_custom_game.game_id),
                response_25.size()
            );
        } else {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Failed unsubscribing previous practice lobby before arcade create previous_lobby_id=%llu custom_game_id=%llu",
                static_cast<unsigned long long>(reset_plan.previous_lobby_id),
                static_cast<unsigned long long>(pre_reset_custom_game.game_id)
            );
        }
    }

    GBE_DotaPracticeLobbyCreateRequest request{};
    const bool parsed_create_request = gbe::proto_wire::parse_dota_practice_lobby_create_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request);
    const gbe::dota_lobby_state::CreateLobbyPlan create_plan = gbe::dota_lobby_state::compose_create_lobby_plan(
        request,
        GBE_GenerateDotaLobbyId(),
        settings->get_local_steam_id().ConvertToUint64(),
        settings->get_local_steam_id().GetAccountID(),
        std::string(settings->get_local_name()),
        GBE_kDotaTeamGoodGuys,
        1u);
    GBE_local_lobby = create_plan.lobby;
    if (parsed_create_request && request.has_lobby_details) {
        GBE_NormalizeDotaCustomGameDetailsFromInstalledMod(settings, GBE_local_lobby.custom_game);
        const bool custom_game_create = GBE_local_lobby.custom_game.game_id != 0ull;
        GBE_NormalizeDotaArcadeLobbyMemberSlots(GBE_local_lobby);
        if (custom_game_create) {
            GBE_recent_dota_reconnect_context_valid = false;
            GBE_recent_dota_reconnect_context = GBE_DotaReconnectContext{};
            GBE_dota_reconnect_eligible.store(true);
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Isolated arcade lobby from prior practice runtime lobby_id=%llu custom_game_id=%llu",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(GBE_local_lobby.custom_game.game_id)
            );
        }
    }

    Steam_Client *steam_client = get_steam_client();
    if (steam_client && steam_client->steam_matchmaking) {
        CSteamID generic_lobby_id = steam_client->steam_matchmaking->CreateLobbyImmediate(k_ELobbyTypeInvisible, 10);
        if (generic_lobby_id.IsLobby())
            GBE_local_lobby.generic_lobby_id = generic_lobby_id.ConvertToUint64();
    }

    GBE_PublishDotaPracticeLobbyLocalMemberData("7038_create");
    GBE_SyncSettingsLobbyFromGenericLobby("7038_create");
    GBE_PublishSharedDotaLobbyState("7038_create");
    GBE_PublishDotaPracticeLobbyMetadata("7038_create");

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] State creating path=%s has_job=%u request_job=%llu NewLobbyID=%llu GenericLobbyID=%llu room=%s server_region=%u lan=%u lan_ping=%s mode=%u pass_len=%zu custom_id=%llu custom_mode=%s custom_map=%s custom_min=%u custom_max=%u",
        wrapped ? "wrapped" : "direct",
        has_request_job ? 1u : 0u,
        static_cast<unsigned long long>(request_job_id),
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        GBE_local_lobby.room_name.c_str(),
        GBE_local_lobby.server_region,
        GBE_local_lobby.lan ? 1u : 0u,
        GBE_local_lobby.lan_host_ping_location.c_str(),
        GBE_local_lobby.game_mode,
        GBE_local_lobby.pass_key.size(),
        static_cast<unsigned long long>(GBE_local_lobby.custom_game.game_id),
        GBE_local_lobby.custom_game.mode.c_str(),
        GBE_local_lobby.custom_game.map_name.c_str(),
        GBE_local_lobby.custom_game.min_players,
        GBE_local_lobby.custom_game.max_players
    );

    std::string response_24;
    const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    if (!GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay(GBE_GetDotaLobbyOwnerName(), response_24)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building template 24 cache update for LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    std::string response_7055;
    if (!gbe::gc_message::build_dota_practice_lobby_response_payload(request_job_id, has_request_job, response_7055)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7055 payload for LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    GBE_RecordDotaLobbyCacheSubscriptionState(response_24, wrapped ? "7038_create_wrapped" : "7038_create_direct");

    std::string wrapped_24;
    if (!GBE_PushDotaResponse(GBE_kDotaCacheSubscribed, response_24, wrapped, outer_session_field_raw, "7038_24", false, 0u, 0u, &wrapped_24))
        return true;

    if (wrapped) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Sent wrapped 24 cache update with NewLobbyID=%llu size=%zu body_prefix=%s packet_prefix=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            wrapped_24.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(response_24.data()), response_24.size(), 32).c_str(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(wrapped_24.data()), wrapped_24.size(), 32).c_str()
        );
    } else {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Sent direct 24 cache update with NewLobbyID=%llu size=%zu body_prefix=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            response_24.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(response_24.data()), response_24.size(), 32).c_str()
        );
    }

    std::string wrapped_7055;
    if (!GBE_PushDotaResponse(GBE_kDotaPracticeLobbyResponse, response_7055, wrapped, outer_session_field_raw, "7038_7055", false, 0u, 0u, &wrapped_7055))
        return true;

    if (wrapped) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Sent wrapped 7055 with NewLobbyID=%llu has_job=%u request_job=%llu size=%zu body_prefix=%s packet_prefix=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            has_request_job ? 1u : 0u,
            static_cast<unsigned long long>(request_job_id),
            wrapped_7055.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(response_7055.data()), response_7055.size(), 32).c_str(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(wrapped_7055.data()), wrapped_7055.size(), 32).c_str()
        );
    } else {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Sent direct 7055 with NewLobbyID=%llu has_job=%u request_job=%llu size=%zu body_prefix=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            has_request_job ? 1u : 0u,
            static_cast<unsigned long long>(request_job_id),
            response_7055.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(response_7055.data()), response_7055.size(), 32).c_str()
        );
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Skipping initial 26 details update for 7038 to match official create flow LobbyID=%llu",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id)
    );

    return true;
}


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
        GBE_PushDotaResponse(GBE_kDotaCacheUnsubscribed, response_25, wrapped, outer_session_field_raw, "7040_leave_after_lobby_list_25");
        ResetGCMemory("7040_leave_after_lobby_list", true, false);
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
    uint64 request_list_job_id = 0ull;
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


bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyJoinRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw, bool send_join_response)
{
    GBE_DotaPracticeLobbyJoinRequest request{};
    if (!gbe::proto_wire::parse_dota_practice_lobby_join_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7044 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str());
        return true;
    }

    CSteamID matched_generic_lobby_id = k_steamIDNil;
    GBE_LocalLobby matched_lobby{};
    bool matched_generic_lobby = request.has_lobby_id && request.lobby_id != 0 &&
        GBE_FindDotaGenericLobbyByDotaLobbyId(request.lobby_id, matched_generic_lobby_id, &matched_lobby, "7044_join");

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
            matched_generic_lobby = GBE_FindDotaGenericLobbyByDotaLobbyId(request.lobby_id, matched_generic_lobby_id, &matched_lobby, "7044_join_local_find");
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] 7044 local find fallback lobby_id=%llu generic_lobby_id=%llu matched=%u members=%zu",
                static_cast<unsigned long long>(request.lobby_id),
                static_cast<unsigned long long>(matched_generic_lobby_id.ConvertToUint64()),
                matched_generic_lobby ? 1u : 0u,
                matched_lobby.members.size()
            );
        }
    }

    if (!matched_generic_lobby && (!request.has_lobby_id || request.lobby_id == 0) && (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0))
        matched_lobby.lobby_id = GBE_GenerateDotaLobbyId();

    const bool join_has_lobby_id = request.has_lobby_id || matched_lobby.lobby_id != 0ull;
    const uint64 join_lobby_id = request.has_lobby_id && request.lobby_id != 0 ? request.lobby_id : matched_lobby.lobby_id;
    const gbe::dota_lobby_state::JoinLobbyMergePlan join_plan = gbe::dota_lobby_state::compose_join_lobby_merge_plan(
        GBE_local_lobby,
        join_has_lobby_id,
        join_lobby_id,
        matched_generic_lobby,
        matched_lobby,
        settings->get_local_steam_id().ConvertToUint64(),
        settings->get_local_steam_id().GetAccountID(),
        std::string(settings->get_local_name()),
        GBE_kDotaTeamGoodGuys,
        GBE_kDotaTeamPlayerPool);
    GBE_local_lobby = join_plan.lobby;

    if (matched_generic_lobby) {
        if (steam_client && steam_client->steam_matchmaking)
            steam_client->steam_matchmaking->JoinLobby(matched_generic_lobby_id);
        GBE_SyncSettingsLobbyFromGenericLobby("7044_join_generic");
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] 7044 matched generic custom metadata lobby_id=%llu generic_lobby_id=%llu custom_game_id=%llu custom_mode=%s custom_map=%s state=%u game_state=%u match_id=%llu server_id=%llu connect=%s",
            static_cast<unsigned long long>(matched_lobby.lobby_id),
            static_cast<unsigned long long>(matched_lobby.generic_lobby_id),
            static_cast<unsigned long long>(matched_lobby.custom_game.game_id),
            matched_lobby.custom_game.mode.c_str(),
            matched_lobby.custom_game.map_name.c_str(),
            matched_lobby.state,
            matched_lobby.game_state,
            static_cast<unsigned long long>(matched_lobby.match_id),
            static_cast<unsigned long long>(matched_lobby.server_id),
            matched_lobby.connect.c_str());
    }

    const GBE_DotaLobbyMemberState &local_member = join_plan.local_member;

    if (request.has_pass_key)
        GBE_local_lobby.pass_key = request.pass_key;
    GBE_PublishDotaPracticeLobbyLocalMemberData("7044_join");
    GBE_PublishSharedDotaLobbyState("7044_join");

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Preparing 7044 cache update LobbyID=%llu owner_steam=%llu local_steam=%llu members=%zu matched_generic_members=%zu custom_game_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu connect=%s",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.owner_steam_id),
        static_cast<unsigned long long>(local_member.steam_id),
        GBE_local_lobby.members.size(),
        matched_lobby.members.size(),
        static_cast<unsigned long long>(GBE_local_lobby.custom_game.game_id),
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id),
        GBE_local_lobby.connect.c_str()
    );

    std::string response_24;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed(GBE_local_lobby, GBE_GetDotaLobbyOwnerName(), response_24)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 24 cache update for 7044 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    std::string response_7113;
    if (send_join_response) {
        if (!gbe::gc_message::build_dota_practice_lobby_join_response_payload(has_request_job, request_job_id, 0u, response_7113)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7113 join response LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
            return true;
        }
    }

    gbe::dota_gc_router::DotaGcOutboundMessage outbound_24{};
    gbe::dota_gc_router::DotaGcOutboundMessage outbound_7113{};
    if (!gbe::dota_gc_router::build_outbound_message(
            GBE_kDotaCacheSubscribed,
            response_24,
            wrapped,
            outer_session_field_raw,
            settings->get_local_steam_id().ConvertToUint64(),
            GBE_kEMsgClientFromGC,
            GBE_kDotaAppId,
            outbound_24) ||
        (send_join_response && !gbe::dota_gc_router::build_outbound_message(
            GBE_kDotaPracticeLobbyJoinResponse,
            response_7113,
            wrapped,
            outer_session_field_raw,
            settings->get_local_steam_id().ConvertToUint64(),
            GBE_kEMsgClientFromGC,
            GBE_kDotaAppId,
            outbound_7113))) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building outbound 7044 responses LobbyID=%llu wrapped=%d", static_cast<unsigned long long>(GBE_local_lobby.lobby_id), wrapped ? 1 : 0);
        return true;
    }

    GBE_RecordDotaLobbyCacheSubscriptionState(response_24, wrapped ? "7044_join_wrapped" : "7044_join_direct");
    push_incoming_now(outbound_24.emsg, outbound_24.payload);
    GBE_LogDotaResponsePacket("7044_join_24", GBE_kDotaCacheSubscribed, wrapped, response_24, outbound_24.payload, GBE_local_lobby.lobby_id, GBE_local_lobby.state, GBE_local_lobby.game_state);
    if (send_join_response) {
        push_incoming_now(outbound_7113.emsg, outbound_7113.payload);
        GBE_LogDotaResponsePacket("7044_join_7113", GBE_kDotaPracticeLobbyJoinResponse, wrapped, response_7113, outbound_7113.payload, GBE_local_lobby.lobby_id, GBE_local_lobby.state, GBE_local_lobby.game_state);
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Joined practice lobby via 7044 LobbyID=%llu generic_lobby_id=%llu matched_generic=%u request_job=%llu has_job=%u pass_len=%zu wrapped=%d",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        matched_generic_lobby ? 1u : 0u,
        static_cast<unsigned long long>(request_job_id),
        has_request_job ? 1u : 0u,
        GBE_local_lobby.pass_key.size(),
        wrapped ? 1 : 0);
    return true;
}


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
    if ((!GBE_local_lobby.active || dota_lobby_id == 0 || !generic_lobby_id.IsLobby()) && GBE_shared_dota_lobby_state.valid) {
        dota_lobby_id = GBE_shared_dota_lobby_state.lobby_id;
        generic_lobby_id = CSteamID((uint64)GBE_shared_dota_lobby_state.generic_lobby_id);
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


bool Steam_Game_Coordinator::GBE_HandleDotaAbandonCurrentGameRequest(bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7035 because no local lobby is active");
        return true;
    }

    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    const uint32 lobby_state = GBE_local_lobby.state;
    const uint32 lobby_game_state = GBE_local_lobby.game_state;
    const bool treat_as_current_game_disconnect =
        is_server &&
        GBE_local_lobby.owner_connected &&
        lobby_state == 2u &&
        (GBE_local_lobby.server_id != 0 || lobby_game_state >= 1u);
    // Wrapped 7035 (user clicked Leave Game) can abandon at game_state >= 1
    // so players can leave during WAIT_FOR_PLAYERS_TO_LOAD if loading stalls.
    // Direct 7035 on a listen server (engine automatic state sync) requires
    // game_state >= 2 to avoid premature abandon during HERO_SELECTION.
    // Direct 7035 on a client (non-host) also uses game_state >= 1 because
    // the client has no engine-initiated 7035 -- it is always user-triggered.
    const uint32 abandon_game_state_threshold = (wrapped || !is_server) ? 1u : 2u;
    const bool ready_for_abandon_teardown =
        lobby_state == 2u &&
        lobby_game_state >= abandon_game_state_threshold;
    const bool arcade_launch_failed_before_connect =
        gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game) &&
        !wrapped &&
        !GBE_local_lobby.owner_connected &&
        lobby_state == 2u &&
        lobby_game_state >= 2u &&
        GBE_local_lobby.launch_phase >= GBE_kDotaLaunchPhaseRunQueued &&
        GBE_local_lobby.launch_phase < GBE_kDotaLaunchPhaseLoaded;
    if (arcade_launch_failed_before_connect && GBE_local_lobby.game_start_time != 0u) {
        const uint32 now = static_cast<uint32>(std::time(nullptr));
        if (now <= GBE_local_lobby.game_start_time + 5u) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Ignoring early arcade direct 7035 during launch grace window LobbyID=%llu state=%u game_state=%u launch_phase=%s start_time=%u now=%u",
                static_cast<unsigned long long>(lobby_id),
                lobby_state,
                lobby_game_state,
                GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase),
                GBE_local_lobby.game_start_time,
                now
            );
            return true;
        }
    }
    if (arcade_launch_failed_before_connect) {
        std::string response_25;
        if (!gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(lobby_id, response_25)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 payload for arcade launch failed 7035 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
            return true;
        }

        GBE_DiscardQueuedDotaLaunchMessagesForAbandon("7035_arcade_launch_failed_before_connect");
        GBE_pending_reset_after_cache_unsubscribed = true;
        GBE_pending_reset_after_cache_unsubscribed_lobby_id = lobby_id;
        GBE_MarkDotaAbandonedLobbySuppressed(lobby_id, "7035_arcade_launch_failed_before_connect");
        push_incoming_now(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25);

        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Treated arcade launch 7035 before connect as failed launch. queued 25 and skipped postgame LobbyID=%llu state=%u game_state=%u launch_phase=%s",
            static_cast<unsigned long long>(lobby_id),
            lobby_state,
            lobby_game_state,
            GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase)
        );
        return true;
    }
    if (!ready_for_abandon_teardown) {
        if (treat_as_current_game_disconnect) {
            std::string response_25;
            if (!gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(lobby_id, response_25)) {
                GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 payload for current-game 7035 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
                return true;
            }

            GBE_pending_reset_after_cache_unsubscribed = true;
            GBE_pending_reset_after_cache_unsubscribed_lobby_id = lobby_id;
            GBE_MarkDotaAbandonedLobbySuppressed(lobby_id, "7035_current_game_disconnect");
            push_incoming_now(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25);

            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Treated 7035 as current-game disconnect. queued 25 and deferred reset until retrieval LobbyID=%llu state=%u game_state=%u owner_connected=%u",
                static_cast<unsigned long long>(lobby_id),
                lobby_state,
                lobby_game_state,
                GBE_local_lobby.owner_connected ? 1u : 0u
            );
            return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Ignoring early 7035 before launch reaches a current-game stage LobbyID=%llu state=%u game_state=%u match_id=%llu server_id=%llu",
            static_cast<unsigned long long>(lobby_id),
            lobby_state,
            lobby_game_state,
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id)
        );
        return true;
    }

    if (wrapped && !outer_session_field_raw) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 7035 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    GBE_DiscardQueuedDotaLaunchMessagesForAbandon("7035_ready_for_abandon_teardown");
    GBE_MarkDotaAbandonedLobbySuppressed(lobby_id, "7035_ready_for_abandon_teardown");

    if (!GBE_QueueDotaPostGameTeardown("7035_abandon_current_game", wrapped, outer_session_field_raw, true, true, true))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Processed 7035. sent 25 and postgame 7010 wrapped=%d LobbyID=%llu",
        wrapped ? 1 : 0,
        static_cast<unsigned long long>(lobby_id)
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaGameMatchSignOutRequest(bool wrapped, const std::string *outer_session_field_raw, bool has_request_job, uint64 request_job_id)
{
    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    const uint64 match_id = GBE_local_lobby.match_id;
    const uint32 game_start_time = GBE_local_lobby.game_start_time;
    const uint32 signout_time = static_cast<uint32>(std::time(nullptr));
    const uint32 duration = game_start_time != 0u ? signout_time - game_start_time : 0u;

    std::string response_7005;
    if (!gbe::gc_message::build_dota_game_match_sign_out_response_payload(match_id, duration, signout_time, has_request_job, request_job_id, response_7005)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7005 signout response request_job=%llu has_job=%d", static_cast<unsigned long long>(request_job_id), has_request_job ? 1 : 0);
        return true;
    }

    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0) {
        if (GBE_local_lobby.game_state < 6u)
            GBE_local_lobby.game_state = 6u;
        if (GBE_local_lobby.state < 2u)
            GBE_local_lobby.state = 2u;
        GBE_PublishSharedDotaLobbyState("7004_signout_post_game");
        GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7004_signout_run_post_game");
    }

    if (!GBE_PushDotaResponse(GBE_kDotaGameMatchSignOutResponse, response_7005, wrapped, outer_session_field_raw, "7004_signout_response"))
        return true;

    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0) {
        GBE_QueueDotaPostGameTeardown("7004_signout_postgame", wrapped, outer_session_field_raw, false, false, false);
        GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7004_signout_postgame_state");

        std::string response_25;
        if (gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(lobby_id, response_25)) {
            GBE_PushDotaResponse(GBE_kDotaCacheUnsubscribed, response_25, wrapped, outer_session_field_raw, "25_after_7004");
            GBE_pending_dota_normal_signout_finalize_after_25 = true;
            GBE_pending_dota_normal_signout_finalize_lobby_id = lobby_id;
        } else {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 after 7004 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
        }
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Processed 7004 signout. replied 7005 and queued postgame teardown wrapped=%d LobbyID=%llu match_id=%llu request_job=%llu has_job=%d",
        wrapped ? 1 : 0,
        static_cast<unsigned long long>(lobby_id),
        static_cast<unsigned long long>(match_id),
        static_cast<unsigned long long>(request_job_id),
        has_request_job ? 1 : 0
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyLeaveRequest(bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7040 because no local lobby is active");
        return true;
    }

    if (GBE_local_lobby.state == 2u) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Ignoring stale 7040 during active runtime lobby LobbyID=%llu state=%u game_state=%u match_id=%llu server_id=%llu wrapped=%d",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id),
            wrapped ? 1 : 0
        );
        return true;
    }

    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    uint64 fallback_generic_lobby_id = 0ull;
    if (GBE_local_lobby.generic_lobby_id == 0ull) {
        CSteamID matched_generic_lobby_id = k_steamIDNil;
        GBE_LocalLobby matched_lobby{};
        bool matched_generic_lobby = GBE_FindDotaGenericLobbyByDotaLobbyId(lobby_id, matched_generic_lobby_id, &matched_lobby, "7040_leave");
        Steam_Client *steam_client = get_steam_client();
        if (!matched_generic_lobby && steam_client && steam_client->steam_matchmaking) {
            matched_generic_lobby_id = steam_client->steam_matchmaking->FindLobbyByDotaLobbyIdForInvite(
                lobby_id,
                GBE_kDotaGenericLobbyMarkerKey,
                GBE_kDotaGenericLobbyMarkerValue,
                GBE_kDotaGenericLobbyDotaLobbyIdKey);
            matched_generic_lobby = matched_generic_lobby_id.IsLobby();
        }
        if (matched_generic_lobby && matched_generic_lobby_id.IsLobby()) {
            fallback_generic_lobby_id = matched_generic_lobby_id.ConvertToUint64();
            GBE_local_lobby.generic_lobby_id = fallback_generic_lobby_id;
            GBE_SyncSettingsLobbyFromGenericLobby("7040_leave_local_find");
        }
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] 7040 leave fallback generic lookup LobbyID=%llu generic_lobby_id=%llu matched=%u members=%zu",
            static_cast<unsigned long long>(lobby_id),
            static_cast<unsigned long long>(fallback_generic_lobby_id),
            matched_generic_lobby ? 1u : 0u,
            matched_lobby.members.size()
        );
    }
    std::string response_25;
    if (!gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(lobby_id, response_25)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 payload for 7040 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
        return true;
    }

    GBE_MarkDotaAbandonedLobbySuppressed(lobby_id, "7040_leave");

    if (!GBE_PushDotaResponse(GBE_kDotaCacheUnsubscribed, response_25, wrapped, outer_session_field_raw, "7040_leave_25"))
        return true;
    ResetGCMemory("7040_leave", true, false);

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Lobby leave requested. sent 25 and left generic lobby LobbyID=%llu fallback_generic_lobby_id=%llu wrapped=%d",
        static_cast<unsigned long long>(lobby_id),
        static_cast<unsigned long long>(fallback_generic_lobby_id),
        wrapped ? 1 : 0
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyLaunchRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw, bool has_request_job, uint64 request_job_id)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7041 because no local lobby is active");
        return true;
    }

    if (wrapped && !outer_session_field_raw) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 7041 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    GBE_ResetDotaPracticeLobbyLaunchPeripheralState();

    const uint32 launch_ip = network ? network->getOwnIP() : 0u;
    const gbe::dota_lobby_state::LaunchInitPlan launch_plan = gbe::dota_lobby_state::compose_launch_init_plan(
        GBE_local_lobby,
        GBE_GenerateDotaMatchId(),
        gbe::dota_custom_game::derive_practice_lobby_ip_server_id(launch_ip),
        gbe::proto_wire::format_dota_practice_lobby_connect_from_ip(launch_ip),
        static_cast<uint32>(std::time(nullptr)),
        GBE_kDotaLaunchPhaseRequested);
    GBE_local_lobby = launch_plan.lobby;
    GBE_PublishSharedDotaLobbyState("7041_launch_init");

    if (gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game)) {
        const gbe::dota_lobby_state::LaunchPresenceEvent presence_event = gbe::dota_lobby_state::compose_launch_serversetup_presence_event("7041_custom_game_launch_init");
        if (presence_event.update) {
            GBE_UpdateDotaPracticeLobbyLaunchRichPresence(presence_event.status.c_str(), presence_event.lobby_state.c_str(), presence_event.include_party);
            GBE_MaybeQueueDotaPracticeLobbyLaunchPersonaState(presence_event.status.c_str(), presence_event.lobby_state.c_str(), presence_event.include_party, presence_event.include_lobby, presence_event.persona_reason.c_str());
        }

        if (GBE_SendDotaCustomGameLaunchSetupFlow(wrapped, outer_session_field_raw, has_request_job, request_job_id)) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Deferred custom game RUN until 8052 after 7041 LobbyID=%llu match_id=%llu server_id=%llu custom_id=%llu custom_map=%s",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(GBE_local_lobby.match_id),
                static_cast<unsigned long long>(GBE_local_lobby.server_id),
                static_cast<unsigned long long>(GBE_local_lobby.custom_game.game_id),
                GBE_local_lobby.custom_game.map_name.c_str()
            );
            return true;
        }
    }

    const gbe::dota_lobby_state::PracticeLobbyLaunchEventPlan event_plan = gbe::dota_lobby_state::compose_practice_lobby_launch_event_plan(GBE_kDotaPracticeLobbyDetailsUpdate);
    std::string stage1_message;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(GBE_local_lobby, GBE_local_lobby.owner_name, stage1_message, true)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed building initial 26 after 7041 LobbyID=%llu match_id=%llu server_id=%llu connect=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id),
            GBE_local_lobby.connect.c_str()
        );
        return true;
    }

    std::string wrapped_stage1_message;
    if (!event_plan.initial_details.send || !GBE_PushDotaResponse(event_plan.initial_details.emsg, stage1_message, wrapped, outer_session_field_raw, event_plan.initial_details.reason.c_str(), event_plan.initial_details.apply_lobby_state, event_plan.initial_details.lobby_state, event_plan.initial_details.lobby_game_state, &wrapped_stage1_message))
        return true;
    if (wrapped)
        stage1_message.swap(wrapped_stage1_message);

    if (event_plan.steam_auth_ack.queue)
        GBE_MaybeQueueDotaPracticeLobbySteamAuthAck(event_plan.steam_auth_ack.reason.c_str(), has_request_job ? request_job_id : 0ull);

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Sent initial 26 after 7041 path=%s has_request_job=%d request_job=%llu LobbyID=%llu match_id=%llu server_id=%llu game_start=%u connect=%s size=%zu body_prefix=%s",
        wrapped ? "wrapped" : "direct",
        has_request_job ? 1 : 0,
        static_cast<unsigned long long>(request_job_id),
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id),
        GBE_local_lobby.game_start_time,
        GBE_local_lobby.connect.c_str(),
        stage1_message.size(),
        gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(stage1_message.data()), stage1_message.size(), 32).c_str()
    );

    if (event_plan.presence.update) {
        GBE_UpdateDotaPracticeLobbyLaunchRichPresence(event_plan.presence.status.c_str(), event_plan.presence.lobby_state.c_str(), event_plan.presence.include_party);
        GBE_MaybeQueueDotaPracticeLobbyLaunchPersonaState(event_plan.presence.status.c_str(), event_plan.presence.lobby_state.c_str(), event_plan.presence.include_party, event_plan.presence.include_lobby, event_plan.presence.persona_reason.c_str());
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Deferring remaining 7041 launch follow-ups until server_id sync LobbyID=%llu match_id=%llu server_id=%llu",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id)
    );

    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbySetDetailsRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7046 because no local lobby is active");
        return true;
    }

    GBE_DotaPracticeLobbyDetailsRequest request{};
    if (!gbe::proto_wire::parse_dota_practice_lobby_set_details_body(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7046 body_size=%zu body_prefix=%s",
            request_body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    if (request.has_lobby_id && request.lobby_id != GBE_local_lobby.lobby_id) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] 7046 LobbyID mismatch request=%llu local=%llu, keeping local state",
            static_cast<unsigned long long>(request.lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id)
        );
    }

    if (request.has_room_name)
        GBE_local_lobby.room_name = request.room_name;
    if (request.has_server_region)
        GBE_local_lobby.server_region = request.server_region;
    if (request.has_lan)
        GBE_local_lobby.lan = request.lan;
    if (request.has_lan_host_ping_location)
        GBE_local_lobby.lan_host_ping_location = request.lan_host_ping_location;
    if (request.has_game_mode)
        GBE_local_lobby.game_mode = request.game_mode;
    if (request.has_bot_difficulty_radiant)
        GBE_local_lobby.bot_difficulty_radiant = request.bot_difficulty_radiant;
    if (request.has_allow_cheats)
        GBE_local_lobby.allow_cheats = request.allow_cheats;
    if (request.has_fill_with_bots)
        GBE_local_lobby.fill_with_bots = request.fill_with_bots;
    if (request.has_allow_spectating)
        GBE_local_lobby.allow_spectating = request.allow_spectating;
    if (request.has_pass_key)
        GBE_local_lobby.pass_key = request.pass_key;
    if (request.has_visibility)
        GBE_local_lobby.visibility = request.visibility;
    if (request.has_bot_difficulty_dire)
        GBE_local_lobby.bot_difficulty_dire = request.bot_difficulty_dire;
    if (request.has_bot_radiant)
        GBE_local_lobby.bot_radiant = request.bot_radiant;
    if (request.has_bot_dire)
        GBE_local_lobby.bot_dire = request.bot_dire;
    GBE_ApplyDotaCustomGameDetailsRequest(request, GBE_local_lobby.custom_game);
    GBE_NormalizeDotaCustomGameDetailsFromInstalledMod(settings, GBE_local_lobby.custom_game);
    GBE_NormalizeDotaArcadeLobbyMemberSlots(GBE_local_lobby);
    GBE_PublishDotaPracticeLobbyLocalMemberData("7046_set_details");
    GBE_PublishSharedDotaLobbyState("7046_set_details");
    GBE_PublishDotaPracticeLobbyMetadata("7046_set_details");

    if (!GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7046"))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Room details updated. mode=%u server_region=%u lan=%u lan_ping=%s cheats=%u bots=%u spectating=%u visibility=%u bot_diff_r=%u bot_diff_d=%u bot_radiant=%llu bot_dire=%llu name=%s password_len=%zu",
        GBE_local_lobby.game_mode,
        GBE_local_lobby.server_region,
        GBE_local_lobby.lan ? 1u : 0u,
        GBE_local_lobby.lan_host_ping_location.c_str(),
        GBE_local_lobby.allow_cheats ? 1u : 0u,
        GBE_local_lobby.fill_with_bots ? 1u : 0u,
        GBE_local_lobby.allow_spectating ? 1u : 0u,
        GBE_local_lobby.visibility,
        GBE_local_lobby.bot_difficulty_radiant,
        GBE_local_lobby.bot_difficulty_dire,
        static_cast<unsigned long long>(GBE_local_lobby.bot_radiant),
        static_cast<unsigned long long>(GBE_local_lobby.bot_dire),
        GBE_local_lobby.room_name.c_str(),
        GBE_local_lobby.pass_key.size()
    );
    return true;
}


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
        if (!GBE_PushDotaResponse(GBE_kDotaPracticeLobbyResponse, response_7055, wrapped, outer_session_field_raw, "7047_7055", false, 0u, 0u, &wrapped_7055))
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


bool Steam_Game_Coordinator::GBE_HandleDotaDestroyLobbyRequest(uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 8246 because no local lobby is active");
        return true;
    }

    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    std::string response_25;
    if (!gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(lobby_id, response_25)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 payload for 8246 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
        return true;
    }

    if (wrapped && !outer_session_field_raw) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 8246 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
        return true;
    }

    std::string response_8247;
    if (has_request_job) {
        if (!gbe::gc_message::build_dota_destroy_lobby_response_payload(request_job_id, response_8247)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 8247 payload for LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
            return true;
        }
    }

    gbe::dota_gc_router::DotaGcOutboundMessage outbound_25{};
    gbe::dota_gc_router::DotaGcOutboundMessage outbound_8247{};
    if (!gbe::dota_gc_router::build_outbound_message(
            GBE_kDotaCacheUnsubscribed,
            response_25,
            wrapped,
            outer_session_field_raw,
            settings->get_local_steam_id().ConvertToUint64(),
            GBE_kEMsgClientFromGC,
            GBE_kDotaAppId,
            outbound_25) ||
        (has_request_job && !gbe::dota_gc_router::build_outbound_message(
            GBE_kDotaDestroyLobbyResponse,
            response_8247,
            wrapped,
            outer_session_field_raw,
            settings->get_local_steam_id().ConvertToUint64(),
            GBE_kEMsgClientFromGC,
            GBE_kDotaAppId,
            outbound_8247))) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building outbound 8246 responses LobbyID=%llu wrapped=%d", static_cast<unsigned long long>(lobby_id), wrapped ? 1 : 0);
        return true;
    }

    ResetGCMemory("8246_destroy", true, true);
    push_incoming_now(outbound_25.emsg, outbound_25.payload);
    GBE_LogDotaResponsePacket("8246_destroy_25", GBE_kDotaCacheUnsubscribed, wrapped, response_25, outbound_25.payload, lobby_id, 0u, 0u);
    if (has_request_job) {
        push_incoming_now(outbound_8247.emsg, outbound_8247.payload);
        GBE_LogDotaResponsePacket("8246_destroy_8247", GBE_kDotaDestroyLobbyResponse, wrapped, response_8247, outbound_8247.payload, lobby_id, 0u, 0u);
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Lobby destroyed. unsubscribed LobbyID=%llu wrapped=%d request_job=%llu has_job=%d",
        static_cast<unsigned long long>(lobby_id),
        wrapped ? 1 : 0,
        static_cast<unsigned long long>(request_job_id),
        has_request_job ? 1 : 0
    );
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaAddSocketRequest(const uint8 *body, size_t body_size, bool has_request_job, uint64 request_job_id)
{
    if (is_server || !body || body_size == 0)
        return false;

    uint64 field1_item_id = 0;
    uint64 field2_item_id = 0;
    uint64 subject_item_id = 0;
    uint64 tool_item_id = 0;
    uint32 socket_index = 0;

    size_t pos = 0;
    while (pos < body_size) {
        gbe::proto_wire::Field field{};
        size_t field_offset = 0;
        size_t field_end = 0;
        if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end))
            break;

        if (field.wire_type != 0u)
            continue;

        uint64 value = 0;
        size_t tmp = field.value_offset;
        if (!gbe::proto_wire::read_varuint(body, body_size, tmp, value))
            continue;

        if (field.number == 1u)
            field1_item_id = value;
        else if (field.number == 2u)
            field2_item_id = value;
        else if (field.number == 3u)
            socket_index = static_cast<uint32>(value);
    }

    tool_item_id = field1_item_id;
    subject_item_id = field2_item_id;

    Econ_Item *subject_item = nullptr;
    bool found_tool = (tool_item_id == 0);
    for (Econ_Item &item : items) {
        if (item.id == subject_item_id)
            subject_item = &item;
        if (item.id == tool_item_id)
            found_tool = true;
    }

    if (!subject_item && field1_item_id != 0) {
        for (Econ_Item &item : items) {
            if (item.id == field1_item_id) {
                subject_item = &item;
                subject_item_id = field1_item_id;
                tool_item_id = field2_item_id;
                found_tool = (tool_item_id == 0);
                for (const Econ_Item &tool_candidate : items) {
                    if (tool_candidate.id == tool_item_id) {
                        found_tool = true;
                        break;
                    }
                }
                break;
            }
        }
    }

    uint32 result = 0u;
    uint32 socket_attr_def = 0u;
    if (!subject_item) {
        result = 1u;
    } else {
        static const uint32 kKnownDotaEmptySocketAttrs[] = { 179u, 180u, 181u, 182u, 183u, 184u, 185u, 186u };

        for (uint32 known_attr : kKnownDotaEmptySocketAttrs) {
            bool used = false;
            for (const Econ_Item_Attribute &attr : subject_item->attributes) {
                if (attr.def == known_attr) {
                    used = true;
                    break;
                }
            }
            if (!used) {
                socket_attr_def = known_attr;
                break;
            }
        }

        if (socket_attr_def == 0u) {
            result = 1u;
        } else {
            Econ_Item_Attribute socket_attr{};
            socket_attr.def = socket_attr_def;
            socket_attr.type = Econ_Item_Attribute::ATTR_TYPE_STRING;
            socket_attr.value = 0.0f;
            std::string socket_payload;
            gbe::proto_wire::append_varint_field(socket_payload, 1u, subject_item_id);
            gbe::proto_wire::append_varint_field(socket_payload, 2u, socket_attr_def);
            gbe::proto_wire::append_bytes_field(socket_attr.value_bytes, 1u, socket_payload);
            subject_item->attributes.push_back(socket_attr);

            save_items_to_file();
            callback_item_updated(settings->get_local_steam_id(), *subject_item);
        }
    }

    std::string response_body;
    gbe::gc_message::build_dota_add_socket_response_body(result, subject_item_id, socket_attr_def, response_body);

    std::string response_message;
    gbe::gc_message::build_dota_job_reply_or_zero_header_payload(GBE_kDotaAddSocketResponse, has_request_job, request_job_id, response_body, response_message);
    push_incoming_now(GBE_kDotaAddSocketResponse | GBE_kProtoMask, response_message);

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "add socket req=1087 resp=1090 result=%u subject_item=0x%llx tool_item=0x%llx found_subject=%u found_tool=%u socket_index=%u attr_def=%u source_job=%llu size=%zu",
        result,
        static_cast<unsigned long long>(subject_item_id),
        static_cast<unsigned long long>(tool_item_id),
        subject_item ? 1u : 0u,
        found_tool ? 1u : 0u,
        socket_index,
        socket_attr_def,
        static_cast<unsigned long long>(request_job_id),
        response_message.size()
    );

    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaWrappedPostLoginRequest(const void *pubData, uint32 cubData)
{
    gbe::dota_gc_router::DotaGcRequestContext route_context{};
    if (!gbe::dota_gc_router::extract_wrapped_post_login_request(pubData, cubData, GBE_kEMsgClientToGC, route_context))
        return false;

    if (!gbe::gc_message::is_supported_dota_wrapped_post_login_request(route_context.inner_emsg))
        return false;

    if (GBE_DispatchDotaPostLoginRequest(route_context))
        return true;

    GBE_DotaWrappedDirectContext context{};
    context.valid = route_context.valid;
    context.inner_emsg = route_context.inner_emsg;
    context.outer_session_field_raw = route_context.outer_session_field_raw;
    context.inner_body_raw = route_context.body;
    context.request_job_id = route_context.request_job_id;
    context.has_request_job = route_context.has_request_job;

    if (context.inner_emsg == GBE_kDotaAbandonCurrentGame) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 7035 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaAbandonCurrentGameRequest(true, &context.outer_session_field_raw);
    }

    if (context.inner_emsg == GBE_kDotaGameMatchSignOut) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 7004 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaGameMatchSignOutRequest(true, &context.outer_session_field_raw, context.has_request_job, context.request_job_id);
    }

    if (context.inner_emsg == 7070u) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 7070 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game)) {
            uint32 ready_state = 0u;
            gbe::proto_wire::read_uint32_field(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 1u, ready_state);
            std::string response_7170;
            if (gbe::gc_message::build_dota_ready_up_status_payload(context.has_request_job, context.request_job_id, GBE_local_lobby.lobby_id, 0u, ready_state != 0u ? ready_state : 1u, response_7170)) {
                GBE_PushDotaResponse(7170u, response_7170, true, &context.outer_session_field_raw, "7070_ready_up_status");
            }

            if (ready_state == 1u && GBE_local_lobby.state == 2u && GBE_local_lobby.game_state < 1u && GBE_local_lobby.launch_phase >= GBE_kDotaLaunchPhaseRunQueued) {
                GBE_local_lobby.game_state = 1u;
                GBE_PublishSharedDotaLobbyState("7070_wrapped_custom_game_ready_up_run_ack");
                GBE_SendDotaPracticeLobbyDetailsUpdate(true, &context.outer_session_field_raw, "7070_wrapped_custom_game_ready_up_run_ack");
            }
        }
        return true;
    }

    if (context.inner_emsg == 8052u) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 8052 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game)) {
            const uint8 *body = reinterpret_cast<const uint8 *>(context.inner_body_raw.data());
            const size_t body_size = context.inner_body_raw.size();
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
                if (!GBE_TryAdvanceDotaLaunchToRun("custom game wrapped 8052 started loading", context.inner_emsg, context.request_job_id, "8052_wrapped_started_loading", 0u)) {
                    GBE_PublishSharedDotaLobbyState("8052_wrapped_started_loading");
                    GBE_SendDotaPracticeLobbyDetailsUpdate(true, &context.outer_session_field_raw, "8052_wrapped_started_loading");
                }
            }
        }
        return true;
    }

    if (context.inner_emsg == 8053u) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 8053 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game)) {
            const uint8 *body = reinterpret_cast<const uint8 *>(context.inner_body_raw.data());
            const size_t body_size = context.inner_body_raw.size();
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
                const char *reason = load_failed ? "8053_wrapped_load_failed" : "8053_wrapped_finished_loading";
                if (!load_failed)
                    GBE_MarkDotaLaunchPhase(GBE_kDotaLaunchPhaseLoaded, reason);
                GBE_PublishSharedDotaLobbyState(reason);
                GBE_SendDotaPracticeLobbyDetailsUpdate(true, &context.outer_session_field_raw, reason);
                GBE_GC_DebugLog(
                    "GC_DOTA_LOBBY",
                    "[LOBBY] Applied wrapped 8053 lobby_id=%llu loading_duration=%llu result_code=%llu signon_states=%llu load_failed=%u result_text=%s",
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

    if (context.inner_emsg == GBE_kDotaLeaveChatChannel) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 7272 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaLeaveChatChannelRequest(
            context.inner_body_raw,
            true,
            &context.outer_session_field_raw
        );
    }

    if (context.inner_emsg == GBE_kDotaDestroyLobbyRequest) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 8246 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaDestroyLobbyRequest(
            context.request_job_id,
            context.has_request_job,
            true,
            &context.outer_session_field_raw
        );
    }

    if (context.inner_emsg == GBE_kDotaFindTopSourceTVGames) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[WATCH] Received wrapped 8009 FindTopSourceTVGames has_job=%d request_job=%llu",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id)
        );

        // Build CMsgGCToClientFindTopSourceTVGamesResponse (8010)
        // If there's an active LAN game with spectating enabled, include it.
        std::string response_body;
        bool found_game_w = false;
        gbe::gc_message::DotaSourceTVGame source_tv_game_w{};

        // First: check local shared lobby state (we are the host)
        if (GBE_shared_dota_lobby_state.valid &&
            GBE_shared_dota_lobby_state.active &&
            GBE_shared_dota_lobby_state.game_state >= 1u &&
            GBE_shared_dota_lobby_state.server_id != 0) {

            source_tv_game_w.start_time = GBE_shared_dota_lobby_state.game_start_time != 0
                ? GBE_shared_dota_lobby_state.game_start_time
                : static_cast<uint32>(std::time(nullptr) - 300);
            source_tv_game_w.server_id = GBE_shared_dota_lobby_state.server_id;
            source_tv_game_w.lobby_id = GBE_shared_dota_lobby_state.lobby_id;
            int32 game_time = GBE_shared_dota_lobby_state.game_start_time != 0
                ? static_cast<int32>(std::time(nullptr)) - static_cast<int32>(GBE_shared_dota_lobby_state.game_start_time)
                : 300;
            source_tv_game_w.game_time = static_cast<uint32>(game_time);
            source_tv_game_w.game_mode = GBE_shared_dota_lobby_state.game_mode;
            source_tv_game_w.match_id = GBE_shared_dota_lobby_state.match_id;
            for (const auto &member : GBE_shared_dota_lobby_state.members) {
                if (member.account_id == 0) continue;
                source_tv_game_w.players.push_back(gbe::gc_message::DotaSourceTVPlayer{member.account_id, member.hero_id, member.slot, member.team});
            }
            found_game_w = true;
        }

        // Second: check remote generic lobbies via matchmaking
        if (!found_game_w) {
            const std::vector<GBE_LocalLobby> snapshots = GBE_GetDotaGenericLobbySnapshots("8009_wrapped_find_top_source_tv");
            for (const auto &snap : snapshots) {
                if (snap.game_state >= 1u && snap.server_id != 0 && snap.allow_spectating) {
                    source_tv_game_w.start_time = snap.game_start_time != 0
                        ? snap.game_start_time
                        : static_cast<uint32>(std::time(nullptr) - 300);
                    source_tv_game_w.server_id = snap.server_id;
                    source_tv_game_w.lobby_id = snap.lobby_id;
                    int32 gt = snap.game_start_time != 0
                        ? static_cast<int32>(std::time(nullptr)) - static_cast<int32>(snap.game_start_time)
                        : 300;
                    source_tv_game_w.game_time = static_cast<uint32>(gt);
                    source_tv_game_w.game_mode = snap.game_mode;
                    source_tv_game_w.match_id = snap.match_id;
                    for (const auto &member : snap.members) {
                        if (member.account_id == 0) continue;
                        source_tv_game_w.players.push_back(gbe::gc_message::DotaSourceTVPlayer{member.account_id, member.hero_id, member.slot, member.team});
                    }
                    found_game_w = true;
                    break;
                }
            }
        }

        if (found_game_w)
            gbe::gc_message::build_dota_find_top_source_tv_games_body(&source_tv_game_w, response_body);
        else
            gbe::gc_message::build_dota_find_top_source_tv_games_empty_body(response_body);

        std::string response_message;
        gbe::gc_message::build_dota_job_reply_or_zero_header_payload(
            GBE_kDotaFindTopSourceTVGamesResponse,
            context.has_request_job, context.request_job_id,
            response_body, response_message);

        if (!GBE_PushDotaResponse(GBE_kDotaFindTopSourceTVGamesResponse, response_message, true, &context.outer_session_field_raw, "8010_watch_response"))
            return true;

        GBE_GC_DebugLog("GC_DOTA_LOBBY",
            "[WATCH] replied 8010 FindTopSourceTVGamesResponse found=%d local_valid=%d",
            found_game_w ? 1 : 0,
            GBE_shared_dota_lobby_state.valid ? 1 : 0);
        return true;
    }

    if (context.inner_emsg == 7091u) {
        // CMsgWatchGame (wrapped) -> CMsgWatchGameResponse
        // Parse server_steamid from inner body (field 1, fixed64).
        uint64 watch_server_steamid = 0;
        {
            const uint8 *wbody = reinterpret_cast<const uint8 *>(context.inner_body_raw.data());
            size_t wbody_size = context.inner_body_raw.size();
            size_t pos = 0;
            while (pos < wbody_size) {
                gbe::proto_wire::Field field{};
                size_t field_offset = 0;
                size_t field_end = 0;
                if (!gbe::proto_wire::read_next_field(wbody, wbody_size, pos, field, &field_offset, &field_end))
                    break;
                if (field.number == 1u && field.wire_type == 1u && field.value_size == 8)
                    memcpy(&watch_server_steamid, wbody + field.value_offset, 8);
            }
        }

        // Parse SourceTV address from lobby connect string
        uint32 source_tv_addr = 0;
        uint32 source_tv_port = 27020;
        uint64 tv_secret_code_w = 0;
        std::string connect_str_w;
        if (GBE_shared_dota_lobby_state.valid && !GBE_shared_dota_lobby_state.connect.empty()) {
            connect_str_w = GBE_shared_dota_lobby_state.connect;
        } else {
            const std::vector<GBE_LocalLobby> snapshots = GBE_GetDotaGenericLobbySnapshots("7091_wrapped_watch_game");
            for (const auto &snap : snapshots) {
                if (snap.game_state >= 1u && snap.server_id != 0 && !snap.connect.empty()) {
                    connect_str_w = snap.connect;
                    if (snap.tv_secret_code != 0)
                        tv_secret_code_w = snap.tv_secret_code;
                    if (snap.tv_port != 0)
                        source_tv_port = snap.tv_port;
                    break;
                }
            }
        }
        if (tv_secret_code_w == 0 && GBE_local_lobby.tv_secret_code != 0)
            tv_secret_code_w = GBE_local_lobby.tv_secret_code;
        if (GBE_local_lobby.tv_port != 0)
            source_tv_port = GBE_local_lobby.tv_port;
        if (!connect_str_w.empty()) {
            size_t colon = connect_str_w.find(':');
            std::string ip_str = (colon != std::string::npos) ? connect_str_w.substr(0, colon) : connect_str_w;
            if (colon != std::string::npos) {
                uint32 game_port = static_cast<uint32>(std::strtoul(connect_str_w.c_str() + colon + 1, nullptr, 10));
                if (game_port > 0) source_tv_port = game_port + 5;
            }
            unsigned int a = 0, b = 0, c = 0, d = 0;
            if (std::sscanf(ip_str.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4)
                source_tv_addr = (a << 24) | (b << 16) | (c << 8) | d;
        }

        // Build PENDING response
        std::string pending_body;
        gbe::gc_message::build_dota_watch_game_pending_response_body(pending_body);
        std::string pending_inner;
        gbe::gc_message::build_dota_job_reply_or_zero_header_payload(7092u, context.has_request_job, context.request_job_id, pending_body, pending_inner);
        GBE_PushDotaResponse(7092u, pending_inner, true, &context.outer_session_field_raw, "7091_watch_pending");

        const CSteamID local_steam_id = settings->get_local_steam_id();
        const uint32 local_account_id = local_steam_id.GetAccountID();

        // Build READY response
        uint64 secret_code = (tv_secret_code_w != 0) ? tv_secret_code_w : (watch_server_steamid ^ 0x0514D449EDC24001ULL);
        std::string ready_body;
        gbe::gc_message::build_dota_watch_game_ready_response_body(source_tv_addr, source_tv_port, watch_server_steamid, secret_code, ready_body);
        std::string ready_inner;
        gbe::gc_message::build_dota_job_reply_or_zero_header_payload(7092u, false, 0, ready_body, ready_inner);
        GBE_PushDotaResponse(7092u, ready_inner, true, &context.outer_session_field_raw, "7091_watch_ready");

        GBE_GC_DebugLog("GC_DOTA_LOBBY",
            "[WATCH] wrapped WatchGame -> READY server=0x%llx tv_addr=0x%x tv_port=%u local_account=%u local_steamid=%llu raw_tv_secret=0x%llx sent_secret=0x%llx",
            static_cast<unsigned long long>(watch_server_steamid),
            source_tv_addr,
            source_tv_port,
            local_account_id,
            static_cast<unsigned long long>(local_steam_id.ConvertToUint64()),
            static_cast<unsigned long long>(tv_secret_code_w),
            static_cast<unsigned long long>(secret_code));
        return true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Received wrapped 7047 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
        context.has_request_job ? 1 : 0,
        static_cast<unsigned long long>(context.request_job_id),
        context.outer_session_field_raw.size(),
        gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
    );

    return GBE_HandleDotaPracticeLobbySetTeamSlotRequest(
        context.inner_body_raw,
        context.request_job_id,
        context.has_request_job,
        true,
        &context.outer_session_field_raw
    );
}
