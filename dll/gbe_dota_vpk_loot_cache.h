#ifndef __INCLUDED_GBE_DOTA_VPK_LOOT_CACHE_H__
#define __INCLUDED_GBE_DOTA_VPK_LOOT_CACHE_H__

#include "dll/gbe_dota_unlock_items.h"

const GBE_DotaLootListData &GBE_GetDotaVpkLootData();
void GBE_SetDotaVpkLootData(GBE_DotaLootListData &&loot_data);

#endif // __INCLUDED_GBE_DOTA_VPK_LOOT_CACHE_H__
