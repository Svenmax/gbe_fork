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

// Pure item-serialization payload helpers for the Dota Game Coordinator.
// Extracted from gbe_dota_gc_payload_helpers.cpp (Phase 3.2.3) as part of the
// payload-helper split. This TU owns the four functions that operate solely on
// Econ_Item / GBE_DotaEquipOp / CSteamID inputs and produce wire/protobuf
// outputs, with no dependency on Steam_Game_Coordinator, Steam_Client,
// Settings, or shared lobby state.
//
// Responsibility boundary: parse equip-op request bodies, mutate item style
// bitmask attributes, serialize Econ_Item to the GC protobuf wire format, and
// build CMsgSOSingleObject envelopes. All functions are pure with respect to
// coordinator/global state (the only side effect is writing into the
// caller-provided output parameters).
//
// Migration note: this extraction resolves the TODO(phase-3.2) stub
// duplication in tools/gbe_dota_handler_test/free_func_stubs.cpp — once these
// four functions live in a TU that does not pull in the heavy SDK include
// chain (steam_game_coordinator.h -> dll.h -> common_includes.h ->
// common_helpers/os_detector.h), the test harness can link the real TU
// directly instead of duplicating the implementations into stubs.

#include "dll/econ_item.h"
#include "gbe_dota_gc_internal.h"
#include "gbe_dota_payload_item_helpers.h"
#include "gbe_proto_wire.h"

#include <cstring>
#include <string>
#include <vector>

#include <steam/steamclientpublic.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/econ_gcmessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>

using namespace gamecoordinator::tf2;


// --- GBE_ParseDotaEquipOps ---
// Parses a ClientToGCEquipItemsRequest body into a list of equip operations.
//
// Body layout: repeated field 1 (tag 0x0a, length-delimited) sub-messages.
// Each sub-message contains varint fields:
//   1 = item_id (uint64)
//   2 = new_class (uint32)
//   3 = new_slot (uint32)
//   4 = style_index (uint32, optional, defaults to 255 = no style change)
bool GBE_ParseDotaEquipOps(const uint8 *body, size_t body_size, std::vector<GBE_DotaEquipOp> &equip_ops)
{
    equip_ops.clear();

    if (!body || body_size == 0)
        return false;

    size_t offset = 0;
    while (offset < body_size) {
        gbe::proto_wire::Field outer{};
        if (!gbe::proto_wire::read_next_field(body, body_size, offset, outer))
            return false;
        if (outer.number != 1u || outer.wire_type != 2u)
            return false;

        const uint8_t *sub = body + outer.value_offset;
        const size_t sub_len = outer.value_size;
        size_t sub_off = 0;
        GBE_DotaEquipOp op{};
        op.style_index = 255u;  // default: no style change

        while (sub_off < sub_len) {
            gbe::proto_wire::Field field{};
            if (!gbe::proto_wire::read_next_field(sub, sub_len, sub_off, field))
                return false;

            if (field.number < 1u || field.number > 4u)
                continue;

            if (field.wire_type != 0u)
                return false;

            uint64_t val = 0;
            if (!gbe::proto_wire::read_field_uint64(sub, sub_len, field, val))
                return false;

            if (field.number == 1u) {
                op.item_id = val;
                op.has_item_id = true;
            } else if (field.number == 2u) {
                if (val > UINT16_MAX)
                    return false;
                op.new_class = static_cast<uint32_t>(val);
                op.has_new_class = true;
            } else if (field.number == 3u) {
                if (val > UINT16_MAX)
                    return false;
                op.new_slot = static_cast<uint32_t>(val);
                op.has_new_slot = true;
            } else if (field.number == 4u) {
                if (val > UINT8_MAX)
                    return false;
                op.style_index = static_cast<uint32_t>(val);
            }
        }

        if (!op.has_item_id || !op.has_new_class || !op.has_new_slot)
            return false;

        equip_ops.push_back(op);
    }

    return !equip_ops.empty();
}

// --- GBE_ApplyDotaUnlockStyleBitmask ---
// Updates an item's attr 400 (unlocked styles bitmask) by OR-ing in the
// style_index bit. If attr 400 doesn't exist, creates it with all bits set
// (0xFFFFFFFF, LAN behavior = unlock all styles). Also sets item.style
// to the requested index.
bool GBE_ApplyDotaUnlockStyleBitmask(Econ_Item &item, uint32 style_index)
{
    if (style_index >= 32u)
        return false;

    // Set the item's current style
    item.style = static_cast<uint8>(style_index);

    // Find or create attr 400 (unlocked styles bitmask)
    for (auto &attr : item.attributes) {
        if (attr.def == 400u) {
            uint32_t current_val = 0;
            if (attr.value_bytes.size() >= 4) {
                memcpy(&current_val, attr.value_bytes.data(), 4);
            }
            current_val |= (1u << style_index);
            attr.value_bytes.assign(reinterpret_cast<const char *>(&current_val), 4);
            return true;
        }
    }

    // Attr 400 not found, create it with all bits set (LAN behavior)
    Econ_Item_Attribute unlock_attr;
    unlock_attr.def = 400u;
    uint32_t val = 0xFFFFFFFFu;
    unlock_attr.value_bytes.assign(reinterpret_cast<const char *>(&val), 4);
    unlock_attr.type = Econ_Item_Attribute::ATTR_TYPE_INT;
    item.attributes.push_back(unlock_attr);
    return true;
}

// --- GBE_SerializeEconItemToGcprotobuf ---
// Serializes an Econ_Item into a CSOEconItem protobuf message string.
// gc_version controls the attribute value encoding (raw uint32 vs value_bytes).
// is_portal2 forces the legacy uint32 encoding regardless of gc_version.
std::string GBE_SerializeEconItemToGcprotobuf(const Econ_Item &item, CSteamID steam_id, uint32 gc_version, bool is_portal2)
{
    CSOEconItem proto_item;
    proto_item.set_id(item.id);
    proto_item.set_account_id(steam_id.GetAccountID());
    proto_item.set_inventory(item.inv_pos);
    proto_item.set_def_index(item.def);
    proto_item.set_quantity(item.quantity);
    proto_item.set_level(item.level);
    proto_item.set_quality(item.quality);
    proto_item.set_flags(item.flags);
    proto_item.set_origin(item.origin);

    if (!item.custom_name.empty())
        proto_item.set_custom_name(item.custom_name);

    if (!item.custom_desc.empty())
        proto_item.set_custom_desc(item.custom_desc);

    proto_item.set_in_use(item.in_use);
    proto_item.set_style(item.style);
    proto_item.set_original_id(item.original_id);

    if (!item.equip_states.empty()) {
        proto_item.set_contains_equipped_state(true);
        proto_item.set_contains_equipped_state_v2(true);
    }

    for (const auto &[class_id, slot_id] : item.equip_states) {
        auto proto_equip = proto_item.add_equipped_state();
        proto_equip->set_new_class(class_id);
        proto_equip->set_new_slot(slot_id);
    }

    for (const Econ_Item_Attribute &attr : item.attributes) {
        auto proto_attr = proto_item.add_attribute();
        proto_attr->set_def_index(attr.def);
        if (gc_version < 20130319 || is_portal2) {
            uint32 value;
            memcpy(&value, &attr.value, sizeof(uint32));
            proto_attr->set_value(value);
        } else {
            proto_attr->set_value_bytes(attr.value_bytes);
        }
    }

    return proto_item.SerializeAsString();
}

// --- GBE_BuildSOSingleObjectFromItem ---
// Builds a CMsgSOSingleObject serialized string from an Econ_Item.
// type_id is set to 1 (econ item), object_data is the item's protobuf
// serialization. owner_soid is set to the player's steam_id.
bool GBE_BuildSOSingleObjectFromItem(const Econ_Item &item, const CSteamID &steam_id, std::string &output)
{
    CMsgSOSingleObject so_msg;
    auto *owner = so_msg.mutable_owner_soid();
    owner->set_type(1u);
    owner->set_id(steam_id.ConvertToUint64());
    so_msg.set_type_id(1);
    so_msg.set_object_data(GBE_SerializeEconItemToGcprotobuf(item, steam_id, 0u, false));
    so_msg.set_version(0);

    output = so_msg.SerializeAsString();
    return true;
}
