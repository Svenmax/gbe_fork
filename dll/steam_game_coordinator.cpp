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
#include <steammessages.pb.h>
#include <tf2/base_gcmessages.pb.h>
#include <tf2/econ_gcmessages.pb.h>
#include <tf2/gcsdk_gcmessages.pb.h>
#include <tf2/gcsystemmsgs.pb.h>
#include <tf2/tf_gcmessages.pb.h>

using namespace gamecoordinator::tf2;

constexpr int GC_MIN_VERSION = 20091217;
bool GBE_PushDotaPlayerEquippedItemsCacheToGC(
    Steam_Game_Coordinator *target_gc,
    const CSteamID &player_steam_id,
    const std::vector<Econ_Item> &source_items,
    bool unsubscribe_first,
    const char *reason);

static void GBE_ComposeDotaPracticeLobbySOObjects(
    uint64 steam_id,
    uint64 lobby_id,
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
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    const GBE_DotaCustomGameDetails *custom_game,
    gbe::gc_message::DotaPracticeLobbyObjects &lobby_objects);

struct GBE_DotaGenericLobbyEntry {
    CSteamID generic_lobby_id{};
    uint64 dota_lobby_id{};
    uint32 owner_account_id{};
    std::string owner_name;
    std::string room_name;
    uint32 game_mode{};
    uint32 server_region{};
    std::string lan_host_ping_location;
    std::string pass_key;
    uint32 player_count{};
    uint32 max_player_count{};
};

GBE_SharedDotaLobbyState GBE_shared_dota_lobby_state;
bool GBE_recent_dota_reconnect_context_valid = false;
GBE_DotaReconnectContext GBE_recent_dota_reconnect_context{};
bool GBE_pending_dota_normal_signout_finalize_after_25 = false;
uint64 GBE_pending_dota_normal_signout_finalize_lobby_id = 0;
GBE_DotaLootListData GBE_vpk_loot_data;

// --- Dota reconnect shared state ---
std::atomic<bool> GBE_dota_reconnect_eligible{true};

bool GBE_GetDotaReconnectContext(GBE_DotaReconnectContext *out)
{
    if (!out)
        return false;

    if (GBE_shared_dota_lobby_state.valid &&
        GBE_shared_dota_lobby_state.active &&
        (GBE_shared_dota_lobby_state.state >= 2u || GBE_shared_dota_lobby_state.game_state >= 2u) &&
        !GBE_shared_dota_lobby_state.connect.empty() &&
        GBE_shared_dota_lobby_state.server_id != 0) {
        out->server_id = GBE_shared_dota_lobby_state.server_id;
        out->lobby_state = GBE_shared_dota_lobby_state.state;
        out->game_state = GBE_shared_dota_lobby_state.game_state;
        out->custom_game_id = GBE_shared_dota_lobby_state.custom_game.game_id;
        const std::string endpoint = gbe::proto_wire::get_dota_practice_lobby_first_connect_endpoint(GBE_shared_dota_lobby_state.connect);
        std::strncpy(out->connect, endpoint.c_str(), sizeof(out->connect) - 1);
        out->connect[sizeof(out->connect) - 1] = '\0';
        out->owner_steam_id = GBE_shared_dota_lobby_state.owner_steam_id;
        return true;
    }

    if (GBE_recent_dota_reconnect_context_valid &&
        GBE_DotaReconnectContextIsStarted(GBE_recent_dota_reconnect_context) &&
        GBE_recent_dota_reconnect_context.connect[0] != '\0' &&
        GBE_recent_dota_reconnect_context.server_id != 0) {
        *out = GBE_recent_dota_reconnect_context;
        return true;
    }

    return false;
}

bool GBE_IsDotaArcadeLobbyActive()
{
    return GBE_shared_dota_lobby_state.valid &&
        GBE_shared_dota_lobby_state.active &&
        GBE_shared_dota_lobby_state.custom_game.game_id != 0ull;
}

bool GBE_TryRecoverDotaReconnectContextFromGenericLobbies(uint64_t local_steam_id, GBE_DotaReconnectContext *out)
{
    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_game_coordinator)
        return false;

    return steam_client->steam_game_coordinator->GBE_TryRecoverDotaReconnectContextFromGenericLobbies(local_steam_id, out);
}
// --- End Dota reconnect shared state ---

const char *GBE_DescribeDotaLaunchPhase(uint32 phase)
{
    switch (phase) {
        case GBE_kDotaLaunchPhaseRequested:
            return "requested";
        case GBE_kDotaLaunchPhaseSetupSynced:
            return "serversetup_synced";
        case GBE_kDotaLaunchPhaseRunQueued:
            return "run_queued";
        case GBE_kDotaLaunchPhaseLoaded:
            return "loaded";
        default:
            return "none";
    }
}

void GBE_GC_DebugLog(const char *scope, const char *fmt, ...);

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
extern const std::array<uint8, 4> GBE_kOldDotaAccountIdVarint = { 0xF5, 0xED, 0x86, 0x41 };
extern const std::array<uint8, 9> GBE_kOldDotaSteamIdVarint = { 0xF5, 0xED, 0x86, 0xC1, 0x90, 0x80, 0x80, 0x88, 0x01 };
extern const std::array<uint8, 8> GBE_kOldDotaLobbyIdVarint = { 0x9D, 0x97, 0xF8, 0x9E, 0x95, 0xD7, 0xF7, 0x34 };
extern const std::array<uint8, 8> GBE_kOldDotaSteamIdFixed64 = { 0xF5, 0xB6, 0x21, 0x08, 0x01, 0x00, 0x10, 0x01 };
extern const std::array<uint8, 8> GBE_kOldDotaPersonaSteamIdFixed64 = { 0x91, 0x1D, 0xDF, 0x05, 0x01, 0x00, 0x10, 0x01 };
extern const std::array<uint8, 4> GBE_kOldDotaAccountIdFixed32 = { 0xF5, 0xB6, 0x21, 0x08 };
static const std::array<uint8, 8> GBE_kOldDotaPracticeLobbyLobbyIdVarint = { 0x83, 0xCF, 0xA2, 0xB4, 0xA2, 0xFF, 0xF9, 0x34 };
extern const std::array<uint8, 5> GBE_kOldDotaPracticeLobbyMatchIdVarint = { 0xDF, 0xF8, 0xBB, 0xDB, 0x20 };
extern const std::array<uint8, 8> GBE_kOldDotaPracticeLobbyServerIdFixed64 = { 0x01, 0x7C, 0x58, 0xCA, 0x8F, 0xC1, 0x40, 0x01 };
static const std::array<uint8, 5> GBE_kOldDotaPracticeLobbyGameStartTimeVarint = { 0xAE, 0xBB, 0xA3, 0xCF, 0x06 };
static constexpr const char *GBE_kOldDotaPracticeLobbyConnect = "117.157.79.194:27015 10.110.4.21:27015";
extern const char *GBE_kOldDotaPracticeLobbyLobbyIdText = "29809934128949123";
extern const char *GBE_kOldDotaPracticeLobbyLobbyIdTextAlt = "29822498642855090";
extern const uint32 GBE_kSteamTicketAuthComplete = 5429u;
static constexpr const char *GBE_kDotaAbandonPersonaStatePrivateLobbyNoLobbyHex =
    "fe0200800f00000009911ddf050100100110c9dbfdd20408dfe60112d80209911ddf0501001001100118ba04300138017a0a636c6f7665726c6f7665c9010000000000000000fa01140000000000000000000000000000000000000000e802a6c7dacf06f00281c8dacf06f802a6c7dacf06ba0300c1033a02000000000000e20300ba04200a06737461747573121623444f54415f52505f505249564154455f4c4f424259ba04270a0d737465616d5f646973706c6179121623444f54415f52505f505249564154455f4c4f424259ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131ba041e0a057061727479121570617274795f73746174653a20494e5f4d41544348c1040000000000000000c9040000000000000000f80400800500880500980501";
extern const char *GBE_kDotaAbandonPersonaStateInitHex =
    "fe0200800f00000009911ddf050100100110c9dbfdd20408dfe60112a50209911ddf0501001001100118ba04300138017a0a636c6f7665726c6f7665c9010000000000000000fa01140000000000000000000000000000000000000000e802a6c7dacf06f00281c8dacf06f802a6c7dacf06ba0300c1033a02000000000000e20300ba04170a06737461747573120d23444f54415f52505f494e4954ba041e0a0d737465616d5f646973706c6179120d23444f54415f52505f494e4954ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131c1040000000000000000c9040000000000000000f80400800500880500980501";
extern const char *GBE_kDotaOfficial032PracticeLobby26Hex =
    "4d15008014000000090eac2b7cdec1400110dbc3fdeafdffffffff0108ba04109a808080081a80041a00008000000000120508dd0f120012e90108d40f12e30108d6f9ac9f95a6fc34180120022a273138322e34322e3232342e31333a3237303135203139322e3136382e342e3136383a3237303135310eac2b7cdec1400159f5b621080100100160016800700082010531313131318a010240008a01024000a80100b0010ae00100f001bbca9fe020f80100a00203d00200d80200e00200f00200f80200800300980300a80300c80301f2030708f54412020800880400d80400900500b805f7e6cbcf06c00500e80503f00500f80500880600b80600c00637f00600880700c2070d09f5b621080100100118003801c80700f807008008d5e6cbcf06121208de0f120d0a090a075376656e6d61781000120708df0f12020a0012cf0108e00f12c9010a3509f5b62108010010014800580060e1ac8b84d0854068008501000000e085014128d9f585015706000098010098010098010098010015000000001a300813122c08f5ed864110001800200038006000d00100d80100e00100fa0106080f100a180afa0108081c10e80718e8071a1c081a121808f5ed864110001800200138006000d00100d80100e001001a1c0827121808f5ed864110001800200138006000d00100d80100e001001a1d0838121908f5ed864110e8071800200138016000d00100d80100e00100190439f15331f16900320b080310d6f9ac9f95a6fc34";
static constexpr const char *GBE_kDotaPracticeLobbyLaunchCacheSubscribedOfficialHex =
    "4d15008014000000090eac2b7cdec1400110dbc3fdeafdffffffff0108ba041098808080081ae9061800008000000000\n"
    "12bd0108d40f12b70108d6f9ac9f95a6fc3418012001310eac2b7cdec1400159f5b62108010010016001680070008201\n"
    "0531313131318a010240008a01024000a80100e00100f001bbca9fe020f80100a00203d00200d80200e00200f00200f8\n"
    "0200800300980300a80300c80301f2030708f54412020800d80400900500b805f7e6cbcf06c00500e80503f00500f805\n"
    "00880600b80600c00637f00600880700c2071009f5b621080100100118003801800101c80700f807008008d5e6cbcf06\n"
    "120508dd0f1200121208de0f120d0a090a075376656e6d61781000129b0308df0f1295030a0012900308a545128a0308\n"
    "f5ed864112bc010a05080210c00c0a05080510c8010a04080a10640a04080b10640a05080c10de020a04082210640a04\n"
    "082310320a05082510ee050a05082810c00c0a04082a10320a05082c10db030a05082f10ac020a05083510de020a0408\n"
    "4510640a04084b10640a05085110d8040a05085310db030a05085410bd150a0508551096010a04086810320a0508c302\n"
    "10010a0508900310010a0508910310010a0508920310010a05089a0310060a0508cd0310030a0508ce0310080a0508cf\n"
    "0310161a060886011086011a0608d10f10d20f1a06088927108a271a0608914e10924e1a0608f95510fa551a0608e15d\n"
    "10e25d1a0808d1890210d289021a0808b9910210ba91021a080889a102108aa1021a0808c1b80210c2b8021a080891c8\n"
    "021092c8021a0808e1d70210e2d7021a080899ef02109aef021a0808899e03108a9e031a0808899b04108a9b041a0808\n"
    "f9c90410fac9041a0808e9f80410eaf8041a0808b9880510ba88051a0808a1900510a290051a0808899805108a98051a\n"
    "0808c1ac0610c2ac0612cf0108e00f12c9010a3509f5b62108010010014800580060e1ac8b84d0854068008501000000\n"
    "e085014128d9f585015706000098010098010098010098010015000000001a300813122c08f5ed864110001800200038\n"
    "006000d00100d80100e00100fa0106080f100a180afa0108081c10e80718e8071a1c081a121808f5ed86411000180020\n"
    "0138006000d00100d80100e001001a1c0827121808f5ed864110001800200138006000d00100d80100e001001a1d0838\n"
    "121908f5ed864110e8071800200138016000d00100d80100e0010019e0a6ef5331f16900220b080310d6f9ac9f95a6fc\n"
    "34";

static const uint8 GBE_kDotaCacheSubscribedTemplate[] = {
    0x18, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x12, 0xC8, 0x18, 0x08, 0x01, 0x12, 0x19, 0x08,
    0x95, 0xA3, 0xED, 0xCE, 0x06, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x01, 0x20, 0x98, 0x2B, 0x78,
    0x00, 0x92, 0x01, 0x04, 0x08, 0x01, 0x10, 0x05, 0x12, 0x16, 0x08, 0xF2, 0xDD, 0xFA, 0xFF, 0x0F,
    0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x03, 0x20, 0xFB, 0x26, 0x40, 0x01, 0x48, 0x06, 0x78, 0x00,
    0x12, 0x16, 0x08, 0xF3, 0xDD, 0xFA, 0xFF, 0x0F, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x02, 0x20,
    0x98, 0x75, 0x40, 0x01, 0x48, 0x06, 0x78, 0x00, 0x12, 0x19, 0x08, 0xCA, 0x8D, 0xA2, 0x80, 0x10,
    0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x05, 0x20, 0xB8, 0x30, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08,
    0x4A, 0x10, 0x06, 0x12, 0x19, 0x08, 0xEA, 0xA5, 0xC1, 0x8D, 0x10, 0x10, 0xF5, 0xED, 0x86, 0x41,
    0x18, 0x06, 0x20, 0xF7, 0x2D, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x32, 0x10, 0x01, 0x12, 0x19,
    0x08, 0xB3, 0xCC, 0xC1, 0x8D, 0x10, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x09, 0x20, 0xEB, 0x84,
    0x01, 0x28, 0x05, 0x40, 0x01, 0x48, 0x06, 0x78, 0x00, 0x12, 0x2F, 0x08, 0xB4, 0xCC, 0xC1, 0x8D,
    0x10, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x08, 0x20, 0xBA, 0x17, 0x30, 0x31, 0x40, 0x01, 0x48,
    0x06, 0x62, 0x15, 0x08, 0x01, 0x1A, 0x11, 0x0A, 0x0B, 0x18, 0x00, 0x22, 0x00, 0x28, 0xBA, 0x17,
    0x30, 0x00, 0x3A, 0x00, 0x10, 0x00, 0x18, 0x00, 0x78, 0x00, 0x12, 0x1D, 0x08, 0xB5, 0xCC, 0xC1,
    0x8D, 0x10, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x04, 0x20, 0xFB, 0x26, 0x40, 0x01, 0x48, 0x06,
    0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x0F, 0x10, 0x00, 0x12, 0x2F, 0x08, 0x96, 0xE6, 0xC1, 0x8D,
    0x10, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x0C, 0x20, 0xBA, 0x17, 0x30, 0x5A, 0x40, 0x01, 0x48,
    0x06, 0x62, 0x15, 0x08, 0x01, 0x1A, 0x11, 0x0A, 0x0B, 0x18, 0x00, 0x22, 0x00, 0x28, 0xBA, 0x17,
    0x30, 0x00, 0x3A, 0x00, 0x10, 0x02, 0x18, 0x00, 0x78, 0x00, 0x12, 0x16, 0x08, 0x97, 0xE6, 0xC1,
    0x8D, 0x10, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x0B, 0x20, 0x99, 0x75, 0x40, 0x01, 0x48, 0x06,
    0x78, 0x00, 0x12, 0x1D, 0x08, 0x98, 0xE6, 0xC1, 0x8D, 0x10, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18,
    0x0A, 0x20, 0xB9, 0x2A, 0x40, 0x01, 0x48, 0x06, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x23, 0x10,
    0x02, 0x12, 0x1E, 0x08, 0x99, 0xE6, 0xC1, 0x8D, 0x10, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x0D,
    0x20, 0xD4, 0x4E, 0x40, 0x01, 0x48, 0x06, 0x78, 0x00, 0x92, 0x01, 0x05, 0x08, 0xE8, 0x07, 0x10,
    0x04, 0x12, 0x36, 0x08, 0x8F, 0xC3, 0xC1, 0xC0, 0x10, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x0E,
    0x20, 0x8B, 0x32, 0x62, 0x1B, 0x08, 0x01, 0x1A, 0x17, 0x0A, 0x0B, 0x18, 0x00, 0x22, 0x00, 0x28,
    0xBB, 0x17, 0x30, 0x00, 0x3A, 0x00, 0x10, 0x01, 0x18, 0x00, 0x20, 0x00, 0x28, 0xF2, 0xE6, 0x06,
    0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x66, 0x10, 0x03, 0x12, 0x42, 0x08, 0xE8, 0xE8, 0xD1, 0xD7,
    0x10, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x0F, 0x20, 0xA8, 0x2E, 0x62, 0x09, 0x08, 0x90, 0x03,
    0x1A, 0x04, 0x00, 0x00, 0x00, 0x00, 0x62, 0x1C, 0x08, 0x01, 0x1A, 0x18, 0x0A, 0x0B, 0x18, 0x00,
    0x22, 0x00, 0x28, 0xBB, 0x17, 0x30, 0x00, 0x3A, 0x00, 0x10, 0x01, 0x18, 0x84, 0xAB, 0xC0, 0x19,
    0x20, 0x00, 0x28, 0x00, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x58, 0x10, 0x00, 0x12, 0x1D, 0x08,
    0xD7, 0xF3, 0x91, 0x82, 0x12, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x10, 0x20, 0xBA, 0x2A, 0x40,
    0x01, 0x48, 0x06, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x23, 0x10, 0x04, 0x12, 0x16, 0x08, 0xAB,
    0xED, 0x92, 0x82, 0x12, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x14, 0x20, 0xE3, 0x2A, 0x40, 0x01,
    0x48, 0x06, 0x78, 0x00, 0x12, 0x16, 0x08, 0xAC, 0xED, 0x92, 0x82, 0x12, 0x10, 0xF5, 0xED, 0x86,
    0x41, 0x18, 0x15, 0x20, 0xE3, 0x2A, 0x40, 0x01, 0x48, 0x06, 0x78, 0x00, 0x12, 0x19, 0x08, 0x8E,
    0xB2, 0xF9, 0x83, 0x12, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x11, 0x20, 0x84, 0x30, 0x78, 0x00,
    0x92, 0x01, 0x04, 0x08, 0x30, 0x10, 0x01, 0x12, 0x19, 0x08, 0xE0, 0x9D, 0x8F, 0x84, 0x12, 0x10,
    0xF5, 0xED, 0x86, 0x41, 0x18, 0x16, 0x20, 0x88, 0x30, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x09,
    0x10, 0x02, 0x12, 0x4E, 0x08, 0xBB, 0xB2, 0xFC, 0x91, 0x15, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18,
    0x17, 0x20, 0x83, 0x2F, 0x40, 0x01, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00,
    0x00, 0x62, 0x2D, 0x08, 0x05, 0x1A, 0x29, 0x0A, 0x25, 0x18, 0x00, 0x22, 0x1A, 0x6E, 0x70, 0x63,
    0x5F, 0x64, 0x6F, 0x74, 0x61, 0x5F, 0x68, 0x65, 0x72, 0x6F, 0x5F, 0x73, 0x74, 0x6F, 0x72, 0x6D,
    0x5F, 0x73, 0x70, 0x69, 0x72, 0x69, 0x74, 0x28, 0xBD, 0x17, 0x30, 0x00, 0x3A, 0x00, 0x10, 0x13,
    0x78, 0x00, 0x12, 0x26, 0x08, 0x8E, 0xFD, 0x86, 0x93, 0x1B, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18,
    0x13, 0x20, 0xF9, 0x1F, 0x40, 0x01, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00,
    0x00, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x35, 0x10, 0x03, 0x12, 0x16, 0x08, 0xE3, 0x8F, 0x88,
    0x93, 0x1B, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x19, 0x20, 0x99, 0x4F, 0x40, 0x01, 0x48, 0x06,
    0x78, 0x00, 0x12, 0x1D, 0x08, 0xE4, 0x8F, 0x88, 0x93, 0x1B, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18,
    0x12, 0x20, 0xCB, 0x26, 0x40, 0x01, 0x48, 0x06, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x10, 0x10,
    0x04, 0x12, 0x2F, 0x08, 0xE5, 0x8F, 0x88, 0x93, 0x1B, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x18,
    0x20, 0xBA, 0x17, 0x30, 0x3D, 0x40, 0x01, 0x48, 0x06, 0x62, 0x15, 0x08, 0x01, 0x1A, 0x11, 0x0A,
    0x0B, 0x18, 0x00, 0x22, 0x00, 0x28, 0xBA, 0x17, 0x30, 0x00, 0x3A, 0x00, 0x10, 0x07, 0x18, 0x00,
    0x78, 0x00, 0x12, 0x27, 0x08, 0xD8, 0x8F, 0x9A, 0xC3, 0x22, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18,
    0x90, 0x4E, 0x20, 0xEA, 0x2F, 0x40, 0x01, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00,
    0x00, 0x00, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x49, 0x10, 0x01, 0x12, 0x35, 0x08, 0x9A, 0x8C,
    0xD5, 0xC3, 0x22, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x8F, 0x4E, 0x20, 0xFA, 0x26, 0x40, 0x01,
    0x48, 0x06, 0x62, 0x15, 0x08, 0x01, 0x1A, 0x11, 0x0A, 0x0B, 0x18, 0x00, 0x22, 0x00, 0x28, 0xBA,
    0x17, 0x30, 0x00, 0x3A, 0x00, 0x10, 0x01, 0x18, 0x00, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x0F,
    0x10, 0x01, 0x12, 0x31, 0x08, 0x9B, 0x8C, 0xD5, 0xC3, 0x22, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18,
    0x8E, 0x4E, 0x20, 0xEA, 0x84, 0x01, 0x28, 0x05, 0x40, 0x01, 0x48, 0x06, 0x62, 0x15, 0x08, 0x01,
    0x1A, 0x11, 0x0A, 0x0B, 0x18, 0x00, 0x22, 0x00, 0x28, 0xBA, 0x17, 0x30, 0x00, 0x3A, 0x00, 0x10,
    0x01, 0x18, 0x00, 0x78, 0x00, 0x12, 0x27, 0x08, 0xB6, 0xFD, 0xFC, 0xD3, 0x22, 0x10, 0xF5, 0xED,
    0x86, 0x41, 0x18, 0x8D, 0x4E, 0x20, 0xA7, 0x2B, 0x40, 0x01, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A,
    0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x4B, 0x10, 0x03, 0x12, 0x45,
    0x08, 0x99, 0xA9, 0xA9, 0xE7, 0x26, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x8C, 0x4E, 0x20, 0x99,
    0x2D, 0x40, 0x01, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x62, 0x1C,
    0x08, 0x01, 0x1A, 0x18, 0x0A, 0x0B, 0x18, 0x00, 0x22, 0x00, 0x28, 0xBB, 0x17, 0x30, 0x00, 0x3A,
    0x00, 0x10, 0x01, 0x18, 0xD8, 0x8C, 0xB6, 0x28, 0x20, 0x00, 0x28, 0x00, 0x78, 0x00, 0x92, 0x01,
    0x04, 0x08, 0x4A, 0x10, 0x04, 0x12, 0x27, 0x08, 0xCF, 0x8F, 0xED, 0xEA, 0x2D, 0x10, 0xF5, 0xED,
    0x86, 0x41, 0x18, 0x8B, 0x4E, 0x20, 0xB1, 0x25, 0x40, 0x01, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A,
    0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x3A, 0x10, 0x00, 0x12, 0x27,
    0x08, 0xED, 0x87, 0x9F, 0x8B, 0x2E, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x8A, 0x4E, 0x20, 0xA1,
    0x2A, 0x40, 0x01, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00,
    0x92, 0x01, 0x04, 0x08, 0x63, 0x10, 0x04, 0x12, 0x20, 0x08, 0xE9, 0xCF, 0xBB, 0xBF, 0x2E, 0x10,
    0xF5, 0xED, 0x86, 0x41, 0x18, 0x89, 0x4E, 0x20, 0xC0, 0x24, 0x40, 0x01, 0x62, 0x09, 0x08, 0xD5,
    0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x12, 0x27, 0x08, 0x9D, 0xD1, 0x82, 0x80,
    0x2F, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x88, 0x4E, 0x20, 0xCE, 0x2B, 0x40, 0x01, 0x62, 0x09,
    0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x08,
    0x10, 0x00, 0x12, 0x27, 0x08, 0xBD, 0xA9, 0x8F, 0xA7, 0x37, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18,
    0x87, 0x4E, 0x20, 0xDA, 0x28, 0x40, 0x01, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00,
    0x00, 0x00, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x4C, 0x10, 0x00, 0x12, 0x20, 0x08, 0xD7, 0xCF,
    0x8A, 0x89, 0x38, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x86, 0x4E, 0x20, 0xAD, 0x2D, 0x40, 0x01,
    0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x12, 0x20, 0x08,
    0xED, 0x85, 0x8E, 0x8F, 0x39, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x85, 0x4E, 0x20, 0xE8, 0x2F,
    0x40, 0x01, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x12,
    0x20, 0x08, 0x95, 0xFC, 0xD8, 0xB9, 0x3A, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x84, 0x4E, 0x20,
    0x86, 0x26, 0x40, 0x01, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78,
    0x00, 0x12, 0x20, 0x08, 0xD3, 0xF7, 0xA4, 0xCA, 0x3A, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x83,
    0x4E, 0x20, 0x9B, 0x22, 0x40, 0x01, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00,
    0x00, 0x78, 0x00, 0x12, 0x20, 0x08, 0xFD, 0xE5, 0x9D, 0xD6, 0x3A, 0x10, 0xF5, 0xED, 0x86, 0x41,
    0x18, 0x82, 0x4E, 0x20, 0x98, 0x28, 0x40, 0x01, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01,
    0x00, 0x00, 0x00, 0x78, 0x00, 0x12, 0x20, 0x08, 0x9D, 0xC5, 0xCF, 0x82, 0x43, 0x10, 0xF5, 0xED,
    0x86, 0x41, 0x18, 0x81, 0x4E, 0x20, 0xF7, 0x25, 0x40, 0x01, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A,
    0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x12, 0x20, 0x08, 0xF1, 0xC2, 0xAF, 0x80, 0x44, 0x10,
    0xF5, 0xED, 0x86, 0x41, 0x18, 0x80, 0x4E, 0x20, 0xB1, 0x2B, 0x40, 0x01, 0x62, 0x09, 0x08, 0xD5,
    0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x12, 0x20, 0x08, 0xC1, 0xF8, 0xA7, 0x9D,
    0x47, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xFF, 0x4D, 0x20, 0xCE, 0x2A, 0x40, 0x01, 0x62, 0x09,
    0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x12, 0x27, 0x08, 0xED, 0xA6,
    0xFF, 0xC4, 0x48, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xFE, 0x4D, 0x20, 0xDA, 0x2C, 0x40, 0x01,
    0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x92, 0x01, 0x04,
    0x08, 0x37, 0x10, 0x03, 0x12, 0x27, 0x08, 0xDD, 0xB1, 0x98, 0xD8, 0x5E, 0x10, 0xF5, 0xED, 0x86,
    0x41, 0x18, 0xFD, 0x4D, 0x20, 0x9B, 0x24, 0x40, 0x01, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04,
    0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x3F, 0x10, 0x03, 0x12, 0x27, 0x08,
    0xA5, 0xCC, 0x8B, 0xB6, 0x5F, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xFC, 0x4D, 0x20, 0xE9, 0x25,
    0x40, 0x01, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x92,
    0x01, 0x04, 0x08, 0x34, 0x10, 0x03, 0x12, 0x2D, 0x08, 0xA5, 0x89, 0x99, 0xB8, 0x5F, 0x10, 0xF5,
    0xED, 0x86, 0x41, 0x18, 0xF8, 0x4D, 0x20, 0x9D, 0x60, 0x48, 0x24, 0x62, 0x16, 0x08, 0x01, 0x1A,
    0x12, 0x0A, 0x0B, 0x18, 0x00, 0x22, 0x00, 0x28, 0xC4, 0x17, 0x30, 0x00, 0x3A, 0x00, 0x10, 0xCD,
    0x01, 0x18, 0x00, 0x78, 0x00, 0x12, 0x17, 0x08, 0x95, 0x86, 0x9A, 0xB8, 0x5F, 0x10, 0xF5, 0xED,
    0x86, 0x41, 0x18, 0xF7, 0x4D, 0x20, 0xF8, 0x61, 0x40, 0x01, 0x48, 0x22, 0x78, 0x00, 0x12, 0x46,
    0x08, 0x9D, 0x86, 0x9A, 0xB8, 0x5F, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xF6, 0x4D, 0x20, 0xE6,
    0x6A, 0x40, 0x01, 0x48, 0x22, 0x62, 0x2D, 0x08, 0x01, 0x1A, 0x29, 0x0A, 0x22, 0x18, 0x00, 0x22,
    0x17, 0x6E, 0x70, 0x63, 0x5F, 0x64, 0x6F, 0x74, 0x61, 0x5F, 0x68, 0x65, 0x72, 0x6F, 0x5F, 0x6F,
    0x67, 0x72, 0x65, 0x5F, 0x6D, 0x61, 0x67, 0x69, 0x28, 0xD2, 0x17, 0x30, 0x00, 0x3A, 0x00, 0x10,
    0x8A, 0x02, 0x18, 0x5C, 0x78, 0x00, 0x12, 0x16, 0x08, 0xF5, 0xD7, 0x9B, 0xB8, 0x5F, 0x10, 0xF5,
    0xED, 0x86, 0x41, 0x18, 0xEF, 0x4D, 0x20, 0xDB, 0xB8, 0x01, 0x48, 0x1F, 0x78, 0x00, 0x12, 0x1E,
    0x08, 0x95, 0xDD, 0x9B, 0xB8, 0x5F, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xEE, 0x4D, 0x20, 0xBC,
    0x94, 0x01, 0x48, 0x1F, 0x78, 0x00, 0x92, 0x01, 0x05, 0x08, 0xE8, 0x07, 0x10, 0x02, 0x12, 0x16,
    0x08, 0xA5, 0xDE, 0x9B, 0xB8, 0x5F, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xED, 0x4D, 0x20, 0xC2,
    0x96, 0x01, 0x48, 0x1F, 0x78, 0x00, 0x12, 0x14, 0x08, 0xCD, 0xE0, 0x9B, 0xB8, 0x5F, 0x10, 0xF5,
    0xED, 0x86, 0x41, 0x18, 0xEA, 0x4D, 0x20, 0xFC, 0x89, 0x01, 0x78, 0x00, 0x12, 0x28, 0x08, 0xF5,
    0xE9, 0x9B, 0xB8, 0x5F, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xF1, 0x4D, 0x20, 0xA3, 0xB9, 0x01,
    0x48, 0x08, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x92,
    0x01, 0x04, 0x08, 0x2E, 0x10, 0x01, 0x12, 0x21, 0x08, 0xF5, 0xBD, 0x9C, 0xB8, 0x5F, 0x10, 0xF5,
    0xED, 0x86, 0x41, 0x18, 0xF0, 0x4D, 0x20, 0xBC, 0xB1, 0x01, 0x48, 0x08, 0x62, 0x09, 0x08, 0xD5,
    0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x12, 0x20, 0x08, 0xA5, 0xC9, 0xD6, 0x9C,
    0x60, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xE6, 0x4D, 0x20, 0xBD, 0x21, 0x40, 0x01, 0x62, 0x09,
    0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x12, 0x20, 0x08, 0xE5, 0xC2,
    0xD6, 0xA0, 0x60, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xE5, 0x4D, 0x20, 0xE1, 0x6B, 0x48, 0x1F,
    0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x12, 0x28, 0x08,
    0xBD, 0x86, 0xF6, 0xA1, 0x60, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xE4, 0x4D, 0x20, 0xC4, 0x94,
    0x01, 0x48, 0x1F, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00,
    0x92, 0x01, 0x04, 0x08, 0x04, 0x10, 0x07, 0x12, 0x28, 0x08, 0x85, 0xB7, 0xE5, 0xA3, 0x60, 0x10,
    0xF5, 0xED, 0x86, 0x41, 0x18, 0xEC, 0x4D, 0x20, 0xCB, 0x98, 0x01, 0x48, 0x22, 0x62, 0x09, 0x08,
    0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x51, 0x10,
    0x00, 0x12, 0x28, 0x08, 0x8D, 0xB7, 0xE5, 0xA3, 0x60, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xE3,
    0x4D, 0x20, 0xCA, 0x98, 0x01, 0x48, 0x22, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00,
    0x00, 0x00, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x51, 0x10, 0x03, 0x12, 0x28, 0x08, 0x95, 0xB7,
    0xE5, 0xA3, 0x60, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xE2, 0x4D, 0x20, 0xC9, 0x98, 0x01, 0x48,
    0x22, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x92, 0x01,
    0x04, 0x08, 0x51, 0x10, 0x01, 0x12, 0x28, 0x08, 0x9D, 0xB7, 0xE5, 0xA3, 0x60, 0x10, 0xF5, 0xED,
    0x86, 0x41, 0x18, 0xE1, 0x4D, 0x20, 0xC8, 0x98, 0x01, 0x48, 0x22, 0x62, 0x09, 0x08, 0xD5, 0x01,
    0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x51, 0x10, 0x04, 0x12,
    0x29, 0x08, 0xA5, 0xB7, 0xE5, 0xA3, 0x60, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xE0, 0x4D, 0x20,
    0xC7, 0x98, 0x01, 0x48, 0x22, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00,
    0x78, 0x00, 0x92, 0x01, 0x05, 0x08, 0xE8, 0x07, 0x10, 0x06, 0x12, 0x28, 0x08, 0xAD, 0xB7, 0xE5,
    0xA3, 0x60, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xDF, 0x4D, 0x20, 0xC6, 0x98, 0x01, 0x48, 0x22,
    0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x92, 0x01, 0x04,
    0x08, 0x51, 0x10, 0x02, 0x12, 0x28, 0x08, 0x9D, 0x82, 0xE6, 0xA3, 0x60, 0x10, 0xF5, 0xED, 0x86,
    0x41, 0x18, 0xEB, 0x4D, 0x20, 0xB4, 0xB1, 0x01, 0x48, 0x22, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A,
    0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x3B, 0x10, 0x00, 0x12, 0x28,
    0x08, 0xA5, 0x82, 0xE6, 0xA3, 0x60, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xDE, 0x4D, 0x20, 0xB5,
    0xB1, 0x01, 0x48, 0x22, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78,
    0x00, 0x92, 0x01, 0x04, 0x08, 0x3B, 0x10, 0x01, 0x12, 0x28, 0x08, 0xAD, 0x82, 0xE6, 0xA3, 0x60,
    0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xDD, 0x4D, 0x20, 0xB6, 0xB1, 0x01, 0x48, 0x22, 0x62, 0x09,
    0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x3B,
    0x10, 0x04, 0x12, 0x28, 0x08, 0xB5, 0x82, 0xE6, 0xA3, 0x60, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18,
    0xDC, 0x4D, 0x20, 0xF1, 0xB5, 0x01, 0x48, 0x22, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01,
    0x00, 0x00, 0x00, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x3B, 0x10, 0x02, 0x12, 0x28, 0x08, 0xBD,
    0x82, 0xE6, 0xA3, 0x60, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xDB, 0x4D, 0x20, 0xF2, 0xB5, 0x01,
    0x48, 0x22, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x92,
    0x01, 0x04, 0x08, 0x3B, 0x10, 0x03, 0x12, 0x4F, 0x08, 0x85, 0x80, 0xEB, 0xA3, 0x60, 0x10, 0xF5,
    0xED, 0x86, 0x41, 0x18, 0x00, 0x20, 0x87, 0x05, 0x62, 0x09, 0x08, 0xAC, 0x03, 0x1A, 0x04, 0x54,
    0x00, 0x00, 0x00, 0x62, 0x30, 0x08, 0x01, 0x1A, 0x2C, 0x0A, 0x22, 0x18, 0x03, 0x22, 0x17, 0x6E,
    0x70, 0x63, 0x5F, 0x64, 0x6F, 0x74, 0x61, 0x5F, 0x68, 0x65, 0x72, 0x6F, 0x5F, 0x6F, 0x67, 0x72,
    0x65, 0x5F, 0x6D, 0x61, 0x67, 0x69, 0x28, 0xDB, 0x17, 0x30, 0x00, 0x3A, 0x00, 0x10, 0x99, 0x2B,
    0x18, 0xB6, 0xFD, 0xBE, 0x02, 0x78, 0x00, 0x12, 0x1F, 0x08, 0xC5, 0xA7, 0xF1, 0xA3, 0x60, 0x10,
    0xF5, 0xED, 0x86, 0x41, 0x18, 0xE7, 0x4D, 0x20, 0xF8, 0x8E, 0x01, 0x40, 0x01, 0x48, 0x22, 0x78,
    0x00, 0x92, 0x01, 0x04, 0x08, 0x02, 0x10, 0x00, 0x12, 0x1F, 0x08, 0xCD, 0xA7, 0xF1, 0xA3, 0x60,
    0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xDA, 0x4D, 0x20, 0xF7, 0x8E, 0x01, 0x40, 0x01, 0x48, 0x22,
    0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x02, 0x10, 0x01, 0x12, 0x1F, 0x08, 0xD5, 0xA7, 0xF1, 0xA3,
    0x60, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xD9, 0x4D, 0x20, 0xF6, 0x8E, 0x01, 0x40, 0x01, 0x48,
    0x22, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x02, 0x10, 0x02, 0x12, 0x1F, 0x08, 0xDD, 0xA7, 0xF1,
    0xA3, 0x60, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xD8, 0x4D, 0x20, 0xF5, 0x8E, 0x01, 0x40, 0x01,
    0x48, 0x22, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x02, 0x10, 0x03, 0x12, 0x1F, 0x08, 0xE5, 0xA7,
    0xF1, 0xA3, 0x60, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xD7, 0x4D, 0x20, 0xF4, 0x8E, 0x01, 0x40,
    0x01, 0x48, 0x22, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08, 0x02, 0x10, 0x05, 0x12, 0x27, 0x08, 0xAD,
    0xD8, 0xEE, 0xA3, 0x61, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xFA, 0x4D, 0x20, 0xFD, 0x1F, 0x40,
    0x01, 0x62, 0x09, 0x08, 0xD5, 0x01, 0x1A, 0x04, 0x01, 0x00, 0x00, 0x00, 0x78, 0x00, 0x92, 0x01,
    0x04, 0x08, 0x1D, 0x10, 0x04, 0x12, 0x1C, 0x08, 0x8D, 0xD5, 0xFD, 0x9A, 0x6B, 0x10, 0xF5, 0xED,
    0x86, 0x41, 0x18, 0xFB, 0x4D, 0x20, 0x9A, 0x20, 0x48, 0x1F, 0x78, 0x00, 0x92, 0x01, 0x04, 0x08,
    0x17, 0x10, 0x02, 0x12, 0x16, 0x08, 0xB5, 0xF4, 0xB0, 0xA5, 0x6B, 0x10, 0xF5, 0xED, 0x86, 0x41,
    0x18, 0xF9, 0x4D, 0x20, 0x85, 0xB9, 0x01, 0x48, 0x1F, 0x78, 0x00, 0x12, 0x16, 0x08, 0xBD, 0xF4,
    0xB0, 0xA5, 0x6B, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xE9, 0x4D, 0x20, 0xC8, 0xB8, 0x02, 0x48,
    0x1F, 0x78, 0x00, 0x12, 0x16, 0x08, 0xB5, 0xAD, 0xC6, 0xA5, 0x6B, 0x10, 0xF5, 0xED, 0x86, 0x41,
    0x18, 0xE8, 0x4D, 0x20, 0x85, 0xB9, 0x01, 0x48, 0x1F, 0x78, 0x00, 0x12, 0x16, 0x08, 0x85, 0xA3,
    0xD6, 0xA8, 0x6B, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xD6, 0x4D, 0x20, 0x85, 0xB9, 0x01, 0x48,
    0x1F, 0x78, 0x00, 0x12, 0x16, 0x08, 0x95, 0xA3, 0xD6, 0xA8, 0x6B, 0x10, 0xF5, 0xED, 0x86, 0x41,
    0x18, 0xD5, 0x4D, 0x20, 0xC7, 0xB8, 0x02, 0x48, 0x1F, 0x78, 0x00, 0x12, 0x16, 0x08, 0xDD, 0xD4,
    0xBF, 0xAA, 0x6B, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0xD4, 0x4D, 0x20, 0x85, 0xB9, 0x01, 0x48,
    0x1F, 0x78, 0x00, 0x12, 0x16, 0x08, 0xE5, 0xD4, 0xBF, 0xAA, 0x6B, 0x10, 0xF5, 0xED, 0x86, 0x41,
    0x18, 0xD3, 0x4D, 0x20, 0xEF, 0xBC, 0x01, 0x48, 0x1F, 0x78, 0x00, 0x12, 0x19, 0x08, 0xDD, 0x90,
    0xC1, 0x80, 0x6F, 0x10, 0xF5, 0xED, 0x86, 0x41, 0x18, 0x89, 0x80, 0x80, 0x80, 0x0C, 0x20, 0xA9,
    0xD5, 0x01, 0x28, 0x03, 0x78, 0x00, 0x12, 0x1B, 0x08, 0x85, 0x91, 0xCB, 0xE0, 0x79, 0x10, 0xF5,
    0xED, 0x86, 0x41, 0x18, 0x87, 0x80, 0x80, 0x80, 0x0C, 0x20, 0xE1, 0x95, 0x02, 0x40, 0x03, 0x48,
    0x07, 0x78, 0x00, 0x12, 0x15, 0x08, 0x07, 0x12, 0x11, 0x08, 0x00, 0x10, 0x00, 0x18, 0x01, 0x20,
    0x00, 0x28, 0x00, 0x35, 0x00, 0x00, 0x00, 0x00, 0x48, 0x00, 0x12, 0xAA, 0x21, 0x08, 0xDA, 0x0F,
    0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xC9, 0x01, 0x38, 0xC2, 0xFC, 0xC3,
    0xBF, 0x06, 0x40, 0x00, 0x48, 0x01, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03,
    0x80, 0x01, 0xAF, 0xF1, 0xA9, 0xA5, 0x05, 0x88, 0x01, 0x02, 0x90, 0x01, 0x90, 0xC9, 0xA4, 0xB3,
    0x0F, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xCA, 0x01, 0x38, 0xC2, 0xFC,
    0xC3, 0xBF, 0x06, 0x40, 0x00, 0x48, 0x01, 0x50, 0x01, 0x58, 0x00, 0x60, 0x08, 0x70, 0x00, 0x78,
    0x03, 0x80, 0x01, 0xEB, 0x8F, 0xAB, 0xA8, 0x0D, 0x88, 0x01, 0x02, 0x90, 0x01, 0xB6, 0x8D, 0xD0,
    0xD3, 0x0C, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xCB, 0x01, 0x38, 0xC2,
    0xFC, 0xC3, 0xBF, 0x06, 0x40, 0x00, 0x48, 0x01, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00,
    0x78, 0x03, 0x80, 0x01, 0xF1, 0xCD, 0xA4, 0xE2, 0x03, 0x88, 0x01, 0x02, 0x90, 0x01, 0x92, 0xFF,
    0x8A, 0x9F, 0x05, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xF5, 0x03, 0x38,
    0x88, 0xDF, 0x90, 0xAC, 0x06, 0x40, 0x00, 0x48, 0x18, 0x50, 0x01, 0x58, 0x00, 0x60, 0x01, 0x70,
    0x00, 0x78, 0x03, 0x80, 0x01, 0xA9, 0xEB, 0xF9, 0xD0, 0x0E, 0x88, 0x01, 0x05, 0x90, 0x01, 0xE0,
    0x92, 0xCE, 0xA9, 0x03, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xF6, 0x03,
    0x38, 0x88, 0xDF, 0x90, 0xAC, 0x06, 0x40, 0x00, 0x48, 0x18, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00,
    0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0x8F, 0x8D, 0xF5, 0xF2, 0x05, 0x88, 0x01, 0x05, 0x90, 0x01,
    0xE7, 0x8E, 0xA2, 0x8B, 0x0B, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xF7,
    0x03, 0x38, 0x88, 0xDF, 0x90, 0xAC, 0x06, 0x40, 0x00, 0x48, 0x18, 0x50, 0x01, 0x58, 0x00, 0x60,
    0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0x82, 0xD8, 0x95, 0xC3, 0x04, 0x88, 0x01, 0x05, 0x90,
    0x01, 0xB8, 0xCA, 0xD0, 0xEE, 0x01, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18,
    0xE9, 0x07, 0x38, 0xC4, 0x89, 0xC0, 0xAB, 0x06, 0x40, 0x00, 0x48, 0x16, 0x50, 0x01, 0x58, 0x00,
    0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0x91, 0xEA, 0xB8, 0xBB, 0x0B, 0x88, 0x01, 0x0A,
    0x90, 0x01, 0x96, 0xBA, 0xAA, 0xB2, 0x0D, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13,
    0x18, 0xEA, 0x07, 0x38, 0xC4, 0x89, 0xC0, 0xAB, 0x06, 0x40, 0x00, 0x48, 0x16, 0x50, 0x01, 0x58,
    0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0x8B, 0xEF, 0x8F, 0xE8, 0x02, 0x88, 0x01,
    0x0A, 0x90, 0x01, 0xC2, 0xEE, 0x8B, 0x87, 0x0B, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10,
    0x13, 0x18, 0xEB, 0x07, 0x38, 0xC4, 0x89, 0xC0, 0xAB, 0x06, 0x40, 0x00, 0x48, 0x16, 0x50, 0x01,
    0x58, 0x00, 0x60, 0x01, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xB9, 0x9A, 0xA1, 0xA5, 0x0E, 0x88,
    0x01, 0x0A, 0x90, 0x01, 0xAB, 0xD8, 0xE2, 0xE7, 0x0E, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41,
    0x10, 0x13, 0x18, 0xCD, 0x08, 0x38, 0x9B, 0xFD, 0xB1, 0xAB, 0x06, 0x40, 0x00, 0x48, 0x15, 0x50,
    0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0x88, 0x93, 0xBA, 0xBF, 0x0F,
    0x88, 0x01, 0x0B, 0x90, 0x01, 0x8A, 0xA1, 0x87, 0xDF, 0x0C, 0x12, 0x2E, 0x08, 0xF5, 0xED, 0x86,
    0x41, 0x10, 0x13, 0x18, 0xCE, 0x08, 0x38, 0x9B, 0xFD, 0xB1, 0xAB, 0x06, 0x40, 0x00, 0x48, 0x15,
    0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0x95, 0xB0, 0xAF, 0xA9,
    0x0D, 0x88, 0x01, 0x0B, 0x90, 0x01, 0xC4, 0xC5, 0xA7, 0x23, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86,
    0x41, 0x10, 0x13, 0x18, 0xCF, 0x08, 0x38, 0x9B, 0xFD, 0xB1, 0xAB, 0x06, 0x40, 0x00, 0x48, 0x15,
    0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0x8B, 0xEF, 0x8F, 0xE8,
    0x02, 0x88, 0x01, 0x0B, 0x90, 0x01, 0xC2, 0xEE, 0x8B, 0x87, 0x0B, 0x12, 0x2F, 0x08, 0xF5, 0xED,
    0x86, 0x41, 0x10, 0x13, 0x18, 0xB1, 0x09, 0x38, 0x8F, 0xB8, 0x9B, 0xAB, 0x06, 0x40, 0x00, 0x48,
    0x14, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xA2, 0xBB, 0xFC,
    0xA2, 0x0C, 0x88, 0x01, 0x0C, 0x90, 0x01, 0xD2, 0xDE, 0x8D, 0xF6, 0x0B, 0x12, 0x2F, 0x08, 0xF5,
    0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xB2, 0x09, 0x38, 0x8F, 0xB8, 0x9B, 0xAB, 0x06, 0x40, 0x00,
    0x48, 0x14, 0x50, 0x01, 0x58, 0x00, 0x60, 0x01, 0x70, 0x01, 0x78, 0x03, 0x80, 0x01, 0xC5, 0x96,
    0xE9, 0x8C, 0x09, 0x88, 0x01, 0x0C, 0x90, 0x01, 0x99, 0xB3, 0xCD, 0xB7, 0x02, 0x12, 0x2F, 0x08,
    0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xB3, 0x09, 0x38, 0x8F, 0xB8, 0x9B, 0xAB, 0x06, 0x40,
    0x00, 0x48, 0x14, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xB0,
    0x93, 0xFA, 0x8C, 0x0F, 0x88, 0x01, 0x0C, 0x90, 0x01, 0xCD, 0xD4, 0xAD, 0xB0, 0x08, 0x12, 0x2F,
    0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xA5, 0x0D, 0x38, 0xC2, 0xC0, 0x96, 0x9D, 0x06,
    0x40, 0x00, 0x48, 0x0B, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01,
    0xA2, 0xBB, 0xFC, 0xA2, 0x0C, 0x88, 0x01, 0x11, 0x90, 0x01, 0xD2, 0xDE, 0x8D, 0xF6, 0x0B, 0x12,
    0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xA6, 0x0D, 0x38, 0xC2, 0xC0, 0x96, 0x9D,
    0x06, 0x40, 0x00, 0x48, 0x0B, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80,
    0x01, 0xD9, 0xBD, 0xBD, 0xB3, 0x02, 0x88, 0x01, 0x11, 0x90, 0x01, 0x96, 0xDE, 0xF6, 0xA2, 0x0E,
    0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xA7, 0x0D, 0x38, 0xC2, 0xC0, 0x96,
    0x9D, 0x06, 0x40, 0x00, 0x48, 0x0B, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03,
    0x80, 0x01, 0xE3, 0xFE, 0xCF, 0xFC, 0x01, 0x88, 0x01, 0x11, 0x90, 0x01, 0x9E, 0xEB, 0xB7, 0x84,
    0x06, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xB5, 0x10, 0x38, 0xE6, 0x97,
    0xE3, 0x9B, 0x06, 0x40, 0x00, 0x48, 0x06, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78,
    0x03, 0x80, 0x01, 0xE3, 0xF5, 0xCF, 0xA2, 0x04, 0x88, 0x01, 0x15, 0x90, 0x01, 0xFD, 0xB9, 0x82,
    0x88, 0x03, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xB6, 0x10, 0x38, 0xE6,
    0x97, 0xE3, 0x9B, 0x06, 0x40, 0x00, 0x48, 0x06, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00,
    0x78, 0x03, 0x80, 0x01, 0xF4, 0xF4, 0x87, 0x9E, 0x0B, 0x88, 0x01, 0x15, 0x90, 0x01, 0xC9, 0xDE,
    0xD1, 0xCE, 0x04, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xB7, 0x10, 0x38,
    0xE6, 0x97, 0xE3, 0x9B, 0x06, 0x40, 0x00, 0x48, 0x06, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70,
    0x00, 0x78, 0x03, 0x80, 0x01, 0xF4, 0xEF, 0xB1, 0xAC, 0x0E, 0x88, 0x01, 0x15, 0x90, 0x01, 0xF0,
    0xB0, 0x90, 0xA7, 0x0E, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xC9, 0x1A,
    0x38, 0xF5, 0xFF, 0x97, 0xAB, 0x06, 0x40, 0x00, 0x48, 0x13, 0x50, 0x01, 0x58, 0x00, 0x60, 0x01,
    0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xB1, 0xFB, 0xCB, 0xF0, 0x0D, 0x88, 0x01, 0x22, 0x90, 0x01,
    0xBB, 0x91, 0x93, 0xED, 0x0C, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xCA,
    0x1A, 0x38, 0xF5, 0xFF, 0x97, 0xAB, 0x06, 0x40, 0x00, 0x48, 0x13, 0x50, 0x01, 0x58, 0x00, 0x60,
    0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0x90, 0xE8, 0x91, 0xF2, 0x07, 0x88, 0x01, 0x22, 0x90,
    0x01, 0xBE, 0xF3, 0xAD, 0xA2, 0x08, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18,
    0xCB, 0x1A, 0x38, 0xF5, 0xFF, 0x97, 0xAB, 0x06, 0x40, 0x00, 0x48, 0x13, 0x50, 0x01, 0x58, 0x00,
    0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xD9, 0xBD, 0xBD, 0xB3, 0x02, 0x88, 0x01, 0x22,
    0x90, 0x01, 0x96, 0xDE, 0xF6, 0xA2, 0x0E, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13,
    0x18, 0xAD, 0x1B, 0x38, 0x9B, 0xD5, 0xA9, 0x9D, 0x06, 0x40, 0x00, 0x48, 0x05, 0x50, 0x01, 0x58,
    0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xF8, 0xA9, 0x9B, 0x89, 0x01, 0x88, 0x01,
    0x23, 0x90, 0x01, 0x96, 0x9C, 0xB1, 0x85, 0x0F, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10,
    0x13, 0x18, 0xAE, 0x1B, 0x38, 0x9B, 0xD5, 0xA9, 0x9D, 0x06, 0x40, 0x00, 0x48, 0x05, 0x50, 0x01,
    0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xA0, 0x97, 0xC3, 0xC7, 0x0B, 0x88,
    0x01, 0x23, 0x90, 0x01, 0xCB, 0x95, 0xE5, 0x9F, 0x0A, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41,
    0x10, 0x13, 0x18, 0xAF, 0x1B, 0x38, 0x9B, 0xD5, 0xA9, 0x9D, 0x06, 0x40, 0x00, 0x48, 0x05, 0x50,
    0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0x96, 0x8F, 0xE7, 0xB6, 0x08,
    0x88, 0x01, 0x23, 0x90, 0x01, 0xD4, 0xA2, 0x9C, 0x86, 0x07, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86,
    0x41, 0x10, 0x13, 0x18, 0xF5, 0x1C, 0x38, 0xE0, 0xF2, 0xF7, 0xAA, 0x06, 0x40, 0x00, 0x48, 0x10,
    0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xB7, 0xC7, 0xC3, 0xFD,
    0x0A, 0x88, 0x01, 0x25, 0x90, 0x01, 0x85, 0xD5, 0xAD, 0xEC, 0x0E, 0x12, 0x2F, 0x08, 0xF5, 0xED,
    0x86, 0x41, 0x10, 0x13, 0x18, 0xF6, 0x1C, 0x38, 0xE0, 0xF2, 0xF7, 0xAA, 0x06, 0x40, 0x01, 0x48,
    0x10, 0x50, 0x01, 0x58, 0x00, 0x60, 0x02, 0x70, 0x03, 0x78, 0x03, 0x80, 0x01, 0xC1, 0xBF, 0xFF,
    0x91, 0x0B, 0x88, 0x01, 0x25, 0x90, 0x01, 0x9A, 0xAD, 0x93, 0xB3, 0x0F, 0x12, 0x2F, 0x08, 0xF5,
    0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xF7, 0x1C, 0x38, 0xE0, 0xF2, 0xF7, 0xAA, 0x06, 0x40, 0x00,
    0x48, 0x10, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0x8F, 0x8D,
    0xF5, 0xF2, 0x05, 0x88, 0x01, 0x25, 0x90, 0x01, 0xE7, 0x8E, 0xA2, 0x8B, 0x0B, 0x12, 0x2F, 0x08,
    0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xA1, 0x1F, 0x38, 0xB8, 0xE7, 0xC2, 0xBF, 0x06, 0x40,
    0x00, 0x48, 0x03, 0x50, 0x01, 0x58, 0x00, 0x60, 0x01, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xC5,
    0xFC, 0xBE, 0xE1, 0x05, 0x88, 0x01, 0x28, 0x90, 0x01, 0xC5, 0xFC, 0xBE, 0xE1, 0x05, 0x12, 0x2F,
    0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xA2, 0x1F, 0x38, 0xB8, 0xE7, 0xC2, 0xBF, 0x06,
    0x40, 0x00, 0x48, 0x03, 0x50, 0x01, 0x58, 0x00, 0x60, 0x06, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01,
    0xE5, 0xB6, 0xC3, 0x99, 0x06, 0x88, 0x01, 0x28, 0x90, 0x01, 0xE5, 0xB4, 0xD5, 0xC3, 0x08, 0x12,
    0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xA3, 0x1F, 0x38, 0xB8, 0xE7, 0xC2, 0xBF,
    0x06, 0x40, 0x00, 0x48, 0x03, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80,
    0x01, 0xF4, 0xAD, 0xE1, 0xB8, 0x04, 0x88, 0x01, 0x28, 0x90, 0x01, 0xB8, 0xA3, 0xCA, 0x99, 0x08,
    0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xE9, 0x20, 0x38, 0x8A, 0x98, 0xE3,
    0x9B, 0x06, 0x40, 0x00, 0x48, 0x07, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03,
    0x80, 0x01, 0xC8, 0xD1, 0xCD, 0xB5, 0x0E, 0x88, 0x01, 0x2A, 0x90, 0x01, 0xC3, 0xDF, 0xD5, 0x95,
    0x0B, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xEA, 0x20, 0x38, 0x8A, 0x98,
    0xE3, 0x9B, 0x06, 0x40, 0x00, 0x48, 0x07, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78,
    0x03, 0x80, 0x01, 0xAD, 0xA2, 0x83, 0xC0, 0x05, 0x88, 0x01, 0x2A, 0x90, 0x01, 0xCA, 0xB5, 0xFF,
    0x83, 0x0F, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xEB, 0x20, 0x38, 0x8A,
    0x98, 0xE3, 0x9B, 0x06, 0x40, 0x00, 0x48, 0x07, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00,
    0x78, 0x03, 0x80, 0x01, 0xB0, 0x93, 0xFA, 0x8C, 0x0F, 0x88, 0x01, 0x2A, 0x90, 0x01, 0xCD, 0xD4,
    0xAD, 0xB0, 0x08, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xB1, 0x22, 0x38,
    0xA1, 0xB2, 0xCC, 0xAB, 0x06, 0x40, 0x00, 0x48, 0x17, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70,
    0x00, 0x78, 0x03, 0x80, 0x01, 0xC2, 0x89, 0xF5, 0xBB, 0x08, 0x88, 0x01, 0x2C, 0x90, 0x01, 0xA0,
    0x93, 0xEE, 0x99, 0x08, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xB2, 0x22,
    0x38, 0xA1, 0xB2, 0xCC, 0xAB, 0x06, 0x40, 0x00, 0x48, 0x17, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00,
    0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xAC, 0x85, 0x8C, 0xCA, 0x0E, 0x88, 0x01, 0x2C, 0x90, 0x01,
    0xA0, 0x86, 0xD0, 0x9B, 0x08, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xB3,
    0x22, 0x38, 0xA1, 0xB2, 0xCC, 0xAB, 0x06, 0x40, 0x00, 0x48, 0x17, 0x50, 0x01, 0x58, 0x00, 0x60,
    0x01, 0x70, 0x02, 0x78, 0x03, 0x80, 0x01, 0xD9, 0xBD, 0xBD, 0xB3, 0x02, 0x88, 0x01, 0x2C, 0x90,
    0x01, 0x96, 0xDE, 0xF6, 0xA2, 0x0E, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18,
    0xDD, 0x24, 0x38, 0xB9, 0xAD, 0xC2, 0xBF, 0x06, 0x40, 0x00, 0x48, 0x1D, 0x50, 0x01, 0x58, 0x00,
    0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xE2, 0xA5, 0xC4, 0xDD, 0x0A, 0x88, 0x01, 0x2F,
    0x90, 0x01, 0xEA, 0xD5, 0xDA, 0x85, 0x05, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13,
    0x18, 0xDE, 0x24, 0x38, 0xB9, 0xAD, 0xC2, 0xBF, 0x06, 0x40, 0x00, 0x48, 0x1D, 0x50, 0x01, 0x58,
    0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0x96, 0x8F, 0xE7, 0xB6, 0x08, 0x88, 0x01,
    0x2F, 0x90, 0x01, 0xD4, 0xA2, 0x9C, 0x86, 0x07, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10,
    0x13, 0x18, 0xDF, 0x24, 0x38, 0xB9, 0xAD, 0xC2, 0xBF, 0x06, 0x40, 0x00, 0x48, 0x1D, 0x50, 0x01,
    0x58, 0x00, 0x60, 0x01, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xF1, 0xCD, 0xA4, 0xE2, 0x03, 0x88,
    0x01, 0x2F, 0x90, 0x01, 0x92, 0xFF, 0x8A, 0x9F, 0x05, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41,
    0x10, 0x13, 0x18, 0xB5, 0x29, 0x38, 0x9E, 0xFB, 0xB1, 0x9C, 0x06, 0x40, 0x00, 0x48, 0x0A, 0x50,
    0x01, 0x58, 0x00, 0x60, 0x01, 0x70, 0x01, 0x78, 0x03, 0x80, 0x01, 0xEC, 0xBF, 0xB8, 0xD1, 0x0E,
    0x88, 0x01, 0x35, 0x90, 0x01, 0xEC, 0xEF, 0xEF, 0xD2, 0x0A, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86,
    0x41, 0x10, 0x13, 0x18, 0xB6, 0x29, 0x38, 0x9E, 0xFB, 0xB1, 0x9C, 0x06, 0x40, 0x00, 0x48, 0x0A,
    0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xEF, 0xA7, 0xBD, 0xD7,
    0x0C, 0x88, 0x01, 0x35, 0x90, 0x01, 0x81, 0xDB, 0x94, 0x91, 0x03, 0x12, 0x2F, 0x08, 0xF5, 0xED,
    0x86, 0x41, 0x10, 0x13, 0x18, 0xB7, 0x29, 0x38, 0x9E, 0xFB, 0xB1, 0x9C, 0x06, 0x40, 0x00, 0x48,
    0x0A, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xBF, 0xD8, 0xF7,
    0xCC, 0x07, 0x88, 0x01, 0x35, 0x90, 0x01, 0x84, 0xB6, 0xF2, 0xA1, 0x0C, 0x12, 0x2F, 0x08, 0xF5,
    0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xB9, 0x30, 0x38, 0xAA, 0xB6, 0x99, 0xAC, 0x06, 0x40, 0x00,
    0x48, 0x1A, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xD9, 0xBD,
    0xBD, 0xB3, 0x02, 0x88, 0x01, 0x3E, 0x90, 0x01, 0x96, 0xDE, 0xF6, 0xA2, 0x0E, 0x12, 0x2F, 0x08,
    0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xBA, 0x30, 0x38, 0xAA, 0xB6, 0x99, 0xAC, 0x06, 0x40,
    0x00, 0x48, 0x1A, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xDC,
    0xF9, 0xC8, 0x89, 0x0F, 0x88, 0x01, 0x3E, 0x90, 0x01, 0xD2, 0xE7, 0x92, 0xD0, 0x0C, 0x12, 0x2F,
    0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xBB, 0x30, 0x38, 0xAA, 0xB6, 0x99, 0xAC, 0x06,
    0x40, 0x00, 0x48, 0x1A, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01,
    0xE8, 0xA1, 0x95, 0xD9, 0x0D, 0x88, 0x01, 0x3E, 0x90, 0x01, 0xD1, 0xAB, 0x9E, 0xE6, 0x03, 0x12,
    0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0x9D, 0x31, 0x38, 0x91, 0xFD, 0xAB, 0x9D,
    0x06, 0x40, 0x00, 0x48, 0x0C, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80,
    0x01, 0x94, 0xD7, 0xB3, 0xE0, 0x04, 0x88, 0x01, 0x3F, 0x90, 0x01, 0x80, 0xE9, 0xE1, 0xD8, 0x04,
    0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0x9E, 0x31, 0x38, 0x91, 0xFD, 0xAB,
    0x9D, 0x06, 0x40, 0x00, 0x48, 0x0C, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03,
    0x80, 0x01, 0xB2, 0x91, 0xCE, 0xA0, 0x0E, 0x88, 0x01, 0x3F, 0x90, 0x01, 0xCD, 0xDB, 0xE7, 0xC8,
    0x09, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0x9F, 0x31, 0x38, 0x91, 0xFD,
    0xAB, 0x9D, 0x06, 0x40, 0x00, 0x48, 0x0C, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78,
    0x03, 0x80, 0x01, 0x88, 0x93, 0xBA, 0xBF, 0x0F, 0x88, 0x01, 0x3F, 0x90, 0x01, 0x8A, 0xA1, 0x87,
    0xDF, 0x0C, 0x12, 0x2E, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xF5, 0x35, 0x38, 0xB1,
    0xB9, 0xB1, 0x9C, 0x06, 0x40, 0x00, 0x48, 0x08, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00,
    0x78, 0x03, 0x80, 0x01, 0xBF, 0xAE, 0xDE, 0xFC, 0x0B, 0x88, 0x01, 0x45, 0x90, 0x01, 0xFE, 0xE6,
    0x8B, 0x36, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xF6, 0x35, 0x38, 0xB1,
    0xB9, 0xB1, 0x9C, 0x06, 0x40, 0x00, 0x48, 0x08, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00,
    0x78, 0x03, 0x80, 0x01, 0xF1, 0xCD, 0xA4, 0xE2, 0x03, 0x88, 0x01, 0x45, 0x90, 0x01, 0x92, 0xFF,
    0x8A, 0x9F, 0x05, 0x12, 0x2E, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xF7, 0x35, 0x38,
    0xB1, 0xB9, 0xB1, 0x9C, 0x06, 0x40, 0x00, 0x48, 0x08, 0x50, 0x01, 0x58, 0x00, 0x60, 0x01, 0x70,
    0x00, 0x78, 0x03, 0x80, 0x01, 0x98, 0xEC, 0xD4, 0xB4, 0x0C, 0x88, 0x01, 0x45, 0x90, 0x01, 0x92,
    0xD6, 0x8F, 0x4E, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xCD, 0x3A, 0x38,
    0xE2, 0xFF, 0x94, 0xAC, 0x06, 0x40, 0x00, 0x48, 0x12, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70,
    0x00, 0x78, 0x03, 0x80, 0x01, 0xD9, 0xBD, 0xBD, 0xB3, 0x02, 0x88, 0x01, 0x4B, 0x90, 0x01, 0x96,
    0xDE, 0xF6, 0xA2, 0x0E, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xCE, 0x3A,
    0x38, 0xE2, 0xFF, 0x94, 0xAC, 0x06, 0x40, 0x00, 0x48, 0x12, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00,
    0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0x82, 0xB1, 0xC3, 0xB5, 0x05, 0x88, 0x01, 0x4B, 0x90, 0x01,
    0xB9, 0x8C, 0x83, 0xCA, 0x04, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xCF,
    0x3A, 0x38, 0xE2, 0xFF, 0x94, 0xAC, 0x06, 0x40, 0x00, 0x48, 0x12, 0x50, 0x01, 0x58, 0x00, 0x60,
    0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xB0, 0xD2, 0xC2, 0xA4, 0x09, 0x88, 0x01, 0x4B, 0x90,
    0x01, 0xE2, 0x86, 0xFC, 0xC6, 0x01, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18,
    0xA5, 0x3F, 0x38, 0x87, 0xB0, 0xF7, 0xAA, 0x06, 0x40, 0x00, 0x48, 0x0F, 0x50, 0x01, 0x58, 0x00,
    0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xE8, 0xA1, 0x95, 0xD9, 0x0D, 0x88, 0x01, 0x51,
    0x90, 0x01, 0xD1, 0xAB, 0x9E, 0xE6, 0x03, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13,
    0x18, 0xA6, 0x3F, 0x38, 0x87, 0xB0, 0xF7, 0xAA, 0x06, 0x40, 0x00, 0x48, 0x0F, 0x50, 0x01, 0x58,
    0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0x96, 0x8F, 0xE7, 0xB6, 0x08, 0x88, 0x01,
    0x51, 0x90, 0x01, 0xD4, 0xA2, 0x9C, 0x86, 0x07, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10,
    0x13, 0x18, 0xA7, 0x3F, 0x38, 0x87, 0xB0, 0xF7, 0xAA, 0x06, 0x40, 0x01, 0x48, 0x0F, 0x50, 0x01,
    0x58, 0x00, 0x60, 0x01, 0x70, 0x03, 0x78, 0x03, 0x80, 0x01, 0x96, 0xFA, 0x9F, 0x9D, 0x0C, 0x88,
    0x01, 0x51, 0x90, 0x01, 0xFD, 0xC8, 0x8C, 0xF0, 0x02, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41,
    0x10, 0x13, 0x18, 0xED, 0x40, 0x38, 0xD2, 0xA7, 0xDD, 0xAA, 0x06, 0x40, 0x00, 0x48, 0x0E, 0x50,
    0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xF4, 0xAD, 0xE1, 0xB8, 0x04,
    0x88, 0x01, 0x53, 0x90, 0x01, 0xB8, 0xA3, 0xCA, 0x99, 0x08, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86,
    0x41, 0x10, 0x13, 0x18, 0xEE, 0x40, 0x38, 0xD2, 0xA7, 0xDD, 0xAA, 0x06, 0x40, 0x00, 0x48, 0x0E,
    0x50, 0x01, 0x58, 0x00, 0x60, 0x01, 0x70, 0x02, 0x78, 0x03, 0x80, 0x01, 0xA8, 0xFA, 0xD4, 0xE4,
    0x03, 0x88, 0x01, 0x53, 0x90, 0x01, 0xB3, 0x90, 0xB6, 0xC5, 0x0E, 0x12, 0x2F, 0x08, 0xF5, 0xED,
    0x86, 0x41, 0x10, 0x13, 0x18, 0xEF, 0x40, 0x38, 0xD2, 0xA7, 0xDD, 0xAA, 0x06, 0x40, 0x00, 0x48,
    0x0E, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xF5, 0xFF, 0xE3,
    0xA9, 0x09, 0x88, 0x01, 0x53, 0x90, 0x01, 0x9C, 0xA1, 0x8B, 0xAE, 0x0A, 0x12, 0x2F, 0x08, 0xF5,
    0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xD1, 0x41, 0x38, 0x87, 0xCF, 0xC2, 0xBF, 0x06, 0x40, 0x00,
    0x48, 0x02, 0x50, 0x01, 0x58, 0x00, 0x60, 0x14, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xBF, 0x90,
    0xA7, 0xB3, 0x0F, 0x88, 0x01, 0x54, 0x90, 0x01, 0xC0, 0xF6, 0x84, 0xAB, 0x0E, 0x12, 0x2F, 0x08,
    0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xD2, 0x41, 0x38, 0x87, 0xCF, 0xC2, 0xBF, 0x06, 0x40,
    0x01, 0x48, 0x02, 0x50, 0x01, 0x58, 0x00, 0x60, 0x12, 0x70, 0x03, 0x78, 0x03, 0x80, 0x01, 0xD9,
    0xF8, 0xFE, 0xA8, 0x03, 0x88, 0x01, 0x54, 0x90, 0x01, 0xE0, 0xF3, 0xEC, 0x85, 0x05, 0x12, 0x2F,
    0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xD3, 0x41, 0x38, 0x87, 0xCF, 0xC2, 0xBF, 0x06,
    0x40, 0x00, 0x48, 0x02, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01,
    0xB4, 0xB8, 0xF9, 0xB3, 0x0E, 0x88, 0x01, 0x54, 0x90, 0x01, 0xC6, 0xF3, 0xE7, 0xC0, 0x08, 0x12,
    0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xB5, 0x42, 0x38, 0xE8, 0xCA, 0xCC, 0xAB,
    0x06, 0x40, 0x00, 0x48, 0x09, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80,
    0x01, 0x9F, 0xD3, 0xAF, 0x9B, 0x0D, 0x88, 0x01, 0x55, 0x90, 0x01, 0xBA, 0xBD, 0x8A, 0xEC, 0x03,
    0x12, 0x2E, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xB6, 0x42, 0x38, 0xE8, 0xCA, 0xCC,
    0xAB, 0x06, 0x40, 0x00, 0x48, 0x09, 0x50, 0x01, 0x58, 0x00, 0x60, 0x01, 0x70, 0x00, 0x78, 0x03,
    0x80, 0x01, 0x98, 0xBF, 0xD5, 0x85, 0x04, 0x88, 0x01, 0x55, 0x90, 0x01, 0xFA, 0xB2, 0xCE, 0x29,
    0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xB7, 0x42, 0x38, 0xE8, 0xCA, 0xCC,
    0xAB, 0x06, 0x40, 0x00, 0x48, 0x09, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03,
    0x80, 0x01, 0xF1, 0xCD, 0xA4, 0xE2, 0x03, 0x88, 0x01, 0x55, 0x90, 0x01, 0x92, 0xFF, 0x8A, 0x9F,
    0x05, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xE5, 0x4B, 0x38, 0xA2, 0xEA,
    0x93, 0xAC, 0x06, 0x40, 0x00, 0x48, 0x19, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78,
    0x03, 0x80, 0x01, 0xDD, 0xF7, 0xA8, 0xC3, 0x03, 0x88, 0x01, 0x61, 0x90, 0x01, 0xD4, 0xE0, 0x99,
    0xDF, 0x03, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xE6, 0x4B, 0x38, 0xA2,
    0xEA, 0x93, 0xAC, 0x06, 0x40, 0x00, 0x48, 0x19, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00,
    0x78, 0x03, 0x80, 0x01, 0x82, 0xD8, 0x95, 0xC3, 0x04, 0x88, 0x01, 0x61, 0x90, 0x01, 0xB8, 0xCA,
    0xD0, 0xEE, 0x01, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xE7, 0x4B, 0x38,
    0xA2, 0xEA, 0x93, 0xAC, 0x06, 0x40, 0x00, 0x48, 0x19, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70,
    0x00, 0x78, 0x03, 0x80, 0x01, 0x9B, 0x85, 0xB8, 0xA5, 0x08, 0x88, 0x01, 0x61, 0x90, 0x01, 0xFA,
    0xC3, 0xCC, 0xBC, 0x01, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xA1, 0x51,
    0x38, 0x87, 0x8A, 0x97, 0xAB, 0x06, 0x40, 0x00, 0x48, 0x11, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00,
    0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xE8, 0xA1, 0x95, 0xD9, 0x0D, 0x88, 0x01, 0x68, 0x90, 0x01,
    0xD1, 0xAB, 0x9E, 0xE6, 0x03, 0x12, 0x2E, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xA2,
    0x51, 0x38, 0x87, 0x8A, 0x97, 0xAB, 0x06, 0x40, 0x00, 0x48, 0x11, 0x50, 0x01, 0x58, 0x00, 0x60,
    0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0x95, 0xB0, 0xAF, 0xA9, 0x0D, 0x88, 0x01, 0x68, 0x90,
    0x01, 0xC4, 0xC5, 0xA7, 0x23, 0x12, 0x2E, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xA3,
    0x51, 0x38, 0x87, 0x8A, 0x97, 0xAB, 0x06, 0x40, 0x00, 0x48, 0x11, 0x50, 0x01, 0x58, 0x00, 0x60,
    0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0x98, 0xEC, 0xD4, 0xB4, 0x0C, 0x88, 0x01, 0x68, 0x90,
    0x01, 0x92, 0xD6, 0x8F, 0x4E, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xE9,
    0x52, 0x38, 0x9C, 0xD2, 0x9B, 0xAC, 0x06, 0x40, 0x00, 0x48, 0x1C, 0x50, 0x01, 0x58, 0x00, 0x60,
    0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xA0, 0x97, 0xC3, 0xC7, 0x0B, 0x88, 0x01, 0x6A, 0x90,
    0x01, 0xCB, 0x95, 0xE5, 0x9F, 0x0A, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18,
    0xEA, 0x52, 0x38, 0x9C, 0xD2, 0x9B, 0xAC, 0x06, 0x40, 0x00, 0x48, 0x1C, 0x50, 0x01, 0x58, 0x00,
    0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0x88, 0x93, 0xBA, 0xBF, 0x0F, 0x88, 0x01, 0x6A,
    0x90, 0x01, 0x8A, 0xA1, 0x87, 0xDF, 0x0C, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13,
    0x18, 0xEB, 0x52, 0x38, 0x9C, 0xD2, 0x9B, 0xAC, 0x06, 0x40, 0x00, 0x48, 0x1C, 0x50, 0x01, 0x58,
    0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xC3, 0xB1, 0xD3, 0xC1, 0x0E, 0x88, 0x01,
    0x6A, 0x90, 0x01, 0xC6, 0x90, 0x83, 0x81, 0x01, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10,
    0x13, 0x18, 0x89, 0x59, 0x38, 0xC1, 0x94, 0xE1, 0x9B, 0x06, 0x40, 0x00, 0x48, 0x04, 0x50, 0x01,
    0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xE8, 0xA1, 0x95, 0xD9, 0x0D, 0x88,
    0x01, 0x72, 0x90, 0x01, 0xD1, 0xAB, 0x9E, 0xE6, 0x03, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86, 0x41,
    0x10, 0x13, 0x18, 0x8A, 0x59, 0x38, 0xC1, 0x94, 0xE1, 0x9B, 0x06, 0x40, 0x00, 0x48, 0x04, 0x50,
    0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xD9, 0xBD, 0xBD, 0xB3, 0x02,
    0x88, 0x01, 0x72, 0x90, 0x01, 0x96, 0xDE, 0xF6, 0xA2, 0x0E, 0x12, 0x2F, 0x08, 0xF5, 0xED, 0x86,
    0x41, 0x10, 0x13, 0x18, 0x8B, 0x59, 0x38, 0xC1, 0x94, 0xE1, 0x9B, 0x06, 0x40, 0x00, 0x48, 0x04,
    0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0x9B, 0xEC, 0x9F, 0xF8,
    0x04, 0x88, 0x01, 0x72, 0x90, 0x01, 0xC1, 0xDE, 0xB0, 0x85, 0x03, 0x12, 0x30, 0x08, 0xF5, 0xED,
    0x86, 0x41, 0x10, 0x13, 0x18, 0x81, 0x64, 0x38, 0xEE, 0xDF, 0x99, 0xAC, 0x06, 0x40, 0x00, 0x48,
    0x1B, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xE6, 0xFA, 0xE5,
    0x98, 0x0E, 0x88, 0x01, 0x80, 0x01, 0x90, 0x01, 0xF2, 0x91, 0xF8, 0xE5, 0x0E, 0x12, 0x30, 0x08,
    0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0x82, 0x64, 0x38, 0xEE, 0xDF, 0x99, 0xAC, 0x06, 0x40,
    0x00, 0x48, 0x1B, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0x9F,
    0x96, 0xAE, 0xDC, 0x01, 0x88, 0x01, 0x80, 0x01, 0x90, 0x01, 0xF5, 0xC9, 0xC5, 0xD4, 0x0E, 0x12,
    0x30, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0x83, 0x64, 0x38, 0xEE, 0xDF, 0x99, 0xAC,
    0x06, 0x40, 0x00, 0x48, 0x1B, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78, 0x03, 0x80,
    0x01, 0x9F, 0xC4, 0xB2, 0xFA, 0x0E, 0x88, 0x01, 0x80, 0x01, 0x90, 0x01, 0x9F, 0xC4, 0xB2, 0xFA,
    0x0E, 0x12, 0x30, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xE9, 0x6B, 0x38, 0xD5, 0xAE,
    0xE9, 0xA9, 0x06, 0x40, 0x00, 0x48, 0x0D, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70, 0x00, 0x78,
    0x03, 0x80, 0x01, 0xA2, 0xBB, 0xFC, 0xA2, 0x0C, 0x88, 0x01, 0x8A, 0x01, 0x90, 0x01, 0xD2, 0xDE,
    0x8D, 0xF6, 0x0B, 0x12, 0x30, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xEA, 0x6B, 0x38,
    0xD5, 0xAE, 0xE9, 0xA9, 0x06, 0x40, 0x00, 0x48, 0x0D, 0x50, 0x01, 0x58, 0x00, 0x60, 0x00, 0x70,
    0x00, 0x78, 0x03, 0x80, 0x01, 0xD9, 0xBD, 0xBD, 0xB3, 0x02, 0x88, 0x01, 0x8A, 0x01, 0x90, 0x01,
    0x96, 0xDE, 0xF6, 0xA2, 0x0E, 0x12, 0x30, 0x08, 0xF5, 0xED, 0x86, 0x41, 0x10, 0x13, 0x18, 0xEB,
    0x6B, 0x38, 0xD5, 0xAE, 0xE9, 0xA9, 0x06, 0x40, 0x00, 0x48, 0x0D, 0x50, 0x01, 0x58, 0x00, 0x60,
    0x00, 0x70, 0x00, 0x78, 0x03, 0x80, 0x01, 0xB2, 0xEF, 0xA2, 0xD8, 0x0A, 0x88, 0x01, 0x8A, 0x01,
    0x90, 0x01, 0xFA, 0x84, 0x92, 0x98, 0x02, 0x19, 0x11, 0xED, 0x99, 0xC8, 0x7E, 0xD5, 0x69, 0x09,
    0x22, 0x0C, 0x08, 0x01, 0x10, 0xF5, 0xED, 0x86, 0xC1, 0x90, 0x80, 0x80, 0x88, 0x01, 0x28, 0x01,
    0x30, 0x00, 0x39, 0xB1, 0x4D, 0xA7, 0x55, 0x7F, 0xD5, 0x69, 0x00,
};

// Removed: GBE_kDota7388Profile20Template (had owned=0, caused "Unavailable")
// Removed: GBE_kDota7388Profile37Template (had owned=1 but used hardcoded account_id)
// All 7387->7388 event queries now use gbe::gc_message::build_dota_7388_minimal_response_payload.

static const uint8 GBE_kDotaPracticeLobbyCacheSubscribedTemplate[] = {
    0x18, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x12, 0xC2, 0x01, 0x08, 0xD4, 0x0F, 0x12, 0xBC,
    0x01, 0x08, 0x9D, 0x97, 0xF8, 0x9E, 0x95, 0xD7, 0xF7, 0x34, 0x18, 0x01, 0x20, 0x00, 0x59, 0xF5,
    0xB6, 0x21, 0x08, 0x01, 0x00, 0x10, 0x01, 0x60, 0x01, 0x68, 0x00, 0x70, 0x01, 0x82, 0x01, 0x05,
    0x30, 0x30, 0x31, 0x32, 0x35, 0xA8, 0x01, 0x05, 0xE0, 0x01, 0x00, 0xF8, 0x01, 0x01, 0xA0, 0x02,
    0x04, 0xD0, 0x02, 0x00, 0xD8, 0x02, 0x00, 0xE0, 0x02, 0x00, 0xF0, 0x02, 0x00, 0xF8, 0x02, 0x00,
    0x80, 0x03, 0x00, 0x98, 0x03, 0x00, 0xA8, 0x03, 0x00, 0xC8, 0x03, 0x00, 0xF2, 0x03, 0x07, 0x08,
    0xF5, 0x44, 0x12, 0x02, 0x08, 0x00, 0xD8, 0x04, 0x00, 0x90, 0x05, 0x00, 0xC0, 0x05, 0x00, 0xE8,
    0x05, 0x04, 0xF0, 0x05, 0x8A, 0xB6, 0xFB, 0x8B, 0x0C, 0xF8, 0x05, 0xCD, 0xFB, 0x92, 0xDA, 0x0A,
    0x88, 0x06, 0x00, 0xF0, 0x06, 0x00, 0x88, 0x07, 0x00, 0xC2, 0x07, 0x10, 0x09, 0xF5, 0xB6, 0x21,
    0x08, 0x01, 0x00, 0x10, 0x01, 0x18, 0x00, 0x38, 0x01, 0x80, 0x01, 0x01, 0xC8, 0x07, 0x00, 0xE0,
    0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07,
    0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00,
    0xE0, 0x07, 0x00, 0xF8, 0x07, 0x00, 0x80, 0x08, 0xF2, 0xA7, 0xFE, 0xCE, 0x06, 0x12, 0x05, 0x08,
    0xDD, 0x0F, 0x12, 0x00, 0x12, 0x12, 0x08, 0xDE, 0x0F, 0x12, 0x0D, 0x0A, 0x09, 0x0A, 0x07, 0x53,
    0x76, 0x65, 0x6E, 0x6D, 0x61, 0x78, 0x10, 0x00, 0x12, 0x07, 0x08, 0xDF, 0x0F, 0x12, 0x02, 0x0A,
    0x00, 0x12, 0x2F, 0x08, 0xE0, 0x0F, 0x12, 0x2A, 0x0A, 0x23, 0x09, 0xF5, 0xB6, 0x21, 0x08, 0x01,
    0x00, 0x10, 0x01, 0x48, 0x00, 0x58, 0x00, 0x60, 0xE1, 0xAC, 0x8B, 0x84, 0xD0, 0x85, 0x40, 0x68,
    0x00, 0x98, 0x01, 0x00, 0x98, 0x01, 0x00, 0x98, 0x01, 0x00, 0x98, 0x01, 0x00, 0x15, 0x00, 0x00,
    0x00, 0x00, 0x19, 0x74, 0x1F, 0xDE, 0x53, 0xB9, 0xDE, 0x69, 0x00, 0x22, 0x0B, 0x08, 0x03, 0x10,
    0x9D, 0x97, 0xF8, 0x9E, 0x95, 0xD7, 0xF7, 0x34,
};

#pragma pack( push, 1 )
//-----------------------------------------------------------------------------
// Purpose: Header for messages from a client or gameserver to or from the GC
//-----------------------------------------------------------------------------
struct GCMsgHdr_t
{
    uint32  m_eMsg;                     // The message type
    uint64  m_ulSteamID;                // User's SteamID
};

//-----------------------------------------------------------------------------
// Purpose: Header for messages from a client or gameserver to or from the GC
//          That contains source and destination jobs for the purpose of
//          replying messages.
//-----------------------------------------------------------------------------
struct GCMsgHdrEx_t
{
    uint32  m_eMsg;                     // The message type
    uint64  m_ulSteamID;                // User's SteamID
    uint16  m_nHdrVersion;
    JobID_t m_JobIDTarget;
    JobID_t m_JobIDSource;
};

#pragma pack(pop)


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

template <class T>
static T deser_var(const char *&p)
{
    T output;
    memcpy(&output, p, sizeof(T));
    p += sizeof(T);
    return output;
}

struct GBE_DotaHelloContext
{
    bool valid{};
    uint32 version{};
    std::string outer_session_field_raw;
    uint64 source_job_id{};
    bool has_source_job{};
};

GBE_DotaServerHelloContext GBE_last_dota_server_hello_context;

using GBE_DotaPracticeLobbyDetailsRequest = gbe::proto_wire::DotaPracticeLobbyDetailsRequest;
using GBE_DotaPracticeLobbyCreateRequest = gbe::proto_wire::DotaPracticeLobbyCreateRequest;
using GBE_DotaPracticeLobbyJoinRequest = gbe::proto_wire::DotaPracticeLobbyJoinRequest;
using GBE_DotaInviteToLobbyRequest = gbe::proto_wire::DotaInviteToLobbyRequest;
using GBE_DotaLobbyInviteResponseRequest = gbe::proto_wire::DotaLobbyInviteResponseRequest;
using GBE_DotaPracticeLobbySetTeamSlotRequest = gbe::proto_wire::DotaPracticeLobbySetTeamSlotRequest;
using GBE_DotaPracticeLobbyKickRequest = gbe::proto_wire::DotaPracticeLobbyKickRequest;

using GBE_DotaPracticeLobbyBroadcastChannelRequest = gbe::proto_wire::DotaPracticeLobbyBroadcastChannelRequest;

using GBE_DotaJoinChatChannelRequest = gbe::proto_wire::DotaJoinChatChannelRequest;
using GBE_DotaLeaveChatChannelRequest = gbe::proto_wire::DotaLeaveChatChannelRequest;
using GBE_DotaChatMessageRequest = gbe::proto_wire::DotaChatMessageRequest;

void GBE_GC_DebugLog(const char *scope, const char *fmt, ...)
{
    const char *log_scope = scope ? scope : "GC";
    if (std::strcmp(log_scope, "GC_SEND") == 0 ||
        std::strcmp(log_scope, "GC_SEND_DOTA") == 0 ||
        std::strcmp(log_scope, "GC_CONFIG") == 0 ||
        std::strcmp(log_scope, "GC_INIT") == 0 ||
        std::strcmp(log_scope, "CREATE_INTERFACE") == 0 ||
        std::strcmp(log_scope, "NETSOCK_CTOR") == 0) {
        return;
    }

    FILE *file = std::fopen(GBE_kGcDebugLogPath, "a");
    if (!file)
        return;

    std::fprintf(file, "[%s] ", log_scope);

    va_list args;
    va_start(args, fmt);
    std::vfprintf(file, fmt, args);
    va_end(args);

    std::fprintf(file, "\n");
    std::fclose(file);
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

using GBE_Dota8053Result = gbe::proto_wire::Dota8053Result;

using GBE_DotaEmptyRequestShape = gbe::proto_wire::DotaEmptyRequestShape;
using GBE_DotaRankRequestShape = gbe::proto_wire::DotaRankRequestShape;
using GBE_Dota7034ConnectedPlayer = gbe::proto_wire::Dota7034ConnectedPlayer;
using GBE_Dota7034DisconnectedPlayer = gbe::proto_wire::Dota7034DisconnectedPlayer;
using GBE_Dota7034RequestShape = gbe::proto_wire::Dota7034RequestShape;

std::string GBE_DotaCustomGameDisplayName(class Settings *settings, const GBE_DotaCustomGameDetails &custom_game, const std::string &fallback)
{
    if (settings && custom_game.game_id != 0ull && settings->isModInstalled(static_cast<PublishedFileId_t>(custom_game.game_id))) {
        Mod_entry mod = settings->getMod(static_cast<PublishedFileId_t>(custom_game.game_id));
        std::string display_name = gbe::dota_custom_game::metadata_value_for_gc(mod.metadata, "display_name", mod.title);
        if (gbe::proto_wire::dota_is_readable_custom_game_name(display_name))
            return display_name;
        display_name = gbe::dota_custom_game::metadata_value_for_gc(mod.metadata, "map_name", "");
        if (gbe::proto_wire::dota_is_readable_custom_game_name(display_name))
            return display_name;
        display_name = gbe::dota_custom_game::metadata_value_for_gc(mod.metadata, "addon_name", mod.title);
        if (gbe::proto_wire::dota_is_readable_custom_game_name(display_name))
            return display_name;
    }

    return gbe::dota_custom_game::custom_game_display_name_from_details(custom_game, fallback);
}

static void GBE_LogGCProtoBoundary(const char *scope, const char *direction, void *self, bool is_server, uint32 emsg, const void *data, uint32 size)
{
    if (!scope || !direction || !data || size < sizeof(ProtoBufMsgHeader_t) || !gbe::gc_message::should_trace_dota_proto_boundary(emsg))
        return;

    GBE_DirectProtoContext context{};
    if (!GBE_ParseDirectProtoContext(data, size, context)) {
        GBE_GC_DebugLog(
            scope,
            "%s this=%p is_server=%u emsg=%u size=%u parse=0",
            direction,
            self,
            is_server ? 1u : 0u,
            emsg,
            size
        );
        return;
    }

    GBE_GC_DebugLog(
        scope,
        "%s this=%p is_server=%u emsg=%u size=%u ext=%u body=%zu has_job_src=%u job_src=%llu has_job_tgt=%u job_tgt=%llu client_steam_id=%llu session=%d app_id=%u",
        direction,
        self,
        is_server ? 1u : 0u,
        emsg,
        size,
        context.hdr.m_cubProtoBufExtHdr,
        context.body_size,
        context.protohdr.has_job_id_source() ? 1u : 0u,
        static_cast<unsigned long long>(context.protohdr.has_job_id_source() ? context.protohdr.job_id_source() : 0ull),
        context.protohdr.has_job_id_target() ? 1u : 0u,
        static_cast<unsigned long long>(context.protohdr.has_job_id_target() ? context.protohdr.job_id_target() : 0ull),
        static_cast<unsigned long long>(context.protohdr.has_client_steam_id() ? context.protohdr.client_steam_id() : 0ull),
        context.protohdr.has_client_session_id() ? context.protohdr.client_session_id() : 0,
        context.protohdr.has_source_app_id() ? context.protohdr.source_app_id() : 0u
    );
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

static bool GBE_PatchDotaLobbyTemplateIdentifiers(std::string &message, uint32 account_id, uint64 steam_id, uint64 lobby_id)
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

static bool GBE_PatchDotaLobbyTemplateIdentifiersIfPresent(std::string &message, uint64 steam_id, uint64 lobby_id)
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

static bool GBE_ForceDotaLobbyCacheOwnerSOID(std::string &message, uint64 lobby_id)
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

static bool GBE_ForceDotaLobbyUpdateOwnerSOID(std::string &message, uint64 lobby_id)
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

void GBE_LogDotaSOCacheSubscribedSummary(const char *tag, const char *label, const std::string &message)
{
    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), proto_context))
        return;

    CMsgSOCacheSubscribed protomsg;
    if (!protomsg.ParseFromArray(proto_context.body, static_cast<int>(proto_context.body_size)))
        return;

    GBE_GC_DebugLog(
        tag,
        "%s owner_type=%u owner_id=%llu objects=%d version_present=%u version=%llu service_id_present=%u service_id=%u service_list_count=%d sync_version_present=%u sync_version=%llu",
        label ? label : "dota_cache_subscribed_summary",
        protomsg.has_owner_soid() ? protomsg.owner_soid().type() : 0u,
        static_cast<unsigned long long>(protomsg.has_owner_soid() ? protomsg.owner_soid().id() : 0ull),
        protomsg.objects_size(),
        protomsg.has_version() ? 1u : 0u,
        static_cast<unsigned long long>(protomsg.has_version() ? protomsg.version() : 0ull),
        protomsg.has_service_id() ? 1u : 0u,
        protomsg.has_service_id() ? protomsg.service_id() : 0u,
        protomsg.service_list_size(),
        protomsg.has_sync_version() ? 1u : 0u,
        static_cast<unsigned long long>(protomsg.has_sync_version() ? protomsg.sync_version() : 0ull)
    );

    for (int object_index = 0; object_index < protomsg.objects_size(); ++object_index) {
        const auto &object = protomsg.objects(object_index);
        GBE_GC_DebugLog(
            tag,
            "%s object[%d] type=%d object_data_count=%d",
            label ? label : "dota_cache_subscribed_summary",
            object_index,
            object.type_id(),
            object.object_data_size()
        );

        for (int data_index = 0; data_index < object.object_data_size(); ++data_index) {
            const std::string &object_data = object.object_data(data_index);
            GBE_GC_DebugLog(
                tag,
                "%s object[%d] data[%d] size=%zu",
                label ? label : "dota_cache_subscribed_summary",
                object_index,
                data_index,
                object_data.size()
            );

            if (object.type_id() == 2004) {
                const uint8 *object_bytes = reinterpret_cast<const uint8 *>(object_data.data());
                const size_t object_size = object_data.size();
                uint64 lobby_id = 0;
                uint32 lobby_state = 0;
                std::string connect;
                uint64 server_id = 0;
                uint32 game_state = 0;
                uint64 match_id = 0;
                uint32 game_start_time = 0;
                const uint32 team_details_count = gbe::proto_wire::count_repeated_bytes_field(object_data, 17u);
                std::string owner_state;
                const bool has_connect = gbe::proto_wire::read_bytes_field(object_bytes, object_size, 5u, connect);
                const bool has_owner_state = gbe::proto_wire::read_bytes_field(object_bytes, object_size, 120u, owner_state);
                gbe::proto_wire::read_uint64_field(object_bytes, object_size, 1u, lobby_id);
                gbe::proto_wire::read_uint32_field(object_bytes, object_size, 4u, lobby_state);
                gbe::proto_wire::read_uint64_field(object_bytes, object_size, 6u, server_id);
                gbe::proto_wire::read_uint32_field(object_bytes, object_size, 22u, game_state);
                gbe::proto_wire::read_uint64_field(object_bytes, object_size, 30u, match_id);
                gbe::proto_wire::read_uint32_field(object_bytes, object_size, 87u, game_start_time);
                GBE_GC_DebugLog(
                    tag,
                    "%s object[%d] data[%d] type=2004 lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu game_start_time=%u connect=%s team_details=%u",
                    label ? label : "dota_cache_subscribed_summary",
                    object_index,
                    data_index,
                    static_cast<unsigned long long>(lobby_id),
                    lobby_state,
                    game_state,
                    static_cast<unsigned long long>(match_id),
                    static_cast<unsigned long long>(server_id),
                    game_start_time,
                    has_connect ? connect.c_str() : "",
                    team_details_count
                );
                if (has_owner_state) {
                    GBE_GC_DebugLog(
                        tag,
                        "%s object[%d] data[%d] type=2004 owner_state{%s}",
                        label ? label : "dota_cache_subscribed_summary",
                        object_index,
                        data_index,
                        gbe::proto_wire::format_dota_lobby_member_state_summary(owner_state).c_str()
                    );
                }
                continue;
            }

            if (object.type_id() == 2014) {
                const uint32 member_count = gbe::proto_wire::count_repeated_bytes_field(object_data, 1u);
                std::string first_member;
                if (!gbe::proto_wire::read_bytes_field(reinterpret_cast<const uint8 *>(object_data.data()), object_data.size(), 1u, first_member)) {
                    first_member.clear();
                }

                GBE_GC_DebugLog(
                    tag,
                    "%s object[%d] data[%d] type=2014 member_count=%u",
                    label ? label : "dota_cache_subscribed_summary",
                    object_index,
                    data_index,
                    member_count
                );
                if (!first_member.empty()) {
                    GBE_GC_DebugLog(
                        tag,
                        "%s object[%d] data[%d] type=2014 member[0]{%s}",
                        label ? label : "dota_cache_subscribed_summary",
                        object_index,
                        data_index,
                        gbe::proto_wire::format_dota_static_lobby_member_summary(first_member).c_str()
                    );
                }
                continue;
            }

            if (object.type_id() == 2016) {
                const uint8 *object_bytes = reinterpret_cast<const uint8 *>(object_data.data());
                const size_t object_size = object_data.size();
                uint32 member_count = 0;
                std::string first_member;
                size_t offset = 0;
                while (offset < object_size) {
                    gbe::proto_wire::Field field{};
                    size_t field_offset = 0;
                    size_t field_end = 0;
                    if (!gbe::proto_wire::read_next_field(object_bytes, object_size, offset, field, &field_offset, &field_end))
                        break;
                    if (field.number == 1u && field.wire_type == 2u) {
                        ++member_count;
                        if (first_member.empty())
                            first_member.assign(object_data.data() + field.value_offset, field.value_size);
                    }
                    offset = field_end;
                }

                GBE_GC_DebugLog(
                    tag,
                    "%s object[%d] data[%d] type=2016 member_count=%u",
                    label ? label : "dota_cache_subscribed_summary",
                    object_index,
                    data_index,
                    member_count
                );
                if (!first_member.empty()) {
                    GBE_GC_DebugLog(
                        tag,
                        "%s object[%d] data[%d] type=2016 member[0]{%s}",
                        label ? label : "dota_cache_subscribed_summary",
                        object_index,
                        data_index,
                        gbe::proto_wire::format_dota_server_static_lobby_member_summary(first_member).c_str()
                    );
                }
            }
        }
    }
}

static bool GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
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
    const GBE_DotaCustomGameDetails *custom_game = nullptr)
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

static uint64 GBE_GenerateDotaPostGameChatChannelId()
{
    std::random_device device;
    std::mt19937_64 generator(
        (static_cast<uint64>(device()) << 32) ^
        static_cast<uint64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );

    const uint64 candidate = 0x62F000ull + (generator() & 0x0000000000000FFFull);
    return candidate != 0 ? candidate : 0x62FEB8ull;
}

static bool GBE_PatchDotaPracticeLobbyLaunchTemplate(
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

bool GBE_ReplayDotaPracticeLobbyOfficial26Payload(
    const char *wrapped_template_hex,
    const char *stage_note,
    uint32 account_id,
    uint64 steam_id,
    uint64 lobby_id,
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
    const std::string &pass_key,
    uint32 lobby_state,
    uint32 lobby_game_state,
    bool rewrite_2015,
    uint32 extra_startup_account_id,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game)
{
    if (!wrapped_template_hex)
        return false;

    std::string wrapped_message;
    if (!gbe::proto_wire::decode_hex_string(wrapped_template_hex, wrapped_message))
        return false;

    std::string inner_payload;
    if (!GBE_ExtractWrappedClientFromGCPayload(wrapped_message, GBE_kDotaPracticeLobbyDetailsUpdate, inner_payload))
        return false;

    if (!GBE_PatchDotaPracticeLobbyLaunchTemplate(
            inner_payload,
            account_id,
            steam_id,
            lobby_id,
            server_id,
            match_id,
            game_start_time,
            connect,
            true,
            true,
            true,
            stage_note))
        return false;

    if (!GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
            inner_payload,
            account_id,
            steam_id,
            lobby_id,
            true,
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
            std::vector<GBE_DotaLobbyMemberState>(),
            rewrite_2015,
            extra_startup_account_id,
            pass_key,
            custom_game))
        return false;

    message.swap(inner_payload);
    return GBE_ForceDotaLobbyUpdateOwnerSOID(message, lobby_id);
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

void GBE_LogDotaResponsePacket(
    const char *reason,
    uint32 inner_emsg,
    bool wrapped,
    const std::string &inner_payload,
    const std::string &outbound_payload,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state)
{
    const std::string body_prefix = gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(inner_payload.data()), inner_payload.size(), 32);
    const std::string packet_prefix = wrapped
        ? gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(outbound_payload.data()), outbound_payload.size(), 32)
        : "-";
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Sent response reason=%s path=%s inner_emsg=%u wrapped=%u payload_size=%zu lobby_id=%llu state=%u game_state=%u body_prefix=%s packet_prefix=%s",
        reason ? reason : "unknown",
        wrapped ? "wrapped" : "direct",
        inner_emsg,
        wrapped ? 1u : 0u,
        outbound_payload.size(),
        static_cast<unsigned long long>(lobby_id),
        lobby_state,
        lobby_game_state,
        body_prefix.c_str(),
        packet_prefix.c_str());
}

static bool GBE_AdaptDotaTopCustomGamesListPayload(class Settings *settings, std::string &message, size_t &game_count)
{
    std::vector<std::uint64_t> mod_ids;
    if (settings) {
        for (PublishedFileId_t mod_id : settings->modSet()) {
            if (mod_id == 0ull || mod_id == k_PublishedFileIdInvalid)
                continue;
            mod_ids.push_back(static_cast<std::uint64_t>(mod_id));
        }
    }
    return gbe::gc_message::build_dota_top_custom_games_list_payload(mod_ids, message, game_count);
}

bool GBE_AdaptDotaJoinChatChannelResponsePayload(
    uint64 steam_id,
    uint64 generic_lobby_id,
    uint64 channel_id,
    const std::string &channel_name,
    const std::string &player_name,
    const std::vector<GBE_DotaLobbyMemberState> &channel_members,
    uint64 owner_steam_id,
    const std::string &owner_name,
    uint32 channel_type,
    std::string &message)
{
    std::vector<GBE_DotaChatMemberState> resolved_remote_names;
    for (const GBE_DotaLobbyMemberState &channel_member : channel_members) {
        if (channel_member.steam_id == steam_id)
            continue;
        std::string generic_member_name;
        std::string friend_member_name;
        Steam_Client *steam_client = get_steam_client();
        if (steam_client && steam_client->steam_matchmaking && generic_lobby_id != 0ull) {
            CSteamID generic_lobby((uint64)generic_lobby_id);
            CSteamID member_id((uint64)channel_member.steam_id);
            if (generic_lobby.IsLobby() && member_id.IsValid()) {
                const char *generic_name = steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby, member_id, GBE_kDotaGenericLobbyMemberNameKey);
                if (generic_name && generic_name[0] != '\0')
                    generic_member_name = generic_name;
            }
        }

        if (steam_client && steam_client->steam_friends) {
            const char *friend_name = steam_client->steam_friends->GetFriendPersonaName(CSteamID((uint64)channel_member.steam_id));
            if (friend_name && friend_name[0] != '\0')
                friend_member_name = friend_name;
        }

        resolved_remote_names.push_back({
            channel_member.steam_id,
            gbe::dota_lobby_flow::resolve_chat_member_display_name(
                channel_member.steam_id,
                steam_id,
                player_name,
                owner_steam_id,
                owner_name,
                generic_member_name,
                friend_member_name,
                std::string()) });
    }

    const std::vector<GBE_DotaChatMemberState> chat_member_states = gbe::dota_lobby_flow::compose_join_chat_channel_members(
        steam_id,
        player_name,
        channel_members,
        owner_steam_id,
        owner_name,
        resolved_remote_names);
    std::vector<gbe::gc_message::DotaChatMember> chat_members;
    chat_members.reserve(chat_member_states.size());
    for (const GBE_DotaChatMemberState &chat_member : chat_member_states) {
        chat_members.push_back({ chat_member.steam_id, chat_member.name });
    }

    return gbe::gc_message::build_dota_join_chat_channel_response_payload(channel_id, channel_name, chat_members, channel_type, message);
}

static bool GBE_IsDotaOtherLeftChannelPayloadForChannel(const std::string &message, uint64 channel_id)
{
    if (message.size() < 8u)
        return false;

    const uint8 *bytes = reinterpret_cast<const uint8 *>(message.data());
    uint32 inner_emsg = 0;
    uint32 header_length = 0;
    std::memcpy(&inner_emsg, bytes, sizeof(inner_emsg));
    std::memcpy(&header_length, bytes + sizeof(inner_emsg), sizeof(header_length));
    if (GBE_GC_MaskedEMsg(inner_emsg) != GBE_kDotaOtherLeftChannel)
        return false;

    const size_t body_offset = 8u + header_length;
    if (body_offset + 9u > message.size())
        return false;

    const uint8 *body = bytes + body_offset;
    if (body[0] != 0x09u)
        return false;

    uint64 payload_channel_id = 0;
    std::memcpy(&payload_channel_id, body + 1u, sizeof(payload_channel_id));
    return payload_channel_id == channel_id;
}

bool GBE_PushDotaPlayerEquippedItemsCacheToGC(
    Steam_Game_Coordinator *target_gc,
    const CSteamID &player_steam_id,
    const std::vector<Econ_Item> &source_items,
    bool unsubscribe_first,
    const char *reason)
{
    if (!target_gc || !player_steam_id.BIndividualAccount())
        return false;

    std::vector<const Econ_Item *> equipped_items;
    for (const Econ_Item &item : source_items) {
        if (!item.equip_states.empty())
            equipped_items.push_back(&item);
    }

    if (equipped_items.empty())
        return false;

    const uint64 player_steam64 = player_steam_id.ConvertToUint64();

    if (unsubscribe_first) {
        std::string unsub_message;
        gbe::gc_message::build_dota_so_owner_cache_unsubscribed_payload(1u, player_steam64, unsub_message);
        target_gc->push_incoming_message(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, unsub_message);
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "pushed player CacheUnsubscribed to target GC: steam64=%llu reason=%s message_size=%zu",
            static_cast<unsigned long long>(player_steam64),
            reason ? reason : "unknown",
            unsub_message.size()
        );
    }

    std::string owner_soid;
    gbe::proto_wire::append_varint_field(owner_soid, 1u, 1u);
    gbe::proto_wire::append_varint_field(owner_soid, 2u, player_steam64);

    std::string subscribed_type;
    gbe::proto_wire::append_varint_field(subscribed_type, 1u, 1u);
    for (const Econ_Item *ep : equipped_items) {
        std::string serialized = target_gc->serialize_item_to_gcprotobuf(*ep, player_steam_id);
        gbe::proto_wire::append_bytes_field(subscribed_type, 2u, serialized);
    }

    std::string cache_body;
    gbe::proto_wire::append_bytes_field(cache_body, 2u, subscribed_type);
    gbe::proto_wire::append_fixed64_field(cache_body, 3u, 1ull);
    gbe::proto_wire::append_bytes_field(cache_body, 4u, owner_soid);

    std::string cache_message;
    gbe::gc_message::build_dota_zero_header_payload(GBE_kDotaCacheSubscribed, cache_body, cache_message);
    target_gc->push_incoming_message(GBE_kDotaCacheSubscribed | GBE_kProtoMask, cache_message);

    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "pushed player item CacheSubscribed to target GC: steam64=%llu equipped_items=%zu reason=%s message_size=%zu unsub_first=%u",
        static_cast<unsigned long long>(player_steam64),
        equipped_items.size(),
        reason ? reason : "unknown",
        cache_message.size(),
        unsubscribe_first ? 1u : 0u
    );
    return true;
}

static void GBE_ComposeDotaPracticeLobbySOObjects(
    uint64 steam_id,
    uint64 lobby_id,
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
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    const GBE_DotaCustomGameDetails *custom_game,
    gbe::gc_message::DotaPracticeLobbyObjects &lobby_objects)
{
    static const uint8 GBE_kDotaLobbyField62Value[] = { 0x08, 0xF5, 0x44, 0x12, 0x02, 0x08, 0x00 };

    const bool has_custom_game = custom_game && custom_game->game_id != 0ull;
    std::vector<GBE_DotaLobbyMemberState> effective_members = gbe::dota_lobby_flow::compose_lobby_members(
        steam_id,
        extra_startup_account_id,
        owner_team,
        owner_slot,
        owner_hero_id,
        true,
        GBE_kDotaTeamPlayerPool,
        members);
    if (has_custom_game) {
        effective_members.erase(
            std::remove_if(effective_members.begin(), effective_members.end(), [](const GBE_DotaLobbyMemberState &member) { return member.steam_id == 0ull; }),
            effective_members.end());
    }

    const std::string normalized_connect = gbe::dota_custom_game::format_practice_lobby_connect_for_custom_game(connect, custom_game);

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Building 2004 team details lobby_id=%llu state=%u game_state=%u custom_game_id=%llu team_details=%u",
        static_cast<unsigned long long>(lobby_id),
        lobby_state,
        lobby_game_state,
        static_cast<unsigned long long>(has_custom_game ? custom_game->game_id : 0ull),
        lobby_state != 0u ? 2u : 0u
    );

    gbe::gc_message::DotaCustomGameDetails parsed_custom_game{};
    const gbe::gc_message::DotaCustomGameDetails *parsed_custom_game_ptr = nullptr;
    if (custom_game) {
        parsed_custom_game.mode = custom_game->mode;
        parsed_custom_game.map_name = custom_game->map_name;
        parsed_custom_game.difficulty = custom_game->difficulty;
        parsed_custom_game.game_id = custom_game->game_id;
        parsed_custom_game.min_players = custom_game->min_players;
        parsed_custom_game.max_players = custom_game->max_players;
        parsed_custom_game.crc = custom_game->crc;
        parsed_custom_game.timestamp = custom_game->timestamp;
        parsed_custom_game.penalties = custom_game->penalties;
        parsed_custom_game_ptr = &parsed_custom_game;
    }

    std::vector<gbe::gc_message::DotaLobbyMemberObjectState> parsed_members;
    parsed_members.reserve(effective_members.size());
    std::vector<gbe::gc_message::DotaStaticLobbyMember> static_lobby_members;
    static_lobby_members.reserve(effective_members.size());
    for (const GBE_DotaLobbyMemberState &member : effective_members) {
        parsed_members.push_back(gbe::gc_message::DotaLobbyMemberObjectState{
            member.steam_id,
            member.team,
            member.slot,
            member.hero_id,
            member.connected,
            member.leaver_status});
        static_lobby_members.push_back(gbe::gc_message::DotaStaticLobbyMember{
            member.steam_id,
            member.account_id != 0u ? member.account_id : (member.steam_id != 0ull ? CSteamID((uint64)member.steam_id).GetAccountID() : 0u),
            member.connected});
    }

    gbe::gc_message::DotaPracticeLobbyObjectOptions lobby_object_options{};
    lobby_object_options.extra_startup_account_id = extra_startup_account_id;
    lobby_object_options.steam_id = steam_id;
    lobby_object_options.game_mode = game_mode;
    lobby_object_options.is_custom_game = has_custom_game;
    lobby_object_options.player_name = player_name;
    lobby_object_options.lobby_members = std::move(parsed_members);
    lobby_object_options.static_lobby_members = std::move(static_lobby_members);

    gbe::gc_message::DotaLobbyObject2004Options &object_2004_options = lobby_object_options.object_2004_options;
    object_2004_options.steam_id = steam_id;
    object_2004_options.lobby_id = lobby_id;
    object_2004_options.lobby_state = lobby_state;
    object_2004_options.lobby_game_state = lobby_game_state;
    object_2004_options.server_id = server_id;
    object_2004_options.match_id = match_id;
    object_2004_options.game_start_time = game_start_time;
    if (lobby_state == 3u && lobby_game_state == 6u && game_start_time != 0u) {
        const uint32 now = static_cast<uint32>(std::time(nullptr));
        object_2004_options.elapsed_game_time = now > game_start_time ? (now - game_start_time) : 0u;
    }
    object_2004_options.connect = normalized_connect;
    object_2004_options.room_name = room_name;
    object_2004_options.game_mode = game_mode;
    object_2004_options.server_region = server_region;
    object_2004_options.lan = lan;
    object_2004_options.lan_host_ping_location = lan_host_ping_location;
    object_2004_options.allow_cheats = allow_cheats;
    object_2004_options.fill_with_bots = fill_with_bots;
    object_2004_options.allow_spectating = allow_spectating;
    object_2004_options.visibility = visibility;
    object_2004_options.bot_difficulty_radiant = bot_difficulty_radiant;
    object_2004_options.bot_difficulty_dire = bot_difficulty_dire;
    object_2004_options.bot_radiant = bot_radiant;
    object_2004_options.bot_dire = bot_dire;
    object_2004_options.has_broadcast_channel = has_broadcast_channel;
    object_2004_options.broadcast_channel_id = broadcast_channel_id;
    object_2004_options.broadcast_country_code = broadcast_country_code;
    object_2004_options.broadcast_description = broadcast_description;
    object_2004_options.broadcast_language_code = broadcast_language_code;
    object_2004_options.pass_key = pass_key;
    object_2004_options.custom_game = parsed_custom_game_ptr;

    gbe::gc_message::build_dota_practice_lobby_objects(lobby_object_options, lobby_objects);
}

static bool GBE_AdaptDotaPracticeLobbyCacheSubscribedPayload(
    uint64 steam_id,
    uint64 lobby_id,
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
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game = nullptr)
{
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
        has_broadcast_channel,
        broadcast_channel_id,
        broadcast_country_code,
        broadcast_description,
        broadcast_language_code,
        pass_key,
        extra_startup_account_id,
        custom_game,
        lobby_objects);

    return gbe::gc_message::build_dota_practice_lobby_cache_subscribed_payload_from_objects(lobby_id, lobby_objects.object_2004, lobby_objects.object_2015, lobby_objects.object_2014, lobby_objects.object_2016, message);
}

static bool GBE_AdaptDotaPracticeLobbyDetailsUpdatePurePayload(
    uint64 steam_id,
    uint64 lobby_id,
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
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    bool include_server_lobby_placeholder,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game = nullptr)
{
    if (steam_id == 0 || lobby_id == 0)
        return false;

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
        has_broadcast_channel,
        broadcast_channel_id,
        broadcast_country_code,
        broadcast_description,
        broadcast_language_code,
        pass_key,
        extra_startup_account_id,
        custom_game,
        lobby_objects);

    return gbe::gc_message::build_dota_practice_lobby_details_update_payload_from_objects(lobby_id, lobby_objects.object_2014, lobby_objects.object_2015, lobby_objects.object_2004, lobby_objects.object_2016, include_server_lobby_placeholder, message);
}

static bool GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedFromWrappedTemplate(
    const char *wrapped_template_hex,
    const char *template_note,
    bool require_lobby_identifiers,
    bool rewrite_runtime_fields,
    bool force_lobby_owner_soid,
    bool rewrite_2015,
    uint32 account_id,
    uint64 steam_id,
    uint64 lobby_id,
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
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game = nullptr)
{
    std::string wrapped_message;
    if (!wrapped_template_hex || !gbe::proto_wire::decode_hex_string(wrapped_template_hex, wrapped_message)) {
        GBE_GC_DebugLog("GC_DOTA_SYNC", "launch cache template decode failed note=%s has_template=%u", template_note ? template_note : "unknown", wrapped_template_hex ? 1u : 0u);
        return false;
    }

    if (!GBE_PatchDotaTemplateIdentifiers(
            wrapped_message,
            account_id,
            steam_id,
            true,
            true,
            GBE_kDotaPracticeLobbyLaunch,
            GBE_kDotaCacheSubscribed,
            0,
            template_note ? template_note : "practice lobby launch cache template")) {
        GBE_GC_DebugLog("GC_DOTA_SYNC", "launch cache template identifier patch failed note=%s", template_note ? template_note : "unknown");
        return false;
    }

    if (!GBE_ExtractWrappedClientFromGCPayload(wrapped_message, GBE_kDotaCacheSubscribed, message)) {
        GBE_GC_DebugLog("GC_DOTA_SYNC", "launch cache template inner extraction failed note=%s wrapped_size=%zu", template_note ? template_note : "unknown", wrapped_message.size());
        return false;
    }

    if (require_lobby_identifiers) {
        if (!GBE_PatchDotaLobbyTemplateIdentifiers(message, account_id, steam_id, lobby_id)) {
            GBE_GC_DebugLog("GC_DOTA_SYNC", "launch cache required lobby identifier patch failed note=%s lobby_id=%llu", template_note ? template_note : "unknown", static_cast<unsigned long long>(lobby_id));
            return false;
        }
    } else {
        if (!GBE_PatchDotaLobbyTemplateIdentifiersIfPresent(message, steam_id, lobby_id)) {
            GBE_GC_DebugLog("GC_DOTA_SYNC", "launch cache optional lobby identifier patch failed note=%s lobby_id=%llu", template_note ? template_note : "unknown", static_cast<unsigned long long>(lobby_id));
            return false;
        }
    }

    if (!rewrite_runtime_fields) {
        if (!force_lobby_owner_soid)
            return true;

        return GBE_ForceDotaLobbyCacheOwnerSOID(message, lobby_id);
    }

    if (!GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
        message,
        account_id,
        steam_id,
        lobby_id,
        rewrite_runtime_fields,
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
        std::vector<GBE_DotaLobbyMemberState>(),
        rewrite_2015,
        extra_startup_account_id,
        pass_key,
        custom_game)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "launch cache runtime state patch failed note=%s lobby_id=%llu state=%u game_state=%u server_id=%llu match_id=%llu body_size=%zu",
            template_note ? template_note : "unknown",
            static_cast<unsigned long long>(lobby_id),
            lobby_state,
            lobby_game_state,
            static_cast<unsigned long long>(server_id),
            static_cast<unsigned long long>(match_id),
            message.size());
        return false;
    }

    if (!GBE_ForceDotaLobbyCacheOwnerSOID(message, lobby_id)) {
        GBE_GC_DebugLog("GC_DOTA_SYNC", "launch cache owner soid patch failed note=%s lobby_id=%llu body_size=%zu", template_note ? template_note : "unknown", static_cast<unsigned long long>(lobby_id), message.size());
        return false;
    }

    return true;
}

bool GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedTemplate(
    uint32 account_id,
    uint64 steam_id,
    uint64 lobby_id,
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
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game)
{
    (void)extra_startup_account_id;

    return GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedFromWrappedTemplate(
        GBE_kDotaPracticeLobbyLaunchCacheSubscribedOfficialHex,
        "practice lobby launch official cache template",
        false,
        true,
        true,
        false,
        account_id,
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
        pass_key,
        extra_startup_account_id,
        message,
        custom_game);
}

bool GBE_AdaptDotaPracticeLobbyDetailsUpdatePayload(
    uint64 steam_id,
    uint32 account_id,
    uint64 lobby_id,
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
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game)
{
    (void)has_broadcast_channel;
    (void)broadcast_channel_id;
    (void)broadcast_country_code;
    (void)broadcast_description;
    (void)broadcast_language_code;

    const uint32 startup_account_id = gbe::dota_gc_wire::get_dota_practice_lobby_startup_account_id_for_state(account_id, lobby_state, lobby_game_state);

    if (gbe::proto_wire::is_dota_practice_lobby_prelaunch_state(server_id, match_id, game_start_time, connect)) {
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
            has_broadcast_channel,
            broadcast_channel_id,
            broadcast_country_code,
            broadcast_description,
            broadcast_language_code,
            pass_key,
            startup_account_id,
            custom_game,
            lobby_objects);

        return gbe::gc_message::build_dota_practice_lobby_prelaunch_details_update_payload_from_objects(lobby_id, lobby_objects.object_2014, lobby_objects.object_2016, lobby_objects.object_2015, lobby_objects.object_2004, message)
            && GBE_ForceDotaLobbyUpdateOwnerSOID(message, lobby_id);
    }

    if (GBE_AdaptDotaPracticeLobbyDetailsUpdatePurePayload(
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
            has_broadcast_channel,
            broadcast_channel_id,
            broadcast_country_code,
            broadcast_description,
            broadcast_language_code,
            pass_key,
            startup_account_id,
            true,
            message,
            custom_game)) {
        return true;
    }

    return GBE_ReplayDotaPracticeLobbyOfficial26Payload(
        GBE_kDotaOfficial032PracticeLobby26Hex,
        "current direct 26 details update",
        account_id,
        steam_id,
        lobby_id,
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
        pass_key,
        lobby_state,
        lobby_game_state,
        startup_account_id != 0u,
        startup_account_id,
        message,
        custom_game);
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

static bool GBE_ExtractDotaHelloContext(const void *pubData, uint32 cubData, GBE_DotaHelloContext &context)
{
    context = {};

    if (!pubData || cubData < 8) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "invalid input pubData=%p cubData=%u", pubData, cubData);
        return false;
    }

    const uint8 *bytes = reinterpret_cast<const uint8 *>(pubData);
    uint32 outer_raw_emsg = 0;
    uint32 outer_header_length = 0;
    std::memcpy(&outer_raw_emsg, bytes, sizeof(outer_raw_emsg));
    std::memcpy(&outer_header_length, bytes + sizeof(outer_raw_emsg), sizeof(outer_header_length));

    if (GBE_GC_MaskedEMsg(outer_raw_emsg) != GBE_kEMsgClientToGC) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "unexpected outer emsg=%u", GBE_GC_MaskedEMsg(outer_raw_emsg));
        return false;
    }

    const size_t outer_header_offset = 8;
    const size_t outer_body_offset = outer_header_offset + outer_header_length;
    if (outer_body_offset > cubData) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "outer body offset overflow header_len=%u cubData=%u", outer_header_length, cubData);
        return false;
    }

    const uint8 *outer_header = bytes + outer_header_offset;
    const uint8 *outer_body = bytes + outer_body_offset;
    const size_t outer_body_size = cubData - outer_body_offset;

    if (!gbe::proto_wire::read_bytes_field(outer_header, outer_header_length, 2u, context.outer_session_field_raw)) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "missing outer session field");
        return false;
    }

    std::string payload_raw;
    if (!gbe::proto_wire::read_bytes_field(outer_body, outer_body_size, 3u, payload_raw) || payload_raw.size() < 8u) {
        GBE_GC_DebugLog(
            "GC_DOTA_HELLO",
            "invalid payload field size=%zu",
            payload_raw.size()
        );
        return false;
    }

    const uint8 *payload = reinterpret_cast<const uint8 *>(payload_raw.data());
    uint32 inner_raw_emsg = 0;
    uint32 inner_header_length = 0;
    std::memcpy(&inner_raw_emsg, payload, sizeof(inner_raw_emsg));
    std::memcpy(&inner_header_length, payload + sizeof(inner_raw_emsg), sizeof(inner_header_length));

    if (GBE_GC_MaskedEMsg(inner_raw_emsg) != GBE_kEMsgGCClientHello) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "unexpected inner emsg=%u", GBE_GC_MaskedEMsg(inner_raw_emsg));
        return false;
    }

    const size_t inner_header_offset = 8;
    const size_t inner_body_offset = inner_header_offset + inner_header_length;
    if (inner_body_offset > payload_raw.size()) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "inner body offset overflow header_len=%u payload_size=%zu", inner_header_length, payload_raw.size());
        return false;
    }

    const uint8 *inner_header = payload + inner_header_offset;
    const uint8 *inner_body = payload + inner_body_offset;
    const size_t inner_body_size = payload_raw.size() - inner_body_offset;

    uint64 parsed_version = 0;
    if (!gbe::proto_wire::read_uint64_field(inner_body, inner_body_size, 1u, parsed_version)) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "failed to extract version field");
        return false;
    }

    context.version = static_cast<uint32>(parsed_version);

    if (inner_header_length > 0) {
        uint64 source_job = 0;
        if (gbe::proto_wire::read_uint64_field(inner_header, inner_header_length, 11u, source_job)) {
            context.source_job_id = source_job;
            context.has_source_job = true;
        }
    }

    context.valid = true;
    GBE_GC_DebugLog(
        "GC_DOTA_HELLO",
        "parsed version=%u source_job=%llu has_source_job=%d session_raw_size=%zu",
        context.version,
        static_cast<unsigned long long>(context.source_job_id),
        context.has_source_job ? 1 : 0,
        context.outer_session_field_raw.size()
    );
    return true;
}

static bool GBE_ExtractDirectDotaHelloContext(uint32 unMsgType, const void *pubData, uint32 cubData, GBE_DotaHelloContext &context)
{
    context = {};

    if (GBE_GC_MaskedEMsg(unMsgType) != GBE_kEMsgGCClientHello) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "direct path rejected msg=%u", GBE_GC_MaskedEMsg(unMsgType));
        return false;
    }

    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(pubData, cubData, proto_context)) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "direct path invalid input pubData=%p cubData=%u", pubData, cubData);
        return false;
    }

    uint64 parsed_version = 0;
    if (!gbe::proto_wire::read_uint64_field(proto_context.body, proto_context.body_size, 1u, parsed_version)) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "direct path failed to extract version field");
        return false;
    }

    context.valid = true;
    context.version = static_cast<uint32>(parsed_version);
    if (proto_context.protohdr.has_job_id_source()) {
        context.source_job_id = proto_context.protohdr.job_id_source();
        context.has_source_job = true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_HELLO",
        "direct path parsed version=%u source_job=%llu has_source_job=%d body_size=%zu",
        context.version,
        static_cast<unsigned long long>(context.source_job_id),
        context.has_source_job ? 1 : 0,
        proto_context.body_size
    );
    return true;
}

static bool GBE_ExtractDirectDotaServerHelloContext(uint32 unMsgType, const void *pubData, uint32 cubData, GBE_DotaServerHelloContext &context)
{
    context = {};

    if (GBE_GC_MaskedEMsg(unMsgType) != GBE_kEMsgGCServerHello) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "direct path rejected msg=%u", GBE_GC_MaskedEMsg(unMsgType));
        return false;
    }

    GBE_DirectProtoContext proto_context{};
    if (!GBE_ParseDirectProtoContext(pubData, cubData, proto_context)) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "direct path invalid input pubData=%p cubData=%u", pubData, cubData);
        return false;
    }

    CMsgServerHello protomsg;
    if (!protomsg.ParseFromArray(proto_context.body, static_cast<int>(proto_context.body_size)) || !protomsg.has_version()) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "direct path failed parsing CMsgServerHello body_size=%zu", proto_context.body_size);
        return false;
    }

    const uint32 version = protomsg.version();

    context.valid = true;
    context.active_version = version;
    context.min_allowed_version = version;
    context.compatibility_value = 0;
    context.universe = 0;
    if (proto_context.protohdr.has_client_steam_id()) {
        context.client_steam_id = proto_context.protohdr.client_steam_id();
        context.has_client_steam_id = true;
    }
    if (proto_context.protohdr.has_client_session_id()) {
        context.client_session_id = proto_context.protohdr.client_session_id();
        context.has_client_session_id = true;
    }
    if (proto_context.protohdr.has_source_app_id()) {
        context.source_app_id = proto_context.protohdr.source_app_id();
        context.has_source_app_id = true;
    }
    if (proto_context.protohdr.has_job_id_source()) {
        context.source_job_id = proto_context.protohdr.job_id_source();
        context.has_source_job = true;
    }
    if (proto_context.protohdr.has_gc_msg_src()) {
        context.gc_msg_src = static_cast<uint32>(proto_context.protohdr.gc_msg_src());
        context.has_gc_msg_src = true;
    }
    if (proto_context.protohdr.has_gc_dir_index_source()) {
        context.gc_dir_index_source = proto_context.protohdr.gc_dir_index_source();
        context.has_gc_dir_index_source = true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_SERVER_HELLO",
        "direct path parsed active_version=%u min_allowed=%u compat=%llu universe=%u source_job=%llu has_source_job=%d client_steam_id=%llu has_client_steam_id=%d client_session_id=%d has_client_session_id=%d source_app_id=%u has_source_app_id=%d gc_msg_src=%u has_gc_msg_src=%d gc_dir_index_source=%u has_gc_dir_index_source=%d body_size=%zu",
        context.active_version,
        context.min_allowed_version,
        static_cast<unsigned long long>(context.compatibility_value),
        context.universe,
        static_cast<unsigned long long>(context.source_job_id),
        context.has_source_job ? 1 : 0,
        static_cast<unsigned long long>(context.client_steam_id),
        context.has_client_steam_id ? 1 : 0,
        context.client_session_id,
        context.has_client_session_id ? 1 : 0,
        context.source_app_id,
        context.has_source_app_id ? 1 : 0,
        context.gc_msg_src,
        context.has_gc_msg_src ? 1 : 0,
        context.gc_dir_index_source,
        context.has_gc_dir_index_source ? 1 : 0,
        proto_context.body_size
    );
    return true;
}

static bool GBE_PrepareDotaWelcomeBody(uint64 steam_id, uint32 account_id, const GBE_DotaHelloContext &context, std::string &inner_body)
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

static bool GBE_BuildDirectDotaClientWelcome(uint64 steam_id, uint32 app_id, uint32 account_id, const GBE_DotaHelloContext &context, std::string &message)
{
    std::string inner_body;
    if (!GBE_PrepareDotaWelcomeBody(steam_id, account_id, context, inner_body))
        return false;

    std::string proto_header;
    ProtoBufMsgHeader_t hdr{};
    hdr.m_EMsgFlagged = GBE_kEMsgGCClientWelcome | GBE_kProtoMask;

    CMsgProtoBufHeader protohdr;
    protohdr.set_client_steam_id(steam_id);
    protohdr.set_client_session_id(1);
    protohdr.set_source_app_id(app_id);
    if (context.has_source_job) {
        protohdr.set_job_id_target(context.source_job_id);
        GBE_GC_DebugLog("GC_DOTA_WELCOME", "direct path mirroring source_job=%llu into target_job", static_cast<unsigned long long>(context.source_job_id));
    }

    hdr.m_cubProtoBufExtHdr = static_cast<uint32>(protohdr.ByteSizeLong());
    message.clear();
    ser_var<ProtoBufMsgHeader_t>(message, hdr);
    protohdr.AppendToString(&message);
    message.append(inner_body);

    GBE_GC_DebugLog("GC_DOTA_WELCOME", "built direct welcome header=%u body=%zu total=%zu", hdr.m_cubProtoBufExtHdr, inner_body.size(), message.size());
    return true;
}

bool GBE_BuildDirectDotaServerWelcome(uint64 steam_id, uint32 app_id, const GBE_DotaServerHelloContext &context, std::string &message)
{
    if (!context.valid) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "invalid server hello context");
        return false;
    }

    ProtoBufMsgHeader_t hdr{};
    hdr.m_EMsgFlagged = EGCBaseClientMsg::k_EMsgGCServerWelcome | GBE_kProtoMask;

    CMsgProtoBufHeader protohdr;
    protohdr.set_client_steam_id(context.has_client_steam_id ? context.client_steam_id : steam_id);
    if (context.has_client_session_id) {
        protohdr.set_client_session_id(context.client_session_id);
    } else {
        protohdr.set_client_session_id(1);
    }
    protohdr.set_source_app_id(context.has_source_app_id ? context.source_app_id : app_id);
    if (context.has_source_job)
        protohdr.set_job_id_target(context.source_job_id);
    if (context.has_gc_msg_src)
        protohdr.set_gc_msg_src(static_cast<GCProtoBufMsgSrc>(context.gc_msg_src));
    if (context.has_gc_dir_index_source)
        protohdr.set_gc_dir_index_source(context.gc_dir_index_source);

    hdr.m_cubProtoBufExtHdr = static_cast<uint32>(protohdr.ByteSizeLong());

    message.clear();
    ser_var<ProtoBufMsgHeader_t>(message, hdr);
    protohdr.AppendToString(&message);

    CMsgServerWelcome protomsg;
    protomsg.set_min_allowed_version(context.min_allowed_version);
    protomsg.set_active_version(context.active_version);
    protomsg.AppendToString(&message);

    GBE_GC_DebugLog(
        "GC_DOTA_SERVER_HELLO",
        "built direct ServerWelcome active_version=%u min_allowed=%u target_job=%llu client_steam_id=%llu client_session_id=%d source_app_id=%u gc_msg_src=%u gc_dir_index_source=%u total=%zu",
        context.active_version,
        context.min_allowed_version,
        static_cast<unsigned long long>(context.has_source_job ? context.source_job_id : 0ull),
        static_cast<unsigned long long>(context.has_client_steam_id ? context.client_steam_id : steam_id),
        context.has_client_session_id ? context.client_session_id : 1,
        context.has_source_app_id ? context.source_app_id : app_id,
        context.has_gc_msg_src ? context.gc_msg_src : 0u,
        context.has_gc_dir_index_source ? context.gc_dir_index_source : 0u,
        message.size()
    );
    return true;
}

static bool GBE_ComposeDotaClientWelcome(uint64 steam_id, uint32 app_id, uint32 account_id, const GBE_DotaHelloContext &context, std::string &message)
{
    std::string inner_body;
    if (!GBE_PrepareDotaWelcomeBody(steam_id, account_id, context, inner_body))
        return false;

    std::string inner_header;
    if (context.has_source_job) {
        gbe::proto_wire::append_varint_field(inner_header, 10, context.source_job_id);
        GBE_GC_DebugLog("GC_DOTA_WELCOME", "mirroring source_job=%llu into inner target_job", static_cast<unsigned long long>(context.source_job_id));
    }

    std::string inner_payload;
    gbe::proto_wire::append_little_endian32(inner_payload, GBE_kEMsgGCClientWelcome | GBE_kProtoMask);
    gbe::proto_wire::append_little_endian32(inner_payload, static_cast<uint32>(inner_header.size()));
    inner_payload.append(inner_header);
    inner_payload.append(inner_body);

    std::string outer_body;
    gbe::proto_wire::append_varint_field(outer_body, 1, app_id);
    gbe::proto_wire::append_varint_field(outer_body, 2, GBE_kEMsgGCClientWelcome | GBE_kProtoMask);
    gbe::proto_wire::append_bytes_field(outer_body, 3, inner_payload);

    std::string outer_header;
    // The outer fixed64 steamid lives outside the inner template, so we rebuild that header directly.
    gbe::proto_wire::append_fixed64_field(outer_header, 1, steam_id);
    gbe::proto_wire::append_varuint(outer_header, (static_cast<uint64>(2) << 3) | 0u);
    outer_header.append(context.outer_session_field_raw);

    message.clear();
    gbe::proto_wire::append_little_endian32(message, GBE_kEMsgClientFromGC | GBE_kProtoMask);
    gbe::proto_wire::append_little_endian32(message, static_cast<uint32>(outer_header.size()));
    message.append(outer_header);
    message.append(outer_body);
    GBE_GC_DebugLog(
        "GC_DOTA_WELCOME",
        "built welcome outer_size=%zu inner_header=%zu inner_body=%zu total=%zu",
        outer_header.size(),
        inner_header.size(),
        inner_body.size(),
        message.size()
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_DispatchDotaPostLoginRequest(const gbe::dota_gc_router::DotaGcRequestContext &context)
{
    if (!context.valid)
        return false;

    const std::string *outer_session_field_raw = gbe::dota_gc_router::outer_session_field_or_null(context);
    const char *path = context.wrapped ? "wrapped" : "direct";
    auto log_lobby_request = [path, &context]() {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received %s %u has_job=%u request_job=%llu session_raw_size=%zu body_size=%zu body_prefix=%s",
            path,
            context.inner_emsg,
            context.has_request_job ? 1u : 0u,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            context.body.size(),
            gbe::proto_wire::format_hex_prefix(reinterpret_cast<const std::uint8_t *>(context.body.data()), context.body.size(), 48).c_str()
        );
    };

    switch (context.inner_emsg) {
        case GBE_kDotaJoinChatChannel:
            log_lobby_request();
            return GBE_HandleDotaJoinChatChannelRequest(context.body, context.wrapped, outer_session_field_raw);
        case GBE_kDotaPracticeLobbyCreate:
            log_lobby_request();
            return GBE_HandleDotaPracticeLobbyCreateRequest(context.body, context.request_job_id, context.has_request_job, context.wrapped, outer_session_field_raw);
        case GBE_kDotaLobbyList:
            log_lobby_request();
            return GBE_HandleDotaLobbyListRequest(context.has_request_job, context.request_job_id, context.wrapped, outer_session_field_raw);
        case GBE_kDotaCustomLobbyListRequest:
            log_lobby_request();
            return GBE_HandleDotaCustomLobbyListRequest(context.body, context.wrapped, outer_session_field_raw);
        case GBE_kDotaFriendPracticeLobbyListRequest:
            log_lobby_request();
            return GBE_HandleDotaFriendPracticeLobbyListRequest(context.wrapped, outer_session_field_raw);
        case GBE_kGCInviteToLobby:
            log_lobby_request();
            return GBE_HandleDotaInviteToLobbyRequest(context.body, context.wrapped, outer_session_field_raw);
        case GBE_kGCLobbyInviteResponse:
            log_lobby_request();
            return GBE_HandleDotaLobbyInviteResponseRequest(context.body, context.wrapped, outer_session_field_raw);
        case GBE_kDotaPracticeLobbyJoin:
            log_lobby_request();
            return GBE_HandleDotaPracticeLobbyJoinRequest(context.body, context.request_job_id, context.has_request_job, context.wrapped, outer_session_field_raw);
        case GBE_kDotaPracticeLobbyLeave:
            log_lobby_request();
            return GBE_HandleDotaPracticeLobbyLeaveRequest(context.wrapped, outer_session_field_raw);
        case GBE_kDotaPracticeLobbyLaunch:
            log_lobby_request();
            return GBE_HandleDotaPracticeLobbyLaunchRequest(context.body, context.wrapped, outer_session_field_raw, context.has_request_job, context.request_job_id);
        case GBE_kDotaPracticeLobbySetDetails:
            log_lobby_request();
            return GBE_HandleDotaPracticeLobbySetDetailsRequest(context.body, context.wrapped, outer_session_field_raw);
        case GBE_kDotaPracticeLobbySetTeamSlot:
            log_lobby_request();
            return GBE_HandleDotaPracticeLobbySetTeamSlotRequest(context.body, context.request_job_id, context.has_request_job, context.wrapped, outer_session_field_raw);
        case GBE_kDotaPracticeLobbyKick:
            log_lobby_request();
            return GBE_HandleDotaPracticeLobbyKickRequest(context.body, context.wrapped, outer_session_field_raw);
        case GBE_kDotaPracticeLobbyJoinBroadcastChannel:
            log_lobby_request();
            return GBE_HandleDotaPracticeLobbyJoinBroadcastChannelRequest(context.body, context.request_job_id, context.has_request_job, context.wrapped, outer_session_field_raw);
        case GBE_kDotaLobbyUpdateBroadcastChannelInfo:
            log_lobby_request();
            return GBE_HandleDotaLobbyUpdateBroadcastChannelInfoRequest(context.body, context.wrapped, outer_session_field_raw);
        case GBE_kDotaPracticeLobbyCloseBroadcastChannel:
            log_lobby_request();
            return GBE_HandleDotaPracticeLobbyCloseBroadcastChannelRequest(context.body, context.wrapped, outer_session_field_raw);
        default:
            return false;
    }
}

bool Steam_Game_Coordinator::gc_enabled()
{
    if (gc_profile == GC_PROFILE_DOTA2)
        return settings && settings->get_local_game_id().AppID() == GBE_kDotaAppId;

    return (gc_version >= GC_MIN_VERSION && gc_profile != GC_PROFILE_INVALID);
}

Steam_User_Items *Steam_Game_Coordinator::client_items()
{
    return get_steam_client()->steam_user_items;
}

Steam_GameServer_Items *Steam_Game_Coordinator::server_items()
{
    return get_steam_client()->steam_gameserver_items;
}

void Steam_Game_Coordinator::parse_gc_config()
{
    gc_profile = GC_PROFILE_INVALID;
    gc_version = 0;
    is_portal2 = false;

    std::string file_path = Local_Storage::get_game_settings_path() + gc_config_file;
    nlohmann::json gc_json;
    const uint32 app_id = settings ? settings->get_local_game_id().AppID() : 0u;
    const bool loaded = local_storage && local_storage->load_json(file_path, gc_json);
    const gbe::gc_config::ParseResult result = gbe::gc_config::parse_gc_config(loaded, gc_json, app_id, GBE_kDotaAppId);

    gc_version = result.version;
    is_portal2 = result.is_portal2;
    switch (result.profile) {
        case gbe::gc_config::Profile::Tf2:
            gc_profile = GC_PROFILE_TF2;
            break;
        case gbe::gc_config::Profile::Dota2:
            gc_profile = GC_PROFILE_DOTA2;
            break;
        default:
            gc_profile = GC_PROFILE_INVALID;
            break;
    }

    if (!result.error_message.empty())
        PRINT_DEBUG("error parsing GC config: %s", result.error_message.c_str());

    if (result.dota_fallback_reason == gbe::gc_config::DotaFallbackReason::MissingConfig) {
        GBE_GC_DebugLog("GC_CONFIG", "auto-enabled Dota2 GC profile for app %u", app_id);
    } else if (result.dota_fallback_reason == gbe::gc_config::DotaFallbackReason::InvalidProfile) {
        GBE_GC_DebugLog("GC_CONFIG", "fell back to Dota2 GC profile for app %u", app_id);
    }
}

bool Steam_Game_Coordinator::is_welcome_message(const GC_Message &message)
{
    uint32 msg_type = message.msg_type & (~protobuf_mask);
    return (msg_type == 4004 ||
        msg_type == 4005);
}

void Steam_Game_Coordinator::GBE_ApplyQueuedLobbyState(const GC_Message &message)
{
    if (!message.apply_lobby_state)
        return;

    if (gc_profile == GC_PROFILE_DOTA2 && (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0)) {
        if (GBE_shared_dota_lobby_state.valid && GBE_shared_dota_lobby_state.active && GBE_shared_dota_lobby_state.lobby_id != 0) {
            GBE_RestoreSharedDotaLobbyState("queued_state_preapply");
        } else {
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "queued lobby state has no full local/shared lobby context this=%p shared_valid=%u shared_active=%u shared_lobby_id=%llu msg=%u state=%u game_state=%u",
                static_cast<void *>(this),
                GBE_shared_dota_lobby_state.valid ? 1u : 0u,
                GBE_shared_dota_lobby_state.active ? 1u : 0u,
                static_cast<unsigned long long>(GBE_shared_dota_lobby_state.lobby_id),
                GBE_GC_MaskedEMsg(message.msg_type),
                message.lobby_state,
                message.lobby_game_state
            );
        }
    }

    const uint32 previous_game_state = GBE_local_lobby.game_state;
    const gbe::dota_lobby_state::QueuedLobbyStateApplyPlan apply_plan = gbe::dota_lobby_state::compose_queued_lobby_state_apply_plan(
        GBE_local_lobby,
        message.lobby_state,
        message.lobby_game_state,
        gc_profile == GC_PROFILE_DOTA2,
        GBE_kDotaLaunchPhaseSetupSynced,
        GBE_kDotaLaunchPhaseRunQueued);
    if (apply_plan.preserved_game_state) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "preserving monotonic launch game_state on queued apply msg=%u state=%u previous_game_state=%u queued_game_state=%u lobby_id=%llu",
            GBE_GC_MaskedEMsg(message.msg_type),
            message.lobby_state,
            previous_game_state,
            message.lobby_game_state,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id)
        );
    }

    GBE_local_lobby.state = apply_plan.state;
    GBE_local_lobby.game_state = apply_plan.game_state;
    GBE_local_lobby.launch_phase = apply_plan.launch_phase;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Applied queued state=%u game_state=%u launch_phase=%s msg=%u active=%u lobby_id=%llu server_id=%llu",
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase),
        GBE_GC_MaskedEMsg(message.msg_type),
        GBE_local_lobby.active ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id)
    );

    GBE_ReapplyDotaPracticeLobbyLaunchRichPresence("queued_state");

    if (gc_profile == GC_PROFILE_DOTA2 &&
        is_server &&
        GBE_local_lobby.state == 2u &&
        GBE_local_lobby.game_state >= 1u) {
        GBE_PushDotaLaunchStateToClientPeer("queued_state");
    }

    GBE_PublishSharedDotaLobbyState("queued_state");
}

void Steam_Game_Coordinator::push_incoming(uint32 msg_type, const std::string &message, double delay, bool apply_lobby_state, uint32 lobby_state, uint32 lobby_game_state)
{
    PRINT_DEBUG("%u %.2f", msg_type, delay);

    GC_Message new_item;
    new_item.msg_type = msg_type;
    new_item.msg_body = message;
    new_item.created = std::chrono::high_resolution_clock::now();
    new_item.post_in = delay;
    new_item.sequence = ++pending_message_sequence;
    new_item.apply_lobby_state = apply_lobby_state;
    new_item.lobby_state = lobby_state;
    new_item.lobby_game_state = lobby_game_state;
    pending_messages.push_back(new_item);

    GBE_GC_DebugLog(
        "GC_CALLBACK",
        "queued delayed msg=%u this=%p delay=%.3f pending_size=%zu incoming_size=%zu apply_state=%u lobby_state=%u game_state=%u",
        GBE_GC_MaskedEMsg(msg_type),
        static_cast<void *>(this),
        delay,
        pending_messages.size(),
        incoming_messages.size(),
        apply_lobby_state ? 1u : 0u,
        lobby_state,
        lobby_game_state
    );
}

bool Steam_Game_Coordinator::GBE_ShouldDiscardQueuedDotaLaunchMessageForAbandon(uint32 masked_emsg) const
{
    switch (masked_emsg) {
        case GBE_kDotaCacheSubscribed:
        case GBE_kDotaPracticeLobbyDetailsUpdate:
        case 7034u:
        case 5501u:
        case 5575u:
        case 779u:
        case 766u:
            return true;
        default:
            return false;
    }
}

void Steam_Game_Coordinator::GBE_DiscardQueuedDotaLaunchMessagesForAbandon(const char *reason)
{
    auto discard_for_instance = [reason](Steam_Game_Coordinator *coordinator) {
        if (!coordinator)
            return;

        size_t removed_pending = 0;
        for (auto it = coordinator->pending_messages.begin(); it != coordinator->pending_messages.end();) {
            if (coordinator->GBE_ShouldDiscardQueuedDotaLaunchMessageForAbandon(GBE_GC_MaskedEMsg(it->msg_type))) {
                it = coordinator->pending_messages.erase(it);
                ++removed_pending;
            } else {
                ++it;
            }
        }

        size_t removed_incoming = 0;
        std::queue<GC_Message> filtered_incoming;
        while (!coordinator->incoming_messages.empty()) {
            GC_Message queued = coordinator->incoming_messages.front();
            coordinator->incoming_messages.pop();
            if (coordinator->GBE_ShouldDiscardQueuedDotaLaunchMessageForAbandon(GBE_GC_MaskedEMsg(queued.msg_type))) {
                ++removed_incoming;
                continue;
            }
            filtered_incoming.push(std::move(queued));
        }
        coordinator->incoming_messages.swap(filtered_incoming);

        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Discarded queued launch messages for abandon reason=%s this=%p is_server=%u removed_pending=%zu removed_incoming=%zu remaining_pending=%zu remaining_incoming=%zu",
            reason ? reason : "unknown",
            static_cast<void *>(coordinator),
            coordinator->is_server ? 1u : 0u,
            removed_pending,
            removed_incoming,
            coordinator->pending_messages.size(),
            coordinator->incoming_messages.size()
        );
    };

    discard_for_instance(this);

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client)
        return;

    Steam_Game_Coordinator *peer = is_server ? steam_client->steam_game_coordinator : steam_client->steam_gameserver_game_coordinator;
    if (peer && peer != this)
        discard_for_instance(peer);
}

bool Steam_Game_Coordinator::GBE_ShouldSuppressDotaAbandonedLobby(uint64 lobby_id) const
{
    return gc_profile == GC_PROFILE_DOTA2 && lobby_id != 0 && GBE_suppressed_dota_abandon_lobby_id == lobby_id;
}

void Steam_Game_Coordinator::GBE_MarkDotaAbandonedLobbySuppressed(uint64 lobby_id, const char *reason)
{
    if (gc_profile != GC_PROFILE_DOTA2 || lobby_id == 0)
        return;

    auto mark_for_instance = [lobby_id, reason](Steam_Game_Coordinator *coordinator) {
        if (!coordinator || coordinator->gc_profile != GC_PROFILE_DOTA2)
            return;

        coordinator->GBE_suppressed_dota_abandon_lobby_id = lobby_id;
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Suppressing stale abandoned lobby updates reason=%s this=%p is_server=%u lobby_id=%llu",
            reason ? reason : "unknown",
            static_cast<void *>(coordinator),
            coordinator->is_server ? 1u : 0u,
            static_cast<unsigned long long>(lobby_id)
        );
    };

    mark_for_instance(this);

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client)
        return;

    Steam_Game_Coordinator *peer = is_server ? steam_client->steam_game_coordinator : steam_client->steam_gameserver_game_coordinator;
    if (peer && peer != this)
        mark_for_instance(peer);
}

void Steam_Game_Coordinator::GBE_ClearDotaAbandonedLobbySuppression(uint64 lobby_id, const char *reason)
{
    if (gc_profile != GC_PROFILE_DOTA2 || lobby_id == 0 || GBE_suppressed_dota_abandon_lobby_id != lobby_id)
        return;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Clearing abandoned lobby suppression reason=%s this=%p is_server=%u lobby_id=%llu",
        reason ? reason : "unknown",
        static_cast<void *>(this),
        is_server ? 1u : 0u,
        static_cast<unsigned long long>(lobby_id)
    );
    GBE_suppressed_dota_abandon_lobby_id = 0;
}

void Steam_Game_Coordinator::push_incoming_now(uint32 msg_type, const std::string &message, bool apply_lobby_state, uint32 lobby_state, uint32 lobby_game_state)
{
    GC_Message new_item;
    new_item.msg_type = msg_type;
    new_item.msg_body = message;
    new_item.created = std::chrono::high_resolution_clock::now();
    new_item.post_in = 0.0;
    new_item.sequence = ++pending_message_sequence;
    new_item.apply_lobby_state = apply_lobby_state;
    new_item.lobby_state = lobby_state;
    new_item.lobby_game_state = lobby_game_state;
    GBE_ApplyQueuedLobbyState(new_item);
    incoming_messages.push(new_item);

    GCMessageAvailable_t data{};
    data.m_nMessageSize = static_cast<uint32>(message.size());
    callbacks->addCBResult(data.k_iCallback, &data, sizeof(data), 0.0);

    GBE_GC_DebugLog(
        "GC_CALLBACK",
        "queued msg=%u this=%p size=%u queue_size=%zu pending_size=%zu apply_state=%u lobby_state=%u game_state=%u and posted GCMessageAvailable_t",
        GBE_GC_MaskedEMsg(msg_type),
        static_cast<void *>(this),
        static_cast<uint32>(message.size()),
        incoming_messages.size(),
        pending_messages.size(),
        apply_lobby_state ? 1u : 0u,
        lobby_state,
        lobby_game_state
    );
}

std::string Steam_Game_Coordinator::build_msg_header(JobID_t target_job, JobID_t source_job)
{
    std::string message;
    GCMsgHdrEx_t hdr{};
    hdr.m_nHdrVersion = 1;
    hdr.m_JobIDTarget = target_job;
    hdr.m_JobIDSource = source_job;
    ser_var<GCMsgHdrEx_t>(message, hdr);
    return message.substr(sizeof(GCMsgHdr_t));
}

GCMsgHdrEx_t Steam_Game_Coordinator::parse_msg_header(const char *&p)
{
    size_t write_offset = sizeof(GCMsgHdr_t);
    size_t hdr_size = sizeof(GCMsgHdrEx_t) - write_offset;
    GCMsgHdrEx_t hdr{};
    memcpy(reinterpret_cast<char *>(&hdr) + write_offset, p, hdr_size);
    p += hdr_size;
    return hdr;
}

std::string Steam_Game_Coordinator::build_protomsg_header(uint32 msg_type, JobID_t target_job, JobID_t source_job)
{
    std::string message;
    ProtoBufMsgHeader_t hdr{};
    hdr.m_EMsgFlagged = msg_type;

    CMsgProtoBufHeader protohdr;
    protohdr.set_client_steam_id(settings->get_local_steam_id().ConvertToUint64());
    protohdr.set_client_session_id(1);
    protohdr.set_source_app_id(settings->get_local_game_id().AppID());
    protohdr.set_job_id_source(source_job);
    protohdr.set_job_id_target(target_job);
    hdr.m_cubProtoBufExtHdr = static_cast<uint32>(protohdr.ByteSizeLong());

    ser_var<ProtoBufMsgHeader_t>(message, hdr);
    protohdr.AppendToString(&message);

    return message;
}

template <class T>
std::tuple<ProtoBufMsgHeader_t, CMsgProtoBufHeader, T, bool> Steam_Game_Coordinator::parse_protomsg(const void *input, uint32 input_size)
{
    T protomsg;

    GBE_DirectProtoContext context{};
    if (!GBE_ParseDirectProtoContext(input, input_size, context))
        return { context.hdr, context.protohdr, protomsg, false };

    if (!protomsg.ParseFromArray(context.body, static_cast<int>(context.body_size)))
        return { context.hdr, context.protohdr, protomsg, false };

    return { context.hdr, context.protohdr, protomsg, true };
}

bool Steam_Game_Coordinator::GBE_PatchDotaLoginCacheSubscribedInventory(std::string &message)
{
    GBE_DirectProtoContext context{};
    if (!GBE_ParseDirectProtoContext(message.data(), static_cast<uint32>(message.size()), context))
        return false;

    const ProtoBufMsgHeader_t hdr = context.hdr;
    const char *proto_header_ptr = message.data() + sizeof(hdr);

    CMsgSOCacheSubscribed protomsg;
    if (!protomsg.ParseFromArray(context.body, static_cast<int>(context.body_size)))
        return false;

    auto *objects = protomsg.mutable_objects();
    const CSteamID steam_id = settings->get_local_steam_id();
    const auto &local_items = load_items_from_file();
    bool patched_items = false;

    for (int i = 0; i < objects->size(); ++i) {
        auto *object = objects->Mutable(i);
        if (object->type_id() != 1u)
            continue;

        object->clear_object_data();
        for (const Econ_Item &item : local_items) {
            object->add_object_data(item_to_gcprotobuf(item, steam_id));
        }

        patched_items = true;
        break;
    }

    if (!patched_items) {
        auto *object = protomsg.add_objects();
        object->set_type_id(1u);
        for (const Econ_Item &item : local_items) {
            object->add_object_data(item_to_gcprotobuf(item, steam_id));
        }
    }

    // Dota 2: inject all cosmetic items from VPK items_game.txt
    // Controlled by GBE_DOTA_UNLOCK_ITEMS env var: "0" to disable, "1" or unset to enable
    // GBE_DOTA_UNLOCK_ITEMS_MAX limits the count (default: no limit)
    if (gc_profile == GC_PROFILE_DOTA2) {
        static bool vpk_items_loaded = false;
        static std::vector<GBE_DotaItemDef> vpk_item_defs;
        static GBE_DotaStyleUnlockInfo vpk_style_unlock{};
        static bool vpk_items_disabled = false;

        if (!vpk_items_loaded) {
            vpk_items_loaded = true;
            const char *env_disable = std::getenv("GBE_DOTA_UNLOCK_ITEMS");
            if (env_disable && std::string(env_disable) == "0") {
                vpk_items_disabled = true;
                GBE_GC_DebugLog("GC_DOTA_ITEMS", "VPK item unlock disabled via GBE_DOTA_UNLOCK_ITEMS=0");
            } else {
                auto vpk_data = GBE_LoadAllDotaItemsFromVpk();
                vpk_item_defs = std::move(vpk_data.item_defs);
                vpk_style_unlock = vpk_data.style_unlock;
                GBE_vpk_loot_data = std::move(vpk_data.loot_data);
                GBE_GC_DebugLog("GC_DOTA_ITEMS", "loaded %zu cosmetic item defs from VPK items_game.txt (style_unlock_attrs=%s loot_lists=%zu treasures=%zu tools=%zu bundles=%zu)",
                    vpk_item_defs.size(), vpk_style_unlock.found ? "found" : "not_found",
                    GBE_vpk_loot_data.loot_lists.size(), GBE_vpk_loot_data.treasure_to_loot_list.size(),
                    GBE_vpk_loot_data.tool_to_loot_list.size(), GBE_vpk_loot_data.bundle_contents.size());

                // Log style diagnostics
                const auto &sd = vpk_data.style_diag;
                GBE_GC_DebugLog("GC_DOTA_STYLES", "multi_style_items=%u locked=%u", sd.multi_style_items, sd.items_with_locked_styles);
                for (const auto &[field_name, count] : sd.style_field_counts) {
                    GBE_GC_DebugLog("GC_DOTA_STYLES", "  field: %s count=%u", field_name.c_str(), count);
                }
                for (const auto &sample : sd.samples) {
                    GBE_GC_DebugLog("GC_DOTA_STYLES", "  sample def=%u styles=%u style1=[%s]",
                        sample.def_index, sample.num_styles, sample.style1_fields.c_str());
                }
            }
        }

        if (!vpk_items_disabled && !vpk_item_defs.empty()) {
            // Max items to inject (default: all, override via GBE_DOTA_UNLOCK_ITEMS_MAX)
            size_t max_items = vpk_item_defs.size();
            const char *env_max = std::getenv("GBE_DOTA_UNLOCK_ITEMS_MAX");
            if (env_max && env_max[0]) {
                try { max_items = std::stoul(env_max); } catch (...) {}
            }

            // Collect existing def_indices from user items to avoid duplicates
            std::unordered_set<uint32_t> existing_defs;
            for (const Econ_Item &item : local_items) {
                existing_defs.insert(item.def);
            }

            // Find the type_id=1 object (or create one)
            CMsgSOCacheSubscribed_SubscribedType *item_object = nullptr;
            for (int i = 0; i < protomsg.mutable_objects()->size(); ++i) {
                if (protomsg.mutable_objects()->Get(i).type_id() == 1u) {
                    item_object = protomsg.mutable_objects()->Mutable(i);
                    break;
                }
            }
            if (!item_object) {
                item_object = protomsg.add_objects();
                item_object->set_type_id(1u);
            }

            // Item ID: use a simple sequential scheme.
            // Start from a base that won't collide with user items.json
            // (which go through item_id_local_to_network and use high bits for account_id).
            // We use IDs in [0x40000001 .. 0x40000001+N] with account_id in high 32 bits.
            const uint32_t account_id = steam_id.GetAccountID();
            uint32_t item_seq = 1; // sequential counter within our block
            size_t injected_count = 0;

            for (const auto &def : vpk_item_defs) {
                if (existing_defs.count(def.def_index)) continue;
                if (injected_count >= max_items) break;

                CSOEconItem proto_item;
                // ID format: high 32 bits = base marker (0x40), low 32 bits = account_id XOR seq
                // This avoids collision with item_id_local_to_network which puts seq in high bits
                uint64_t item_id = (static_cast<uint64_t>(0x40000000u + item_seq) << 32ull) | static_cast<uint64_t>(account_id);
                proto_item.set_id(item_id);
                proto_item.set_account_id(account_id);
                proto_item.set_def_index(def.def_index);
                // Inventory position: use backpack region.
                // Dota uses bit 31 clear, bits 0-15 for position.
                // Position 0 means "not in backpack", valid positions start at 1.
                proto_item.set_inventory(item_seq); // simple 1-based position
                proto_item.set_quantity(1);
                proto_item.set_level(1);
                proto_item.set_quality(4); // Unique
                proto_item.set_flags(0);
                proto_item.set_origin(0);
                proto_item.set_in_use(false);
                proto_item.set_style(0);
                proto_item.set_original_id(item_id);
                proto_item.set_contains_equipped_state(false);
                proto_item.set_contains_equipped_state_v2(false);

                // Add attr=400 (unlocked styles bitmask) to ALL items unconditionally.
                // Setting 0xFFFFFFFF ensures all styles are unlocked from the start,
                // so the client never shows "locked" UI for any style variant.
                // For single-style items this is harmless (style 0 is always available).
                {
                    auto *attr = proto_item.add_attribute();
                    attr->set_def_index(400u);
                    uint32_t all_unlocked = 0xFFFFFFFFu;
                    std::string val_bytes(reinterpret_cast<const char *>(&all_unlocked), 4);
                    attr->set_value_bytes(val_bytes);
                }

                item_object->add_object_data(proto_item.SerializeAsString());

                // Also add to in-memory items vector so equip handler (2569) can find them
                Econ_Item mem_item;
                mem_item.id = item_id;
                mem_item.def = def.def_index;
                mem_item.level = 1;
                mem_item.quality = static_cast<EItemQuality>(4); // Unique
                mem_item.inv_pos = item_seq;
                mem_item.quantity = 1;
                mem_item.flags = 0;
                mem_item.origin = 0;
                mem_item.in_use = false;
                mem_item.original_id = item_id;
                mem_item.style = 0;

                // Add style unlock attr to in-memory item (all items get attr 400)
                {
                    Econ_Item_Attribute unlock_attr;
                    unlock_attr.def = 400u;
                    uint32_t all_unlocked = 0xFFFFFFFFu;
                    unlock_attr.value_bytes.assign(reinterpret_cast<const char *>(&all_unlocked), 4);
                    unlock_attr.type = Econ_Item_Attribute::ATTR_TYPE_INT;
                    mem_item.attributes.push_back(unlock_attr);
                }

                items.push_back(mem_item);

                item_seq++;
                injected_count++;

                // Inject sticker quality variants (quality field = 22, 23, 24)
                // Each sticker has Glitter/Holo/Gold variants that the client
                // treats as separate collectible items. The variant is determined
                // by CSOEconItem.quality field, not by attributes.
                // These do NOT count against max_items to avoid displacing other base items.
                if (def.is_sticker) {
                    static const uint32_t sticker_variant_qualities[] = { 22, 23, 24 };
                    for (uint32_t sq : sticker_variant_qualities) {
                        CSOEconItem variant_item;
                        uint64_t var_id = (static_cast<uint64_t>(0x40000000u + item_seq) << 32ull) | static_cast<uint64_t>(account_id);
                        variant_item.set_id(var_id);
                        variant_item.set_account_id(account_id);
                        variant_item.set_def_index(def.def_index);
                        variant_item.set_inventory(item_seq);
                        variant_item.set_quantity(1);
                        variant_item.set_level(1);
                        variant_item.set_quality(sq); // 22=Glitter, 23=Holo, 24=Gold
                        variant_item.set_flags(0);
                        variant_item.set_origin(0);
                        variant_item.set_in_use(false);
                        variant_item.set_style(0);
                        variant_item.set_original_id(var_id);
                        variant_item.set_contains_equipped_state(false);
                        variant_item.set_contains_equipped_state_v2(false);

                        item_object->add_object_data(variant_item.SerializeAsString());

                        // In-memory copy
                        Econ_Item var_mem;
                        var_mem.id = var_id;
                        var_mem.def = def.def_index;
                        var_mem.level = 1;
                        var_mem.quality = static_cast<EItemQuality>(sq);
                        var_mem.inv_pos = item_seq;
                        var_mem.quantity = 1;
                        var_mem.flags = 0;
                        var_mem.origin = 0;
                        var_mem.in_use = false;
                        var_mem.original_id = var_id;
                        var_mem.style = 0;

                        items.push_back(var_mem);
                        item_seq++;
                    }
                }
            }

            GBE_GC_DebugLog("GC_DOTA_ITEMS", "injected %zu CSOEconItem entries from %zu unique defs (skipped %zu existing)",
                injected_count, vpk_item_defs.size(), existing_defs.size());

            // Ensure ALL existing items have attr=400 with all bits set.
            // Items loaded from items.json may or may not already have attr=400.
            {
                size_t patched_count = 0;
                uint32_t all_unlocked = 0xFFFFFFFFu;
                for (Econ_Item &item : items) {
                    bool found = false;
                    for (auto &attr : item.attributes) {
                        if (attr.def == 400u) {
                            if (attr.value_bytes.size() < 4 ||
                                memcmp(attr.value_bytes.data(), &all_unlocked, 4) != 0) {
                                attr.value_bytes.assign(reinterpret_cast<const char *>(&all_unlocked), 4);
                                patched_count++;
                            }
                            found = true;
                            break;
                        }
                    }
                    if (!found) {
                        Econ_Item_Attribute unlock_attr;
                        unlock_attr.def = 400u;
                        unlock_attr.value_bytes.assign(reinterpret_cast<const char *>(&all_unlocked), 4);
                        unlock_attr.type = Econ_Item_Attribute::ATTR_TYPE_INT;
                        item.attributes.push_back(unlock_attr);
                        patched_count++;
                    }
                }
                if (patched_count > 0) {
                    GBE_GC_DebugLog("GC_DOTA_ITEMS", "ensured attr=400 on %zu items (all styles unlocked)", patched_count);
                }
            }
        }
    }

    std::string updated;
    ser_var<ProtoBufMsgHeader_t>(updated, hdr);
    updated.append(proto_header_ptr, hdr.m_cubProtoBufExtHdr);
    protomsg.AppendToString(&updated);
    message.swap(updated);

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "patched login CacheSubscribed inventory items=%zu had_type1=%d total_types=%d",
        local_items.size(),
        patched_items ? 1 : 0,
        protomsg.objects_size()
    );

    return true;
}

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

    // Only declare equipped_state presence when the item actually has equip
    // data.  Setting contains_equipped_state=true on items with an empty
    // equipped_state list causes the engine to "lock in" the item as
    // unequipped in the SO cache established by the login CacheSubscribed.
    // Subsequent emsg=26 (UpdateMultiple) cannot override this because the
    // shared SO cache is already marked [in cache].  By omitting the flag
    // for items without equip_states, the engine treats their equip status
    // as unknown, allowing later updates to take effect.
    if (!item.equip_states.empty()) {
        proto_item.set_contains_equipped_state(true);
        proto_item.set_contains_equipped_state_v2(true);
    }

    for (const auto &[class_id, slot_id] : item.equip_states) {
        auto proto_equip = proto_item.add_equipped_state();
        proto_equip->set_new_class(class_id);
        proto_equip->set_new_slot(slot_id);
    }

    // Serialize all item attributes (including attr=400 for style unlock)
    for (const Econ_Item_Attribute &attr : item.attributes) {
        auto proto_attr = proto_item.add_attribute();
        proto_attr->set_def_index(attr.def);
        if (gc_version < 20130319 || is_portal2) {
            // Derp.
            uint32 value;
            memcpy(&value, &attr.value, sizeof(uint32));
            proto_attr->set_value(value);
        } else {
            proto_attr->set_value_bytes(attr.value_bytes);
        }
    }

    return proto_item.SerializeAsString();
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

void Steam_Game_Coordinator::handle_motd_request(const void *input, uint32 input_size)
{
    if (is_server || input_size < 24)
        return;

    const char *p = reinterpret_cast<const char *>(input);
    GCMsgHdrEx_t hdr = parse_msg_header(p);
    uint32 last_req_time = deser_var<uint32>(p);
    uint16 language = deser_var<uint16>(p);
    PRINT_DEBUG("%u %u", last_req_time, language);

    uint32 msg_type = EGCItemMsg::k_EMsgGCMOTDRequestResponse;
    std::string message = build_msg_header();
    uint16 num_entries = 0;
    ser_var<uint16>(message, num_entries);

    push_incoming(msg_type, message);
}

void Steam_Game_Coordinator::handle_respawn(const void *input, uint32 input_size)
{
    if (is_server || input_size < 19)
        return;

    auto gameserver_items_msg = new GameServer_Items_Messages();
    gameserver_items_msg->set_type(GameServer_Items_Messages::Request_Respawn);
    gameserver_items_msg->set_is_gc(true);

    Common_Message msg{};
    msg.set_allocated_gameserver_items_messages(gameserver_items_msg);
    msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
    network->sendToAllGameservers(&msg, true);
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

void Steam_Game_Coordinator::callback_client_welcome()
{
    if (!gc_initialized)
        return;

    uint32 msg_type = EGCBaseClientMsg::k_EMsgGCClientWelcome | protobuf_mask;
    std::string message = build_protomsg_header(msg_type);

    CMsgClientWelcome protomsg;
    protomsg.set_version(0);

    protomsg.AppendToString(&message);
    push_incoming(msg_type, message);
}

void Steam_Game_Coordinator::callback_server_welcome()
{
    if (!gc_initialized)
        return;

    uint32 msg_type = EGCBaseClientMsg::k_EMsgGCServerWelcome | protobuf_mask;
    std::string message = build_protomsg_header(msg_type);

    CMsgServerWelcome protomsg;
    protomsg.set_min_allowed_version(0);
    protomsg.set_active_version(0);

    protomsg.AppendToString(&message);
    push_incoming(msg_type, message);
}

void Steam_Game_Coordinator::GBE_MaybePrimeDotaServerWelcomeFromCache(const char *reason)
{
    if (!is_server || gc_profile != GC_PROFILE_DOTA2 || !GBE_last_dota_server_hello_context.valid)
        return;

    if (!gc_initialized)
        initialize_gc();

    GBE_RestoreSharedDotaLobbyState("prime_server_welcome");

    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || welcome_received)
        return;

    if (GBE_ShouldSuppressDotaAbandonedLobby(GBE_local_lobby.lobby_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SERVER_HELLO",
            "skipping cached ServerWelcome for suppressed abandoned lobby reason=%s lobby_id=%llu",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id)
        );
        return;
    }

    std::queue<GC_Message> queued_messages = incoming_messages;
    while (!queued_messages.empty()) {
        if (GBE_GC_MaskedEMsg(queued_messages.front().msg_type) == EGCBaseClientMsg::k_EMsgGCServerWelcome)
            return;
        queued_messages.pop();
    }

    std::string welcome_message;
    if (!GBE_BuildDirectDotaServerWelcome(
            settings->get_local_steam_id().ConvertToUint64(),
            settings->get_local_game_id().AppID(),
            GBE_last_dota_server_hello_context,
            welcome_message)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SERVER_HELLO",
            "failed priming cached ServerWelcome reason=%s lobby_id=%llu",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id)
        );
        return;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_SERVER_HELLO",
        "priming cached ServerWelcome reason=%s lobby_id=%llu size=%zu active_version=%u",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        welcome_message.size(),
        GBE_last_dota_server_hello_context.active_version
    );
    push_incoming_now(EGCBaseClientMsg::k_EMsgGCServerWelcome | GBE_kProtoMask, welcome_message);

    std::string runtime_cache_message;
    const uint64 owner_steam_id = GBE_GetDotaLobbyOwnerSteamId();
    const uint32 owner_account_id = GBE_GetDotaLobbyOwnerAccountId();
    const bool launch_started = GBE_local_lobby.match_id != 0;
    const bool built_runtime_cache = GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed(
        GBE_local_lobby,
        GBE_local_lobby.owner_name,
        runtime_cache_message,
        true);

    if (built_runtime_cache && !GBE_ShouldSuppressDotaAbandonedLobby(GBE_local_lobby.lobby_id)) {
        GBE_RecordDotaLobbyCacheSubscriptionState(runtime_cache_message, "prime_server_welcome_current_cache_subscribed");
        push_incoming_now(GBE_kDotaCacheSubscribed | GBE_kProtoMask, runtime_cache_message);
        GBE_GC_DebugLog(
            "GC_DOTA_SERVER_HELLO",
            "queued synthetic CacheSubscribed after cached ServerWelcome reason=%s lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu launch_started=%u owner_steam_id=%llu owner_account_id=%u size=%zu",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id),
            launch_started ? 1u : 0u,
            static_cast<unsigned long long>(owner_steam_id),
            owner_account_id,
            runtime_cache_message.size()
        );
    } else if (!built_runtime_cache) {
        GBE_GC_DebugLog(
            "GC_DOTA_SERVER_HELLO",
            "failed building synthetic CacheSubscribed after cached ServerWelcome reason=%s lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id)
        );
    } else {
        GBE_GC_DebugLog(
            "GC_DOTA_SERVER_HELLO",
            "skipped synthetic CacheSubscribed after cached ServerWelcome for suppressed abandoned lobby reason=%s lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id)
        );
    }
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

void Steam_Game_Coordinator::callback_respawn_request(CSteamID steam_id)
{
    if (!gc_initialized)
        return;

    uint32 msg_type = EGCItemMsg::k_EMsgGCRespawnPostLoadoutChange;
    std::string message = build_msg_header();
    ser_var<uint64>(message, steam_id.ConvertToUint64());

    push_incoming(msg_type, message);
}

void Steam_Game_Coordinator::steam_network_callback(void *object, Common_Message *msg)
{
    //PRINT_DEBUG_ENTRY();

    auto inst = (Steam_Game_Coordinator *)object;
    inst->network_callback(msg);
}

void Steam_Game_Coordinator::steam_run_every_runcb(void *object)
{
    // PRINT_DEBUG_ENTRY();

    Steam_Game_Coordinator *steam_gamecoordinator = (Steam_Game_Coordinator *)object;
    steam_gamecoordinator->RunCallbacks();
}

Steam_Game_Coordinator::Steam_Game_Coordinator(class Settings *settings, class Networking *network, class Local_Storage *local_storage, class SteamCallBacks *callbacks, class RunEveryRunCB *run_every_runcb, bool is_server)
{
    this->settings = settings;
    this->network = network;
    this->local_storage = local_storage;
    this->callbacks = callbacks;
    this->run_every_runcb = run_every_runcb;
    this->is_server = is_server;

    this->network->setCallback(CALLBACK_ID_GAMESERVER_ITEMS, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->network->setCallback(CALLBACK_ID_FRIEND_MESSAGES, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->network->setCallback(CALLBACK_ID_STEAM_MESSAGES, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->network->setCallback(CALLBACK_ID_USER_STATUS, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->run_every_runcb->add(&Steam_Game_Coordinator::steam_run_every_runcb, this);

    parse_gc_config();

    if (gc_profile == GC_PROFILE_DOTA2) {
        GBE_GC_DebugLog(
            "GC_INIT",
            "eagerly initializing Dota2 GC from constructor this=%p is_server=%u",
            static_cast<void *>(this),
            this->is_server ? 1u : 0u
        );
        initialize_gc();
    }

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "coordinator init this=%p is_server=%u shared_lobby=%p shared_valid=%u active=%u lobby_id=%llu match_id=%llu state=%u game_state=%u",
        static_cast<void *>(this),
        this->is_server ? 1u : 0u,
        static_cast<void *>(&GBE_shared_dota_lobby_state),
        GBE_shared_dota_lobby_state.valid ? 1u : 0u,
        GBE_shared_dota_lobby_state.active ? 1u : 0u,
        static_cast<unsigned long long>(GBE_shared_dota_lobby_state.lobby_id),
        static_cast<unsigned long long>(GBE_shared_dota_lobby_state.match_id),
        GBE_shared_dota_lobby_state.state,
        GBE_shared_dota_lobby_state.game_state
    );
}

Steam_Game_Coordinator::~Steam_Game_Coordinator()
{
    this->network->rmCallback(CALLBACK_ID_GAMESERVER_ITEMS, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->network->rmCallback(CALLBACK_ID_FRIEND_MESSAGES, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->network->rmCallback(CALLBACK_ID_STEAM_MESSAGES, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->network->rmCallback(CALLBACK_ID_USER_STATUS, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->run_every_runcb->remove(&Steam_Game_Coordinator::steam_run_every_runcb, this);
}

void Steam_Game_Coordinator::initialize_gc()
{
    if (!gc_enabled() || gc_initialized)
        return;

    gc_initialized = true;

    if (gc_profile == GC_PROFILE_DOTA2) {
        GBE_RestoreSharedDotaLobbyState("initialize_gc");
        GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot("initialize_gc");
        GBE_GC_DebugLog("GC_INIT", "initialized Dota2 GC profile for app %u", settings->get_local_game_id().AppID());
        return;
    }

    if (is_server) {
        callback_server_welcome();
    } else {
        callback_client_welcome();

        // Load user's items.
        const auto &items = load_items_from_file();
        callback_items_received(settings->get_local_steam_id(), items);
    }

    // Wait a bit until after the game has received welcome message from us before posting anything else.
    // This avoids a race condition that can cause the game to receive inventory before parsing item schema.
    // For old versions of TF2, we can't do that because they instead have a different bug where receiving
    // the inventory late causes erroneous "new items" notifications.
    if (gc_version >= 20110414) {
        delay_init = true;
    }
}

void Steam_Game_Coordinator::shutdown_gc()
{
    if (!gc_initialized)
        return;

    GBE_GC_DebugLog(
        "GC_INIT",
        "shutdown GC this=%p is_server=%u profile=%u pending_size=%zu incoming_size=%zu active=%u lobby_id=%llu state=%u game_state=%u",
        static_cast<void *>(this),
        is_server ? 1u : 0u,
        static_cast<uint32>(gc_profile),
        pending_messages.size(),
        incoming_messages.size(),
        GBE_local_lobby.active ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        GBE_local_lobby.state,
        GBE_local_lobby.game_state
    );

    items_loaded = false;
    items.clear();
    all_user_items.clear();
    pending_items_requests.clear();
    pending_messages.clear();
    while (incoming_messages.size())
        incoming_messages.pop();

    welcome_received = false;
    delay_init = false;
    GBE_dota_login_sync_sent = false;
    GBE_dota_host_showcase_equip_pushed = false;
    GBE_dota_private_lobby_snapshot_replayed = false;
    GBE_last_dota_launch_state_pushed_game_state = 0;
    GBE_last_lobby_poll_time = {};
    if (gc_profile == GC_PROFILE_DOTA2) {
        const uint64 previous_lobby_id = GBE_local_lobby.lobby_id;
        GBE_ResetDotaPracticeLobbyLaunchPeripheralState();
        GBE_local_lobby = GBE_LocalLobby{};
        GBE_shared_dota_lobby_state = GBE_SharedDotaLobbyState{};
        GBE_recent_dota_reconnect_context_valid = false;
        GBE_recent_dota_reconnect_context = GBE_DotaReconnectContext{};
        GBE_pending_dota_normal_signout_finalize_after_25 = false;
        GBE_pending_dota_normal_signout_finalize_lobby_id = 0;
        if (settings && settings->get_lobby().ConvertToUint64() != 0)
            settings->set_lobby(k_steamIDNil);
        GBE_GC_DebugLog(
            "GC_INIT",
            "cleared Dota2 GC runtime on shutdown this=%p previous_lobby_id=%llu",
            static_cast<void *>(this),
            static_cast<unsigned long long>(previous_lobby_id)
        );
    }
    gc_initialized = false;
}

void Steam_Game_Coordinator::on_appid_changed(uint32 appid)
{
    if (!appid)
        return;

    const uint32 previous_profile = static_cast<uint32>(gc_profile);
    parse_gc_config();
    GBE_GC_DebugLog(
        "GC_CONFIG",
        "refreshed GC config after appid change this=%p is_server=%u appid=%u previous_profile=%u new_profile=%u initialized=%u",
        static_cast<void *>(this),
        is_server ? 1u : 0u,
        appid,
        previous_profile,
        static_cast<uint32>(gc_profile),
        gc_initialized ? 1u : 0u
    );

    if (gc_profile == GC_PROFILE_DOTA2)
        initialize_gc();
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

void Steam_Game_Coordinator::on_client_connected(CSteamID steam_id)
{
    if (!steam_id.BIndividualAccount())
        return;

    if (is_server && gc_profile == GC_PROFILE_DOTA2) {
        GBE_RestoreSharedDotaLobbyState("on_client_connected");

        const uint64 connected_steam_id = steam_id.ConvertToUint64();
        const uint64 owner_steam_id = GBE_GetDotaLobbyOwnerSteamId();
        if (gc_initialized && GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && connected_steam_id != 0 && connected_steam_id == owner_steam_id) {
            if (GBE_ShouldSuppressDotaAbandonedLobby(GBE_local_lobby.lobby_id)) {
                GBE_GC_DebugLog(
                    "GC_DOTA_SYNC",
                    "ignoring owner reconnect for suppressed abandoned lobby steam_id=%llu lobby_id=%llu state=%u game_state=%u",
                    static_cast<unsigned long long>(connected_steam_id),
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state
                );
                return;
            }
            if (!GBE_local_lobby.owner_connected) {
                GBE_local_lobby.owner_connected = true;
                GBE_PublishSharedDotaLobbyState("owner_connected");
            }
        } else if (gc_initialized && GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && connected_steam_id != 0) {
            if (GBE_SetDotaLobbyMemberConnected(connected_steam_id, true)) {
                GBE_PublishSharedDotaLobbyState("member_connected");
                GBE_GC_DebugLog(
                    "GC_DOTA_SYNC",
                    "marked Dota lobby member connected steam_id=%llu lobby_id=%llu state=%u game_state=%u members=%zu",
                    static_cast<unsigned long long>(connected_steam_id),
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state,
                    GBE_local_lobby.members.size()
                );
            }
        }
    }

    if (gc_initialized) {
        request_user_items(steam_id, generate_steam_api_call_id(), true);
    }
}

void Steam_Game_Coordinator::on_client_disconnected(CSteamID steam_id)
{
    if (!steam_id.BIndividualAccount())
        return;

    bool suppress_user_item_unsubscribe = false;
    if (is_server && gc_profile == GC_PROFILE_DOTA2) {
        const uint64 disconnected_steam_id = steam_id.ConvertToUint64();
        const uint64 owner_steam_id = GBE_GetDotaLobbyOwnerSteamId();

        // In PostGame (state >= 3), do NOT publish shared state for disconnect
        // events.  The 7004 signout finalize path handles PostGame cleanup.
        // Re-publishing state=3 here would race with finalize and leave stale
        // shared state that causes an initialize_gc adopt loop.
        const bool postgame_suppress_publish = GBE_local_lobby.state >= 3u;

        if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && disconnected_steam_id != 0 && disconnected_steam_id == owner_steam_id) {
            GBE_local_lobby.owner_connected = false;
            if (!postgame_suppress_publish)
                GBE_PublishSharedDotaLobbyState("owner_disconnected");

            suppress_user_item_unsubscribe = true;
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "preserving owner inventory cache across dota reconnect steam_id=%llu lobby_id=%llu state=%u game_state=%u launch_phase=%s abandon_postgame=%u postgame_suppress=%u",
                static_cast<unsigned long long>(disconnected_steam_id),
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state,
                GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase),
                GBE_local_lobby.abandon_postgame_active ? 1u : 0u,
                postgame_suppress_publish ? 1u : 0u
            );
        } else if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && disconnected_steam_id != 0) {
            if (GBE_SetDotaLobbyMemberConnected(disconnected_steam_id, false)) {
                if (!postgame_suppress_publish)
                    GBE_PublishSharedDotaLobbyState("member_disconnected");
                GBE_GC_DebugLog(
                    "GC_DOTA_SYNC",
                    "marked Dota lobby member disconnected steam_id=%llu lobby_id=%llu state=%u game_state=%u members=%zu postgame_suppress=%u",
                    static_cast<unsigned long long>(disconnected_steam_id),
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state,
                    GBE_local_lobby.members.size(),
                    postgame_suppress_publish ? 1u : 0u
                );
            }
        }
    }

    if (!suppress_user_item_unsubscribe)
        remove_user_items(steam_id);
}

void Steam_Game_Coordinator::GBE_PushDotaLoginSyncMessages()
{
    if (GBE_dota_login_sync_sent)
        return;

    const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    const uint32 account_id = settings->get_local_steam_id().GetAccountID();

    std::string cache_subscribed_message;
    if (!GBE_PrepareDotaDirectReplayMessage(
            GBE_kDotaCacheSubscribedTemplate,
            sizeof(GBE_kDotaCacheSubscribedTemplate),
            account_id,
            steam_id,
            true,
            true,
            false,
            0,
            24u,
            24u,
            0,
            "login cache subscribed template",
            cache_subscribed_message)) {
        GBE_GC_DebugLog("GC_DOTA_SYNC", "failed to build CacheSubscribed replay steamid=%llu accountid=%u", static_cast<unsigned long long>(steam_id), account_id);
        return;
    }

    if (!GBE_PatchDotaLoginCacheSubscribedInventory(cache_subscribed_message)) {
        GBE_GC_DebugLog("GC_DOTA_SYNC", "failed patching login CacheSubscribed inventory steamid=%llu accountid=%u", static_cast<unsigned long long>(steam_id), account_id);
    }

    GBE_dota_login_sync_sent = true;
    GBE_GC_DebugLog("GC_DOTA_SYNC", "queueing CacheSubscribed replay size=%zu steamid=%llu accountid=%u", cache_subscribed_message.size(), static_cast<unsigned long long>(steam_id), account_id);
    push_incoming_now(24u | GBE_kProtoMask, cache_subscribed_message);
}

bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl(
    uint64 steam_id,
    uint32 account_id,
    uint64 lobby_id,
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
    const std::string &pass_key,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    bool rewrite_runtime_fields,
    bool rewrite_2015,
    uint32 extra_startup_account_id,
    const GBE_DotaCustomGameDetails *custom_game,
    std::string &message)
{
    if (steam_id == 0 || account_id == 0 || lobby_id == 0)
        return false;

    if (!GBE_PrepareDotaDirectReplayMessage(
            GBE_kDotaPracticeLobbyCacheSubscribedTemplate,
            sizeof(GBE_kDotaPracticeLobbyCacheSubscribedTemplate),
            account_id,
            steam_id,
            false,
            false,
            false,
            0,
            7038u,
            24u,
            0,
            "practice lobby cache template",
            message)) {
        return false;
    }

    if (!GBE_PatchDotaLobbyTemplateIdentifiers(message, account_id, steam_id, lobby_id))
        return false;

    return GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
        message,
        account_id,
        steam_id,
        lobby_id,
        rewrite_runtime_fields,
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
        rewrite_2015,
        extra_startup_account_id,
        pass_key,
        custom_game);
}

bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl(
    uint64 steam_id,
    uint64 lobby_id,
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
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    const GBE_DotaCustomGameDetails *custom_game,
    std::string &message)
{
    if (steam_id == 0 || lobby_id == 0)
        return false;

    return GBE_AdaptDotaPracticeLobbyCacheSubscribedPayload(
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
        has_broadcast_channel,
        broadcast_channel_id,
        broadcast_country_code,
        broadcast_description,
        broadcast_language_code,
        pass_key,
        extra_startup_account_id,
        message,
        custom_game);
}

bool Steam_Game_Coordinator::GBE_TryQueueDotaPrelaunch021(const char *note, uint32 trigger_emsg, uint64 source_job)
{
    GBE_LocalLobby wait_for_players_lobby = GBE_local_lobby;
    wait_for_players_lobby.state = 2u;
    wait_for_players_lobby.game_state = 1u;

    std::string wait_for_players_message;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(wait_for_players_lobby, GBE_local_lobby.owner_name, wait_for_players_message, is_server))
        return false;

    push_incoming_now(
        GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask,
        wait_for_players_message,
        true,
        wait_for_players_lobby.state,
        wait_for_players_lobby.game_state);
    GBE_GC_DebugLog(
        "GC_DOTA_DIRECT",
        "replying req=%u resp=%u source_job=%llu size=%zu note=%s apply_state=%u apply_game_state=%u source=runtime",
        trigger_emsg,
        GBE_kDotaPracticeLobbyDetailsUpdate,
        static_cast<unsigned long long>(source_job),
        wait_for_players_message.size(),
        note ? note : "unknown",
        wait_for_players_lobby.state,
        wait_for_players_lobby.game_state
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_SetDotaLobbyMemberConnected(uint64 steam_id, bool connected)
{
    if (steam_id == 0ull)
        return false;

    bool changed = false;
    if (steam_id == GBE_local_lobby.owner_steam_id && GBE_local_lobby.owner_connected != connected) {
        GBE_local_lobby.owner_connected = connected;
        changed = true;
    }

    const bool has_custom_game = gbe::dota_custom_game::has_custom_game_details(GBE_local_lobby.custom_game);
    const bool should_mark_leaver =
        GBE_local_lobby.state == 2u &&
        GBE_local_lobby.game_state >= 1u &&
        GBE_local_lobby.match_id != 0ull;
    changed = gbe::dota_lobby_flow::set_lobby_member_connected(
        GBE_local_lobby.members,
        steam_id,
        CSteamID((uint64)steam_id).GetAccountID(),
        connected,
        has_custom_game,
        should_mark_leaver,
        GBE_local_lobby.owner_steam_id,
        GBE_local_lobby.owner_slot,
        GBE_kDotaTeamGoodGuys,
        GBE_kDotaTeamPlayerPool) || changed;

    return changed;
}

bool Steam_Game_Coordinator::GBE_SetDotaLobbyMemberRuntimeState(uint64 steam_id, bool connected, uint32 hero_id, bool has_hero_id)
{
    if (steam_id == 0ull)
        return false;

    bool changed = GBE_SetDotaLobbyMemberConnected(steam_id, connected);
    if (has_hero_id && hero_id != 0u) {
        if (steam_id == GBE_local_lobby.owner_steam_id && GBE_local_lobby.owner_hero_id != hero_id) {
            GBE_local_lobby.owner_hero_id = hero_id;
            changed = true;
        }
        changed = gbe::dota_lobby_flow::set_lobby_member_hero(GBE_local_lobby.members, steam_id, hero_id) || changed;
    }

    return changed;
}

bool Steam_Game_Coordinator::GBE_ShouldHoldDotaLanLaunchForRemoteMembers(uint32 next_game_state, uint32 *remote_count_out, uint32 *connected_remote_count_out) const
{
    if (remote_count_out)
        *remote_count_out = 0u;
    if (connected_remote_count_out)
        *connected_remote_count_out = 0u;

    if (next_game_state < 2u)
        return false;
    if (!GBE_local_lobby.active || GBE_local_lobby.state != 2u || GBE_local_lobby.match_id == 0ull)
        return false;
    if (gbe::proto_wire::parse_dota_practice_lobby_connect_ipv4(GBE_local_lobby.connect) == 0u && !GBE_local_lobby.lan)
        return false;

    uint32 remote_count = 0u;
    uint32 connected_remote_count = 0u;
    const uint64 owner_steam_id = GBE_GetDotaLobbyOwnerSteamId();
    const bool should_hold = gbe::dota_lobby_flow::should_hold_lan_launch_for_remote_members(GBE_local_lobby.members, owner_steam_id, remote_count, connected_remote_count);

    if (remote_count_out)
        *remote_count_out = remote_count;
    if (connected_remote_count_out)
        *connected_remote_count_out = connected_remote_count;

    return should_hold;
}

bool Steam_Game_Coordinator::GBE_TryQueueDotaRuntimeLobbyDetailsUpdate(const char *note, uint32 trigger_emsg, uint64 source_job, uint32 next_state, uint32 next_game_state, double delay)
{
    if (GBE_ShouldSuppressDotaAbandonedLobby(GBE_local_lobby.lobby_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "skipping runtime lobby update for suppressed abandoned lobby req=%u source_job=%llu note=%s lobby_id=%llu state=%u game_state=%u next_state=%u next_game_state=%u",
            trigger_emsg,
            static_cast<unsigned long long>(source_job),
            note ? note : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            next_state,
            next_game_state
        );
        return false;
    }

    GBE_LocalLobby next_lobby = GBE_local_lobby;
    next_lobby.state = next_state;
    next_lobby.game_state = next_game_state;

    auto queue_details_update = [&](Steam_Game_Coordinator *target) -> bool {
        if (!target || target->gc_profile != GC_PROFILE_DOTA2)
            return false;

        std::string response_message;
        if (!target->GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(next_lobby, target->GBE_GetDotaLobbyOwnerName(), response_message, target->is_server))
            return false;

        if (delay > 0.0) {
            target->push_incoming(
                GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask,
                response_message,
                delay,
                true,
                next_lobby.state,
                next_lobby.game_state);
        } else {
            target->push_incoming_now(
                GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask,
                response_message,
                true,
                next_lobby.state,
                next_lobby.game_state);
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu size=%zu note=%s apply_state=%u apply_game_state=%u delay=%.3f source=runtime target=%p",
            trigger_emsg,
            GBE_kDotaPracticeLobbyDetailsUpdate,
            static_cast<unsigned long long>(source_job),
            response_message.size(),
            note ? note : "unknown",
            next_lobby.state,
            next_lobby.game_state,
            delay,
            static_cast<void *>(target)
        );

        return true;
    };

    if (!queue_details_update(this))
        return false;

    if (is_server && GBE_local_lobby.state == 2u && GBE_local_lobby.game_state >= 1u) {
        if (delay > 0.0) {
            Steam_Client *steam_client = get_steam_client();
            Steam_Game_Coordinator *client_target = steam_client ? steam_client->steam_game_coordinator : nullptr;
            if (client_target && client_target != this)
                queue_details_update(client_target);
        } else {
            GBE_PushDotaLaunchStateToClientPeer(note ? note : "runtime_lobby_update");
        }
    }

    return true;
}

bool Steam_Game_Coordinator::GBE_HasDotaLaunchServerSetupSync() const
{
    return gbe::dota_lobby_state::has_launch_server_setup_sync(GBE_local_lobby);
}

void Steam_Game_Coordinator::GBE_MarkDotaLaunchPhase(uint32 phase, const char *reason)
{
    if (GBE_local_lobby.launch_phase >= phase)
        return;

    const uint32 previous_phase = GBE_local_lobby.launch_phase;
    GBE_local_lobby.launch_phase = phase;
    GBE_PublishSharedDotaLobbyState(reason ? reason : "launch_phase");
    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "advanced launch phase reason=%s lobby_id=%llu state=%u game_state=%u previous=%s next=%s",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        GBE_DescribeDotaLaunchPhase(previous_phase),
        GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase)
    );
}

bool Steam_Game_Coordinator::GBE_TryAdvanceDotaLaunchToRun(const char *note, uint32 trigger_emsg, uint64 source_job, const char *reason, uint32 next_game_state)
{
    const gbe::dota_lobby_state::LaunchRunPlan launch_plan = gbe::dota_lobby_state::compose_launch_run_plan(
        GBE_local_lobby,
        GBE_kDotaLaunchPhaseSetupSynced,
        GBE_kDotaLaunchPhaseRunQueued,
        next_game_state);
    if (!launch_plan.can_advance)
        return false;

    GBE_MarkDotaLaunchPhase(launch_plan.launch_phase, reason ? reason : "launch_run_queued");

    return GBE_TryQueueDotaRuntimeLobbyDetailsUpdate(
        note ? note : "runtime packet after launch run gate",
        trigger_emsg,
        source_job,
        launch_plan.next_state,
        launch_plan.next_game_state);
}









std::string Steam_Game_Coordinator::GBE_GetDotaJoinableCustomLobbiesHTTPJSON(uint64 requested_custom_game_id)
{
    nlohmann::json response = nlohmann::json::object();
    response["lobbies"] = nlohmann::json::array();

    std::vector<GBE_LocalLobby> lobbies = GBE_GetDotaGenericLobbySnapshots("http_joinable_custom_lobbies");
    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0ull)
        lobbies.push_back(GBE_local_lobby);

    std::vector<uint64> seen_lobby_ids;
    for (const GBE_LocalLobby &lobby : lobbies) {
        if (!gbe::dota_custom_game::should_include_joinable_custom_lobby(
                lobby.active,
                lobby.lobby_id,
                lobby.custom_game.game_id,
                requested_custom_game_id,
                seen_lobby_ids))
            continue;

        const GBE_DotaJoinableCustomLobbyItemData item_data = gbe::dota_custom_game::compose_joinable_custom_lobby_item_data(
            lobby.custom_game,
            lobby.members.size(),
            lobby.owner_account_id,
            settings ? settings->get_local_steam_id().GetAccountID() : 0u,
            lobby.owner_name,
            settings ? std::string(settings->get_local_name()) : std::string(),
            lobby.room_name,
            lobby.game_start_time,
            static_cast<uint32>(std::time(nullptr)));
        const std::string display_name = GBE_DotaCustomGameDisplayName(settings, lobby.custom_game, item_data.room_name);

        nlohmann::json item = gbe::dota_custom_lobby_http::compose_joinable_custom_lobby_json_item(
            item_data,
            lobby.custom_game,
            display_name,
            lobby.lobby_id,
            lobby.custom_game.game_id,
            lobby.server_region,
            !lobby.pass_key.empty(),
            lobby.lan_host_ping_location,
            lobby.custom_game.timestamp,
            std::to_string(lobby.custom_game.crc),
            lobby.custom_game.penalties);
        response["lobbies"].push_back(std::move(item));
        seen_lobby_ids.push_back(lobby.lobby_id);
    }

    return response.dump();
}


void Steam_Game_Coordinator::ResetGCMemory(const char *reason, bool leave_generic_lobby, bool clear_queued_messages)
{
    const uint64 previous_lobby_id = GBE_local_lobby.lobby_id;
    const uint64 previous_match_id = GBE_local_lobby.match_id;
    const uint64 previous_generic_lobby_id = GBE_local_lobby.generic_lobby_id;
    const size_t previous_pending_count = pending_messages.size();
    const size_t previous_incoming_count = incoming_messages.size();

    GBE_ResetDotaPracticeLobbyLaunchPeripheralState();
    GBE_ClearDotaPracticeLobbyLaunchRichPresence();

    if (leave_generic_lobby && GBE_local_lobby.generic_lobby_id != 0)
        GBE_LeaveGenericLobby();

    if (clear_queued_messages) {
        pending_messages.clear();
        std::queue<GC_Message> empty_messages;
        incoming_messages.swap(empty_messages);
        pending_message_sequence = 0;
    }

    GBE_local_lobby = GBE_LocalLobby{};
    GBE_shared_dota_lobby_state = GBE_SharedDotaLobbyState{};
    if (!reason || std::strcmp(reason, "7035_disconnect_current_game_after_25") != 0) {
        GBE_recent_dota_reconnect_context_valid = false;
        GBE_recent_dota_reconnect_context = GBE_DotaReconnectContext{};
    }
    GBE_dota_private_lobby_snapshot_replayed = false;
    GBE_last_dota_launch_state_pushed_game_state = 0;
    GBE_pending_reset_after_cache_unsubscribed = false;
    GBE_pending_reset_after_cache_unsubscribed_lobby_id = 0;
    GBE_pending_dota_normal_signout_finalize_after_25 = false;
    GBE_pending_dota_normal_signout_finalize_lobby_id = 0;
    if (previous_lobby_id != 0 && GBE_suppressed_dota_abandon_lobby_id != previous_lobby_id)
        GBE_ClearDotaAbandonedLobbySuppression(previous_lobby_id, reason ? reason : "reset_gc_memory");
    GBE_pending_dota_abandon_finalize_after_7014 = false;
    GBE_pending_dota_abandon_finalize_lobby_id = 0;
    GBE_SyncSettingsLobbyFromGenericLobby(reason ? reason : "reset_gc_memory");
    GBE_UpdateDotaPracticeLobbyLaunchRichPresence("#DOTA_RP_INIT", "SERVERSETUP", false, false);

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] ResetGCMemory reason=%s leave_generic=%d clear_queues=%d previous_lobby_id=%llu previous_match_id=%llu previous_generic_lobby_id=%llu pending_count=%zu incoming_count=%zu",
        reason ? reason : "unknown",
        leave_generic_lobby ? 1 : 0,
        clear_queued_messages ? 1 : 0,
        static_cast<unsigned long long>(previous_lobby_id),
        static_cast<unsigned long long>(previous_match_id),
        static_cast<unsigned long long>(previous_generic_lobby_id),
        previous_pending_count,
        previous_incoming_count
    );
}



void Steam_Game_Coordinator::GBE_PushDotaLaunchStateToClientPeer(const char *reason)
{
    if (GBE_ShouldSuppressDotaAbandonedLobby(GBE_local_lobby.lobby_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipped pushing launch state from suppressed abandoned lobby reason=%s source=%p is_server=%u lobby_id=%llu",
            reason ? reason : "unknown",
            static_cast<void *>(this),
            is_server ? 1u : 0u,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id)
        );
        return;
    }

    Steam_Game_Coordinator *target = this;
    if (is_server) {
        Steam_Client *steam_client = get_steam_client();
        if (steam_client && steam_client->steam_game_coordinator)
            target = steam_client->steam_game_coordinator;
    }

    if (!target || target->is_server || target->gc_profile != GC_PROFILE_DOTA2)
        return;

    target->GBE_RestoreSharedDotaLobbyState(reason ? reason : "push_launch_state_to_client");

    if (target->GBE_ShouldSuppressDotaAbandonedLobby(GBE_shared_dota_lobby_state.lobby_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipped pushing launch state to client for suppressed abandoned lobby reason=%s target=%p lobby_id=%llu",
            reason ? reason : "unknown",
            static_cast<void *>(target),
            static_cast<unsigned long long>(GBE_shared_dota_lobby_state.lobby_id)
        );
        return;
    }

    GBE_LocalLobby lobby{};
    if (!target->GBE_CaptureCurrentDotaLobbyState(reason ? reason : "push_launch_state_to_client", lobby, false)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipped pushing launch state to client reason=%s target=%p active=0",
            reason ? reason : "unknown",
            static_cast<void *>(target)
        );
        return;
    }

    if (lobby.state != 2u || lobby.game_state < 1u || (lobby.server_id == 0 && lobby.connect.empty())) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipped pushing launch state to client reason=%s target=%p lobby_id=%llu state=%u game_state=%u server_id=%llu",
            reason ? reason : "unknown",
            static_cast<void *>(target),
            static_cast<unsigned long long>(lobby.lobby_id),
            lobby.state,
            lobby.game_state,
            static_cast<unsigned long long>(lobby.server_id)
        );
        return;
    }

    if (target->GBE_last_dota_launch_state_pushed_game_state >= lobby.game_state) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipping duplicate launch state push to client reason=%s target=%p lobby_id=%llu state=%u game_state=%u last_game_state=%u server_id=%llu",
            reason ? reason : "unknown",
            static_cast<void *>(target),
            static_cast<unsigned long long>(lobby.lobby_id),
            lobby.state,
            lobby.game_state,
            target->GBE_last_dota_launch_state_pushed_game_state,
            static_cast<unsigned long long>(lobby.server_id)
        );
        return;
    }

    const uint64 target_local_steam_id = target->settings ? target->settings->get_local_steam_id().ConvertToUint64() : 0ull;
    const bool target_owner_lan_launch =
        target_local_steam_id != 0ull &&
        lobby.owner_steam_id != 0ull &&
        target_local_steam_id == lobby.owner_steam_id &&
        lobby.lan &&
        lobby.match_id != 0ull;

    std::string response_24;
    if (!target->GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed(lobby, target->GBE_GetDotaLobbyOwnerName(), response_24, target_owner_lan_launch)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "failed building launch state 24 for client reason=%s target=%p lobby_id=%llu state=%u game_state=%u server_id=%llu",
            reason ? reason : "unknown",
            static_cast<void *>(target),
            static_cast<unsigned long long>(lobby.lobby_id),
            lobby.state,
            lobby.game_state,
            static_cast<unsigned long long>(lobby.server_id)
        );
        return;
    }

    std::string response_26;
    if (!target->GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(lobby, target->GBE_GetDotaLobbyOwnerName(), response_26, target_owner_lan_launch)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "failed building launch state 26 for client reason=%s target=%p lobby_id=%llu state=%u game_state=%u server_id=%llu",
            reason ? reason : "unknown",
            static_cast<void *>(target),
            static_cast<unsigned long long>(lobby.lobby_id),
            lobby.state,
            lobby.game_state,
            static_cast<unsigned long long>(lobby.server_id)
        );
        return;
    }

    target->GBE_RecordDotaLobbyCacheSubscriptionState(response_24, reason ? reason : "push_launch_state_to_client");
    target->push_incoming_now(GBE_kDotaCacheSubscribed | GBE_kProtoMask, response_24);
    target->push_incoming_now(
        GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask,
        response_26,
        true,
        lobby.state,
        lobby.game_state
    );
    target->GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(reason ? reason : "push_launch_state_to_client");
    target->GBE_last_dota_launch_state_pushed_game_state = lobby.game_state;

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "pushed launch state to client reason=%s target=%p lobby_id=%llu state=%u game_state=%u server_id=%llu size24=%zu size26=%zu",
        reason ? reason : "unknown",
        static_cast<void *>(target),
        static_cast<unsigned long long>(lobby.lobby_id),
        lobby.state,
        lobby.game_state,
        static_cast<unsigned long long>(lobby.server_id),
        response_24.size(),
        response_26.size()
    );
}


std::string Steam_Game_Coordinator::GBE_GetDotaLobbyOwnerName() const
{
    if (!GBE_local_lobby.owner_name.empty())
        return GBE_local_lobby.owner_name;

    if (!GBE_shared_dota_lobby_state.owner_name.empty())
        return GBE_shared_dota_lobby_state.owner_name;

    return std::string(settings->get_local_name());
}




bool Steam_Game_Coordinator::GBE_QueueDotaPostGameTeardown(const char *reason, bool wrapped, const std::string *outer_session_field_raw, bool suppress_previous_chat_channel, bool push_cache_unsubscribed, bool push_postgame_join)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring postgame teardown because no local lobby is active reason=%s", reason ? reason : "unknown");
        return true;
    }

    if (wrapped && !outer_session_field_raw) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for postgame teardown reason=%s LobbyID=%llu", reason ? reason : "unknown", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return false;
    }

    const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    const uint64 pre_postgame_chat_channel_id = suppress_previous_chat_channel ? GBE_local_lobby.chat_channel_id : 0u;

    std::string response_25;
    if (!gbe::gc_message::build_dota_lobby_cache_unsubscribed_payload(lobby_id, response_25)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 payload for postgame teardown LobbyID=%llu reason=%s", static_cast<unsigned long long>(lobby_id), reason ? reason : "unknown");
        return false;
    }

    GBE_ResetDotaPracticeLobbyLaunchPeripheralState();

    GBE_local_lobby.state = 3u;
    GBE_local_lobby.game_state = 6u;
    GBE_local_lobby.has_chat_channel = true;
    GBE_local_lobby.chat_channel_id = GBE_GenerateDotaPostGameChatChannelId();
    GBE_local_lobby.chat_channel_name = "PostGame_" + std::to_string(lobby_id);
    GBE_local_lobby.chat_channel_type = 18u;
    GBE_local_lobby.abandon_pre_postgame_chat_channel_id = pre_postgame_chat_channel_id;
    GBE_local_lobby.has_cache_version = false;
    GBE_local_lobby.cache_version = 0;
    GBE_local_lobby.has_cache_service_id = false;
    GBE_local_lobby.cache_service_id = 0;
    GBE_local_lobby.cache_service_list.clear();
    GBE_local_lobby.has_cache_sync_version = false;
    GBE_local_lobby.cache_sync_version = 0;
    GBE_local_lobby.abandon_postgame_active = true;
    GBE_PublishSharedDotaLobbyState(reason ? reason : "postgame_teardown");

    std::string response_7010_postgame;
    if (!gbe::gc_message::build_dota_post_game_join_chat_channel_response_payload(
            steam_id,
            GBE_local_lobby.chat_channel_id,
            GBE_local_lobby.chat_channel_name,
            std::string(settings->get_local_name()),
            response_7010_postgame)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed building postgame 7010 payload LobbyID=%llu channel=%llu reason=%s",
            static_cast<unsigned long long>(lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.chat_channel_id),
            reason ? reason : "unknown"
        );
        return false;
    }

    auto push_reply = [&](const std::string &payload, uint32 direct_emsg, const char *label) -> bool {
        return GBE_PushDotaResponse(direct_emsg, payload, wrapped, outer_session_field_raw, label ? label : reason);
    };

    if (push_cache_unsubscribed && !push_reply(response_25, GBE_kDotaCacheUnsubscribed, "25"))
        return false;

    if (push_postgame_join && !push_reply(response_7010_postgame, GBE_kDotaJoinChatChannelResponse, "7010_postgame"))
        return false;

    GBE_pending_reset_after_cache_unsubscribed = false;
    GBE_pending_reset_after_cache_unsubscribed_lobby_id = lobby_id;
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Queued postgame teardown cache_unsub=%u postgame_join=%u; deferring reset until 7272/7014 LobbyID=%llu pre_channel=%llu post_channel=%llu reason=%s",
        push_cache_unsubscribed ? 1u : 0u,
        push_postgame_join ? 1u : 0u,
        static_cast<unsigned long long>(lobby_id),
        static_cast<unsigned long long>(pre_postgame_chat_channel_id),
        static_cast<unsigned long long>(GBE_local_lobby.chat_channel_id),
        reason ? reason : "unknown"
    );

    GBE_UpdateDotaPracticeLobbyLaunchRichPresence("#DOTA_RP_PRIVATE_LOBBY", "RUN", true, false);

    std::string no_lobby_persona;
    if (GBE_PrepareDotaPersonaStatePeripheralMessage(GBE_kDotaAbandonPersonaStatePrivateLobbyNoLobbyHex, steam_id, lobby_id, no_lobby_persona)) {
        // Rich Presence is already updated via ISteamFriends::SetRichPresence above.
        // No need to push 766 (CMsgClientPersonaState) into GC queue -- Dota ignores it.
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "built postgame persona label=private_lobby_no_lobby lobby_id=%llu size=%zu reason=%s (not queued, using SetRichPresence)",
            static_cast<unsigned long long>(lobby_id),
            no_lobby_persona.size(),
            reason ? reason : "unknown"
        );
    } else {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "failed building postgame persona label=private_lobby_no_lobby lobby_id=%llu reason=%s",
            static_cast<unsigned long long>(lobby_id),
            reason ? reason : "unknown"
        );
    }

    return true;
}

bool Steam_Game_Coordinator::GBE_SendDotaPracticeLobbyDetailsUpdate(bool wrapped, const std::string *outer_session_field_raw, const char *reason)
{
    GBE_LocalLobby lobby{};
    if (!GBE_CaptureCurrentDotaLobbyState(reason ? reason : "details_update", lobby))
        return false;

    if (GBE_ShouldSuppressDotaAbandonedLobby(lobby.lobby_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Skipped 26 details update for suppressed abandoned lobby LobbyID=%llu reason=%s state=%u game_state=%u",
            static_cast<unsigned long long>(lobby.lobby_id),
            reason ? reason : "unknown",
            lobby.state,
            lobby.game_state
        );
        return false;
    }

    const char *details_reason = reason ? reason : "";
    const bool arcade_runtime_loading_result =
        !wrapped &&
        lobby.custom_game.game_id != 0ull &&
        lobby.match_id != 0ull &&
        lobby.launch_phase >= GBE_kDotaLaunchPhaseLoaded &&
        (std::strcmp(details_reason, "8053_finished_loading") == 0 ||
            std::strcmp(details_reason, "8053_load_failed") == 0);
    const bool arcade_runtime_poll =
        !wrapped &&
        lobby.custom_game.game_id != 0ull &&
        lobby.match_id != 0ull &&
        lobby.launch_phase >= GBE_kDotaLaunchPhaseRunQueued &&
        std::strcmp(details_reason, "7034_launch_poll") == 0;
    if (arcade_runtime_loading_result || arcade_runtime_poll) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Skipped arcade runtime 26 details update LobbyID=%llu reason=%s state=%u game_state=%u launch_phase=%s",
            static_cast<unsigned long long>(lobby.lobby_id),
            reason ? reason : "unknown",
            lobby.state,
            lobby.game_state,
            GBE_DescribeDotaLaunchPhase(lobby.launch_phase)
        );
        return false;
    }

    const bool preserve_server_id =
        lobby.custom_game.game_id != 0ull &&
        lobby.match_id != 0ull &&
        (lobby.state >= 2u || lobby.game_state >= 1u);

    std::string response_26;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(lobby, GBE_GetDotaLobbyOwnerName(), response_26, preserve_server_id)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 26 details update for LobbyID=%llu reason=%s", static_cast<unsigned long long>(lobby.lobby_id), reason ? reason : "unknown");
        return false;
    }

    if (!GBE_PushDotaResponse(GBE_kDotaPracticeLobbyDetailsUpdate, response_26, wrapped, outer_session_field_raw, reason, false, 0u, 0u))
        return false;

    return true;
}

bool Steam_Game_Coordinator::GBE_PushDotaResponse(uint32 inner_emsg, const std::string &inner_message, bool wrapped, const std::string *outer_session_field_raw, const char *reason, bool apply_lobby_state, uint32 lobby_state, uint32 lobby_game_state, std::string *out_wrapped_message)
{
    gbe::dota_gc_router::DotaGcOutboundMessage outbound{};
    if (!gbe::dota_gc_router::build_outbound_message(
            inner_emsg,
            inner_message,
            wrapped,
            outer_session_field_raw,
            settings->get_local_steam_id().ConvertToUint64(),
            GBE_kEMsgClientFromGC,
            GBE_kDotaAppId,
            outbound)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed response reason=%s path=%s inner_emsg=%u wrapped=%u payload_size=%zu lobby_id=%llu state=%u game_state=%u has_session=%u",
            reason ? reason : "unknown",
            wrapped ? "wrapped" : "direct",
            inner_emsg,
            wrapped ? 1u : 0u,
            inner_message.size(),
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            outer_session_field_raw ? 1u : 0u
        );
        return false;
    }

    push_incoming_now(outbound.emsg, outbound.payload, apply_lobby_state, lobby_state, lobby_game_state);
    GBE_LogDotaResponsePacket(
        reason,
        inner_emsg,
        wrapped,
        inner_message,
        outbound.payload,
        GBE_local_lobby.lobby_id,
        apply_lobby_state ? lobby_state : GBE_local_lobby.state,
        apply_lobby_state ? lobby_game_state : GBE_local_lobby.game_state);
    if (wrapped && out_wrapped_message)
        *out_wrapped_message = outbound.payload;
    return true;
}

bool Steam_Game_Coordinator::GBE_SendDotaCustomGameLaunchSetupFlow(bool wrapped, const std::string *outer_session_field_raw, bool has_request_job, uint64 request_job_id)
{
    const gbe::dota_lobby_state::CustomGameLaunchSetupPlan launch_plan = gbe::dota_lobby_state::compose_custom_game_launch_setup_plan(
        GBE_local_lobby,
        GBE_kDotaLaunchPhaseSetupSynced);
    const gbe::dota_lobby_state::CustomGameLaunchSetupEventPlan event_plan = gbe::dota_lobby_state::compose_custom_game_launch_setup_event_plan(GBE_kDotaPracticeLobbyDetailsUpdate);
    const GBE_LocalLobby &readyup_lobby = launch_plan.readyup_lobby;
    const gbe::dota_lobby_state::LaunchDetailsEvent *readyup_event = event_plan.details_events.size() > 0 ? &event_plan.details_events[0] : nullptr;
    const gbe::dota_lobby_state::LaunchDetailsEvent *serversetup_event = event_plan.details_events.size() > 1 ? &event_plan.details_events[1] : nullptr;

    std::string readyup_26;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(readyup_lobby, GBE_local_lobby.owner_name, readyup_26, true)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building custom game READYUP 26 after 7041 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return false;
    }

    if (!readyup_event || !GBE_PushDotaResponse(readyup_event->emsg, readyup_26, wrapped, outer_session_field_raw, readyup_event->reason.c_str(), readyup_event->apply_lobby_state, readyup_event->lobby_state, readyup_event->lobby_game_state))
        return false;

    GBE_local_lobby = launch_plan.serversetup_lobby;
    GBE_PublishSharedDotaLobbyState("7041_custom_game_serversetup");

    std::string serversetup_26;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(GBE_local_lobby, GBE_local_lobby.owner_name, serversetup_26, true)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building custom game SERVERSETUP 26 after 7041 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return false;
    }

    if (!serversetup_event || !GBE_PushDotaResponse(serversetup_event->emsg, serversetup_26, wrapped, outer_session_field_raw, serversetup_event->reason.c_str(), serversetup_event->apply_lobby_state, serversetup_event->lobby_state, serversetup_event->lobby_game_state))
        return false;

    GBE_MarkDotaLaunchPhase(launch_plan.synced_launch_phase, event_plan.mark_phase_reason.c_str());
    if (event_plan.steam_auth_ack.queue)
        GBE_MaybeQueueDotaPracticeLobbySteamAuthAck(event_plan.steam_auth_ack.reason.c_str(), has_request_job ? request_job_id : 0ull);
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Sent custom game launch setup flow after 7041 path=%s has_request_job=%d request_job=%llu LobbyID=%llu match_id=%llu server_id=%llu custom_id=%llu custom_map=%s",
        wrapped ? "wrapped" : "direct",
        has_request_job ? 1 : 0,
        static_cast<unsigned long long>(request_job_id),
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id),
        static_cast<unsigned long long>(GBE_local_lobby.custom_game.game_id),
        GBE_local_lobby.custom_game.map_name.c_str()
    );
    return true;
}

bool Steam_Game_Coordinator::handle_dota_client_message(uint32 unMsgType, const void *pubData, uint32 cubData)
{
    const uint32 masked_emsg = GBE_GC_MaskedEMsg(unMsgType);
    GBE_GC_DebugLog("GC_SEND_DOTA", "outer_emsg=%u len=%u", masked_emsg, cubData);

    GBE_RestoreSharedDotaLobbyState("handle_dota_client_message");

    if (masked_emsg == GBE_kEMsgGCServerHello) {
        GBE_RestoreSharedDotaLobbyState("server_hello");

        GBE_DotaServerHelloContext server_hello_context{};
        if (!GBE_ExtractDirectDotaServerHelloContext(unMsgType, pubData, cubData, server_hello_context)) {
            GBE_GC_DebugLog("GC_SEND_DOTA", "ignored direct ServerHello payload because parsing failed");
            return false;
        }

        GBE_last_dota_server_hello_context = server_hello_context;

        if (is_server && welcome_received) {
            GBE_GC_DebugLog(
                "GC_DOTA_SERVER_HELLO",
                "skipping ServerWelcome replay because server GC is already connected active_version=%u",
                server_hello_context.active_version
            );
            return true;
        }

        if (is_server) {
            std::queue<GC_Message> queued_messages = incoming_messages;
            while (!queued_messages.empty()) {
                if (GBE_GC_MaskedEMsg(queued_messages.front().msg_type) == EGCBaseClientMsg::k_EMsgGCServerWelcome) {
                    GBE_GC_DebugLog(
                        "GC_DOTA_SERVER_HELLO",
                        "skipping ServerWelcome replay because one is already queued active_version=%u",
                        server_hello_context.active_version
                    );
                    return true;
                }
                queued_messages.pop();
            }
        }

        std::string welcome_message;
        const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
        const uint32 app_id = settings->get_local_game_id().AppID();
        if (!GBE_BuildDirectDotaServerWelcome(steam_id, app_id, server_hello_context, welcome_message)) {
            GBE_GC_DebugLog(
                "GC_SEND_DOTA",
                "failed to build ServerWelcome active_version=%u min_allowed=%u steamid=%llu",
                server_hello_context.active_version,
                server_hello_context.min_allowed_version,
                static_cast<unsigned long long>(steam_id)
            );
            return true;
        }

        GBE_GC_DebugLog(
            "GC_SEND_DOTA",
            "replaying ServerWelcome active_version=%u min_allowed=%u target_job=%llu direct=1",
            server_hello_context.active_version,
            server_hello_context.min_allowed_version,
            static_cast<unsigned long long>(server_hello_context.has_source_job ? server_hello_context.source_job_id : 0ull)
        );

        push_incoming_now(EGCBaseClientMsg::k_EMsgGCServerWelcome | GBE_kProtoMask, welcome_message);
        if (is_server && GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0) {
            GBE_GC_DebugLog(
                "GC_DOTA_SERVER_HELLO",
                "queued immediate ServerWelcome for active lobby this=%p lobby_id=%llu size=%zu",
                static_cast<void *>(this),
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                welcome_message.size()
            );
        }

        if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0) {
            if (is_server) {
                std::string runtime_cache_message;
                const uint64 owner_steam_id = GBE_GetDotaLobbyOwnerSteamId();
                const uint32 owner_account_id = GBE_GetDotaLobbyOwnerAccountId();
                const bool launch_started = GBE_local_lobby.match_id != 0;
                const bool built_runtime_cache = GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed(
                    GBE_local_lobby,
                    GBE_local_lobby.owner_name,
                    runtime_cache_message,
                    true);

                if (built_runtime_cache) {
                    GBE_RecordDotaLobbyCacheSubscriptionState(runtime_cache_message, "server_welcome_current_cache_subscribed");
                    push_incoming_now(GBE_kDotaCacheSubscribed | GBE_kProtoMask, runtime_cache_message);
                    GBE_GC_DebugLog(
                        "GC_DOTA_SERVER_HELLO",
                        "queued synthetic CacheSubscribed after ServerWelcome lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu launch_started=%u owner_steam_id=%llu owner_account_id=%u size=%zu",
                        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                        GBE_local_lobby.state,
                        GBE_local_lobby.game_state,
                        static_cast<unsigned long long>(GBE_local_lobby.match_id),
                        static_cast<unsigned long long>(GBE_local_lobby.server_id),
                        launch_started ? 1u : 0u,
                        static_cast<unsigned long long>(owner_steam_id),
                        owner_account_id,
                        runtime_cache_message.size()
                    );
                } else {
                    GBE_GC_DebugLog(
                        "GC_DOTA_SERVER_HELLO",
                        "failed building synthetic CacheSubscribed after ServerWelcome lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu",
                        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                        GBE_local_lobby.state,
                        GBE_local_lobby.game_state,
                        static_cast<unsigned long long>(GBE_local_lobby.match_id),
                        static_cast<unsigned long long>(GBE_local_lobby.server_id)
                    );
                }

                GBE_GC_DebugLog(
                    "GC_DOTA_SERVER_HELLO",
                    "skipping synthetic direct 7034 after ServerWelcome to match official launch timing lobby_id=%llu state=%u game_state=%u team=%u slot=%u",
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state,
                    GBE_local_lobby.owner_team,
                    GBE_local_lobby.owner_slot
                );
            }
        }

        return true;
    }

    if (masked_emsg != GBE_kEMsgGCClientHello && masked_emsg != GBE_kEMsgClientToGC)
        return GBE_HandleDotaDirectPostLoginRequest(unMsgType, pubData, cubData);

    GBE_DotaHelloContext hello_context{};
    bool direct_message = false;
    if (masked_emsg == GBE_kEMsgGCClientHello) {
        direct_message = true;
        if (!GBE_ExtractDirectDotaHelloContext(unMsgType, pubData, cubData, hello_context)) {
            GBE_GC_DebugLog("GC_SEND_DOTA", "ignored direct ClientHello payload because parsing failed");
            return false;
        }
    } else if (masked_emsg == GBE_kEMsgClientToGC) {
        if (!GBE_ExtractDotaHelloContext(pubData, cubData, hello_context)) {
            if (GBE_HandleDotaWrappedPostLoginRequest(pubData, cubData))
                return true;

            GBE_GC_DebugLog("GC_SEND_DOTA", "ignored ClientToGC payload because it was not a valid Dota ClientHello or supported wrapped request");
            return false;
        }
    } else {
        GBE_GC_DebugLog("GC_SEND_DOTA", "ignored non-Dota-GC message emsg=%u", masked_emsg);
        return false;
    }

    std::string welcome_message;
    const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    const uint32 account_id = settings->get_local_steam_id().GetAccountID();
    const uint32 app_id = settings->get_local_game_id().AppID();

    const bool built = direct_message
        ? GBE_BuildDirectDotaClientWelcome(steam_id, app_id, account_id, hello_context, welcome_message)
        : GBE_ComposeDotaClientWelcome(steam_id, app_id, account_id, hello_context, welcome_message);

    if (!built) {
        GBE_GC_DebugLog(
            "GC_SEND_DOTA",
            "failed to build ClientWelcome version=%u steamid=%llu accountid=%u",
            hello_context.version,
            static_cast<unsigned long long>(steam_id),
            account_id
        );
        return true;
    }

    GBE_GC_DebugLog(
        "GC_SEND_DOTA",
        "replaying ClientWelcome version=%u steamid=%llu accountid=%u target_job=%llu direct=%d",
        hello_context.version,
        static_cast<unsigned long long>(steam_id),
        account_id,
        static_cast<unsigned long long>(hello_context.has_source_job ? hello_context.source_job_id : 0ull),
        direct_message ? 1 : 0
    );

    push_incoming_now((direct_message ? GBE_kEMsgGCClientWelcome : GBE_kEMsgClientFromGC) | GBE_kProtoMask, welcome_message);

    std::string top_custom_games_message;
    size_t top_custom_games_count = 0;
    if (GBE_AdaptDotaTopCustomGamesListPayload(settings, top_custom_games_message, top_custom_games_count)) {
        GBE_PushDotaResponse(GBE_kDotaTopCustomGamesList, top_custom_games_message, false, nullptr, "top_custom_games_after_welcome");
        GBE_GC_DebugLog(
            "GC_DOTA_CUSTOM_GAMES",
            "queued top custom games list count=%zu direct=%d",
            top_custom_games_count,
            direct_message ? 1 : 0);
    }

    if (direct_message)
        GBE_PushDotaLoginSyncMessages();
    return true;
}

// sends a message to the Game Coordinator
EGCResults Steam_Game_Coordinator::SendMessage_( uint32 unMsgType, const void *pubData, uint32 cubData )
{
    PRINT_DEBUG("0x%08X %u len %u", unMsgType, (~protobuf_mask) & unMsgType, cubData);
    GBE_GC_DebugLog("GC_SEND", "outer_emsg=%u len=%u", GBE_GC_MaskedEMsg(unMsgType), cubData);
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    if ((unMsgType & protobuf_mask) != 0) {
        GBE_LogGCProtoBoundary(
            "GC_SEND_HEADER",
            "send",
            static_cast<void *>(this),
            is_server,
            GBE_GC_MaskedEMsg(unMsgType),
            pubData,
            cubData
        );
    }

    if (!gc_initialized && gc_profile == GC_PROFILE_DOTA2) {
        GBE_GC_DebugLog("GC_SEND", "initializing GC lazily for Dota2 profile");
        initialize_gc();
    }

    if (!gc_initialized) {
        GBE_GC_DebugLog("GC_SEND", "gc not initialized, swallowing msg=%u", GBE_GC_MaskedEMsg(unMsgType));
        return k_EGCResultOK;
    }

    if (gc_profile == GC_PROFILE_DOTA2 && handle_dota_client_message(unMsgType, pubData, cubData)) {
        GBE_GC_DebugLog("GC_SEND", "handled by Dota2 replay path msg=%u", GBE_GC_MaskedEMsg(unMsgType));
        return k_EGCResultOK;
    }

    switch (unMsgType) {
        case EGCItemMsg::k_EMsgGCSetSingleItemPosition:
            PRINT_DEBUG("k_EMsgGCSetSingleItemPosition");
            handle_set_item_pos(pubData, cubData);
            break;
        case EGCItemMsg::k_EMsgGCDelete:
            PRINT_DEBUG("k_EMsgGCDelete");
            handle_delete_item(pubData, cubData);
            break;
        case EGCItemMsg::k_EMsgGCMOTDRequest:
            PRINT_DEBUG("k_EMsgGCMOTDRequest");
            handle_motd_request(pubData, cubData);
            break;
        case EGCItemMsg::k_EMsgGCRespawnPostLoadoutChange:
            PRINT_DEBUG("k_EMsgGCRespawnPostLoadoutChange");
            handle_respawn(pubData, cubData);
            break;
        case EGCItemMsg::k_EMsgGCSetItemStyle:
            PRINT_DEBUG("k_EMsgGCSetItemStyle");
            handle_set_item_style(pubData, cubData);
            break;
        case EGCItemMsg::k_EMsgGCAdjustItemEquippedState | protobuf_mask:
            PRINT_DEBUG("k_EMsgGCAdjustItemEquippedState");
            handle_adjust_equip_state(pubData, cubData);
            break;
        case EGCItemMsg::k_EMsgGCSetItemPositions | protobuf_mask:
            PRINT_DEBUG("k_EMsgGCSetItemPositions");
            handle_set_multiple_item_pos(pubData, cubData);
            break;
        default:
            break;
    }

    return k_EGCResultOK;
}

// returns true if there is a message waiting from the game coordinator
bool Steam_Game_Coordinator::IsMessageAvailable( uint32 *pcubMsgSize )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    if (!gc_initialized || incoming_messages.empty()) {
        GBE_GC_DebugLog(
            "GC_CALLBACK",
            "IsMessageAvailable this=%p gc_initialized=%u queue_size=%zu returning=0",
            static_cast<void *>(this),
            gc_initialized ? 1u : 0u,
            incoming_messages.size()
        );
        *pcubMsgSize = 0;
        return false;
    }

    GC_Message &message = incoming_messages.front();
    *pcubMsgSize = static_cast<uint32>(message.msg_body.size());

    GBE_GC_DebugLog(
        "GC_CALLBACK",
        "IsMessageAvailable this=%p gc_initialized=%u queue_size=%zu front_emsg=%u size=%u returning=1",
        static_cast<void *>(this),
        gc_initialized ? 1u : 0u,
        incoming_messages.size(),
        GBE_GC_MaskedEMsg(message.msg_type),
        *pcubMsgSize
    );

    return true;
}

// fills the provided buffer with the first message in the queue and returns k_EGCResultOK or 
// returns k_EGCResultNoMessage if there is no message waiting. pcubMsgSize is filled with the message size.
// If the provided buffer is not large enough to fit the entire message, k_EGCResultBufferTooSmall is returned
// and the message remains at the head of the queue.
EGCResults Steam_Game_Coordinator::RetrieveMessage( uint32 *punMsgType, void *pubDest, uint32 cubDest, uint32 *pcubMsgSize )
{
    PRINT_DEBUG_ENTRY();
    std::lock_guard<std::recursive_mutex> lock(global_mutex);

    const uint32 queued_emsg = incoming_messages.empty() ? 0u : GBE_GC_MaskedEMsg(incoming_messages.front().msg_type);
    GBE_GC_DebugLog(
        "GC_RETRIEVE",
        "queued_emsg=%u this=%p queue_size=%zu pending_size=%zu cubDest=%u",
        queued_emsg,
        static_cast<void *>(this),
        incoming_messages.size(),
        pending_messages.size(),
        cubDest
    );

    if (!gc_initialized || incoming_messages.empty()) {
        *pcubMsgSize = 0;
        return k_EGCResultNoMessage;
    }

    GC_Message &message = incoming_messages.front();

    uint32 outsize = static_cast<uint32>(message.msg_body.size());
    if (outsize > cubDest) {
        return k_EGCResultBufferTooSmall;
    }

    if (is_welcome_message(message)) {
        welcome_received = true;
        welcome_time = std::chrono::high_resolution_clock::now();
    }

    *punMsgType = message.msg_type;
    *pcubMsgSize = outsize;
    message.msg_body.copy(reinterpret_cast<char *>(pubDest), cubDest);

    const bool should_finalize_dota_abandon_after_7014 =
        gc_profile == GC_PROFILE_DOTA2 &&
        GBE_pending_dota_abandon_finalize_after_7014 &&
        GBE_GC_MaskedEMsg(*punMsgType) == GBE_kDotaOtherLeftChannel &&
        GBE_IsDotaOtherLeftChannelPayloadForChannel(message.msg_body, GBE_local_lobby.abandon_pre_postgame_chat_channel_id);
    const bool should_finalize_dota_normal_signout_after_25 =
        gc_profile == GC_PROFILE_DOTA2 &&
        GBE_pending_dota_normal_signout_finalize_after_25 &&
        GBE_GC_MaskedEMsg(*punMsgType) == GBE_kDotaCacheUnsubscribed;

    incoming_messages.pop();

    if ((message.msg_type & protobuf_mask) != 0) {
        GBE_LogGCProtoBoundary(
            "GC_RETRIEVE_HEADER",
            "recv",
            static_cast<void *>(this),
            is_server,
            GBE_GC_MaskedEMsg(message.msg_type),
            message.msg_body.data(),
            outsize
        );
    }

    if (!incoming_messages.empty()) {
        GCMessageAvailable_t data{};
        data.m_nMessageSize = static_cast<uint32>(incoming_messages.front().msg_body.size());
        callbacks->addCBResult(data.k_iCallback, &data, sizeof(data), 0.0);
        GBE_GC_DebugLog(
            "GC_CALLBACK",
            "reposted GCMessageAvailable_t after retrieving msg=%u this=%p next_emsg=%u remaining_queue=%zu pending_size=%zu size=%u",
            GBE_GC_MaskedEMsg(*punMsgType),
            static_cast<void *>(this),
            GBE_GC_MaskedEMsg(incoming_messages.front().msg_type),
            incoming_messages.size(),
            pending_messages.size(),
            data.m_nMessageSize
        );
    }

    if (should_finalize_dota_abandon_after_7014) {
        const uint64 finalize_lobby_id = GBE_pending_dota_abandon_finalize_lobby_id;
        GBE_pending_dota_abandon_finalize_after_7014 = false;
        GBE_pending_dota_abandon_finalize_lobby_id = 0;
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Consumed pending 7014; finalizing abandon teardown LobbyID=%llu channel=%llu",
            static_cast<unsigned long long>(finalize_lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.abandon_pre_postgame_chat_channel_id)
        );
        GBE_FinalizeDotaAbandonAfterOtherLeftChannel(finalize_lobby_id, "7014_pre_postgame_retrieved");
    }

    if (should_finalize_dota_normal_signout_after_25) {
        const uint64 finalize_lobby_id = GBE_pending_dota_normal_signout_finalize_lobby_id;
        GBE_pending_dota_normal_signout_finalize_after_25 = false;
        GBE_pending_dota_normal_signout_finalize_lobby_id = 0;
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Consumed normal signout 25; finalizing postgame teardown LobbyID=%llu",
            static_cast<unsigned long long>(finalize_lobby_id)
        );
        GBE_FinalizeDotaNormalSignoutAfterCacheUnsubscribed(finalize_lobby_id, "7004_signout_after_25_retrieved");
    }

    if (gc_profile == GC_PROFILE_DOTA2 &&
        GBE_pending_reset_after_cache_unsubscribed &&
        GBE_GC_MaskedEMsg(*punMsgType) == GBE_kDotaCacheUnsubscribed) {
        const uint64 pending_lobby_id = GBE_pending_reset_after_cache_unsubscribed_lobby_id;
        GBE_pending_reset_after_cache_unsubscribed = false;
        GBE_pending_reset_after_cache_unsubscribed_lobby_id = 0;
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Consumed pending 25; applying deferred current-game reset for LobbyID=%llu",
            static_cast<unsigned long long>(pending_lobby_id)
        );
        ResetGCMemory("7035_disconnect_current_game_after_25", true, true);
    }

    GBE_GC_DebugLog(
        "GC_RETRIEVE",
        "returned_emsg=%u this=%p size=%u remaining_queue=%zu pending_size=%zu",
        GBE_GC_MaskedEMsg(*punMsgType),
        static_cast<void *>(this),
        outsize,
        incoming_messages.size(),
        pending_messages.size()
    );

    return k_EGCResultOK;
}

// server requested our inventory
void Steam_Game_Coordinator::network_callback_inventory_request(Common_Message *msg)
{
    // Server instance should never receive this.
    if (is_server)
        return;

    uint64 server_steamid = msg->source_id();

    if (!msg->gameserver_items_messages().has_inventory_request()) {
        PRINT_DEBUG("error empty msg");
        return;
    }

    bool is_gc = msg->gameserver_items_messages().is_gc();
    const auto &request_msg = msg->gameserver_items_messages().inventory_request();
    auto response_msg = new GameServer_Items_Messages::InventoryResponse();
    response_msg->set_steam_api_call(request_msg.steam_api_call());

    for (const Econ_Item &item : items) {
        auto new_item = response_msg->add_items();
        new_item->set_id(item.id);
        new_item->set_def(item.def);
        new_item->set_level(item.level);
        new_item->set_quality(static_cast<int32>(item.quality));
        new_item->set_inv_pos(item.inv_pos);
        new_item->set_quantity(item.quantity);
        new_item->set_flags(item.flags);
        new_item->set_origin(item.origin);
        new_item->set_custom_name(item.custom_name);
        new_item->set_custom_desc(item.custom_desc);
        new_item->set_original_id(item.original_id);
        new_item->set_in_use(item.in_use);
        new_item->set_style(item.style);

        for (const auto &[class_id, slot_id] : item.equip_states) {
            auto new_state = new_item->add_equip_states();
            new_state->set_class_id(class_id);
            new_state->set_slot_id(slot_id);
        }

        for (const Econ_Item_Attribute &attr : item.attributes) {
            auto new_attr = new_item->add_attributes();
            new_attr->set_def(attr.def);
            new_attr->set_value(attr.value);
            new_attr->set_value_bytes(attr.value_bytes);
        }
    }

    auto gameserver_items_msg = new GameServer_Items_Messages();
    gameserver_items_msg->set_type(GameServer_Items_Messages::Response_Inventory);
    gameserver_items_msg->set_is_gc(is_gc);
    gameserver_items_msg->set_allocated_inventory_response(response_msg);

    Common_Message new_msg{};
    new_msg.set_allocated_gameserver_items_messages(gameserver_items_msg);
    new_msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
    new_msg.set_dest_id(server_steamid);
    network->sendTo(&new_msg, true);

    PRINT_DEBUG("server requested inventory, sent %u items", static_cast<uint32>(items.size()));
}

// user sent their inventory
void Steam_Game_Coordinator::network_callback_inventory_response(Common_Message *msg)
{
    uint64 user_steamid = msg->source_id();

    PRINT_DEBUG("player sent their inventory %llu", user_steamid);
    if (!msg->gameserver_items_messages().has_inventory_response()) {
        PRINT_DEBUG("error empty msg");
        return;
    }

    bool is_gc = msg->gameserver_items_messages().is_gc();
    const auto &response_msg = msg->gameserver_items_messages().inventory_response();
    SteamAPICall_t api_call = response_msg.steam_api_call();

    // For unsolicited pushes (api_call == 0, e.g. from Dota 2 equip broadcast),
    // skip the pending request check.
    bool is_unsolicited = (api_call == 0);

    if (!is_unsolicited) {
        // Find this pending request.
        auto it = std::find_if(
            pending_items_requests.begin(), pending_items_requests.end(),
            [=](const RequestInventory &item) {
                return item.steam_api_call == response_msg.steam_api_call() &&
                    item.steam_id == user_steamid;
            }
        );
        if (pending_items_requests.end() == it) {
            PRINT_DEBUG("error got player inventory but pending request timedout/removed (doesn't exist)");
            return;
        }
        pending_items_requests.erase(it);
    }

    auto &items = all_user_items[user_steamid];
    items.clear();

    for (const auto &item : response_msg.items()) {
        Econ_Item new_item{};
        new_item.id = item.id();
        new_item.def = item.def();
        new_item.level = item.level();
        new_item.quality = static_cast<EItemQuality>(item.quality());
        new_item.inv_pos = item.inv_pos();
        new_item.quantity = item.quantity();
        new_item.flags = item.flags();
        new_item.origin = item.origin();
        new_item.custom_name = item.custom_name();
        new_item.custom_desc = item.custom_desc();
        new_item.original_id = item.original_id();
        new_item.in_use = item.in_use();
        new_item.style = item.style();
        if (new_item.id == 0)
            continue;

        for (const auto &state : item.equip_states()) {
            new_item.equip_states.insert({ state.class_id(), state.slot_id() });
        }

        for (const auto &attr : item.attributes()) {
            Econ_Item_Attribute new_attr{};
            new_attr.def = attr.def();
            new_attr.value = attr.value();
            new_attr.value_bytes = attr.value_bytes();
            if (new_attr.def == 0)
                continue;

            new_item.attributes.push_back(new_attr);
        }

        // Check custom name and custom description limits.
        if (!check_econ_item_name(new_item.custom_name)) {
            new_item.custom_name.clear();
        }

        if (!check_econ_item_desc(new_item.custom_desc)) {
            new_item.custom_desc.clear();
        }

        items.push_back(new_item);
    }

    if (is_gc) {
        callback_items_received(user_steamid, items);
    } else {
        server_items()->callback_items_received(user_steamid, items.size(), api_call, true);
    }

    PRINT_DEBUG("got player inventory: %u items", response_msg.items_size());

    // [Dota 2 LAN] When the server GC receives a remote player's inventory,
    // build and inject a player item CacheSubscribed so the dedicated server
    // knows that player's equipped cosmetics for wearable loading.
    if (is_server && gc_profile == GC_PROFILE_DOTA2 && GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0) {
        std::vector<const Econ_Item *> equipped_items;
        for (const Econ_Item &item : items) {
            if (!item.equip_states.empty())
                equipped_items.push_back(&item);
        }

        if (!equipped_items.empty()) {
            const CSteamID player_steam_id(user_steamid);

            // For the lobby owner (host), push CacheUnsubscribed first to
            // force the engine to re-evaluate equipped items and spawn
            // wearables for an already-spawned hero.
            const bool is_owner = (user_steamid == GBE_GetDotaLobbyOwnerSteamId());
            // Push to server GC so remote players see cosmetics
            GBE_PushDotaPlayerEquippedItemsCacheToGC(this, player_steam_id, items, is_owner, is_owner ? "inventory_response_owner_resubscribe_server" : "inventory_response_remote_subscribe");

            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "pushed %s player item CacheSubscribed for server after inventory response: steam64=%llu equipped_items=%zu total_items=%zu unsub_first=%d",
                is_owner ? "owner" : "remote",
                static_cast<unsigned long long>(user_steamid),
                equipped_items.size(),
                items.size(),
                is_owner ? 1 : 0
            );
        }
    }
}

// user updated an item
void Steam_Game_Coordinator::network_callback_item_update(Common_Message *msg)
{
    uint64 user_steamid = msg->source_id();

    PRINT_DEBUG("player updated an item %llu", user_steamid);
    if (!msg->gameserver_items_messages().has_item_update()) {
        PRINT_DEBUG("error empty msg");
        return;
    }

    if (!all_user_items.count(user_steamid)) {
        PRINT_DEBUG("error no inventory for player", user_steamid);
        return;
    }

    bool is_gc = msg->gameserver_items_messages().is_gc();
    const auto &inventory_msg = msg->gameserver_items_messages().item_update();
    uint64 item_id = inventory_msg.id();

    auto &items = all_user_items.at(user_steamid);

    for (Econ_Item &item : items) {
        if (item.id != item_id)
            continue;

        if (inventory_msg.has_inv_pos()) {
            item.inv_pos = inventory_msg.inv_pos();
            PRINT_DEBUG("got updated item inventory pos: %llu 0x%08X", item_id, inventory_msg.inv_pos());
        }

        if (inventory_msg.has_style()) {
            item.style = inventory_msg.style();
            PRINT_DEBUG("got updated item style: %llu %u", item_id, inventory_msg.style());
        }

        if (inventory_msg.has_equip_states()) {
            item.equip_states.clear();
            for (const auto &state : inventory_msg.equip_states()) {
                item.equip_states.insert({ state.class_id(), state.slot_id() });
            }
            PRINT_DEBUG("got updated item equip states: %llu", item_id);
        }

        if (is_gc) {
            callback_item_updated(user_steamid, item);
        } else if (inventory_msg.has_inv_pos()) {
            server_items()->callback_item_pos_updated(user_steamid, item_id, inventory_msg.inv_pos());
        }

        return;
    }

    PRINT_DEBUG("error item %llu not found", item_id);
}

// user deleted an item
void Steam_Game_Coordinator::network_callback_item_deletion(Common_Message *msg)
{
    uint64 user_steamid = msg->source_id();

    PRINT_DEBUG("player deleted inventory item %llu", user_steamid);
    if (!msg->gameserver_items_messages().has_item_deletion()) {
        PRINT_DEBUG("error empty msg");
        return;
    }

    if (!all_user_items.count(user_steamid)) {
        PRINT_DEBUG("error no inventory for player", user_steamid);
        return;
    }

    bool is_gc = msg->gameserver_items_messages().is_gc();
    const auto &delete_msg = msg->gameserver_items_messages().item_deletion();
    uint64 item_id = delete_msg.item_id();

    auto &items = all_user_items.at(user_steamid);

    for (auto it = items.begin(); it != items.end(); it++) {
        if (it->id != item_id)
            continue;

        items.erase(it);

        if (is_gc) {
            callback_item_deleted(user_steamid, item_id);
        } else {
            server_items()->callback_item_deleted(user_steamid, item_id);
        }

        PRINT_DEBUG("deleted player's inventory item: %llu", item_id);
        return;
    }

    PRINT_DEBUG("error item %llu not found", item_id);
}

// user wants to respawn after loadout change
void Steam_Game_Coordinator::network_callback_respawn_request(Common_Message *msg)
{
    if (!is_server)
        return;

    uint64 user_steamid = msg->source_id();
    if (!all_user_items.count(user_steamid))
        return;

    callback_respawn_request(user_steamid);
}

// only triggered when we have a message
void Steam_Game_Coordinator::network_callback(Common_Message *msg)
{
    if (msg->source_id() == settings->get_local_steam_id().ConvertToUint64()) return;

    if (msg->has_gameserver_items_messages()) {
        switch (msg->gameserver_items_messages().type()) {
        // server requested our inventory
        case GameServer_Items_Messages::Request_Inventory:
            network_callback_inventory_request(msg);
        break;

        // user sent their inventory
        case GameServer_Items_Messages::Response_Inventory:
            network_callback_inventory_response(msg);
        break;

        // user updated an item
        case GameServer_Items_Messages::Request_UpdateItem:
            network_callback_item_update(msg);
        break;

        // user deleted an item
        case GameServer_Items_Messages::Request_DeleteItem:
            network_callback_item_deletion(msg);
        break;

        // user wants to respawn after loadout change
        case GameServer_Items_Messages::Request_Respawn:
            network_callback_respawn_request(msg);
        break;

        default:
            PRINT_DEBUG("unhandled type %i", (int)msg->gameserver_items_messages().type());
        break;
        }
    } else if (msg->has_friend_messages()) {
        if (GBE_HandleDotaFriendLobbyInviteMessage(msg))
            return;
    } else if (msg->has_steam_messages()) {
        if (GBE_HandleDotaNetworkLobbyInviteMessage(msg))
            return;
        if (GBE_HandleDotaNetworkChatMessage(msg))
            return;
    } else if (msg->has_low_level()) {
        if (!is_server && gc_initialized) {
            CSteamID user_steamid;
            user_steamid.SetFromUint64(msg->source_id());

            if (user_steamid.BIndividualAccount()) {
                // Client needs to know other players' inventories as well since the game uses them to
                // validate cosmetic items.
                switch (msg->low_level().type()) {
                case Low_Level::CONNECT:
                    request_user_items(user_steamid, generate_steam_api_call_id(), true);
                break;

                case Low_Level::DISCONNECT:
                    remove_user_items(user_steamid);
                    if (gc_profile == GC_PROFILE_DOTA2) {
                        Steam_Client *steam_client = get_steam_client();
                        if (steam_client && steam_client->steam_matchmaking)
                            steam_client->steam_matchmaking->RefreshLobbyCallbacksForDota();
                        if (GBE_MaybeHandleDotaPracticeLobbyKicked("network_low_level_disconnect"))
                            break;
                        GBE_MaybeNotifyDotaPracticeLobbyMembersChanged("network_low_level_disconnect");
                    }
                break;
                }
            }
        }
    }
}

void Steam_Game_Coordinator::RunCallbacks()
{
    if (!gc_initialized)
        return;

    if (delay_init && welcome_received && check_timedout(welcome_time, 0.2)) {
        delay_init = false;
    }

    // [Dota 2 LAN] Throttle lobby state polling during an active LAN match to
    // reduce mutex contention with the engine's networking thread.  Lobby state
    // changes (player disconnect, game_state advance, PostGame transition) are
    // second-granularity events; 500ms latency is imperceptible.
    {
        bool skip_lobby_poll = false;
        if (!is_server && gc_profile == GC_PROFILE_DOTA2 &&
            GBE_local_lobby.active && GBE_local_lobby.lan &&
            GBE_local_lobby.state == 2u && GBE_local_lobby.match_id != 0ull) {
            if (!check_timedout(GBE_last_lobby_poll_time, 0.5))
                skip_lobby_poll = true;
            else
                GBE_last_lobby_poll_time = std::chrono::high_resolution_clock::now();
        }
        if (!skip_lobby_poll) {
            if (!GBE_MaybeHandleDotaPracticeLobbyKicked("run_callbacks_generic_lobby_members_changed"))
                GBE_MaybeNotifyDotaPracticeLobbyMembersChanged("run_callbacks_generic_lobby_members_changed");
        }
    }

    // [Dota 2 LAN] Once per match, when the client detects an active lobby with
    // a server, broadcast equipped items to the gameserver so it can build
    // CacheSubscribed for this player.  This handles items equipped in the armory
    // before the game starts (when no gameserver existed yet to receive them).
    if (!is_server && gc_profile == GC_PROFILE_DOTA2 &&
        GBE_local_lobby.active && GBE_local_lobby.server_id != 0 && GBE_local_lobby.state >= 1u) {
        static uint64 s_last_broadcast_match_id = 0;
        if (s_last_broadcast_match_id != GBE_local_lobby.match_id && GBE_local_lobby.match_id != 0) {
            s_last_broadcast_match_id = GBE_local_lobby.match_id;

            std::vector<const Econ_Item *> equipped_items;
            for (const auto &item : items) {
                if (!item.equip_states.empty())
                    equipped_items.push_back(&item);
            }

            if (!equipped_items.empty()) {
                auto response_msg = new GameServer_Items_Messages::InventoryResponse();
                response_msg->set_steam_api_call(0);  // unsolicited push

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
                    "broadcast initial equipped items to gameservers: steam64=%llu equipped_items=%zu match_id=%llu",
                    static_cast<unsigned long long>(settings->get_local_steam_id().ConvertToUint64()),
                    equipped_items.size(),
                    static_cast<unsigned long long>(GBE_local_lobby.match_id)
                );
            }
        }
    }

    auto due_time = [](const GC_Message &message) {
        return message.created + std::chrono::duration_cast<std::chrono::high_resolution_clock::duration>(std::chrono::duration<double>(message.post_in));
    };

    std::stable_sort(
        pending_messages.begin(),
        pending_messages.end(),
        [&due_time](const GC_Message &lhs, const GC_Message &rhs) {
            const auto lhs_due = due_time(lhs);
            const auto rhs_due = due_time(rhs);
            if (lhs_due != rhs_due)
                return lhs_due < rhs_due;
            return lhs.sequence < rhs.sequence;
        }
    );

    for (auto it = pending_messages.begin(); it != pending_messages.end();) {
        if (delay_init && !is_welcome_message(*it)) {
            it++;
            continue;
        }

        if (check_timedout(it->created, it->post_in)) {
            GBE_ApplyQueuedLobbyState(*it);
            incoming_messages.push(*it);

            GCMessageAvailable_t data{};
            data.m_nMessageSize = static_cast<uint32>(it->msg_body.size());
            callbacks->addCBResult(data.k_iCallback, &data, sizeof(data), 0.0);

            GBE_GC_DebugLog(
                "GC_CALLBACK",
                "moved pending msg=%u this=%p to incoming queue_size=%zu pending_size_before_erase=%zu size=%u and posted GCMessageAvailable_t",
                GBE_GC_MaskedEMsg(it->msg_type),
                static_cast<void *>(this),
                incoming_messages.size(),
                pending_messages.size(),
                data.m_nMessageSize
            );

            it = pending_messages.erase(it);
        } else {
            it++;
        }
    }

    for (auto it = pending_items_requests.begin(); it != pending_items_requests.end();) {
        if (check_timedout(it->created, 7.0)) {
            if (!it->is_gc) {
                server_items()->callback_items_received(it->steam_id, items.size(), it->steam_api_call, false);
            }

            PRINT_DEBUG("player inventory request timeout %llu", it->steam_id);
            it = pending_items_requests.erase(it);
        } else {
            it++;
        }
    }
}
