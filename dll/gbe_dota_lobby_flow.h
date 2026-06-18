#ifndef GBE_DOTA_LOBBY_FLOW_H
#define GBE_DOTA_LOBBY_FLOW_H

#include "gbe_dota_types.h"

#include <cstddef>
#include <cstdint>
#include <vector>

namespace gbe::dota_lobby_flow {

bool lobby_members_equal(
    const std::vector<GBE_DotaLobbyMemberState> &left,
    const std::vector<GBE_DotaLobbyMemberState> &right);

bool lobby_members_contain_steam_id(
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t steam_id);

bool find_lobby_member_index(
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t steam_id,
    std::size_t &index);

std::vector<GBE_DotaLobbyMemberState> find_joined_lobby_members(
    const std::vector<GBE_DotaLobbyMemberState> &previous_members,
    const std::vector<GBE_DotaLobbyMemberState> &current_members);

std::vector<GBE_DotaLobbyMemberState> filter_nonzero_lobby_members(
    const std::vector<GBE_DotaLobbyMemberState> &members);

void preserve_lobby_owner_transfer_slots(
    std::vector<GBE_DotaLobbyMemberState> &members,
    const std::vector<GBE_DotaLobbyMemberState> &previous_members,
    std::uint64_t previous_owner_steam_id,
    std::uint64_t new_owner_steam_id);

void upsert_lobby_member(
    std::vector<GBE_DotaLobbyMemberState> &members,
    const GBE_DotaLobbyMemberState &member);

std::vector<GBE_DotaLobbyMemberState> compose_lobby_members(
    std::uint64_t owner_steam_id,
    std::uint32_t owner_account_id,
    std::uint32_t owner_team,
    std::uint32_t owner_slot,
    std::uint32_t owner_hero_id,
    bool owner_connected,
    std::uint32_t player_pool_team,
    const std::vector<GBE_DotaLobbyMemberState> &members);

std::uint32_t select_arcade_lobby_member_slot(
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t steam_id,
    std::uint32_t owner_slot);

bool normalize_arcade_lobby_member_slot(
    GBE_DotaLobbyMemberState &member,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t owner_steam_id,
    std::uint32_t owner_slot,
    std::uint32_t good_guys_team,
    std::uint32_t player_pool_team);

bool normalize_arcade_lobby_member_slots(
    bool has_custom_game,
    std::uint64_t owner_steam_id,
    std::uint32_t &owner_team,
    std::uint32_t &owner_slot,
    std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint32_t good_guys_team);

} // namespace gbe::dota_lobby_flow

#endif // GBE_DOTA_LOBBY_FLOW_H
