// gbe_dota_action_model.h
//
// Side-effect action model for Dota GC handlers.
//
// This header defines the canonical action type used by the contract between
// pure helper functions and coordinator-owned side-effect execution. It is the
// single source of truth for action enumeration names; the test harness in
// tools/gbe_dota_handler_test/stubs.h includes this header and uses
// GBE_DotaActionType for its RecordedAction type field.
//
// ============================================================================
// Contract
// ============================================================================
//
// - Pure helper functions build a GBE_DotaActionList (decisions + payloads)
//   WITHOUT touching coordinator state or executing real side effects.
// - Coordinator methods consume a GBE_DotaActionList and execute each action
//   serially via the real side-effect methods (push_incoming_now,
//   push_incoming, save_items_to_file, callback_item_updated, server-GC
//   forward, network broadcast, lobby snapshot refresh).
// - Tests intercept execution via the recording stub coordinator in
//   tools/gbe_dota_handler_test/stubs.h and assert on the recorded sequence
//   (see smoke_test.cpp).
//
// ============================================================================
// When to use an action list
// ============================================================================
//
// - ONLY when a handler produces multiple observable side effects.
// - Single-side-effect handlers keep their direct coordinator call; do not
//   wrap every handler in an action list.
// - A handler with a single response push and nothing else does not need an
//   action list.
//
// ============================================================================
// Protocol-sensitive ordering invariants
// ============================================================================
//
// These invariants MUST be preserved by every handler that builds an action
// list. They are enforced by the smoke tests in
// tools/gbe_dota_handler_test/smoke_test.cpp.
//
//   1. SO Update precedes SO Destroy for the unlock-style flow:
//      PushIncomingNow(emsg=22, SO Update for unlocked item)
//        -> PushIncomingNow(emsg=24, SO Destroy for spent consumable)
//        -> PushIncomingNow(emsg=2572, response)
//      Rationale: the client must observe the new style bit before the
//      consumable removal is acknowledged.
//
//   2. State mutation precedes persistence precedes response for set-style:
//      CallbackItemUpdated(item)
//        -> SaveItemsToFile()
//        -> PushIncomingNow(emsg=2578, response)
//      Rationale: the callback fires from the in-memory mutation, persistence
//      follows, and the client only receives the ack after durable state.
//
//   3. Full item cache precedes create/update SO messages when
//      bootstrapping a remote GC's view of a player's inventory
//      (CacheSubscribed before emsg=21/26 in the cross-GC push path).
//
//   4. Local response precedes server-GC forward, network broadcast, and
//      lobby snapshot refresh in the equip flow:
//      PushIncomingNow(emsg=26, SO Update for modified items)
//        -> PushIncomingNow(emsg=2570, local response)
//        -> ServerGcForward(equipped-items cache push)
//        -> NetworkBroadcast(gameserver items message)
//        -> LobbySnapshotRefresh(reason)
//      Rationale: the local client is acknowledged first; cross-process and
//      cross-network propagation happen after.
//
// ============================================================================
// Field usage by action type
// ============================================================================
//
//   PushIncomingNow       emsg, payload                 (job_id when tied)
//   PushIncoming          emsg, payload                 (job_id when tied)
//   SaveItemsToFile       (no fields)
//   CallbackItemUpdated   target_steam_id, item_id
//   ServerGcForward       emsg, target_steam_id         (reason optional)
//   NetworkBroadcast      (no fields)
//   GcMemoryReset         reason, leave_generic_lobby, clear_queued_messages
//   LobbySnapshotRefresh  reason
//   GenericLobbyCreate    reason
//   GenericLobbyLeave     lobby_id
//   GenericLobbyJoin      lobby_id
//   RichPresenceUpdate    status, lobby_state, include_party, include_lobby
//   RichPresenceClear     reason
//   LaunchPersonaState    status, lobby_state, include_party, include_lobby, reason
//   LobbyLocalMemberData  reason
//   LobbyMetadataPublish  reason
//   LaunchPeripheralReset reason
//   LobbyCacheSubscriptionRecord reason
//   LaunchStateGameStateRecord reason
//   SettingsLobbySync  reason
//   SettingsLobbyClear item_id, reason
//   DotaLobbyRuntimeClear reason
//   AbandonedLobbySuppressed lobby_id, reason
//   LaunchMessagesDiscardedForAbandon reason
//   PendingResetAfterCacheUnsubscribed lobby_id
//   PendingResetAfterCacheUnsubscribedClear lobby_id
//   PendingNormalSignoutFinalizeAfterCacheUnsubscribed lobby_id
//   LobbyStateApply       lobby_state, lobby_game_state
//   PostGameLobbyStateApply lobby_state, lobby_game_state, item_id=pre_channel,
//                           job_id=post_channel, payload=channel_name, reason
//   LobbyMemberRuntimeUpdate target_steam_id, connected, hero_id, has_hero_id
//   LaunchPhaseMark       launch_phase, reason
//   SharedLobbyPublish    reason
//   PracticeLobbyDetailsUpdate reason
//   RuntimeLobbyDetailsUpdate emsg, job_id, lobby_state, lobby_game_state, delay, reason
//   RichPresenceUpdate    status, presence_lobby_state, include_party, include_lobby

#ifndef GBE_DOTA_ACTION_MODEL_H
#define GBE_DOTA_ACTION_MODEL_H

#include "gbe_dota_lobby_generation.h"

#include <cstdint>
#include <string>
#include <vector>

enum class GBE_DotaActionType {
    PushIncomingNow,       // coordinator->client immediate push (emsg + payload)
    PushIncoming,          // delayed coordinator->client push
    SaveItemsToFile,       // persist local inventory to disk
    CallbackItemUpdated,   // notify Steam callback (target_steam_id, item_id)
    ServerGcForward,       // forward to server GC (emsg, target_steam_id)
    NetworkBroadcast,      // broadcast to all gameservers
    GcMemoryReset,         // reset coordinator GC memory before lifecycle create
    LobbySnapshotRefresh,  // replay private lobby snapshot (reason)
    GenericLobbyCreate,    // create generic Steam lobby for Dota lobby
    GenericLobbyLeave,     // leave local generic lobby during lifecycle cleanup
    GenericLobbyJoin,      // join matched generic lobby during join flow
    SettingsLobbyClear,    // clear settings lobby during signout cleanup
    RichPresenceUpdate,    // update Dota launch rich presence
    RichPresenceClear,     // clear Dota launch rich presence
    LaunchPersonaState,    // build Dota launch persona state metadata
    LobbyLocalMemberData,  // publish local member data to generic lobby
    LobbyMetadataPublish,  // publish generic lobby metadata
    LaunchPeripheralReset, // clear launch peripheral dedupe/callback state
    LobbyCacheSubscriptionRecord, // record cache-subscription payload state
    LaunchStateGameStateRecord, // record last pushed Dota launch game state
    SettingsLobbySync,   // sync settings lobby from generic lobby metadata
    DotaLobbyRuntimeClear, // clear local Dota lobby state and shared runtime snapshot
    AbandonedLobbySuppressed, // mark a Dota lobby as abandoned/suppressed
    LaunchMessagesDiscardedForAbandon, // discard queued launch messages during abandon
    PendingResetAfterCacheUnsubscribed, // defer full reset until cache unsubscribe is observed
    PendingResetAfterCacheUnsubscribedClear, // clear deferred reset after postgame teardown is queued
    PendingNormalSignoutFinalizeAfterCacheUnsubscribed, // defer normal signout cleanup until 25 is retrieved
    LobbyStateApply,      // apply local lobby state and game state
    PostGameLobbyStateApply, // apply postgame lobby chat/state/cache patch before publish
    LobbyMemberRuntimeUpdate, // apply connected/hero state to a lobby member
    LaunchPhaseMark,      // advance launch phase without an implicit publish
    SharedLobbyPublish,   // publish the current local lobby to shared state
    PracticeLobbyDetailsUpdate, // send direct or wrapped practice lobby details
    RuntimeLobbyDetailsUpdate, // queue delayed runtime lobby details update
};

constexpr const char *GBE_DescribeDotaActionType(GBE_DotaActionType type)
{
    switch (type) {
        case GBE_DotaActionType::PushIncomingNow: return "push_incoming_now";
        case GBE_DotaActionType::PushIncoming: return "push_incoming";
        case GBE_DotaActionType::SaveItemsToFile: return "save_items_to_file";
        case GBE_DotaActionType::CallbackItemUpdated: return "callback_item_updated";
        case GBE_DotaActionType::ServerGcForward: return "server_gc_forward";
        case GBE_DotaActionType::NetworkBroadcast: return "network_broadcast";
        case GBE_DotaActionType::GcMemoryReset: return "gc_memory_reset";
        case GBE_DotaActionType::LobbySnapshotRefresh: return "lobby_snapshot_refresh";
        case GBE_DotaActionType::GenericLobbyCreate: return "generic_lobby_create";
        case GBE_DotaActionType::GenericLobbyLeave: return "generic_lobby_leave";
        case GBE_DotaActionType::GenericLobbyJoin: return "generic_lobby_join";
        case GBE_DotaActionType::SettingsLobbyClear: return "settings_lobby_clear";
        case GBE_DotaActionType::RichPresenceUpdate: return "rich_presence_update";
        case GBE_DotaActionType::RichPresenceClear: return "rich_presence_clear";
        case GBE_DotaActionType::LaunchPersonaState: return "launch_persona_state";
        case GBE_DotaActionType::LobbyLocalMemberData: return "lobby_local_member_data";
        case GBE_DotaActionType::LobbyMetadataPublish: return "lobby_metadata_publish";
        case GBE_DotaActionType::LaunchPeripheralReset: return "launch_peripheral_reset";
        case GBE_DotaActionType::LobbyCacheSubscriptionRecord: return "lobby_cache_subscription_record";
        case GBE_DotaActionType::LaunchStateGameStateRecord: return "launch_state_game_state_record";
        case GBE_DotaActionType::SettingsLobbySync: return "settings_lobby_sync";
        case GBE_DotaActionType::DotaLobbyRuntimeClear: return "dota_lobby_runtime_clear";
        case GBE_DotaActionType::AbandonedLobbySuppressed: return "abandoned_lobby_suppressed";
        case GBE_DotaActionType::LaunchMessagesDiscardedForAbandon: return "launch_messages_discarded_for_abandon";
        case GBE_DotaActionType::PendingResetAfterCacheUnsubscribed: return "pending_reset_after_cache_unsubscribed";
        case GBE_DotaActionType::PendingResetAfterCacheUnsubscribedClear: return "pending_reset_after_cache_unsubscribed_clear";
        case GBE_DotaActionType::PendingNormalSignoutFinalizeAfterCacheUnsubscribed: return "pending_normal_signout_finalize_after_cache_unsubscribed";
        case GBE_DotaActionType::LobbyStateApply: return "lobby_state_apply";
        case GBE_DotaActionType::PostGameLobbyStateApply: return "postgame_lobby_state_apply";
        case GBE_DotaActionType::LobbyMemberRuntimeUpdate: return "lobby_member_runtime_update";
        case GBE_DotaActionType::LaunchPhaseMark: return "launch_phase_mark";
        case GBE_DotaActionType::SharedLobbyPublish: return "shared_lobby_publish";
        case GBE_DotaActionType::PracticeLobbyDetailsUpdate: return "practice_lobby_details_update";
        case GBE_DotaActionType::RuntimeLobbyDetailsUpdate: return "runtime_lobby_details_update";
    }
    return "unknown";
}

struct GBE_DotaAction {
    GBE_DotaActionType type{};
    uint32_t emsg{};              // PushIncomingNow / PushIncoming / ServerGcForward
    std::string payload;          // PushIncomingNow / PushIncoming / PostGame channel name
    uint64_t target_steam_id{};   // CallbackItemUpdated / ServerGcForward
    uint64_t item_id{};           // CallbackItemUpdated / PostGame pre_channel
    uint64_t job_id{};            // when tied to a source/target job / PostGame post_channel
    std::string reason;           // LobbySnapshotRefresh
    bool leave_generic_lobby{};   // GcMemoryReset
    bool clear_queued_messages{}; // GcMemoryReset
    uint32_t lobby_state{};       // lifecycle state apply/runtime update
    uint32_t lobby_game_state{};  // lifecycle state apply/runtime update
    uint32_t launch_phase{};      // LaunchPhaseMark
    uint32_t hero_id{};           // LobbyMemberRuntimeUpdate
    bool connected{};             // LobbyMemberRuntimeUpdate
    bool has_hero_id{};           // LobbyMemberRuntimeUpdate
    bool only_when_previous_action_succeeded{}; // conditional follow-up action
    bool only_when_runtime_update_not_queued{}; // runtime update fallback action
    double delay{};               // RuntimeLobbyDetailsUpdate
    std::string status;           // RichPresenceUpdate / LaunchPersonaState
    std::string presence_lobby_state; // RichPresenceUpdate lobby state label
    bool include_party{};         // RichPresenceUpdate
    bool include_lobby{true};     // RichPresenceUpdate
    gbe::dota_lobby_generation::Boundary generation_boundary{gbe::dota_lobby_generation::Boundary::Reset}; // GcMemoryReset / DotaLobbyRuntimeClear
};

// Ordered list of intended side effects built by a pure helper and consumed
// by a coordinator method. Kept as a plain alias (no builder wrapper) until
// repeated construction patterns in 3.1.7-3.1.10 justify helpers.
using GBE_DotaActionList = std::vector<GBE_DotaAction>;

#endif // GBE_DOTA_ACTION_MODEL_H
