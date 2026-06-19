#include "dll/gbe_dota_custom_game.h"

#include <iostream>
#include <vector>

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

bool expect_eq_u64(unsigned long long actual, unsigned long long expected, const char *label)
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

bool test_compose_snapshot_custom_game_details()
{
    bool ok = true;

    GBE_DotaCustomGameDetails custom_game = gbe::dota_custom_game::compose_snapshot_custom_game_details(
        "addon",
        "map",
        "2",
        "123456789",
        "3",
        "10",
        "987654321",
        "42",
        "1");
    ok &= expect_eq_string(custom_game.mode, "addon", "snapshot custom game mode");
    ok &= expect_eq_string(custom_game.map_name, "map", "snapshot custom game map");
    ok &= expect_eq_u64(custom_game.difficulty, 2u, "snapshot custom game difficulty");
    ok &= expect_eq_u64(custom_game.game_id, 123456789ull, "snapshot custom game id");
    ok &= expect_eq_u64(custom_game.min_players, 3u, "snapshot custom game min players");
    ok &= expect_eq_u64(custom_game.max_players, 10u, "snapshot custom game max players");
    ok &= expect_eq_u64(custom_game.crc, 987654321ull, "snapshot custom game crc");
    ok &= expect_eq_u64(custom_game.timestamp, 42u, "snapshot custom game timestamp");
    ok &= expect_true(custom_game.penalties, "snapshot custom game penalties");

    custom_game = gbe::dota_custom_game::compose_snapshot_custom_game_details(
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr,
        nullptr);
    ok &= expect_eq_string(custom_game.mode, "", "snapshot custom game null mode");
    ok &= expect_eq_string(custom_game.map_name, "", "snapshot custom game null map");
    ok &= expect_eq_u64(custom_game.difficulty, 0u, "snapshot custom game null difficulty");
    ok &= expect_eq_u64(custom_game.game_id, 0ull, "snapshot custom game null id");
    ok &= expect_true(!custom_game.penalties, "snapshot custom game null penalties");

    return ok;
}

bool test_compose_joinable_custom_lobby_item_data()
{
    bool ok = true;

    GBE_DotaCustomGameDetails custom_game{};
    custom_game.mode = "addon_mode";
    GBE_DotaJoinableCustomLobbyItemData item = gbe::dota_custom_game::compose_joinable_custom_lobby_item_data(
        custom_game,
        0u,
        0u,
        55u,
        "",
        "Local User",
        "",
        0u,
        1234u);
    ok &= expect_eq_u64(item.member_count, 1u, "joinable item default member count");
    ok &= expect_eq_u64(item.max_players, 10u, "joinable item default max players");
    ok &= expect_eq_u64(item.min_players, 1u, "joinable item default min players");
    ok &= expect_eq_u64(item.leader_account_id, 55u, "joinable item local leader account fallback");
    ok &= expect_eq_string(item.leader_name, "Local User", "joinable item local leader name fallback");
    ok &= expect_eq_string(item.room_name, "Lobby", "joinable item room fallback");
    ok &= expect_eq_string(item.custom_map_name, "addon_mode", "joinable item map falls back to mode");
    ok &= expect_eq_u64(item.lobby_creation_time, 1234u, "joinable item creation time fallback");

    custom_game.map_name = "custom_map";
    custom_game.min_players = 2u;
    custom_game.max_players = 8u;
    item = gbe::dota_custom_game::compose_joinable_custom_lobby_item_data(
        custom_game,
        4u,
        77u,
        55u,
        "Owner User",
        "Local User",
        "Room Name",
        2222u,
        1234u);
    ok &= expect_eq_u64(item.member_count, 4u, "joinable item member count");
    ok &= expect_eq_u64(item.max_players, 8u, "joinable item max players");
    ok &= expect_eq_u64(item.min_players, 2u, "joinable item min players");
    ok &= expect_eq_u64(item.leader_account_id, 77u, "joinable item owner leader account");
    ok &= expect_eq_string(item.leader_name, "Owner User", "joinable item owner leader name");
    ok &= expect_eq_string(item.room_name, "Room Name", "joinable item room name");
    ok &= expect_eq_string(item.custom_map_name, "custom_map", "joinable item custom map name");
    ok &= expect_eq_u64(item.lobby_creation_time, 2222u, "joinable item creation time");

    item = gbe::dota_custom_game::compose_joinable_custom_lobby_item_data(
        GBE_DotaCustomGameDetails{},
        0u,
        0u,
        0u,
        "",
        "",
        "",
        0u,
        1234u);
    ok &= expect_eq_string(item.leader_name, "Lobby Host", "joinable item host fallback without local name");

    return ok;
}

bool test_compose_custom_game_publish_data()
{
    bool ok = true;

    GBE_DotaCustomGameDetails custom_game{};
    custom_game.mode = "addon";
    custom_game.map_name = "map";
    custom_game.difficulty = 2u;
    custom_game.game_id = 123456789ull;
    custom_game.min_players = 3u;
    custom_game.max_players = 10u;
    custom_game.crc = 987654321ull;
    custom_game.timestamp = 42u;
    custom_game.penalties = true;

    GBE_DotaCustomGamePublishData publish_data = gbe::dota_custom_game::compose_custom_game_publish_data(custom_game);
    ok &= expect_eq_string(publish_data.mode, "addon", "custom game publish mode");
    ok &= expect_eq_string(publish_data.map_name, "map", "custom game publish map");
    ok &= expect_eq_string(publish_data.difficulty, "2", "custom game publish difficulty");
    ok &= expect_eq_string(publish_data.game_id, "123456789", "custom game publish id");
    ok &= expect_eq_string(publish_data.min_players, "3", "custom game publish min players");
    ok &= expect_eq_string(publish_data.max_players, "10", "custom game publish max players");
    ok &= expect_eq_string(publish_data.crc, "987654321", "custom game publish crc");
    ok &= expect_eq_string(publish_data.timestamp, "42", "custom game publish timestamp");
    ok &= expect_eq_string(publish_data.penalties, "1", "custom game publish penalties enabled");

    custom_game.penalties = false;
    publish_data = gbe::dota_custom_game::compose_custom_game_publish_data(custom_game);
    ok &= expect_eq_string(publish_data.penalties, "0", "custom game publish penalties disabled");

    return ok;
}

bool test_should_include_joinable_custom_lobby()
{
    bool ok = true;
    const std::vector<std::uint64_t> empty_seen;
    const std::vector<std::uint64_t> seen{10ull};

    ok &= expect_true(
        gbe::dota_custom_game::should_include_joinable_custom_lobby(true, 10ull, 20ull, 0ull, empty_seen),
        "include active joinable lobby");
    ok &= expect_true(
        !gbe::dota_custom_game::should_include_joinable_custom_lobby(false, 10ull, 20ull, 0ull, empty_seen),
        "exclude inactive joinable lobby");
    ok &= expect_true(
        !gbe::dota_custom_game::should_include_joinable_custom_lobby(true, 0ull, 20ull, 0ull, empty_seen),
        "exclude zero lobby id");
    ok &= expect_true(
        !gbe::dota_custom_game::should_include_joinable_custom_lobby(true, 10ull, 0ull, 0ull, empty_seen),
        "exclude zero custom game id");
    ok &= expect_true(
        gbe::dota_custom_game::should_include_joinable_custom_lobby(true, 10ull, 20ull, 20ull, empty_seen),
        "include requested custom game match");
    ok &= expect_true(
        !gbe::dota_custom_game::should_include_joinable_custom_lobby(true, 10ull, 20ull, 30ull, empty_seen),
        "exclude requested custom game mismatch");
    ok &= expect_true(
        !gbe::dota_custom_game::should_include_joinable_custom_lobby(true, 10ull, 20ull, 0ull, seen),
        "exclude seen lobby id");

    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= test_has_custom_game_details();
    ok &= test_custom_game_details_equal();
    ok &= test_mod_metadata_value_for_gc();
    ok &= test_compose_snapshot_custom_game_details();
    ok &= test_compose_joinable_custom_lobby_item_data();
    ok &= test_compose_custom_game_publish_data();
    ok &= test_should_include_joinable_custom_lobby();

    if (!ok)
        return 1;

    std::cout << "gbe_dota_custom_game_test passed" << std::endl;
    return 0;
}
