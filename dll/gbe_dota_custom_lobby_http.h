#ifndef GBE_DOTA_CUSTOM_LOBBY_HTTP_H
#define GBE_DOTA_CUSTOM_LOBBY_HTTP_H

#include "gbe_dota_types.h"

#include <cstdint>
#include <json/json.hpp>
#include <string>

namespace gbe::dota_custom_lobby_http {

// Formats joinable custom lobby data for the HTTP surface.

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
    bool penalties_enabled);

} // namespace gbe::dota_custom_lobby_http

#endif // GBE_DOTA_CUSTOM_LOBBY_HTTP_H
