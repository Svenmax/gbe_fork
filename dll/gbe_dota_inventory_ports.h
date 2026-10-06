#ifndef GBE_DOTA_INVENTORY_PORTS_H
#define GBE_DOTA_INVENTORY_PORTS_H

// Cross-GC inventory equip ports (host server cache push / client wearable push).
// Kept out of gbe_dota_gc_internal.h so networking and non-inventory TUs need not
// pull free equip helpers.

#include <cstdint>
#include <vector>

#include <steam/steamtypes.h>

class Steam_Game_Coordinator;
class CSteamID;
struct Econ_Item;

bool GBE_PushDotaPlayerEquippedItemsCacheToGC(
    Steam_Game_Coordinator *target_gc,
    const CSteamID &player_steam_id,
    const std::vector<Econ_Item> &source_items,
    bool unsubscribe_first,
    const char *reason);

bool GBE_RefreshDotaHostEquippedItemsCache(
    Steam_Game_Coordinator *server_gc,
    const CSteamID &player_steam_id,
    const std::vector<Econ_Item> &source_items,
    const char *reason);

bool GBE_PushDotaHeroEquippedItemUpdatesToClientGC(
    Steam_Game_Coordinator *client_gc,
    const CSteamID &player_steam_id,
    uint32 hero_id,
    const std::vector<Econ_Item> &source_items,
    const char *reason);

#endif // GBE_DOTA_INVENTORY_PORTS_H
