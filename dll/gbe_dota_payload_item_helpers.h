#ifndef __INCLUDED_GBE_DOTA_PAYLOAD_ITEM_HELPERS_H__
#define __INCLUDED_GBE_DOTA_PAYLOAD_ITEM_HELPERS_H__

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include <steam/steamtypes.h>

class CSteamID;
struct Econ_Item;

struct GBE_DotaEquipOp {
    uint64_t item_id{};
    uint32_t new_class{};
    uint32_t new_slot{};
    uint32_t style_index{255u};
    bool has_item_id{};
    bool has_new_class{};
    bool has_new_slot{};
};

bool GBE_ParseDotaEquipOps(const uint8 *body, size_t body_size, std::vector<GBE_DotaEquipOp> &equip_ops);

bool GBE_ApplyDotaUnlockStyleBitmask(Econ_Item &item, uint32 style_index);

bool GBE_BuildSOSingleObjectFromItem(const Econ_Item &item, const CSteamID &steam_id, std::string &output);

std::string GBE_SerializeEconItemToGcprotobuf(const Econ_Item &item, CSteamID steam_id, uint32 gc_version, bool is_portal2);

#endif // __INCLUDED_GBE_DOTA_PAYLOAD_ITEM_HELPERS_H__
