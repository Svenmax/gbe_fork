#ifndef GBE_DOTA_CUSTOM_GAME_H
#define GBE_DOTA_CUSTOM_GAME_H

#include "gbe_dota_types.h"

#include <string>

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

} // namespace gbe::dota_custom_game

#endif // GBE_DOTA_CUSTOM_GAME_H
