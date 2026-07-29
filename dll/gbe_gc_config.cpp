#include "gbe_gc_config.h"

#include <algorithm>
#include <cctype>

namespace gbe::gc_config {

namespace {

std::string normalized_profile_name(std::string name)
{
    std::transform(name.begin(), name.end(), name.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return name;
}

bool should_fallback_to_dota(Profile profile, std::uint32_t app_id, std::uint32_t dota_app_id)
{
    return profile == Profile::Invalid && app_id == dota_app_id;
}

} // namespace

ParseResult parse_gc_config(bool loaded, const nlohmann::json &gc_json, std::uint32_t app_id, std::uint32_t dota_app_id)
{
    ParseResult result{};
    if (!loaded) {
        if (app_id == dota_app_id) {
            result.profile = Profile::Dota2;
            result.dota_fallback_reason = DotaFallbackReason::MissingConfig;
        }
        return result;
    }

    try {
        const std::string profile_name = normalized_profile_name(gc_json.value("gc_profile", std::string()));
        if (profile_name == "tf2") {
            result.profile = Profile::Tf2;
        } else if (profile_name == "dota2" || profile_name == "dota") {
            result.profile = Profile::Dota2;
        } else if (profile_name == "portal2") {
            result.profile = Profile::Tf2;
            result.is_portal2 = true;
        } else {
            result.profile = Profile::Invalid;
        }

        result.version = gc_json.value("gc_version", 0);
    } catch (const std::exception &e) {
        result.profile = Profile::Invalid;
        result.version = 0;
        result.is_portal2 = false;
        result.error_message = e.what();
    }

    if (should_fallback_to_dota(result.profile, app_id, dota_app_id)) {
        result.profile = Profile::Dota2;
        result.dota_fallback_reason = DotaFallbackReason::InvalidProfile;
    }

    return result;
}

} // namespace gbe::gc_config
