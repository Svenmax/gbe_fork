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

// Pure wire-level payload helpers for the Dota Game Coordinator.
// Extracted from gbe_dota_gc_payload_helpers.cpp (Phase 3.2.2) as part of the
// payload-helper split. This TU owns the byte/varint/field parsing & patching
// helpers that operate on direct-protobuf-framed GC messages with no dependency
// on Steam_Game_Coordinator, Steam_Client, Settings, or shared lobby state.
//
// Responsibility boundary:
//   - parse direct proto contexts and rewrite account_id varints / fixed32
//   - patch lobby template identifiers (lobby_id / steam_id fixed64)
//   - force SOID ownership on SO update / cache-subscribed messages
//   - patch practice-lobby cache-subscribed template state and launch template
//   - prepare welcome body / direct replay / peripheral messages from canned
//     templates
//
// All functions are pure with respect to coordinator/global state: the only
// side effects are writes into caller-provided output parameters and
// GBE_GC_DebugLog trace calls (the log helper is a free function, not
// coordinator state).
//
// Migration note: this extraction is part of Phase 3.2's payload-helper split.
// Combined with the earlier pure item TU (gbe_dota_payload_item_helpers.cpp),
// it shrinks gbe_dota_gc_payload_helpers.cpp toward the 1200-line target
// (Phase 3.2.5) and lets the offline test harness compile the real wire logic
// against stub protobuf types instead of duplicating implementations.
//
// Three helpers that were file-scope `static` in the original TU
// (GBE_PatchDotaLobbyTemplateIdentifiersIfPresent,
// GBE_ForceDotaLobbyCacheOwnerSOID, GBE_PrepareDotaWelcomeBody) are now
// external-linkage so the pure lobby TU can still call them; their
// declarations live in gbe_dota_payload_wire_helpers.h. GBE_PatchDotaWelcomeAccountObjects
// remains file-scope `static` because it is only called from
// GBE_PrepareDotaWelcomeBody within this TU.

#include "gbe_dota_binary_helpers.h"
#include "gbe_dota_gc_diagnostics.h"
#include "gbe_dota_protocol_assets.h"
#include "gbe_dota_payload_lobby_helpers.h"
#include "gbe_dota_payload_wire_helpers.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_request_router.h"
#include "gbe_proto_wire.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>

#include <steam/steamclientpublic.h>
#include <steammessages.pb.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>

using namespace gamecoordinator::tf2;

// --- File-local static byte arrays (Phase 3.2.2 migration) ---
// These five constants are used only by the wire helpers below
// (GBE_PrepareDotaWelcomeBody uses the welcome template + version varint;
// GBE_PatchDotaPracticeLobbyLaunchTemplate uses the lobby_id varint,
// game_start_time varint, and connect string).

static const uint8 GBE_kDotaClientWelcomeTemplate[] = {
    0x4D, 0x15, 0x00, 0x80, 0x14, 0x00, 0x00, 0x00, 0x09, 0xF5, 0xB6, 0x21, 0x08, 0x01, 0x00, 0x10,
    0x01, 0x10, 0xEB, 0xFC, 0x88, 0xA1, 0xF8, 0xFF, 0xFF, 0xFF, 0xFF, 0x01, 0x08, 0xBA, 0x04, 0x10,
    0xA4, 0x9F, 0x80, 0x80, 0x08, 0x1A, 0xD6, 0x06, 0xA4, 0x0F, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00,
    0x08, 0xEB, 0x34, 0x12, 0xB0, 0x03, 0x28, 0xBF, 0xA4, 0xE0, 0x86, 0x07, 0x38, 0x01, 0x68, 0xC6,
    0xCC, 0xBC, 0xEE, 0x0D, 0x88, 0x01, 0x00, 0x90, 0x01, 0x1B, 0xB0, 0x01, 0x09, 0xD2, 0x01, 0x32,
    0x08, 0x9F, 0x14, 0x12, 0x2D, 0x0A, 0x0D, 0x08, 0x80, 0xFB, 0xE0, 0xCE, 0x06, 0x10, 0x8A, 0xCF,
    0xB8, 0xE0, 0x82, 0x01, 0x0A, 0x0D, 0x08, 0x80, 0xA9, 0xC1, 0xCE, 0x06, 0x10, 0xEA, 0x98, 0xA6,
    0xD2, 0x82, 0x01, 0x0A, 0x0D, 0x08, 0x80, 0x84, 0xC8, 0xCD, 0x06, 0x10, 0xFE, 0xCA, 0x98, 0x8E,
    0x82, 0x01, 0xD2, 0x01, 0x47, 0x08, 0xD8, 0x3E, 0x12, 0x42, 0x08, 0xDF, 0xE5, 0xC7, 0x81, 0x06,
    0x08, 0x82, 0xB7, 0x99, 0xB9, 0x07, 0x08, 0xB8, 0x99, 0xE2, 0xCA, 0x0A, 0x08, 0xBB, 0x8B, 0xB4,
    0xAF, 0x0C, 0x08, 0x81, 0xFC, 0xE4, 0x9E, 0x0A, 0x08, 0xFB, 0x90, 0xE9, 0xDC, 0x0B, 0x08, 0x91,
    0xCE, 0xA9, 0xA4, 0x0A, 0x08, 0xC9, 0xAE, 0x89, 0xA0, 0x09, 0x08, 0xE7, 0xC4, 0xD1, 0xEF, 0x05,
    0x08, 0xA7, 0x83, 0xC7, 0xDA, 0x0C, 0x10, 0x84, 0x96, 0xCC, 0x8C, 0x0D, 0xD2, 0x01, 0x07, 0x08,
    0x8B, 0x3F, 0x12, 0x02, 0x08, 0x6B, 0xD2, 0x01, 0x7D, 0x08, 0xA9, 0x3A, 0x12, 0x78, 0x0A, 0x16,
    0x08, 0x02, 0x10, 0xA0, 0x98, 0xEA, 0xCE, 0x06, 0x18, 0xA4, 0x9F, 0xEA, 0xCE, 0x06, 0x20, 0xA0,
    0x8D, 0x8F, 0xCF, 0x06, 0x28, 0x43, 0x0A, 0x16, 0x08, 0x03, 0x10, 0xB0, 0xC8, 0x8D, 0xCF, 0x06,
    0x18, 0xB4, 0xCF, 0x8D, 0xCF, 0x06, 0x20, 0xB0, 0xBD, 0xB2, 0xCF, 0x06, 0x28, 0x43, 0x0A, 0x16,
    0x08, 0x04, 0x10, 0xC0, 0xEF, 0xE8, 0xCE, 0x06, 0x18, 0xC4, 0xF6, 0xE8, 0xCE, 0x06, 0x20, 0xC0,
    0xE4, 0x8D, 0xCF, 0x06, 0x28, 0x43, 0x0A, 0x16, 0x08, 0x06, 0x10, 0x90, 0xDD, 0xEB, 0xCE, 0x06,
    0x18, 0x94, 0xE4, 0xEB, 0xCE, 0x06, 0x20, 0x90, 0xD2, 0x90, 0xCF, 0x06, 0x28, 0x43, 0x0A, 0x16,
    0x08, 0x07, 0x10, 0xF0, 0xA4, 0xEB, 0xCE, 0x06, 0x18, 0xF4, 0xAB, 0xEB, 0xCE, 0x06, 0x20, 0xF0,
    0x99, 0x90, 0xCF, 0x06, 0x28, 0x43, 0xD2, 0x01, 0x07, 0x08, 0x83, 0x3F, 0x12, 0x02, 0x08, 0x01,
    0xD8, 0x01, 0xF5, 0xE8, 0xDB, 0xBF, 0x82, 0x01, 0xE0, 0x01, 0x00, 0xF0, 0x01, 0xB0, 0x18, 0x92,
    0x02, 0x71, 0x08, 0xA7, 0x14, 0x12, 0x6C, 0x0A, 0x2F, 0x31, 0x4F, 0x63, 0x95, 0x01, 0xAF, 0x01,
    0xC7, 0x01, 0xF9, 0x01, 0xAB, 0x02, 0x8F, 0x03, 0xF3, 0x03, 0xD7, 0x04, 0xBB, 0x05, 0x9F, 0x06,
    0x83, 0x07, 0xE7, 0x07, 0xCB, 0x08, 0xAF, 0x09, 0x93, 0x0A, 0xDB, 0x0B, 0xD5, 0x0D, 0xCF, 0x0F,
    0xC9, 0x11, 0x8B, 0x15, 0xAB, 0x1B, 0xE7, 0x20, 0x12, 0x39, 0x08, 0x1B, 0x12, 0x35, 0xAC, 0x02,
    0xD8, 0x04, 0xBC, 0x05, 0xE8, 0x07, 0xB0, 0x09, 0xF8, 0x0A, 0xA4, 0x0D, 0xB4, 0x10, 0xF0, 0x15,
    0xAC, 0x1B, 0xE8, 0x20, 0xC0, 0x25, 0xFC, 0x2A, 0xB8, 0x30, 0xF4, 0x35, 0xB0, 0x3B, 0xEC, 0x40,
    0xA8, 0x46, 0xBC, 0x50, 0xA8, 0x5F, 0xE8, 0x6B, 0xF0, 0x79, 0xA8, 0x91, 0x01, 0xEC, 0xBD, 0x01,
    0x80, 0xE1, 0x01, 0x98, 0x02, 0x37, 0x1A, 0xEA, 0x02, 0x12, 0xA1, 0x02, 0x08, 0xD2, 0x0F, 0x12,
    0x9B, 0x02, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x9A, 0x01, 0x20, 0xA0, 0x01, 0x60, 0x3A, 0x68,
    0x07, 0x70, 0x66, 0x78, 0x02, 0x90, 0x01, 0x00, 0xA0, 0x01, 0x00, 0xA8, 0x01, 0x00, 0xB0, 0x01,
    0xA5, 0xF1, 0xFA, 0xAA, 0x06, 0xB8, 0x01, 0x02, 0xC0, 0x01, 0x3E, 0xB0, 0x02, 0x00, 0xB8, 0x02,
    0x00, 0xC8, 0x02, 0xE5, 0xAF, 0xE4, 0xBF, 0x06, 0xD0, 0x02, 0x0C, 0x80, 0x03, 0x00, 0xB8, 0x03,
    0x01, 0xC8, 0x03, 0x00, 0xD0, 0x03, 0x01, 0xD8, 0x03, 0xCF, 0xD2, 0x9B, 0xA7, 0x05, 0xE0, 0x03,
    0xD5, 0x01, 0xE8, 0x03, 0x36, 0xF0, 0x03, 0x1C, 0x88, 0x04, 0x00, 0x98, 0x04, 0x07, 0xA0, 0x04,
    0x94, 0x01, 0xA8, 0x04, 0x03, 0xB0, 0x04, 0x43, 0xB8, 0x04, 0x95, 0x02, 0xC0, 0x04, 0xD2, 0x22,
    0xC8, 0x04, 0x01, 0xD0, 0x04, 0x03, 0xB0, 0x05, 0x00, 0xC0, 0x05, 0xA4, 0xC7, 0xC7, 0x94, 0x80,
    0xE3, 0xC8, 0xCE, 0x75, 0xC8, 0x05, 0xC4, 0xEF, 0x8F, 0xD0, 0x05, 0xD0, 0x05, 0xAC, 0xF1, 0xEE,
    0xBF, 0x06, 0xD8, 0x05, 0xEF, 0xDB, 0xEE, 0xBF, 0x06, 0xE0, 0x05, 0xBE, 0xC8, 0xEE, 0xBF, 0x06,
    0xB8, 0x06, 0xE1, 0xAC, 0x8B, 0x84, 0xD0, 0x85, 0x40, 0xC0, 0x06, 0xB4, 0xA7, 0xAC, 0x9D, 0x06,
    0xC8, 0x06, 0x80, 0x9A, 0x9A, 0x9B, 0x06, 0xD0, 0x06, 0xAC, 0xF1, 0xEE, 0xBF, 0x06, 0xD8, 0x06,
    0xEF, 0xDB, 0xEE, 0xBF, 0x06, 0xE0, 0x06, 0x99, 0xC1, 0xE4, 0xBF, 0x06, 0xE8, 0x06, 0x00, 0x90,
    0x07, 0x3C, 0x9A, 0x07, 0x07, 0x08, 0x01, 0x15, 0xCD, 0xCC, 0x4C, 0x3D, 0x9A, 0x07, 0x07, 0x08,
    0x04, 0x15, 0xCD, 0xCC, 0x4C, 0x3D, 0x9A, 0x07, 0x07, 0x08, 0x02, 0x15, 0x00, 0x00, 0x00, 0x00,
    0x9A, 0x07, 0x07, 0x08, 0x08, 0x15, 0xCD, 0xCC, 0x4C, 0x3D, 0x9A, 0x07, 0x07, 0x08, 0x10, 0x15,
    0xCD, 0xCC, 0x4C, 0x3D, 0xC0, 0x07, 0x00, 0xC8, 0x07, 0xA0, 0x90, 0xDC, 0xB9, 0x06, 0xD0, 0x07,
    0x00, 0xD8, 0x07, 0x00, 0xD8, 0x07, 0x00, 0xD8, 0x07, 0x00, 0xD8, 0x07, 0x00, 0x12, 0x22, 0x08,
    0xDC, 0x0F, 0x12, 0x1D, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x00, 0x18, 0x01, 0x20, 0x00, 0x28,
    0x00, 0x30, 0x00, 0x3D, 0x00, 0x00, 0x00, 0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x19, 0x2D, 0x4B, 0xA7, 0x55, 0x7F, 0xD5, 0x69, 0x00, 0x22, 0x0C, 0x08, 0x01, 0x10, 0xF5,
    0xED, 0x86, 0xC1, 0x90, 0x80, 0x80, 0x88, 0x01, 0x30, 0x01, 0x39, 0xB1, 0x4D, 0xA7, 0x55, 0x7F,
    0xD5, 0x69, 0x00, 0x2A, 0x0E, 0x0D, 0x48, 0xA1, 0x1F, 0x42, 0x15, 0x51, 0xCB, 0xE8, 0x42, 0x1A,
    0x02, 0x43, 0x4E, 0x48, 0x09, 0x52, 0x02, 0x43, 0x4E, 0x80, 0x01, 0x00, 0x88, 0x01, 0x00, 0x92,
    0x01, 0x0C, 0x08, 0xA7, 0x23, 0x12, 0x07, 0x0A, 0x05, 0x08, 0xA8, 0x23, 0x12, 0x00,
};
static const std::array<uint8, 2> GBE_kOldDotaVersionVarint = { 0xEB, 0x34 };
static const std::array<uint8, 8> GBE_kOldDotaPracticeLobbyLobbyIdVarint = { 0x83, 0xCF, 0xA2, 0xB4, 0xA2, 0xFF, 0xF9, 0x34 };
static const std::array<uint8, 5> GBE_kOldDotaPracticeLobbyGameStartTimeVarint = { 0xAE, 0xBB, 0xA3, 0xCF, 0x06 };
static constexpr const char *GBE_kOldDotaPracticeLobbyConnect = "117.157.79.194:27015 10.110.4.21:27015";

// --- Wire helper functions ---

static bool GBE_PatchDotaWelcomeAccountObjects(std::string &inner_body, uint32 account_id)
{
    std::string rewritten_body;
    int patched_object_count = 0;
    size_t offset = 0;
    while (offset < inner_body.size()) {
        gbe::proto_wire::Field field{};
        size_t field_offset = 0;
        size_t field_end = 0;
        if (!gbe::proto_wire::read_next_field(reinterpret_cast<const uint8 *>(inner_body.data()), inner_body.size(), offset, field, &field_offset, &field_end))
            return false;

        if (field.number != 3u || field.wire_type != 2u) {
            rewritten_body.append(inner_body.data() + field_offset, field_end - field_offset);
            continue;
        }

        CMsgSOCacheSubscribed cache;
        if (!cache.ParseFromArray(inner_body.data() + field.value_offset, static_cast<int>(field.value_size)))
            return false;

        int cache_patch_count = 0;
        for (int object_index = 0; object_index < cache.objects_size(); ++object_index) {
            auto *object = cache.mutable_objects(object_index);
            const int type_id = object->type_id();
            if (type_id != 2002 && type_id != 2012)
                continue;

            for (int data_index = 0; data_index < object->object_data_size(); ++data_index) {
                std::string rewritten_object;
                if (!gbe::proto_wire::rewrite_dota_account_bound_object_data(object->object_data(data_index), type_id, account_id, rewritten_object))
                    return false;

                object->set_object_data(data_index, rewritten_object);
                ++patched_object_count;
                ++cache_patch_count;
            }
        }

        if (cache_patch_count != 0) {
            gbe::proto_wire::append_bytes_field(rewritten_body, 3u, cache.SerializeAsString());
            continue;
        }

        rewritten_body.append(inner_body.data() + field_offset, field_end - field_offset);
    }

    if (patched_object_count != 0) {
        inner_body.swap(rewritten_body);
        GBE_GC_DebugLog("GC_DOTA_WELCOME", "patched welcome account-bound objects count=%d account_id=%u", patched_object_count, account_id);
    }

    return true;
}

bool GBE_PatchDotaLobbyTemplateIdentifiers(std::string &message, uint32 account_id, uint64 steam_id, uint64 lobby_id)
{
    (void)account_id;
    const std::vector<uint8> old_lobby_id = gbe::proto_wire::vector_from_bytes(GBE_kOldDotaLobbyIdVarint.data(), GBE_kOldDotaLobbyIdVarint.size());
    const std::vector<uint8> old_steam_id_fixed64 = gbe::proto_wire::vector_from_bytes(GBE_kOldDotaSteamIdFixed64.data(), GBE_kOldDotaSteamIdFixed64.size());
    gbe::proto_wire::PatchTemplateIdentifierResult patch_result{};
    if (!gbe::proto_wire::patch_dota_lobby_template_identifiers(message, old_lobby_id, lobby_id, true, old_steam_id_fixed64, steam_id, true, patch_result)) {
        if (patch_result.lobby_id_match_count == 0)
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed replacing lobby_id bytes lobby_id=%llu", static_cast<unsigned long long>(lobby_id));
        else
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed replacing steam_id fixed64 bytes steam_id=%llu", static_cast<unsigned long long>(steam_id));
        return false;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Patched template LobbyID matches=%zu SteamIDFixed64 matches=%zu body_prefix=%s",
        patch_result.lobby_id_match_count,
        patch_result.steam_id_fixed64_match_count,
        gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(message.data()), message.size(), 32).c_str()
    );
    return true;
}

bool GBE_PatchDotaLobbyTemplateIdentifiersIfPresent(std::string &message, uint64 steam_id, uint64 lobby_id)
{
    const std::vector<uint8> old_lobby_id = gbe::proto_wire::vector_from_bytes(GBE_kOldDotaLobbyIdVarint.data(), GBE_kOldDotaLobbyIdVarint.size());
    const std::vector<uint8> old_steam_id_fixed64 = gbe::proto_wire::vector_from_bytes(GBE_kOldDotaSteamIdFixed64.data(), GBE_kOldDotaSteamIdFixed64.size());
    gbe::proto_wire::PatchTemplateIdentifierResult patch_result{};
    if (!gbe::proto_wire::patch_dota_lobby_template_identifiers(message, old_lobby_id, lobby_id, false, old_steam_id_fixed64, steam_id, false, patch_result)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed replacing optional template identifiers lobby_id=%llu steam_id=%llu", static_cast<unsigned long long>(lobby_id), static_cast<unsigned long long>(steam_id));
        return false;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Patched optional template identifiers LobbyID matches=%zu SteamIDFixed64 matches=%zu body_prefix=%s",
        patch_result.lobby_id_match_count,
        patch_result.steam_id_fixed64_match_count,
        gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(message.data()), message.size(), 32).c_str()
    );
    return true;
}

bool GBE_ForceDotaLobbyCacheOwnerSOID(std::string &message, uint64 lobby_id)
{
    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return false;

    CMsgSOCacheSubscribed protomsg;
    if (!protomsg.ParseFromArray(proto_context.body, static_cast<int>(proto_context.body_size)))
        return false;

    protomsg.clear_owner();
    CMsgSOIDOwner *owner_soid = protomsg.mutable_owner_soid();
    owner_soid->set_type(3u);
    owner_soid->set_id(lobby_id);

    std::string updated = message.substr(0, proto_context.body_offset);
    protomsg.AppendToString(&updated);
    message.swap(updated);
    return true;
}

bool GBE_ForceDotaLobbyUpdateOwnerSOID(std::string &message, uint64 lobby_id)
{
    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return false;

    CMsgSOMultipleObjects protomsg;
    if (!protomsg.ParseFromArray(proto_context.body, static_cast<int>(proto_context.body_size)))
        return false;

    protomsg.clear_owner();
    CMsgSOIDOwner *owner_soid = protomsg.mutable_owner_soid();
    owner_soid->set_type(3u);
    owner_soid->set_id(lobby_id);

    std::string updated = message.substr(0, proto_context.body_offset);
    protomsg.AppendToString(&updated);
    message.swap(updated);
    return true;
}

bool GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
    std::string &message,
    uint32 account_id,
    uint64 steam_id,
    uint64 lobby_id,
    bool rewrite_runtime_fields,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool lan,
    const std::string &lan_host_ping_location,
    bool allow_cheats,
    bool fill_with_bots,
    bool allow_spectating,
    uint32 visibility,
    uint32 bot_difficulty_radiant,
    uint32 bot_difficulty_dire,
    uint64 bot_radiant,
    uint64 bot_dire,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool rewrite_2015,
    uint32 extra_startup_account_id,
    const std::string &pass_key,
    const GBE_DotaCustomGameDetails *custom_game)
{
    (void)account_id;

    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return false;

    const std::string body(reinterpret_cast<const char *>(proto_context.body), proto_context.body_size);
    std::string rewritten_body;
    const uint32 scratch_startup_account_id = rewrite_2015 ? extra_startup_account_id : 0u;

    gbe::gc_message::DotaPracticeLobbyObjects lobby_objects;
    GBE_ComposeDotaPracticeLobbySOObjects(
        steam_id,
        lobby_id,
        lobby_state,
        lobby_game_state,
        server_id,
        match_id,
        game_start_time,
        connect,
        player_name,
        room_name,
        game_mode,
        server_region,
        lan,
        lan_host_ping_location,
        allow_cheats,
        fill_with_bots,
        allow_spectating,
        visibility,
        bot_difficulty_radiant,
        bot_difficulty_dire,
        bot_radiant,
        bot_dire,
        owner_team,
        owner_slot,
        owner_hero_id,
        members,
        false,
        0u,
        std::string(),
        std::string(),
        std::string(),
        pass_key,
        scratch_startup_account_id,
        custom_game,
        lobby_objects);

    size_t offset = 0;
    while (offset < body.size()) {
        gbe::proto_wire::Field field{};
        size_t field_offset = 0;
        size_t field_end = 0;
        if (!gbe::proto_wire::read_next_field(reinterpret_cast<const uint8 *>(body.data()), body.size(), offset, field, &field_offset, &field_end))
            return false;

        if (field.number != 2u || field.wire_type != 2u) {
            rewritten_body.append(body.data() + field_offset, field_end - field_offset);
            continue;
        }

        const std::string subscribed = body.substr(field.value_offset, field.value_size);
        uint64 type_id = 0;
        if (!gbe::proto_wire::read_uint64_field(reinterpret_cast<const uint8 *>(subscribed.data()), subscribed.size(), 1u, type_id)) {
            rewritten_body.append(body.data() + field_offset, field_end - field_offset);
            continue;
        }

        const bool should_rewrite_type = rewrite_runtime_fields
            ? (type_id == 2004u || type_id == 2014u || type_id == 2015u || type_id == 2016u)
            : (type_id == 2004u || type_id == 2014u);
        if (!should_rewrite_type) {
            rewritten_body.append(body.data() + field_offset, field_end - field_offset);
            continue;
        }

        std::string rewritten_subscribed;
        size_t subscribed_offset = 0;
        while (subscribed_offset < subscribed.size()) {
            gbe::proto_wire::Field subscribed_field{};
            size_t subscribed_field_offset = 0;
            size_t subscribed_field_end = 0;
            if (!gbe::proto_wire::read_next_field(reinterpret_cast<const uint8 *>(subscribed.data()), subscribed.size(), subscribed_offset, subscribed_field, &subscribed_field_offset, &subscribed_field_end))
                return false;

            if (subscribed_field.number == 2u && subscribed_field.wire_type == 2u) {
                std::string rewritten_object;
                switch (type_id) {
                case 2004u:
                    rewritten_object = lobby_objects.object_2004;
                    break;
                case 2014u:
                    rewritten_object = lobby_objects.object_2014;
                    break;
                case 2015u:
                    rewritten_object = lobby_objects.object_2015;
                    break;
                case 2016u:
                    rewritten_object = lobby_objects.object_2016;
                    break;
                default:
                    rewritten_object.assign(subscribed.data() + subscribed_field.value_offset, subscribed_field.value_size);
                    break;
                }
                gbe::proto_wire::append_bytes_field(rewritten_subscribed, 2u, rewritten_object);
                continue;
            }

            rewritten_subscribed.append(subscribed.data() + subscribed_field_offset, subscribed_field_end - subscribed_field_offset);
        }

        gbe::proto_wire::append_bytes_field(rewritten_body, 2u, rewritten_subscribed);
    }

    message.resize(proto_context.body_offset);
    message.append(rewritten_body);
    return true;
}

bool GBE_PatchDotaPracticeLobbyLaunchTemplate(
    std::string &message,
    uint32 account_id,
    uint64 steam_id,
    uint64 lobby_id,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
    bool patch_server_id,
    bool patch_game_start_time,
    bool patch_connect,
    const char *stage_note)
{
    if (!GBE_TryPatchDotaAccountIdVarint(message, account_id, "GC_DOTA_PATCH", GBE_kDotaPracticeLobbyLaunch, GBE_kDotaPracticeLobbyDetailsUpdate, 0, stage_note)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Launch account_id varint patch failed stage=%s", stage_note ? stage_note : "");
        return false;
    }

    gbe::proto_wire::DotaPracticeLobbyLaunchTemplatePatchResult patch_result{};
    if (!gbe::proto_wire::patch_dota_practice_lobby_launch_template(
            message,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaSteamIdFixed64.data(), GBE_kOldDotaSteamIdFixed64.size()),
            steam_id,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaPracticeLobbyLobbyIdVarint.data(), GBE_kOldDotaPracticeLobbyLobbyIdVarint.size()),
            lobby_id,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaPracticeLobbyMatchIdVarint.data(), GBE_kOldDotaPracticeLobbyMatchIdVarint.size()),
            match_id,
            patch_server_id,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaPracticeLobbyServerIdFixed64.data(), GBE_kOldDotaPracticeLobbyServerIdFixed64.size()),
            server_id,
            patch_game_start_time,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaPracticeLobbyGameStartTimeVarint.data(), GBE_kOldDotaPracticeLobbyGameStartTimeVarint.size()),
            game_start_time,
            patch_connect,
            GBE_kOldDotaPracticeLobbyConnect,
            connect,
            patch_result)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Launch template patch failed stage=%s", stage_note ? stage_note : "");
        return false;
    }

    if (patch_result.steam_id_fixed64_match_count == 0) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Launch steam_id fixed64 patch skipped stage=%s steam_id=%llu; donor does not expose expected template bytes",
            stage_note ? stage_note : "",
            static_cast<unsigned long long>(steam_id)
        );
    }

    if (!GBE_TryPatchDotaAccountIdFixed32(message, account_id, "GC_DOTA_PATCH")) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Launch account_id fixed32 patch skipped stage=%s account_id=%u; donor does not expose expected template bytes",
            stage_note ? stage_note : "",
            account_id
        );
    }

    if (!patch_result.lobby_id_size_ok) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Launch lobby_id size mismatch skipped stage=%s lobby_id=%llu; donor varint width differs",
            stage_note ? stage_note : "",
            static_cast<unsigned long long>(lobby_id)
        );
    } else if (patch_result.lobby_id_match_count == 0) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Launch lobby_id patch skipped stage=%s lobby_id=%llu; donor does not expose expected template bytes",
            stage_note ? stage_note : "",
            static_cast<unsigned long long>(lobby_id)
        );
    }

    if (!patch_result.match_id_size_ok) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Launch match_id size mismatch skipped stage=%s match_id=%llu; donor varint width differs",
            stage_note ? stage_note : "",
            static_cast<unsigned long long>(match_id)
        );
    } else if (patch_result.match_id_match_count == 0) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Launch match_id patch skipped stage=%s match_id=%llu; donor does not expose expected template bytes",
            stage_note ? stage_note : "",
            static_cast<unsigned long long>(match_id)
        );
    }

    if (patch_server_id && patch_result.server_id_fixed64_match_count == 0) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Launch server_id patch skipped stage=%s server_id=%llu; donor does not expose expected template bytes",
            stage_note ? stage_note : "",
            static_cast<unsigned long long>(server_id)
        );
    }

    if (patch_game_start_time) {
        if (!patch_result.game_start_time_size_ok) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Launch game_start_time size mismatch skipped stage=%s game_start_time=%u; donor varint width differs",
                stage_note ? stage_note : "",
                game_start_time
            );
        } else if (patch_result.game_start_time_match_count == 0) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Launch game_start_time patch skipped stage=%s game_start_time=%u; donor does not expose expected template bytes",
                stage_note ? stage_note : "",
                game_start_time
            );
        }
    }

    if (patch_connect) {
        if (!patch_result.connect_size_ok) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Launch connect size changed stage=%s size=%zu expected=%zu; skipping fixed-width overwrite and relying on proto rewrite",
                stage_note ? stage_note : "",
                connect.size(),
                std::strlen(GBE_kOldDotaPracticeLobbyConnect)
            );
        } else if (patch_result.connect_match_count == 0) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Launch connect patch skipped stage=%s connect=%s; donor does not expose expected template string",
                stage_note ? stage_note : "",
                connect.c_str()
            );
        }
    }

    return true;
}

bool GBE_PrepareDotaWelcomeBody(uint64 steam_id, uint32 account_id, const GBE_DotaHelloContext &context, std::string &inner_body)
{
    if (!context.valid || GBE_kDotaWelcomeInnerBodyOffset >= sizeof(GBE_kDotaClientWelcomeTemplate)) {
        GBE_GC_DebugLog("GC_DOTA_WELCOME", "invalid context valid=%d offset=%zu template_size=%zu", context.valid ? 1 : 0, GBE_kDotaWelcomeInnerBodyOffset, sizeof(GBE_kDotaClientWelcomeTemplate));
        return false;
    }

    inner_body.assign(
        reinterpret_cast<const char *>(GBE_kDotaClientWelcomeTemplate + GBE_kDotaWelcomeInnerBodyOffset),
        sizeof(GBE_kDotaClientWelcomeTemplate) - GBE_kDotaWelcomeInnerBodyOffset
    );

    {
        size_t match_count = 0;
        bool size_ok = false;
        if (!gbe::proto_wire::patch_varint_template_value(
                inner_body,
                gbe::proto_wire::vector_from_bytes(GBE_kOldDotaVersionVarint.data(), GBE_kOldDotaVersionVarint.size()),
                context.version,
                match_count,
                size_ok)) {
            if (!size_ok) {
                GBE_GC_DebugLog("GC_DOTA_WELCOME", "version varint size mismatch version=%u expected=%zu", context.version, GBE_kOldDotaVersionVarint.size());
                return false;
            }
            GBE_GC_DebugLog("GC_DOTA_WELCOME", "failed replacing version bytes version=%u", context.version);
            return false;
        }
    }

    if (!GBE_PatchDotaWelcomeAccountObjects(inner_body, account_id)) {
        GBE_GC_DebugLog("GC_DOTA_WELCOME", "failed patching welcome account-bound objects account_id=%u", account_id);
        return false;
    }

    {
        size_t match_count = 0;
        bool size_ok = false;
        if (!gbe::proto_wire::patch_varint_template_value(
                inner_body,
                gbe::proto_wire::vector_from_bytes(GBE_kOldDotaSteamIdVarint.data(), GBE_kOldDotaSteamIdVarint.size()),
                steam_id,
                match_count,
                size_ok)) {
            if (!size_ok) {
                GBE_GC_DebugLog("GC_DOTA_WELCOME", "steam_id varint size mismatch steam_id=%llu expected=%zu", static_cast<unsigned long long>(steam_id), GBE_kOldDotaSteamIdVarint.size());
                return false;
            }
            GBE_GC_DebugLog("GC_DOTA_WELCOME", "failed replacing steam_id bytes steam_id=%llu", static_cast<unsigned long long>(steam_id));
            return false;
        }
    }

    GBE_GC_DebugLog("GC_DOTA_WELCOME", "prepared welcome body size=%zu", inner_body.size());
    return true;
}

bool GBE_PrepareDotaDirectReplayMessage(
    const uint8 *template_bytes,
    size_t template_size,
    uint32 account_id,
    uint64 steam_id,
    bool replace_account,
    bool replace_steam_id,
    bool has_target_job,
    uint64 target_job,
    uint32 request_emsg,
    uint32 response_emsg,
    size_t body_size,
    const char *context_note,
    std::string &message)
{
    message.assign(reinterpret_cast<const char *>(template_bytes), template_size);

    if (!GBE_PatchDotaTemplateIdentifiers(message, account_id, steam_id, replace_account, replace_steam_id, request_emsg, response_emsg, body_size, context_note))
        return false;

    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return false;

    CMsgProtoBufHeader protohdr = proto_context.protohdr;

    if (has_target_job) {
        protohdr.set_job_id_target(target_job);
    } else {
        protohdr.clear_job_id_target();
    }
    protohdr.clear_job_id_source();

    const char *body_ptr = reinterpret_cast<const char *>(proto_context.body);
    const size_t serialized_body_size = proto_context.body_size;

    std::string updated;
    ProtoBufMsgHeader_t hdr = proto_context.hdr;
    hdr.m_cubProtoBufExtHdr = static_cast<uint32>(protohdr.ByteSizeLong());
    ser_var<ProtoBufMsgHeader_t>(updated, hdr);
    protohdr.AppendToString(&updated);
    updated.append(body_ptr, serialized_body_size);
    message.swap(updated);
    return true;
}

bool GBE_RewriteAccountIdVarintInDirectProtoBody(
    std::string &message,
    uint32 account_id,
    size_t &replacement_count)
{
    replacement_count = 0;

    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return false;

    const std::string body(reinterpret_cast<const char *>(proto_context.body), proto_context.body_size);
    std::string rewritten_body;
    if (!gbe::proto_wire::rewrite_varint_bytes_recursive(
            body,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaAccountIdVarint.data(), GBE_kOldDotaAccountIdVarint.size()),
            1u,
            account_id,
            rewritten_body,
            replacement_count))
        return false;

    if (replacement_count == 0)
        return true;

    message.resize(proto_context.body_offset);
    message.append(rewritten_body);
    return true;
}

bool GBE_TryPatchDotaAccountIdVarint(
    std::string &message,
    uint32 account_id,
    const char *log_scope,
    uint32 request_emsg,
    uint32 response_emsg,
    size_t body_size,
    const char *context_note)
{
    std::string encoded_account_raw;
    gbe::proto_wire::append_varuint(encoded_account_raw, account_id);

    std::string rewritten_message;
    size_t replacement_count = 0;
    const bool full_message_parse_ok = gbe::proto_wire::rewrite_varint_bytes_recursive(
            message,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaAccountIdVarint.data(), GBE_kOldDotaAccountIdVarint.size()),
            1u,
            account_id,
            rewritten_message,
            replacement_count);
    if (full_message_parse_ok && replacement_count > 0) {
        message.swap(rewritten_message);
        GBE_GC_DebugLog(
            log_scope,
            "rewrote semantic account_id varint req=%u resp=%u body_size=%zu note=%s account_id=%u encoded_size=%zu donor_size=%zu replacements=%zu target_field=%u scope=full_message",
            request_emsg,
            response_emsg,
            body_size,
            context_note ? context_note : "",
            account_id,
            encoded_account_raw.size(),
            GBE_kOldDotaAccountIdVarint.size(),
            replacement_count,
            1u
        );
        return true;
    }

    size_t direct_body_replacements = 0;
    if (GBE_RewriteAccountIdVarintInDirectProtoBody(message, account_id, direct_body_replacements) && direct_body_replacements > 0) {
        GBE_GC_DebugLog(
            log_scope,
            "rewrote semantic account_id varint req=%u resp=%u body_size=%zu note=%s account_id=%u encoded_size=%zu donor_size=%zu replacements=%zu target_field=%u scope=direct_body",
            request_emsg,
            response_emsg,
            body_size,
            context_note ? context_note : "",
            account_id,
            encoded_account_raw.size(),
            GBE_kOldDotaAccountIdVarint.size(),
            direct_body_replacements,
            1u
        );
        return true;
    }

    if (!full_message_parse_ok) {
        GBE_GC_DebugLog(
            log_scope,
            "account_id semantic rewrite skipped full parse req=%u resp=%u note=%s account_id=%u encoded_size=%zu donor_size=%zu",
            request_emsg,
            response_emsg,
            context_note ? context_note : "",
            account_id,
            encoded_account_raw.size(),
            GBE_kOldDotaAccountIdVarint.size()
        );
    }

    GBE_GC_DebugLog(
        log_scope,
        "no semantic account_id varint replacements req=%u resp=%u body_size=%zu note=%s account_id=%u encoded_size=%zu donor_size=%zu target_field=%u",
        request_emsg,
        response_emsg,
        body_size,
        context_note ? context_note : "",
        account_id,
        encoded_account_raw.size(),
        GBE_kOldDotaAccountIdVarint.size(),
        1u
    );
    return true;
}

bool GBE_TryPatchDotaAccountIdFixed32(std::string &message, uint32 account_id, const char *log_scope)
{
    const std::vector<uint8> old_account_id_fixed32 = gbe::proto_wire::vector_from_bytes(GBE_kOldDotaAccountIdFixed32.data(), GBE_kOldDotaAccountIdFixed32.size());
    size_t match_count = 0;
    if (!gbe::proto_wire::patch_fixed32_template_value(message, old_account_id_fixed32, account_id, match_count)) {
        GBE_GC_DebugLog(log_scope, "failed replacing account_id fixed32 bytes account_id=%u matches=%zu", account_id, match_count);
        return false;
    }

    return true;
}

bool GBE_PatchDotaTemplateIdentifiers(
    std::string &message,
    uint32 account_id,
    uint64 steam_id,
    bool replace_account,
    bool replace_steam_id,
    uint32 request_emsg,
    uint32 response_emsg,
    size_t body_size,
    const char *context_note)
{
    if (replace_account) {
        if (!GBE_TryPatchDotaAccountIdVarint(message, account_id, "GC_DOTA_PATCH", request_emsg, response_emsg, body_size, context_note))
            return false;
        if (!GBE_TryPatchDotaAccountIdFixed32(message, account_id, "GC_DOTA_PATCH"))
            return false;
    }

    if (replace_steam_id) {
        const std::vector<uint8> old_steam_id_varint = gbe::proto_wire::vector_from_bytes(GBE_kOldDotaSteamIdVarint.data(), GBE_kOldDotaSteamIdVarint.size());
        size_t steam_id_match_count = 0;
        bool steam_id_size_ok = false;
        if (!gbe::proto_wire::patch_varint_template_value(message, old_steam_id_varint, steam_id, steam_id_match_count, steam_id_size_ok)) {
            if (!steam_id_size_ok) {
                GBE_GC_DebugLog(
                    "GC_DOTA_PATCH",
                    "steam_id template rewrite skipped due to size mismatch req=%u resp=%u note=%s steam_id=%llu encoded_expected=%zu",
                    request_emsg,
                    response_emsg,
                    context_note ? context_note : "",
                    static_cast<unsigned long long>(steam_id),
                    GBE_kOldDotaSteamIdVarint.size());
                return false;
            }

            if (steam_id_match_count == 0) {
                GBE_GC_DebugLog(
                    "GC_DOTA_PATCH",
                    "steam_id template rewrite skipped; donor does not expose expected varint req=%u resp=%u note=%s steam_id=%llu expected_size=%zu",
                    request_emsg,
                    response_emsg,
                    context_note ? context_note : "",
                    static_cast<unsigned long long>(steam_id),
                    GBE_kOldDotaSteamIdVarint.size());
            } else {
                return false;
            }
        }
    }

    return true;
}

bool GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage(
    const char *template_hex,
    uint64 steam_id,
    uint64 lobby_id,
    uint64 server_id,
    bool patch_server_id,
    std::string &message)
{
    if (!gbe::proto_wire::decode_hex_string(template_hex, message))
        return false;

    const std::vector<std::string> old_lobby_id_texts = {
        GBE_kOldDotaPracticeLobbyLobbyIdText,
        GBE_kOldDotaPracticeLobbyLobbyIdTextAlt,
    };
    gbe::proto_wire::DotaPracticeLobbyPeripheralTemplatePatchResult patch_result{};
    if (!gbe::proto_wire::patch_dota_practice_lobby_peripheral_template(
            message,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaSteamIdFixed64.data(), GBE_kOldDotaSteamIdFixed64.size()),
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaPersonaSteamIdFixed64.data(), GBE_kOldDotaPersonaSteamIdFixed64.size()),
            steam_id,
            patch_server_id,
            gbe::proto_wire::vector_from_bytes(GBE_kOldDotaPracticeLobbyServerIdFixed64.data(), GBE_kOldDotaPracticeLobbyServerIdFixed64.size()),
            server_id,
            old_lobby_id_texts,
            lobby_id,
            patch_result))
        return false;

    return true;
}

bool GBE_PrepareDotaPersonaStatePeripheralMessage(
    const char *template_hex,
    uint64 steam_id,
    uint64 lobby_id,
    std::string &message)
{
    return GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage(template_hex, steam_id, lobby_id, 0u, false, message);
}
