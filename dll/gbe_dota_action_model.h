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
//   LobbySnapshotRefresh  reason
//   GenericLobbyLeave     lobby_id
//   RichPresenceUpdate    status, lobby_state, include_party, include_lobby
//   RichPresenceClear     reason
//   LaunchPersonaState    status, lobby_state, include_party, include_lobby, reason
//   LobbyLocalMemberData  reason
//   LobbyMetadataPublish  reason
//   LaunchPeripheralReset reason
//   LobbyCacheSubscriptionRecord reason
//   LaunchStateGameStateRecord reason
//   SettingsLobbySync  reason
//   AbandonedLobbySuppressed lobby_id, reason
//   LaunchMessagesDiscardedForAbandon reason

#ifndef GBE_DOTA_ACTION_MODEL_H
#define GBE_DOTA_ACTION_MODEL_H

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
    LobbySnapshotRefresh,  // replay private lobby snapshot (reason)
    GenericLobbyLeave,     // leave local generic lobby during lifecycle cleanup
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
    AbandonedLobbySuppressed, // mark a Dota lobby as abandoned/suppressed
    LaunchMessagesDiscardedForAbandon, // discard queued launch messages during abandon
};

struct GBE_DotaAction {
    GBE_DotaActionType type{};
    uint32_t emsg{};              // PushIncomingNow / PushIncoming / ServerGcForward
    std::string payload;          // PushIncomingNow / PushIncoming
    uint64_t target_steam_id{};   // CallbackItemUpdated / ServerGcForward
    uint64_t item_id{};           // CallbackItemUpdated
    uint64_t job_id{};            // when tied to a source/target job
    std::string reason;           // LobbySnapshotRefresh
};

// Ordered list of intended side effects built by a pure helper and consumed
// by a coordinator method. Kept as a plain alias (no builder wrapper) until
// repeated construction patterns in 3.1.7-3.1.10 justify helpers.
using GBE_DotaActionList = std::vector<GBE_DotaAction>;

#endif // GBE_DOTA_ACTION_MODEL_H
