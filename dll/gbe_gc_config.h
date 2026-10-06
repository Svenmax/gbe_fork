#ifndef GBE_GC_CONFIG_H
#define GBE_GC_CONFIG_H

#include <cstdint>
#include <json/json.hpp>
#include <string>

namespace gbe::gc_config {

// Parses GC profile settings into a side-effect-free result for the coordinator.

enum class Profile : std::uint32_t {
    Invalid = 0u,
    Tf2 = 1u,
    Dota2 = 2u,
};

enum class DotaFallbackReason : std::uint32_t {
    None = 0u,
    MissingConfig = 1u,
    InvalidProfile = 2u,
};

struct ParseResult {
    Profile profile{Profile::Invalid};
    int version{};
    bool is_portal2{};
    DotaFallbackReason dota_fallback_reason{DotaFallbackReason::None};
    std::string error_message;
};

ParseResult parse_gc_config(bool loaded, const nlohmann::json &gc_json, std::uint32_t app_id, std::uint32_t dota_app_id);

} // namespace gbe::gc_config

#endif // GBE_GC_CONFIG_H
