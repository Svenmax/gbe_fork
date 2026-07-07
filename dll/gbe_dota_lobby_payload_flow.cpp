#include "gbe_dota_lobby_payload_flow.h"

namespace gbe::dota_lobby_flow {

bool should_use_current_practice_lobby_payload_for_details_update(
    std::uint64_t match_id,
    bool lan,
    std::size_t member_count,
    std::uint64_t custom_game_id)
{
    if (custom_game_id != 0ull)
        return true;

    const bool launched_lan_with_remote_members = match_id != 0ull && lan && member_count > 1u;
    return (match_id == 0ull || launched_lan_with_remote_members) && member_count > 1u;
}

bool should_use_current_practice_lobby_payload_for_cache_subscribed(
    bool launch_started,
    bool lan,
    std::size_t member_count)
{
    const bool launched_lan_with_remote_members = launch_started && lan && member_count > 1u;
    return (!launch_started || launched_lan_with_remote_members) && member_count > 1u;
}

GBE_DotaAuthoritativeLobbyPayloadData compose_authoritative_lobby_payload_data(
    bool preserve_server_id,
    bool launch_started,
    std::uint64_t current_server_id,
    std::uint64_t server_candidate_id,
    const std::string &formatted_connect)
{
    GBE_DotaAuthoritativeLobbyPayloadData data{};
    data.connect = formatted_connect;
    if (preserve_server_id && launch_started)
        data.server_id = current_server_id != 0ull ? current_server_id : server_candidate_id;
    return data;
}

} // namespace gbe::dota_lobby_flow
