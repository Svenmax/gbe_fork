#include "dll/gbe_gc_config.h"

#include <iostream>

namespace {

bool expect_true(bool value, const char *label)
{
    if (value)
        return true;

    std::cerr << "failed: " << label << std::endl;
    return false;
}

bool expect_eq_int(int actual, int expected, const char *label)
{
    if (actual == expected)
        return true;

    std::cerr << "failed: " << label << " actual=" << actual << " expected=" << expected << std::endl;
    return false;
}

bool test_gc_config_profiles()
{
    bool ok = true;
    constexpr std::uint32_t dota_app_id = 570u;
    constexpr std::uint32_t other_app_id = 440u;

    nlohmann::json config = nlohmann::json::object();
    config["gc_profile"] = "tf2";
    config["gc_version"] = 20130319;
    gbe::gc_config::ParseResult result = gbe::gc_config::parse_gc_config(true, config, other_app_id, dota_app_id);
    ok &= expect_true(result.profile == gbe::gc_config::Profile::Tf2, "tf2 profile");
    ok &= expect_eq_int(result.version, 20130319, "tf2 version");
    ok &= expect_true(!result.is_portal2, "tf2 portal flag");
    ok &= expect_true(result.dota_fallback_reason == gbe::gc_config::DotaFallbackReason::None, "tf2 no fallback");

    config["gc_profile"] = "DOTA";
    config["gc_version"] = 20200101;
    result = gbe::gc_config::parse_gc_config(true, config, other_app_id, dota_app_id);
    ok &= expect_true(result.profile == gbe::gc_config::Profile::Dota2, "dota alias profile");
    ok &= expect_eq_int(result.version, 20200101, "dota version");

    config["gc_profile"] = "dota2";
    result = gbe::gc_config::parse_gc_config(true, config, other_app_id, dota_app_id);
    ok &= expect_true(result.profile == gbe::gc_config::Profile::Dota2, "dota2 profile");

    config["gc_profile"] = "portal2";
    config["gc_version"] = 20120000;
    result = gbe::gc_config::parse_gc_config(true, config, other_app_id, dota_app_id);
    ok &= expect_true(result.profile == gbe::gc_config::Profile::Tf2, "portal2 maps to tf2");
    ok &= expect_true(result.is_portal2, "portal2 flag");

    config["gc_profile"] = "unknown";
    result = gbe::gc_config::parse_gc_config(true, config, other_app_id, dota_app_id);
    ok &= expect_true(result.profile == gbe::gc_config::Profile::Invalid, "unknown profile invalid");
    ok &= expect_true(result.dota_fallback_reason == gbe::gc_config::DotaFallbackReason::None, "unknown no fallback for other app");

    result = gbe::gc_config::parse_gc_config(false, nlohmann::json::object(), dota_app_id, dota_app_id);
    ok &= expect_true(result.profile == gbe::gc_config::Profile::Dota2, "missing config dota fallback profile");
    ok &= expect_true(result.dota_fallback_reason == gbe::gc_config::DotaFallbackReason::MissingConfig, "missing config fallback reason");
    ok &= expect_eq_int(result.version, 0, "missing config version");

    config["gc_profile"] = "unknown";
    result = gbe::gc_config::parse_gc_config(true, config, dota_app_id, dota_app_id);
    ok &= expect_true(result.profile == gbe::gc_config::Profile::Dota2, "invalid profile dota fallback profile");
    ok &= expect_true(result.dota_fallback_reason == gbe::gc_config::DotaFallbackReason::InvalidProfile, "invalid profile fallback reason");

    config = nlohmann::json::object();
    config["gc_profile"] = nlohmann::json::array();
    result = gbe::gc_config::parse_gc_config(true, config, dota_app_id, dota_app_id);
    ok &= expect_true(result.profile == gbe::gc_config::Profile::Dota2, "parse error dota fallback profile");
    ok &= expect_true(!result.error_message.empty(), "parse error message");
    ok &= expect_true(result.dota_fallback_reason == gbe::gc_config::DotaFallbackReason::InvalidProfile, "parse error fallback reason");

    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= test_gc_config_profiles();

    if (!ok)
        return 1;

    std::cout << "gbe_gc_config_test passed" << std::endl;
    return 0;
}
