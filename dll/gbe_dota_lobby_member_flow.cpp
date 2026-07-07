#include "gbe_dota_lobby_member_flow.h"

namespace gbe::dota_lobby_flow {

namespace {

std::uint32_t member_account_id_or_steam_account_id(
    const GBE_DotaLobbyMemberState &member)
{
    return member.account_id != 0u ? member.account_id : static_cast<std::uint32_t>(member.steam_id & 0xffffffffu);
}

} // namespace

bool lobby_members_equal(
    const std::vector<GBE_DotaLobbyMemberState> &left,
    const std::vector<GBE_DotaLobbyMemberState> &right)
{
    if (left.size() != right.size())
        return false;

    for (std::size_t i = 0; i < left.size(); ++i) {
        if (left[i].steam_id != right[i].steam_id ||
                left[i].account_id != right[i].account_id ||
                left[i].team != right[i].team ||
                left[i].slot != right[i].slot ||
                left[i].hero_id != right[i].hero_id ||
                left[i].connected != right[i].connected ||
                left[i].leaver_status != right[i].leaver_status)
            return false;
    }

    return true;
}

bool lobby_members_contain_steam_id(
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t steam_id)
{
    std::size_t index = 0;
    return find_lobby_member_index(members, steam_id, index);
}

void count_remote_lobby_members(
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t owner_steam_id,
    std::uint32_t &remote_count,
    std::uint32_t &connected_remote_count)
{
    remote_count = 0u;
    connected_remote_count = 0u;

    for (const GBE_DotaLobbyMemberState &member : members) {
        if (member.steam_id == 0ull || member.steam_id == owner_steam_id)
            continue;
        ++remote_count;
        if (member.connected)
            ++connected_remote_count;
    }
}

bool should_hold_lan_launch_for_remote_members(
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t owner_steam_id,
    std::uint32_t &remote_count,
    std::uint32_t &connected_remote_count)
{
    count_remote_lobby_members(members, owner_steam_id, remote_count, connected_remote_count);
    return remote_count != 0u && connected_remote_count < remote_count;
}

bool find_lobby_member_index(
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t steam_id,
    std::size_t &index)
{
    if (steam_id == 0ull)
        return false;

    for (std::size_t i = 0; i < members.size(); ++i) {
        if (members[i].steam_id == steam_id) {
            index = i;
            return true;
        }
    }

    return false;
}

std::uint64_t find_lobby_member_steam_id_by_account_id(
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint32_t account_id)
{
    if (account_id == 0u)
        return 0ull;

    for (const GBE_DotaLobbyMemberState &member : members) {
        const std::uint32_t member_account_id = member_account_id_or_steam_account_id(member);
        if (member.steam_id != 0ull && member_account_id == account_id)
            return member.steam_id;
    }

    return 0ull;
}

bool clear_lobby_member_by_account_id(
    std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint32_t account_id)
{
    if (account_id == 0u)
        return false;

    for (GBE_DotaLobbyMemberState &member : members) {
        const std::uint32_t member_account_id = member_account_id_or_steam_account_id(member);
        if (member.steam_id != 0ull && member_account_id == account_id) {
            member = GBE_DotaLobbyMemberState{};
            return true;
        }
    }

    return false;
}

std::vector<GBE_DotaLobbyMemberState> find_joined_lobby_members(
    const std::vector<GBE_DotaLobbyMemberState> &previous_members,
    const std::vector<GBE_DotaLobbyMemberState> &current_members)
{
    std::vector<GBE_DotaLobbyMemberState> joined_members;
    for (const GBE_DotaLobbyMemberState &member : current_members) {
        if (member.steam_id == 0ull)
            continue;
        if (!lobby_members_contain_steam_id(previous_members, member.steam_id))
            joined_members.push_back(member);
    }

    return joined_members;
}

std::vector<GBE_DotaLobbyMemberState> filter_nonzero_lobby_members(
    const std::vector<GBE_DotaLobbyMemberState> &members)
{
    std::vector<GBE_DotaLobbyMemberState> result;
    result.reserve(members.size());
    for (const GBE_DotaLobbyMemberState &member : members) {
        if (member.steam_id != 0ull)
            result.push_back(member);
    }

    return result;
}

void upsert_lobby_member(
    std::vector<GBE_DotaLobbyMemberState> &members,
    const GBE_DotaLobbyMemberState &member)
{
    if (member.steam_id == 0ull)
        return;

    for (GBE_DotaLobbyMemberState &existing : members) {
        if (existing.steam_id == member.steam_id) {
            existing = member;
            return;
        }
    }

    members.push_back(member);
}

} // namespace gbe::dota_lobby_flow
