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

// Post-login / socket / server-assignment handlers for the Dota Game
// Coordinator. Extracted from gbe_dota_handlers.cpp (Phase 3.1.5b) to group the
// protocol-level handlers that share Steam-network session state.
//
// Responsibility boundary: owns the direct/wrapped post-login dispatch (which
// sets up the GC session and replays the initial welcome cascade), socket
// request handling, and server assignment. Side-effect ownership and ordering
// are unchanged from the prior monolithic handler file; only the file location
// moved. The two handler-local session constants (`GBE_kSteamGamesPlayedWithDataBlob`
// and `GBE_kSteamAuthList`) moved with the handlers (List X, kept `static` in
// new TU); no cross-TU externalization was needed (List Y = 0).

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_request_router.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_lobby_state.h"
#include "gbe_dota_lobby_state_store.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_gc_internal.h"
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

using GBE_Dota8053Result = gbe::proto_wire::Dota8053Result;

// --- List A: handler-only static helpers (moved from steam_game_coordinator.cpp) ---

static constexpr uint32 GBE_kSteamGamesPlayedWithDataBlob = 5410u;


static constexpr uint32 GBE_kSteamAuthList = 5432u;



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
        const auto shared_update_result = GBE_SharedLobbyStore().compare_update(
            GBE_local_lobby.generation,
            [&](GBE_SharedDotaLobbyState &shared_lobby) {
                if (shared_lobby.valid && shared_lobby.lobby_id == GBE_local_lobby.lobby_id)
                    shared_lobby.connect = runtime_connect;
            });
        if (shared_update_result == gbe::dota_lobby_state::StoreUpdateResult::StaleGeneration) {
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "skipped stale shared lobby connect update reason=4508_game_server_info lobby_id=%llu generation=%llu candidate=%s",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(GBE_local_lobby.generation),
                runtime_connect.c_str());
        }

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
    const bool has_target_job = proto_context.protohdr.has_job_id_target();
    const uint64 target_job = has_target_job ? proto_context.protohdr.job_id_target() : 0ull;
    const uint8 *body = proto_context.body;
    const size_t body_size = proto_context.body_size;

    gbe::dota_gc_router::DotaGcRequestContext request_context{};
    request_context.valid = true;
    request_context.inner_emsg = request_emsg;
    request_context.body.assign(reinterpret_cast<const char *>(body), body_size);
    request_context.request_job_id = source_job;
    request_context.has_request_job = has_source_job;
    request_context.target_job_id = target_job;
    request_context.has_target_job = has_target_job;
    request_context.wrapped = false;
    request_context.path = gbe::dota_gc_router::DotaGcRequestPath::Direct;
    if (GBE_DispatchDotaPostLoginRequest(request_context))
        return true;

    // Direct fallback ladder (documented in MESSAGE_ROUTING §2):
    // CONDITIONAL_PROBE 8744 -> log then fall through to template
    // CONDITIONAL_CONSUME 5410/5432 when late-steam tracking is on
    // else TEMPLATE_ONLY catch-all

    if (request_emsg == 8744u) {
        // CONDITIONAL_PROBE: observe only; production reply is TEMPLATE_ONLY 8744.
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

    if (request_emsg == GBE_kSteamGamesPlayedWithDataBlob && GBE_ShouldTrackDotaPracticeLobbyLateSteamChain()) {
        // CONDITIONAL_CONSUME: swallow without synthetic followup while tracking.
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
        // CONDITIONAL_CONSUME: swallow without synthetic followup while tracking.
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

    return GBE_HandleDotaTemplateReplayRequest(request_emsg, body, body_size, has_source_job, source_job);
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

    // B4: unregistered wrapped miss is a hard stop. Former dead path misrouted to
    // SetTeamSlot (7047); registry owns 7047, so do not invent a mutation here.
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] unregistered wrapped miss emsg=%u has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
        route_context.inner_emsg,
        route_context.has_request_job ? 1 : 0,
        static_cast<unsigned long long>(route_context.request_job_id),
        route_context.outer_session_field_raw.size(),
        gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(route_context.body.data()), route_context.body.size(), 48).c_str()
    );
    return false;
}

bool Steam_Game_Coordinator::GBE_HandleDotaFindTopSourceTVGamesRequest(
    const std::string &request_body,
    bool has_request_job,
    uint64 request_job_id,
    bool wrapped,
    const std::string *outer_session_field_raw)
{
    (void)request_body;

    if (wrapped) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[WATCH] Received wrapped 8009 FindTopSourceTVGames has_job=%d request_job=%llu",
            has_request_job ? 1 : 0,
            static_cast<unsigned long long>(request_job_id));
    }

    gbe::gc_message::DotaSourceTVGame source_tv_game{};
    bool found_game = false;
    const auto shared_lobby = GBE_SharedLobbyStore().snapshot();

    if (shared_lobby.valid &&
        shared_lobby.active &&
        shared_lobby.game_state >= 1u &&
        shared_lobby.server_id != 0) {
        source_tv_game.start_time = shared_lobby.game_start_time != 0
            ? shared_lobby.game_start_time
            : static_cast<uint32>(std::time(nullptr) - 300);
        source_tv_game.server_id = shared_lobby.server_id;
        source_tv_game.lobby_id = shared_lobby.lobby_id;
        source_tv_game.game_time = shared_lobby.game_start_time != 0
            ? static_cast<uint32>(std::time(nullptr)) - static_cast<uint32>(shared_lobby.game_start_time)
            : 300u;
        source_tv_game.game_mode = shared_lobby.game_mode;
        source_tv_game.match_id = shared_lobby.match_id;
        for (const auto &member : shared_lobby.members) {
            if (member.account_id == 0)
                continue;
            source_tv_game.players.push_back(gbe::gc_message::DotaSourceTVPlayer{
                member.account_id, member.hero_id, member.slot, member.team});
        }
        found_game = true;
    }

    if (!found_game) {
        const char *snap_reason = wrapped ? "8009_wrapped_find_top_source_tv" : "8009_find_top_source_tv";
        const std::vector<GBE_LocalLobby> snapshots = GBE_GetDotaGenericLobbySnapshots(snap_reason);
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
                    if (member.account_id == 0)
                        continue;
                    source_tv_game.players.push_back(gbe::gc_message::DotaSourceTVPlayer{
                        member.account_id, member.hero_id, member.slot, member.team});
                }
                found_game = true;
                break;
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
        has_request_job,
        request_job_id,
        response_body,
        response_message);

    const char *push_note = wrapped ? "8010_watch_response" : "find_top_source_tv_games";
    if (!GBE_PushDotaResponse(
            GBE_kDotaFindTopSourceTVGamesResponse,
            response_message,
            wrapped,
            outer_session_field_raw,
            push_note))
        return true;

    if (wrapped) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[WATCH] replied 8010 FindTopSourceTVGamesResponse found=%d local_valid=%d",
            found_game ? 1 : 0,
            shared_lobby.valid ? 1 : 0);
    } else {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "FindTopSourceTVGames -> response found=%d source_job=%llu local_valid=%d",
            found_game ? 1 : 0,
            static_cast<unsigned long long>(request_job_id),
            shared_lobby.valid ? 1 : 0);
    }
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaWatchGameRequest(
    const std::string &request_body,
    bool has_request_job,
    uint64 request_job_id,
    bool wrapped,
    const std::string *outer_session_field_raw)
{
    const uint8 *body = reinterpret_cast<const uint8 *>(request_body.data());
    const size_t body_size = request_body.size();

    uint64 watch_server_steamid = 0;
    {
        size_t pos = 0;
        while (pos < body_size) {
            gbe::proto_wire::Field field{};
            size_t field_offset = 0;
            size_t field_end = 0;
            if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end))
                break;
            if (field.number == 1u && field.wire_type == 1u && field.value_size == 8)
                memcpy(&watch_server_steamid, body + field.value_offset, 8);
        }
    }

    uint32 source_tv_addr = 0;
    uint32 source_tv_port = 27020;
    uint64 tv_secret_code = 0;
    std::string connect_str;
    const auto shared_lobby = GBE_SharedLobbyStore().snapshot();
    if (shared_lobby.valid && !shared_lobby.connect.empty()) {
        connect_str = shared_lobby.connect;
    } else {
        const char *snap_reason = wrapped ? "7091_wrapped_watch_game" : "7091_watch_game";
        const std::vector<GBE_LocalLobby> snapshots = GBE_GetDotaGenericLobbySnapshots(snap_reason);
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
    if (tv_secret_code == 0 && GBE_local_lobby.tv_secret_code != 0)
        tv_secret_code = GBE_local_lobby.tv_secret_code;
    if (GBE_local_lobby.tv_port != 0)
        source_tv_port = GBE_local_lobby.tv_port;
    if (!connect_str.empty()) {
        size_t colon = connect_str.find(':');
        std::string ip_str = (colon != std::string::npos) ? connect_str.substr(0, colon) : connect_str;
        if (colon != std::string::npos) {
            uint32 game_port = static_cast<uint32>(std::strtoul(connect_str.c_str() + colon + 1, nullptr, 10));
            if (game_port > 0)
                source_tv_port = game_port + 5;
        }
        unsigned int a = 0, b = 0, c = 0, d = 0;
        if (std::sscanf(ip_str.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4)
            source_tv_addr = (a << 24) | (b << 16) | (c << 8) | d;
    }

    {
        std::string pending_body;
        gbe::gc_message::build_dota_watch_game_pending_response_body(pending_body);
        std::string pending_msg;
        if (wrapped) {
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(
                7092u, has_request_job, request_job_id, pending_body, pending_msg);
            GBE_PushDotaResponse(7092u, pending_msg, true, outer_session_field_raw, "7091_watch_pending");
        } else {
            const uint64 pending_reply_job = has_request_job ? request_job_id : 0xFFFFFFFFFFFFFFFFULL;
            gbe::gc_message::build_dota_job_reply_payload(7092u, pending_reply_job, pending_body, pending_msg);
            push_incoming_now(7092u | GBE_kProtoMask, pending_msg);
        }
    }

    const CSteamID local_steam_id = settings->get_local_steam_id();
    const uint32 local_account_id = local_steam_id.GetAccountID();
    const uint64 secret_code = (tv_secret_code != 0) ? tv_secret_code : (watch_server_steamid ^ 0x0514D449EDC24001ULL);

    {
        std::string ready_body;
        gbe::gc_message::build_dota_watch_game_ready_response_body(
            source_tv_addr, source_tv_port, watch_server_steamid, secret_code, ready_body);
        std::string ready_msg;
        gbe::gc_message::build_dota_job_reply_or_zero_header_payload(7092u, false, 0, ready_body, ready_msg);
        if (wrapped)
            GBE_PushDotaResponse(7092u, ready_msg, true, outer_session_field_raw, "7091_watch_ready");
        else
            push_incoming_now(7092u | GBE_kProtoMask, ready_msg);
    }

    if (wrapped) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[WATCH] wrapped WatchGame -> READY server=0x%llx tv_addr=0x%x tv_port=%u local_account=%u local_steamid=%llu raw_tv_secret=0x%llx sent_secret=0x%llx",
            static_cast<unsigned long long>(watch_server_steamid),
            source_tv_addr,
            source_tv_port,
            local_account_id,
            static_cast<unsigned long long>(local_steam_id.ConvertToUint64()),
            static_cast<unsigned long long>(tv_secret_code),
            static_cast<unsigned long long>(secret_code));
    } else {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "watch game request -> READY server=0x%llx tv_addr=0x%x tv_port=%u local_account=%u local_steamid=%llu raw_tv_secret=0x%llx sent_secret=0x%llx source_job=%llu",
            static_cast<unsigned long long>(watch_server_steamid),
            source_tv_addr,
            source_tv_port,
            local_account_id,
            static_cast<unsigned long long>(local_steam_id.ConvertToUint64()),
            static_cast<unsigned long long>(tv_secret_code),
            static_cast<unsigned long long>(secret_code),
            static_cast<unsigned long long>(request_job_id));
    }
    return true;
}
