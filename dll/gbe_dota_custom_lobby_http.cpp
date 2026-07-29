#include "gbe_dota_custom_lobby_http.h"

namespace gbe::dota_custom_lobby_http {

nlohmann::json compose_joinable_custom_lobby_json_item(
    const GBE_DotaJoinableCustomLobbyItemData &item_data,
    const GBE_DotaCustomGameDetails &custom_game,
    const std::string &display_name,
    std::uint64_t lobby_id,
    std::uint64_t custom_game_id,
    std::uint32_t server_region,
    bool has_pass_key,
    const std::string &lan_host_ping_location,
    std::uint32_t custom_game_timestamp,
    const std::string &custom_game_crc,
    bool penalties_enabled)
{
    nlohmann::json item = nlohmann::json::object();
    item["lobby_id"] = std::to_string(lobby_id);
    item["custom_game_id"] = std::to_string(custom_game_id);
    item["member_count"] = item_data.member_count;
    item["leader_account_id"] = item_data.leader_account_id;
    item["leader_name"] = item_data.leader_name;
    item["custom_map_name"] = item_data.custom_map_name;
    item["max_player_count"] = item_data.max_players;
    item["server_region"] = server_region;
    item["has_pass_key"] = has_pass_key;
    item["lobby_creation_time"] = item_data.lobby_creation_time;
    item["custom_game_timestamp"] = custom_game_timestamp;
    item["custom_game_crc"] = custom_game_crc;
    item["min_player_count"] = item_data.min_players;
    item["penalties_enabled"] = penalties_enabled;
    item["name"] = display_name;
    item["display_name"] = display_name;
    item["title"] = display_name;
    item["room_name"] = item_data.room_name;
    item["custom_game_mode"] = custom_game.mode;
    item["custom_game_mode_name"] = display_name;
    item["lan_host_ping_location"] = lan_host_ping_location;
    return item;
}

} // namespace gbe::dota_custom_lobby_http
