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

// Phase 1.1 of the Steam_Game_Coordinator refactor.
//
// Extracts the Dota 2 GC request "envelope" parsers from
// steam_game_coordinator.cpp. These functions take raw GC message bytes
// (either a direct protobuf-framed message, or a Valve "wrapped"
// ClientToGC/ClientFromGC replay envelope that nests an inner message) and
// produce a structured context the dispatcher can consume.
//
// All functions are kept at global scope with their original GBE_* names so
// existing call sites in steam_game_coordinator.cpp resolve unchanged via
// #include. They are declared `inline` so the non-static definitions in the
// matching .cpp do not cause ODR violations when included from one TU.

#ifndef GBE_DOTA_REQUEST_ROUTER_H
#define GBE_DOTA_REQUEST_ROUTER_H

#include <cstdint>
#include <cstring>
#include <string>

#include "gbe_dota_protocol_constants.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"

// Forward declarations of the protobuf-generated types used in the contexts.
// The full definitions come from <steammessages.pb.h> which the .cpp includes;
// the header only needs the type names for struct fields.
struct ProtoBufMsgHeader_t;
class CMsgProtoBufHeader;

// --- Mask helper -----------------------------------------------------------
// Strips the protobuf mask bit (0x80000000) from an emsg. Used everywhere the
// dispatcher compares a raw msg_type against an unmasked GBE_k* constant.

inline std::uint32_t GBE_GC_MaskedEMsg(std::uint32_t msg_type)
{
    return gbe::gc_message::without_proto_mask(msg_type);
}

// --- Direct (non-wrapped) protobuf message context -------------------------
// Layout: [ProtoBufMsgHeader_t][optional extended proto header][body bytes].

struct GBE_DirectProtoContext
{
    ProtoBufMsgHeader_t hdr{};
    CMsgProtoBufHeader protohdr;
    std::size_t body_offset{};
    const std::uint8 *body{};
    std::size_t body_size{};
};

// Parses a direct protobuf-framed GC message into context. Returns false on
// truncated/invalid input. On success context.body points into the caller's
// buffer (no copy) and context.protohdr holds the parsed extended header.
inline bool GBE_ParseDirectProtoContext(const void *pubData, std::uint32_t cubData, GBE_DirectProtoContext &context);

// NOTE: GBE_ParseDirectProtoContext's implementation lives in
// gbe_dota_request_router.cpp because it calls CMsgProtoBufHeader methods
// (Clear/ParseFromArray) and pulling <steammessages.pb.h> into this header
// would force every includer to compile the heavyweight protobuf-generated
// header. The other two parsers only touch bytes/gbe_proto_wire and stay
// inline here.

// --- Wrapped ClientToGC replay envelope ------------------------------------
// Layout (two layers):
//   outer: [u32 emsg=ClientToGC][u32 header_len][header bytes][body bytes]
//          header field 2 = outer session field (raw bytes)
//          body   field 3 = inner payload
//   inner: [u32 emsg][u32 header_len][header bytes][body bytes]
//          header fields 10/11 = request job id (source/expected)

struct GBE_DotaWrappedDirectContext
{
    bool valid{};
    std::uint32_t inner_emsg{};
    std::string outer_session_field_raw;
    std::string inner_body_raw;
    std::uint64_t request_job_id{};
    bool has_request_job{};
};

// Extracts the inner direct message from a wrapped ClientToGC envelope.
inline bool GBE_ExtractWrappedDotaDirectContext(const void *pubData, std::uint32_t cubData, GBE_DotaWrappedDirectContext &context);

// --- Wrapped ClientFromGC payload extraction -------------------------------
// Given a wrapped message already in a std::string (typically produced by
// patching a template), extracts the inner payload and verifies its inner
// emsg matches expected_inner_emsg. Used when re-using captured GC templates.

inline bool GBE_ExtractWrappedClientFromGCPayload(
    const std::string &wrapped_message,
    std::uint32_t expected_inner_emsg,
    std::string &inner_payload);

// =====================================================================
// Inline implementations (kept in the header to match the original
// `static` semantics for the two bytes-only parsers; the direct-proto
// parser lives in gbe_dota_request_router.cpp due to its protobuf dep).
// =====================================================================

inline bool GBE_ExtractWrappedDotaDirectContext(const void *pubData, std::uint32_t cubData, GBE_DotaWrappedDirectContext &context)
{
    context = {};

    if (!pubData || cubData < 8)
        return false;

    const std::uint8 *bytes = reinterpret_cast<const std::uint8 *>(pubData);
    std::uint32_t outer_raw_emsg = 0;
    std::uint32_t outer_header_length = 0;
    std::memcpy(&outer_raw_emsg, bytes, sizeof(outer_raw_emsg));
    std::memcpy(&outer_header_length, bytes + sizeof(outer_raw_emsg), sizeof(outer_header_length));

    if (GBE_GC_MaskedEMsg(outer_raw_emsg) != GBE_kEMsgClientToGC)
        return false;

    const std::size_t outer_header_offset = 8;
    const std::size_t outer_body_offset = outer_header_offset + outer_header_length;
    if (outer_body_offset > cubData)
        return false;

    const std::uint8 *outer_header = bytes + outer_header_offset;
    const std::uint8 *outer_body = bytes + outer_body_offset;
    const std::size_t outer_body_size = cubData - outer_body_offset;

    if (!gbe::proto_wire::read_bytes_field(outer_header, outer_header_length, 2u, context.outer_session_field_raw))
        return false;

    std::string payload_raw;
    if (!gbe::proto_wire::read_bytes_field(outer_body, outer_body_size, 3u, payload_raw) || payload_raw.size() < 8u)
        return false;

    const std::uint8 *payload = reinterpret_cast<const std::uint8 *>(payload_raw.data());
    std::uint32_t inner_raw_emsg = 0;
    std::uint32_t inner_header_length = 0;
    std::memcpy(&inner_raw_emsg, payload, sizeof(inner_raw_emsg));
    std::memcpy(&inner_header_length, payload + sizeof(inner_raw_emsg), sizeof(inner_header_length));

    const std::size_t inner_header_offset = 8;
    const std::size_t inner_body_offset = inner_header_offset + inner_header_length;
    if (inner_body_offset > payload_raw.size())
        return false;

    const std::uint8 *inner_header = payload + inner_header_offset;
    context.inner_emsg = GBE_GC_MaskedEMsg(inner_raw_emsg);

    context.inner_body_raw.assign(
        reinterpret_cast<const char *>(payload + inner_body_offset),
        payload_raw.size() - inner_body_offset
    );

    std::uint64_t request_job_id = 0;
    if (gbe::proto_wire::read_uint64_field(inner_header, inner_header_length, 10u, request_job_id) ||
        gbe::proto_wire::read_uint64_field(inner_header, inner_header_length, 11u, request_job_id)) {
        context.request_job_id = request_job_id;
        context.has_request_job = true;
    }

    context.valid = true;
    return true;
}

inline bool GBE_ExtractWrappedClientFromGCPayload(
    const std::string &wrapped_message,
    std::uint32_t expected_inner_emsg,
    std::string &inner_payload)
{
    inner_payload.clear();
    if (wrapped_message.size() < 8u)
        return false;

    const std::uint8 *bytes = reinterpret_cast<const std::uint8 *>(wrapped_message.data());
    std::uint32_t outer_raw_emsg = 0;
    std::uint32_t outer_header_length = 0;
    std::memcpy(&outer_raw_emsg, bytes, sizeof(outer_raw_emsg));
    std::memcpy(&outer_header_length, bytes + sizeof(outer_raw_emsg), sizeof(outer_header_length));

    if (GBE_GC_MaskedEMsg(outer_raw_emsg) != GBE_kEMsgClientFromGC)
        return false;

    const std::size_t outer_body_offset = 8u + outer_header_length;
    if (outer_body_offset > wrapped_message.size())
        return false;

    const std::uint8 *outer_body = bytes + outer_body_offset;
    const std::size_t outer_body_size = wrapped_message.size() - outer_body_offset;
    if (!gbe::proto_wire::read_bytes_field(outer_body, outer_body_size, 3u, inner_payload) || inner_payload.size() < 8u)
        return false;

    std::uint32_t inner_raw_emsg = 0;
    std::memcpy(&inner_raw_emsg, inner_payload.data(), sizeof(inner_raw_emsg));
    return GBE_GC_MaskedEMsg(inner_raw_emsg) == expected_inner_emsg;
}

#endif // GBE_DOTA_REQUEST_ROUTER_H
