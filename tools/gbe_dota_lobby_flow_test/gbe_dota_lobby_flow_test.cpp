#include "dll/gbe_dota_lobby_flow.h"

#include <iostream>

namespace {

constexpr std::uint32_t kGoodGuys = 0u;
constexpr std::uint32_t kPlayerPool = 4u;

GBE_DotaLobbyMemberState member(std::uint64_t steam_id, std::uint32_t slot)
{
    GBE_DotaLobbyMemberState value{};
    value.steam_id = steam_id;
    value.account_id = static_cast<std::uint32_t>(steam_id & 0xffffffffu);
    value.team = kGoodGuys;
    value.slot = slot;
    value.hero_id = slot + 10u;
    value.connected = true;
    return value;
}

bool expect_true(bool value, const char *label)
{
    if (value)
        return true;

    std::cerr << "failed: " << label << std::endl;
    return false;
}

bool expect_eq_u64(std::uint64_t actual, std::uint64_t expected, const char *label)
{
    if (actual == expected)
        return true;

    std::cerr << "failed: " << label << " actual=" << actual << " expected=" << expected << std::endl;
    return false;
}

bool test_member_find_and_equality()
{
    bool ok = true;
    std::vector<GBE_DotaLobbyMemberState> members{member(10ull, 1u), member(20ull, 2u)};
    std::vector<GBE_DotaLobbyMemberState> same = members;
    std::vector<GBE_DotaLobbyMemberState> different = members;
    different[1].slot = 3u;

    ok &= expect_true(gbe::dota_lobby_flow::lobby_members_equal(members, same), "members equal");
    ok &= expect_true(!gbe::dota_lobby_flow::lobby_members_equal(members, different), "members differ");
    ok &= expect_true(gbe::dota_lobby_flow::lobby_members_contain_steam_id(members, 20ull), "contains steam id");
    ok &= expect_true(!gbe::dota_lobby_flow::lobby_members_contain_steam_id(members, 0ull), "reject zero steam id");

    std::size_t index = 0;
    ok &= expect_true(gbe::dota_lobby_flow::find_lobby_member_index(members, 20ull, index), "find member index");
    ok &= expect_eq_u64(index, 1u, "member index value");
    ok &= expect_true(!gbe::dota_lobby_flow::find_lobby_member_index(members, 30ull, index), "missing member index");

    return ok;
}

bool test_upsert_and_slot_selection()
{
    bool ok = true;
    std::vector<GBE_DotaLobbyMemberState> members{member(10ull, 1u)};

    gbe::dota_lobby_flow::upsert_lobby_member(members, member(0ull, 9u));
    ok &= expect_eq_u64(members.size(), 1u, "ignore zero upsert");

    gbe::dota_lobby_flow::upsert_lobby_member(members, member(20ull, 2u));
    ok &= expect_eq_u64(members.size(), 2u, "insert member");

    GBE_DotaLobbyMemberState updated = member(20ull, 5u);
    gbe::dota_lobby_flow::upsert_lobby_member(members, updated);
    ok &= expect_eq_u64(members.size(), 2u, "update member keeps size");
    ok &= expect_eq_u64(members[1].slot, 5u, "update member slot");

    ok &= expect_eq_u64(gbe::dota_lobby_flow::select_arcade_lobby_member_slot(members, 20ull, 1u), 5u, "reuse existing slot");
    ok &= expect_eq_u64(gbe::dota_lobby_flow::select_arcade_lobby_member_slot(members, 30ull, 1u), 2u, "select first free slot");

    return ok;
}

bool test_compose_lobby_members()
{
    bool ok = true;

    std::vector<GBE_DotaLobbyMemberState> members;
    GBE_DotaLobbyMemberState pool_member = member(20ull, 7u);
    pool_member.team = kPlayerPool;
    pool_member.account_id = 0u;
    members.push_back(pool_member);
    members.push_back(GBE_DotaLobbyMemberState{});

    std::vector<GBE_DotaLobbyMemberState> composed = gbe::dota_lobby_flow::compose_lobby_members(10ull, 100u, kGoodGuys, 1u, 42u, true, kPlayerPool, members);
    ok &= expect_eq_u64(composed.size(), 3u, "compose inserts missing owner");
    ok &= expect_eq_u64(composed[0].steam_id, 10ull, "compose owner steam id");
    ok &= expect_eq_u64(composed[0].account_id, 100u, "compose owner account id");
    ok &= expect_eq_u64(composed[0].hero_id, 42u, "compose owner hero id");
    ok &= expect_eq_u64(composed[1].account_id, 20u, "compose derives member account id");
    ok &= expect_eq_u64(composed[1].slot, 0u, "compose clears player pool slot");
    ok &= expect_eq_u64(composed[2].steam_id, 0ull, "compose preserves zero member");

    std::vector<GBE_DotaLobbyMemberState> owner_members{member(10ull, 9u), member(20ull, 2u), member(20ull, 5u)};
    owner_members[0].team = kPlayerPool;
    owner_members[0].connected = false;
    std::vector<GBE_DotaLobbyMemberState> overridden = gbe::dota_lobby_flow::compose_lobby_members(10ull, 101u, kGoodGuys, 1u, 77u, true, kPlayerPool, owner_members);
    ok &= expect_eq_u64(overridden.size(), 2u, "compose updates duplicate member");
    ok &= expect_eq_u64(overridden[0].account_id, 101u, "compose overrides owner account id");
    ok &= expect_eq_u64(overridden[0].slot, 1u, "compose overrides owner slot");
    ok &= expect_eq_u64(overridden[0].hero_id, 77u, "compose overrides owner hero");
    ok &= expect_true(overridden[0].connected, "compose overrides owner connected");
    ok &= expect_eq_u64(overridden[1].slot, 5u, "compose keeps last duplicate");

    return ok;
}

bool test_normalize_and_owner_transfer()
{
    bool ok = true;
    std::vector<GBE_DotaLobbyMemberState> members{member(10ull, 1u), member(20ull, 2u)};

    GBE_DotaLobbyMemberState pool = member(30ull, 0u);
    pool.team = kPlayerPool;
    ok &= expect_true(gbe::dota_lobby_flow::normalize_arcade_lobby_member_slot(pool, members, 10ull, 1u, kGoodGuys, kPlayerPool), "normalize pool member");
    ok &= expect_eq_u64(pool.team, kGoodGuys, "normalized team");
    ok &= expect_eq_u64(pool.slot, 3u, "normalized slot");

    GBE_DotaLobbyMemberState owner = member(10ull, 1u);
    ok &= expect_true(!gbe::dota_lobby_flow::normalize_arcade_lobby_member_slot(owner, members, 10ull, 1u, kGoodGuys, kPlayerPool), "skip owner normalize");

    std::vector<GBE_DotaLobbyMemberState> previous{member(10ull, 1u), member(20ull, 2u), member(30ull, 3u)};
    std::vector<GBE_DotaLobbyMemberState> current{member(20ull, 1u), member(30ull, 3u), member(40ull, 4u)};
    gbe::dota_lobby_flow::preserve_lobby_owner_transfer_slots(current, previous, 10ull, 20ull);
    ok &= expect_eq_u64(current.size(), 4u, "owner transfer size");
    ok &= expect_eq_u64(current[1].steam_id, 20ull, "new owner old index");
    ok &= expect_eq_u64(current[2].steam_id, 30ull, "preserve existing member index");
    ok &= expect_eq_u64(current[3].steam_id, 40ull, "append new member");

    return ok;
}

bool test_normalize_arcade_lobby_member_slots()
{
    bool ok = true;
    std::uint32_t owner_team = kPlayerPool;
    std::uint32_t owner_slot = 0u;
    std::vector<GBE_DotaLobbyMemberState> members{member(10ull, 8u), member(20ull, 0u), member(30ull, 9u)};
    members[0].team = kPlayerPool;
    members[1].team = kPlayerPool;
    members[2].team = 3u;

    ok &= expect_true(gbe::dota_lobby_flow::normalize_arcade_lobby_member_slots(true, 10ull, owner_team, owner_slot, members, kGoodGuys), "normalize arcade slots changed");
    ok &= expect_eq_u64(owner_team, kGoodGuys, "normalize owner team");
    ok &= expect_eq_u64(owner_slot, 1u, "normalize owner slot");
    ok &= expect_eq_u64(members[0].team, kGoodGuys, "normalize owner member team");
    ok &= expect_eq_u64(members[0].slot, 1u, "normalize owner member slot");
    ok &= expect_eq_u64(members[1].team, kGoodGuys, "normalize first remote team");
    ok &= expect_eq_u64(members[1].slot, 2u, "normalize first remote slot");
    ok &= expect_eq_u64(members[2].slot, 3u, "normalize second remote slot");

    ok &= expect_true(!gbe::dota_lobby_flow::normalize_arcade_lobby_member_slots(true, 10ull, owner_team, owner_slot, members, kGoodGuys), "normalize arcade slots stable");

    std::uint32_t non_custom_owner_team = kPlayerPool;
    std::uint32_t non_custom_owner_slot = 0u;
    std::vector<GBE_DotaLobbyMemberState> non_custom_members{member(10ull, 8u)};
    ok &= expect_true(!gbe::dota_lobby_flow::normalize_arcade_lobby_member_slots(false, 10ull, non_custom_owner_team, non_custom_owner_slot, non_custom_members, kGoodGuys), "skip non custom lobby");
    ok &= expect_eq_u64(non_custom_owner_team, kPlayerPool, "non custom owner team unchanged");
    ok &= expect_eq_u64(non_custom_owner_slot, 0u, "non custom owner slot unchanged");

    return ok;
}

} // namespace

int main()
{
    bool ok = true;
    ok &= test_member_find_and_equality();
    ok &= test_upsert_and_slot_selection();
    ok &= test_compose_lobby_members();
    ok &= test_normalize_and_owner_transfer();
    ok &= test_normalize_arcade_lobby_member_slots();

    if (!ok)
        return 1;

    std::cout << "gbe_dota_lobby_flow_test passed" << std::endl;
    return 0;
}
