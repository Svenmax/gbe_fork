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
#include "dll/gbe_proto_wire.h"

#include <array>

// --- Logging stub (no-op) ---
// The real GBE_GC_DebugLog is defined in steam_game_coordinator.cpp and
// writes to a log file. For tests, we silently discard the message.
void GBE_GC_DebugLog(const char *scope, const char *fmt, ...)
{
    (void)scope; (void)fmt;
}

const char *GBE_DescribeDotaLaunchPhase(uint32 phase)
{
    switch (phase) {
        case 0: return "none";
        case 1: return "requested";
        case 2: return "serversetup_synced";
        case 3: return "run_queued";
        case 4: return "loaded";
        default: return "none";
    }
}

const char * const GBE_kDotaAbandonPersonaStateInitHex = "";
bool GBE_pending_reset_after_cache_unsubscribed = false;
uint64 GBE_pending_reset_after_cache_unsubscribed_lobby_id = 0;
bool GBE_pending_dota_normal_signout_finalize_after_25 = false;
uint64 GBE_pending_dota_normal_signout_finalize_lobby_id = 0;
bool GBE_recent_dota_reconnect_context_valid = false;
GBE_DotaReconnectContext GBE_recent_dota_reconnect_context{};
std::atomic<bool> GBE_dota_reconnect_eligible{true};
bool GBE_dota_host_showcase_equip_pushed = false;
uint64 GBE_dota_host_local_wearable_refresh_generation = 0;
uint64 GBE_dota_host_local_wearable_refresh_steam_id = 0;
uint32 GBE_dota_host_local_wearable_refresh_hero_id = 0;
const std::array<uint8, 8> GBE_kOldDotaLobbyIdVarint = { 0x83, 0xcf, 0xa2, 0xb4, 0xa2, 0xff, 0xf9, 0x34 };
const std::array<uint8, 5> GBE_kOldDotaPracticeLobbyMatchIdVarint = { 0xae, 0xbb, 0xa3, 0xcf, 0x06 };

bool GBE_GetRecentDotaReconnectContext(GBE_DotaReconnectContext *out)
{
    if (!out || !GBE_recent_dota_reconnect_context_valid)
        return false;
    *out = GBE_recent_dota_reconnect_context;
    return true;
}

void GBE_SetRecentDotaReconnectContext(const GBE_DotaReconnectContext &ctx)
{
    GBE_recent_dota_reconnect_context = ctx;
    GBE_recent_dota_reconnect_context_valid = true;
}

void GBE_ClearRecentDotaReconnectContext()
{
    GBE_recent_dota_reconnect_context_valid = false;
    GBE_recent_dota_reconnect_context = GBE_DotaReconnectContext{};
}

bool GBE_IsDotaReconnectEligible()
{
    return GBE_dota_reconnect_eligible.load();
}

void GBE_SetDotaReconnectEligible(bool eligible)
{
    GBE_dota_reconnect_eligible.store(eligible);
}

bool GBE_ConsumeDotaReconnectEligibility()
{
    bool expected = true;
    return GBE_dota_reconnect_eligible.compare_exchange_strong(expected, false);
}

void GBE_LogDotaResponsePacket(const char *reason, uint32 inner_emsg, bool wrapped, const std::string &inner_payload, const std::string &outbound_payload, uint64 job_id, uint32 lobby_state, uint32 lobby_game_state)
{
    (void)reason; (void)inner_emsg; (void)wrapped; (void)inner_payload; (void)outbound_payload; (void)job_id; (void)lobby_state; (void)lobby_game_state;
}

bool GBE_PrepareDotaPersonaStatePeripheralMessage(
    const char *template_hex,
    uint64 steam_id,
    uint64 lobby_id,
    std::string &message)
{
    (void)template_hex; (void)steam_id; (void)lobby_id;
    message.clear();
    return true;
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
    if (target_gc)
        target_gc->GBE_MirrorDotaEquippedItemsForUser(player_steam_id, source_items, reason);
    if (g_action_recorder)
        g_action_recorder->record_server_gc_cache_forward(
            0,
            target_gc,
            player_steam_id.ConvertToUint64(),
            source_items.size(),
            unsubscribe_first,
            reason); // emsg=0 = cache push
    return true;
}

bool GBE_RefreshDotaHostEquippedItemsCache(
    Steam_Game_Coordinator *server_gc,
    const CSteamID &player_steam_id,
    const std::vector<Econ_Item> &source_items,
    const char *reason)
{
    return GBE_PushDotaPlayerEquippedItemsCacheToGC(server_gc, player_steam_id, source_items, true, reason);
}

bool GBE_AdaptDotaJoinChatChannelResponsePayload(
    uint64 steam_id,
    uint64 generic_lobby_id,
    uint64 channel_id,
    const std::string &channel_name,
    const std::string &player_name,
    const std::vector<GBE_DotaLobbyMemberState> &channel_members,
    uint64 owner_steam_id,
    const std::string &owner_name,
    uint32 channel_type,
    std::string &message)
{
    (void)steam_id; (void)generic_lobby_id; (void)channel_name;
    (void)player_name; (void)channel_members; (void)owner_steam_id;
    (void)owner_name; (void)channel_type;
    message.clear();
    gbe::proto_wire::append_varint_field(message, 1u, channel_id);
    return true;
}

// --- Item unlock style bitmask and equip ops parser ---
// The real GBE_ApplyDotaUnlockStyleBitmask, GBE_ParseDotaEquipOps,
// GBE_SerializeEconItemToGcprotobuf, and GBE_BuildSOSingleObjectFromItem now
// live in dll/gbe_dota_payload_item_helpers.cpp (Phase 3.2.3 extraction).
// That TU is pure (no Steam_Game_Coordinator / Steam_Client / Settings /
// shared lobby state dependency), so the build script links it
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
