#include "gbe_dota_lobby_snapshot.h"

#include "gbe_dota_lobby_flow.h"

#include <climits>
#include <cstdlib>

namespace gbe::dota_lobby_flow {

namespace {

std::uint32_t parse_uint32_or_zero(
    const std::string &text)
{
    if (text.empty())
        return 0u;

    char *end = nullptr;
    const unsigned long long value = std::strtoull(text.c_str(), &end, 10);
    if (!end || *end != '\0' || value > UINT32_MAX)
        return 0u;
    return static_cast<std::uint32_t>(value);
}

} // namespace

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
    std::uint32_t tv_port)
{
    GBE_DotaGenericLobbySnapshotScalarData data{};
    data.room_name = resolve_snapshot_room_name(room_name);
    data.game_mode = game_mode;
    data.server_region = server_region;
    data.lan = true;
    data.lan_host_ping_location = lan_host_ping_location;
    data.allow_cheats = allow_cheats;
    data.fill_with_bots = fill_with_bots;
    data.allow_spectating = allow_spectating;
    data.visibility = visibility;
    data.bot_difficulty_radiant = bot_difficulty_radiant;
    data.bot_difficulty_dire = bot_difficulty_dire;
    data.bot_radiant = bot_radiant;
    data.bot_dire = bot_dire;
    data.state = resolve_snapshot_lobby_state(state);
    data.game_state = game_state;
    data.match_id = match_id;
    data.server_id = server_id;
    data.connect = connect;
    data.game_start_time = game_start_time;
    data.pass_key = pass_key;
    data.tv_secret_code = tv_secret_code;
    data.tv_port = tv_port;
    return data;
}

GBE_DotaLobbyMemberSnapshotData compose_generic_lobby_member_snapshot_data(
    const GBE_DotaGenericLobbyMemberSnapshotInput &input)
{
    const bool has_team = !input.member_team_raw.empty();
    const bool has_slot = !input.member_slot_raw.empty();
    return compose_lobby_member_snapshot_data(
        input.member_steam_id,
        input.member_account_id,
        input.owner_steam_id,
        input.owner_account_id,
        input.owner_team,
        input.owner_slot,
        input.owner_hero_id,
        input.owner_connected,
        has_team,
        parse_uint32_or_zero(input.member_team_raw),
        has_slot,
        parse_uint32_or_zero(input.member_slot_raw),
        parse_uint32_or_zero(input.member_hero_raw),
        parse_uint32_or_zero(input.member_connected_raw) != 0u,
        input.player_pool_team);
}

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
    std::uint32_t player_pool_team)
{
    GBE_DotaLobbyMemberState member = member_snapshot.member;
    owner_team = member_snapshot.owner_team;
    owner_slot = member_snapshot.owner_slot;
    owner_hero_id = member_snapshot.owner_hero_id;
    owner_connected = member_snapshot.owner_connected;
    if (member.steam_id != owner_steam_id && has_custom_game)
        normalize_arcade_lobby_member_slot(member, members, owner_steam_id, owner_slot, good_guys_team, player_pool_team);
    upsert_lobby_member(members, member);
}

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
    const std::string &fallback_owner_name)
{
    GBE_DotaLobbyOwnerSnapshotData data{};
    data.owner_steam_id = stored_owner_steam_id;
    if (generic_owner_valid && generic_owner_steam_id != 0ull)
        data.owner_steam_id = generic_owner_steam_id;

    data.owner_account_id = stored_owner_account_id;
    if (generic_owner_valid && generic_owner_steam_id == data.owner_steam_id)
        data.owner_account_id = generic_owner_account_id;
    if (data.owner_account_id == 0u)
        data.owner_account_id = fallback_account_id;

    data.owner_name = stored_owner_name;
    if (generic_owner_valid && generic_owner_steam_id == local_steam_id) {
        data.owner_name = local_name;
        data.should_publish_local_owner = true;
    }
    if (data.owner_name.empty())
        data.owner_name = fallback_owner_name.empty() ? std::string("Lobby Host") : fallback_owner_name;

    return data;
}

GBE_DotaLobbyOwnerPublishData compose_lobby_owner_publish_data(
    const GBE_DotaLobbyOwnerSnapshotData &owner_snapshot,
    const std::string &local_name)
{
    GBE_DotaLobbyOwnerPublishData data{};
    data.owner_steam_id = owner_snapshot.owner_steam_id;
    data.owner_account_id = owner_snapshot.owner_account_id;
    data.owner_name = owner_snapshot.should_publish_local_owner ? local_name : owner_snapshot.owner_name;
    data.should_publish_local_owner = owner_snapshot.should_publish_local_owner;
    return data;
}

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

std::uint32_t resolve_snapshot_lobby_state(
    std::uint32_t state)
{
    return state == 0u ? 1u : state;
}

std::string resolve_snapshot_room_name(
    const std::string &room_name)
{
    return room_name.empty() ? std::string("Lobby") : room_name;
}

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
    std::uint32_t player_pool_team)
{
    GBE_DotaLobbyMemberSnapshotData data{};
    data.owner_team = owner_team;
    data.owner_slot = owner_slot;
    data.owner_hero_id = owner_hero_id;
    data.owner_connected = owner_connected;

    data.member.steam_id = member_steam_id;
    data.member.account_id = member_account_id;
    data.member.connected = false;

    if (owner_steam_id != 0ull && member_steam_id == owner_steam_id) {
        data.member.account_id = owner_account_id != 0u ? owner_account_id : member_account_id;
        data.member.team = owner_team;
        data.member.slot = owner_slot;
        data.member.hero_id = owner_hero_id;
        if (has_team) {
            data.member.team = parsed_team;
            data.owner_team = parsed_team;
        }
        if (has_slot) {
            data.member.slot = parsed_slot;
            data.owner_slot = parsed_slot;
        }
        data.member.hero_id = parsed_hero_id;
        data.member.connected = parsed_connected;
        data.owner_hero_id = data.member.hero_id;
        data.owner_connected = data.member.connected;
        return data;
    }

    data.member.team = has_team ? parsed_team : player_pool_team;
    data.member.slot = parsed_slot;
    data.member.hero_id = parsed_hero_id;
    data.member.connected = parsed_connected;
    return data;
}

} // namespace gbe::dota_lobby_flow
