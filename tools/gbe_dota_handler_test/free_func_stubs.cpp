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
static Steam_Client g_test_steam_client;

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

// --- Item unlock style bitmask stub ---
// The real GBE_ApplyDotaUnlockStyleBitmask modifies the item's attr 400
// (unlocked styles bitmask). For the smoke test, we just set item.style
// and return true for valid indices. The real logic is tested in the
// payload_helpers_test suite.
bool GBE_ApplyDotaUnlockStyleBitmask(Econ_Item &item, uint32 style_index)
{
    if (style_index >= 32)
        return false;
    item.style = static_cast<uint8>(style_index);
    return true;
}

// --- Equip ops parser stub ---
// The real GBE_ParseDotaEquipOps parses repeated equip operations from a
// ClientToGCEquipItemsRequest body. For the smoke test (which doesn't
// exercise the equip handler), we return false (parse failure / no ops).
// When the equip handler smoke test is added, replace this with the real
// implementation from gbe_dota_gc_payload_helpers.cpp.
bool GBE_ParseDotaEquipOps(const uint8 *body, size_t body_size, std::vector<GBE_DotaEquipOp> &equip_ops)
{
    (void)body; (void)body_size; (void)equip_ops;
    return false;
}

// --- get_full_program_path stub ---
// The real get_full_program_path() returns the directory of the running
// executable. For tests, return a dummy path (no VPK file will be found).
std::string get_full_program_path()
{
    return "/tmp/gbe_dota_handler_test/";
}
