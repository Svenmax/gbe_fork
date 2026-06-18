#include "gbe_dota_custom_game.h"

#include "gbe_proto_wire.h"

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

} // namespace gbe::dota_custom_game
