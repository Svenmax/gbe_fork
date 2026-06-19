#include "gbe_dota_custom_game.h"

#include "gbe_proto_wire.h"

#include <algorithm>

#include <json/json.hpp>

namespace gbe::dota_custom_game {

bool has_custom_game_details(const GBE_DotaCustomGameDetails &custom_game)
{
    return !custom_game.mode.empty() ||
        !custom_game.map_name.empty() ||
        custom_game.game_id != 0ull ||
        custom_game.crc != 0ull ||
        custom_game.timestamp != 0u;
}

bool custom_game_details_equal(
    const GBE_DotaCustomGameDetails &left,
    const GBE_DotaCustomGameDetails &right)
{
    return left.mode == right.mode &&
        left.map_name == right.map_name &&
        left.difficulty == right.difficulty &&
        left.game_id == right.game_id &&
        left.min_players == right.min_players &&
        left.max_players == right.max_players &&
        left.crc == right.crc &&
        left.timestamp == right.timestamp &&
        left.penalties == right.penalties;
}

std::string metadata_value_for_gc(
    const std::string &metadata_json,
    const char *key,
    const std::string &fallback)
{
    if (metadata_json.empty())
        return fallback;

    try {
        nlohmann::json metadata = nlohmann::json::parse(metadata_json);
        return metadata.value(key, fallback);
    } catch (...) {
        return fallback;
    }
}

std::string custom_game_display_name_from_details(
    const GBE_DotaCustomGameDetails &custom_game,
    const std::string &fallback)
{
    if (proto_wire::dota_is_readable_custom_game_name(custom_game.map_name))
        return custom_game.map_name;
    if (proto_wire::dota_is_readable_custom_game_name(custom_game.mode))
        return custom_game.mode;
    return fallback.empty() ? std::string("Lobby") : fallback;
}

GBE_DotaCustomGameDetails compose_snapshot_custom_game_details(
    const char *mode,
    const char *map_name,
    const char *difficulty,
    const char *game_id,
    const char *min_players,
    const char *max_players,
    const char *crc,
    const char *timestamp,
    const char *penalties)
{
    GBE_DotaCustomGameDetails custom_game{};
    custom_game.mode = mode ? mode : "";
    custom_game.map_name = map_name ? map_name : "";
    custom_game.difficulty = proto_wire::parse_uint32_or_zero(difficulty);
    custom_game.game_id = proto_wire::parse_uint64_or_zero(game_id);
    custom_game.min_players = proto_wire::parse_uint32_or_zero(min_players);
    custom_game.max_players = proto_wire::parse_uint32_or_zero(max_players);
    custom_game.crc = proto_wire::parse_uint64_or_zero(crc);
    custom_game.timestamp = proto_wire::parse_uint32_or_zero(timestamp);
    custom_game.penalties = proto_wire::parse_uint32_or_zero(penalties) != 0u;
    return custom_game;
}

GBE_DotaJoinableCustomLobbyItemData compose_joinable_custom_lobby_item_data(
    const GBE_DotaCustomGameDetails &custom_game,
    std::size_t member_count,
    std::uint32_t owner_account_id,
    std::uint32_t local_account_id,
    const std::string &owner_name,
    const std::string &local_name,
    const std::string &room_name,
    std::uint32_t game_start_time,
    std::uint32_t now_time)
{
    GBE_DotaJoinableCustomLobbyItemData item{};
    item.member_count = static_cast<std::uint32_t>(member_count == 0u ? 1u : member_count);
    item.max_players = custom_game.max_players != 0u ? custom_game.max_players : 10u;
    item.min_players = custom_game.min_players != 0u ? custom_game.min_players : 1u;
    item.leader_account_id = owner_account_id != 0u ? owner_account_id : local_account_id;
    item.leader_name = owner_name.empty() ? (local_name.empty() ? std::string("Lobby Host") : local_name) : owner_name;
    item.room_name = room_name.empty() ? std::string("Lobby") : room_name;
    item.custom_map_name = custom_game.map_name.empty() ? custom_game.mode : custom_game.map_name;
    item.lobby_creation_time = game_start_time != 0u ? game_start_time : now_time;
    return item;
}

GBE_DotaCustomGamePublishData compose_custom_game_publish_data(
    const GBE_DotaCustomGameDetails &custom_game)
{
    GBE_DotaCustomGamePublishData data{};
    data.mode = custom_game.mode;
    data.map_name = custom_game.map_name;
    data.difficulty = std::to_string(custom_game.difficulty);
    data.game_id = std::to_string(custom_game.game_id);
    data.min_players = std::to_string(custom_game.min_players);
    data.max_players = std::to_string(custom_game.max_players);
    data.crc = std::to_string(custom_game.crc);
    data.timestamp = std::to_string(custom_game.timestamp);
    data.penalties = custom_game.penalties ? "1" : "0";
    return data;
}

bool should_include_joinable_custom_lobby(
    bool active,
    std::uint64_t lobby_id,
    std::uint64_t custom_game_id,
    std::uint64_t requested_custom_game_id,
    const std::vector<std::uint64_t> &seen_lobby_ids)
{
    if (!active || lobby_id == 0ull || custom_game_id == 0ull)
        return false;
    if (requested_custom_game_id != 0ull && requested_custom_game_id != custom_game_id)
        return false;
    return std::find(seen_lobby_ids.begin(), seen_lobby_ids.end(), lobby_id) == seen_lobby_ids.end();
}

} // namespace gbe::dota_custom_game
