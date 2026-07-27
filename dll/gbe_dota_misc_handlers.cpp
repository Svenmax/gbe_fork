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

// Misc one-off Dota Game Coordinator request handlers. Extracted from
// gbe_dota_handlers.cpp (Phase 3.1.5c) to group the remaining standalone
// handlers that do not belong to the inventory / chat / lobby / match domains.
//
// Responsibility boundary: owns minimal-varint success responses, profile-card
// / account-name / emoticon / conduct-scorecard / coaching-summary / rank
// queries, launch advance-or-consume and 8870 launch-marker consumption,
// LAN server available notification, batch player resources response with
// per-player CacheSubscribed side effect, cache subscription refresh
// acknowledgement, leaver detection member mutation, sign-out permission, and
// submit-player-report-v2 acknowledgement. Side-effect ownership and ordering
// are unchanged from the prior monolithic handler file; only the file location
// moved. No handler-local statics moved with this group (List X = 0); no
// cross-TU externalization was needed (List Y = 0, only the
// `GBE_DotaEmptyRequestShape` and `GBE_DotaRankRequestShape` aliases were
// repeated in the new TU).

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_request_router.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_gc_diagnostics.h"
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

using GBE_DotaEmptyRequestShape = gbe::proto_wire::DotaEmptyRequestShape;
using GBE_DotaRankRequestShape = gbe::proto_wire::DotaRankRequestShape;

// ============================================================================
// Side-effect order documentation (see dll/gbe_dota_action_model.h for the
// canonical action type and cross-domain ordering invariants).
// ============================================================================
//
// GBE_HandleDotaMinimalVarintSuccessRequest (generic -> response_emsg):
//   1. Build varint response payload (pure: build_dota_varint_response_payload)
//   2. PushDotaResponse(response_emsg) [coordinator: push_incoming_now]
//   Invariant: single response, no state mutation.
//
// GBE_HandleDota7427NotificationsRequest (emsg 7427 -> 7428):
//   1. Build 7428 notifications payload (pure)
//   2. PushDotaResponse(7428) [coordinator]
//
// GBE_HandleDotaUploadRateRequest (emsg 7440 -> 7441):
//   1. Build 7441 upload rate payload (pure)
//   2. PushDotaResponse(7441) [coordinator]
//
// GBE_HandleDotaProfileCardRequest (emsg 7485 -> 7486):
//   1. Build 7486 profile card payload (pure)
//   2. PushDotaResponse(7486) [coordinator]
//
// GBE_HandleDotaLookupAccountNameRequest (emsg 7537 -> 7538):
//   1. Parse: steam_id (field 1)
//   2. Resolve account_name via steam_friends (coordinator read)
//   3. Build 7538 lookup response (pure)
//   4. PushDotaResponse(7538) [coordinator]
//
// GBE_HandleDotaEmoticonDataRequest (emsg 7563 -> 7564):
//   1. Build 7564 emoticon data payload (pure)
//   2. PushDotaResponse(7564) [coordinator]
//
// GBE_HandleDotaConductScorecardRequest (emsg 7591 -> 7592):
//   1. Build 7592 conduct scorecard payload (pure)
//   2. PushDotaResponse(7592) [coordinator]
//
// GBE_HandleDotaCoachingSummaryRequest (emsg 7812 -> 7813):
//   1. Build 7813 coaching summary payload (pure)
//   2. PushDotaResponse(7813) [coordinator]
//
// GBE_HandleDotaRankRequest (emsg 7674 -> 7675):
//   1. Parse: rank params (account_id, rank_type)
//   2. Build 7675 rank payload (pure)
//   3. PushDotaResponse(7675) [coordinator]
//
// GBE_HandleDotaLaunchAdvanceOrConsume (4506/5429 -> optional launch advance):
//   1. If state==1 && game_state==0 && HasDotaLaunchServerSetupSync:
//      GBE_TryAdvanceDotaLaunchToRun (emits 26) [coordinator]; if advanced,
//      return early
//   2. Else: log consume note + return
//   Invariant: launch advance is the only side effect when it fires; no
//   response is pushed.
//
// GBE_HandleDota8870LaunchMarkerRequest (emsg 8870):
//   1. Log consume note (no side effects)
//
// GBE_HandleDotaLanServerAvailableRequest (emsg 4511):
//   1. Parse: lobby_id (field 1)
//   2. If matches_local_lobby:
//      a. If !launch_4511_seen: set launch_4511_seen=true + PublishSharedDotaLobbyState [publish]
//      b. GBE_TrySyncDotaLobbyServerIdFromGameServer [coordinator]
//   3. If matches_local_lobby && !incoming_messages.empty():
//      callbacks->addCBResult(GCMessageAvailable_t) [coordinator callback]
//   4. Log
//   Invariant: state mutation + publish precedes callback repost.
//
// GBE_HandleDotaBatchPlayerResourcesRequest (emsg 7450 -> 7451 + per-player
// CacheSubscribed when on server GC):
//   1. Parse: packed account_ids (field 1); fallback to local account_id
//   2. Build 7451 batch response (pure)
//   3. PushDotaResponse(7451) [coordinator]
//   4. If is_server && DOTA2: for each account_id:
//      a. Resolve equipped items (host: client_gc->get_items; remote:
//         all_user_items) [coordinator read]
//      b. If equipped_items empty: log + continue
//      c. Build per-player CacheSubscribed (pure: append_*_field)
//      d. push_incoming_now(CacheSubscribed) [coordinator]
//   Invariant: 7451 precedes per-player CacheSubscribed; per-player push only
//   on server GC profile.
//
// GBE_HandleDotaCacheSubscriptionRefreshRequest (emsg 2008 -> 2009):
//   1. Parse: owner_soid (field 2) -> owner_type, owner_id
//   2. Compute matches_lobby_owner (owner_type==3 && owner_id==local lobby_id)
//   3. Log observation
//   4. If matches_lobby_owner:
//      a. Build 2009 up-to-date payload (pure)
//      b. PushDotaResponse(2009) [coordinator]
//   Invariant: response only when owner matches local lobby; no state mutation.
//
// GBE_HandleDotaLeaverDetectedRequest (emsg 7072):
//   1. If !active || lobby_id==0: early return
//   2. Parse: steam_id (field 1), leaver_status (field 2), disconnect_reason
//      (field 6)
//   3. GBE_SetDotaLobbyMemberRuntimeState(steam_id, false, 0, false) [coordinator]
//   4. If member updated: PublishSharedDotaLobbyState [publish]
//   5. Log
//   Invariant: member mutation precedes publish.
//
// GBE_HandleDotaSignOutPermissionRequest (emsg 7027 -> 7028):
//   1. Build 7028 signout permission payload (pure)
//   2. PushDotaResponse(7028) [coordinator]
//
// GBE_HandleDotaSubmitPlayerReportV2Request (emsg 7925 -> 7926):
//   1. Parse: target_account_id, report_type
//   2. Build 7926 report ack payload (pure)
//   3. PushDotaResponse(7926) [coordinator]
// ============================================================================

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


bool Steam_Game_Coordinator::GBE_HandleDotaLaunchAdvance4506Request(uint32 request_emsg, uint64 source_job, size_t body_size)
{
    return GBE_HandleDotaLaunchAdvanceOrConsume(
        request_emsg,
        "runtime packet after 4506 stall recovery",
        "4506_launch_run",
        "server available acknowledgement",
        source_job,
        body_size);
}

bool Steam_Game_Coordinator::GBE_HandleDotaLaunchAdvanceTicketAuthRequest(uint32 request_emsg, uint64 source_job, size_t body_size)
{
    return GBE_HandleDotaLaunchAdvanceOrConsume(
        request_emsg,
        "runtime packet after 5429",
        "5429_launch_run",
        "ticket auth complete",
        source_job,
        body_size);
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
        bool did_mark_launch_4511_seen = false;
        {
            gbe::dota_lobby_state::LocalLobbyOwner local_lobby(GBE_local_lobby);
            local_lobby.apply("4511_lan_server_available_seen", [&did_mark_launch_4511_seen](GBE_LocalLobby &lobby) {
                did_mark_launch_4511_seen = gbe::dota_lobby_state::mark_launch_4511_seen(lobby);
            });
        }
        if (did_mark_launch_4511_seen) {
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
        gbe::dota_lobby_state::LocalLobbyOwner local_lobby(GBE_local_lobby);
        const bool updated = local_lobby.apply("7072_leaver_detected", [this, leaver_steam_id, leaver_status](GBE_LocalLobby &lobby) {
            for (GBE_DotaLobbyMemberState &member : lobby.members) {
                if (member.steam_id == leaver_steam_id) {
                    if (member.leaver_status != leaver_status) {
                        member.leaver_status = leaver_status;
                        member.connected = false;
                        GBE_GC_DebugLog(
                            "GC_DOTA_DIRECT",
                            "updated member leaver_status steam_id=%llu leaver_status=%u",
                            static_cast<unsigned long long>(leaver_steam_id),
                            leaver_status
                        );
                        return true;
                    }
                    break;
                }
            }
            return false;
        });

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
