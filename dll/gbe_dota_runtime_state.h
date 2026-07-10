#ifndef GBE_DOTA_RUNTIME_STATE_H
#define GBE_DOTA_RUNTIME_STATE_H

#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_payload_wire_helpers.h"

#include <atomic>
#include <cstdint>
#include <vector>

namespace gbe::dota {

struct RuntimeState {
    bool recent_reconnect_context_valid{};
    GBE_DotaReconnectContext recent_reconnect_context{};
    std::atomic<bool> reconnect_eligible{true};
    GBE_DotaServerHelloContext last_server_hello_context{};
    GBE_DotaLootListData vpk_loot_data;
    bool vpk_items_loaded{};
    std::vector<GBE_DotaItemDef> vpk_item_defs;
    GBE_DotaStyleUnlockInfo vpk_style_unlock{};
    bool vpk_items_disabled{};
    uint64_t equip_cache_version{};
};

} // namespace gbe::dota

#endif
