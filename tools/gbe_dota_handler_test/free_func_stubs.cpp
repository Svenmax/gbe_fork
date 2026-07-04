/* Free function stubs for the handler test harness.
 *
 * These stubs provide minimal implementations of free functions that the
 * handler TUs reference but that are not defined in the stubs.h class.
 * The stubs either no-op (for logging) or provide minimal behavior
 * sufficient for the smoke tests.
 *
 * For logic-refactor tasks (3.1.7-3.1.10), replace these stubs with
 * compilations of the real implementations once the pure helpers are
 * extracted into testable translation units.
 */

#include "stubs.h"
#include "dll/gbe_dota_gc_internal.h"

#include <cstdarg>
#include <cstdio>
#include <cstring>

// --- Logging stub (no-op) ---
// The real GBE_GC_DebugLog is defined in steam_game_coordinator.cpp and
// writes to a log file. For tests, we silently discard the message.
void GBE_GC_DebugLog(const char *scope, const char *fmt, ...)
{
    (void)scope; (void)fmt;
}

// --- Steam_Client accessor stub ---
// The real get_steam_client() returns the global Steam_Client singleton.
// For tests, we return a static instance so the handler can check
// steam_gameserver_game_coordinator (which defaults to nullptr).
// Exposed as non-static so smoke tests can wire a server GC into it to
// exercise the server-GC-forward path of the equip handler.
Steam_Client g_test_steam_client;

Steam_Client *get_steam_client()
{
    return &g_test_steam_client;
}

// --- Server GC forwarding stub ---
// The real GBE_PushDotaPlayerEquippedItemsCacheToGC pushes a full item
// cache to the target GC. For tests, we record the call as a ServerGcForward.
bool GBE_PushDotaPlayerEquippedItemsCacheToGC(
    Steam_Game_Coordinator *target_gc,
    const CSteamID &player_steam_id,
    const std::vector<Econ_Item> &source_items,
    bool unsubscribe_first,
    const char *reason)
{
    (void)target_gc; (void)player_steam_id; (void)source_items;
    (void)unsubscribe_first; (void)reason;
    if (g_action_recorder)
        g_action_recorder->record_server_gc_forward(0); // emsg=0 = cache push
    return true;
}

// --- Item unlock style bitmask and equip ops parser ---
// The real GBE_ApplyDotaUnlockStyleBitmask, GBE_ParseDotaEquipOps,
// GBE_SerializeEconItemToGcprotobuf, and GBE_BuildSOSingleObjectFromItem now
// live in dll/gbe_dota_payload_item_helpers.cpp (Phase 3.2.3 extraction).
// That TU is pure (no Steam_Game_Coordinator / Steam_Client / Settings /
// GBE_shared_dota_lobby_state dependency), so the build script links it
// directly into the test binary — no stub duplication required.
//
// History: before 3.2.3, the test harness duplicated the real implementations
// of GBE_ParseDotaEquipOps and GBE_ApplyDotaUnlockStyleBitmask here because
// the full gbe_dota_gc_payload_helpers.cpp TU could not compile offline (it
// pulled in the heavy SDK include chain: steam_game_coordinator.h -> dll.h ->
// common_includes.h -> common_helpers/os_detector.h). The 3.2.3 extraction
// resolved that root cause by moving the pure item helpers into a TU with a
// minimal include list (econ_item.h, gbe_dota_gc_internal.h, gbe_proto_wire.h,
// steamclientpublic.h, tf2 protobuf headers).

// --- get_full_program_path stub ---
// The real get_full_program_path() returns the directory of the running
// executable. For tests, return a dummy path (no VPK file will be found).
std::string get_full_program_path()
{
    return "/tmp/gbe_dota_handler_test/";
}
