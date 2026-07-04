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

// Pure lobby payload composition helpers for the Dota Game Coordinator.
// Extracted from gbe_dota_gc_payload_helpers.cpp (Phase 3.2.4) as part of the
// payload-helper split. This TU owns the lobby cache-subscribed / details-update
// / launch replay payload composition functions plus the hello/server-hello
// context extractors and welcome builders. All functions are pure with respect
// to coordinator/global state: they take context structs (GBE_DotaHelloContext,
// GBE_DotaServerHelloContext, GBE_DotaLobbyMemberState, GBE_DotaCustomGameDetails)
// and caller-provided output parameters, with no dependency on
// Steam_Game_Coordinator, Steam_Client, Settings, or GBE_shared_dota_lobby_state.
//
// Responsibility boundary:
//   - compose SO objects for practice-lobby cache-subscribed payloads
//   - adapt practice-lobby cache-subscribed / details-update payloads from
//     wrapped templates (rewrite lobby_id / steam_id / runtime fields)
//   - replay practice-lobby launch cache-subscribed templates with full
//     runtime field rewriting
//   - extract hello / server-hello contexts from GC message bodies
//   - build direct client-welcome / server-welcome messages
//   - compose the full Dota client welcome (inner body + outer envelope)
//   - replay the official 032 practice-lobby 26 payload (full canned replay)
//
// Migration note: this extraction is part of Phase 3.2's payload-helper split.
// Combined with the earlier pure item TU (gbe_dota_payload_item_helpers.cpp)
// and the pure wire TU (gbe_dota_payload_wire_helpers.cpp), it shrinks
// gbe_dota_gc_payload_helpers.cpp toward the 1200-line target (Phase 3.2.5) and
// lets the offline test harness compile the real lobby logic against stub
// protobuf types instead of duplicating implementations.
//
// All 15 functions in this TU were already external-linkage in the original
// file (no file-scope static promotion needed); their declarations already
// live in gbe_dota_gc_internal.h. The shared extern byte arrays
// (GBE_kOldDotaAccountIdVarint, GBE_kOldDotaSteamIdVarint, etc.) stay in the
// original TU because they are used by both wire and lobby functions. This TU
// references them via the extern declarations in gbe_dota_gc_internal.h.
//
// The functions in this TU call into the pure wire helpers
// (GBE_PatchDotaLobbyTemplateIdentifiers, GBE_ForceDotaLobbyCacheOwnerSOID,
// GBE_PrepareDotaWelcomeBody, GBE_PrepareDotaDirectReplayMessage, etc.) which
// now live in gbe_dota_payload_wire_helpers.cpp; those calls resolve via the
// extern declarations in gbe_dota_gc_internal.h at link time.

#include "gbe_dota_gc_internal.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_request_router.h"
#include "gbe_proto_buf_header.h"
#include "gbe_proto_wire.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_gc_wire.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_gc_message_utils.h"

#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <steam/steamclientpublic.h>
#include <steammessages.pb.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/gcsystemmsgs.pb.h>

using namespace gamecoordinator::tf2;

void GBE_ComposeDotaPracticeLobbySOObjects(
    uint64 steam_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    const GBE_DotaCustomGameDetails *custom_game,
    gbe::gc_message::DotaPracticeLobbyObjects &lobby_objects)
{
    static const uint8 GBE_kDotaLobbyField62Value[] = { 0x08, 0xF5, 0x44, 0x12, 0x02, 0x08, 0x00 };

    const bool has_custom_game = custom_game && custom_game->game_id != 0ull;
    std::vector<GBE_DotaLobbyMemberState> effective_members = gbe::dota_lobby_flow::compose_lobby_members(
        steam_id,
        extra_startup_account_id,
        owner_team,
        owner_slot,
        owner_hero_id,
        true,
        GBE_kDotaTeamPlayerPool,
        members);
    if (has_custom_game) {
        effective_members.erase(
            std::remove_if(effective_members.begin(), effective_members.end(), [](const GBE_DotaLobbyMemberState &member) { return member.steam_id == 0ull; }),
            effective_members.end());
    }

    const std::string normalized_connect = gbe::dota_custom_game::format_practice_lobby_connect_for_custom_game(connect, custom_game);

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Building 2004 team details lobby_id=%llu state=%u game_state=%u custom_game_id=%llu team_details=%u",
        static_cast<unsigned long long>(lobby_id),
        lobby_state,
        lobby_game_state,
        static_cast<unsigned long long>(has_custom_game ? custom_game->game_id : 0ull),
        lobby_state != 0u ? 2u : 0u
    );

    gbe::gc_message::DotaCustomGameDetails parsed_custom_game{};
    const gbe::gc_message::DotaCustomGameDetails *parsed_custom_game_ptr = nullptr;
    if (custom_game) {
        parsed_custom_game.mode = custom_game->mode;
        parsed_custom_game.map_name = custom_game->map_name;
        parsed_custom_game.difficulty = custom_game->difficulty;
        parsed_custom_game.game_id = custom_game->game_id;
        parsed_custom_game.min_players = custom_game->min_players;
        parsed_custom_game.max_players = custom_game->max_players;
        parsed_custom_game.crc = custom_game->crc;
        parsed_custom_game.timestamp = custom_game->timestamp;
        parsed_custom_game.penalties = custom_game->penalties;
        parsed_custom_game_ptr = &parsed_custom_game;
    }

    std::vector<gbe::gc_message::DotaLobbyMemberObjectState> parsed_members;
    parsed_members.reserve(effective_members.size());
    std::vector<gbe::gc_message::DotaStaticLobbyMember> static_lobby_members;
    static_lobby_members.reserve(effective_members.size());
    for (const GBE_DotaLobbyMemberState &member : effective_members) {
        parsed_members.push_back(gbe::gc_message::DotaLobbyMemberObjectState{
            member.steam_id,
            member.team,
            member.slot,
            member.hero_id,
            member.connected,
            member.leaver_status});
        static_lobby_members.push_back(gbe::gc_message::DotaStaticLobbyMember{
            member.steam_id,
            member.account_id != 0u ? member.account_id : (member.steam_id != 0ull ? CSteamID((uint64)member.steam_id).GetAccountID() : 0u),
            member.connected});
    }

    gbe::gc_message::DotaPracticeLobbyObjectOptions lobby_object_options{};
    lobby_object_options.extra_startup_account_id = extra_startup_account_id;
    lobby_object_options.steam_id = steam_id;
    lobby_object_options.game_mode = game_mode;
    lobby_object_options.is_custom_game = has_custom_game;
    lobby_object_options.player_name = player_name;
    lobby_object_options.lobby_members = std::move(parsed_members);
    lobby_object_options.static_lobby_members = std::move(static_lobby_members);

    gbe::gc_message::DotaLobbyObject2004Options &object_2004_options = lobby_object_options.object_2004_options;
    object_2004_options.steam_id = steam_id;
    object_2004_options.lobby_id = lobby_id;
    object_2004_options.lobby_state = lobby_state;
    object_2004_options.lobby_game_state = lobby_game_state;
    object_2004_options.server_id = server_id;
    object_2004_options.match_id = match_id;
    object_2004_options.game_start_time = game_start_time;
    if (lobby_state == 3u && lobby_game_state == 6u && game_start_time != 0u) {
        const uint32 now = static_cast<uint32>(std::time(nullptr));
        object_2004_options.elapsed_game_time = now > game_start_time ? (now - game_start_time) : 0u;
    }
    object_2004_options.connect = normalized_connect;
    object_2004_options.room_name = room_name;
    object_2004_options.game_mode = game_mode;
    object_2004_options.server_region = server_region;
    object_2004_options.lan = lan;
    object_2004_options.lan_host_ping_location = lan_host_ping_location;
    object_2004_options.allow_cheats = allow_cheats;
    object_2004_options.fill_with_bots = fill_with_bots;
    object_2004_options.allow_spectating = allow_spectating;
    object_2004_options.visibility = visibility;
    object_2004_options.bot_difficulty_radiant = bot_difficulty_radiant;
    object_2004_options.bot_difficulty_dire = bot_difficulty_dire;
    object_2004_options.bot_radiant = bot_radiant;
    object_2004_options.bot_dire = bot_dire;
    object_2004_options.has_broadcast_channel = has_broadcast_channel;
    object_2004_options.broadcast_channel_id = broadcast_channel_id;
    object_2004_options.broadcast_country_code = broadcast_country_code;
    object_2004_options.broadcast_description = broadcast_description;
    object_2004_options.broadcast_language_code = broadcast_language_code;
    object_2004_options.pass_key = pass_key;
    object_2004_options.custom_game = parsed_custom_game_ptr;

    gbe::gc_message::build_dota_practice_lobby_objects(lobby_object_options, lobby_objects);
}

static bool GBE_AdaptDotaPracticeLobbyCacheSubscribedPayload(
    uint64 steam_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game)
{
    gbe::gc_message::DotaPracticeLobbyObjects lobby_objects;
    GBE_ComposeDotaPracticeLobbySOObjects(
        steam_id,
        lobby_id,
        lobby_state,
        lobby_game_state,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        members,
        has_broadcast_channel,
        broadcast_channel_id,
        broadcast_country_code,
        broadcast_description,
        broadcast_language_code,
        pass_key,
        extra_startup_account_id,
        custom_game,
        lobby_objects);

    return gbe::gc_message::build_dota_practice_lobby_cache_subscribed_payload_from_objects(lobby_id, lobby_objects.object_2004, lobby_objects.object_2015, lobby_objects.object_2014, lobby_objects.object_2016, message);
}

static bool GBE_AdaptDotaPracticeLobbyDetailsUpdatePurePayload(
    uint64 steam_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    bool include_server_lobby_placeholder,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game)
{
    if (steam_id == 0 || lobby_id == 0)
        return false;

    gbe::gc_message::DotaPracticeLobbyObjects lobby_objects;
    GBE_ComposeDotaPracticeLobbySOObjects(
        steam_id,
        lobby_id,
        lobby_state,
        lobby_game_state,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        members,
        has_broadcast_channel,
        broadcast_channel_id,
        broadcast_country_code,
        broadcast_description,
        broadcast_language_code,
        pass_key,
        extra_startup_account_id,
        custom_game,
        lobby_objects);

    return gbe::gc_message::build_dota_practice_lobby_details_update_payload_from_objects(lobby_id, lobby_objects.object_2014, lobby_objects.object_2015, lobby_objects.object_2004, lobby_objects.object_2016, include_server_lobby_placeholder, message);
}

static bool GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedFromWrappedTemplate(
    const char *wrapped_template_hex,
    const char *template_note,
    bool require_lobby_identifiers,
    bool rewrite_runtime_fields,
    bool force_lobby_owner_soid,
    bool rewrite_2015,
    uint32 account_id,
    uint64 steam_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game)
{
    std::string wrapped_message;
    if (!wrapped_template_hex || !gbe::proto_wire::decode_hex_string(wrapped_template_hex, wrapped_message)) {
        GBE_GC_DebugLog("GC_DOTA_SYNC", "launch cache template decode failed note=%s has_template=%u", template_note ? template_note : "unknown", wrapped_template_hex ? 1u : 0u);
        return false;
    }

    if (!GBE_PatchDotaTemplateIdentifiers(
            wrapped_message,
            account_id,
            steam_id,
            true,
            true,
            GBE_kDotaPracticeLobbyLaunch,
            GBE_kDotaCacheSubscribed,
            0,
            template_note ? template_note : "practice lobby launch cache template")) {
        GBE_GC_DebugLog("GC_DOTA_SYNC", "launch cache template identifier patch failed note=%s", template_note ? template_note : "unknown");
        return false;
    }

    if (!GBE_ExtractWrappedClientFromGCPayload(wrapped_message, GBE_kDotaCacheSubscribed, message)) {
        GBE_GC_DebugLog("GC_DOTA_SYNC", "launch cache template inner extraction failed note=%s wrapped_size=%zu", template_note ? template_note : "unknown", wrapped_message.size());
        return false;
    }

    if (require_lobby_identifiers) {
        if (!GBE_PatchDotaLobbyTemplateIdentifiers(message, account_id, steam_id, lobby_id)) {
            GBE_GC_DebugLog("GC_DOTA_SYNC", "launch cache required lobby identifier patch failed note=%s lobby_id=%llu", template_note ? template_note : "unknown", static_cast<unsigned long long>(lobby_id));
            return false;
        }
    } else {
        if (!GBE_PatchDotaLobbyTemplateIdentifiersIfPresent(message, steam_id, lobby_id)) {
            GBE_GC_DebugLog("GC_DOTA_SYNC", "launch cache optional lobby identifier patch failed note=%s lobby_id=%llu", template_note ? template_note : "unknown", static_cast<unsigned long long>(lobby_id));
            return false;
        }
    }

    if (!rewrite_runtime_fields) {
        if (!force_lobby_owner_soid)
            return true;

        return GBE_ForceDotaLobbyCacheOwnerSOID(message, lobby_id);
    }

    if (!GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
        message,
        account_id,
        steam_id,
        lobby_id,
        rewrite_runtime_fields,
        lobby_state,
        lobby_game_state,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        std::vector<GBE_DotaLobbyMemberState>(),
        rewrite_2015,
        extra_startup_account_id,
        pass_key,
        custom_game)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "launch cache runtime state patch failed note=%s lobby_id=%llu state=%u game_state=%u server_id=%llu match_id=%llu body_size=%zu",
            template_note ? template_note : "unknown",
            static_cast<unsigned long long>(lobby_id),
            lobby_state,
            lobby_game_state,
            static_cast<unsigned long long>(server_id),
            static_cast<unsigned long long>(match_id),
            message.size());
        return false;
    }

    if (!GBE_ForceDotaLobbyCacheOwnerSOID(message, lobby_id)) {
        GBE_GC_DebugLog("GC_DOTA_SYNC", "launch cache owner soid patch failed note=%s lobby_id=%llu body_size=%zu", template_note ? template_note : "unknown", static_cast<unsigned long long>(lobby_id), message.size());
        return false;
    }

    return true;
}

bool GBE_ExtractDotaHelloContext(const void *pubData, uint32 cubData, GBE_DotaHelloContext &context)
{
    context = {};

    if (!pubData || cubData < 8) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "invalid input pubData=%p cubData=%u", pubData, cubData);
        return false;
    }

    const uint8 *bytes = reinterpret_cast<const uint8 *>(pubData);
    uint32 outer_raw_emsg = 0;
    uint32 outer_header_length = 0;
    std::memcpy(&outer_raw_emsg, bytes, sizeof(outer_raw_emsg));
    std::memcpy(&outer_header_length, bytes + sizeof(outer_raw_emsg), sizeof(outer_header_length));

    if (GBE_GC_MaskedEMsg(outer_raw_emsg) != GBE_kEMsgClientToGC) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "unexpected outer emsg=%u", GBE_GC_MaskedEMsg(outer_raw_emsg));
        return false;
    }

    const size_t outer_header_offset = 8;
    const size_t outer_body_offset = outer_header_offset + outer_header_length;
    if (outer_body_offset > cubData) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "outer body offset overflow header_len=%u cubData=%u", outer_header_length, cubData);
        return false;
    }

    const uint8 *outer_header = bytes + outer_header_offset;
    const uint8 *outer_body = bytes + outer_body_offset;
    const size_t outer_body_size = cubData - outer_body_offset;

    if (!gbe::proto_wire::read_bytes_field(outer_header, outer_header_length, 2u, context.outer_session_field_raw)) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "missing outer session field");
        return false;
    }

    std::string payload_raw;
    if (!gbe::proto_wire::read_bytes_field(outer_body, outer_body_size, 3u, payload_raw) || payload_raw.size() < 8u) {
        GBE_GC_DebugLog(
            "GC_DOTA_HELLO",
            "invalid payload field size=%zu",
            payload_raw.size()
        );
        return false;
    }

    const uint8 *payload = reinterpret_cast<const uint8 *>(payload_raw.data());
    uint32 inner_raw_emsg = 0;
    uint32 inner_header_length = 0;
    std::memcpy(&inner_raw_emsg, payload, sizeof(inner_raw_emsg));
    std::memcpy(&inner_header_length, payload + sizeof(inner_raw_emsg), sizeof(inner_header_length));

    if (GBE_GC_MaskedEMsg(inner_raw_emsg) != GBE_kEMsgGCClientHello) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "unexpected inner emsg=%u", GBE_GC_MaskedEMsg(inner_raw_emsg));
        return false;
    }

    const size_t inner_header_offset = 8;
    const size_t inner_body_offset = inner_header_offset + inner_header_length;
    if (inner_body_offset > payload_raw.size()) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "inner body offset overflow header_len=%u payload_size=%zu", inner_header_length, payload_raw.size());
        return false;
    }

    const uint8 *inner_header = payload + inner_header_offset;
    const uint8 *inner_body = payload + inner_body_offset;
    const size_t inner_body_size = payload_raw.size() - inner_body_offset;

    uint64 parsed_version = 0;
    if (!gbe::proto_wire::read_uint64_field(inner_body, inner_body_size, 1u, parsed_version)) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "failed to extract version field");
        return false;
    }

    context.version = static_cast<uint32>(parsed_version);

    if (inner_header_length > 0) {
        uint64 source_job = 0;
        if (gbe::proto_wire::read_uint64_field(inner_header, inner_header_length, 11u, source_job)) {
            context.source_job_id = source_job;
            context.has_source_job = true;
        }
    }

    context.valid = true;
    GBE_GC_DebugLog(
        "GC_DOTA_HELLO",
        "parsed version=%u source_job=%llu has_source_job=%d session_raw_size=%zu",
        context.version,
        static_cast<unsigned long long>(context.source_job_id),
        context.has_source_job ? 1 : 0,
        context.outer_session_field_raw.size()
    );
    return true;
}

bool GBE_ExtractDirectDotaHelloContext(uint32 unMsgType, const void *pubData, uint32 cubData, GBE_DotaHelloContext &context)
{
    context = {};

    if (GBE_GC_MaskedEMsg(unMsgType) != GBE_kEMsgGCClientHello) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "direct path rejected msg=%u", GBE_GC_MaskedEMsg(unMsgType));
        return false;
    }

    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(pubData, cubData, proto_context)) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "direct path invalid input pubData=%p cubData=%u", pubData, cubData);
        return false;
    }

    uint64 parsed_version = 0;
    if (!gbe::proto_wire::read_uint64_field(proto_context.body, proto_context.body_size, 1u, parsed_version)) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "direct path failed to extract version field");
        return false;
    }

    context.valid = true;
    context.version = static_cast<uint32>(parsed_version);
    if (proto_context.protohdr.has_job_id_source()) {
        context.source_job_id = proto_context.protohdr.job_id_source();
        context.has_source_job = true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_HELLO",
        "direct path parsed version=%u source_job=%llu has_source_job=%d body_size=%zu",
        context.version,
        static_cast<unsigned long long>(context.source_job_id),
        context.has_source_job ? 1 : 0,
        proto_context.body_size
    );
    return true;
}

bool GBE_ExtractDirectDotaServerHelloContext(uint32 unMsgType, const void *pubData, uint32 cubData, GBE_DotaServerHelloContext &context)
{
    context = {};

    if (GBE_GC_MaskedEMsg(unMsgType) != GBE_kEMsgGCServerHello) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "direct path rejected msg=%u", GBE_GC_MaskedEMsg(unMsgType));
        return false;
    }

    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(pubData, cubData, proto_context)) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "direct path invalid input pubData=%p cubData=%u", pubData, cubData);
        return false;
    }

    CMsgServerHello protomsg;
    if (!protomsg.ParseFromArray(proto_context.body, static_cast<int>(proto_context.body_size)) || !protomsg.has_version()) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "direct path failed parsing CMsgServerHello body_size=%zu", proto_context.body_size);
        return false;
    }

    const uint32 version = protomsg.version();

    context.valid = true;
    context.active_version = version;
    context.min_allowed_version = version;
    context.compatibility_value = 0;
    context.universe = 0;
    if (proto_context.protohdr.has_client_steam_id()) {
        context.client_steam_id = proto_context.protohdr.client_steam_id();
        context.has_client_steam_id = true;
    }
    if (proto_context.protohdr.has_client_session_id()) {
        context.client_session_id = proto_context.protohdr.client_session_id();
        context.has_client_session_id = true;
    }
    if (proto_context.protohdr.has_source_app_id()) {
        context.source_app_id = proto_context.protohdr.source_app_id();
        context.has_source_app_id = true;
    }
    if (proto_context.protohdr.has_job_id_source()) {
        context.source_job_id = proto_context.protohdr.job_id_source();
        context.has_source_job = true;
    }
    if (proto_context.protohdr.has_gc_msg_src()) {
        context.gc_msg_src = static_cast<uint32>(proto_context.protohdr.gc_msg_src());
        context.has_gc_msg_src = true;
    }
    if (proto_context.protohdr.has_gc_dir_index_source()) {
        context.gc_dir_index_source = proto_context.protohdr.gc_dir_index_source();
        context.has_gc_dir_index_source = true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_SERVER_HELLO",
        "direct path parsed active_version=%u min_allowed=%u compat=%llu universe=%u source_job=%llu has_source_job=%d client_steam_id=%llu has_client_steam_id=%d client_session_id=%d has_client_session_id=%d source_app_id=%u has_source_app_id=%d gc_msg_src=%u has_gc_msg_src=%d gc_dir_index_source=%u has_gc_dir_index_source=%d body_size=%zu",
        context.active_version,
        context.min_allowed_version,
        static_cast<unsigned long long>(context.compatibility_value),
        context.universe,
        static_cast<unsigned long long>(context.source_job_id),
        context.has_source_job ? 1 : 0,
        static_cast<unsigned long long>(context.client_steam_id),
        context.has_client_steam_id ? 1 : 0,
        context.client_session_id,
        context.has_client_session_id ? 1 : 0,
        context.source_app_id,
        context.has_source_app_id ? 1 : 0,
        context.gc_msg_src,
        context.has_gc_msg_src ? 1 : 0,
        context.gc_dir_index_source,
        context.has_gc_dir_index_source ? 1 : 0,
        proto_context.body_size
    );
    return true;
}

bool GBE_BuildDirectDotaClientWelcome(uint64 steam_id, uint32 app_id, uint32 account_id, const GBE_DotaHelloContext &context, std::string &message)
{
    std::string inner_body;
    if (!GBE_PrepareDotaWelcomeBody(steam_id, account_id, context, inner_body))
        return false;

    std::string proto_header;
    ProtoBufMsgHeader_t hdr{};
    hdr.m_EMsgFlagged = GBE_kEMsgGCClientWelcome | GBE_kProtoMask;

    CMsgProtoBufHeader protohdr;
    protohdr.set_client_steam_id(steam_id);
    protohdr.set_client_session_id(1);
    protohdr.set_source_app_id(app_id);
    if (context.has_source_job) {
        protohdr.set_job_id_target(context.source_job_id);
        GBE_GC_DebugLog("GC_DOTA_WELCOME", "direct path mirroring source_job=%llu into target_job", static_cast<unsigned long long>(context.source_job_id));
    }

    hdr.m_cubProtoBufExtHdr = static_cast<uint32>(protohdr.ByteSizeLong());
    message.clear();
    ser_var<ProtoBufMsgHeader_t>(message, hdr);
    protohdr.AppendToString(&message);
    message.append(inner_body);

    GBE_GC_DebugLog("GC_DOTA_WELCOME", "built direct welcome header=%u body=%zu total=%zu", hdr.m_cubProtoBufExtHdr, inner_body.size(), message.size());
    return true;
}

bool GBE_ComposeDotaClientWelcome(uint64 steam_id, uint32 app_id, uint32 account_id, const GBE_DotaHelloContext &context, std::string &message)
{
    std::string inner_body;
    if (!GBE_PrepareDotaWelcomeBody(steam_id, account_id, context, inner_body))
        return false;

    std::string inner_header;
    if (context.has_source_job) {
        gbe::proto_wire::append_varint_field(inner_header, 10, context.source_job_id);
        GBE_GC_DebugLog("GC_DOTA_WELCOME", "mirroring source_job=%llu into inner target_job", static_cast<unsigned long long>(context.source_job_id));
    }

    std::string inner_payload;
    gbe::proto_wire::append_little_endian32(inner_payload, GBE_kEMsgGCClientWelcome | GBE_kProtoMask);
    gbe::proto_wire::append_little_endian32(inner_payload, static_cast<uint32>(inner_header.size()));
    inner_payload.append(inner_header);
    inner_payload.append(inner_body);

    std::string outer_body;
    gbe::proto_wire::append_varint_field(outer_body, 1, app_id);
    gbe::proto_wire::append_varint_field(outer_body, 2, GBE_kEMsgGCClientWelcome | GBE_kProtoMask);
    gbe::proto_wire::append_bytes_field(outer_body, 3, inner_payload);

    std::string outer_header;
    // The outer fixed64 steamid lives outside the inner template, so we rebuild that header directly.
    gbe::proto_wire::append_fixed64_field(outer_header, 1, steam_id);
    gbe::proto_wire::append_varuint(outer_header, (static_cast<uint64>(2) << 3) | 0u);
    outer_header.append(context.outer_session_field_raw);

    message.clear();
    gbe::proto_wire::append_little_endian32(message, GBE_kEMsgClientFromGC | GBE_kProtoMask);
    gbe::proto_wire::append_little_endian32(message, static_cast<uint32>(outer_header.size()));
    message.append(outer_header);
    message.append(outer_body);
    GBE_GC_DebugLog(
        "GC_DOTA_WELCOME",
        "built welcome outer_size=%zu inner_header=%zu inner_body=%zu total=%zu",
        outer_header.size(),
        inner_header.size(),
        inner_body.size(),
        message.size()
    );
    return true;
}

bool GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedTemplate(
    uint32 account_id,
    uint64 steam_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game)
{
    (void)extra_startup_account_id;

    return GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedFromWrappedTemplate(
        GBE_kDotaPracticeLobbyLaunchCacheSubscribedOfficialHex,
        "practice lobby launch official cache template",
        false,
        true,
        true,
        false,
        account_id,
        steam_id,
        lobby_id,
        lobby_state,
        lobby_game_state,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        pass_key,
        extra_startup_account_id,
        message,
        custom_game);
}

bool GBE_AdaptDotaPracticeLobbyDetailsUpdatePayload(
    uint64 steam_id,
    uint32 account_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game)
{
    (void)has_broadcast_channel;
    (void)broadcast_channel_id;
    (void)broadcast_country_code;
    (void)broadcast_description;
    (void)broadcast_language_code;

    const uint32 startup_account_id = gbe::dota_gc_wire::get_dota_practice_lobby_startup_account_id_for_state(account_id, lobby_state, lobby_game_state);

    if (gbe::proto_wire::is_dota_practice_lobby_prelaunch_state(server_id, match_id, game_start_time, connect)) {
        gbe::gc_message::DotaPracticeLobbyObjects lobby_objects;

        GBE_ComposeDotaPracticeLobbySOObjects(
            steam_id,
            lobby_id,
            lobby_state,
            lobby_game_state,
            server_id,
            match_id,
            game_start_time,
            connect,
            player_name,
            room_name,
            game_mode,
            server_region,
            lan,
            lan_host_ping_location,
            allow_cheats,
            fill_with_bots,
            allow_spectating,
            visibility,
            bot_difficulty_radiant,
            bot_difficulty_dire,
            bot_radiant,
            bot_dire,
            owner_team,
            owner_slot,
            owner_hero_id,
            members,
            has_broadcast_channel,
            broadcast_channel_id,
            broadcast_country_code,
            broadcast_description,
            broadcast_language_code,
            pass_key,
            startup_account_id,
            custom_game,
            lobby_objects);

        return gbe::gc_message::build_dota_practice_lobby_prelaunch_details_update_payload_from_objects(lobby_id, lobby_objects.object_2014, lobby_objects.object_2016, lobby_objects.object_2015, lobby_objects.object_2004, message)
            && GBE_ForceDotaLobbyUpdateOwnerSOID(message, lobby_id);
    }

    if (GBE_AdaptDotaPracticeLobbyDetailsUpdatePurePayload(
            steam_id,
            lobby_id,
            lobby_state,
            lobby_game_state,
            server_id,
            match_id,
            game_start_time,
            connect,
            player_name,
            room_name,
            game_mode,
            server_region,
            lan,
            lan_host_ping_location,
            allow_cheats,
            fill_with_bots,
            allow_spectating,
            visibility,
            bot_difficulty_radiant,
            bot_difficulty_dire,
            bot_radiant,
            bot_dire,
            owner_team,
            owner_slot,
            owner_hero_id,
            members,
            has_broadcast_channel,
            broadcast_channel_id,
            broadcast_country_code,
            broadcast_description,
            broadcast_language_code,
            pass_key,
            startup_account_id,
            true,
            message,
            custom_game)) {
        return true;
    }

    return GBE_ReplayDotaPracticeLobbyOfficial26Payload(
        GBE_kDotaOfficial032PracticeLobby26Hex,
        "current direct 26 details update",
        account_id,
        steam_id,
        lobby_id,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        pass_key,
        lobby_state,
        lobby_game_state,
        startup_account_id != 0u,
        startup_account_id,
        message,
        custom_game);
}

bool GBE_BuildDirectDotaServerWelcome(uint64 steam_id, uint32 app_id, const GBE_DotaServerHelloContext &context, std::string &message)
{
    if (!context.valid) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "invalid server hello context");
        return false;
    }

    ProtoBufMsgHeader_t hdr{};
    hdr.m_EMsgFlagged = EGCBaseClientMsg::k_EMsgGCServerWelcome | GBE_kProtoMask;

    CMsgProtoBufHeader protohdr;
    protohdr.set_client_steam_id(context.has_client_steam_id ? context.client_steam_id : steam_id);
    if (context.has_client_session_id) {
        protohdr.set_client_session_id(context.client_session_id);
    } else {
        protohdr.set_client_session_id(1);
    }
    protohdr.set_source_app_id(context.has_source_app_id ? context.source_app_id : app_id);
    if (context.has_source_job)
        protohdr.set_job_id_target(context.source_job_id);
    if (context.has_gc_msg_src)
        protohdr.set_gc_msg_src(static_cast<GCProtoBufMsgSrc>(context.gc_msg_src));
    if (context.has_gc_dir_index_source)
        protohdr.set_gc_dir_index_source(context.gc_dir_index_source);

    hdr.m_cubProtoBufExtHdr = static_cast<uint32>(protohdr.ByteSizeLong());

    message.clear();
    ser_var<ProtoBufMsgHeader_t>(message, hdr);
    protohdr.AppendToString(&message);

    CMsgServerWelcome protomsg;
    protomsg.set_min_allowed_version(context.min_allowed_version);
    protomsg.set_active_version(context.active_version);
    protomsg.AppendToString(&message);

    GBE_GC_DebugLog(
        "GC_DOTA_SERVER_HELLO",
        "built direct ServerWelcome active_version=%u min_allowed=%u target_job=%llu client_steam_id=%llu client_session_id=%d source_app_id=%u gc_msg_src=%u gc_dir_index_source=%u total=%zu",
        context.active_version,
        context.min_allowed_version,
        static_cast<unsigned long long>(context.has_source_job ? context.source_job_id : 0ull),
        static_cast<unsigned long long>(context.has_client_steam_id ? context.client_steam_id : steam_id),
        context.has_client_session_id ? context.client_session_id : 1,
        context.has_source_app_id ? context.source_app_id : app_id,
        context.has_gc_msg_src ? context.gc_msg_src : 0u,
        context.has_gc_dir_index_source ? context.gc_dir_index_source : 0u,
        message.size()
    );
    return true;
}

bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl(
    uint64 steam_id,
    uint32 account_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::string &pass_key,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool rewrite_runtime_fields,
    bool rewrite_2015,
    uint32 extra_startup_account_id,
    const GBE_DotaCustomGameDetails *custom_game,
    std::string &message)
{
    if (steam_id == 0 || account_id == 0 || lobby_id == 0)
        return false;

    if (!GBE_PrepareDotaDirectReplayMessage(
            GBE_kDotaPracticeLobbyCacheSubscribedTemplate,
            sizeof(GBE_kDotaPracticeLobbyCacheSubscribedTemplate),
            account_id,
            steam_id,
            false,
            false,
            false,
            0,
            7038u,
            24u,
            0,
            "practice lobby cache template",
            message)) {
        return false;
    }

    if (!GBE_PatchDotaLobbyTemplateIdentifiers(message, account_id, steam_id, lobby_id))
        return false;

    return GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
        message,
        account_id,
        steam_id,
        lobby_id,
        rewrite_runtime_fields,
        lobby_state,
        lobby_game_state,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        members,
        rewrite_2015,
        extra_startup_account_id,
        pass_key,
        custom_game);
}

bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl(
    uint64 steam_id,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    const GBE_DotaCustomGameDetails *custom_game,
    std::string &message)
{
    if (steam_id == 0 || lobby_id == 0)
        return false;

    return GBE_AdaptDotaPracticeLobbyCacheSubscribedPayload(
        steam_id,
        lobby_id,
        lobby_state,
        lobby_game_state,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        members,
        has_broadcast_channel,
        broadcast_channel_id,
        broadcast_country_code,
        broadcast_description,
        broadcast_language_code,
        pass_key,
        extra_startup_account_id,
        message,
        custom_game);
}

bool GBE_ReplayDotaPracticeLobbyOfficial26Payload(
    const char *wrapped_template_hex,
    const char *stage_note,
    uint32 account_id,
    uint64 steam_id,
    uint64 lobby_id,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::string &pass_key,
    uint32 lobby_state,
    uint32 lobby_game_state,
    bool rewrite_2015,
    uint32 extra_startup_account_id,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game)
{
    if (!wrapped_template_hex)
        return false;

    std::string wrapped_message;
    if (!gbe::proto_wire::decode_hex_string(wrapped_template_hex, wrapped_message))
        return false;

    std::string inner_payload;
    if (!GBE_ExtractWrappedClientFromGCPayload(wrapped_message, GBE_kDotaPracticeLobbyDetailsUpdate, inner_payload))
        return false;

    if (!GBE_PatchDotaPracticeLobbyLaunchTemplate(
            inner_payload,
            account_id,
            steam_id,
            lobby_id,
            server_id,
            match_id,
            game_start_time,
            connect,
            true,
            true,
            true,
            stage_note))
        return false;

    if (!GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
            inner_payload,
            account_id,
            steam_id,
            lobby_id,
            true,
            lobby_state,
            lobby_game_state,
            server_id,
            match_id,
            game_start_time,
            connect,
            player_name,
            room_name,
            game_mode,
            server_region,
            lan,
            lan_host_ping_location,
            allow_cheats,
            fill_with_bots,
            allow_spectating,
            visibility,
            bot_difficulty_radiant,
            bot_difficulty_dire,
            bot_radiant,
            bot_dire,
            owner_team,
            owner_slot,
            owner_hero_id,
            std::vector<GBE_DotaLobbyMemberState>(),
            rewrite_2015,
            extra_startup_account_id,
            pass_key,
            custom_game))
        return false;

    message.swap(inner_payload);
    return GBE_ForceDotaLobbyUpdateOwnerSOID(message, lobby_id);
}
