#ifndef GBE_DOTA_GC_ROUTER_H
#define GBE_DOTA_GC_ROUTER_H

#include <cstdint>
#include <string>

namespace gbe::dota_gc_router {

// Routes Dota direct/wrapped GC requests and responses without owning queue or lobby side effects.

struct DotaGcRequestContext {
    bool valid{};
    std::uint32_t inner_emsg{};
    std::string body;
    std::uint64_t request_job_id{};
    bool has_request_job{};
    bool wrapped{};
    std::string outer_session_field_raw;
};

struct DotaGcOutboundMessage {
    std::uint32_t emsg{};
    std::string payload;
};

bool extract_wrapped_post_login_request(
    const void *data,
    std::uint32_t size,
    std::uint32_t expected_outer_emsg,
    DotaGcRequestContext &context);

const std::string *outer_session_field_or_null(const DotaGcRequestContext &context);

bool build_outbound_message(
    std::uint32_t inner_emsg,
    const std::string &inner_payload,
    bool wrapped,
    const std::string *outer_session_field_raw,
    std::uint64_t steam_id,
    std::uint32_t wrapped_outer_emsg,
    std::uint32_t app_id,
    DotaGcOutboundMessage &message);

} // namespace gbe::dota_gc_router

#endif // GBE_DOTA_GC_ROUTER_H
