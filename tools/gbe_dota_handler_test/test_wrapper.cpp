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

#include <mutex>

std::recursive_mutex global_mutex;

// Include the pure item payload helpers TU inline (Phase 3.2.3).
// This compiles the real GBE_ParseDotaEquipOps / GBE_ApplyDotaUnlockStyleBitmask
// / GBE_SerializeEconItemToGcprotobuf / GBE_BuildSOSingleObjectFromItem against
// our stub CSteamID / Econ_Item / CSOEconItem / CMsgSOSingleObject types,
// replacing the previous stub duplication in free_func_stubs.cpp.
#include "dll/gbe_dota_payload_item_helpers.cpp"

// Include the actual handler TU inline.
// This compiles the real handler definitions against our stub class.
#include "dll/gbe_dota_inventory_handlers.cpp"

// Compile the shared lifecycle executor against the same recording seams used
// by direct and wrapped handler smoke tests.
#include "dll/gbe_dota_lifecycle_actions.cpp"
#include "dll/gbe_dota_custom_game_lifecycle_coordinator.cpp"

// Keep the wrapped custom-game lifecycle path executable in the lightweight
// harness without pulling in the full post-login dispatcher dependency graph.
#include "dll/gbe_dota_wrapped_custom_game_handlers.cpp"

struct TestEquipPlannerSummary {
    bool parse_failed{};
    size_t equip_op_count{};
    size_t modified_item_count{};
    size_t action_count{};
    bool has_server_forward{};
    bool broadcast_equipped_items{};
    const char *snapshot_refresh_reason{};
    uint16 first_item_slot{};
    uint8 first_item_style{};
    GBE_DotaActionType actions[8]{};
    uint32 action_emsgs[8]{};
};

TestEquipPlannerSummary test_plan_equip_items_request(
    const uint8 *body,
    size_t body_size,
    const std::vector<Econ_Item> &items,
    uint64_t cache_version,
    bool is_dota_client,
    bool server_gc_has_active_lobby,
    bool lobby_snapshot_refresh_available)
{
    EquipItemsPlanningContext context{};
    context.is_dota_client = is_dota_client;
    context.server_gc_has_active_lobby = server_gc_has_active_lobby;
    context.lobby_snapshot_refresh_available = lobby_snapshot_refresh_available;

    EquipItemsPlan plan = plan_equip_items_request(body, body_size, items, cache_version, context);

    TestEquipPlannerSummary summary{};
    summary.parse_failed = plan.parse_failed;
    summary.equip_op_count = plan.equip_ops.size();
    summary.modified_item_count = plan.modified_item_ids.size();
    summary.action_count = plan.actions.size();
    summary.has_server_forward = plan.server_forward.enabled;
    summary.broadcast_equipped_items = plan.broadcast_equipped_items;
    summary.snapshot_refresh_reason = plan.snapshot_refresh_reason;
    if (!plan.items_after_mutation.empty()) {
        auto it = plan.items_after_mutation[0].equip_states.find(2u);
        if (it != plan.items_after_mutation[0].equip_states.end())
            summary.first_item_slot = it->second;
        summary.first_item_style = plan.items_after_mutation[0].style;
    }
    const size_t max_actions = sizeof(summary.actions) / sizeof(summary.actions[0]);
    for (size_t i = 0; i < plan.actions.size() && i < max_actions; ++i) {
        summary.actions[i] = plan.actions[i].type;
        summary.action_emsgs[i] = plan.actions[i].emsg;
    }
    return summary;
}

// Include low-dependency misc handlers so smoke tests can cover standalone
// request/response paths without pulling in the full coordinator.
#include "dll/gbe_dota_misc_handlers.cpp"

// Include chat handlers for minimal smoke coverage of chat-channel side effects.
#include "dll/gbe_dota_chat_handlers.cpp"

// Include lobby handlers for minimal smoke coverage of lobby teardown/order paths.
#include "dll/gbe_dota_lobby_handlers.cpp"

// Include match handlers for minimal smoke coverage of launch/match order paths.
#include "dll/gbe_dota_match_handlers.cpp"
