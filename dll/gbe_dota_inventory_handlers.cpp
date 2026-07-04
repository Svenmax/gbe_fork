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
#include "gbe_dota_request_router.h"
#include "gbe_proto_buf_header.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_custom_lobby_http.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_gc_wire.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_gc_config.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_gc_internal.h"
#include <atomic>
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cstring>
#include <sstream>
#include <iomanip>
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

bool Steam_Game_Coordinator::GBE_HandleDotaUnlockItemStyleRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job) {
    uint64 unlock_item_id = 0;
    uint32 unlock_style_index = 255u;
    uint64 consumable_item_id = 0;
    gbe::proto_wire::read_uint64_field(body, body_size, 1u, unlock_item_id);
    gbe::proto_wire::read_uint32_field(body, body_size, 2u, unlock_style_index);
    gbe::proto_wire::read_uint64_field(body, body_size, 3u, consumable_item_id);

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "received direct 2571 UnlockItemStyle source_job=%llu item_id=0x%llx style_index=%u consumable=0x%llx body_size=%zu",
        static_cast<unsigned long long>(source_job),
        static_cast<unsigned long long>(unlock_item_id),
        unlock_style_index,
        static_cast<unsigned long long>(consumable_item_id),
        body_size
    );

    // Step 1: Update item's attr 400 (unlocked styles bitmask) BEFORE replying
    if (unlock_item_id != 0 && unlock_style_index != 255u) {
        for (Econ_Item &item : items) {
            if (item.id == unlock_item_id) {
                if (!GBE_ApplyDotaUnlockStyleBitmask(item, unlock_style_index)) {
                    GBE_GC_DebugLog(
                        "GC_DOTA_DIRECT",
                        "2571 unlock: rejected invalid style_index=%u for item 0x%llx",
                        unlock_style_index,
                        static_cast<unsigned long long>(unlock_item_id)
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
                    so_msg.set_object_data(item_to_gcprotobuf(item, settings->get_local_steam_id()));
                    so_msg.AppendToString(&so_message);
                    push_incoming_now(msg_type_so, so_message);
                }
                GBE_GC_DebugLog(
                    "GC_DOTA_DIRECT",
                    "2571 unlock: updated attr=400 for item 0x%llx style_bit=%u and pushed SO update (immediate)",
                    static_cast<unsigned long long>(unlock_item_id),
                    unlock_style_index
                );
                break;
            }
        }
    }

    // Step 2: Consume the consumable item (delete from inventory + push SO Destroy immediately)
    if (consumable_item_id != 0) {
        for (auto it = items.begin(); it != items.end(); ++it) {
            if (it->id == consumable_item_id) {
                items.erase(it);
                // Push SO Destroy immediately (emsg=24 CMsgSOSingleObject)
                {
                    uint32 msg_type_del = ESOMsg::k_ESOMsg_Destroy | protobuf_mask;
                    std::string del_message = build_protomsg_header(msg_type_del);
                    CMsgSOSingleObject del_msg;
                    del_msg.set_owner(settings->get_local_steam_id().ConvertToUint64());
                    del_msg.set_type_id(1);
                    CSOEconItem del_proto_item;
                    del_proto_item.set_id(consumable_item_id);
                    del_msg.set_object_data(del_proto_item.SerializeAsString());
                    del_msg.AppendToString(&del_message);
                    push_incoming_now(msg_type_del, del_message);
                }
                GBE_GC_DebugLog(
                    "GC_DOTA_DIRECT",
                    "2571 unlock: consumed item 0x%llx (SO Destroy pushed immediate)",
                    static_cast<unsigned long long>(consumable_item_id)
                );
                break;
            }
        }
    }

    // Step 3: Build and send 2572 response
    std::string resp_body;
    gbe::gc_message::build_dota_unlock_item_style_response_body(unlock_item_id, unlock_style_index, resp_body);

    std::string response_message;
    gbe::gc_message::build_dota_job_reply_or_zero_header_payload(GBE_kDotaUnlockItemStyleResponse, has_source_job, source_job, resp_body, response_message);

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=2571 resp=2572 source_job=%llu size=%zu note=unlock style success item_id=0x%llx style_index=%u",
        static_cast<unsigned long long>(source_job),
        response_message.size(),
        static_cast<unsigned long long>(unlock_item_id),
        unlock_style_index
    );
    push_incoming_now(GBE_kDotaUnlockItemStyleResponse | GBE_kProtoMask, response_message);

    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaSetItemStyleRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job) {
    uint64 style_item_id = 0;
    uint32 style_index = 255u;
    gbe::proto_wire::read_uint64_field(body, body_size, 1u, style_item_id);
    gbe::proto_wire::read_uint32_field(body, body_size, 2u, style_index);

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "received direct 2577 SetItemStyle source_job=%llu item_id=0x%llx style_index=%u body_size=%zu",
        static_cast<unsigned long long>(source_job),
        static_cast<unsigned long long>(style_item_id),
        style_index,
        body_size
    );

    bool found = false;
    if (style_item_id != 0 && style_index != 255u) {
        for (Econ_Item &item : items) {
            if (item.id != style_item_id)
                continue;
            item.style = static_cast<uint8>(style_index);
            found = true;

            // Push SO update (emsg=21) so the client immediately sees the style change
            callback_item_updated(settings->get_local_steam_id(), item);

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
                static_cast<unsigned long long>(style_item_id),
                style_index
            );
            break;
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
        style_index
    );
    push_incoming_now(GBE_kDotaSetItemStyleResponse | GBE_kProtoMask, response_message);
    return true;
}


bool Steam_Game_Coordinator::GBE_HandleDotaEquipItemsRequest(const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job) {
    // Parse the repeated equips (field 1, length-delimited sub-messages)
    std::vector<GBE_DotaEquipOp> equip_ops;
    if (!GBE_ParseDotaEquipOps(body, body_size, equip_ops)) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "failed parsing direct 2569 EquipItems source_job=%llu body_size=%zu",
            static_cast<unsigned long long>(source_job),
            body_size
        );
        return true;
    }

    // Apply equip logic and track which items were modified
    std::unordered_set<uint64_t> modified_item_ids;
    for (size_t ei = 0; ei < equip_ops.size(); ei++) {
        const auto &op = equip_ops[ei];
        bool found_target = false;
        for (Econ_Item &item : items) {
            if (op.item_id != UINT64_MAX && op.item_id != 0 && item.id == op.item_id) {
                item.equip_states.insert_or_assign(static_cast<uint16>(op.new_class), static_cast<uint16>(op.new_slot));
                if (op.style_index != 255u)
                    item.style = static_cast<uint8>(op.style_index);
                modified_item_ids.insert(item.id);
                found_target = true;
            } else {
                auto it = item.equip_states.find(static_cast<uint16>(op.new_class));
                if (it == item.equip_states.end() || it->second != static_cast<uint16>(op.new_slot))
                    continue;
                item.equip_states.erase(it);
                modified_item_ids.insert(item.id);
            }
        }
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "equip op[%zu]: item_id=0x%llx (%llu) new_class=%u new_slot=%u style_index=%u found=%d items_count=%zu",
            ei,
            static_cast<unsigned long long>(op.item_id),
            static_cast<unsigned long long>(op.item_id),
            op.new_class, op.new_slot,
            op.style_index,
            (int)found_target,
            items.size()
        );
    }
    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "equip result: modified_items=%zu total_ops=%zu",
        modified_item_ids.size(), equip_ops.size()
    );

    // Generate a cache version (monotonically increasing timestamp-based)
    static uint64_t equip_cache_version = 0;
    if (equip_cache_version == 0) {
        equip_cache_version = static_cast<uint64_t>(
            std::chrono::duration_cast<std::chrono::microseconds>(
                std::chrono::system_clock::now().time_since_epoch()
            ).count()
        );
    }
    equip_cache_version++;

    // Build msg 26 (k_ESOMsg_UpdateMultiple = CMsgSOMultipleObjects)
    // Contains: objects_modified (field 2), version (field 3), owner_soid (field 6), service_id (field 7)
    std::string update_message;
    if (!modified_item_ids.empty()) {
        CMsgSOMultipleObjects update_msg;

        // owner_soid: type=1, id=steam_id
        auto *owner = update_msg.mutable_owner_soid();
        owner->set_type(1u);
        owner->set_id(settings->get_local_steam_id().ConvertToUint64());

        // Add modified items (field 2 = repeated SingleObject objects)
        for (uint64_t mid : modified_item_ids) {
            for (const Econ_Item &item : items) {
                if (item.id != mid) continue;
                auto *obj = update_msg.add_objects();
                obj->set_type_id(1);
                obj->set_object_data(item_to_gcprotobuf(item, settings->get_local_steam_id()));
                break;
            }
        }

        update_msg.set_version(equip_cache_version);
        update_msg.set_service_id(1u);

        // Serialize into GC message format: emsg(4) + proto_hdr_len(4) + proto_hdr + body
        {
            uint32_t flagged_emsg = 26u | GBE_kProtoMask;
            // Empty proto header (matching official capture)
            uint32_t hdr_len = 0;
            update_message.resize(sizeof(flagged_emsg) + sizeof(hdr_len));
            memcpy(&update_message[0], &flagged_emsg, sizeof(flagged_emsg));
            memcpy(&update_message[sizeof(flagged_emsg)], &hdr_len, sizeof(hdr_len));
            update_msg.AppendToString(&update_message);
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=2569 resp=26 source_job=%llu size=%zu note=SO cache update modified_items=%zu",
            static_cast<unsigned long long>(source_job),
            update_message.size(),
            modified_item_ids.size()
        );
        push_incoming_now(26u | GBE_kProtoMask, update_message);
    }

    // Build msg 2570 (CMsgClientToGCEquipItemsResponse) with so_cache_version_id
    {
        std::string resp_body;
        // field 1, wire type 1 (fixed64) -> tag = 0x09
        resp_body.push_back(0x09);
        resp_body.append(reinterpret_cast<const char*>(&equip_cache_version), 8);

        std::string response_message;
        uint32_t flagged_emsg = 2570u | GBE_kProtoMask;
        CMsgProtoBufHeader response_protohdr;
        if (has_source_job) {
            response_protohdr.set_job_id_target(source_job);
        }
        response_protohdr.set_job_id_source(18446744073709551615ULL);
        std::string serialized_protohdr = response_protohdr.SerializeAsString();
        uint32_t hdr_len = static_cast<uint32_t>(serialized_protohdr.size());

        response_message.resize(sizeof(flagged_emsg) + sizeof(hdr_len));
        memcpy(&response_message[0], &flagged_emsg, sizeof(flagged_emsg));
        memcpy(&response_message[sizeof(flagged_emsg)], &hdr_len, sizeof(hdr_len));
        response_message += serialized_protohdr;
        response_message += resp_body;

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=2569 resp=2570 source_job=%llu size=%zu note=equip response count=%zu version=%llu",
            static_cast<unsigned long long>(source_job),
            response_message.size(),
            equip_ops.size(),
            static_cast<unsigned long long>(equip_cache_version)
        );
        push_incoming_now(2570u | GBE_kProtoMask, response_message);
    }

    save_items_to_file();

    // Forward item equip changes to the server GC so the dedicated server
    // can update wearables in real-time (e.g. during strategy phase).
    //
    // [FIX] The server GC's settings->get_local_steam_id() returns the
    // game-server steam ID, not the lobby owner's personal steam ID.  At
    // 7450 time (batch player resources) the server therefore fails to
    // recognise the host as "local" and falls through to all_user_items,
    // which may have no equip_states yet.  Even when equip_states arrive
    // later via network inventory response, the Source 2 engine only
    // creates wearable entities from a CacheSubscribed (emsg=24) that
    // establishes the player's SO cache.  Bare emsg=21/26 arriving
    // *before* any CacheSubscribed for that owner are silently dropped by
    // the engine because no SO cache exists for the owner yet.
    //
    // Fix: before pushing emsg=21/26, always push a full player-item
    // CacheSubscribed (emsg=24, owner_type=1) containing ALL currently
    // equipped items.  This guarantees the engine has a valid SO cache
    // for the player before the individual Create/Update messages arrive,
    // and – crucially – provides the engine with the loadout data it
    // needs to build wearable entities at hero-spawn time.
    if (!is_server && gc_profile == GC_PROFILE_DOTA2 && !update_message.empty()) {
        Steam_Client *steam_client = get_steam_client();
        Steam_Game_Coordinator *server_gc = steam_client ? steam_client->steam_gameserver_game_coordinator : nullptr;
        if (server_gc && server_gc->GBE_HasActiveServerLobby(GBE_local_lobby.lobby_id)) {
            const uint64 player_steam64 = settings->get_local_steam_id().ConvertToUint64();
            const CSteamID player_steam_id = settings->get_local_steam_id();

            // Push to server GC so remote players see host cosmetics
            GBE_PushDotaPlayerEquippedItemsCacheToGC(server_gc, player_steam_id, items, true, "equip_forward_host_resubscribe_server");

            // Step 1: Send emsg=21 (k_ESOMsg_Create) for each modified item
            uint64_t create_version = equip_cache_version - modified_item_ids.size();
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

            // Step 2: Send emsg=26 (CMsgSOMultipleObjects) with all modified items
            server_gc->push_incoming_message(26u | GBE_kProtoMask, update_message);

            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "forwarded equip to server GC: emsg21_count=%zu emsg26_size=%zu lobby_id=%llu",
                modified_item_ids.size(),
                update_message.size(),
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id)
            );
        }

        // Also broadcast equipped items via network so the remote server GC
        // (on the host machine in LAN mode) can build CacheSubscribed for us.
        // This handles the case where we are a remote player and the server GC
        // is in a different process.
        {
            std::vector<const Econ_Item *> equipped_items;
            for (const auto &item : items) {
                if (!item.equip_states.empty())
                    equipped_items.push_back(&item);
            }

            if (!equipped_items.empty()) {
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
        }
    }

    // [FIX] When the local player (host) equips items during an active game,
    // the lobby snapshot that was previously replayed is now stale.  Reset the
    // flag so the next GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot call will
    // rebuild and push a fresh Lobby CacheSubscribed (emsg=24) containing the
    // updated equipped items.  Without this, the host's game client never
    // receives the updated wearable data for its own hero.
    if (!is_server && gc_profile == GC_PROFILE_DOTA2 &&
        GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 &&
        GBE_local_lobby.state == 2u && GBE_local_lobby.game_state >= 2u &&
        GBE_dota_private_lobby_snapshot_replayed) {
        GBE_dota_private_lobby_snapshot_replayed = false;
        GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot("equip_items_refresh");
    }

    return true;
}
