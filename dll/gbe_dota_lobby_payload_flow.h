#ifndef GBE_DOTA_LOBBY_PAYLOAD_FLOW_H
#define GBE_DOTA_LOBBY_PAYLOAD_FLOW_H

#include "gbe_dota_types.h"

#include <cstddef>
#include <cstdint>
#include <string>

namespace gbe::dota_lobby_flow {

bool should_use_current_practice_lobby_payload_for_details_update(
    std::uint64_t match_id,
    bool lan,
    std::size_t member_count,
    std::uint64_t custom_game_id);

bool should_use_current_practice_lobby_payload_for_cache_subscribed(
    bool launch_started,
    bool lan,
    std::size_t member_count);

GBE_DotaAuthoritativeLobbyPayloadData compose_authoritative_lobby_payload_data(
    bool preserve_server_id,
    bool launch_started,
    std::uint64_t current_server_id,
    std::uint64_t server_candidate_id,
    const std::string &formatted_connect);

} // namespace gbe::dota_lobby_flow

#endif // GBE_DOTA_LOBBY_PAYLOAD_FLOW_H
