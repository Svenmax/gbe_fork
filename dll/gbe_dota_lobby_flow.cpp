#include "gbe_dota_lobby_flow.h"

#include "gbe_dota_protocol_constants.h"

#include <algorithm>
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

gbe::dota_lobby_state::CreateLobbyResetPlan create_lobby_reset_plan_from_context(
    const CreateLobbyContext &context)
{
    return gbe::dota_lobby_state::compose_create_lobby_reset_plan(
        context.previous_lobby,
        context.pre_reset_custom_game);
}

gbe::dota_lobby_state::CreateLobbyPlan create_lobby_state_plan_from_context(
    const CreateLobbyContext &context)
{
    return gbe::dota_lobby_state::compose_create_lobby_plan(
        context.request,
        context.new_lobby_id,
        context.owner_steam_id,
        context.owner_account_id,
        context.owner_name,
        context.owner_team,
        context.owner_slot);
}

CreateLobbyActionPlan create_lobby_action_plan_from_reset_plan(
    const gbe::dota_lobby_state::CreateLobbyResetPlan &reset_plan)
{
    CreateLobbyActionPlan plan{};
    plan.reset_gc_memory = reset_plan.reset_gc_memory;
    plan.reset_reason = reset_plan.reset_reason;
    plan.reset_leave_generic_lobby = reset_plan.reset_leave_generic_lobby;
    plan.reset_clear_queued_messages = reset_plan.reset_clear_queued_messages;
    plan.unsubscribe_previous_practice_lobby = reset_plan.unsubscribe_previous_practice_lobby;
    return plan;
}

gbe::dota_lobby_state::JoinLobbyMergePlan join_lobby_merge_plan_from_context(
    const JoinLobbyContext &context)
{
    const bool join_has_lobby_id = context.request_has_lobby_id || context.matched_lobby.lobby_id != 0ull;
    const std::uint64_t join_lobby_id = context.request_has_lobby_id && context.request_lobby_id != 0ull ?
        context.request_lobby_id :
        context.matched_lobby.lobby_id;
    return gbe::dota_lobby_state::compose_join_lobby_merge_plan(
        context.current_lobby,
        join_has_lobby_id,
        join_lobby_id,
        context.request_has_pass_key,
        context.request_pass_key,
        context.matched_generic_lobby,
        context.matched_lobby,
        context.local_steam_id,
        context.local_account_id,
        context.local_name,
        context.good_guys_team,
        context.player_pool_team);
}

JoinLobbyActionPlan join_lobby_action_plan_from_context(
    const JoinLobbyContext &context)
{
    return JoinLobbyActionPlan{
        context.matched_generic_lobby,
        context.send_join_response,
        context.matched_generic_lobby_id};
}

GBE_DotaActionList create_lobby_action_list(
    const CreateLobbyActionPlan &plan,
    bool wrapped)
{
    GBE_DotaActionList actions;
    if (plan.reset_gc_memory)
        actions.push_back(GBE_DotaAction{ GBE_DotaActionType::GcMemoryReset, 0u, std::string(), 0ull, 0ull, 0ull, plan.reset_reason, plan.reset_leave_generic_lobby, plan.reset_clear_queued_messages });
    if (plan.unsubscribe_previous_practice_lobby)
        actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PushIncomingNow, GBE_kDotaCacheUnsubscribed | GBE_kProtoMask });

    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::GenericLobbyCreate, 0u, std::string(), 0ull, 0ull, 0ull, "7038_create" });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::LobbyLocalMemberData, 0u, std::string(), 0ull, 0ull, 0ull, "7038_create" });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::SettingsLobbySync, 0u, std::string(), 0ull, 0ull, 0ull, "7038_create" });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::LobbySnapshotRefresh, 0u, std::string(), 0ull, 0ull, 0ull, "7038_create" });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::LobbyMetadataPublish, 0u, std::string(), 0ull, 0ull, 0ull, "7038_create" });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::LobbyCacheSubscriptionRecord, 0u, std::string(), 0ull, 0ull, 0ull, wrapped ? "7038_create_wrapped" : "7038_create_direct" });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PushIncomingNow, GBE_kDotaCacheSubscribed | GBE_kProtoMask, std::string(), 0ull, 0ull, 0ull, "7038_24" });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PushIncomingNow, GBE_kDotaPracticeLobbyResponse | GBE_kProtoMask, std::string(), 0ull, 0ull, 0ull, "7038_7055" });
    return actions;
}

GBE_DotaActionList join_lobby_action_list(
    const JoinLobbyActionPlan &plan,
    bool wrapped)
{
    GBE_DotaActionList actions;
    if (plan.matched_generic_lobby) {
        actions.push_back(GBE_DotaAction{ GBE_DotaActionType::GenericLobbyJoin, 0u, std::string(), 0ull, plan.generic_lobby_id, 0ull, "7044_join_generic" });
        actions.push_back(GBE_DotaAction{ GBE_DotaActionType::SettingsLobbySync, 0u, std::string(), 0ull, 0ull, 0ull, "7044_join_generic" });
    }

    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::LobbyLocalMemberData, 0u, std::string(), 0ull, 0ull, 0ull, "7044_join" });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::LobbySnapshotRefresh, 0u, std::string(), 0ull, 0ull, 0ull, "7044_join" });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::LobbyCacheSubscriptionRecord, 0u, std::string(), 0ull, 0ull, 0ull, wrapped ? "7044_join_wrapped" : "7044_join_direct" });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PushIncomingNow, GBE_kDotaCacheSubscribed | GBE_kProtoMask, std::string(), 0ull, 0ull, 0ull, "7044_join_24" });
    if (plan.send_join_response)
        actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PushIncomingNow, GBE_kDotaPracticeLobbyJoinResponse | GBE_kProtoMask, std::string(), 0ull, 0ull, 0ull, "7044_join_7113" });
    return actions;
}

GBE_DotaActionList abandon_cache_unsubscribed_action_list(
    const gbe::dota_lobby_state::AbandonDecision &decision,
    const std::string &response_25,
    const char *reason)
{
    GBE_DotaActionList actions;
    const std::string action_reason = reason ? reason : std::string();
    if (decision.discard_queued_launch_messages)
        actions.push_back(GBE_DotaAction{ GBE_DotaActionType::LaunchMessagesDiscardedForAbandon, 0u, std::string(), 0ull, 0ull, 0ull, action_reason });
    if (decision.set_pending_reset_after_cache_unsubscribed)
        actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PendingResetAfterCacheUnsubscribed, 0u, std::string(), 0ull, decision.lobby_id, 0ull, action_reason });
    if (decision.suppress_abandoned_lobby)
        actions.push_back(GBE_DotaAction{ GBE_DotaActionType::AbandonedLobbySuppressed, 0u, std::string(), 0ull, decision.lobby_id, 0ull, action_reason });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PushIncomingNow, GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25, 0ull, 0ull, 0ull, action_reason });
    return actions;
}

GBE_DotaActionList normal_signout_cache_unsubscribed_action_list(
    std::uint64_t lobby_id,
    const std::string &response_25,
    const char *reason)
{
    GBE_DotaActionList actions;
    const std::string action_reason = reason ? reason : std::string();
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PushIncomingNow, GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25, 0ull, 0ull, 0ull, action_reason });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PendingNormalSignoutFinalizeAfterCacheUnsubscribed, 0u, std::string(), 0ull, lobby_id, 0ull, action_reason });
    return actions;
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

void apply_lobby_member_team_slot_update(
    std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t steam_id,
    std::uint32_t account_id,
    bool has_team,
    std::uint32_t team,
    bool has_slot,
    std::uint32_t slot,
    std::uint32_t default_team,
    bool connected)
{
    if (steam_id == 0ull)
        return;

    for (GBE_DotaLobbyMemberState &member : members) {
        if (member.steam_id != steam_id)
            continue;
        if (has_team)
            member.team = team;
        if (has_slot)
            member.slot = slot;
        return;
    }

    GBE_DotaLobbyMemberState member{};
    member.steam_id = steam_id;
    member.account_id = account_id;
    member.team = has_team ? team : default_team;
    member.slot = has_slot ? slot : 0u;
    member.connected = connected;
    upsert_lobby_member(members, member);
}

bool set_lobby_member_connected(
    std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t steam_id,
    std::uint32_t account_id,
    bool connected,
    bool has_custom_game,
    bool should_mark_leaver,
    std::uint64_t owner_steam_id,
    std::uint32_t owner_slot,
    std::uint32_t good_guys_team,
    std::uint32_t player_pool_team)
{
    if (steam_id == 0ull)
        return false;

    bool changed = false;
    for (GBE_DotaLobbyMemberState &member : members) {
        if (member.steam_id != steam_id)
            continue;
        if (member.connected != connected) {
            member.connected = connected;
            changed = true;
        }
        if (connected && has_custom_game)
            changed = normalize_arcade_lobby_member_slot(member, members, owner_steam_id, owner_slot, good_guys_team, player_pool_team) || changed;
        if (!connected && has_custom_game && should_mark_leaver && member.leaver_status == 0u) {
            member.leaver_status = 1u;
            changed = true;
        }
        if (connected && member.leaver_status != 0u) {
            member.leaver_status = 0u;
            changed = true;
        }
        return changed;
    }

    if (connected && steam_id != owner_steam_id) {
        GBE_DotaLobbyMemberState member{};
        member.steam_id = steam_id;
        member.account_id = account_id;
        member.team = player_pool_team;
        if (has_custom_game)
            normalize_arcade_lobby_member_slot(member, members, owner_steam_id, owner_slot, good_guys_team, player_pool_team);
        member.connected = true;
        upsert_lobby_member(members, member);
        return true;
    }

    return changed;
}

bool set_lobby_member_hero(
    std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t steam_id,
    std::uint32_t hero_id)
{
    if (steam_id == 0ull || hero_id == 0u)
        return false;

    for (GBE_DotaLobbyMemberState &member : members) {
        if (member.steam_id != steam_id)
            continue;
        if (member.hero_id == hero_id)
            return false;
        member.hero_id = hero_id;
        return true;
    }

    return false;
}

void preserve_generic_snapshot_member_runtime(
    GBE_DotaLobbyMemberState &member,
    const std::vector<GBE_DotaLobbyMemberState> &existing_members,
    bool preserve_launched_lan_members,
    bool preserve_custom_game_runtime_members)
{
    if (preserve_launched_lan_members && !preserve_custom_game_runtime_members && !member.connected) {
        for (const GBE_DotaLobbyMemberState &existing : existing_members) {
            if (existing.steam_id == member.steam_id && existing.connected) {
                member.connected = true;
                break;
            }
        }
    }
    if (preserve_custom_game_runtime_members && !member.connected)
        member.leaver_status = 1u;
}

void update_generic_lobby_member_snapshot_state(
    GBE_DotaLobbyMemberState &member,
    const std::vector<GBE_DotaLobbyMemberState> &existing_members,
    bool has_custom_game,
    std::uint64_t owner_steam_id,
    std::uint32_t owner_slot,
    std::vector<GBE_DotaLobbyMemberState> &members,
    bool preserve_launched_lan_members,
    bool preserve_custom_game_runtime_members,
    std::uint32_t good_guys_team,
    std::uint32_t player_pool_team)
{
    preserve_generic_snapshot_member_runtime(
        member,
        existing_members,
        preserve_launched_lan_members,
        preserve_custom_game_runtime_members);

    if (member.steam_id == owner_steam_id)
        return;

    if (has_custom_game)
        normalize_arcade_lobby_member_slot(member, members, owner_steam_id, owner_slot, good_guys_team, player_pool_team);
}

void merge_existing_lobby_members_for_generic_snapshot(
    std::vector<GBE_DotaLobbyMemberState> &members,
    const std::vector<GBE_DotaLobbyMemberState> &existing_members,
    bool generic_members_empty,
    std::uint64_t local_steam_id,
    std::uint64_t owner_steam_id,
    bool preserve_launched_lan_members,
    bool preserve_custom_game_runtime_members)
{
    for (const GBE_DotaLobbyMemberState &existing : existing_members) {
        const bool missing_from_generic =
            existing.steam_id != 0ull &&
            !lobby_members_contain_steam_id(members, existing.steam_id);
        if (generic_members_empty || existing.steam_id == local_steam_id || (preserve_launched_lan_members && missing_from_generic)) {
            GBE_DotaLobbyMemberState preserved = existing;
            if (preserve_launched_lan_members && missing_from_generic && preserved.steam_id != owner_steam_id) {
                preserved.connected = false;
                if (preserve_custom_game_runtime_members && preserved.leaver_status == 0u)
                    preserved.leaver_status = 1u;
            }
            upsert_lobby_member(members, preserved);
        }
    }
}

void upsert_owner_member_for_generic_snapshot(
    std::vector<GBE_DotaLobbyMemberState> &members,
    bool owner_in_generic_members,
    bool generic_members_empty,
    std::uint64_t owner_steam_id,
    std::uint32_t owner_account_id,
    std::uint32_t owner_team,
    std::uint32_t owner_slot,
    std::uint32_t owner_hero_id,
    bool owner_connected)
{
    if (!owner_in_generic_members && !generic_members_empty)
        return;

    GBE_DotaLobbyMemberState owner{};
    owner.steam_id = owner_steam_id;
    owner.account_id = owner_account_id;
    owner.team = owner_team;
    owner.slot = owner_slot;
    owner.hero_id = owner_hero_id;
    owner.connected = owner_connected;
    upsert_lobby_member(members, owner);
}

GBE_DotaLobbyMemberState adopt_lobby_owner_member(
    std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t new_owner_steam_id,
    std::uint32_t default_owner_account_id,
    std::uint32_t default_owner_team,
    bool default_owner_connected,
    std::uint64_t &owner_steam_id,
    std::uint32_t &owner_account_id,
    std::uint32_t &owner_team,
    std::uint32_t &owner_slot,
    std::uint32_t &owner_hero_id,
    bool &owner_connected)
{
    GBE_DotaLobbyMemberState new_owner{};
    new_owner.steam_id = new_owner_steam_id;
    new_owner.account_id = default_owner_account_id;
    new_owner.team = default_owner_team;
    new_owner.slot = 0u;
    new_owner.connected = default_owner_connected;

    for (const GBE_DotaLobbyMemberState &member : members) {
        if (member.steam_id != new_owner_steam_id)
            continue;
        new_owner = member;
        break;
    }

    owner_steam_id = new_owner_steam_id;
    owner_account_id = new_owner.account_id;
    owner_team = new_owner.team;
    owner_slot = new_owner.slot;
    owner_hero_id = new_owner.hero_id;
    owner_connected = new_owner.connected;
    upsert_lobby_member(members, new_owner);
    return new_owner;
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
