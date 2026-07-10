/* Shared custom-game lifecycle side-effect executor. */

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_custom_game_lifecycle.h"
#include "gbe_dota_protocol_constants.h"

gbe::dota_lifecycle::ExecutionResult Steam_Game_Coordinator::GBE_ExecuteDotaLifecycleActions(
    const GBE_DotaActionList &actions,
    const gbe::dota_lifecycle::ExecutionOptions &options)
{
    gbe::dota_lifecycle::ExecutionResult result;
    bool previous_action_succeeded = true;
    bool abort_execution = false;
    for (const GBE_DotaAction &action : actions) {
        if (action.only_when_previous_action_succeeded && !previous_action_succeeded)
            continue;
        if (action.only_when_runtime_update_not_queued && result.runtime_update_queued)
            continue;

        previous_action_succeeded = true;
        switch (action.type) {
            case GBE_DotaActionType::LobbyStateApply:
                GBE_local_lobby.state = action.lobby_state;
                GBE_local_lobby.game_state = action.lobby_game_state;
                break;
            case GBE_DotaActionType::LobbyMemberRuntimeUpdate:
                previous_action_succeeded = GBE_SetDotaLobbyMemberRuntimeState(
                    action.target_steam_id,
                    action.connected,
                    action.hero_id,
                    action.has_hero_id);
                result.state_changed = previous_action_succeeded || result.state_changed;
                break;
            case GBE_DotaActionType::LaunchPhaseMark:
                GBE_MarkDotaLaunchPhase(action.launch_phase, action.reason.c_str(), false);
                break;
            case GBE_DotaActionType::LobbyLocalMemberData:
                GBE_PublishDotaPracticeLobbyLocalMemberData(action.reason.c_str());
                break;
            case GBE_DotaActionType::RuntimeLobbyDetailsUpdate:
                result.runtime_update_queued = GBE_TryQueueDotaRuntimeLobbyDetailsUpdate(
                    action.reason.c_str(),
                    action.emsg,
                    action.job_id,
                    action.lobby_state,
                    action.lobby_game_state,
                    action.delay);
                previous_action_succeeded = result.runtime_update_queued;
                break;
            case GBE_DotaActionType::SharedLobbyPublish:
                GBE_PublishSharedDotaLobbyState(action.reason.c_str());
                break;
            case GBE_DotaActionType::PracticeLobbyDetailsUpdate:
                result.details_update_sent = GBE_SendDotaPracticeLobbyDetailsUpdate(
                    options.wrapped,
                    options.outer_session_field_raw,
                    action.reason.c_str());
                previous_action_succeeded = result.details_update_sent;
                break;
            case GBE_DotaActionType::LaunchMessagesDiscardedForAbandon:
                GBE_DiscardQueuedDotaLaunchMessagesForAbandon(action.reason.c_str());
                break;
            case GBE_DotaActionType::PendingResetAfterCacheUnsubscribed:
                GBE_SetPendingResetAfterCacheUnsubscribed(action.item_id);
                break;
            case GBE_DotaActionType::PendingResetAfterCacheUnsubscribedClear:
                GBE_ClearPendingResetAfterCacheUnsubscribed(action.item_id);
                break;
            case GBE_DotaActionType::PendingNormalSignoutFinalizeAfterCacheUnsubscribed:
                GBE_SetPendingDotaNormalSignoutFinalizeAfterCacheUnsubscribed(action.item_id);
                break;
            case GBE_DotaActionType::AbandonedLobbySuppressed:
                GBE_MarkDotaAbandonedLobbySuppressed(action.item_id, action.reason.c_str());
                break;
            case GBE_DotaActionType::GcMemoryReset:
                previous_action_succeeded = ResetGCMemory(
                    action.reason.c_str(),
                    action.leave_generic_lobby,
                    action.clear_queued_messages,
                    action.generation_boundary);
                break;
            case GBE_DotaActionType::SettingsLobbyClear:
                GBE_ClearSettingsLobbyForDotaSignout();
                break;
            case GBE_DotaActionType::LaunchPeripheralReset:
                if (options.mirror_launch_peripheral_to_client_target &&
                    options.client_target &&
                    options.client_target != this &&
                    !options.client_target->is_server) {
                    options.client_target->GBE_ResetDotaPracticeLobbyLaunchPeripheralState();
                    if (options.client_lobby_restore && options.client_lobby_restore->active && options.client_lobby_restore->lobby_id != 0)
                        options.client_target->GBE_local_lobby = *options.client_lobby_restore;
                    options.client_target->GBE_ClearLastDotaLaunchStatePushedGameState();
                }
                GBE_ResetDotaPracticeLobbyLaunchPeripheralState();
                break;
            case GBE_DotaActionType::DotaLobbyRuntimeClear:
                if (GBE_AdvanceDotaLobbyGeneration(action.generation_boundary, action.reason.c_str()) == GBE_DotaGenerationAdvanceResult::Exhausted) {
                    previous_action_succeeded = false;
                    break;
                }
                {
                    const uint64 next_generation = GBE_CurrentDotaLobbyGeneration();
                    GBE_ClearDotaLobbyRuntimeState();
                    GBE_local_lobby.generation = next_generation;
                }
                break;
            case GBE_DotaActionType::RichPresenceClear: {
                Steam_Game_Coordinator *target = options.route_rich_presence_to_client_target
                    ? options.client_target
                    : this;
                if (target && target->gc_profile == GC_PROFILE_DOTA2)
                    target->GBE_ClearDotaPracticeLobbyLaunchRichPresence();
                break;
            }
            case GBE_DotaActionType::PushIncomingNow: {
                Steam_Game_Coordinator *target = options.route_push_to_client_target
                    ? options.client_target
                    : this;
                if (!target || target->gc_profile != GC_PROFILE_DOTA2) {
                    previous_action_succeeded = false;
                    abort_execution = true;
                    break;
                }
                const char *push_reason = options.push_reason_override
                    ? options.push_reason_override
                    : action.reason.c_str();
                switch (options.push_route) {
                    case gbe::dota_lifecycle::PushRoute::Immediate:
                        target->push_incoming_now(action.emsg, action.payload);
                        break;
                    case gbe::dota_lifecycle::PushRoute::DotaResponse:
                        previous_action_succeeded = target->GBE_PushDotaResponse(
                            action.emsg & ~GBE_kProtoMask,
                            action.payload,
                            options.wrapped,
                            options.outer_session_field_raw,
                            push_reason);
                        abort_execution = options.abort_on_push_failure && !previous_action_succeeded;
                        break;
                    case gbe::dota_lifecycle::PushRoute::CacheUnsubscribedResponse:
                        previous_action_succeeded = target->GBE_PushDotaCacheUnsubscribedResponse(
                            action.payload,
                            options.wrapped,
                            options.outer_session_field_raw,
                            push_reason);
                        abort_execution = options.abort_on_push_failure && !previous_action_succeeded;
                        break;
                }
                break;
            }
            default:
                break;
        }
        if (abort_execution) {
            result.succeeded = false;
            break;
        }
    }
    return result;
}

bool Steam_Game_Coordinator::GBE_ExecuteDotaCustomGameLifecycleTransition(
    const gbe::dota_custom_game_lifecycle::ExecutionContext &context)
{
    gbe::dota_lifecycle::TransitionEffects effects;
    effects.transition = context.transition;
    effects.local_steam_id = settings ? settings->get_local_steam_id().ConvertToUint64() : 0ull;
    effects.trigger_emsg = context.trigger_emsg;
    effects.source_job = context.source_job;
    effects.runtime_update_note = context.runtime_update_note;
    effects.update_local_member_runtime = context.update_local_member_runtime;
    effects.publish_local_member_data = context.publish_local_member_data;
    effects.fallback_publish_on_runtime_update_failure = context.transition.queue_runtime_lobby_update;

    gbe::dota_lifecycle::ExecutionOptions options;
    options.wrapped = context.wrapped;
    options.outer_session_field_raw = context.outer_session_field_raw;
    return GBE_ExecuteDotaLifecycleActions(
        gbe::dota_lifecycle::build_transition_actions(effects),
        options).runtime_update_queued;
}
