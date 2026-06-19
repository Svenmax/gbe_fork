#ifndef GBE_DOTA_CUSTOM_GAME_H
#define GBE_DOTA_CUSTOM_GAME_H

#include "gbe_dota_types.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gbe::dota_custom_game {

bool has_custom_game_details(const GBE_DotaCustomGameDetails &custom_game);

bool custom_game_details_equal(
    const GBE_DotaCustomGameDetails &left,
    const GBE_DotaCustomGameDetails &right);

std::string metadata_value_for_gc(
    const std::string &metadata_json,
    const char *key,
    const std::string &fallback);

std::string custom_game_display_name_from_details(
    const GBE_DotaCustomGameDetails &custom_game,
    const std::string &fallback);

GBE_DotaCustomGameDetails compose_snapshot_custom_game_details(
    const char *mode,
    const char *map_name,
    const char *difficulty,
    const char *game_id,
    const char *min_players,
    const char *max_players,
    const char *crc,
    const char *timestamp,
    const char *penalties);

GBE_DotaJoinableCustomLobbyItemData compose_joinable_custom_lobby_item_data(
    const GBE_DotaCustomGameDetails &custom_game,
    std::size_t member_count,
    std::uint32_t owner_account_id,
    std::uint32_t local_account_id,
    const std::string &owner_name,
    const std::string &local_name,
    const std::string &room_name,
    std::uint32_t game_start_time,
    std::uint32_t now_time);

GBE_DotaCustomGamePublishData compose_custom_game_publish_data(
    const GBE_DotaCustomGameDetails &custom_game);

bool should_include_joinable_custom_lobby(
    bool active,
    std::uint64_t lobby_id,
    std::uint64_t custom_game_id,
    std::uint64_t requested_custom_game_id,
    const std::vector<std::uint64_t> &seen_lobby_ids);

} // namespace gbe::dota_custom_game

#endif // GBE_DOTA_CUSTOM_GAME_H
