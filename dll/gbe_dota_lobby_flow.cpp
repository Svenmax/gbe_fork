#include "gbe_dota_lobby_flow.h"

#include <algorithm>

namespace gbe::dota_lobby_flow {

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

void preserve_lobby_owner_transfer_slots(
    std::vector<GBE_DotaLobbyMemberState> &members,
    const std::vector<GBE_DotaLobbyMemberState> &previous_members,
    std::uint64_t previous_owner_steam_id,
    std::uint64_t new_owner_steam_id)
{
    if (previous_owner_steam_id == 0ull || new_owner_steam_id == 0ull || previous_owner_steam_id == new_owner_steam_id)
        return;

    std::size_t previous_owner_index = 0;
    std::size_t previous_new_owner_index = 0;
    if (!find_lobby_member_index(previous_members, previous_owner_steam_id, previous_owner_index) ||
            !find_lobby_member_index(previous_members, new_owner_steam_id, previous_new_owner_index))
        return;

    std::size_t current_new_owner_index = 0;
    if (!find_lobby_member_index(members, new_owner_steam_id, current_new_owner_index))
        return;

    const GBE_DotaLobbyMemberState new_owner = members[current_new_owner_index];
    std::vector<GBE_DotaLobbyMemberState> reordered(std::max(previous_members.size(), previous_new_owner_index + 1));
    std::vector<bool> occupied(reordered.size(), false);

    if (previous_owner_index < reordered.size()) {
        reordered[previous_owner_index] = GBE_DotaLobbyMemberState{};
        occupied[previous_owner_index] = true;
    }

    reordered[previous_new_owner_index] = new_owner;
    occupied[previous_new_owner_index] = true;

    for (const GBE_DotaLobbyMemberState &member : members) {
        if (member.steam_id == 0ull || member.steam_id == new_owner_steam_id || member.steam_id == previous_owner_steam_id)
            continue;

        std::size_t previous_index = 0;
        if (find_lobby_member_index(previous_members, member.steam_id, previous_index)) {
            if (previous_index >= reordered.size()) {
                reordered.resize(previous_index + 1);
                occupied.resize(previous_index + 1, false);
            }
            if (!occupied[previous_index]) {
                reordered[previous_index] = member;
                occupied[previous_index] = true;
                continue;
            }
        }

        reordered.push_back(member);
        occupied.push_back(true);
    }

    while (!reordered.empty() && reordered.back().steam_id == 0ull)
        reordered.pop_back();

    members = reordered;
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

std::vector<GBE_DotaLobbyMemberState> compose_lobby_members(
    std::uint64_t owner_steam_id,
    std::uint32_t owner_account_id,
    std::uint32_t owner_team,
    std::uint32_t owner_slot,
    std::uint32_t owner_hero_id,
    bool owner_connected,
    std::uint32_t player_pool_team,
    const std::vector<GBE_DotaLobbyMemberState> &members)
{
    std::vector<GBE_DotaLobbyMemberState> result;

    auto append_or_update = [&result](GBE_DotaLobbyMemberState member) {
        if (member.steam_id == 0ull) {
            result.push_back(member);
            return;
        }
        if (member.account_id == 0u)
            member.account_id = static_cast<std::uint32_t>(member.steam_id & 0xffffffffu);
        for (GBE_DotaLobbyMemberState &existing : result) {
            if (existing.steam_id == member.steam_id) {
                existing = member;
                return;
            }
        }
        result.push_back(member);
    };

    bool has_owner = false;
    for (const GBE_DotaLobbyMemberState &member : members) {
        if (member.steam_id == owner_steam_id) {
            has_owner = true;
            break;
        }
    }

    if (!has_owner) {
        GBE_DotaLobbyMemberState owner{};
        owner.steam_id = owner_steam_id;
        owner.account_id = owner_account_id;
        owner.team = owner_team;
        owner.slot = owner_slot;
        owner.hero_id = owner_hero_id;
        owner.connected = owner_connected;
        append_or_update(owner);
    }

    for (GBE_DotaLobbyMemberState member : members) {
        if (member.steam_id == owner_steam_id) {
            member.account_id = owner_account_id;
            member.team = owner_team;
            member.slot = owner_slot;
            member.hero_id = owner_hero_id;
            member.connected = owner_connected;
        } else if (member.team == player_pool_team) {
            member.slot = 0u;
        }
        append_or_update(member);
    }

    return result;
}

std::uint32_t select_arcade_lobby_member_slot(
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t steam_id,
    std::uint32_t owner_slot)
{
    if (owner_slot == 0u)
        owner_slot = 1u;

    for (const GBE_DotaLobbyMemberState &member : members) {
        if (member.steam_id == steam_id && member.slot != 0u)
            return member.slot;
    }

    bool occupied[11]{};
    if (owner_slot <= 10u)
        occupied[owner_slot] = true;
    for (const GBE_DotaLobbyMemberState &member : members) {
        if (member.steam_id == 0ull || member.steam_id == steam_id)
            continue;
        if (member.slot > 0u && member.slot <= 10u)
            occupied[member.slot] = true;
    }

    for (std::uint32_t slot = 1u; slot <= 10u; ++slot) {
        if (!occupied[slot])
            return slot;
    }

    return owner_slot;
}

bool normalize_arcade_lobby_member_slot(
    GBE_DotaLobbyMemberState &member,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t owner_steam_id,
    std::uint32_t owner_slot,
    std::uint32_t good_guys_team,
    std::uint32_t player_pool_team)
{
    if (member.steam_id == 0ull || member.steam_id == owner_steam_id)
        return false;
    if (member.team != player_pool_team && member.slot != 0u)
        return false;

    member.team = good_guys_team;
    member.slot = select_arcade_lobby_member_slot(members, member.steam_id, owner_slot);
    return true;
}

bool normalize_arcade_lobby_member_slots(
    bool has_custom_game,
    std::uint64_t owner_steam_id,
    std::uint32_t &owner_team,
    std::uint32_t &owner_slot,
    std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint32_t good_guys_team)
{
    if (!has_custom_game)
        return false;

    bool changed = false;
    if (owner_team != good_guys_team) {
        owner_team = good_guys_team;
        changed = true;
    }
    if (owner_slot != 1u) {
        owner_slot = 1u;
        changed = true;
    }

    std::uint32_t next_slot = 2u;
    for (GBE_DotaLobbyMemberState &member : members) {
        if (member.steam_id == 0ull)
            continue;
        if (member.steam_id == owner_steam_id) {
            if (member.team != owner_team) {
                member.team = owner_team;
                changed = true;
            }
            if (member.slot != owner_slot) {
                member.slot = owner_slot;
                changed = true;
            }
            continue;
        }

        if (member.team != good_guys_team) {
            member.team = good_guys_team;
            changed = true;
        }
        if (member.slot != next_slot) {
            member.slot = next_slot;
            changed = true;
        }
        if (next_slot < 10u)
            ++next_slot;
    }

    return changed;
}

} // namespace gbe::dota_lobby_flow
