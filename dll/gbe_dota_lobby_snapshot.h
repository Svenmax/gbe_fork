#ifndef GBE_DOTA_LOBBY_SNAPSHOT_H
#define GBE_DOTA_LOBBY_SNAPSHOT_H

#include "gbe_dota_types.h"

#include <cstdint>
#include <string>
#include <vector>

namespace gbe::dota_lobby_flow {

GBE_DotaGenericLobbySnapshotScalarData compose_generic_lobby_snapshot_scalar_data(
    const std::string &room_name,
    std::uint32_t game_mode,
    std::uint32_t server_region,
    std::uint32_t state,
    std::uint32_t game_state,
    std::uint64_t match_id,
    std::uint64_t server_id,
    const std::string &connect,
    std::uint32_t game_start_time,
    const std::string &lan_host_ping_location,
    const std::string &pass_key,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    std::uint32_t visibility,
    std::uint32_t bot_difficulty_radiant,
    std::uint32_t bot_difficulty_dire,
    std::uint64_t bot_radiant,
    std::uint64_t bot_dire,
    std::uint64_t tv_secret_code,
    std::uint32_t tv_port);

GBE_DotaLobbyMemberSnapshotData compose_generic_lobby_member_snapshot_data(
    const GBE_DotaGenericLobbyMemberSnapshotInput &input);

void apply_generic_lobby_member_snapshot(
    std::vector<GBE_DotaLobbyMemberState> &members,
    const GBE_DotaLobbyMemberSnapshotData &member_snapshot,
    bool has_custom_game,
    std::uint64_t owner_steam_id,
    std::uint32_t &owner_team,
    std::uint32_t &owner_slot,
    std::uint32_t &owner_hero_id,
    bool &owner_connected,
    std::uint32_t good_guys_team,
    std::uint32_t player_pool_team);

GBE_DotaLobbyOwnerSnapshotData compose_lobby_owner_snapshot_data(
    std::uint64_t stored_owner_steam_id,
    std::uint32_t stored_owner_account_id,
    const std::string &stored_owner_name,
    std::uint64_t generic_owner_steam_id,
    std::uint32_t generic_owner_account_id,
    bool generic_owner_valid,
    std::uint64_t local_steam_id,
    const std::string &local_name,
    std::uint32_t fallback_account_id,
    const std::string &fallback_owner_name);

GBE_DotaLobbyOwnerPublishData compose_lobby_owner_publish_data(
    const GBE_DotaLobbyOwnerSnapshotData &owner_snapshot,
    const std::string &local_name);

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

std::uint32_t resolve_snapshot_lobby_state(
    std::uint32_t state);

std::string resolve_snapshot_room_name(
    const std::string &room_name);

GBE_DotaLobbyMemberSnapshotData compose_lobby_member_snapshot_data(
    std::uint64_t member_steam_id,
    std::uint32_t member_account_id,
    std::uint64_t owner_steam_id,
    std::uint32_t owner_account_id,
    std::uint32_t owner_team,
    std::uint32_t owner_slot,
    std::uint32_t owner_hero_id,
    bool owner_connected,
    bool has_team,
    std::uint32_t parsed_team,
    bool has_slot,
    std::uint32_t parsed_slot,
    std::uint32_t parsed_hero_id,
    bool parsed_connected,
    std::uint32_t player_pool_team);

} // namespace gbe::dota_lobby_flow

#endif // GBE_DOTA_LOBBY_SNAPSHOT_H
