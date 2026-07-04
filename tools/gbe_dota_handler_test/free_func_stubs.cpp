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
// The real GBE_ApplyDotaUnlockStyleBitmask and GBE_ParseDotaEquipOps live
// in dll/gbe_dota_gc_payload_helpers.cpp, but that TU pulls in the full
// Steam SDK include chain (steam_game_coordinator.h -> dll.h ->
// common_includes.h -> common_helpers/os_detector.h) which is not
// available in the offline test environment.
//
// Both functions are pure (they only touch proto_wire + Econ_Item + the
// GBE_DotaEquipOp DTO), so we compile the real implementations here
// instead of trying to compile the whole payload_helpers TU. This gives
// the handler smoke tests access to the real parsing and mutation logic.
//
// TODO(phase-3.2): once payload helpers are split into a pure-logic TU
// that doesn't depend on the heavy SDK headers, both this duplicate and
// the gbe_dota_gc_payload_helpers_test/test_wrapper.cpp shim can be
// replaced by linking against that TU directly.
#include "dll/gbe_proto_wire.h"

bool GBE_ParseDotaEquipOps(const uint8 *body, size_t body_size, std::vector<GBE_DotaEquipOp> &equip_ops)
{
    equip_ops.clear();

    if (!body || body_size == 0)
        return false;

    size_t offset = 0;
    while (offset < body_size) {
        gbe::proto_wire::Field outer{};
        if (!gbe::proto_wire::read_next_field(body, body_size, offset, outer))
            return false;
        if (outer.number != 1u || outer.wire_type != 2u)
            return false;

        const uint8_t *sub = body + outer.value_offset;
        const size_t sub_len = outer.value_size;
        size_t sub_off = 0;
        GBE_DotaEquipOp op{};
        op.style_index = 255u;  // default: no style change

        while (sub_off < sub_len) {
            gbe::proto_wire::Field field{};
            if (!gbe::proto_wire::read_next_field(sub, sub_len, sub_off, field))
                return false;

            if (field.number < 1u || field.number > 4u)
                continue;

            if (field.wire_type != 0u)
                return false;

            uint64_t val = 0;
            if (!gbe::proto_wire::read_field_uint64(sub, sub_len, field, val))
                return false;

            if (field.number == 1u) {
                op.item_id = val;
                op.has_item_id = true;
            } else if (field.number == 2u) {
                if (val > UINT16_MAX)
                    return false;
                op.new_class = static_cast<uint32_t>(val);
                op.has_new_class = true;
            } else if (field.number == 3u) {
                if (val > UINT16_MAX)
                    return false;
                op.new_slot = static_cast<uint32_t>(val);
                op.has_new_slot = true;
            } else if (field.number == 4u) {
                if (val > UINT8_MAX)
                    return false;
                op.style_index = static_cast<uint32_t>(val);
            }
        }

        if (!op.has_item_id || !op.has_new_class || !op.has_new_slot)
            return false;

        equip_ops.push_back(op);
    }

    return !equip_ops.empty();
}

bool GBE_ApplyDotaUnlockStyleBitmask(Econ_Item &item, uint32 style_index)
{
    if (style_index >= 32u)
        return false;

    // Set the item's current style
    item.style = static_cast<uint8>(style_index);

    // Find or create attr 400 (unlocked styles bitmask)
    for (auto &attr : item.attributes) {
        if (attr.def == 400u) {
            uint32_t current_val = 0;
            if (attr.value_bytes.size() >= 4) {
                memcpy(&current_val, attr.value_bytes.data(), 4);
            }
            current_val |= (1u << style_index);
            attr.value_bytes.assign(reinterpret_cast<const char *>(&current_val), 4);
            return true;
        }
    }

    // Attr 400 not found, create it with all bits set (LAN behavior)
    Econ_Item_Attribute unlock_attr;
    unlock_attr.def = 400u;
    uint32_t val = 0xFFFFFFFFu;
    unlock_attr.value_bytes.assign(reinterpret_cast<const char *>(&val), 4);
    unlock_attr.type = Econ_Item_Attribute::ATTR_TYPE_INT;
    item.attributes.push_back(unlock_attr);
    return true;
}

// --- get_full_program_path stub ---
// The real get_full_program_path() returns the directory of the running
// executable. For tests, return a dummy path (no VPK file will be found).
std::string get_full_program_path()
{
    return "/tmp/gbe_dota_handler_test/";
}
