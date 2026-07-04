/* Test wrapper for gbe_dota_gc_payload_helpers.cpp
 *
 * This file intercepts the heavy Steam SDK and protobuf includes from
 * steam_game_coordinator.h, dll.h, and the .pb.h headers, replacing them
 * with lightweight stubs defined in stubs.h. This allows the payload_helpers
 * TU to compile in the offline test environment without the full SDK
 * dependency chain or generated protobuf files.
 *
 * Usage: compile this file instead of gbe_dota_gc_payload_helpers.cpp directly.
 */

// Prevent the real Steam SDK headers from being included by pre-defining their guards
#define __INCLUDED_STEAM_GAME_COORDINATOR_H__
#define __INCLUDED_DLL_H__
#define BASE_INCLUDE_H
#define __INCLUDED_CALLSYSTEM_H__
#define __INCLUDED_ECON_ITEM_H__
#define __INCLUDED_COMMON_INCLUDES__
#define STEAMCLIENTPUBLIC_H

// Prevent protobuf headers from being included (we provide stubs)
#define STEAMMESSAGES_PB_H
#define BASE_GCMESSAGES_PB_H
#define ECON_GCMESSAGES_PB_H
#define GCSDK_GCMESSAGES_PB_H
#define GCSYSTEMMSGS_PB_H
#define TF_GCMESSAGES_PB_H

// Provide stub types before including the TU
#include "stubs.h"

// Include the pure item payload helpers TU inline (Phase 3.2.3).
// gbe_dota_gc_payload_helpers.cpp no longer defines GBE_ParseDotaEquipOps /
// GBE_ApplyDotaUnlockStyleBitmask / GBE_SerializeEconItemToGcprotobuf /
// GBE_BuildSOSingleObjectFromItem (they moved to
// dll/gbe_dota_payload_item_helpers.cpp), so we compile that TU inline here
// against the stub CSteamID / Econ_Item / CSOEconItem / CMsgSOSingleObject
// types to satisfy the test's references.
#include "dll/gbe_dota_payload_item_helpers.cpp"

// Include the pure wire payload helpers TU inline (Phase 3.2.2).
// gbe_dota_gc_payload_helpers.cpp no longer defines the 14 pure wire helpers
// (GBE_PatchDotaLobbyTemplateIdentifiers, GBE_ForceDotaLobbyCacheOwnerSOID,
// GBE_PrepareDotaWelcomeBody, GBE_PrepareDotaDirectReplayMessage,
// GBE_RewriteAccountIdVarintInDirectProtoBody, GBE_TryPatchDotaAccountIdVarint,
// GBE_TryPatchDotaAccountIdFixed32, GBE_PatchDotaTemplateIdentifiers,
// GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState,
// GBE_PatchDotaPracticeLobbyLaunchTemplate, GBE_ForceDotaLobbyUpdateOwnerSOID,
// GBE_PatchDotaLobbyTemplateIdentifiersIfPresent, GBE_PatchDotaWelcomeAccountObjects,
// GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage,
// GBE_PrepareDotaPersonaStatePeripheralMessage) -- they moved to
// dll/gbe_dota_payload_wire_helpers.cpp. We compile that TU inline here against
// the stub CMsgProtoBufHeader / CMsgSOCacheSubscribed / CMsgSOMultipleObjects /
// CMsgSOIDOwner / CMsgServerWelcome types so the test's references resolve to
// the real wire logic instead of duplicated stubs.
#include "dll/gbe_dota_payload_wire_helpers.cpp"

// Include the actual payload_helpers TU inline
#include "dll/gbe_dota_gc_payload_helpers.cpp"
