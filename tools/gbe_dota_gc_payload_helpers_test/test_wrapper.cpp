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

// Prevent protobuf headers from being included (we provide stubs)
#define STEAMMESSAGES_PB_H
#define BASE_GCMESSAGES_PB_H
#define ECON_GCMESSAGES_PB_H
#define GCSDK_GCMESSAGES_PB_H
#define GCSYSTEMMSGS_PB_H
#define TF_GCMESSAGES_PB_H

// Provide stub types before including the TU
#include "stubs.h"

// Include the actual payload_helpers TU inline
#include "dll/gbe_dota_gc_payload_helpers.cpp"
