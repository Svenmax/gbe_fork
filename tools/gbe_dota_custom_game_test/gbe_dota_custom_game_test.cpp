#include "dll/gbe_dota_custom_game.h"

#include <iostream>

namespace {

bool expect_true(bool value, const char *label)
{
    if (value)
        return true;

    std::cerr << "failed: " << label << std::endl;
    return false;
}

bool expect_eq_string(const std::string &actual, const std::string &expected, const char *label)
{
    if (actual == expected)
        return true;

    std::cerr << "failed: " << label << " actual=" << actual << " expected=" << expected << std::endl;
    return false;
}

bool test_has_custom_game_details()
{
    bool ok = true;

    GBE_DotaCustomGameDetails empty{};
    ok &= expect_true(!gbe::dota_custom_game::has_custom_game_details(empty), "empty custom game details");

    GBE_DotaCustomGameDetails mode{};
    mode.mode = "addon";
    ok &= expect_true(gbe::dota_custom_game::has_custom_game_details(mode), "mode custom game details");

    GBE_DotaCustomGameDetails map{};
    map.map_name = "map";
    ok &= expect_true(gbe::dota_custom_game::has_custom_game_details(map), "map custom game details");

    GBE_DotaCustomGameDetails id{};
    id.game_id = 42ull;
    ok &= expect_true(gbe::dota_custom_game::has_custom_game_details(id), "id custom game details");

    GBE_DotaCustomGameDetails crc{};
    crc.crc = 42ull;
    ok &= expect_true(gbe::dota_custom_game::has_custom_game_details(crc), "crc custom game details");

    GBE_DotaCustomGameDetails timestamp{};
    timestamp.timestamp = 42u;
    ok &= expect_true(gbe::dota_custom_game::has_custom_game_details(timestamp), "timestamp custom game details");

    return ok;
}

bool test_custom_game_details_equal()
{
    bool ok = true;

    GBE_DotaCustomGameDetails left{};
    left.mode = "mode";
    left.map_name = "map";
    left.difficulty = 1u;
    left.game_id = 2ull;
    left.min_players = 3u;
    left.max_players = 4u;
    left.crc = 5ull;
    left.timestamp = 6u;
    left.penalties = true;

    GBE_DotaCustomGameDetails right = left;
    ok &= expect_true(gbe::dota_custom_game::custom_game_details_equal(left, right), "equal custom game details");

    right.max_players = 5u;
    ok &= expect_true(!gbe::dota_custom_game::custom_game_details_equal(left, right), "different custom game details");

    return ok;
}

bool test_mod_metadata_value_for_gc()
{
    bool ok = true;

    ok &= expect_eq_string(
        gbe::dota_custom_game::metadata_value_for_gc("", "display_name", "Fallback Title"),
        "Fallback Title",
        "empty metadata fallback");

    const std::string metadata = "{\"display_name\":\"Display\",\"map_name\":\"Map\"}";
    ok &= expect_eq_string(
        gbe::dota_custom_game::metadata_value_for_gc(metadata, "display_name", "Fallback Title"),
        "Display",
        "metadata display name");
    ok &= expect_eq_string(
        gbe::dota_custom_game::metadata_value_for_gc(metadata, "missing", "Fallback"),
        "Fallback",
        "metadata missing fallback");

    ok &= expect_eq_string(
        gbe::dota_custom_game::metadata_value_for_gc("{", "display_name", "Fallback"),
        "Fallback",
        "invalid metadata fallback");

    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= test_has_custom_game_details();
    ok &= test_custom_game_details_equal();
    ok &= test_mod_metadata_value_for_gc();

    if (!ok)
        return 1;

    std::cout << "gbe_dota_custom_game_test passed" << std::endl;
    return 0;
}
