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

GBE_DotaActionList launch_init_action_list()
{
    GBE_DotaActionList actions;
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::LaunchPeripheralReset, 0u, std::string(), 0ull, 0ull, 0ull, "7041_launch_init" });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::SharedLobbyPublish, 0u, std::string(), 0ull, 0ull, 0ull, "7041_launch_init" });
    return actions;
}

GBE_DotaActionList destroy_lobby_reset_action_list()
{
    GBE_DotaAction reset{ GBE_DotaActionType::GcMemoryReset, 0u, std::string(), 0ull, 0ull, 0ull, "8246_destroy", true, true };
    reset.generation_boundary = gbe::dota_lobby_generation::Boundary::Leave;
    return { std::move(reset) };
}

GBE_DotaActionList abandon_disconnect_reset_action_list()
{
    return { GBE_DotaAction{ GBE_DotaActionType::GcMemoryReset, 0u, std::string(), 0ull, 0ull, 0ull, "7035_disconnect_current_game_after_25", true, true } };
}

GBE_DotaActionList signout_postgame_action_list(
    std::uint32_t current_state,
    std::uint32_t current_game_state)
{
    GBE_DotaActionList actions;
    GBE_DotaAction apply{ GBE_DotaActionType::LobbyStateApply };
    apply.lobby_state = std::max(current_state, 2u);
    apply.lobby_game_state = std::max(current_game_state, 6u);
    apply.reason = "7004_signout_post_game";
    actions.push_back(std::move(apply));
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::SharedLobbyPublish, 0u, std::string(), 0ull, 0ull, 0ull, "7004_signout_post_game" });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PracticeLobbyDetailsUpdate, 0u, std::string(), 0ull, 0ull, 0ull, "7004_signout_run_post_game" });
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

GBE_DotaActionList abandon_initiate_preflight_action_list(
    const gbe::dota_lobby_state::AbandonDecision &decision,
    const char *reason)
{
    GBE_DotaActionList actions;
    const std::string action_reason = reason ? reason : std::string();
    if (decision.discard_queued_launch_messages)
        actions.push_back(GBE_DotaAction{ GBE_DotaActionType::LaunchMessagesDiscardedForAbandon, 0u, std::string(), 0ull, 0ull, 0ull, action_reason });
    if (decision.suppress_abandoned_lobby)
        actions.push_back(GBE_DotaAction{ GBE_DotaActionType::AbandonedLobbySuppressed, 0u, std::string(), 0ull, decision.lobby_id, 0ull, action_reason });
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

GBE_DotaActionList normal_signout_postgame_followup_action_list(
    std::uint64_t lobby_id,
    const std::string &response_25)
{
    GBE_DotaActionList actions;
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PracticeLobbyDetailsUpdate, 0u, std::string(), 0ull, 0ull, 0ull, "7004_signout_postgame_state" });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PushIncomingNow, GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25, 0ull, 0ull, 0ull, "25_after_7004" });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PendingNormalSignoutFinalizeAfterCacheUnsubscribed, 0u, std::string(), 0ull, lobby_id, 0ull, "25_after_7004" });
    return actions;
}

GBE_DotaActionList leave_lobby_cache_unsubscribed_action_list(
    std::uint64_t lobby_id,
    const std::string &response_25,
    const char *reason)
{
    GBE_DotaActionList actions;
    const std::string action_reason = reason ? reason : std::string();
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::AbandonedLobbySuppressed, 0u, std::string(), 0ull, lobby_id, 0ull, action_reason });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PushIncomingNow, GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25, 0ull, 0ull, 0ull, action_reason });
    GBE_DotaAction reset_action{ GBE_DotaActionType::GcMemoryReset, 0u, std::string(), 0ull, 0ull, 0ull, action_reason, true, false };
    reset_action.generation_boundary = gbe::dota_lobby_generation::Boundary::Leave;
    actions.push_back(std::move(reset_action));
    return actions;
}

GBE_DotaActionList leave_lobby_finalize_action_list(
    const std::string &response_25,
    const char *reason)
{
    GBE_DotaActionList actions;
    const std::string action_reason = reason ? reason : std::string();
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PushIncomingNow, GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25, 0ull, 0ull, 0ull, action_reason });
    GBE_DotaAction reset_action{ GBE_DotaActionType::GcMemoryReset, 0u, std::string(), 0ull, 0ull, 0ull, action_reason, true, false };
    reset_action.generation_boundary = gbe::dota_lobby_generation::Boundary::Leave;
    actions.push_back(std::move(reset_action));
    return actions;
}

GBE_DotaActionList postgame_teardown_action_list(
    std::uint64_t lobby_id,
    std::uint64_t pre_postgame_chat_channel_id,
    std::uint64_t postgame_chat_channel_id,
    const std::string &postgame_chat_channel_name,
    const std::string &response_25,
    const std::string &response_7010_postgame,
    bool push_cache_unsubscribed,
    bool push_postgame_join,
    const char *reason)
{
    GBE_DotaActionList actions;
    const std::string action_reason = reason ? reason : std::string();

    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::LaunchPeripheralReset, 0u, std::string(), 0ull, 0ull, 0ull, action_reason });

    GBE_DotaAction postgame_state;
    postgame_state.type = GBE_DotaActionType::PostGameLobbyStateApply;
    postgame_state.lobby_state = 3u;
    postgame_state.lobby_game_state = 6u;
    postgame_state.item_id = pre_postgame_chat_channel_id;
    postgame_state.job_id = postgame_chat_channel_id;
    postgame_state.payload = postgame_chat_channel_name;
    postgame_state.reason = action_reason;
    actions.push_back(std::move(postgame_state));

    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::SharedLobbyPublish, 0u, std::string(), 0ull, 0ull, 0ull, action_reason });

    if (push_cache_unsubscribed)
        actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PushIncomingNow, GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25, 0ull, 0ull, 0ull, "25" });
    if (push_postgame_join)
        actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PushIncomingNow, GBE_kDotaJoinChatChannelResponse | GBE_kProtoMask, response_7010_postgame, 0ull, 0ull, 0ull, "7010_postgame" });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PendingResetAfterCacheUnsubscribedClear, 0u, std::string(), 0ull, lobby_id, 0ull, action_reason });

    GBE_DotaAction rich_presence;
    rich_presence.type = GBE_DotaActionType::RichPresenceUpdate;
    rich_presence.status = "#DOTA_RP_PRIVATE_LOBBY";
    rich_presence.presence_lobby_state = "RUN";
    rich_presence.include_party = true;
    rich_presence.include_lobby = false;
    rich_presence.reason = action_reason;
    actions.push_back(std::move(rich_presence));
    return actions;
}

GBE_DotaActionList player_postgame_cleanup_action_list(
    std::uint64_t lobby_id,
    const std::string &response_25,
    bool push_cache_unsubscribed,
    const char *reason)
{
    GBE_DotaActionList actions;
    const std::string action_reason = reason ? reason : std::string();
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::RichPresenceClear, 0u, std::string(), 0ull, 0ull, 0ull, "clear_launch_rich_presence" });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::LaunchPeripheralReset, 0u, std::string(), 0ull, 0ull, 0ull, "launch_peripheral_reset" });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::DotaLobbyRuntimeClear, 0u, std::string(), 0ull, 0ull, 0ull, action_reason });
    if (push_cache_unsubscribed)
        actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PushIncomingNow, GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25, 0ull, lobby_id, 0ull, action_reason });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::SettingsLobbyClear, 0u, std::string(), 0ull, lobby_id, 0ull, action_reason });
    return actions;
}

GBE_DotaActionList normal_signout_finalize_action_list(
    std::uint64_t lobby_id,
    const std::string &client_response_25,
    bool push_client_cache_unsubscribed,
    const char *reason)
{
    GBE_DotaActionList actions;
    const std::string action_reason = reason ? reason : std::string();
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::SettingsLobbyClear, 0u, std::string(), 0ull, lobby_id, 0ull, action_reason });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::LaunchPeripheralReset, 0u, std::string(), 0ull, 0ull, 0ull, action_reason });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::DotaLobbyRuntimeClear, 0u, std::string(), 0ull, 0ull, 0ull, action_reason });
    actions.push_back(GBE_DotaAction{ GBE_DotaActionType::RichPresenceClear, 0u, std::string(), 0ull, 0ull, 0ull, "clear_launch_rich_presence" });
    if (push_client_cache_unsubscribed)
        actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PushIncomingNow, GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, client_response_25, 0ull, lobby_id, 0ull, "25" });
    return actions;
}

GBE_DotaActionList abandon_finalize_action_list(
    const char *reason)
{
    GBE_DotaActionList actions;
    const std::string action_reason = reason ? reason : "7014_abandon_finalize";
    GBE_DotaAction reset_action{ GBE_DotaActionType::GcMemoryReset, 0u, std::string(), 0ull, 0ull, 0ull, action_reason, true, false };
    reset_action.generation_boundary = gbe::dota_lobby_generation::Boundary::Leave;
    actions.push_back(std::move(reset_action));
    return actions;
}

} // namespace gbe::dota_lobby_flow
