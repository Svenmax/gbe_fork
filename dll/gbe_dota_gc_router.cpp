#include "gbe_dota_gc_router.h"

#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"

#include <cstring>

namespace gbe::dota_gc_router {

bool extract_wrapped_post_login_request(
    const void *data,
    std::uint32_t size,
    std::uint32_t expected_outer_emsg,
    DotaGcRequestContext &context)
{
    context = {};

    if (!data || size < 8u)
        return false;

    const auto *bytes = reinterpret_cast<const std::uint8_t *>(data);
    std::uint32_t outer_raw_emsg = 0;
    std::uint32_t outer_header_length = 0;
    std::memcpy(&outer_raw_emsg, bytes, sizeof(outer_raw_emsg));
    std::memcpy(&outer_header_length, bytes + sizeof(outer_raw_emsg), sizeof(outer_header_length));

    if (gc_message::without_proto_mask(outer_raw_emsg) != expected_outer_emsg)
        return false;

    const std::size_t outer_header_offset = 8u;
    const std::size_t outer_body_offset = outer_header_offset + outer_header_length;
    if (outer_body_offset > size)
        return false;

    const std::uint8_t *outer_header = bytes + outer_header_offset;
    const std::uint8_t *outer_body = bytes + outer_body_offset;
    const std::size_t outer_body_size = size - outer_body_offset;

    if (!proto_wire::read_bytes_field(outer_header, outer_header_length, 2u, context.outer_session_field_raw))
        return false;

    std::string payload_raw;
    if (!proto_wire::read_bytes_field(outer_body, outer_body_size, 3u, payload_raw) || payload_raw.size() < 8u)
        return false;

    const auto *payload = reinterpret_cast<const std::uint8_t *>(payload_raw.data());
    std::uint32_t inner_raw_emsg = 0;
    std::uint32_t inner_header_length = 0;
    std::memcpy(&inner_raw_emsg, payload, sizeof(inner_raw_emsg));
    std::memcpy(&inner_header_length, payload + sizeof(inner_raw_emsg), sizeof(inner_header_length));

    const std::size_t inner_header_offset = 8u;
    const std::size_t inner_body_offset = inner_header_offset + inner_header_length;
    if (inner_body_offset > payload_raw.size())
        return false;

    const std::uint8_t *inner_header = payload + inner_header_offset;
    context.inner_emsg = gc_message::without_proto_mask(inner_raw_emsg);
    context.body.assign(
        reinterpret_cast<const char *>(payload + inner_body_offset),
        payload_raw.size() - inner_body_offset);

    std::uint64_t request_job_id = 0;
    if (proto_wire::read_uint64_field(inner_header, inner_header_length, 10u, request_job_id) ||
        proto_wire::read_uint64_field(inner_header, inner_header_length, 11u, request_job_id)) {
        context.request_job_id = request_job_id;
        context.has_request_job = true;
    }

    context.wrapped = true;
    context.valid = true;
    return true;
}

const std::string *outer_session_field_or_null(const DotaGcRequestContext &context)
{
    return context.wrapped ? &context.outer_session_field_raw : nullptr;
}

bool build_outbound_message(
    std::uint32_t inner_emsg,
    const std::string &inner_payload,
    bool wrapped,
    const std::string *outer_session_field_raw,
    std::uint64_t steam_id,
    std::uint32_t wrapped_outer_emsg,
    std::uint32_t app_id,
    DotaGcOutboundMessage &message)
{
    message = {};

    if (!wrapped) {
        message.emsg = gc_message::with_proto_mask(inner_emsg);
        message.payload = inner_payload;
        return true;
    }

    if (!outer_session_field_raw)
        return false;

    if (!gc_message::build_wrapped_dota_replay_message(wrapped_outer_emsg, app_id, inner_payload, *outer_session_field_raw, steam_id, message.payload))
        return false;

    message.emsg = gc_message::with_proto_mask(wrapped_outer_emsg);
    return true;
}

} // namespace gbe::dota_gc_router
