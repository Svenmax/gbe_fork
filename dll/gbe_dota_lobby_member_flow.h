#ifndef GBE_DOTA_LOBBY_MEMBER_FLOW_H
#define GBE_DOTA_LOBBY_MEMBER_FLOW_H

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

std::uint64_t find_lobby_member_steam_id_by_account_id(
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint32_t account_id);

bool clear_lobby_member_by_account_id(
    std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint32_t account_id);

void upsert_lobby_member(
    std::vector<GBE_DotaLobbyMemberState> &members,
    const GBE_DotaLobbyMemberState &member);

std::vector<GBE_DotaLobbyMemberState> find_joined_lobby_members(
    const std::vector<GBE_DotaLobbyMemberState> &previous_members,
    const std::vector<GBE_DotaLobbyMemberState> &current_members);

std::vector<GBE_DotaLobbyMemberState> filter_nonzero_lobby_members(
    const std::vector<GBE_DotaLobbyMemberState> &members);

void count_remote_lobby_members(
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t owner_steam_id,
    std::uint32_t &remote_count,
    std::uint32_t &connected_remote_count);

bool should_hold_lan_launch_for_remote_members(
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t owner_steam_id,
    std::uint32_t &remote_count,
    std::uint32_t &connected_remote_count);

} // namespace gbe::dota_lobby_flow

#endif // GBE_DOTA_LOBBY_MEMBER_FLOW_H
