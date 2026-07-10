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

// Template-replay handler for the Dota Game Coordinator. Extracted from
// gbe_dota_handlers.cpp (Phase 3.1.5b) to isolate the canned-response switch
// that emits pre-built payloads for a fixed set of legacy request emsgs.
//
// Responsibility boundary: owns the template-replay switch table and its 12
// handler-local static template byte arrays / hex strings. Side-effect
// ownership and ordering are unchanged from the prior monolithic handler file;
// only the file location moved. All template statics moved with the handler
// (List X, kept `static` in new TU); no cross-TU externalization was needed
// (List Y = 0).

#include "dll/steam_game_coordinator.h"
#include "dll/dll.h"
#include "gbe_dota_protocol_constants.h"
#include "gbe_dota_request_router.h"
#include "gbe_dota_custom_game.h"
#include "gbe_dota_gc_router.h"
#include "gbe_dota_lobby_flow.h"
#include "gbe_dota_lobby_state_store.h"
#include "gbe_dota_payload_item_helpers.h"
#include "gbe_gc_message_utils.h"
#include "gbe_proto_wire.h"
#include "dll/gbe_dota_reconnect_shared.h"
#include "dll/gbe_dota_unlock_items.h"
#include "gbe_dota_gc_internal.h"
#include "gbe_dota_payload_lobby_helpers.h"
#include "gbe_dota_payload_wire_helpers.h"
#include <algorithm>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cstring>
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


static constexpr const char *GBE_kDota8730TemplateHex =
    "1a22008009000000594600000000000000080112100a0e088ea83d1206cfb9cfb9d48c18001200"
    "120f0a0d08e0d61f1205313773656318001200120d0a0b0884b0331203534b2b180012120a1008"
    "f7a9021208d09fd090d09fd090180012160a1408bfcb03120ce58fb2e4b88ae69c80e5bcba1800"
    "12100a0e08ded42a120632306d6dd1801800120012100a0e08f3b03a1206e6a097e889b2180012"
    "0f0a0d08a9b63b12055b3939395d1800120e0a0c088ca4011204636963611800120f0a0d0884f0"
    "19120575796b75791800120d0a0b08d9d20612035246351800120012160a14088d851b120ce981"
    "97e8bfb9e58fb2e8af971800120e0a0c08b6e8261204486c6c45180012120a1008d2fe1e1208e2"
    "969a5450e29784180012140a1208ca8737120ad184d188d0b8d181d0bf1800120f0a0d08e1fe3e"
    "12055f6e415353180012190a170888ba28120fe99693e98195e38184e381aae381841800120012"
    "190a170884c206120fe4bda0e694bee5ada6e588abe8b5b0180012110a0f08dcb60f1207e299a5"
    "20e299821800120f0a0d08b3b73e12056e756d6239180012120a1008b1d5281208d0a1d09bd090"
    "d0911800120f0a0d08b1940112054d4f4c43481800120012120a1008acc12912052d5552412d18"
    "8080800112130a1108e9c9031209e385a4e29885e385a41800120f0a0d08c9b03c120557455853"
    "531800120f0a0d08d4e223120553742e203118001200120f0a0d08bcc70d120543687672731800"
    "120e0a0c08c2d2171204c2a17a2118001200120f0a0d08dfb91a1205545072737218001200";


static constexpr const char *GBE_kDota8331TemplateHex =
    "4d15008014000000090eac2b7cdec1400110dbc3fdeafdffffffff0108ba04108bc18080081a138b200080090000005917000000000000000804";


static constexpr const char *GBE_kDotaOfficial8745TemplateHex =
    "4d15008014000000090eac2b7cdec1400110dbc3fdeafdffffffff0108ba0410a9c48080081a112922008009000000592100000000000000";


static const uint8 GBE_kDota8678Template[] = {
    0xE6, 0x21, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x0A, 0x00,
};


static const uint8 GBE_kDota8136Template[] = {
    0xC8, 0x1F, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x59, 0x03, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00,
};


static const uint8 GBE_kDota2538Template[] = {
    0xEA, 0x09, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x08, 0x00, 0x10, 0xFF, 0xFF, 0xFF, 0xFF, 0x0F,
};


static const uint8 GBE_kDota2618Template[] = {
    0x3A, 0x0A, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x59, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF,
    0xFF, 0x08, 0x01,
};


static const uint8 GBE_kDota8674Template[] = {
    0xE2, 0x21, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x59, 0x02, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x05,
};


static const uint8 GBE_kDota8677Template[] = {
    0xE5, 0x21, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x59, 0x08, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x01, 0x12, 0x00,
};


static const uint8 GBE_kDota7198Template[] = {
    0x1E, 0x1C, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x38, 0x95, 0x03, 0x38, 0xE3, 0x04, 0x38, 0xA1,
    0x1F, 0x38, 0xB4, 0x16, 0x38, 0x00, 0x38, 0xBF, 0x03, 0x38, 0xD1, 0x03, 0x38, 0xAA, 0x36, 0x38,
    0xF3, 0x1C, 0x38, 0xA5, 0x01, 0x38, 0x30, 0x38, 0xEB, 0x08, 0x38, 0xB5, 0x02, 0x38, 0x4E, 0x38,
    0xE1, 0x03, 0x38, 0xCF, 0x07, 0x38, 0x6B, 0x38, 0xB5, 0x04, 0x38, 0xE2, 0x07, 0x38, 0xDE, 0x06,
    0x38, 0xB7, 0x09, 0x38, 0x00, 0x38, 0x00, 0x38, 0x00, 0x38, 0x00, 0x38, 0xD8, 0x02, 0x42, 0x09,
    0x08, 0x95, 0x03, 0x10, 0x00, 0x18, 0x00, 0x20, 0x00, 0x42, 0x09, 0x08, 0xE3, 0x04, 0x10, 0x00,
    0x18, 0x00, 0x20, 0x00, 0x42, 0x09, 0x08, 0xA1, 0x1F, 0x10, 0x00, 0x18, 0x00, 0x20, 0x22, 0x42,
    0x0A, 0x08, 0xB4, 0x16, 0x10, 0x06, 0x18, 0x00, 0x20, 0xBC, 0x01, 0x42, 0x00, 0x42, 0x09, 0x08,
    0xBF, 0x03, 0x10, 0x00, 0x18, 0x00, 0x20, 0x00, 0x42, 0x09, 0x08, 0xD1, 0x03, 0x10, 0x00, 0x18,
    0x00, 0x20, 0x00, 0x42, 0x09, 0x08, 0xAA, 0x36, 0x10, 0x00, 0x18, 0x00, 0x20, 0x6C, 0x42, 0x09,
    0x08, 0xF3, 0x1C, 0x10, 0x00, 0x18, 0x00, 0x20, 0x00, 0x42, 0x09, 0x08, 0xA5, 0x01, 0x10, 0x00,
    0x18, 0x00, 0x20, 0x0E, 0x42, 0x08, 0x08, 0x30, 0x10, 0x00, 0x18, 0x00, 0x20, 0x00, 0x42, 0x0A,
    0x08, 0xEB, 0x08, 0x10, 0x00, 0x18, 0x00, 0x20, 0xA0, 0x02, 0x42, 0x0A, 0x08, 0xB5, 0x02, 0x10,
    0x00, 0x18, 0x00, 0x20, 0x80, 0x02, 0x42, 0x08, 0x08, 0x4E, 0x10, 0x00, 0x18, 0x00, 0x20, 0x3E,
    0x42, 0x09, 0x08, 0xE1, 0x03, 0x10, 0x00, 0x18, 0x00, 0x20, 0x00, 0x42, 0x09, 0x08, 0xCF, 0x07,
    0x10, 0x00, 0x18, 0x00, 0x20, 0x00, 0x42, 0x09, 0x08, 0x6B, 0x10, 0x00, 0x18, 0x00, 0x20, 0x92,
    0x01, 0x42, 0x0A, 0x08, 0xB5, 0x04, 0x10, 0x00, 0x18, 0x00, 0x20, 0x9A, 0x02, 0x42, 0x0A, 0x08,
    0xE2, 0x07, 0x10, 0x00, 0x18, 0x00, 0x20, 0x9C, 0x02, 0x42, 0x0A, 0x08, 0xDE, 0x06, 0x10, 0x00,
    0x18, 0x00, 0x20, 0xBC, 0x01, 0x42, 0x0A, 0x08, 0xB7, 0x09, 0x10, 0x00, 0x18, 0x00, 0x20, 0xA2,
    0x02, 0x42, 0x08, 0x08, 0x00, 0x10, 0x01, 0x18, 0x02, 0x20, 0x01, 0x42, 0x00, 0x42, 0x00, 0x42,
    0x00, 0x42, 0x0A, 0x08, 0xD8, 0x02, 0x10, 0x00, 0x18, 0x00, 0x20, 0x82, 0x02,
};


static const uint8 GBE_kDota8079Template[] = {
    0x8F, 0x1F, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x59, 0x0F, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x01,
};


static const uint8 GBE_kDota8854Template[] = {
    0x96, 0x22, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x59, 0x0B, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x01, 0x12, 0x20, 0x0A, 0x0E, 0x08, 0x51, 0x10, 0xFF, 0xB9, 0x01, 0x18, 0x04, 0x20,
    0xCD, 0x87, 0xD5, 0xB8, 0x5F, 0x0A, 0x0E, 0x08, 0x54, 0x10, 0xFE, 0xB9, 0x01, 0x18, 0x04, 0x20,
    0xBD, 0xFA, 0xD4, 0xB8, 0x5F,
};


static const uint8 GBE_kDota9024Template[] = {
    0x40, 0x23, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x59, 0x0C, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x01, 0x12, 0x02, 0x18, 0x00,
};


bool Steam_Game_Coordinator::GBE_HandleDotaTemplateReplayRequest(uint32 request_emsg, const uint8 *body, size_t body_size, bool has_source_job, uint64 source_job) {
    const GBE_DotaLootListData &loot_data = GBE_GetDotaVpkLootData();
    const uint8 *template_bytes = nullptr;
    size_t template_size = 0;
    const char *template_hex = nullptr;
    uint32 response_emsg = 0;
    bool replace_account = false;
    bool replace_steam_id = false;
    const char *response_note = "";

    switch (request_emsg) {
        case 2536:
            template_bytes = GBE_kDota2538Template;
            template_size = sizeof(GBE_kDota2538Template);
            response_emsg = 2538;
            response_note = "2536->2538";
            break;
        case 2617:
            template_bytes = GBE_kDota2618Template;
            template_size = sizeof(GBE_kDota2618Template);
            response_emsg = 2618;
            response_note = "2617->2618";
            break;
        case 8137:
            template_bytes = GBE_kDota8136Template;
            template_size = sizeof(GBE_kDota8136Template);
            response_emsg = 8136;
            response_note = "8137->8136";
            break;
        case 8673:
            template_bytes = GBE_kDota8674Template;
            template_size = sizeof(GBE_kDota8674Template);
            response_emsg = 8674;
            response_note = "8673->8674";
            break;
        case 7197:
            template_bytes = GBE_kDota7198Template;
            template_size = sizeof(GBE_kDota7198Template);
            response_emsg = 7198;
            response_note = "7197->7198";
            break;
        case 8729:
            template_hex = GBE_kDota8730TemplateHex;
            response_emsg = 8730;
            response_note = "8729->8730";
            break;
        case 8744:
            template_hex = GBE_kDotaOfficial8745TemplateHex;
            response_emsg = 8745;
            response_note = "8744->8745";
            break;
        case 8330:
            template_hex = GBE_kDota8331TemplateHex;
            response_emsg = 8331;
            response_note = "8330->8331";
            break;
        case 8676:
            template_bytes = GBE_kDota8677Template;
            template_size = sizeof(GBE_kDota8677Template);
            response_emsg = 8677;
            response_note = "8676->8677 + 8678 update";
            break;
        case 7387: {
            uint64 profile_selector = 0;
            if (!gbe::proto_wire::read_uint64_field(body, body_size, 1u, profile_selector)) {
                GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed reading 7387 selector body_size=%zu", body_size);
                return true;
            }

            uint64 account_id_field = settings->get_local_steam_id().GetAccountID();
            gbe::proto_wire::read_uint64_field(body, body_size, 2u, account_id_field);

            // Always use minimal response with owned=true for ALL event selectors.
            // Previous code had hardcoded templates for 0x20 (owned=0) and 0x37 (owned=1)
            // which caused items bound to event 0x20 to show "Unavailable".
            {
                std::string response_message;
                if (!gbe::gc_message::build_dota_7388_minimal_response_payload(
                        static_cast<uint32>(profile_selector),
                        static_cast<uint32>(account_id_field),
                        has_source_job,
                        source_job,
                        response_message)) {
                    GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building minimal 7388 selector=%llu", static_cast<unsigned long long>(profile_selector));
                    return true;
                }

                GBE_GC_DebugLog(
                    "GC_DOTA_DIRECT",
                    "replying req=%u resp=%u source_job=%llu size=%zu note=7387 selector=%llu owned=true level=1000 account_id=%u",
                    request_emsg,
                    7388u,
                    static_cast<unsigned long long>(source_job),
                    response_message.size(),
                    static_cast<unsigned long long>(profile_selector),
                    static_cast<unsigned>(account_id_field)
                );
                GBE_PushDotaResponse(7388u, response_message, false, nullptr, "7387_7388");
                return true;
            }
        }
        case 8078:
            template_bytes = GBE_kDota8079Template;
            template_size = sizeof(GBE_kDota8079Template);
            response_emsg = 8079;
            response_note = "8078->8079";
            break;
        case 8853:
            template_bytes = GBE_kDota8854Template;
            template_size = sizeof(GBE_kDota8854Template);
            response_emsg = 8854;
            response_note = "8853->8854";
            break;
        case 9023:
            template_bytes = GBE_kDota9024Template;
            template_size = sizeof(GBE_kDota9024Template);
            response_emsg = 9024;
            response_note = "9023->9024";
            break;
        case 8218: {
            // CMsgClientToGCGiveTip -> CMsgClientToGCGiveTipResponse
            // Return result=0 (success) so tipping works in-game.
            std::string tip_response;
            gbe::gc_message::build_dota_give_tip_response_payload(has_source_job, source_job, tip_response);
            GBE_PushDotaResponse(8219u, tip_response, false, nullptr, "8218_8219");

            GBE_GC_DebugLog("GC_DOTA_DIRECT", "tip request -> success response source_job=%llu", static_cast<unsigned long long>(source_job));
            return true;
        }
        case 8879: {
            // CMsgClientToGCRankRequest -> CMsgGCToClientRankResponse
            // Return empty first, then with rank data (field 2=10000, field 3=10000)
            std::string rank_response;
            gbe::gc_message::build_dota_rank_request_response_payload(has_source_job, source_job, rank_response);
            GBE_PushDotaResponse(8880u, rank_response, false, nullptr, "8879_8880_switch");

            GBE_GC_DebugLog("GC_DOTA_DIRECT", "rank request -> response source_job=%llu", static_cast<unsigned long long>(source_job));
            return true;
        }
        case 8095: {
            // CMsgPlayerConductScorecardRequest -> suppress (don't reply)
            // Not replying prevents misleading conduct scorecard popup.
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "conduct scorecard request suppressed source_job=%llu", static_cast<unsigned long long>(source_job));
            return true;
        }
        case GBE_kDotaCustomGameInfoRequest: {
            uint64 custom_game_id = 0ull;
            gbe::proto_wire::read_uint64_field(body, body_size, 1u, custom_game_id);

            std::string response_message;
            if (!gbe::gc_message::build_dota_custom_game_info_response_payload(custom_game_id, has_source_job, source_job, response_message)) {
                GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, GBE_kDotaCustomGameInfoResponse);
                return true;
            }

            GBE_PushDotaResponse(GBE_kDotaCustomGameInfoResponse, response_message, false, nullptr, "custom_game_info");
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "custom game info -> response custom_game_id=%llu source_job=%llu",
                static_cast<unsigned long long>(custom_game_id),
                static_cast<unsigned long long>(source_job));
            return true;
        }
        case GBE_kDotaJoinableCustomGameModesRequest: {
            const std::vector<GBE_LocalLobby> snapshots = GBE_GetDotaGenericLobbySnapshots("7466_joinable_custom_modes");
            const auto shared_lobby = GBE_GetSharedDotaLobbyStateStore().snapshot();
            std::vector<gbe::gc_message::DotaJoinableCustomGameMode> modes;

            for (const GBE_LocalLobby &snapshot : snapshots) {
                if (!snapshot.active)
                    continue;
                modes.push_back(gbe::gc_message::DotaJoinableCustomGameMode{snapshot.custom_game.game_id, static_cast<uint32>(snapshot.members.size())});
            }
            if (GBE_local_lobby.active)
                modes.push_back(gbe::gc_message::DotaJoinableCustomGameMode{GBE_local_lobby.custom_game.game_id, static_cast<uint32>(GBE_local_lobby.members.size())});
            if (shared_lobby.valid && shared_lobby.active)
                modes.push_back(gbe::gc_message::DotaJoinableCustomGameMode{shared_lobby.custom_game.game_id, static_cast<uint32>(shared_lobby.members.size())});

            std::string response_body;
            const size_t mode_count = gbe::gc_message::build_dota_joinable_custom_game_modes_body(modes, response_body);

            std::string response_message;
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(
                GBE_kDotaJoinableCustomGameModesResponse,
                has_source_job,
                source_job,
                response_body,
                response_message);
            GBE_PushDotaResponse(GBE_kDotaJoinableCustomGameModesResponse, response_message, false, nullptr, "joinable_custom_game_modes");
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "joinable custom game modes -> response modes=%zu source_job=%llu",
                mode_count,
                static_cast<unsigned long long>(source_job));
            return true;
        }
        case GBE_kDotaJoinableCustomLobbiesRequest: {
            uint64 requested_custom_game_id = 0ull;
            gbe::proto_wire::read_uint64_field(body, body_size, 2u, requested_custom_game_id);

            std::vector<GBE_LocalLobby> lobbies = GBE_GetDotaGenericLobbySnapshots("7468_joinable_custom_lobbies");
            if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0ull)
                lobbies.push_back(GBE_local_lobby);

            std::vector<gbe::gc_message::DotaJoinableCustomLobby> joinable_lobbies;
            const uint32 now = static_cast<uint32>(std::time(nullptr));
            for (const GBE_LocalLobby &lobby : lobbies) {
                if (!lobby.active || lobby.lobby_id == 0ull || lobby.custom_game.game_id == 0ull)
                    continue;

                const std::string room_name = lobby.room_name.empty() ? std::string("Lobby") : lobby.room_name;
                const std::string display_name = GBE_DotaCustomGameDisplayName(settings, lobby.custom_game, room_name);
                gbe::gc_message::DotaJoinableCustomLobby parsed_lobby{};
                parsed_lobby.lobby_id = lobby.lobby_id;
                parsed_lobby.custom_game_id = lobby.custom_game.game_id;
                parsed_lobby.display_name = display_name;
                parsed_lobby.member_count = static_cast<uint32>(lobby.members.empty() ? 1u : lobby.members.size());
                parsed_lobby.owner_account_id = lobby.owner_account_id != 0u ? lobby.owner_account_id : settings->get_local_steam_id().GetAccountID();
                parsed_lobby.owner_name = lobby.owner_name.empty() ? std::string(settings->get_local_name()) : lobby.owner_name;
                parsed_lobby.map_name = lobby.custom_game.map_name;
                parsed_lobby.max_players = lobby.custom_game.max_players;
                parsed_lobby.server_region = lobby.server_region;
                parsed_lobby.has_pass_key = !lobby.pass_key.empty();
                parsed_lobby.lan_host_ping_location = lobby.lan_host_ping_location;
                parsed_lobby.created_time = now;
                parsed_lobby.custom_game_timestamp = lobby.custom_game.timestamp;
                parsed_lobby.custom_game_crc = lobby.custom_game.crc;
                parsed_lobby.min_players = lobby.custom_game.min_players;
                parsed_lobby.penalties = lobby.custom_game.penalties;
                joinable_lobbies.push_back(parsed_lobby);
            }

            std::string response_body;
            const size_t lobby_count = gbe::gc_message::build_dota_joinable_custom_lobbies_body(joinable_lobbies, requested_custom_game_id, response_body);

            std::string response_message;
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(
                GBE_kDotaJoinableCustomLobbiesResponse,
                has_source_job,
                source_job,
                response_body,
                response_message);
            GBE_PushDotaResponse(GBE_kDotaJoinableCustomLobbiesResponse, response_message, false, nullptr, "joinable_custom_lobbies");
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "joinable custom lobbies -> response requested_game_id=%llu lobbies=%zu source_job=%llu",
                static_cast<unsigned long long>(requested_custom_game_id),
                lobby_count,
                static_cast<unsigned long long>(source_job));
            return true;
        }
        case GBE_kDotaFindTopSourceTVGames: {
            // CMsgClientToGCFindTopSourceTVGames -> CMsgGCToClientFindTopSourceTVGamesResponse
            // Build response from local shared lobby state OR remote generic lobbies.
            gbe::gc_message::DotaSourceTVGame source_tv_game{};
            bool found_game = false;
            const auto shared_lobby = GBE_GetSharedDotaLobbyStateStore().snapshot();

            // First: check local shared lobby state (we are the host)
            if (!found_game &&
                shared_lobby.valid &&
                shared_lobby.active &&
                shared_lobby.game_state >= 1u &&
                shared_lobby.server_id != 0) {
                source_tv_game.start_time = shared_lobby.game_start_time != 0
                    ? shared_lobby.game_start_time
                    : static_cast<uint32>(std::time(nullptr) - 300);
                source_tv_game.server_id = shared_lobby.server_id;
                source_tv_game.lobby_id = shared_lobby.lobby_id;
                source_tv_game.game_time = shared_lobby.game_start_time != 0
                    ? static_cast<uint32>(std::time(nullptr)) - static_cast<uint32>(shared_lobby.game_start_time)
                    : 300u;
                source_tv_game.game_mode = shared_lobby.game_mode;
                source_tv_game.match_id = shared_lobby.match_id;
                for (const auto &member : shared_lobby.members) {
                    if (member.account_id == 0) continue;
                    source_tv_game.players.push_back(gbe::gc_message::DotaSourceTVPlayer{member.account_id, member.hero_id, member.slot, member.team});
                }
                found_game = true;
            }

            // Second: check remote generic lobbies via matchmaking
            if (!found_game) {
                const std::vector<GBE_LocalLobby> snapshots = GBE_GetDotaGenericLobbySnapshots("8009_find_top_source_tv");
                for (const auto &snap : snapshots) {
                    if (snap.game_state >= 1u && snap.server_id != 0 && snap.allow_spectating) {
                        source_tv_game.start_time = snap.game_start_time != 0
                            ? snap.game_start_time
                            : static_cast<uint32>(std::time(nullptr) - 300);
                        source_tv_game.server_id = snap.server_id;
                        source_tv_game.lobby_id = snap.lobby_id;
                        source_tv_game.game_time = snap.game_start_time != 0
                            ? static_cast<uint32>(std::time(nullptr)) - static_cast<uint32>(snap.game_start_time)
                            : 300u;
                        source_tv_game.game_mode = snap.game_mode;
                        source_tv_game.match_id = snap.match_id;
                        for (const auto &member : snap.members) {
                            if (member.account_id == 0) continue;
                            source_tv_game.players.push_back(gbe::gc_message::DotaSourceTVPlayer{member.account_id, member.hero_id, member.slot, member.team});
                        }
                        found_game = true;
                        break; // Use first matching game
                    }
                }
            }

            std::string response_body;
            if (found_game)
                gbe::gc_message::build_dota_find_top_source_tv_games_body(&source_tv_game, response_body);
            else
                gbe::gc_message::build_dota_find_top_source_tv_games_empty_body(response_body);

            std::string response_message;
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(
                GBE_kDotaFindTopSourceTVGamesResponse,
                has_source_job, source_job,
                response_body, response_message);
            GBE_PushDotaResponse(GBE_kDotaFindTopSourceTVGamesResponse, response_message, false, nullptr, "find_top_source_tv_games");

            GBE_GC_DebugLog("GC_DOTA_DIRECT",
                "FindTopSourceTVGames -> response found=%d source_job=%llu local_valid=%d",
                found_game ? 1 : 0,
                static_cast<unsigned long long>(source_job),
                shared_lobby.valid ? 1 : 0);
            return true;
        }
        case 7073: {
            // CMsgSpectateFriendGame -> CMsgSpectateFriendGameResponse
            // Sent when a player clicks "Watch Game" on a friend's profile card.
            // Parse target steam_id (field 1, fixed64), find the game server they're on,
            // and return server_steamid + SUCCESS so the client can follow up with 7091.
            //
            // CMsgSpectateFriendGameResponse proto:
            //   field 4: server_steamid (fixed64)
            //   field 5: watch_live_result (varint enum, 0=SUCCESS)
            uint64 spectate_target_steamid = 0;
            {
                size_t pos = 0;
                while (pos < body_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end))
                        break;
                    if (field.number == 1u && field.wire_type == 1u && field.value_size == 8) {
                        memcpy(&spectate_target_steamid, body + field.value_offset, 8);
                    }
                }
            }

            // Find the server_steamid from local shared state or remote lobbies
            uint64 spectate_server_steamid = 0;
            const auto shared_lobby = GBE_GetSharedDotaLobbyStateStore().snapshot();

            if (shared_lobby.valid && shared_lobby.server_id != 0) {
                spectate_server_steamid = shared_lobby.server_id;
            } else {
                const std::vector<GBE_LocalLobby> snapshots = GBE_GetDotaGenericLobbySnapshots("7073_spectate_friend");
                for (const auto &snap : snapshots) {
                    if (snap.game_state >= 1u && snap.server_id != 0) {
                        spectate_server_steamid = snap.server_id;
                        break;
                    }
                }
            }

            if (spectate_server_steamid == 0)
                spectate_server_steamid = GBE_local_lobby.server_id;

            // Build 7074 CMsgSpectateFriendGameResponse
            // Official GC always sends job_id_target = 0xFFFFFFFFFFFFFFFF in the header,
            // even when the request has no source_job. The client requires this to process
            // the response and trigger the follow-up 7091 WatchGame request.
            // Body contains only field 4 (server_steamid); field 5 (watch_live_result)
            // is omitted when SUCCESS (default value 0 is not serialized by official GC).
            {
                std::string resp_body;
                gbe::gc_message::build_dota_spectate_friend_game_response_body(spectate_server_steamid, resp_body);
                std::string response_msg;
                uint64 reply_job = has_source_job ? source_job : 0xFFFFFFFFFFFFFFFFULL;
                gbe::gc_message::build_dota_job_reply_payload(7074u, reply_job, resp_body, response_msg);
                GBE_PushDotaResponse(7074u, response_msg, false, nullptr, "7073_7074");
            }

            GBE_GC_DebugLog("GC_DOTA_DIRECT",
                "spectate friend game -> SUCCESS target=%llu server=0x%llx source_job=%llu",
                static_cast<unsigned long long>(spectate_target_steamid),
                static_cast<unsigned long long>(spectate_server_steamid),
                static_cast<unsigned long long>(source_job));
            return true;
        }
        case 7091: {
            // CMsgWatchGame -> CMsgWatchGameResponse
            // Parse server_steamid from request (field 1, fixed64).
            // Return READY with SourceTV connection info for LAN spectating.
            uint64 watch_server_steamid = 0;
            {
                size_t pos = 0;
                while (pos < body_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end))
                        break;
                    if (field.number == 1u && field.wire_type == 1u && field.value_size == 8) {
                        memcpy(&watch_server_steamid, body + field.value_offset, 8);
                    }
                }
            }

            // Parse SourceTV address from lobby connect string (ip:port -> ip, port+5)
            uint32 source_tv_addr = 0;
            uint32 source_tv_port = 27020; // default SourceTV port
            uint64 tv_secret_code = 0;
            std::string connect_str;
            const auto shared_lobby = GBE_GetSharedDotaLobbyStateStore().snapshot();
            if (shared_lobby.valid && !shared_lobby.connect.empty()) {
                connect_str = shared_lobby.connect;
            } else {
                // Fallback: find connect from remote generic lobbies
                const std::vector<GBE_LocalLobby> snapshots = GBE_GetDotaGenericLobbySnapshots("7091_watch_game");
                for (const auto &snap : snapshots) {
                    if (snap.game_state >= 1u && snap.server_id != 0 && !snap.connect.empty()) {
                        connect_str = snap.connect;
                        if (snap.tv_secret_code != 0)
                            tv_secret_code = snap.tv_secret_code;
                        if (snap.tv_port != 0)
                            source_tv_port = snap.tv_port;
                        break;
                    }
                }
            }
            // If we have local lobby tv_secret_code (host side), use it
            if (tv_secret_code == 0 && GBE_local_lobby.tv_secret_code != 0)
                tv_secret_code = GBE_local_lobby.tv_secret_code;
            if (GBE_local_lobby.tv_port != 0)
                source_tv_port = GBE_local_lobby.tv_port;
            if (!connect_str.empty()) {
                size_t colon = connect_str.find(':');
                std::string ip_str = (colon != std::string::npos) ? connect_str.substr(0, colon) : connect_str;
                if (colon != std::string::npos) {
                    uint32 game_port = static_cast<uint32>(std::strtoul(connect_str.c_str() + colon + 1, nullptr, 10));
                    if (game_port > 0) source_tv_port = game_port + 5;
                }
                // Convert IP string to uint32 (network byte order as stored by Dota)
                unsigned int a = 0, b = 0, c = 0, d = 0;
                if (std::sscanf(ip_str.c_str(), "%u.%u.%u.%u", &a, &b, &c, &d) == 4) {
                    source_tv_addr = (a << 24) | (b << 16) | (c << 8) | d;
                }
            }

            // First response: PENDING (field 1 = 0)
            // Official GC sends job_id_target = 0xFFFFFFFFFFFFFFFF in the header.
            {
                std::string pending_body;
                gbe::gc_message::build_dota_watch_game_pending_response_body(pending_body);
                std::string pending_msg;
                uint64 pending_reply_job = has_source_job ? source_job : 0xFFFFFFFFFFFFFFFFULL;
                gbe::gc_message::build_dota_job_reply_payload(7092u, pending_reply_job, pending_body, pending_msg);
                push_incoming_now(7092u | GBE_kProtoMask, pending_msg);
            }

            const CSteamID local_steam_id = settings->get_local_steam_id();
            const uint32 local_account_id = local_steam_id.GetAccountID();

            // Second response: READY with server info
            {
                // Use real tv_secret_code from host's 4508 if available, otherwise fallback
                uint64 secret_code = (tv_secret_code != 0) ? tv_secret_code : (watch_server_steamid ^ 0x0514D449EDC24001ULL);
                std::string ready_body;
                gbe::gc_message::build_dota_watch_game_ready_response_body(source_tv_addr, source_tv_port, watch_server_steamid, secret_code, ready_body);
                std::string ready_msg;
                gbe::gc_message::build_dota_job_reply_or_zero_header_payload(7092u, false, 0, ready_body, ready_msg);
                push_incoming_now(7092u | GBE_kProtoMask, ready_msg);
            }

            GBE_GC_DebugLog("GC_DOTA_DIRECT", "watch game request -> READY server=0x%llx tv_addr=0x%x tv_port=%u local_account=%u local_steamid=%llu raw_tv_secret=0x%llx sent_secret=0x%llx source_job=%llu",
                static_cast<unsigned long long>(watch_server_steamid),
                source_tv_addr, source_tv_port,
                local_account_id,
                static_cast<unsigned long long>(local_steam_id.ConvertToUint64()),
                static_cast<unsigned long long>(tv_secret_code),
                static_cast<unsigned long long>(tv_secret_code != 0 ? tv_secret_code : (watch_server_steamid ^ 0x0514D449EDC24001ULL)),
                static_cast<unsigned long long>(source_job));
            return true;
        }
        case 8209: {
            // CMsgDOTAClaimEventAction -> CMsgDOTAClaimEventActionResponse
            // Parse event_id (field 1) and action_id (field 2) from request.
            // Return result=Success(0) with the action_id echoed back.
            uint32 claim_event_id = 0;
            uint32 claim_action_id = 0;
            {
                size_t pos = 0;
                while (pos < body_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end))
                        break;
                    if (field.number == 1u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        claim_event_id = static_cast<uint32>(v);
                    }
                    if (field.number == 2u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        claim_action_id = static_cast<uint32>(v);
                    }
                }
            }

            // Build CMsgDOTAClaimEventActionResponse:
            // field 1 = result (varint, 0=Success)
            // field 2 = reward_results (repeated, empty for now)
            // field 3 = action_id (varint)
            std::string claim_body;
            gbe::gc_message::build_dota_claim_event_action_response_body(claim_action_id, claim_body);

            std::string claim_response;
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(8210u, has_source_job, source_job, claim_body, claim_response);
            GBE_PushDotaResponse(8210u, claim_response, false, nullptr, "8209_8210");

            GBE_GC_DebugLog("GC_DOTA_DIRECT", "claim event action -> success event=%u action=%u source_job=%llu",
                claim_event_id, claim_action_id, static_cast<unsigned long long>(source_job));
            return true;
        }
        case 2510: {
            // StorePurchaseInit - client wants to buy an item from the store.
            // Parse the request to get item_def_id, then respond with success
            // and grant the item to the player's inventory.

            // Parse line_items (field 4) to get item_def_id
            uint32_t purchased_def = 0;
            uint32_t purchased_qty = 1;
            {
                size_t pos = 0;
                while (pos < body_size) {
                    uint8_t tag = body[pos];
                    uint8_t field_num = tag >> 3;
                    uint8_t wire_type = tag & 0x07;
                    pos++;
                    if (wire_type == 0) { // varint
                        while (pos < body_size && (body[pos] & 0x80)) pos++;
                        pos++;
                    } else if (wire_type == 2) { // length-delimited
                        uint32_t len = 0;
                        uint8_t shift = 0;
                        while (pos < body_size) {
                            uint8_t b = body[pos++];
                            len |= (b & 0x7F) << shift;
                            shift += 7;
                            if (!(b & 0x80)) break;
                        }
                        if (field_num == 4 && len > 0 && pos + len <= body_size) {
                            // Parse CGCStorePurchaseInit_LineItem sub-message
                            const uint8_t *sub = body + pos;
                            size_t sub_pos = 0;
                            while (sub_pos < len) {
                                uint8_t st = sub[sub_pos];
                                uint8_t sf = st >> 3;
                                uint8_t sw = st & 0x07;
                                sub_pos++;
                                if (sw == 0) {
                                    uint32_t val = 0;
                                    uint8_t s2 = 0;
                                    while (sub_pos < len) {
                                        uint8_t b = sub[sub_pos++];
                                        val |= (b & 0x7F) << s2;
                                        s2 += 7;
                                        if (!(b & 0x80)) break;
                                    }
                                    if (sf == 1) purchased_def = val;
                                    else if (sf == 2) purchased_qty = val;
                                } else if (sw == 2) {
                                    uint32_t sl = 0;
                                    uint8_t s2 = 0;
                                    while (sub_pos < len) {
                                        uint8_t b = sub[sub_pos++];
                                        sl |= (b & 0x7F) << s2;
                                        s2 += 7;
                                        if (!(b & 0x80)) break;
                                    }
                                    sub_pos += sl;
                                } else {
                                    break;
                                }
                            }
                        }
                        pos += len;
                    } else {
                        break;
                    }
                }
            }

            if (purchased_def == 0) {
                GBE_GC_DebugLog("GC_DOTA_DIRECT", "StorePurchaseInit failed to parse item_def_id body_size=%zu", body_size);
                return true;
            }

            // Build StorePurchaseInitResponse (msg 2511): result=1, txn_id=fake
            {
                std::string resp_body;
                uint64_t fake_txn = (static_cast<uint64_t>(0xBEEF0000u) | purchased_def);
                gbe::gc_message::build_dota_store_purchase_init_response_body(fake_txn, resp_body);

                std::string response_message;
                gbe::gc_message::build_dota_job_reply_or_zero_header_payload(2511u, has_source_job, source_job, resp_body, response_message);
                push_incoming_now(2511u | GBE_kProtoMask, response_message);
            }

            // Grant the purchased item to inventory
            {
                CSteamID player_steam_id = settings->get_local_steam_id();
                uint32_t account_id = player_steam_id.GetAccountID();

                for (uint32_t qi = 0; qi < purchased_qty; qi++) {
                    uint32_t seq = static_cast<uint32_t>(items.size()) + 1;
                    uint64_t new_item_id = (static_cast<uint64_t>(0x50000000u + seq) << 32ull) | static_cast<uint64_t>(account_id);

                    Econ_Item item;
                    item.id = new_item_id;
                    item.def = purchased_def;
                    item.level = 1;
                    item.quality = static_cast<EItemQuality>(4);
                    item.inv_pos = seq;
                    item.quantity = 1;
                    item.flags = 0;
                    item.origin = 2; // purchased
                    item.in_use = false;
                    item.original_id = new_item_id;
                    item.style = 0;
                    items.push_back(item);

                    // Notify client via SOCreate (msg 21 = k_ESOMsg_Create)
                    std::string create_body;
                    GBE_BuildSOSingleObjectFromItem(item, player_steam_id, create_body);
                    std::string create_response;
                    gbe::gc_message::build_dota_zero_header_payload(21u, create_body, create_response);
                    push_incoming_now(21u | GBE_kProtoMask, create_response);
                }

                save_items_to_file();
            }

            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "StorePurchaseInit success def=%u qty=%u source_job=%llu",
                purchased_def, purchased_qty,
                static_cast<unsigned long long>(source_job)
            );
            return true;
        }
        case 1092: {
            // k_EMsgGCRequestCrateItems -> k_EMsgGCRequestCrateItemsResponse (1093)
            // Client asks what items are in a crate. Parse crate_item_def, look up
            // loot list, return the def_indexes of items in the loot list.
            uint32 crate_def = 0;
            {
                size_t pos = 0;
                while (pos < body_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end)) break;
                    if (field.number == 1u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        crate_def = static_cast<uint32>(v);
                    }
                }
            }
            // Build response with item_defs from loot list
            std::vector<uint32> crate_item_defs;
            // Look up loot list for this treasure
            auto tll_it = loot_data.treasure_to_loot_list.find(crate_def);
            if (tll_it != loot_data.treasure_to_loot_list.end()) {
                auto ll_it = loot_data.loot_lists.find(tll_it->second);
                if (ll_it != loot_data.loot_lists.end()) {
                    for (const auto &entry_name : ll_it->second) {
                        auto def_it = loot_data.name_to_def.find(entry_name);
                        if (def_it != loot_data.name_to_def.end())
                            crate_item_defs.push_back(def_it->second);
                    }
                }
            }
            std::string resp_body;
            gbe::gc_message::build_dota_crate_items_response_body(crate_item_defs, resp_body);
            std::string response_message;
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(1093u, has_source_job, source_job, resp_body, response_message);
            GBE_PushDotaResponse(1093u, response_message, false, nullptr, "1092_1093_crate_items");
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "RequestCrateItems -> success crate_def=%u source_job=%llu",
                crate_def, static_cast<unsigned long long>(source_job));
            return true;
        }
        case 1025: {
            // k_EMsgGCUseItemRequest -> k_EMsgGCUseItemResponse (1026)
            // Client wants to "use" an item (open a treasure).
            // Find the item in inventory, get its def_index, look up loot list,
            // pick a random item, grant it, then respond.
            uint64 use_item_id = 0;
            {
                size_t pos = 0;
                while (pos < body_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end)) break;
                    if (field.number == 1u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        use_item_id = v;
                    } else if (field.number == 1u && field.wire_type == 1u) {
                        if (field.value_offset + 8 <= body_size) memcpy(&use_item_id, body + field.value_offset, 8);
                    }
                }
            }
            // Find item def_index from inventory
            uint32 item_def = 0;
            for (const auto &inv_item : items) {
                if (inv_item.id == use_item_id) { item_def = inv_item.def; break; }
            }
            // Try to open as treasure: look up loot list
            uint32 granted_def = 0;
            std::string resolved_loot_list;
            auto tll_it = loot_data.treasure_to_loot_list.find(item_def);
            if (tll_it != loot_data.treasure_to_loot_list.end()) {
                resolved_loot_list = tll_it->second;
            } else {
                // Try tool.usage.loot_list path (gem packs, gifts, etc.)
                auto tool_it = loot_data.tool_to_loot_list.find(item_def);
                if (tool_it != loot_data.tool_to_loot_list.end()) {
                    resolved_loot_list = tool_it->second;
                }
            }
            if (!resolved_loot_list.empty()) {
                auto ll_it = loot_data.loot_lists.find(resolved_loot_list);
                if (ll_it != loot_data.loot_lists.end() && !ll_it->second.empty()) {
                    // Pick random item from loot list
                    size_t idx = static_cast<size_t>(rand()) % ll_it->second.size();
                    const std::string &chosen_name = ll_it->second[idx];
                    auto def_it = loot_data.name_to_def.find(chosen_name);
                    if (def_it != loot_data.name_to_def.end()) {
                        granted_def = def_it->second;
                    }
                }
            }
            // Grant the item if we found one
            if (granted_def != 0) {
                CSteamID player_steam_id = settings->get_local_steam_id();
                uint32_t account_id = player_steam_id.GetAccountID();
                uint32_t seq = static_cast<uint32_t>(items.size()) + 1;
                uint64_t new_item_id = (static_cast<uint64_t>(0x60000000u + seq) << 32ull) | static_cast<uint64_t>(account_id);

                Econ_Item new_item;
                new_item.id = new_item_id;
                new_item.def = granted_def;
                new_item.level = 1;
                new_item.quality = static_cast<EItemQuality>(4);
                new_item.inv_pos = seq;
                new_item.quantity = 1;
                new_item.flags = 0;
                new_item.origin = 8; // opened from crate
                new_item.in_use = false;
                new_item.original_id = new_item_id;
                new_item.style = 0;
                items.push_back(new_item);

                std::string create_body;
                GBE_BuildSOSingleObjectFromItem(new_item, player_steam_id, create_body);
                std::string create_response;
                gbe::gc_message::build_dota_zero_header_payload(21u, create_body, create_response);
                push_incoming_now(21u | GBE_kProtoMask, create_response);

                save_items_to_file();
            }
            // Response
            std::string resp_body;
            gbe::gc_message::build_dota_use_item_response_body(granted_def != 0, resp_body);
            std::string response_message;
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(1026u, has_source_job, source_job, resp_body, response_message);
            push_incoming_now(1026u | GBE_kProtoMask, response_message);
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "UseItemRequest -> item_id=0x%llx def=%u granted_def=%u source_job=%llu",
                static_cast<unsigned long long>(use_item_id), item_def, granted_def,
                static_cast<unsigned long long>(source_job));
            return true;
        }
        case 2574: {
            // k_EMsgClientToGCUnlockCrate -> k_EMsgClientToGCUnlockCrateResponse (2575)
            // Open a treasure chest. Find crate in inventory, pick random from loot list, grant it.
            uint64 crate_item_id = 0;
            uint64 key_item_id = 0;
            {
                size_t pos = 0;
                while (pos < body_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end)) break;
                    if (field.number == 1u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        crate_item_id = v;
                    } else if (field.number == 2u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        key_item_id = v;
                    }
                }
            }
            // Find crate def_index
            uint32 crate_def = 0;
            for (const auto &inv_item : items) {
                if (inv_item.id == crate_item_id) { crate_def = inv_item.def; break; }
            }
            // Pick random item from loot list
            std::vector<uint32> granted_defs;
            auto tll_it = loot_data.treasure_to_loot_list.find(crate_def);
            if (tll_it != loot_data.treasure_to_loot_list.end()) {
                auto ll_it = loot_data.loot_lists.find(tll_it->second);
                if (ll_it != loot_data.loot_lists.end() && !ll_it->second.empty()) {
                    size_t idx = static_cast<size_t>(rand()) % ll_it->second.size();
                    const std::string &chosen_name = ll_it->second[idx];
                    auto def_it = loot_data.name_to_def.find(chosen_name);
                    if (def_it != loot_data.name_to_def.end()) {
                        granted_defs.push_back(def_it->second);
                    }
                }
            }
            // Grant items
            CSteamID player_steam_id = settings->get_local_steam_id();
            uint32_t account_id = player_steam_id.GetAccountID();
            for (uint32 gdef : granted_defs) {
                uint32_t seq = static_cast<uint32_t>(items.size()) + 1;
                uint64_t new_item_id = (static_cast<uint64_t>(0x61000000u + seq) << 32ull) | static_cast<uint64_t>(account_id);

                Econ_Item new_item;
                new_item.id = new_item_id;
                new_item.def = gdef;
                new_item.level = 1;
                new_item.quality = static_cast<EItemQuality>(4);
                new_item.inv_pos = seq;
                new_item.quantity = 1;
                new_item.flags = 0;
                new_item.origin = 8;
                new_item.in_use = false;
                new_item.original_id = new_item_id;
                new_item.style = 0;
                items.push_back(new_item);

                std::string create_body;
                GBE_BuildSOSingleObjectFromItem(new_item, player_steam_id, create_body);
                std::string create_response;
                gbe::gc_message::build_dota_zero_header_payload(21u, create_body, create_response);
                push_incoming_now(21u | GBE_kProtoMask, create_response);
            }
            if (!granted_defs.empty()) save_items_to_file();
            // Build response with granted items
            std::string resp_body;
            gbe::gc_message::build_dota_unlock_crate_response_body(granted_defs, resp_body);
            std::string response_message;
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(2575u, has_source_job, source_job, resp_body, response_message);
            push_incoming_now(2575u | GBE_kProtoMask, response_message);
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "UnlockCrate -> OK crate_id=0x%llx crate_def=%u granted=%zu source_job=%llu",
                static_cast<unsigned long long>(crate_item_id), crate_def,
                granted_defs.size(), static_cast<unsigned long long>(source_job));
            return true;
        }
        case 2576: {
            // k_EMsgClientToGCUnpackBundle -> k_EMsgClientToGCUnpackBundleResponse (2567)
            // Unpack a bundle: grant all contained items.
            uint64 bundle_item_id = 0;
            {
                size_t pos = 0;
                while (pos < body_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end)) break;
                    if (field.number == 1u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        bundle_item_id = v;
                    }
                }
            }
            // Find bundle def_index
            uint32 bundle_def = 0;
            for (const auto &inv_item : items) {
                if (inv_item.id == bundle_item_id) { bundle_def = inv_item.def; break; }
            }
            // Look up bundle contents and grant all items
            std::vector<uint32> granted_defs;
            auto bc_it = loot_data.bundle_contents.find(bundle_def);
            if (bc_it != loot_data.bundle_contents.end()) {
                for (const auto &item_name : bc_it->second) {
                    auto def_it = loot_data.name_to_def.find(item_name);
                    if (def_it != loot_data.name_to_def.end()) {
                        granted_defs.push_back(def_it->second);
                    }
                }
            }
            // Grant items
            CSteamID player_steam_id2 = settings->get_local_steam_id();
            uint32_t account_id2 = player_steam_id2.GetAccountID();
            for (uint32 gdef : granted_defs) {
                uint32_t seq = static_cast<uint32_t>(items.size()) + 1;
                uint64_t new_item_id = (static_cast<uint64_t>(0x62000000u + seq) << 32ull) | static_cast<uint64_t>(account_id2);

                Econ_Item new_item;
                new_item.id = new_item_id;
                new_item.def = gdef;
                new_item.level = 1;
                new_item.quality = static_cast<EItemQuality>(4);
                new_item.inv_pos = seq;
                new_item.quantity = 1;
                new_item.flags = 0;
                new_item.origin = 8;
                new_item.in_use = false;
                new_item.original_id = new_item_id;
                new_item.style = 0;
                items.push_back(new_item);

                std::string create_body;
                GBE_BuildSOSingleObjectFromItem(new_item, player_steam_id2, create_body);
                std::string create_response;
                gbe::gc_message::build_dota_zero_header_payload(21u, create_body, create_response);
                push_incoming_now(21u | GBE_kProtoMask, create_response);
            }
            if (!granted_defs.empty()) save_items_to_file();
            // Response
            std::string resp_body;
            gbe::gc_message::build_dota_unpack_bundle_response_body(granted_defs, resp_body);
            std::string response_message;
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(2567u, has_source_job, source_job, resp_body, response_message);
            push_incoming_now(2567u | GBE_kProtoMask, response_message);
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "UnpackBundle -> Succeeded bundle_id=0x%llx bundle_def=%u granted=%zu source_job=%llu",
                static_cast<unsigned long long>(bundle_item_id), bundle_def,
                granted_defs.size(), static_cast<unsigned long long>(source_job));
            return true;
        }
        case 8260: {
            // k_EMsgClientToGCClaimEventActionUsingItem -> k_EMsgClientToGCClaimEventActionUsingItemResponse (8261)
            // Similar to 8209 but uses an item. Parse event_id, action_id, item_id.
            uint32 claim_event_id = 0;
            uint32 claim_action_id = 0;
            uint64 claim_item_id = 0;
            {
                size_t pos = 0;
                while (pos < body_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(body, body_size, pos, field, &field_offset, &field_end)) break;
                    if (field.number == 1u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        claim_event_id = static_cast<uint32>(v);
                    } else if (field.number == 2u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        claim_action_id = static_cast<uint32>(v);
                    } else if (field.number == 3u && field.wire_type == 0u) {
                        uint64 v = 0; size_t tmp = field.value_offset;
                        gbe::proto_wire::read_varuint(body, body_size, tmp, v);
                        claim_item_id = v;
                    }
                }
            }
            // CMsgClientToGCClaimEventActionUsingItemResponse:
            // field 1 = action_results (CMsgDOTAClaimEventActionResponse sub-message)
            //   sub field 1 = result (0=Success)
            //   sub field 3 = action_id
            std::string resp_body;
            gbe::gc_message::build_dota_claim_event_action_using_item_response_body(claim_action_id, resp_body);
            std::string response_message;
            gbe::gc_message::build_dota_job_reply_or_zero_header_payload(8261u, has_source_job, source_job, resp_body, response_message);
            GBE_PushDotaResponse(8261u, response_message, false, nullptr, "8260_8261");
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "ClaimEventActionUsingItem -> success event=%u action=%u item=0x%llx source_job=%llu",
                claim_event_id, claim_action_id,
                static_cast<unsigned long long>(claim_item_id),
                static_cast<unsigned long long>(source_job));
            return true;
        }
        default:
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "no replay template req=%u source_job=%llu body_size=%zu body_prefix=%s",
                request_emsg,
                static_cast<unsigned long long>(source_job),
                body_size,
                gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(body), body_size, 32).c_str()
            );
            return false;
    }

    std::string decoded_template;
    if (template_hex != nullptr) {
        if (!gbe::proto_wire::decode_hex_string(template_hex, decoded_template)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed decoding replay template req=%u resp=%u", request_emsg, response_emsg);
            return true;
        }

        template_bytes = reinterpret_cast<const uint8 *>(decoded_template.data());
        template_size = decoded_template.size();
    }

    const bool mirror_source_job_to_target = has_source_job;

    std::string response_message;
    if (!GBE_PrepareDotaDirectReplayMessage(
            template_bytes,
            template_size,
            settings->get_local_steam_id().GetAccountID(),
            settings->get_local_steam_id().ConvertToUint64(),
            replace_account,
            replace_steam_id,
            mirror_source_job_to_target,
            source_job,
            request_emsg,
            response_emsg,
            body_size,
            response_note,
            response_message)) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building replay req=%u resp=%u", request_emsg, response_emsg);
        return true;
    }

    if (request_emsg == 8744u) {
        size_t response_body_offset = 8u;
        if (response_message.size() >= 8u) {
            uint32 response_header_length = 0;
            std::memcpy(&response_header_length, response_message.data() + 4, sizeof(response_header_length));
            response_body_offset += response_header_length;
        }
        const uint8 *response_body = response_body_offset <= response_message.size()
            ? reinterpret_cast<const uint8 *>(response_message.data() + response_body_offset)
            : nullptr;
        const size_t response_body_size = response_body_offset <= response_message.size()
            ? (response_message.size() - response_body_offset)
            : 0u;

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "observed resp=%u for req=%u source_job=%llu body_size=%zu fields=%s body_prefix=%s",
            response_emsg,
            request_emsg,
            static_cast<unsigned long long>(source_job),
            response_body_size,
            gbe::proto_wire::format_top_level_field_summary(response_body, response_body_size).c_str(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(response_body), response_body_size, 32).c_str()
        );
    }

    GBE_GC_DebugLog("GC_DOTA_DIRECT", "replying req=%u resp=%u source_job=%llu size=%zu note=%s", request_emsg, response_emsg, static_cast<unsigned long long>(source_job), response_message.size(), response_note);
    GBE_PushDotaResponse(response_emsg, response_message, false, nullptr, response_note);

    if (request_emsg == 8676) {
        std::string followup_message;
        if (!GBE_PrepareDotaDirectReplayMessage(
                GBE_kDota8678Template,
                sizeof(GBE_kDota8678Template),
                settings->get_local_steam_id().GetAccountID(),
                settings->get_local_steam_id().ConvertToUint64(),
                false,
                false,
                false,
                0,
                request_emsg,
                8678u,
                body_size,
                "8676 followup 8678",
                followup_message)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building 8678 followup after 8676");
            return true;
        }

        GBE_GC_DebugLog("GC_DOTA_DIRECT", "queueing followup req=%u resp=%u size=%zu note=member.zip unsolicited update", request_emsg, 8678u, followup_message.size());
        GBE_PushDotaResponse(8678u, followup_message, false, nullptr, "8676_followup_8678");
    }

    return true;
}
