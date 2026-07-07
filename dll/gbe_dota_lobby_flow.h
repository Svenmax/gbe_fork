#ifndef GBE_DOTA_LOBBY_FLOW_H
#define GBE_DOTA_LOBBY_FLOW_H

#include "gbe_dota_chat_flow.h"
#include "gbe_dota_lobby_publish.h"
#include "gbe_dota_lobby_launch_flow.h"
#include "gbe_dota_lobby_member_flow.h"
#include "gbe_dota_lobby_payload_flow.h"
#include "gbe_dota_lobby_snapshot.h"
#include "gbe_dota_lobby_state.h"
#include "gbe_dota_types.h"
#include "gbe_dota_action_model.h"
#include "gbe_proto_wire.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace gbe::dota_lobby_flow {

struct CreateLobbyActionPlan
{
    bool reset_gc_memory{};
    std::string reset_reason;
    bool reset_leave_generic_lobby{};
    bool reset_clear_queued_messages{};
    bool unsubscribe_previous_practice_lobby{};
};

struct CreateLobbyContext
{
    GBE_LocalLobby previous_lobby{};
    GBE_DotaCustomGameDetails pre_reset_custom_game{};
    gbe::proto_wire::DotaPracticeLobbyCreateRequest request{};
    bool parsed_request{};
    std::uint64_t new_lobby_id{};
    std::uint64_t owner_steam_id{};
    std::uint32_t owner_account_id{};
    std::string owner_name;
    std::uint32_t owner_team{};
    std::uint32_t owner_slot{};
};

struct JoinLobbyActionPlan
{
    bool matched_generic_lobby{};
    bool send_join_response{};
    std::uint64_t generic_lobby_id{};
};

struct JoinLobbyContext
{
    GBE_LocalLobby current_lobby{};
    bool request_has_lobby_id{};
    std::uint64_t request_lobby_id{};
    bool request_has_pass_key{};
    std::string request_pass_key;
    bool matched_generic_lobby{};
    GBE_LocalLobby matched_lobby{};
    std::uint64_t matched_generic_lobby_id{};
    bool send_join_response{};
    std::uint64_t local_steam_id{};
    std::uint32_t local_account_id{};
    std::string local_name;
    std::uint32_t good_guys_team{};
    std::uint32_t player_pool_team{};
};

GBE_DotaActionList create_lobby_action_list(
    const CreateLobbyActionPlan &plan,
    bool wrapped);

gbe::dota_lobby_state::CreateLobbyResetPlan create_lobby_reset_plan_from_context(
    const CreateLobbyContext &context);

gbe::dota_lobby_state::CreateLobbyPlan create_lobby_state_plan_from_context(
    const CreateLobbyContext &context);

CreateLobbyActionPlan create_lobby_action_plan_from_reset_plan(
    const gbe::dota_lobby_state::CreateLobbyResetPlan &reset_plan);

GBE_DotaActionList join_lobby_action_list(
    const JoinLobbyActionPlan &plan,
    bool wrapped);

gbe::dota_lobby_state::JoinLobbyMergePlan join_lobby_merge_plan_from_context(
    const JoinLobbyContext &context);

JoinLobbyActionPlan join_lobby_action_plan_from_context(
    const JoinLobbyContext &context);

GBE_DotaActionList abandon_cache_unsubscribed_action_list(
    const gbe::dota_lobby_state::AbandonDecision &decision,
    const std::string &response_25,
    const char *reason);

GBE_DotaActionList normal_signout_cache_unsubscribed_action_list(
    std::uint64_t lobby_id,
    const std::string &response_25,
    const char *reason);

GBE_DotaActionList leave_lobby_cache_unsubscribed_action_list(
    std::uint64_t lobby_id,
    const std::string &response_25,
    const char *reason);

GBE_DotaActionList postgame_teardown_action_list(
    std::uint64_t lobby_id,
    const std::string &response_25,
    const std::string &response_7010_postgame,
    bool push_cache_unsubscribed,
    bool push_postgame_join,
    const char *reason);

GBE_DotaActionList player_postgame_cleanup_action_list(
    std::uint64_t lobby_id,
    const std::string &response_25,
    bool push_cache_unsubscribed,
    const char *reason);

GBE_DotaActionList normal_signout_finalize_action_list(
    std::uint64_t lobby_id,
    const std::string &client_response_25,
    bool push_client_cache_unsubscribed,
    const char *reason);

GBE_DotaActionList abandon_finalize_action_list(
    const char *reason);

void apply_lobby_member_team_slot_update(
    std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t steam_id,
    std::uint32_t account_id,
    bool has_team,
    std::uint32_t team,
    bool has_slot,
    std::uint32_t slot,
    std::uint32_t default_team,
    bool connected);

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
    std::uint32_t player_pool_team);

bool set_lobby_member_hero(
    std::vector<GBE_DotaLobbyMemberState> &members,
    std::uint64_t steam_id,
    std::uint32_t hero_id);

void preserve_generic_snapshot_member_runtime(
    GBE_DotaLobbyMemberState &member,
    const std::vector<GBE_DotaLobbyMemberState> &existing_members,
    bool preserve_launched_lan_members,
    bool preserve_custom_game_runtime_members);

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
    std::uint32_t player_pool_team);

void merge_existing_lobby_members_for_generic_snapshot(
    std::vector<GBE_DotaLobbyMemberState> &members,
    const std::vector<GBE_DotaLobbyMemberState> &existing_members,
    bool generic_members_empty,
    std::uint64_t local_steam_id,
    std::uint64_t owner_steam_id,
    bool preserve_launched_lan_members,
    bool preserve_custom_game_runtime_members);

void upsert_owner_member_for_generic_snapshot(
    std::vector<GBE_DotaLobbyMemberState> &members,
    bool owner_in_generic_members,
    bool generic_members_empty,
    std::uint64_t owner_steam_id,
    std::uint32_t owner_account_id,
    std::uint32_t owner_team,
    std::uint32_t owner_slot,
    std::uint32_t owner_hero_id,
    bool owner_connected);

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
    bool &owner_connected);

void preserve_lobby_owner_transfer_slots(
    std::vector<GBE_DotaLobbyMemberState> &members,
    const std::vector<GBE_DotaLobbyMemberState> &previous_members,
    std::uint64_t previous_owner_steam_id,
    std::uint64_t new_owner_steam_id);

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
