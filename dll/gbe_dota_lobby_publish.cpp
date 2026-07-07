#include "gbe_dota_lobby_publish.h"

namespace gbe::dota_lobby_flow {

GBE_DotaLobbyMemberPublishData compose_lobby_member_publish_data(
    const GBE_DotaLobbyMemberState &member,
    const std::string &member_name)
{
    GBE_DotaLobbyMemberPublishData data{};
    data.team = member.team;
    data.slot = member.slot;
    data.hero_id = member.hero_id;
    data.connected = member.connected;
    data.name = member_name;
    return data;
}

GBE_DotaLobbyMetadataPublishData compose_lobby_metadata_publish_data(
    const std::string &room_name,
    const std::string &owner_name,
    const std::string &fallback_owner_name,
    std::uint64_t match_id,
    std::uint64_t server_id,
    const std::string &normalized_connect)
{
    GBE_DotaLobbyMetadataPublishData data{};
    data.room_name = room_name.empty() ? std::string("Lobby") : room_name;
    data.owner_name = owner_name.empty() ? fallback_owner_name : owner_name;
    data.server_id = should_clear_lobby_server_id_for_metadata_publish(match_id) ? 0ull : server_id;
    data.connect = normalized_connect;
    return data;
}

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
    std::uint64_t bot_dire)
{
    GBE_DotaLobbyOptionsPublishData data{};
    data.game_mode = std::to_string(game_mode);
    data.server_region = std::to_string(server_region);
    data.lan_host_ping_location = lan_host_ping_location;
    data.pass_key = pass_key;
    data.allow_cheats = allow_cheats ? "1" : "0";
    data.fill_with_bots = fill_with_bots ? "1" : "0";
    data.allow_spectating = allow_spectating ? "1" : "0";
    data.visibility = std::to_string(visibility);
    data.bot_difficulty_radiant = std::to_string(bot_difficulty_radiant);
    data.bot_difficulty_dire = std::to_string(bot_difficulty_dire);
    data.bot_radiant = std::to_string(bot_radiant);
    data.bot_dire = std::to_string(bot_dire);
    return data;
}

GBE_DotaLobbyScalarPublishData compose_lobby_scalar_publish_data(
    std::uint64_t dota_lobby_id,
    std::uint64_t owner_steam_id,
    std::uint64_t owner_account_id,
    std::uint32_t state,
    std::uint32_t game_state,
    std::uint64_t match_id,
    std::uint32_t game_start_time,
    std::uint32_t tv_secret_code,
    std::uint32_t tv_port)
{
    GBE_DotaLobbyScalarPublishData data{};
    data.dota_lobby_id = std::to_string(dota_lobby_id);
    data.owner_steam_id = std::to_string(owner_steam_id);
    data.owner_account_id = std::to_string(owner_account_id);
    data.state = std::to_string(state);
    data.game_state = std::to_string(game_state);
    data.match_id = std::to_string(match_id);
    data.game_start_time = std::to_string(game_start_time);
    data.tv_secret_code = std::to_string(tv_secret_code);
    data.tv_port = std::to_string(tv_port);
    return data;
}

bool should_clear_lobby_server_id_for_metadata_publish(
    std::uint64_t match_id)
{
    return match_id == 0ull;
}

} // namespace gbe::dota_lobby_flow
