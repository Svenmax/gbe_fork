/* Test wrapper for gbe_dota_inventory_handlers.cpp
 *
 * This file intercepts the heavy Steam SDK and protobuf includes from
 * steam_game_coordinator.h, dll.h, and the .pb.h headers, replacing them
 * with lightweight stubs defined in stubs.h. This allows the handler TU
 * to compile in the offline test environment without the full SDK
 * dependency chain or generated protobuf files.
 *
 * The stubs.h Steam_Game_Coordinator class records side effects
 * (push_incoming_now, save_items_to_file, callback_item_updated) to a
 * global ActionRecorder instead of executing real I/O.
 *
 * Usage: compile this file together with smoke_test.cpp and free_func_stubs.cpp.
 */

// Prevent the real Steam SDK headers from being included by pre-defining their guards
#define __INCLUDED_STEAM_GAME_COORDINATOR_H__
#define __INCLUDED_DLL_H__
#define BASE_INCLUDE_H
#define __INCLUDED_CALLSYSTEM_H__
#define __INCLUDED_ECON_ITEM_H__
#define __INCLUDED_COMMON_INCLUDES__
#define STEAMCLIENTPUBLIC_H
#define STEAMTYPES_H

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
// This compiles the real GBE_ParseDotaEquipOps / GBE_ApplyDotaUnlockStyleBitmask
// / GBE_SerializeEconItemToGcprotobuf / GBE_BuildSOSingleObjectFromItem against
// our stub CSteamID / Econ_Item / CSOEconItem / CMsgSOSingleObject types,
// replacing the previous stub duplication in free_func_stubs.cpp.
#include "dll/gbe_dota_payload_item_helpers.cpp"

// Include the actual handler TU inline.
// This compiles the real handler definitions against our stub class.
#include "dll/gbe_dota_inventory_handlers.cpp"

// Include low-dependency misc handlers so smoke tests can cover standalone
// request/response paths without pulling in the full coordinator.
#include "dll/gbe_dota_misc_handlers.cpp"

// Include chat handlers for minimal smoke coverage of chat-channel side effects.
#include "dll/gbe_dota_chat_handlers.cpp"

// Include lobby handlers for minimal smoke coverage of lobby teardown/order paths.
#include "dll/gbe_dota_lobby_handlers.cpp"

// Include match handlers for minimal smoke coverage of launch/match order paths.
#include "dll/gbe_dota_match_handlers.cpp"
