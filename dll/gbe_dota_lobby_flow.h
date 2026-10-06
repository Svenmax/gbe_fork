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

GBE_DotaActionList launch_init_action_list();

GBE_DotaActionList destroy_lobby_reset_action_list();

GBE_DotaActionList abandon_disconnect_reset_action_list();

GBE_DotaActionList signout_postgame_action_list(
    std::uint32_t current_state,
    std::uint32_t current_game_state);

gbe::dota_lobby_state::JoinLobbyMergePlan join_lobby_merge_plan_from_context(
    const JoinLobbyContext &context);

JoinLobbyActionPlan join_lobby_action_plan_from_context(
    const JoinLobbyContext &context);

GBE_DotaActionList abandon_cache_unsubscribed_action_list(
    const gbe::dota_lobby_state::AbandonDecision &decision,
    const std::string &response_25,
    const char *reason);

// C6: discard/suppress preflight before QueuePostGame (7035 ready path).
GBE_DotaActionList abandon_initiate_preflight_action_list(
    const gbe::dota_lobby_state::AbandonDecision &decision,
    const char *reason);

GBE_DotaActionList normal_signout_cache_unsubscribed_action_list(
    std::uint64_t lobby_id,
    const std::string &response_25,
    const char *reason);

GBE_DotaActionList normal_signout_postgame_followup_action_list(
    std::uint64_t lobby_id,
    const std::string &response_25);

GBE_DotaActionList leave_lobby_cache_unsubscribed_action_list(
    std::uint64_t lobby_id,
    const std::string &response_25,
    const char *reason);

// C6: list-path leave finalize (25 via CacheUnsubscribedResponse + Leave reset).
GBE_DotaActionList leave_lobby_finalize_action_list(
    const std::string &response_25,
    const char *reason);

// C9: full QueuePostGame mutation + push list (state/chat/publish before 25/7010).
GBE_DotaActionList postgame_teardown_action_list(
    std::uint64_t lobby_id,
    std::uint64_t pre_postgame_chat_channel_id,
    std::uint64_t postgame_chat_channel_id,
    const std::string &postgame_chat_channel_name,
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

} // namespace gbe::dota_lobby_flow

#endif // GBE_DOTA_LOBBY_FLOW_H
