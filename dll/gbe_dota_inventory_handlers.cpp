/* Copyright (C) 2019 Mr Goldberg
   This file is part of the Goldberg Emulator

   The Goldberg Emulator is free software; you can redistribute it and/or
   modify it under the terms of the GNU Lesser General Public
   License as published by the Free Software Foundation; either
   version 3 of the License, or (at your option) any later version.

   The Goldberg Emulator is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
   Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public
   License along with the Goldberg Emulator; if not, see
   <http://www.gnu.org/licenses/>.  */

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_action_model.h"
#include "gbe_dota_request_router.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_dota_payload_item_helpers.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_gc_internal.h"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <random>
#include <string>
#include <vector>
#include <unordered_set>
#include <steammessages.pb.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/econ_gcmessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>
#include <tf2/gcsystemmsgs.pb.h>
#include <tf2/tf_gcmessages.pb.h>

using namespace gamecoordinator::tf2;

// ============================================================================
// Side-effect order documentation (see dll/gbe_dota_action_model.h for the
// canonical action type and cross-domain ordering invariants).
// ============================================================================
//
// GBE_HandleDotaUnlockItemStyleRequest (emsg 2571 -> 2572):
//   1. Parse: item_id (field 1), style_index (field 2), consumable_id (field 3)
//   2. If item found and style valid:
//      a. GBE_ApplyDotaUnlockStyleBitmask(item, style_index)   [pure mutation]
//      b. PushIncomingNow(emsg=22, SO Update for item)          [coordinator]
//      - If style invalid: return early (no response, no further side effects)
//   3. If consumable found:
//      a. Erase consumable from items                          [coordinator mutation]
//      b. PushIncomingNow(emsg=24, SO Destroy for consumable)   [coordinator]
//   4. PushIncomingNow(emsg=2572, response)                     [coordinator]
//   Invariant: SO Update(22) precedes SO Destroy(24) precedes Response(2572).
//
// GBE_HandleDotaSetItemStyleRequest (emsg 2577 -> 2578):
//   1. Parse: item_id (field 1), style_index (field 2)
//   2. If item found:
//      a. item.style = style_index                             [coordinator mutation]
//      b. CallbackItemUpdated(steam_id, item)                  [coordinator]
//      c. If DOTA2 + !is_server + server_gc has active lobby:
//           GBE_PushDotaPlayerEquippedItemsCacheToGC(...)      [coordinator]
//      d. SaveItemsToFile()                                     [coordinator]
//   3. PushIncomingNow(emsg=2578, response)                     [coordinator]
//   Invariant: CallbackItemUpdated precedes SaveItemsToFile precedes Response.
//   Note: response is always sent (even if item not found).
//
// GBE_HandleDotaEquipItemsRequest (emsg 2569 -> 2570 + 26):
//   1. Parse: GBE_ParseDotaEquipOps(body) -> equip_ops          [pure]
//   2. apply_equip_ops(equip_ops, items) -> modified_item_ids   [pure on items]
//   3. Generate equip_cache_version (coordinator static)
//   4. If modified:
//      PushIncomingNow(emsg=26, CMsgSOMultipleObjects)          [coordinator]
//   5. PushIncomingNow(emsg=2570, response with cache version)  [coordinator]
//   6. SaveItemsToFile()                                         [coordinator]
//   7. If DOTA2 + !is_server + modified:
//      a. If server_gc has active lobby:
//         - GBE_PushDotaPlayerEquippedItemsCacheToGC (full cache) [coordinator]
//         - For each modified item: server_gc->push_incoming_message(21) [coordinator]
//         - server_gc->push_incoming_message(26)                   [coordinator]
//      b. If equipped items exist:
//         - network->sendToAllGameservers(gameserver items msg)   [coordinator]
//   8. If DOTA2 + !is_server + lobby active + in-game + snapshot replayed:
//      GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot("equip_items_refresh") [coordinator]
//   Invariant: local response(2570) precedes server-GC forward, network
//   broadcast, and lobby snapshot refresh. Full item cache (CacheSubscribed)
//   precedes emsg 21/26 when forwarding to server GC.
// ============================================================================

namespace {

// --- Pure request parsers ---

struct UnlockStyleRequest {
    uint64 item_id{};
    uint32 style_index{255u};
    uint64 consumable_id{};
};

inline UnlockStyleRequest parse_unlock_style_request(const uint8 *body, size_t body_size)
{
    UnlockStyleRequest req;
    gbe::proto_wire::read_uint64_field(body, body_size, 1u, req.item_id);
    gbe::proto_wire::read_uint32_field(body, body_size, 2u, req.style_index);
    gbe::proto_wire::read_uint64_field(body, body_size, 3u, req.consumable_id);
    return req;
}

struct SetStyleRequest {
    uint64 item_id{};
    uint32 style_index{255u};
};

inline SetStyleRequest parse_set_style_request(const uint8 *body, size_t body_size)
{
    SetStyleRequest req;
    gbe::proto_wire::read_uint64_field(body, body_size, 1u, req.item_id);
    gbe::proto_wire::read_uint32_field(body, body_size, 2u, req.style_index);
    return req;
}

// --- Pure equip-op application ---
// Applies equip ops to a passed-in items vector. For each op:
//   - If item.id == op.item_id: set equip_state[class]=slot, optionally set style.
//   - Else: if item has matching (class, slot) equipped, remove it (swap logic).
// Returns the set of modified item IDs.

inline std::unordered_set<uint64_t> apply_equip_ops_to_items(
    const std::vector<GBE_DotaEquipOp> &ops,
    std::vector<Econ_Item> &items)
{
    std::unordered_set<uint64_t> modified;
    for (size_t ei = 0; ei < ops.size(); ++ei) {
        const auto &op = ops[ei];
        bool found_target = false;
        for (Econ_Item &item : items) {
            if (op.item_id != UINT64_MAX && op.item_id != 0 && item.id == op.item_id) {
                item.equip_states.insert_or_assign(static_cast<uint16>(op.new_class), static_cast<uint16>(op.new_slot));
                if (op.style_index != 255u)
                    item.style = static_cast<uint8>(op.style_index);
                modified.insert(item.id);
                found_target = true;
            } else {
                auto it = item.equip_states.find(static_cast<uint16>(op.new_class));
                if (it == item.equip_states.end() || it->second != static_cast<uint16>(op.new_slot))
                    continue;
                item.equip_states.erase(it);
                modified.insert(item.id);
            }
        }
    }
    return modified;
}

// --- Pure response body builder for equip (emsg 2570) ---
// Body: field 1, wire type 1 (fixed64) -> tag 0x09 + 8 bytes cache version.

inline std::string build_equip_response_body(uint64_t cache_version)
{
    std::string body;
    body.push_back(0x09);
    body.append(reinterpret_cast<const char *>(&cache_version), sizeof(cache_version));
    return body;
}

struct EquipItemsServerForwardPlan {
    bool enabled{};
    bool unsubscribe_first{};
    const char *cache_reason{};
};

struct EquipItemsPlanningContext {
    bool is_dota_client{};
    bool server_gc_has_active_lobby{};
    bool lobby_snapshot_refresh_available{};
};

struct EquipItemsPlan {
    bool parse_failed{};
    std::vector<GBE_DotaEquipOp> equip_ops;
    std::vector<Econ_Item> items_after_mutation;
    std::unordered_set<uint64_t> modified_item_ids;
    std::string response_body;
    uint64_t cache_version{};
    EquipItemsServerForwardPlan server_forward;
    bool broadcast_equipped_items{};
    const char *snapshot_refresh_reason{};
    GBE_DotaActionList actions;
};

struct EquipItemsExecutionContext {
    bool has_source_job{};
    uint64 source_job{};
    uint64 local_steam_id{};
    Steam_Game_Coordinator *server_gc{};
};

inline std::string build_equip_response_message(
    const EquipItemsPlan &plan,
    const EquipItemsExecutionContext &context)
{
    std::string response_message;
    uint32_t flagged_emsg = 2570u | GBE_kProtoMask;
    CMsgProtoBufHeader response_protohdr;
    if (context.has_source_job) {
        response_protohdr.set_job_id_target(context.source_job);
    }
    response_protohdr.set_job_id_source(18446744073709551615ULL);
    std::string serialized_protohdr = response_protohdr.SerializeAsString();
    uint32_t hdr_len = static_cast<uint32_t>(serialized_protohdr.size());

    response_message.resize(sizeof(flagged_emsg) + sizeof(hdr_len));
    memcpy(&response_message[0], &flagged_emsg, sizeof(flagged_emsg));
    memcpy(&response_message[sizeof(flagged_emsg)], &hdr_len, sizeof(hdr_len));
    response_message += serialized_protohdr;
    response_message += plan.response_body;
    return response_message;
}

template <typename PushSoUpdate,
          typename PushResponse,
          typename SaveItems,
          typename ForwardServerGC,
          typename BroadcastNetwork,
          typename RefreshSnapshot>
inline void execute_equip_items_plan(
    const EquipItemsPlan &plan,
    const EquipItemsExecutionContext &context,
    const std::string &update_message,
    const std::string &response_message,
    PushSoUpdate push_so_update,
    PushResponse push_response,
    SaveItems save_items,
    ForwardServerGC forward_server_gc,
    BroadcastNetwork broadcast_network,
    RefreshSnapshot refresh_snapshot)
{
    if (!update_message.empty()) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=2569 resp=26 source_job=%llu size=%zu note=SO cache update modified_items=%zu",
            static_cast<unsigned long long>(context.source_job),
            update_message.size(),
            plan.modified_item_ids.size()
        );
        push_so_update(update_message);
    }

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=2569 resp=2570 source_job=%llu size=%zu note=equip response count=%zu version=%llu",
        static_cast<unsigned long long>(context.source_job),
        response_message.size(),
        plan.equip_ops.size(),
        static_cast<unsigned long long>(plan.cache_version)
    );
    push_response(response_message);

    save_items();

    if (plan.server_forward.enabled) {
        forward_server_gc(context.server_gc);
        broadcast_network();
    }

    if (plan.snapshot_refresh_reason) {
        refresh_snapshot(plan.snapshot_refresh_reason);
    }
}

inline EquipItemsPlan plan_equip_items_request(
    const uint8 *body,
    size_t body_size,
    const std::vector<Econ_Item> &current_items,
    uint64_t cache_version,
    const EquipItemsPlanningContext &context)
{
    EquipItemsPlan plan;
    plan.cache_version = cache_version;
    plan.items_after_mutation = current_items;

    if (!GBE_ParseDotaEquipOps(body, body_size, plan.equip_ops)) {
        plan.parse_failed = true;
        return plan;
    }

    plan.modified_item_ids = apply_equip_ops_to_items(plan.equip_ops, plan.items_after_mutation);
    plan.response_body = build_equip_response_body(cache_version);

    if (!plan.modified_item_ids.empty()) {
        plan.actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PushIncomingNow, 26u | GBE_kProtoMask });
    }
    plan.actions.push_back(GBE_DotaAction{ GBE_DotaActionType::PushIncomingNow, 2570u | GBE_kProtoMask, plan.response_body });
    plan.actions.push_back(GBE_DotaAction{ GBE_DotaActionType::SaveItemsToFile });

    if (context.is_dota_client && !plan.modified_item_ids.empty()) {
        if (context.server_gc_has_active_lobby) {
            plan.server_forward.enabled = true;
            plan.server_forward.unsubscribe_first = true;
            plan.server_forward.cache_reason = "equip_forward_host_resubscribe_server";
            plan.actions.push_back(GBE_DotaAction{ GBE_DotaActionType::ServerGcForward, 0u, std::string(), 0u, 0u, 0u, plan.server_forward.cache_reason });
            for (size_t i = 0; i < plan.modified_item_ids.size(); ++i) {
                plan.actions.push_back(GBE_DotaAction{ GBE_DotaActionType::ServerGcForward, 21u | GBE_kProtoMask });
            }
            plan.actions.push_back(GBE_DotaAction{ GBE_DotaActionType::ServerGcForward, 26u | GBE_kProtoMask });
        }

        bool has_equipped_items = false;
        for (const auto &item : plan.items_after_mutation) {
            if (!item.equip_states.empty()) {
                has_equipped_items = true;
                break;
            }
        }
        plan.broadcast_equipped_items = has_equipped_items;
        if (plan.broadcast_equipped_items) {
            plan.actions.push_back(GBE_DotaAction{ GBE_DotaActionType::NetworkBroadcast });
        }
    }

    if (context.lobby_snapshot_refresh_available) {
        plan.snapshot_refresh_reason = "equip_items_refresh";
        plan.actions.push_back(GBE_DotaAction{ GBE_DotaActionType::LobbySnapshotRefresh, 0u, std::string(), 0u, 0u, 0u, plan.snapshot_refresh_reason });
    }

    return plan;
}

// --- Pure item lookup helpers ---

inline Econ_Item *find_item_by_id(std::vector<Econ_Item> &items, uint64 id)
{
    for (auto &item : items)
        if (item.id == id) return &item;
    return nullptr;
}

inline bool erase_item_by_id(std::vector<Econ_Item> &items, uint64 id)
{
    for (auto it = items.begin(); it != items.end(); ++it) {
        if (it->id == id) {
            items.erase(it);
            return true;
        }
    }
    return false;
}

} // anonymous namespace

void Steam_Game_Coordinator::GBE_SaveDotaItemsFromExecutor(const char *reason)
{
    GBE_GC_DebugLog("GC_DOTA_DIRECT", "persisting Dota items from executor reason=%s", reason ? reason : "");
    save_items_to_file();
}

void Steam_Game_Coordinator::GBE_ForwardDotaEquipItemsToServerGC(
    Steam_Game_Coordinator *server_gc,
    const std::unordered_set<uint64> &modified_item_ids,
    const std::string &update_message,
    uint64 cache_version,
    bool unsubscribe_first,
    const char *cache_reason)
{
    if (!server_gc)
        return;

    const uint64 player_steam64 = settings->get_local_steam_id().ConvertToUint64();
    const CSteamID player_steam_id = settings->get_local_steam_id();

    // The full CacheSubscribed must precede SO Create/Update messages.
    GBE_PushDotaPlayerEquippedItemsCacheToGC(server_gc, player_steam_id, items, unsubscribe_first, cache_reason);

    uint64_t create_version = cache_version - modified_item_ids.size();
    for (uint64_t mid : modified_item_ids) {
        for (const Econ_Item &item : items) {
            if (item.id != mid) continue;

            create_version++;
            CMsgSOSingleObject create_msg;
            auto *create_owner = create_msg.mutable_owner_soid();
            create_owner->set_type(1u);
            create_owner->set_id(player_steam64);
            create_msg.set_type_id(1);
            create_msg.set_object_data(item_to_gcprotobuf(item, player_steam_id));
            create_msg.set_version(create_version);

            std::string create_message;
            gbe::gc_message::build_dota_zero_header_payload(21u, create_msg.SerializeAsString(), create_message);
            server_gc->push_incoming_message(21u | GBE_kProtoMask, create_message);
            break;
        }
    }

    server_gc->push_incoming_message(26u | GBE_kProtoMask, update_message);

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "forwarded equip to server GC: emsg21_count=%zu emsg26_size=%zu lobby_id=%llu",
        modified_item_ids.size(),
        update_message.size(),
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id)
    );
}

void Steam_Game_Coordinator::GBE_BroadcastDotaEquippedItemsToGameServers()
{
    std::vector<const Econ_Item *> equipped_items;
    for (const auto &item : items) {
        if (!item.equip_states.empty())
            equipped_items.push_back(&item);
    }

    if (equipped_items.empty())
        return;

    auto response_msg = new GameServer_Items_Messages::InventoryResponse();
    response_msg->set_steam_api_call(0);  // 0 = unsolicited push

    for (const Econ_Item *ep : equipped_items) {
        auto new_item = response_msg->add_items();
        new_item->set_id(ep->id);
        new_item->set_def(ep->def);
        new_item->set_level(ep->level);
        new_item->set_quality(static_cast<int32>(ep->quality));
        new_item->set_inv_pos(ep->inv_pos);
        new_item->set_quantity(ep->quantity);
        new_item->set_flags(ep->flags);
        new_item->set_origin(ep->origin);
        new_item->set_original_id(ep->original_id);
        new_item->set_in_use(ep->in_use);
        new_item->set_style(ep->style);

        for (const auto &[class_id, slot_id] : ep->equip_states) {
            auto new_state = new_item->add_equip_states();
            new_state->set_class_id(class_id);
            new_state->set_slot_id(slot_id);
        }

        for (const Econ_Item_Attribute &attr : ep->attributes) {
            auto new_attr = new_item->add_attributes();
            new_attr->set_def(attr.def);
            new_attr->set_value(attr.value);
            new_attr->set_value_bytes(attr.value_bytes);
        }
    }

    auto gameserver_items_msg = new GameServer_Items_Messages();
    gameserver_items_msg->set_type(GameServer_Items_Messages::Response_Inventory);
    gameserver_items_msg->set_is_gc(true);
    gameserver_items_msg->set_allocated_inventory_response(response_msg);

    Common_Message msg{};
    msg.set_allocated_gameserver_items_messages(gameserver_items_msg);
    msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
    network->sendToAllGameservers(&msg, true);

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "broadcast equipped items to gameservers via network: steam64=%llu equipped_items=%zu",
        static_cast<unsigned long long>(settings->get_local_steam_id().ConvertToUint64()),
        equipped_items.size()
    );
}

void Steam_Game_Coordinator::GBE_RefreshDotaEquipLobbySnapshot(const char *reason)
{
    GBE_ClearDotaPrivateLobbySnapshotReplayed();
    GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot(reason);
}

// ============================================================================
// Handler implementations
// ============================================================================

bool Steam_Game_Coordinator::GBE_HandleDotaUnlockItemStyleRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job) {
    auto req = parse_unlock_style_request(body, body_size);

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "received direct 2571 UnlockItemStyle source_job=%llu item_id=0x%llx style_index=%u consumable=0x%llx body_size=%zu",
        static_cast<unsigned long long>(source_job),
        static_cast<unsigned long long>(req.item_id),
        req.style_index,
        static_cast<unsigned long long>(req.consumable_id),
        body_size
    );

    // Step 1: Update item's attr 400 (unlocked styles bitmask) BEFORE replying
    if (req.item_id != 0 && req.style_index != 255u) {
        Econ_Item *item = find_item_by_id(items, req.item_id);
        if (item) {
            if (!GBE_ApplyDotaUnlockStyleBitmask(*item, req.style_index)) {
                GBE_GC_DebugLog(
                    "GC_DOTA_DIRECT",
                    "2571 unlock: rejected invalid style_index=%u for item 0x%llx",
                    req.style_index,
                    static_cast<unsigned long long>(req.item_id)
                );
                return true;
            }
            // Push SO update immediately (emsg=22 CMsgSOSingleObject) via push_incoming_now
            {
                uint32 msg_type_so = ESOMsg::k_ESOMsg_Update | protobuf_mask;
                std::string so_message = build_protomsg_header(msg_type_so);
                CMsgSOSingleObject so_msg;
                so_msg.set_owner(settings->get_local_steam_id().ConvertToUint64());
                so_msg.set_type_id(1);
                so_msg.set_object_data(item_to_gcprotobuf(*item, settings->get_local_steam_id()));
                so_msg.AppendToString(&so_message);
                push_incoming_now(msg_type_so, so_message);
            }
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "2571 unlock: updated attr=400 for item 0x%llx style_bit=%u and pushed SO update (immediate)",
                static_cast<unsigned long long>(req.item_id),
                req.style_index
            );
        }
    }

    // Step 2: Consume the consumable item (delete from inventory + push SO Destroy immediately)
    if (req.consumable_id != 0) {
        if (erase_item_by_id(items, req.consumable_id)) {
            // Push SO Destroy immediately (emsg=24 CMsgSOSingleObject)
            uint32 msg_type_del = ESOMsg::k_ESOMsg_Destroy | protobuf_mask;
            std::string del_message = build_protomsg_header(msg_type_del);
            CMsgSOSingleObject del_msg;
            del_msg.set_owner(settings->get_local_steam_id().ConvertToUint64());
            del_msg.set_type_id(1);
            CSOEconItem del_proto_item;
            del_proto_item.set_id(req.consumable_id);
            del_msg.set_object_data(del_proto_item.SerializeAsString());
            del_msg.AppendToString(&del_message);
            push_incoming_now(msg_type_del, del_message);

            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "2571 unlock: consumed item 0x%llx (SO Destroy pushed immediate)",
                static_cast<unsigned long long>(req.consumable_id)
            );
        }
    }

    // Step 3: Build and send 2572 response
    std::string resp_body;
    gbe::gc_message::build_dota_unlock_item_style_response_body(req.item_id, req.style_index, resp_body);

    std::string response_message;
    gbe::gc_message::build_dota_job_reply_or_zero_header_payload(GBE_kDotaUnlockItemStyleResponse, has_source_job, source_job, resp_body, response_message);

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=2571 resp=2572 source_job=%llu size=%zu note=unlock style success item_id=0x%llx style_index=%u",
        static_cast<unsigned long long>(source_job),
        response_message.size(),
        static_cast<unsigned long long>(req.item_id),
        req.style_index
    );
    push_incoming_now(GBE_kDotaUnlockItemStyleResponse | GBE_kProtoMask, response_message);

    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaSetItemStyleRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job) {
    auto req = parse_set_style_request(body, body_size);

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "received direct 2577 SetItemStyle source_job=%llu item_id=0x%llx style_index=%u body_size=%zu",
        static_cast<unsigned long long>(source_job),
        static_cast<unsigned long long>(req.item_id),
        req.style_index,
        body_size
    );

    bool found = false;
    if (req.item_id != 0 && req.style_index != 255u) {
        Econ_Item *item = find_item_by_id(items, req.item_id);
        if (item) {
            item->style = static_cast<uint8>(req.style_index);
            found = true;

            // Notify callback so the client immediately sees the style change
            callback_item_updated(settings->get_local_steam_id(), *item);

            // Forward style change to server GC if active
            if (gc_profile == GC_PROFILE_DOTA2 && !is_server) {
                Steam_Client *steam_client = get_steam_client();
                Steam_Game_Coordinator *server_gc = steam_client ? steam_client->steam_gameserver_game_coordinator : nullptr;
                if (server_gc && server_gc->GBE_HasActiveServerLobby(GBE_local_lobby.lobby_id)) {
                    GBE_PushDotaPlayerEquippedItemsCacheToGC(server_gc, settings->get_local_steam_id(), items, true, "set_item_style_forward");
                }
            }

            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "applied style: item_id=0x%llx new_style=%u",
                static_cast<unsigned long long>(req.item_id),
                req.style_index
            );
        }
        if (found)
            save_items_to_file();
    }

    // Build 2578 response: field 1 varint = 0 (k_SetStyle_Succeeded)
    std::string resp_body;
    gbe::gc_message::build_dota_set_item_style_response_body(resp_body);

    std::string response_message;
    gbe::gc_message::build_dota_job_reply_or_zero_header_payload(GBE_kDotaSetItemStyleResponse, has_source_job, source_job, resp_body, response_message);

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=2577 resp=2578 source_job=%llu size=%zu found=%d style_index=%u",
        static_cast<unsigned long long>(source_job),
        response_message.size(),
        (int)found,
        req.style_index
    );
    push_incoming_now(GBE_kDotaSetItemStyleResponse | GBE_kProtoMask, response_message);
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaEquipItemsRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job) {
    // Generate a candidate cache version without committing it until parse succeeds.
    static uint64_t equip_cache_version = 0;
    uint64_t next_cache_version = equip_cache_version;
    if (next_cache_version == 0) {
        next_cache_version = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
    }
    next_cache_version++;

    Steam_Client *steam_client = nullptr;
    Steam_Game_Coordinator *server_gc = nullptr;
    const bool is_dota_client = !is_server && gc_profile == GC_PROFILE_DOTA2;
    if (is_dota_client) {
        steam_client = get_steam_client();
        server_gc = steam_client ? steam_client->steam_gameserver_game_coordinator : nullptr;
    }

    EquipItemsPlanningContext planning_context{};
    planning_context.is_dota_client = is_dota_client;
    planning_context.server_gc_has_active_lobby = server_gc && server_gc->GBE_HasActiveServerLobby(GBE_local_lobby.lobby_id);
    planning_context.lobby_snapshot_refresh_available = is_dota_client &&
        GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 &&
        GBE_local_lobby.state == 2u && GBE_local_lobby.game_state >= 2u &&
        GBE_HasReplayedDotaPrivateLobbySnapshot();

    EquipItemsPlan plan = plan_equip_items_request(
        body,
        body_size,
        items,
        next_cache_version,
        planning_context);

    if (plan.parse_failed) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "failed parsing direct 2569 EquipItems source_job=%llu body_size=%zu",
            static_cast<unsigned long long>(source_job),
            body_size
        );
        return true;
    }

    equip_cache_version = next_cache_version;
    items = plan.items_after_mutation;

    std::string update_message;
    if (!plan.modified_item_ids.empty()) {
        CMsgSOMultipleObjects update_msg;

        auto *owner = update_msg.mutable_owner_soid();
        owner->set_type(1u);
        owner->set_id(settings->get_local_steam_id().ConvertToUint64());

        for (uint64_t mid : plan.modified_item_ids) {
            for (const Econ_Item &item : items) {
                if (item.id != mid) continue;
                auto *obj = update_msg.add_objects();
                obj->set_type_id(1);
                obj->set_object_data(item_to_gcprotobuf(item, settings->get_local_steam_id()));
                break;
            }
        }

        update_msg.set_version(plan.cache_version);
        update_msg.set_service_id(1u);

        uint32_t flagged_emsg = 26u | GBE_kProtoMask;
        uint32_t hdr_len = 0;
        update_message.resize(sizeof(flagged_emsg) + sizeof(hdr_len));
        memcpy(&update_message[0], &flagged_emsg, sizeof(flagged_emsg));
        memcpy(&update_message[sizeof(flagged_emsg)], &hdr_len, sizeof(hdr_len));
        update_msg.AppendToString(&update_message);
    }

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "equip result: modified_items=%zu total_ops=%zu",
        plan.modified_item_ids.size(), plan.equip_ops.size()
    );

    EquipItemsExecutionContext execution_context{};
    execution_context.has_source_job = has_source_job;
    execution_context.source_job = source_job;
    execution_context.local_steam_id = settings->get_local_steam_id().ConvertToUint64();
    execution_context.server_gc = server_gc;

    const std::string response_message = build_equip_response_message(plan, execution_context);

    execute_equip_items_plan(
        plan,
        execution_context,
        update_message,
        response_message,
        [this](const std::string &message) {
            push_incoming_now(26u | GBE_kProtoMask, message);
        },
        [this](const std::string &message) {
            push_incoming_now(2570u | GBE_kProtoMask, message);
        },
        [this]() {
            GBE_SaveDotaItemsFromExecutor("equip_items");
        },
        [this, &plan, &update_message](Steam_Game_Coordinator *target_server_gc) {
            GBE_ForwardDotaEquipItemsToServerGC(
                target_server_gc,
                plan.modified_item_ids,
                update_message,
                plan.cache_version,
                plan.server_forward.unsubscribe_first,
                plan.server_forward.cache_reason);
        },
        [this]() {
            GBE_BroadcastDotaEquippedItemsToGameServers();
        },
        [this](const char *reason) {
            GBE_RefreshDotaEquipLobbySnapshot(reason);
        });

    return true;
}
