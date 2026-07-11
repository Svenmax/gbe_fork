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
#include "gbe_dota_payload_item_helpers.h"
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
#include <steammessages.pb.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/econ_gcmessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>
#include <tf2/gcsystemmsgs.pb.h>
#include <tf2/tf_gcmessages.pb.h>

using namespace gamecoordinator::tf2;

// --- List X: inventory/item-only static symbols (moved from steam_game_coordinator.cpp) ---

static void ser_varstring(std::string &buf, const std::string &input)
{
    uint16 len = static_cast<uint16>(input.size());
    if (len != 0) {
        ser_var<uint16>(buf, len + 1);
        buf.append(input + '\0');
    } else {
        ser_var<uint16>(buf, 0);
    }
}


// --- Inventory/item member functions (moved from steam_game_coordinator.cpp) ---

uint64 Steam_Game_Coordinator::item_id_local_to_network(uint64 item_id)
{
    if (item_id == 0)
        return 0;

    // Add SteamID to item ID to avoid ID collisions in multiplayer games.
    uint32 account_id = settings->get_local_steam_id().GetAccountID();

    if (settings->use_32bit_inventory_item_ids) {
        // 32-bit mode
        item_id <<= 20ull;
        item_id |= static_cast<uint64>(account_id) & 0x000FFFFFull;
    } else {
        // 64-bit mode
        item_id <<= 32ull;
        item_id |= static_cast<uint64>(account_id);
    }

    return item_id;
}


uint64 Steam_Game_Coordinator::item_id_network_to_local(uint64 item_id)
{
    if (settings->use_32bit_inventory_item_ids) {
        // 32-bit mode
        item_id >>= 20ull;
    } else {
        // 64-bit mode
        item_id >>= 32ull;
    }

    return item_id;
}


std::string Steam_Game_Coordinator::item_to_gcstruct(const Econ_Item &item, CSteamID steam_id)
{
    std::string message;

    ser_var<uint64>(message, item.id);
    ser_var<uint32>(message, steam_id.GetAccountID());
    ser_var<uint16>(message, item.def);
    ser_var<uint8>(message, item.level);
    ser_var<uint8>(message, item.quality);
    ser_var<uint32>(message, item.inv_pos);
    ser_var<uint32>(message, item.quantity);

    if (gc_version >= 20100428) {
        // Strings are passed as UTF-8 which is good for us since we can just copy std::string as is.
        ser_varstring(message, item.custom_name);

        if (gc_version >= 20100930) {
            ser_var<uint8>(message, item.flags);

            if (gc_version >= 20101027) {
                ser_var<uint8>(message, item.origin);
                ser_varstring(message, item.custom_desc);
                ser_var<bool>(message, item.in_use);
            }
        }
    }

    ser_var<uint16>(message, static_cast<uint16>(item.attributes.size()));

    for (const Econ_Item_Attribute &attr : item.attributes) {
        ser_var<uint16>(message, attr.def);
        ser_var<float>(message, attr.value);
    }

    if (gc_version >= 20101217) {
        ser_var<uint64>(message, item.original_id);
    }

    return message;
}


std::string Steam_Game_Coordinator::item_to_gcprotobuf(const Econ_Item &item, CSteamID steam_id)
{
    return GBE_SerializeEconItemToGcprotobuf(item, steam_id, gc_version, is_portal2);
}


void Steam_Game_Coordinator::handle_set_item_pos(const void *input, uint32 input_size)
{
    if (is_server || input_size < 30)
        return;

    const char *p = reinterpret_cast<const char *>(input);
    GCMsgHdrEx_t hdr = parse_msg_header(p);
    uint64 item_id = deser_var<uint64>(p);
    uint32 inv_pos = deser_var<uint32>(p);
    PRINT_DEBUG("%llu %u", item_id, inv_pos);

    if (const Econ_Item *item = set_item_pos(item_id, inv_pos, true)) {
        callback_item_updated(settings->get_local_steam_id(), *item);
    }
}


void Steam_Game_Coordinator::handle_delete_item(const void *input, uint32 input_size)
{
    if (is_server || input_size < 26)
        return;

    const char *p = reinterpret_cast<const char *>(input);
    GCMsgHdrEx_t hdr = parse_msg_header(p);
    uint64 item_id = deser_var<uint64>(p);
    PRINT_DEBUG("%llu", item_id);

    if (delete_item(item_id, true)) {
        callback_item_deleted(settings->get_local_steam_id(), item_id);
    }
}


void Steam_Game_Coordinator::handle_set_item_style(const void *input, uint32 input_size)
{
    if (is_server || input_size < 27)
        return;

    const char *p = reinterpret_cast<const char *>(input);
    GCMsgHdrEx_t hdr = parse_msg_header(p);
    uint64 item_id = deser_var<uint64>(p);
    uint8 style = deser_var<uint8>(p);
    PRINT_DEBUG("%llu %u", item_id, style);

    for (Econ_Item &item : items) {
        if (item.id != item_id)
            continue;

        item.style = style;
        save_items_to_file();

        // Let the others know, too.
        auto inventory_msg = new GameServer_Items_Messages::ItemUpdate();
        inventory_msg->set_id(item_id);
        inventory_msg->set_style(style);

        auto gameserver_items_msg = new GameServer_Items_Messages();
        gameserver_items_msg->set_type(GameServer_Items_Messages::Request_UpdateItem);
        gameserver_items_msg->set_is_gc(true);
        gameserver_items_msg->set_allocated_item_update(inventory_msg);

        Common_Message msg{};
        msg.set_allocated_gameserver_items_messages(gameserver_items_msg);
        msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
        network->sendToAll(&msg, true);

        callback_item_updated(settings->get_local_steam_id(), item);
        break;
    }
}


void Steam_Game_Coordinator::handle_adjust_equip_state(const void *input, uint32 input_size)
{
    if (is_server)
        return;

    auto [hdr, protohdr, protomsg, success] = parse_protomsg<CMsgAdjustItemEquippedState>(input, input_size);
    if (!success)
        return;

    uint64 item_id = protomsg.item_id();
    uint32 new_class_id = protomsg.new_class();
    uint32 new_slot_id = protomsg.new_slot();
    PRINT_DEBUG("%llu %u %u", item_id, new_class_id, new_slot_id);

    for (Econ_Item &item : items) {
        if (item_id != UINT64_MAX && item.id == item_id) {
            // Equip the item into this slot.
            item.equip_states.insert_or_assign(new_class_id, new_slot_id);
        } else {
            // Unequip whatever else we had in this slot.
            auto it = item.equip_states.find(new_class_id);
            if (it == item.equip_states.end() || it->second != new_slot_id)
                continue;

            item.equip_states.erase(it);
        }

        // Let the others know, too.
        auto inventory_msg = new GameServer_Items_Messages::ItemUpdate();
        inventory_msg->set_id(item.id);
        inventory_msg->set_has_equip_states(true);
        for (const auto &[class_id, slot_id] : item.equip_states) {
            auto new_state = inventory_msg->add_equip_states();
            new_state->set_class_id(class_id);
            new_state->set_slot_id(slot_id);
        }

        auto gameserver_items_msg = new GameServer_Items_Messages();
        gameserver_items_msg->set_type(GameServer_Items_Messages::Request_UpdateItem);
        gameserver_items_msg->set_is_gc(true);
        gameserver_items_msg->set_allocated_item_update(inventory_msg);

        Common_Message msg{};
        msg.set_allocated_gameserver_items_messages(gameserver_items_msg);
        msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
        network->sendToAll(&msg, true);

        callback_item_updated(settings->get_local_steam_id(), item);
    }

    save_items_to_file();
}


void Steam_Game_Coordinator::handle_set_multiple_item_pos(const void *input, uint32 input_size)
{
    if (is_server)
        return;

    auto [hdr, protohdr, protomsg, success] = parse_protomsg<CMsgSetItemPositions>(input, input_size);
    if (!success)
        return;

    for (auto &entry : protomsg.item_positions()) {
        uint64 item_id = entry.item_id();
        uint32 inv_pos = entry.position();

        if (const Econ_Item *item = set_item_pos(item_id, inv_pos, true, false)) {
            callback_item_updated(settings->get_local_steam_id(), *item);
        }
    }

    save_items_to_file();
}


void Steam_Game_Coordinator::callback_items_received(CSteamID steam_id, const std::vector<Econ_Item> &items)
{
    if (!gc_initialized)
        return;

    if (is_server && gc_profile == GC_PROFILE_DOTA2) {
        GBE_RestoreSharedDotaLobbyState("callback_items_received");

        if (GBE_local_lobby.active &&
            GBE_local_lobby.lobby_id != 0 &&
            steam_id.BIndividualAccount() &&
            steam_id.ConvertToUint64() == GBE_GetDotaLobbyOwnerSteamId()) {
            // Skip the full generic CacheSubscribed (too large, e.g. 27k items / 706KB)
            // but still push equipped-only items so the server engine creates wearables.
            // Push to server GC for remote players
            if (GBE_PushDotaPlayerEquippedItemsCacheToGC(this, steam_id, items, false, "callback_items_received_owner_skip_server")) {
                size_t equipped_count = 0;
                for (const Econ_Item &item : items) {
                    if (!item.equip_states.empty())
                        ++equipped_count;
                }
                GBE_GC_DebugLog(
                    "GC_DOTA_SYNC",
                    "pushed owner equipped-only CacheSubscribed for server (skipped full generic): steam_id=%llu lobby_id=%llu state=%u game_state=%u equipped=%zu total=%zu",
                    static_cast<unsigned long long>(steam_id.ConvertToUint64()),
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state,
                    equipped_count,
                    items.size()
                );
            } else {
                GBE_GC_DebugLog(
                    "GC_DOTA_SYNC",
                    "skipping generic CacheSubscribed for active dota owner (no equipped items): steam_id=%llu lobby_id=%llu state=%u game_state=%u items=%zu",
                    static_cast<unsigned long long>(steam_id.ConvertToUint64()),
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state,
                    items.size()
                );
            }
            return;
        }

        // [FIX] In a listen server, never push the host's full generic
        // CacheSubscribed (706KB / 27k items) to the server engine.  The
        // shared SO cache already has these items from the login
        // CacheSubscribed.  Pushing them again with no equipped_state
        // pollutes the server engine's item cache and prevents the later
        // equip-forward CacheSubscribed (with equipped items) from creating
        // wearable entities.  Just skip entirely -- the equip-forward path
        // will push the correct equipped-only CacheSubscribed when needed.
        //
        // Detection: check if steam_id matches client GC's local ID, OR if
        // the item count matches the client GC's full inventory (covers the
        // case where source_id is the game-server steam ID instead of the
        // player's personal ID).
        Steam_Client *steam_client = get_steam_client();
        Steam_Game_Coordinator *client_gc = steam_client ? steam_client->steam_game_coordinator : nullptr;
        if (client_gc) {
            const bool is_host_by_id = (steam_id.ConvertToUint64() == client_gc->settings->get_local_steam_id().ConvertToUint64());
            const bool is_host_by_inventory = (!items.empty() && items.size() == client_gc->get_items().size());
            if (is_host_by_id || is_host_by_inventory) {
                GBE_GC_DebugLog(
                    "GC_DOTA_SYNC",
                    "skipping generic CacheSubscribed for host (fallback): steam_id=%llu lobby_active=%d lobby_id=%llu items=%zu match_by_id=%d match_by_inv=%d",
                    static_cast<unsigned long long>(steam_id.ConvertToUint64()),
                    GBE_local_lobby.active ? 1 : 0,
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    items.size(),
                    is_host_by_id ? 1 : 0,
                    is_host_by_inventory ? 1 : 0
                );
                return;
            }
        }
    }

    if (gc_version < 20110414) {
        uint32 msg_type = ESOMsg::k_ESOMsg_CacheSubscribed;
        std::string message = build_msg_header();

        uint64 owner_id = steam_id.ConvertToUint64();
        uint16 num_types = 1;

        ser_var<uint64>(message, owner_id);
        ser_var<uint16>(message, num_types);

        // econ items (1)
        uint32 object_type = 1;
        uint16 num_objects = static_cast<uint16>(items.size());

        ser_var<uint32>(message, object_type);
        ser_var<uint16>(message, num_objects);

        for (const Econ_Item &item : items) {
            message.append(item_to_gcstruct(item, steam_id));
        }

        push_incoming(msg_type, message);
    } else {
        uint32 msg_type = ESOMsg::k_ESOMsg_CacheSubscribed | protobuf_mask;
        std::string message = build_protomsg_header(msg_type);

        CMsgSOCacheSubscribed protomsg;
        protomsg.set_owner(steam_id.ConvertToUint64());
        auto objects = protomsg.add_objects();
        objects->set_type_id(1);

        for (const Econ_Item &item : items) {
            objects->add_object_data(item_to_gcprotobuf(item, steam_id));
        }

        protomsg.AppendToString(&message);
        push_incoming(msg_type, message);
    }
}


void Steam_Game_Coordinator::GBE_MirrorDotaEquippedItemsForUser(
    CSteamID steam_id,
    const std::vector<Econ_Item> &source_items,
    const char *reason)
{
    if (!is_server || gc_profile != GC_PROFILE_DOTA2 || !steam_id.BIndividualAccount())
        return;

    std::uint32_t hero_id = 0u;
    const std::uint64_t steam64 = steam_id.ConvertToUint64();
    if (steam64 == GBE_local_lobby.owner_steam_id) {
        hero_id = GBE_local_lobby.owner_hero_id;
    } else {
        for (const GBE_DotaLobbyMemberState &member : GBE_local_lobby.members) {
            if (member.steam_id == steam64) {
                hero_id = member.hero_id;
                break;
            }
        }
    }

    std::vector<Econ_Item> mirrored_items;
    mirrored_items.reserve(source_items.size());
    std::size_t equip_state_count = 0u;
    std::size_t hero_match_count = 0u;
    for (const Econ_Item &item : source_items) {
        if (item.equip_states.empty())
            continue;

        mirrored_items.push_back(item);
        equip_state_count += item.equip_states.size();
        for (const auto &[class_id, slot_id] : item.equip_states) {
            (void)slot_id;
            if (hero_id != 0u && class_id == hero_id)
                ++hero_match_count;
        }
    }

    all_user_items[steam_id] = mirrored_items;

    GBE_GC_DebugLog(
        "GC_DOTA_INVENTORY_MIRROR",
        "mirrored equipped inventory into server GC: steam64=%llu hero_id=%u source_items=%zu equipped_items=%zu equip_states=%zu hero_matches=%zu reason=%s",
        static_cast<unsigned long long>(steam64),
        hero_id,
        source_items.size(),
        mirrored_items.size(),
        equip_state_count,
        hero_match_count,
        reason ? reason : "unknown"
    );
}


void Steam_Game_Coordinator::callback_items_removed(CSteamID steam_id)
{
    if (!gc_initialized)
        return;

    if (gc_profile == GC_PROFILE_DOTA2 &&
        GBE_local_lobby.active &&
        GBE_local_lobby.lobby_id != 0 &&
        GBE_local_lobby.state < 3u &&
        steam_id.BIndividualAccount() &&
        steam_id.ConvertToUint64() == GBE_GetDotaLobbyOwnerSteamId()) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipping generic CacheUnsubscribed for active dota owner steam_id=%llu lobby_id=%llu state=%u game_state=%u launch_phase=%s is_server=%u",
            static_cast<unsigned long long>(steam_id.ConvertToUint64()),
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase),
            is_server ? 1u : 0u
        );
        return;
    }

    if (gc_version < 20110414) {
        uint32 msg_type = ESOMsg::k_ESOMsg_CacheUnsubscribed;
        std::string message = build_msg_header();
        ser_var<uint64>(message, steam_id.ConvertToUint64());

        push_incoming(msg_type, message);
    } else {
        uint32 msg_type = ESOMsg::k_ESOMsg_CacheUnsubscribed | protobuf_mask;
        std::string message = build_protomsg_header(msg_type);

        CMsgSOCacheUnsubscribed protomsg;
        protomsg.set_owner(steam_id.ConvertToUint64());

        protomsg.AppendToString(&message);
        push_incoming(msg_type, message);
    }
}


void Steam_Game_Coordinator::callback_item_updated(CSteamID steam_id, const Econ_Item &item)
{
    if (!gc_initialized)
        return;

    if (gc_version < 20110414) {
        uint32 msg_type = ESOMsg::k_ESOMsg_Update;
        std::string message = build_msg_header();

        uint64 owner_id = steam_id.ConvertToUint64();
        uint32 object_type = 1;
        uint8 num_fields = 1;

        ser_var<uint64>(message, owner_id);
        ser_var<uint32>(message, object_type);
        ser_var<uint64>(message, item.id);
        ser_var<uint8>(message, num_fields);

        uint8 field_idx = 5;
        ser_var<uint8>(message, field_idx);
        ser_var<uint32>(message, item.inv_pos);

        if (gc_version >= 20101027) {
            ser_var<bool>(message, item.in_use);
        }

        push_incoming(msg_type, message);
    } else {
        uint32 msg_type = ESOMsg::k_ESOMsg_Update | protobuf_mask;
        std::string message = build_protomsg_header(msg_type);

        CMsgSOSingleObject protomsg;
        protomsg.set_owner(steam_id.ConvertToUint64());
        protomsg.set_type_id(1);
        protomsg.set_object_data(item_to_gcprotobuf(item, steam_id));

        protomsg.AppendToString(&message);
        push_incoming(msg_type, message);
    }
}


void Steam_Game_Coordinator::callback_item_deleted(CSteamID steam_id, uint64 item_id)
{
    if (!gc_initialized)
        return;

    if (gc_version < 20110414) {
        uint32 msg_type = ESOMsg::k_ESOMsg_Destroy;
        std::string message = build_msg_header();

        uint64 owner_id = steam_id.ConvertToUint64();
        uint32 object_type = 1;

        ser_var<uint64>(message, owner_id);
        ser_var<uint32>(message, object_type);
        ser_var<uint64>(message, item_id);

        push_incoming(msg_type, message);
    } else {
        uint32 msg_type = ESOMsg::k_ESOMsg_Destroy | protobuf_mask;
        std::string message = build_protomsg_header(msg_type);

        CMsgSOSingleObject protomsg;
        protomsg.set_owner(steam_id.ConvertToUint64());
        protomsg.set_type_id(1);

        CSOEconItem proto_item;
        proto_item.set_id(item_id);
        protomsg.set_object_data(proto_item.SerializeAsString());

        protomsg.AppendToString(&message);
        push_incoming(msg_type, message);
    }
}


const std::vector<Econ_Item> &Steam_Game_Coordinator::load_items_from_file()
{
    if (items_loaded)
        return items;

    items_loaded = true;

    nlohmann::json items_json;
    if (!local_storage->load_json_file("", items_user_file, items_json))
        return items;

    for (auto it = items_json.begin(); it != items_json.end(); it++) {
        Econ_Item new_item{};
        try {
            new_item.id = std::stoull(it.key());
        } catch (...) {
            continue;
        }
        if (new_item.id == 0)
            continue;

        try {
            new_item.def = it->value("definition", 0u); // 0 is a valid item definition
            new_item.level = it->value("level", 1u);
            new_item.quality = static_cast<EItemQuality>(it->value("quality", 0));
            new_item.inv_pos = it->value("inventory_pos", 0u);
            new_item.quantity = it->value("quantity", 1u);
            new_item.flags = it->value("flags", 0u);
            new_item.origin = it->value("origin", 0u);
            new_item.custom_name = it->value("custom_name", std::string());
            new_item.custom_desc = it->value("custom_desc", std::string());
            new_item.original_id = it->value("original_id", 0ull);
            new_item.style = it->value("style", 0u);
            new_item.in_use = false;

            if (it->contains("equip_states")) {
                for (const auto &equip : it->at("equip_states")) {
                    uint32 class_id = equip.value("class", 0u);
                    uint32 slot_id = equip.value("slot", 0u);

                    new_item.equip_states.insert({ class_id, slot_id });
                }
            }

            if (it->contains("attributes")) {
                for (const auto &attr : it->at("attributes")) {
                    Econ_Item_Attribute new_attr{};
                    new_attr.def = attr.value("definition", 0u);
                    if (new_attr.def == 0) // 0 is not a valid attribute definition, however
                        continue;

                    if (attr.contains("value")) {
                        new_attr.type = Econ_Item_Attribute::ATTR_TYPE_DEFAULT;
                        float value = attr.value("value", 0.0f);
                        ser_var<float>(new_attr.value_bytes, value);
                        new_attr.value = value;
                    } else if (attr.contains("value_float")) {
                        new_attr.type = Econ_Item_Attribute::ATTR_TYPE_FLOAT;
                        float value = attr.value("value_float", 0.0f);
                        ser_var<float>(new_attr.value_bytes, value);
                        new_attr.value = value;
                    } else if (attr.contains("value_int")) {
                        new_attr.type = Econ_Item_Attribute::ATTR_TYPE_INT;
                        uint32 value = attr.value("value_int", 0u);
                        ser_var<uint32>(new_attr.value_bytes, value);
                        memcpy(&new_attr.value, &value, sizeof(float));
                    } else if (attr.contains("value_string")) {
                        new_attr.type = Econ_Item_Attribute::ATTR_TYPE_STRING;
                        std::string value = attr.value("value_string", std::string());
                        new_attr.value_bytes = value + '\0';
                        new_attr.value = 0.0f;
                    } else if (attr.contains("value_bytes_hex")) {
                        new_attr.type = Econ_Item_Attribute::ATTR_TYPE_STRING;
                        std::string hex = attr.value("value_bytes_hex", std::string());
                        if (!gbe::proto_wire::decode_hex_string(hex.c_str(), new_attr.value_bytes))
                            continue;
                        new_attr.value = 0.0f;
                    } else {
                        continue;
                    }

                    new_item.attributes.push_back(new_attr);
                }
            }
        } catch (std::exception &e) {
            const char *errorMessage = e.what();
            PRINT_DEBUG("error parsing item %llu: %s", new_item.id, errorMessage);
            continue;
        }

        new_item.id = item_id_local_to_network(new_item.id);
        new_item.original_id = item_id_local_to_network(new_item.original_id);

        // Check custom name and custom description limits.
        if (!check_econ_item_name(new_item.custom_name)) {
            new_item.custom_name.clear();
        }

        if (!check_econ_item_desc(new_item.custom_desc)) {
            new_item.custom_desc.clear();
        }

        items.push_back(new_item);
    }

    return items;
}


void Steam_Game_Coordinator::save_items_to_file()
{
    nlohmann::json items_json;

    for (const Econ_Item &item : items) {
        uint64 item_id = item_id_network_to_local(item.id);

        nlohmann::json json_item;
        json_item["definition"] = item.def;
        json_item["level"] = item.level;
        json_item["quality"] = item.quality;
        json_item["inventory_pos"] = item.inv_pos;
        json_item["quantity"] = item.quantity;
        json_item["flags"] = item.flags;
        json_item["origin"] = item.origin;
        json_item["custom_name"] = item.custom_name;
        json_item["custom_desc"] = item.custom_desc;
        json_item["original_id"] = item_id_network_to_local(item.original_id);
        json_item["style"] = item.style;

        for (auto &[class_id, slot_id] : item.equip_states) {
            nlohmann::json json_equip;
            json_equip["class"] = class_id;
            json_equip["slot"] = slot_id;
            json_item["equip_states"].push_back(json_equip);
        }

        for (const Econ_Item_Attribute &attr : item.attributes) {
            nlohmann::json json_attr;
            json_attr["definition"] = attr.def;

            switch (attr.type) {
                case Econ_Item_Attribute::ATTR_TYPE_DEFAULT: {
                    float value;
                    attr.value_bytes.copy(reinterpret_cast<char *>(&value), sizeof(float));
                    json_attr["value"] = value;
                    break;
                }
                case Econ_Item_Attribute::ATTR_TYPE_FLOAT: {
                    float value;
                    attr.value_bytes.copy(reinterpret_cast<char *>(&value), sizeof(float));
                    json_attr["value_float"] = value;
                    break;
                }
                case Econ_Item_Attribute::ATTR_TYPE_INT: {
                    uint32 value;
                    attr.value_bytes.copy(reinterpret_cast<char *>(&value), sizeof(uint32));
                    json_attr["value_int"] = value;
                    break;
                }
                case Econ_Item_Attribute::ATTR_TYPE_STRING: {
                    const bool has_trailing_nul = !attr.value_bytes.empty() && attr.value_bytes.back() == '\0';
                    const size_t text_size = has_trailing_nul ? attr.value_bytes.size() - 1u : attr.value_bytes.size();
                    bool printable_text = true;
                    for (size_t i = 0; i < text_size; ++i) {
                        const unsigned char ch = static_cast<unsigned char>(attr.value_bytes[i]);
                        if (ch == 0 || !std::isprint(ch)) {
                            printable_text = false;
                            break;
                        }
                    }

                    if (printable_text) {
                        json_attr["value_string"] = attr.value_bytes.substr(0, text_size);
                    } else {
                        json_attr["value_bytes_hex"] = gbe::proto_wire::format_hex(reinterpret_cast<const std::uint8_t *>(attr.value_bytes.data()), attr.value_bytes.size());
                    }
                    break;
                }
            }

            json_item["attributes"].push_back(json_attr);
        }

        items_json[std::to_string(item_id)] = json_item;
    }

    local_storage->write_json_file("", items_user_file, items_json);
}


const Econ_Item *Steam_Game_Coordinator::set_item_pos(uint64 item_id, uint32 inv_pos, bool is_gc, bool save)
{
    for (Econ_Item &item : items) {
        if (item.id != item_id)
            continue;

        item.inv_pos = inv_pos;
        if (save) {
            save_items_to_file();
        }

        // Let the others know, too.
        auto inventory_msg = new GameServer_Items_Messages::ItemUpdate();
        inventory_msg->set_id(item_id);
        inventory_msg->set_inv_pos(inv_pos);

        auto gameserver_items_msg = new GameServer_Items_Messages();
        gameserver_items_msg->set_type(GameServer_Items_Messages::Request_UpdateItem);
        gameserver_items_msg->set_is_gc(is_gc);
        gameserver_items_msg->set_allocated_item_update(inventory_msg);

        Common_Message msg{};
        msg.set_allocated_gameserver_items_messages(gameserver_items_msg);
        msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
        network->sendToAll(&msg, true);

        return &item;
    }

    return nullptr;
}


bool Steam_Game_Coordinator::delete_item(uint64 item_id, bool is_gc)
{
    for (auto it = items.begin(); it != items.end(); it++) {
        if (it->id != item_id)
            continue;

        items.erase(it);
        save_items_to_file();

        // Let the others know, too.
        auto delete_msg = new GameServer_Items_Messages::ItemDeletion();
        delete_msg->set_item_id(item_id);

        auto gameserver_items_msg = new GameServer_Items_Messages();
        gameserver_items_msg->set_type(GameServer_Items_Messages::Request_DeleteItem);
        gameserver_items_msg->set_is_gc(is_gc);
        gameserver_items_msg->set_allocated_item_deletion(delete_msg);

        Common_Message msg{};
        msg.set_allocated_gameserver_items_messages(gameserver_items_msg);
        msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
        network->sendToAll(&msg, true);

        return true;
    }

    return false;
}


void Steam_Game_Coordinator::request_user_items(CSteamID steam_id, SteamAPICall_t api_call, bool is_gc)
{
    RequestInventory new_request{};
    new_request.created = std::chrono::high_resolution_clock::now();
    new_request.steam_id = steam_id;
    new_request.steam_api_call = api_call;
    new_request.is_gc = is_gc;
    pending_items_requests.push_back(new_request);

    auto request_msg = new GameServer_Items_Messages::InventoryRequest();
    request_msg->set_steam_api_call(new_request.steam_api_call);

    auto gameserver_items_msg = new GameServer_Items_Messages();
    gameserver_items_msg->set_type(GameServer_Items_Messages::Request_Inventory);
    gameserver_items_msg->set_is_gc(is_gc);
    gameserver_items_msg->set_allocated_inventory_request(request_msg);

    Common_Message msg{};
    msg.set_allocated_gameserver_items_messages(gameserver_items_msg);
    msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
    msg.set_dest_id(steam_id.ConvertToUint64());
    network->sendTo(&msg, true);
}


SteamAPICall_t Steam_Game_Coordinator::find_items_request(CSteamID steam_id)
{
    auto it = std::find_if(
        pending_items_requests.begin(), pending_items_requests.end(),
        [=](const RequestInventory &item) {
            return item.steam_id == steam_id;
        }
    );

    if (it == pending_items_requests.end())
        return k_uAPICallInvalid;

    return it->steam_api_call;
}


void Steam_Game_Coordinator::remove_user_items(CSteamID steam_id)
{
    all_user_items.erase(steam_id);

    // Clean up any pending requests we have.
    for (auto it = pending_items_requests.begin(); it != pending_items_requests.end();) {
        if (it->steam_id == steam_id) {
            it = pending_items_requests.erase(it);
        } else {
            it++;
        }
    }

    callback_items_removed(steam_id);
}
