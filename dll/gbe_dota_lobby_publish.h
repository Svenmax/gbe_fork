#ifndef GBE_DOTA_LOBBY_PUBLISH_H
#define GBE_DOTA_LOBBY_PUBLISH_H

#include "gbe_dota_types.h"

#include <cstdint>
#include <string>

namespace gbe::dota_lobby_flow {

GBE_DotaLobbyMemberPublishData compose_lobby_member_publish_data(
    const GBE_DotaLobbyMemberState &member,
    const std::string &member_name);

GBE_DotaLobbyMetadataPublishData compose_lobby_metadata_publish_data(
    const std::string &room_name,
    const std::string &owner_name,
    const std::string &fallback_owner_name,
    std::uint64_t match_id,
    std::uint64_t server_id,
    const std::string &normalized_connect);

GBE_DotaLobbyOptionsPublishData compose_lobby_options_publish_data(
    std::uint32_t game_mode,
    std::uint32_t server_region,
    const std::string &lan_host_ping_location,
    const std::string &pass_key,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    std::uint32_t visibility,
    std::uint32_t bot_difficulty_radiant,
    std::uint32_t bot_difficulty_dire,
    std::uint64_t bot_radiant,
    std::uint64_t bot_dire);

GBE_DotaLobbyScalarPublishData compose_lobby_scalar_publish_data(
    std::uint64_t dota_lobby_id,
    std::uint64_t owner_steam_id,
    std::uint64_t owner_account_id,
    std::uint32_t state,
    std::uint32_t game_state,
    std::uint64_t match_id,
    std::uint32_t game_start_time,
    std::uint32_t tv_secret_code,
    std::uint32_t tv_port);

bool should_clear_lobby_server_id_for_metadata_publish(
    std::uint64_t match_id);

} // namespace gbe::dota_lobby_flow

#endif // GBE_DOTA_LOBBY_PUBLISH_H
