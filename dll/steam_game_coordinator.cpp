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
#include <algorithm>
#include <array>
#include <cctype>
#include <cstdarg>
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
static constexpr uint32 GBE_kProtoMask = 0x80000000u;
static constexpr uint32 GBE_kEMsgClientToGC = 5452u;
static constexpr uint32 GBE_kEMsgClientFromGC = 5453u;
static constexpr uint32 GBE_kEMsgGCClientHello = 4006u;
static constexpr uint32 GBE_kEMsgGCServerHello = 4007u;
static constexpr uint32 GBE_kEMsgGCClientWelcome = 4004u;
static constexpr uint32 GBE_kDotaAppId = 570u;
static constexpr uint32 GBE_kDotaCacheSubscribed = 24u;
static constexpr uint32 GBE_kDotaCacheUnsubscribed = 25u;
static constexpr uint32 GBE_kDotaPracticeLobbyDetailsUpdate = 26u;
static constexpr uint32 GBE_kDotaJoinChatChannel = 7009u;
static constexpr uint32 GBE_kDotaJoinChatChannelResponse = 7010u;
static constexpr uint32 GBE_kDotaOtherLeftChannel = 7014u;
static constexpr uint32 GBE_kDotaLobbyAdditionalAccountData = 8869u;
static constexpr uint32 GBE_kDotaPracticeLobbyCreate = 7038u;
static constexpr uint32 GBE_kDotaPracticeLobbyLeave = 7040u;
static constexpr uint32 GBE_kDotaPracticeLobbyLaunch = 7041u;
static constexpr uint32 GBE_kDotaPracticeLobbySetDetails = 7046u;
static constexpr uint32 GBE_kDotaPracticeLobbySetTeamSlot = 7047u;
static constexpr uint32 GBE_kDotaPracticeLobbyResponse = 7055u;
static constexpr uint32 GBE_kDotaLeaveChatChannel = 7272u;
static constexpr uint32 GBE_kDotaPracticeLobbyJoinBroadcastChannel = 7149u;
static constexpr uint32 GBE_kDotaLobbyUpdateBroadcastChannelInfo = 7367u;
static constexpr uint32 GBE_kDotaDestroyLobbyRequest = 8246u;
static constexpr uint32 GBE_kDotaDestroyLobbyResponse = 8247u;
static constexpr uint32 GBE_kDotaPracticeLobbyCloseBroadcastChannel = 8054u;
static constexpr uint32 GBE_kDotaSOUpdateMultiple = 6146u;
static constexpr size_t GBE_kDotaWelcomeInnerBodyOffset = 48u;
static constexpr const char *GBE_kGcDebugLogPath = "C:\\Users\\Public\\gbe_gc_debug.log";
static constexpr uint64 GBE_kDotaLobbyDetailsTimestamp = 0x0069E7F5C567E78Bull;
static constexpr uint32 GBE_kDotaLobbyField128Value = 1776809986u;

struct GBE_SharedDotaLobbyState {
    bool valid{};
    bool active{};
    uint64 lobby_id{};
    bool has_chat_channel{};
    uint64 chat_channel_id{};
    std::string chat_channel_name;
    uint32 chat_channel_type{};
    std::string room_name;
    uint32 game_mode{};
    uint32 server_region{};
    bool lan{};
    std::string lan_host_ping_location;
    bool allow_cheats{};
    bool fill_with_bots{};
    bool allow_spectating{};
    uint32 visibility{};
    uint32 bot_difficulty_radiant{};
    uint32 bot_difficulty_dire{};
    uint64 bot_radiant{};
    uint64 bot_dire{};
    uint32 state{};
    uint32 game_state{};
    uint64 match_id{};
    uint64 server_id{};
    uint64 owner_steam_id{};
    uint32 owner_account_id{};
    std::string owner_name;
    std::string connect;
    uint32 game_start_time{};
    uint32 owner_team{};
    uint32 owner_slot{};
    bool has_broadcast_channel{};
    uint32 broadcast_channel_id{};
    std::string broadcast_country_code;
    std::string broadcast_description;
    std::string broadcast_language_code;
    std::string pass_key;
};

static GBE_SharedDotaLobbyState GBE_shared_dota_lobby_state;

static void GBE_GC_DebugLog(const char *scope, const char *fmt, ...);

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
static const std::array<uint8, 4> GBE_kOldDotaAccountIdVarint = { 0xF5, 0xED, 0x86, 0x41 };
static const std::array<uint8, 9> GBE_kOldDotaSteamIdVarint = { 0xF5, 0xED, 0x86, 0xC1, 0x90, 0x80, 0x80, 0x88, 0x01 };
static const std::array<uint8, 8> GBE_kOldDotaLobbyIdVarint = { 0x9D, 0x97, 0xF8, 0x9E, 0x95, 0xD7, 0xF7, 0x34 };
static const std::array<uint8, 8> GBE_kOldDotaSteamIdFixed64 = { 0xF5, 0xB6, 0x21, 0x08, 0x01, 0x00, 0x10, 0x01 };
static const std::array<uint8, 4> GBE_kOldDotaAccountIdFixed32 = { 0xF5, 0xB6, 0x21, 0x08 };
static const std::array<uint8, 8> GBE_kOldDotaPracticeLobbyLobbyIdVarint = { 0x83, 0xCF, 0xA2, 0xB4, 0xA2, 0xFF, 0xF9, 0x34 };
static const std::array<uint8, 5> GBE_kOldDotaPracticeLobbyMatchIdVarint = { 0xDF, 0xF8, 0xBB, 0xDB, 0x20 };
static const std::array<uint8, 8> GBE_kOldDotaPracticeLobbyServerIdFixed64 = { 0x01, 0x7C, 0x58, 0xCA, 0x8F, 0xC1, 0x40, 0x01 };
static const std::array<uint8, 5> GBE_kOldDotaPracticeLobbyGameStartTimeVarint = { 0xAE, 0xBB, 0xA3, 0xCF, 0x06 };
static constexpr const char *GBE_kOldDotaPracticeLobbyConnect = "117.157.79.194:27015 10.110.4.21:27015";
static constexpr const char *GBE_kOldDotaPracticeLobbyLobbyIdText = "29809934128949123";
static constexpr const char *GBE_kLocalDotaPracticeLobbyLoopbackEndpoint = "127.0.0.1:27015";

static std::string GBE_BuildDotaPracticeLobbyConnectPair(const std::string &endpoint)
{
    if (endpoint.empty())
        return std::string();

    std::string connect = endpoint;
    connect.push_back(' ');
    connect.append(endpoint);
    return connect;
}

static std::string GBE_FormatDotaPracticeLobbyLoopbackConnect()
{
    return GBE_BuildDotaPracticeLobbyConnectPair(GBE_kLocalDotaPracticeLobbyLoopbackEndpoint);
}

static std::string GBE_FormatDotaPracticeLobbyConnectFromEndpoint(const char *endpoint)
{
    if (!endpoint || endpoint[0] == '\0')
        return GBE_FormatDotaPracticeLobbyLoopbackConnect();

    return GBE_BuildDotaPracticeLobbyConnectPair(endpoint);
}

static std::string GBE_FormatDotaPracticeLobbyConnectFromIp(uint32 ip)
{
    if (ip == 0)
        return GBE_FormatDotaPracticeLobbyLoopbackConnect();

    const uint32 octet1 = (ip >> 24) & 0xFFu;
    const uint32 octet2 = (ip >> 16) & 0xFFu;
    const uint32 octet3 = (ip >> 8) & 0xFFu;
    const uint32 octet4 = ip & 0xFFu;

    char endpoint[32] = {};
    std::snprintf(
        endpoint,
        sizeof(endpoint),
        "%u.%u.%u.%u:27015",
        octet1,
        octet2,
        octet3,
        octet4
    );

    return GBE_FormatDotaPracticeLobbyConnectFromEndpoint(endpoint);
}

static constexpr uint32 GBE_kSteamServersAvailable = 5501u;
static constexpr uint32 GBE_kSteamAuthListAck = 5575u;
static constexpr uint32 GBE_kSteamGameConnectTokens = 779u;
static constexpr uint32 GBE_kSteamPersonaState = 766u;
static constexpr uint32 GBE_kSteamTicketAuthComplete = 5429u;

static constexpr const char *GBE_kDotaPracticeLobbyLaunchServersAvailableHex =
    "7d1500800f00000009f5b62108010010011086b0cfdc030a0208030a04082910000a04082a10000a04082210000a04082d10000a04083510000a04083b10000a04087710000a04081110000a04083a10010a04080e10000a04082f10000a0408661000103a";
static constexpr const char *GBE_kDotaPracticeLobbyLaunchPersonaStateInitHex =
    "fe0200800f00000009f5b62108010010011086b0cfdc0308dfe60112bf0309f5b6210801001001100118ba04300138017a075376656e6d6178c9010000000000000000fa0114b7a33b90cbbf208c93b197a18ba999e3110d5908e802ac98a3cf06f0029cb5a3cf06f802ac98a3cf06ba0300c1033a02000000000000e20300ba04170a06737461747573120d23444f54415f52505f494e4954ba041e0a0d737465616d5f646973706c6179120d23444f54415f52505f494e4954ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131ba0499010a056c6f626279128f016c6f6262795f69643a203239383039393334313238393439313233206c6f6262795f73746174653a2053455256455253455455502067616d655f6d6f64653a20444f54415f47414d454d4f44455f4150206d656d6265725f636f756e743a2031206d61785f6d656d6265725f636f756e743a203130206e616d653a202231323322206c6f6262795f747970653a2031c1040000000000000000c9040000000000000000f80400800500880500980501";
static constexpr const char *GBE_kDotaPracticeLobbyLaunchAuthListAckStage1Hex =
    "c71500801800000009f5b62108010010011086b0cfdc0359ffffffffffffffff08a0ffd1c1071801";
static constexpr const char *GBE_kDotaPracticeLobbyLaunchGameConnectTokensStage1Hex =
    "0b0300800f00000009f5b62108010010011086b0cfdc03080a1214a054405a60f87d81f5b621080100100198dde869";
static constexpr const char *GBE_kDotaPracticeLobbyLaunchPersonaStateSetupHex =
    "fe0200800f00000009f5b62108010010011086b0cfdc0308dfe60112f20309f5b6210801001001100118ba04300138017a075376656e6d6178c9010000000000000000fa0114b7a33b90cbbf208c93b197a18ba999e3110d5908e802ac98a3cf06f0029cb5a3cf06f802ac98a3cf06ba0300c1033a02000000000000e20300ba04200a06737461747573121623444f54415f52505f46494e44494e475f4d41544348ba04270a0d737465616d5f646973706c6179121623444f54415f52505f46494e44494e475f4d41544348ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131ba0499010a056c6f626279128f016c6f6262795f69643a203239383039393334313238393439313233206c6f6262795f73746174653a2053455256455253455455502067616d655f6d6f64653a20444f54415f47414d454d4f44455f4150206d656d6265725f636f756e743a2031206d61785f6d656d6265725f636f756e743a203130206e616d653a202231323322206c6f6262795f747970653a2031ba041e0a057061727479121570617274795f73746174653a20494e5f4d41544348c1040000000000000000c9040000000000000000f80400800500880500980501";
static constexpr const char *GBE_kDotaPracticeLobbyLaunchAuthListAckStage2Hex =
    "c71500801800000009f5b62108010010011086b0cfdc0359ffffffffffffffff08a0ffd1c1071802";
static constexpr const char *GBE_kDotaPracticeLobbyLaunchGameConnectTokensStage2Hex =
    "0b0300800f00000009f5b62108010010011086b0cfdc03080a";
static constexpr const char *GBE_kDotaPracticeLobbyLaunchPersonaStateRunHex =
    "fe0200800f00000009f5b62108010010011086b0cfdc0308dfe60112ea0309f5b6210801001001100118ba04300138017a075376656e6d6178c9010000000000000000fa0114b7a33b90cbbf208c93b197a18ba999e3110d5908e802ac98a3cf06f0029cb5a3cf06f802ac98a3cf06ba0300c1033a02000000000000e20300ba04200a06737461747573121623444f54415f52505f46494e44494e475f4d41544348ba04270a0d737465616d5f646973706c6179121623444f54415f52505f46494e44494e475f4d41544348ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131ba0491010a056c6f6262791287016c6f6262795f69643a203239383039393334313238393439313233206c6f6262795f73746174653a2052554e2067616d655f6d6f64653a20444f54415f47414d454d4f44455f4150206d656d6265725f636f756e743a2031206d61785f6d656d6265725f636f756e743a203130206e616d653a202231323322206c6f6262795f747970653a2031ba041e0a057061727479121570617274795f73746174653a20494e5f4d41544348c1040000000000000000c9040000000000000000f80400800500880500980501";
static constexpr const char *GBE_kDotaPracticeLobbyLaunchGameConnectTokensStage3Hex =
    "0b0300800f00000009f5b62108010010011086b0cfdc03080a1214dea8ca5a920d8868f5b6210801001001b2dde869";
static constexpr const char *GBE_kDotaPracticeLobbyLaunchPersonaStatePrivateLobbyHex =
    "fe0200800f00000009f5b62108010010011086b0cfdc0308dfe60112ea0309f5b6210801001001100118ba04300138017a075376656e6d6178c9010000000000000000fa0114b7a33b90cbbf208c93b197a18ba999e3110d5908e802ac98a3cf06f0029cb5a3cf06f802ac98a3cf06ba0300c1033a02000000000000e20300ba04200a06737461747573121623444f54415f52505f505249564154455f4c4f424259ba04270a0d737465616d5f646973706c6179121623444f54415f52505f505249564154455f4c4f424259ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131ba0491010a056c6f6262791287016c6f6262795f69643a203239383039393334313238393439313233206c6f6262795f73746174653a2052554e2067616d655f6d6f64653a20444f54415f47414d454d4f44455f4150206d656d6265725f636f756e743a2031206d61785f6d656d6265725f636f756e743a203130206e616d653a202231323322206c6f6262795f747970653a2031ba041e0a057061727479121570617274795f73746174653a20494e5f4d41544348c1040000000000000000c9040000000000000000f80400800500880500980501";
static constexpr const char *GBE_kDotaPracticeLobbyLaunchTicketAuthCompleteHex =
    "351500800f00000009f5b62108010010011086b0cfdc0309017c58ca8fc14001113a020000000000001802200030a0ffd1c10741f5b6210801001001";
static constexpr const char *GBE_kDotaPracticeLobbyLaunchPersonaStateServerRunHex =
    "fe0200800f00000009f5b62108010010011086b0cfdc03080812e60309f5b6210801001001100118ba04300138017a075376656e6d6178c901017c58ca8fc14001fa0114b7a33b90cbbf208c93b197a18ba999e3110d5908e802ac98a3cf06f0029cb5a3cf06f80200ba0300c1033a02000000000000e20300ba04200a06737461747573121623444f54415f52505f46494e44494e475f4d41544348ba04270a0d737465616d5f646973706c6179121623444f54415f52505f46494e44494e475f4d41544348ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131ba0491010a056c6f6262791287016c6f6262795f69643a203239383039393334313238393439313233206c6f6262795f73746174653a2052554e2067616d655f6d6f64653a20444f54415f47414d454d4f44455f4150206d656d6265725f636f756e743a2031206d61785f6d656d6265725f636f756e743a203130206e616d653a202231323322206c6f6262795f747970653a2031ba041e0a057061727479121570617274795f73746174653a20494e5f4d41544348c1040000000000000000c9040000000000000000f80400800500880500980501";
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

struct GBE_DotaPracticeLobbyLaunchPeripheralTemplate
{
    uint32 emsg{};
    const char *hex{};
    double delay{};
    bool patch_server_id{};
};

static constexpr const char *GBE_kDotaPracticeLobbyLaunchStage1Hex =
    "4d 15 00 80 0f 00 00 00 09 f5 b6 21 08 01 00 10\n"
    "01 10 86 b0 cf dc 03 08 ba 04 10 9a 80 80 80 08\n"
    "1a d7 06 1a 00 00 80 00 00 00 00 12 12 08 de 0f\n"
    "12 0d 0a 09 0a 07 53 76 65 6e 6d 61 78 10 00 12\n"
    "cf 01 08 e0 0f 12 c9 01 0a 35 09 f5 b6 21 08 01\n"
    "00 10 01 48 00 58 00 60 e1 ac 8b 84 d0 85 40 68\n"
    "00 85 01 00 00 00 e0 85 01 41 28 d9 f5 85 01 57\n"
    "06 00 00 98 01 00 98 01 00 98 01 00 98 01 00 15\n"
    "00 00 00 00 1a 30 08 13 12 2c 08 f5 ed 86 41 10\n"
    "00 18 00 20 00 38 00 60 00 d0 01 00 d8 01 00 e0\n"
    "01 00 fa 01 06 08 0f 10 0a 18 0a fa 01 08 08 1c\n"
    "10 e8 07 18 e8 07 1a 1c 08 1a 12 18 08 f5 ed 86\n"
    "41 10 00 18 00 20 01 38 00 60 00 d0 01 00 d8 01\n"
    "00 e0 01 00 1a 1c 08 27 12 18 08 f5 ed 86 41 10\n"
    "00 18 00 20 01 38 00 60 00 d0 01 00 d8 01 00 e0\n"
    "01 00 1a 1d 08 38 12 19 08 f5 ed 86 41 10 e8 07\n"
    "18 00 20 01 38 01 60 00 d0 01 00 d8 01 00 e0 01\n"
    "00 12 ab 01 08 d4 0f 12 a5 01 08 83 cf a2 b4 a2\n"
    "ff f9 34 18 01 20 01 59 f5 b6 21 08 01 00 10 01\n"
    "60 01 68 01 70 01 82 01 03 31 32 33 8a 01 02 40\n"
    "00 8a 01 02 40 00 a8 01 00 e0 01 00 f0 01 df f8\n"
    "bb db 20 f8 01 00 a0 02 03 d0 02 00 d8 02 00 e0\n"
    "02 00 f0 02 00 f8 02 00 80 03 00 98 03 00 a8 03\n"
    "00 c8 03 01 f2 03 07 08 f5 44 12 02 08 00 d8 04\n"
    "00 90 05 00 c0 05 00 e8 05 03 f0 05 00 f8 05 00\n"
    "88 06 00 b8 06 00 c0 06 37 f0 06 00 88 07 00 c2\n"
    "07 10 09 f5 b6 21 08 01 00 10 01 18 00 38 01 80\n"
    "01 01 c8 07 00 f8 07 00 80 08 f3 ba a3 cf 06 12\n"
    "9b 03 08 df 0f 12 95 03 0a 00 12 90 03 08 a5 45\n"
    "12 8a 03 08 f5 ed 86 41 12 bc 01 0a 05 08 02 10\n"
    "c0 0c 0a 05 08 05 10 c8 01 0a 04 08 0a 10 64 0a\n"
    "04 08 0b 10 64 0a 05 08 0c 10 de 02 0a 04 08 22\n"
    "10 64 0a 04 08 23 10 32 0a 05 08 25 10 ee 05 0a\n"
    "05 08 28 10 c0 0c 0a 04 08 2a 10 32 0a 05 08 2c\n"
    "10 db 03 0a 05 08 2f 10 ac 02 0a 05 08 35 10 de\n"
    "02 0a 04 08 45 10 64 0a 04 08 4b 10 64 0a 05 08\n"
    "51 10 d8 04 0a 05 08 53 10 db 03 0a 05 08 54 10\n"
    "bd 15 0a 05 08 55 10 96 01 0a 04 08 68 10 32 0a\n"
    "05 08 c3 02 10 01 0a 05 08 90 03 10 01 0a 05 08\n"
    "91 03 10 01 0a 05 08 92 03 10 01 0a 05 08 9a 03\n"
    "10 06 0a 05 08 cd 03 10 03 0a 05 08 ce 03 10 08\n"
    "0a 05 08 cf 03 10 16 1a 06 08 86 01 10 86 01 1a\n"
    "06 08 d1 0f 10 d2 0f 1a 06 08 89 27 10 8a 27 1a\n"
    "06 08 91 4e 10 92 4e 1a 06 08 f9 55 10 fa 55 1a\n"
    "06 08 e1 5d 10 e2 5d 1a 08 08 d1 89 02 10 d2 89\n"
    "02 1a 08 08 b9 91 02 10 ba 91 02 1a 08 08 89 a1\n"
    "02 10 8a a1 02 1a 08 08 c1 b8 02 10 c2 b8 02 1a\n"
    "08 08 91 c8 02 10 92 c8 02 1a 08 08 e1 d7 02 10\n"
    "e2 d7 02 1a 08 08 99 ef 02 10 9a ef 02 1a 08 08\n"
    "89 9e 03 10 8a 9e 03 1a 08 08 89 9b 04 10 8a 9b\n"
    "04 1a 08 08 f9 c9 04 10 fa c9 04 1a 08 08 e9 f8\n"
    "04 10 ea f8 04 1a 08 08 b9 88 05 10 ba 88 05 1a\n"
    "08 08 a1 90 05 10 a2 90 05 1a 08 08 89 98 05 10\n"
    "8a 98 05 1a 08 08 c1 ac 06 10 c2 ac 06 12 05 08\n"
    "dd 0f 12 00 19 9e ed 8f 26 fa e7 69 00 32 0b 08\n"
    "03 10 83 cf a2 b4 a2 ff f9 34";

static constexpr const char *GBE_kDotaPracticeLobbyLaunchStage2Hex =
    "4d 15 00 80 0f 00 00 00 09 f5 b6 21 08 01 00 10\n"
    "01 10 86 b0 cf dc 03 08 ba 04 10 9a 80 80 80 08\n"
    "1a e7 06 1a 00 00 80 00 00 00 00 12 12 08 de 0f\n"
    "12 0d 0a 09 0a 07 53 76 65 6e 6d 61 78 10 00 12\n"
    "cf 01 08 e0 0f 12 c9 01 0a 35 09 f5 b6 21 08 01\n"
    "00 10 01 48 00 58 00 60 e1 ac 8b 84 d0 85 40 68\n"
    "00 85 01 00 00 00 e0 85 01 41 28 d9 f5 85 01 57\n"
    "06 00 00 98 01 00 98 01 00 98 01 00 98 01 00 15\n"
    "00 00 00 00 1a 30 08 13 12 2c 08 f5 ed 86 41 10\n"
    "00 18 00 20 00 38 00 60 00 d0 01 00 d8 01 00 e0\n"
    "01 00 fa 01 06 08 0f 10 0a 18 0a fa 01 08 08 1c\n"
    "10 e8 07 18 e8 07 1a 1c 08 1a 12 18 08 f5 ed 86\n"
    "41 10 00 18 00 20 01 38 00 60 00 d0 01 00 d8 01\n"
    "00 e0 01 00 1a 1c 08 27 12 18 08 f5 ed 86 41 10\n"
    "00 18 00 20 01 38 00 60 00 d0 01 00 d8 01 00 e0\n"
    "01 00 1a 1d 08 38 12 19 08 f5 ed 86 41 10 e8 07\n"
    "18 00 20 01 38 01 60 00 d0 01 00 d8 01 00 e0 01\n"
    "00 12 bb 01 08 d4 0f 12 b5 01 08 83 cf a2 b4 a2\n"
    "ff f9 34 18 01 20 01 31 01 7c 58 ca 8f c1 40 01\n"
    "59 f5 b6 21 08 01 00 10 01 60 01 68 01 70 01 82\n"
    "01 03 31 32 33 8a 01 02 40 00 8a 01 02 40 00 a8\n"
    "01 00 e0 01 00 f0 01 df f8 bb db 20 f8 01 00 a0\n"
    "02 03 d0 02 00 d8 02 00 e0 02 00 f0 02 00 f8 02\n"
    "00 80 03 00 98 03 00 a8 03 00 c8 03 01 f2 03 07\n"
    "08 f5 44 12 02 08 00 d8 04 00 90 05 00 b8 05 ae\n"
    "bb a3 cf 06 c0 05 00 e8 05 03 f0 05 00 f8 05 00\n"
    "88 06 00 b8 06 00 c0 06 37 f0 06 00 88 07 00 c2\n"
    "07 10 09 f5 b6 21 08 01 00 10 01 18 00 38 01 80\n"
    "01 01 c8 07 00 f8 07 00 80 08 f3 ba a3 cf 06 12\n"
    "9b 03 08 df 0f 12 95 03 0a 00 12 90 03 08 a5 45\n"
    "12 8a 03 08 f5 ed 86 41 12 bc 01 0a 05 08 02 10\n"
    "c0 0c 0a 05 08 05 10 c8 01 0a 04 08 0a 10 64 0a\n"
    "04 08 0b 10 64 0a 05 08 0c 10 de 02 0a 04 08 22\n"
    "10 64 0a 04 08 23 10 32 0a 05 08 25 10 ee 05 0a\n"
    "05 08 28 10 c0 0c 0a 04 08 2a 10 32 0a 05 08 2c\n"
    "10 db 03 0a 05 08 2f 10 ac 02 0a 05 08 35 10 de\n"
    "02 0a 04 08 45 10 64 0a 04 08 4b 10 64 0a 05 08\n"
    "51 10 d8 04 0a 05 08 53 10 db 03 0a 05 08 54 10\n"
    "bd 15 0a 05 08 55 10 96 01 0a 04 08 68 10 32 0a\n"
    "05 08 c3 02 10 01 0a 05 08 90 03 10 01 0a 05 08\n"
    "91 03 10 01 0a 05 08 92 03 10 01 0a 05 08 9a 03\n"
    "10 06 0a 05 08 cd 03 10 03 0a 05 08 ce 03 10 08\n"
    "0a 05 08 cf 03 10 16 1a 06 08 86 01 10 86 01 1a\n"
    "06 08 d1 0f 10 d2 0f 1a 06 08 89 27 10 8a 27 1a\n"
    "06 08 91 4e 10 92 4e 1a 06 08 f9 55 10 fa 55 1a\n"
    "06 08 e1 5d 10 e2 5d 1a 08 08 d1 89 02 10 d2 89\n"
    "02 1a 08 08 b9 91 02 10 ba 91 02 1a 08 08 89 a1\n"
    "02 10 8a a1 02 1a 08 08 c1 b8 02 10 c2 b8 02 1a\n"
    "08 08 91 c8 02 10 92 c8 02 1a 08 08 e1 d7 02 10\n"
    "e2 d7 02 1a 08 08 99 ef 02 10 9a ef 02 1a 08 08\n"
    "89 9e 03 10 8a 9e 03 1a 08 08 89 9b 04 10 8a 9b\n"
    "04 1a 08 08 f9 c9 04 10 fa c9 04 1a 08 08 e9 f8\n"
    "04 10 ea f8 04 1a 08 08 b9 88 05 10 ba 88 05 1a\n"
    "08 08 a1 90 05 10 a2 90 05 1a 08 08 89 98 05 10\n"
    "8a 98 05 1a 08 08 c1 ac 06 10 c2 ac 06 12 05 08\n"
    "dd 0f 12 00 19 00 a4 95 26 fa e7 69 00 32 0b 08\n"
    "03 10 83 cf a2 b4 a2 ff f9 34";

static constexpr const char *GBE_kDotaPracticeLobbyLaunchStage3Hex =
    "4d 15 00 80 0f 00 00 00 09 f5 b6 21 08 01 00 10\n"
    "01 10 86 b0 cf dc 03 08 ba 04 10 9a 80 80 80 08\n"
    "1a 8c 07 1a 00 00 80 00 00 00 00 12 12 08 de 0f\n"
    "12 0d 0a 09 0a 07 53 76 65 6e 6d 61 78 10 00 12\n"
    "cf 01 08 e0 0f 12 c9 01 0a 35 09 f5 b6 21 08 01\n"
    "00 10 01 48 00 58 00 60 e1 ac 8b 84 d0 85 40 68\n"
    "00 85 01 00 00 00 e0 85 01 41 28 d9 f5 85 01 57\n"
    "06 00 00 98 01 00 98 01 00 98 01 00 98 01 00 15\n"
    "00 00 00 00 1a 30 08 13 12 2c 08 f5 ed 86 41 10\n"
    "00 18 00 20 00 38 00 60 00 d0 01 00 d8 01 00 e0\n"
    "01 00 fa 01 06 08 0f 10 0a 18 0a fa 01 08 08 1c\n"
    "10 e8 07 18 e8 07 1a 1c 08 1a 12 18 08 f5 ed 86\n"
    "41 10 00 18 00 20 01 38 00 60 00 d0 01 00 d8 01\n"
    "00 e0 01 00 1a 1c 08 27 12 18 08 f5 ed 86 41 10\n"
    "00 18 00 20 01 38 00 60 00 d0 01 00 d8 01 00 e0\n"
    "01 00 1a 1d 08 38 12 19 08 f5 ed 86 41 10 e8 07\n"
    "18 00 20 01 38 01 60 00 d0 01 00 d8 01 00 e0 01\n"
    "00 12 e0 01 08 d4 0f 12 da 01 08 83 cf a2 b4 a2\n"
    "ff f9 34 18 01 20 02 2a 26 31 31 37 2e 31 35 37\n"
    "2e 37 39 2e 31 39 34 3a 32 37 30 31 35 20 31 30\n"
    "2e 31 31 30 2e 34 2e 32 31 3a 32 37 30 31 35 31\n"
    "01 7c 58 ca 8f c1 40 01 59 f5 b6 21 08 01 00 10\n"
    "01 60 01 68 01 70 01 82 01 03 31 32 33 8a 01 02\n"
    "40 00 8a 01 02 40 00 a8 01 00 e0 01 00 f0 01 df\n"
    "f8 bb db 20 f8 01 00 a0 02 03 d0 02 00 d8 02 00\n"
    "e0 02 00 f0 02 00 f8 02 00 80 03 00 98 03 00 a8\n"
    "03 00 c8 03 01 f2 03 07 08 f5 44 12 02 08 00 d8\n"
    "04 00 90 05 00 b8 05 ae bb a3 cf 06 c0 05 00 e8\n"
    "05 03 f0 05 00 f8 05 00 88 06 00 b8 06 00 c0 06\n"
    "37 f0 06 00 88 07 00 c2 07 0d 09 f5 b6 21 08 01\n"
    "00 10 01 18 00 38 01 c8 07 00 f8 07 00 80 08 f3\n"
    "ba a3 cf 06 12 9b 03 08 df 0f 12 95 03 0a 00 12\n"
    "90 03 08 a5 45 12 8a 03 08 f5 ed 86 41 12 bc 01\n"
    "0a 05 08 02 10 c0 0c 0a 05 08 05 10 c8 01 0a 04\n"
    "08 0a 10 64 0a 04 08 0b 10 64 0a 05 08 0c 10 de\n"
    "02 0a 04 08 22 10 64 0a 04 08 23 10 32 0a 05 08\n"
    "25 10 ee 05 0a 05 08 28 10 c0 0c 0a 04 08 2a 10\n"
    "32 0a 05 08 2c 10 db 03 0a 05 08 2f 10 ac 02 0a\n"
    "05 08 35 10 de 02 0a 04 08 45 10 64 0a 04 08 4b\n"
    "10 64 0a 05 08 51 10 d8 04 0a 05 08 53 10 db 03\n"
    "0a 05 08 54 10 bd 15 0a 05 08 55 10 96 01 0a 04\n"
    "08 68 10 32 0a 05 08 c3 02 10 01 0a 05 08 90 03\n"
    "10 01 0a 05 08 91 03 10 01 0a 05 08 92 03 10 01\n"
    "0a 05 08 9a 03 10 06 0a 05 08 cd 03 10 03 0a 05\n"
    "08 ce 03 10 08 0a 05 08 cf 03 10 16 1a 06 08 86\n"
    "01 10 86 01 1a 06 08 d1 0f 10 d2 0f 1a 06 08 89\n"
    "27 10 8a 27 1a 06 08 91 4e 10 92 4e 1a 06 08 f9\n"
    "55 10 fa 55 1a 06 08 e1 5d 10 e2 5d 1a 08 08 d1\n"
    "89 02 10 d2 89 02 1a 08 08 b9 91 02 10 ba 91 02\n"
    "1a 08 08 89 a1 02 10 8a a1 02 1a 08 08 c1 b8 02\n"
    "10 c2 b8 02 1a 08 08 91 c8 02 10 92 c8 02 1a 08\n"
    "08 e1 d7 02 10 e2 d7 02 1a 08 08 99 ef 02 10 9a\n"
    "ef 02 1a 08 08 89 9e 03 10 8a 9e 03 1a 08 08 89\n"
    "9b 04 10 8a 9b 04 1a 08 08 f9 c9 04 10 fa c9 04\n"
    "1a 08 08 e9 f8 04 10 ea f8 04 1a 08 08 b9 88 05\n"
    "10 ba 88 05 1a 08 08 a1 90 05 10 a2 90 05 1a 08\n"
    "08 89 98 05 10 8a 98 05 1a 08 08 c1 ac 06 10 c2\n"
    "ac 06 12 05 08 dd 0f 12 00 19 46 b8 95 26 fa e7\n"
    "69 00 32 0b 08 03 10 83 cf a2 b4 a2 ff f9 34";

static constexpr const char *GBE_kDotaPracticeLobbyLaunchStage4Hex =
    "4d 15 00 80 0f 00 00 00 09 f5 b6 21 08 01 00 10\n"
    "01 10 86 b0 cf dc 03 08 ba 04 10 9a 80 80 80 08\n"
    "1a fd 03 1a 00 00 80 00 00 00 00 12 12 08 de 0f\n"
    "12 0d 0a 09 0a 07 53 76 65 6e 6d 61 78 10 00 12\n"
    "cf 01 08 e0 0f 12 c9 01 0a 35 09 f5 b6 21 08 01\n"
    "00 10 01 48 00 58 00 60 e1 ac 8b 84 d0 85 40 68\n"
    "00 85 01 00 00 00 e0 85 01 41 28 d9 f5 85 01 57\n"
    "06 00 00 98 01 00 98 01 00 98 01 00 98 01 00 15\n"
    "00 00 00 00 1a 30 08 13 12 2c 08 f5 ed 86 41 10\n"
    "00 18 00 20 00 38 00 60 00 d0 01 00 d8 01 00 e0\n"
    "01 00 fa 01 06 08 0f 10 0a 18 0a fa 01 08 08 1c\n"
    "10 e8 07 18 e8 07 1a 1c 08 1a 12 18 08 f5 ed 86\n"
    "41 10 00 18 00 20 01 38 00 60 00 d0 01 00 d8 01\n"
    "00 e0 01 00 1a 1c 08 27 12 18 08 f5 ed 86 41 10\n"
    "00 18 00 20 01 38 00 60 00 d0 01 00 d8 01 00 e0\n"
    "01 00 1a 1d 08 38 12 19 08 f5 ed 86 41 10 e8 07\n"
    "18 00 20 01 38 01 60 00 d0 01 00 d8 01 00 e0 01\n"
    "00 12 e6 01 08 d4 0f 12 e0 01 08 83 cf a2 b4 a2\n"
    "ff f9 34 18 01 20 02 2a 26 31 31 37 2e 31 35 37\n"
    "2e 37 39 2e 31 39 34 3a 32 37 30 31 35 20 31 30\n"
    "2e 31 31 30 2e 34 2e 32 31 3a 32 37 30 31 35 31\n"
    "01 7c 58 ca 8f c1 40 01 59 f5 b6 21 08 01 00 10\n"
    "01 60 01 68 01 70 01 82 01 03 31 32 33 8a 01 02\n"
    "40 00 8a 01 02 40 00 a8 01 00 b0 01 01 e0 01 00\n"
    "f0 01 df f8 bb db 20 f8 01 00 a0 02 03 d0 02 00\n"
    "d8 02 00 e0 02 00 f0 02 00 f8 02 00 80 03 00 98\n"
    "03 00 a8 03 00 c8 03 01 f2 03 07 08 f5 44 12 02\n"
    "08 00 88 04 00 d8 04 00 90 05 00 b8 05 ae bb a3\n"
    "cf 06 c0 05 00 e8 05 03 f0 05 00 f8 05 00 88 06\n"
    "00 b8 06 00 c0 06 37 f0 06 00 88 07 00 c2 07 0d\n"
    "09 f5 b6 21 08 01 00 10 01 18 00 38 01 c8 07 00\n"
    "f8 07 00 80 08 f3 ba a3 cf 06 12 07 08 df 0f 12\n"
    "02 0a 00 12 05 08 dd 0f 12 00 19 62 01 96 26 fa\n"
    "e7 69 00 32 0b 08 03 10 83 cf a2 b4 a2 ff f9 34";

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

static const uint8 GBE_kDota7388Profile20Template[] = {
    0xDC, 0x1C, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x59, 0x07, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x00, 0x10, 0x00, 0x18, 0x20, 0x20, 0x00, 0x28, 0x00, 0x38, 0xF5, 0xED, 0x86, 0x41,
    0x40, 0x00, 0x50, 0x00,
};

static const uint8 GBE_kDota7388Profile37Template[] = {
    0xDC, 0x1C, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x59, 0x0E, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0xE8, 0x07, 0x10, 0x00, 0x18, 0x37, 0x20, 0xE8, 0x07, 0x28, 0x00, 0x38, 0xF5, 0xED,
    0x86, 0x41, 0x40, 0x01, 0x50, 0x00,
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

static const uint8 GBE_kDotaPracticeLobbyResponseTemplate[] = {
    0x8F, 0x1B, 0x00, 0x80, 0x09, 0x00, 0x00, 0x00, 0x59, 0x30, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
    0x00, 0x08, 0x01,
};

static const uint8 GBE_kDotaPracticeLobbySOUpdateTemplate[] = {
    0x02, 0x18, 0x00, 0x00, 0x80, 0x00, 0x00, 0x00, 0x00, 0x12, 0xC2, 0x01, 0x08, 0xD4, 0x0F, 0x12,
    0xBC, 0x01, 0x08, 0x9D, 0x97, 0xF8, 0x9E, 0x95, 0xD7, 0xF7, 0x34, 0x18, 0x01, 0x20, 0x00, 0x59,
    0xF5, 0xB6, 0x21, 0x08, 0x01, 0x00, 0x10, 0x01, 0x60, 0x01, 0x68, 0x00, 0x70, 0x01, 0x82, 0x01,
    0x05, 0x30, 0x30, 0x31, 0x32, 0x35, 0xA8, 0x01, 0x05, 0xE0, 0x01, 0x00, 0xF8, 0x01, 0x01, 0xA0,
    0x02, 0x04, 0xD0, 0x02, 0x00, 0xD8, 0x02, 0x00, 0xE0, 0x02, 0x00, 0xF0, 0x02, 0x00, 0xF8, 0x02,
    0x00, 0x80, 0x03, 0x00, 0x98, 0x03, 0x00, 0xA8, 0x03, 0x00, 0xC8, 0x03, 0x00, 0xF2, 0x03, 0x07,
    0x08, 0xF5, 0x44, 0x12, 0x02, 0x08, 0x00, 0xD8, 0x04, 0x00, 0x90, 0x05, 0x00, 0xC0, 0x05, 0x00,
    0xE8, 0x05, 0x04, 0xF0, 0x05, 0x8A, 0xB6, 0xFB, 0x8B, 0x0C, 0xF8, 0x05, 0xCD, 0xFB, 0x92, 0xDA,
    0x0A, 0x88, 0x06, 0x00, 0xF0, 0x06, 0x00, 0x88, 0x07, 0x00, 0xC2, 0x07, 0x10, 0x09, 0xF5, 0xB6,
    0x21, 0x08, 0x01, 0x00, 0x10, 0x01, 0x18, 0x00, 0x38, 0x01, 0x80, 0x01, 0x01, 0xC8, 0x07, 0x00,
    0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0,
    0x07, 0x00, 0xE0, 0x07, 0x00, 0xE0, 0x07, 0x00,
};

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

struct ProtoBufMsgHeader_t
{
    uint32          m_EMsgFlagged;          // High bit should be set to indicate this message header type is in use.  The rest of the bits indicate message type.
    uint32          m_cubProtoBufExtHdr;    // Size of the extended header which is a serialized protobuf object.  Indicates where it ends and the serialized body protobuf begins.
};
#pragma pack(pop)

template <class T>
static void ser_var(std::string &buf, const T &input)
{
    buf.append(reinterpret_cast<const char *>(&input), sizeof(T));
}

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

struct GBE_ProtoFieldView
{
    bool found{};
    uint32 field_number{};
    uint32 wire_type{};
    size_t value_offset{};
    size_t value_size{};
};

struct GBE_DotaHelloContext
{
    bool valid{};
    uint32 version{};
    std::string outer_session_field_raw;
    uint64 source_job_id{};
    bool has_source_job{};
};

struct GBE_DotaServerHelloContext
{
    bool valid{};
    uint32 active_version{};
    uint32 min_allowed_version{};
    uint64 compatibility_value{};
    uint32 universe{};
    uint64 source_job_id{};
    bool has_source_job{};
};

struct GBE_DotaWrappedDirectContext
{
    bool valid{};
    uint32 inner_emsg{};
    std::string outer_session_field_raw;
    std::string inner_body_raw;
    uint64 request_job_id{};
    bool has_request_job{};
};

struct GBE_DotaPracticeLobbyDetailsRequest
{
    bool has_lobby_id{};
    uint64 lobby_id{};
    bool has_room_name{};
    std::string room_name;
    bool has_server_region{};
    uint32 server_region{};
    bool has_lan{};
    bool lan{};
    bool has_lan_host_ping_location{};
    std::string lan_host_ping_location;
    bool has_game_mode{};
    uint32 game_mode{};
    bool has_bot_difficulty_radiant{};
    uint32 bot_difficulty_radiant{};
    bool has_allow_cheats{};
    bool allow_cheats{};
    bool has_fill_with_bots{};
    bool fill_with_bots{};
    bool has_allow_spectating{};
    bool allow_spectating{};
    bool has_pass_key{};
    std::string pass_key;
    bool has_visibility{};
    uint32 visibility{};
    bool has_bot_difficulty_dire{};
    uint32 bot_difficulty_dire{};
    bool has_bot_radiant{};
    uint64 bot_radiant{};
    bool has_bot_dire{};
    uint64 bot_dire{};
};

struct GBE_DotaPracticeLobbyCreateRequest
{
    bool has_pass_key{};
    std::string pass_key;
    bool has_lobby_details{};
    GBE_DotaPracticeLobbyDetailsRequest lobby_details;
};

struct GBE_DotaPracticeLobbySetTeamSlotRequest
{
    bool has_team{};
    uint32 team{};
    bool has_slot{};
    uint32 slot{};
    bool has_bot_difficulty{};
    uint32 bot_difficulty{};
};

struct GBE_DotaPracticeLobbyBroadcastChannelRequest
{
    bool has_channel{};
    uint32 channel{};
    bool has_country_code{};
    std::string country_code;
    bool has_description{};
    std::string description;
    bool has_language_code{};
    std::string language_code;
};

struct GBE_DotaJoinChatChannelRequest
{
    bool has_channel_name{};
    std::string channel_name;
    bool has_channel_type{};
    uint32 channel_type{};
};

struct GBE_DotaLeaveChatChannelRequest
{
    bool has_channel_id{};
    uint64 channel_id{};
};

static void GBE_GC_DebugLog(const char *scope, const char *fmt, ...)
{
    FILE *file = std::fopen(GBE_kGcDebugLogPath, "a");
    if (!file)
        return;

    std::fprintf(file, "[%s] ", scope ? scope : "GC");

    va_list args;
    va_start(args, fmt);
    std::vfprintf(file, fmt, args);
    va_end(args);

    std::fprintf(file, "\n");
    std::fclose(file);
}

static uint32 GBE_GC_MaskedEMsg(uint32 msg_type)
{
    return msg_type & (~GBE_kProtoMask);
}

static bool GBE_ReadVarUint64(const uint8 *data, size_t size, size_t &offset, uint64 &value, size_t *raw_begin = nullptr, size_t *raw_end = nullptr)
{
    if (!data || offset >= size)
        return false;

    size_t begin = offset;
    value = 0;
    int shift = 0;

    while (offset < size && shift <= 63) {
        const uint8 byte = data[offset++];
        value |= static_cast<uint64>(byte & 0x7F) << shift;

        if ((byte & 0x80) == 0) {
            if (raw_begin)
                *raw_begin = begin;
            if (raw_end)
                *raw_end = offset;
            return true;
        }

        shift += 7;
    }

    return false;
}

static GBE_ProtoFieldView GBE_FindProtoField(const uint8 *data, size_t size, uint32 wanted_field)
{
    GBE_ProtoFieldView view{};
    size_t offset = 0;

    while (offset < size) {
        uint64 key = 0;
        if (!GBE_ReadVarUint64(data, size, offset, key))
            return {};

        const uint32 field_number = static_cast<uint32>(key >> 3);
        const uint32 wire_type = static_cast<uint32>(key & 0x7);
        size_t value_offset = offset;
        size_t value_size = 0;

        switch (wire_type) {
            case 0: {
                size_t raw_begin = offset;
                size_t raw_end = offset;
                uint64 ignored = 0;
                if (!GBE_ReadVarUint64(data, size, offset, ignored, &raw_begin, &raw_end))
                    return {};
                value_size = raw_end - raw_begin;
                break;
            }
            case 1:
                if (offset + 8 > size)
                    return {};
                offset += 8;
                value_size = 8;
                break;
            case 2: {
                uint64 length = 0;
                if (!GBE_ReadVarUint64(data, size, offset, length))
                    return {};
                if (offset + length > size)
                    return {};
                value_offset = offset;
                value_size = static_cast<size_t>(length);
                offset += static_cast<size_t>(length);
                break;
            }
            case 5:
                if (offset + 4 > size)
                    return {};
                offset += 4;
                value_size = 4;
                break;
            default:
                return {};
        }

        if (field_number == wanted_field) {
            view.found = true;
            view.field_number = field_number;
            view.wire_type = wire_type;
            view.value_offset = value_offset;
            view.value_size = value_size;
            return view;
        }
    }

    return {};
}

static bool GBE_ReadNextProtoField(
    const uint8 *data,
    size_t size,
    size_t &offset,
    uint32 &field_number,
    uint32 &wire_type,
    size_t &field_offset,
    size_t &value_offset,
    size_t &value_size,
    size_t &field_end)
{
    if (!data || offset >= size)
        return false;

    field_offset = offset;

    uint64 key = 0;
    if (!GBE_ReadVarUint64(data, size, offset, key))
        return false;

    field_number = static_cast<uint32>(key >> 3);
    wire_type = static_cast<uint32>(key & 0x7);
    value_offset = offset;
    value_size = 0;

    switch (wire_type) {
        case 0: {
            size_t raw_begin = offset;
            size_t raw_end = offset;
            uint64 ignored = 0;
            if (!GBE_ReadVarUint64(data, size, offset, ignored, &raw_begin, &raw_end))
                return false;
            value_offset = raw_begin;
            value_size = raw_end - raw_begin;
            break;
        }
        case 1:
            if (offset + 8 > size)
                return false;
            value_size = 8;
            offset += 8;
            break;
        case 2: {
            uint64 length = 0;
            if (!GBE_ReadVarUint64(data, size, offset, length))
                return false;
            if (offset + length > size)
                return false;
            value_offset = offset;
            value_size = static_cast<size_t>(length);
            offset += static_cast<size_t>(length);
            break;
        }
        case 5:
            if (offset + 4 > size)
                return false;
            value_size = 4;
            offset += 4;
            break;
        default:
            return false;
    }

    field_end = offset;
    return true;
}

static void GBE_AppendVarUint64(std::string &buffer, uint64 value)
{
    do {
        uint8 byte = static_cast<uint8>(value & 0x7F);
        value >>= 7;
        if (value != 0)
            byte |= 0x80;
        buffer.push_back(static_cast<char>(byte));
    } while (value != 0);
}

static void GBE_AppendLittleEndian32(std::string &buffer, uint32 value)
{
    ser_var<uint32>(buffer, value);
}

static void GBE_AppendLittleEndian64(std::string &buffer, uint64 value)
{
    ser_var<uint64>(buffer, value);
}

static void GBE_AppendRawBytes(std::string &buffer, const uint8 *data, size_t size)
{
    if (!data || size == 0)
        return;

    buffer.append(reinterpret_cast<const char *>(data), size);
}

static void GBE_AppendProtoVarIntField(std::string &buffer, uint32 field_number, uint64 value)
{
    GBE_AppendVarUint64(buffer, (static_cast<uint64>(field_number) << 3) | 0u);
    GBE_AppendVarUint64(buffer, value);
}

static void GBE_AppendProtoFixed64Field(std::string &buffer, uint32 field_number, uint64 value)
{
    GBE_AppendVarUint64(buffer, (static_cast<uint64>(field_number) << 3) | 1u);
    ser_var<uint64>(buffer, value);
}

static void GBE_AppendProtoBytesField(std::string &buffer, uint32 field_number, const std::string &value)
{
    GBE_AppendVarUint64(buffer, (static_cast<uint64>(field_number) << 3) | 2u);
    GBE_AppendVarUint64(buffer, static_cast<uint64>(value.size()));
    buffer.append(value);
}

static void GBE_AppendProtoFixed32Field(std::string &buffer, uint32 field_number, uint32 value)
{
    GBE_AppendVarUint64(buffer, (static_cast<uint64>(field_number) << 3) | 5u);
    ser_var<uint32>(buffer, value);
}

static std::vector<uint8> GBE_VectorFromBytes(const uint8 *data, size_t size)
{
    return std::vector<uint8>(data, data + size);
}

static size_t GBE_CountBytePatternMatches(const std::string &buffer, const std::vector<uint8> &needle)
{
    if (needle.empty())
        return 0;

    size_t matches = 0;
    for (size_t offset = 0; offset + needle.size() <= buffer.size(); ++offset) {
        bool matched = true;
        for (size_t i = 0; i < needle.size(); ++i) {
            if (static_cast<uint8>(buffer[offset + i]) != needle[i]) {
                matched = false;
                break;
            }
        }

        if (!matched)
            continue;

        ++matches;
        offset += needle.size() - 1;
    }

    return matches;
}

static std::string GBE_FormatHexPrefix(const uint8 *data, size_t size, size_t max_bytes)
{
    if (!data || size == 0 || max_bytes == 0)
        return std::string();

    const size_t bytes_to_dump = std::min(size, max_bytes);
    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (size_t i = 0; i < bytes_to_dump; ++i) {
        if (i != 0)
            stream << ' ';
        stream << std::setw(2) << static_cast<unsigned int>(data[i]);
    }
    if (size > bytes_to_dump)
        stream << " ...";
    return stream.str();
}

// Raw binary template patch helper. Replacement must keep the exact same byte length.
static bool GBE_FindAndOverwriteBytes(std::string &buffer, const std::vector<uint8> &needle, const std::vector<uint8> &replacement)
{
    if (needle.empty() || needle.size() != replacement.size())
        return false;

    bool replaced = false;
    for (size_t offset = 0; offset + needle.size() <= buffer.size(); ++offset) {
        bool matched = true;
        for (size_t i = 0; i < needle.size(); ++i) {
            if (static_cast<uint8>(buffer[offset + i]) != needle[i]) {
                matched = false;
                break;
            }
        }

        if (!matched)
            continue;

        for (size_t i = 0; i < replacement.size(); ++i) {
            buffer[offset + i] = static_cast<char>(replacement[i]);
        }

        offset += replacement.size() - 1;
        replaced = true;
    }

    return replaced;
}

static bool GBE_DecodeHexString(const char *hex, std::string &decoded)
{
    decoded.clear();
    if (!hex)
        return false;

    int high_nibble = -1;
    for (const char *cursor = hex; *cursor != '\0'; ++cursor) {
        const unsigned char ch = static_cast<unsigned char>(*cursor);
        if (std::isspace(ch))
            continue;

        int value = -1;
        if (ch >= '0' && ch <= '9')
            value = ch - '0';
        else if (ch >= 'a' && ch <= 'f')
            value = 10 + (ch - 'a');
        else if (ch >= 'A' && ch <= 'F')
            value = 10 + (ch - 'A');
        else
            return false;

        if (high_nibble < 0) {
            high_nibble = value;
            continue;
        }

        decoded.push_back(static_cast<char>((high_nibble << 4) | value));
        high_nibble = -1;
    }

    return high_nibble < 0;
}

static bool GBE_FindAndOverwriteString(std::string &buffer, const std::string &needle, const std::string &replacement)
{
    if (needle.empty() || needle.size() != replacement.size())
        return false;

    bool replaced = false;
    size_t offset = 0;
    while ((offset = buffer.find(needle, offset)) != std::string::npos) {
        buffer.replace(offset, replacement.size(), replacement);
        offset += replacement.size();
        replaced = true;
    }

    return replaced;
}

static bool GBE_EncodeVarUint64WithExpectedSize(uint64 value, size_t expected_size, std::vector<uint8> &encoded)
{
    std::string encoded_raw;
    GBE_AppendVarUint64(encoded_raw, value);
    if (encoded_raw.size() != expected_size)
        return false;

    encoded.assign(encoded_raw.begin(), encoded_raw.end());
    return true;
}

static bool GBE_RewriteProtoVarintBytesRecursive(
    const std::string &input,
    const std::vector<uint8> &old_encoded,
    uint32 target_field_number,
    uint64 new_value,
    std::string &output,
    size_t &replacement_count)
{
    output.clear();
    replacement_count = 0;

    size_t offset = 0;
    while (offset < input.size()) {
        uint32 field_number = 0;
        uint32 wire_type = 0;
        size_t field_offset = 0;
        size_t value_offset = 0;
        size_t value_size = 0;
        size_t field_end = 0;
        if (!GBE_ReadNextProtoField(
                reinterpret_cast<const uint8 *>(input.data()),
                input.size(),
                offset,
                field_number,
                wire_type,
                field_offset,
                value_offset,
                value_size,
                field_end))
            return false;

        if (wire_type == 0u) {
            GBE_AppendVarUint64(output, (static_cast<uint64>(field_number) << 3) | wire_type);

            const uint8 *raw_value = reinterpret_cast<const uint8 *>(input.data() + value_offset);
            if (field_number == target_field_number
                && value_size == old_encoded.size()
                && std::memcmp(raw_value, old_encoded.data(), value_size) == 0) {
                GBE_AppendVarUint64(output, new_value);
                ++replacement_count;
            } else {
                output.append(input.data() + value_offset, value_size);
            }

            continue;
        }

        if (wire_type == 2u) {
            std::string nested_input(input.data() + value_offset, value_size);
            std::string nested_output;
            size_t nested_replacement_count = 0;
            if (GBE_RewriteProtoVarintBytesRecursive(
                    nested_input,
                    old_encoded,
                    target_field_number,
                    new_value,
                    nested_output,
                    nested_replacement_count)
                && nested_replacement_count > 0) {
                GBE_AppendProtoBytesField(output, field_number, nested_output);
                replacement_count += nested_replacement_count;
                continue;
            }
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    return true;
}

static bool GBE_RewriteAccountIdVarintInDirectProtoBody(
    std::string &message,
    uint32 account_id,
    size_t &replacement_count)
{
    replacement_count = 0;

    if (message.size() < sizeof(ProtoBufMsgHeader_t))
        return false;

    ProtoBufMsgHeader_t hdr{};
    std::memcpy(&hdr, message.data(), sizeof(hdr));

    const size_t body_offset = sizeof(hdr) + hdr.m_cubProtoBufExtHdr;
    if (body_offset > message.size())
        return false;

    const std::string body = message.substr(body_offset);
    std::string rewritten_body;
    if (!GBE_RewriteProtoVarintBytesRecursive(
            body,
            GBE_VectorFromBytes(GBE_kOldDotaAccountIdVarint.data(), GBE_kOldDotaAccountIdVarint.size()),
            1u,
            account_id,
            rewritten_body,
            replacement_count))
        return false;

    if (replacement_count == 0)
        return true;

    message.resize(body_offset);
    message.append(rewritten_body);
    return true;
}

static bool GBE_TryPatchDotaAccountIdVarint(
    std::string &message,
    uint32 account_id,
    const char *log_scope,
    uint32 request_emsg,
    uint32 response_emsg,
    size_t body_size,
    const char *context_note)
{
    std::string encoded_account_raw;
    GBE_AppendVarUint64(encoded_account_raw, account_id);

    std::string rewritten_message;
    size_t replacement_count = 0;
    const bool full_message_parse_ok = GBE_RewriteProtoVarintBytesRecursive(
            message,
            GBE_VectorFromBytes(GBE_kOldDotaAccountIdVarint.data(), GBE_kOldDotaAccountIdVarint.size()),
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

static bool GBE_TryPatchDotaAccountIdFixed32(std::string &message, uint32 account_id, const char *log_scope)
{
    std::string encoded_account_raw;
    GBE_AppendLittleEndian32(encoded_account_raw, account_id);

    const std::vector<uint8> old_account_id_fixed32 = GBE_VectorFromBytes(GBE_kOldDotaAccountIdFixed32.data(), GBE_kOldDotaAccountIdFixed32.size());
    const size_t match_count = GBE_CountBytePatternMatches(message, old_account_id_fixed32);
    if (match_count == 0)
        return true;

    if (!GBE_FindAndOverwriteBytes(
            message,
            old_account_id_fixed32,
            GBE_VectorFromBytes(reinterpret_cast<const uint8 *>(encoded_account_raw.data()), encoded_account_raw.size()))) {
        GBE_GC_DebugLog(log_scope, "failed replacing account_id fixed32 bytes account_id=%u matches=%zu", account_id, match_count);
        return false;
    }

    return true;
}

static bool GBE_RewriteDotaAccountBoundObjectData(const std::string &input, uint32 account_id, std::string &output)
{
    output.clear();
    bool saw_account_id = false;

    size_t offset = 0;
    while (offset < input.size()) {
        uint32 field_number = 0;
        uint32 wire_type = 0;
        size_t field_offset = 0;
        size_t value_offset = 0;
        size_t value_size = 0;
        size_t field_end = 0;
        if (!GBE_ReadNextProtoField(
                reinterpret_cast<const uint8 *>(input.data()),
                input.size(),
                offset,
                field_number,
                wire_type,
                field_offset,
                value_offset,
                value_size,
                field_end))
            return false;

        if (field_number == 1u && wire_type == 0u) {
            saw_account_id = true;
            GBE_AppendProtoVarIntField(output, 1u, account_id);
            continue;
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    if (!saw_account_id)
        GBE_AppendProtoVarIntField(output, 1u, account_id);

    return true;
}

static bool GBE_PatchDotaWelcomeAccountObjects(std::string &inner_body, uint32 account_id)
{
    std::string rewritten_body;
    int patched_object_count = 0;
    size_t offset = 0;
    while (offset < inner_body.size()) {
        uint32 field_number = 0;
        uint32 wire_type = 0;
        size_t field_offset = 0;
        size_t value_offset = 0;
        size_t value_size = 0;
        size_t field_end = 0;
        if (!GBE_ReadNextProtoField(
                reinterpret_cast<const uint8 *>(inner_body.data()),
                inner_body.size(),
                offset,
                field_number,
                wire_type,
                field_offset,
                value_offset,
                value_size,
                field_end))
            return false;

        if (field_number != 3u || wire_type != 2u) {
            rewritten_body.append(inner_body.data() + field_offset, field_end - field_offset);
            continue;
        }

        CMsgSOCacheSubscribed cache;
        if (!cache.ParseFromArray(inner_body.data() + value_offset, static_cast<int>(value_size)))
            return false;

        int cache_patch_count = 0;
        for (int object_index = 0; object_index < cache.objects_size(); ++object_index) {
            auto *object = cache.mutable_objects(object_index);
            const int type_id = object->type_id();
            if (type_id != 2002 && type_id != 2012)
                continue;

            for (int data_index = 0; data_index < object->object_data_size(); ++data_index) {
                std::string rewritten_object;
                if (!GBE_RewriteDotaAccountBoundObjectData(object->object_data(data_index), account_id, rewritten_object))
                    return false;

                object->set_object_data(data_index, rewritten_object);
                ++patched_object_count;
                ++cache_patch_count;
            }
        }

        if (cache_patch_count != 0) {
            GBE_AppendProtoBytesField(rewritten_body, 3u, cache.SerializeAsString());
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

static bool GBE_ExtractProtoFieldUint64(const uint8 *data, size_t size, const GBE_ProtoFieldView &view, uint64 &value)
{
    if (!view.found)
        return false;

    if (view.wire_type == 0) {
        size_t offset = view.value_offset;
        return GBE_ReadVarUint64(data, size, offset, value);
    }

    if (view.wire_type == 1 && view.value_size == 8) {
        std::memcpy(&value, data + view.value_offset, sizeof(value));
        return true;
    }

    return false;
}

static bool GBE_ExtractProtoFieldUint32(const uint8 *data, size_t size, const GBE_ProtoFieldView &view, uint32 &value)
{
    uint64 wide_value = 0;
    if (!view.found)
        return false;

    if (view.wire_type == 0u) {
        if (!GBE_ExtractProtoFieldUint64(data, size, view, wide_value))
            return false;
        value = static_cast<uint32>(wide_value > 0xFFFFFFFFull ? 0xFFFFFFFFu : wide_value);
        return true;
    }

    if (view.wire_type == 5u && view.value_size == 4u) {
        std::memcpy(&value, data + view.value_offset, sizeof(value));
        return true;
    }

    return false;
}

static std::string GBE_FormatIPv4(uint32 value)
{
    std::ostringstream stream;
    stream
        << ((value >> 24) & 0xFFu)
        << '.'
        << ((value >> 16) & 0xFFu)
        << '.'
        << ((value >> 8) & 0xFFu)
        << '.'
        << (value & 0xFFu);
    return stream.str();
}

static bool GBE_ExtractProtoFieldBytes(const uint8 *data, size_t size, const GBE_ProtoFieldView &view, std::string &value)
{
    if (!view.found || view.wire_type != 2)
        return false;

    if (view.value_offset + view.value_size > size)
        return false;

    value.assign(reinterpret_cast<const char *>(data + view.value_offset), view.value_size);
    return true;
}

static bool GBE_ExtractProtoPackedUint32Field(const uint8 *data, size_t size, const GBE_ProtoFieldView &view, std::vector<uint32> &values)
{
    values.clear();
    if (!view.found || view.wire_type != 2u || !data || view.value_offset + view.value_size > size)
        return false;

    size_t offset = view.value_offset;
    const size_t end = view.value_offset + view.value_size;
    while (offset < end) {
        uint64 value = 0;
        if (!GBE_ReadVarUint64(data, end, offset, value))
            return false;
        values.push_back(static_cast<uint32>(value > 0xFFFFFFFFull ? 0xFFFFFFFFu : value));
    }

    return true;
}

static std::string GBE_FormatDota7034LeaverStateSummary(const std::string &input)
{
    uint32 lobby_state = 0;
    uint32 game_state = 0;
    uint32 leaver_detected = 0;
    uint32 first_blood_happened = 0;
    uint32 discard_match_results = 0;
    uint32 mass_disconnect = 0;

    GBE_ExtractProtoFieldUint32(reinterpret_cast<const uint8 *>(input.data()), input.size(), GBE_FindProtoField(reinterpret_cast<const uint8 *>(input.data()), input.size(), 1u), lobby_state);
    GBE_ExtractProtoFieldUint32(reinterpret_cast<const uint8 *>(input.data()), input.size(), GBE_FindProtoField(reinterpret_cast<const uint8 *>(input.data()), input.size(), 2u), game_state);
    GBE_ExtractProtoFieldUint32(reinterpret_cast<const uint8 *>(input.data()), input.size(), GBE_FindProtoField(reinterpret_cast<const uint8 *>(input.data()), input.size(), 3u), leaver_detected);
    GBE_ExtractProtoFieldUint32(reinterpret_cast<const uint8 *>(input.data()), input.size(), GBE_FindProtoField(reinterpret_cast<const uint8 *>(input.data()), input.size(), 4u), first_blood_happened);
    GBE_ExtractProtoFieldUint32(reinterpret_cast<const uint8 *>(input.data()), input.size(), GBE_FindProtoField(reinterpret_cast<const uint8 *>(input.data()), input.size(), 5u), discard_match_results);
    GBE_ExtractProtoFieldUint32(reinterpret_cast<const uint8 *>(input.data()), input.size(), GBE_FindProtoField(reinterpret_cast<const uint8 *>(input.data()), input.size(), 6u), mass_disconnect);

    char buffer[192];
    std::snprintf(
        buffer,
        sizeof(buffer),
        "lobby_state=%u game_state=%u leaver_detected=%u first_blood=%u discard=%u mass_disconnect=%u",
        lobby_state,
        game_state,
        leaver_detected,
        first_blood_happened,
        discard_match_results,
        mass_disconnect);
    return std::string(buffer);
}

static std::string GBE_FormatDota7034PlayerSummary(const std::string &input)
{
    uint64 steam_id = 0;
    uint32 hero_id = 0;
    uint32 disconnect_reason = 0;
    std::string leaver_state_raw;

    GBE_ExtractProtoFieldUint64(reinterpret_cast<const uint8 *>(input.data()), input.size(), GBE_FindProtoField(reinterpret_cast<const uint8 *>(input.data()), input.size(), 1u), steam_id);
    GBE_ExtractProtoFieldUint32(reinterpret_cast<const uint8 *>(input.data()), input.size(), GBE_FindProtoField(reinterpret_cast<const uint8 *>(input.data()), input.size(), 2u), hero_id);
    GBE_ExtractProtoFieldUint32(reinterpret_cast<const uint8 *>(input.data()), input.size(), GBE_FindProtoField(reinterpret_cast<const uint8 *>(input.data()), input.size(), 4u), disconnect_reason);
    GBE_ExtractProtoFieldBytes(reinterpret_cast<const uint8 *>(input.data()), input.size(), GBE_FindProtoField(reinterpret_cast<const uint8 *>(input.data()), input.size(), 3u), leaver_state_raw);

    std::ostringstream stream;
    stream << "steam_id=" << static_cast<unsigned long long>(steam_id)
           << " hero_id=" << hero_id
           << " disconnect_reason=" << disconnect_reason;
    if (!leaver_state_raw.empty())
        stream << " leaver_state{" << GBE_FormatDota7034LeaverStateSummary(leaver_state_raw) << '}';
    return stream.str();
}

static std::string GBE_FormatDota7034DraftSummary(const std::string &input)
{
    uint64 steam_id = 0;
    uint32 team = 0;
    uint32 team_slot = 0;

    GBE_ExtractProtoFieldUint64(reinterpret_cast<const uint8 *>(input.data()), input.size(), GBE_FindProtoField(reinterpret_cast<const uint8 *>(input.data()), input.size(), 1u), steam_id);
    GBE_ExtractProtoFieldUint32(reinterpret_cast<const uint8 *>(input.data()), input.size(), GBE_FindProtoField(reinterpret_cast<const uint8 *>(input.data()), input.size(), 2u), team);
    GBE_ExtractProtoFieldUint32(reinterpret_cast<const uint8 *>(input.data()), input.size(), GBE_FindProtoField(reinterpret_cast<const uint8 *>(input.data()), input.size(), 3u), team_slot);

    char buffer[128];
    std::snprintf(
        buffer,
        sizeof(buffer),
        "steam_id=%llu team=%u team_slot=%u",
        static_cast<unsigned long long>(steam_id),
        team,
        team_slot);
    return std::string(buffer);
}

static std::string GBE_FormatDota7034Summary(const uint8 *data, size_t size)
{
    if (!data || size == 0)
        return "empty";

    uint32 game_state = 0;
    uint32 send_reason = 0;
    uint32 radiant_kills = 0;
    uint32 dire_kills = 0;
    uint32 radiant_lead = 0;
    uint32 building_state = 0;
    uint32 connected_count = 0;
    uint32 disconnected_count = 0;
    uint32 draft_count = 0;
    std::string first_connected;
    std::string first_disconnected;
    std::string first_draft;

    size_t offset = 0;
    while (offset < size) {
        uint32 field_number = 0;
        uint32 wire_type = 0;
        size_t field_offset = 0;
        size_t value_offset = 0;
        size_t value_size = 0;
        size_t field_end = 0;
        if (!GBE_ReadNextProtoField(data, size, offset, field_number, wire_type, field_offset, value_offset, value_size, field_end))
            break;

        if (field_number == 1u && wire_type == 2u) {
            ++connected_count;
            if (first_connected.empty())
                first_connected = GBE_FormatDota7034PlayerSummary(std::string(reinterpret_cast<const char *>(data + value_offset), value_size));
            continue;
        }

        if (field_number == 7u && wire_type == 2u) {
            ++disconnected_count;
            if (first_disconnected.empty())
                first_disconnected = GBE_FormatDota7034PlayerSummary(std::string(reinterpret_cast<const char *>(data + value_offset), value_size));
            continue;
        }

        if (field_number == 16u && wire_type == 2u) {
            ++draft_count;
            if (first_draft.empty())
                first_draft = GBE_FormatDota7034DraftSummary(std::string(reinterpret_cast<const char *>(data + value_offset), value_size));
            continue;
        }

        GBE_ProtoFieldView view{ true, field_number, wire_type, value_offset, value_size };
        if (field_number == 2u)
            GBE_ExtractProtoFieldUint32(data, size, view, game_state);
        else if (field_number == 8u)
            GBE_ExtractProtoFieldUint32(data, size, view, send_reason);
        else if (field_number == 11u)
            GBE_ExtractProtoFieldUint32(data, size, view, radiant_kills);
        else if (field_number == 12u)
            GBE_ExtractProtoFieldUint32(data, size, view, dire_kills);
        else if (field_number == 14u)
            GBE_ExtractProtoFieldUint32(data, size, view, radiant_lead);
        else if (field_number == 15u)
            GBE_ExtractProtoFieldUint32(data, size, view, building_state);
    }

    std::ostringstream stream;
    stream << "game_state=" << game_state
           << " send_reason=" << send_reason
           << " connected=" << connected_count
           << " disconnected=" << disconnected_count
           << " drafts=" << draft_count
           << " radiant_kills=" << radiant_kills
           << " dire_kills=" << dire_kills
           << " radiant_lead=" << radiant_lead
           << " building_state=" << building_state;
    if (!first_connected.empty())
        stream << " connected0{" << first_connected << '}';
    if (!first_disconnected.empty())
        stream << " disconnected0{" << first_disconnected << '}';
    if (!first_draft.empty())
        stream << " draft0{" << first_draft << '}';
    return stream.str();
}

struct GBE_Dota7034RequestShape
{
    uint32 game_state{};
    uint32 send_reason{};
    uint32 first_blood_happened{};
    uint32 radiant_kills{};
    uint32 dire_kills{};
    uint32 radiant_lead{};
    uint32 building_state{};
    uint64 disconnected_steam_id{};
    uint32 disconnected_lobby_state{};
    uint32 disconnected_game_state{};
    bool has_game_state{};
    bool has_send_reason{};
    bool has_first_blood_happened{};
    bool has_radiant_kills{};
    bool has_dire_kills{};
    bool has_radiant_lead{};
    bool has_building_state{};
    bool has_disconnected_player{};
    bool has_disconnected_steam_id{};
    bool has_disconnected_lobby_state{};
    bool has_disconnected_game_state{};
};

struct GBE_DotaEmptyRequestShape
{
    bool valid{};
    uint32 field_count{};
};

struct GBE_DotaRankRequestShape
{
    bool valid{};
    uint32 field_count{};
    uint32 rank_type{};
    bool has_rank_type{};
};

static GBE_Dota7034RequestShape GBE_ParseDota7034RequestShape(const uint8 *data, size_t size)
{
    GBE_Dota7034RequestShape shape{};
    if (!data || size == 0)
        return shape;

    GBE_ProtoFieldView view = GBE_FindProtoField(data, size, 2u);
    if (GBE_ExtractProtoFieldUint32(data, size, view, shape.game_state))
        shape.has_game_state = true;

    view = GBE_FindProtoField(data, size, 8u);
    if (GBE_ExtractProtoFieldUint32(data, size, view, shape.send_reason))
        shape.has_send_reason = true;

    view = GBE_FindProtoField(data, size, 6u);
    if (GBE_ExtractProtoFieldUint32(data, size, view, shape.first_blood_happened))
        shape.has_first_blood_happened = true;

    view = GBE_FindProtoField(data, size, 11u);
    if (GBE_ExtractProtoFieldUint32(data, size, view, shape.radiant_kills))
        shape.has_radiant_kills = true;

    view = GBE_FindProtoField(data, size, 12u);
    if (GBE_ExtractProtoFieldUint32(data, size, view, shape.dire_kills))
        shape.has_dire_kills = true;

    view = GBE_FindProtoField(data, size, 14u);
    if (GBE_ExtractProtoFieldUint32(data, size, view, shape.radiant_lead))
        shape.has_radiant_lead = true;

    view = GBE_FindProtoField(data, size, 15u);
    if (GBE_ExtractProtoFieldUint32(data, size, view, shape.building_state))
        shape.has_building_state = true;

    std::string disconnected_player_raw;
    if (GBE_ExtractProtoFieldBytes(data, size, GBE_FindProtoField(data, size, 7u), disconnected_player_raw) && !disconnected_player_raw.empty()) {
        shape.has_disconnected_player = true;

        const uint8 *player_data = reinterpret_cast<const uint8 *>(disconnected_player_raw.data());
        const size_t player_size = disconnected_player_raw.size();
        uint64 disconnected_steam_id = 0;
        if (GBE_ExtractProtoFieldUint64(player_data, player_size, GBE_FindProtoField(player_data, player_size, 1u), disconnected_steam_id)) {
            shape.disconnected_steam_id = disconnected_steam_id;
            shape.has_disconnected_steam_id = true;
        }

        std::string leaver_state_raw;
        if (GBE_ExtractProtoFieldBytes(player_data, player_size, GBE_FindProtoField(player_data, player_size, 3u), leaver_state_raw) && !leaver_state_raw.empty()) {
            const uint8 *leaver_state_data = reinterpret_cast<const uint8 *>(leaver_state_raw.data());
            const size_t leaver_state_size = leaver_state_raw.size();
            if (GBE_ExtractProtoFieldUint32(leaver_state_data, leaver_state_size, GBE_FindProtoField(leaver_state_data, leaver_state_size, 1u), shape.disconnected_lobby_state))
                shape.has_disconnected_lobby_state = true;
            if (GBE_ExtractProtoFieldUint32(leaver_state_data, leaver_state_size, GBE_FindProtoField(leaver_state_data, leaver_state_size, 2u), shape.disconnected_game_state))
                shape.has_disconnected_game_state = true;
        }
    }

    return shape;
}

static std::string GBE_BuildDota7034LeaverStatePayload(uint32 lobby_state, uint32 game_state)
{
    std::string leaver_state;
    GBE_AppendProtoVarIntField(leaver_state, 1u, lobby_state);
    GBE_AppendProtoVarIntField(leaver_state, 2u, game_state);
    GBE_AppendProtoVarIntField(leaver_state, 3u, 0u);
    GBE_AppendProtoVarIntField(leaver_state, 4u, 0u);
    GBE_AppendProtoVarIntField(leaver_state, 5u, 0u);
    GBE_AppendProtoVarIntField(leaver_state, 6u, 0u);
    return leaver_state;
}

static GBE_DotaEmptyRequestShape GBE_ParseDotaEmptyRequestShape(const uint8 *data, size_t size)
{
    GBE_DotaEmptyRequestShape shape{};
    shape.valid = true;
    if (!data || size == 0)
        return shape;

    size_t offset = 0;
    while (offset < size) {
        uint32 field_number = 0;
        uint32 wire_type = 0;
        size_t field_offset = 0;
        size_t value_offset = 0;
        size_t value_size = 0;
        size_t field_end = 0;
        if (!GBE_ReadNextProtoField(data, size, offset, field_number, wire_type, field_offset, value_offset, value_size, field_end)) {
            shape.valid = false;
            break;
        }
        ++shape.field_count;
    }

    return shape;
}

static GBE_DotaRankRequestShape GBE_ParseDotaRankRequestShape(const uint8 *data, size_t size)
{
    GBE_DotaRankRequestShape shape{};
    shape.valid = true;
    if (!data || size == 0)
        return shape;

    size_t offset = 0;
    while (offset < size) {
        uint32 field_number = 0;
        uint32 wire_type = 0;
        size_t field_offset = 0;
        size_t value_offset = 0;
        size_t value_size = 0;
        size_t field_end = 0;
        if (!GBE_ReadNextProtoField(data, size, offset, field_number, wire_type, field_offset, value_offset, value_size, field_end)) {
            shape.valid = false;
            break;
        }

        ++shape.field_count;
        if (field_number == 1u) {
            GBE_ProtoFieldView view{ true, field_number, wire_type, value_offset, value_size };
            if (GBE_ExtractProtoFieldUint32(data, size, view, shape.rank_type))
                shape.has_rank_type = true;
        }
    }

    return shape;
}

static bool GBE_IsDotaRankTypeSupported(uint32 rank_type)
{
    switch (rank_type) {
        case 1u:
        case 2u:
        case 3u:
        case 4u:
        case 5u:
        case 6u:
        case 100u:
        case 101u:
            return true;
        default:
            return false;
    }
}

static bool GBE_RewriteDotaLobbyTemplateMemberObject(
    const std::string &input,
    uint32 account_id,
    uint64 steam_id,
    uint32 owner_team,
    uint32 owner_slot,
    std::string &output)
{
    output.clear();
    bool saw_team = false;
    bool saw_slot = false;
    bool saw_leaver_status = false;
    bool saw_leaver_actions = false;

    size_t offset = 0;
    while (offset < input.size()) {
        uint32 field_number = 0;
        uint32 wire_type = 0;
        size_t field_offset = 0;
        size_t value_offset = 0;
        size_t value_size = 0;
        size_t field_end = 0;
        if (!GBE_ReadNextProtoField(
                reinterpret_cast<const uint8 *>(input.data()),
                input.size(),
                offset,
                field_number,
                wire_type,
                field_offset,
                value_offset,
                value_size,
                field_end))
            return false;

        if (field_number == 1u) {
            if (wire_type == 1u) {
                GBE_AppendProtoFixed64Field(output, 1u, steam_id);
                continue;
            }

            if (wire_type == 0u) {
                GBE_AppendProtoVarIntField(output, 1u, account_id);
                continue;
            }
        }

        if (field_number == 55u && wire_type == 0u) {
            GBE_AppendProtoVarIntField(output, 55u, account_id);
            continue;
        }

        if (field_number == 16u && wire_type == 0u) {
            saw_leaver_status = true;
            GBE_AppendProtoVarIntField(output, 16u, 0u);
            continue;
        }

        if (field_number == 3u && wire_type == 0u) {
            saw_team = true;
            GBE_AppendProtoVarIntField(output, 3u, owner_team <= 1u ? owner_team : 0u);
            continue;
        }

        if (field_number == 7u && wire_type == 0u) {
            saw_slot = true;
            GBE_AppendProtoVarIntField(output, 7u, owner_slot);
            continue;
        }

        if (field_number == 28u && wire_type == 0u) {
            saw_leaver_actions = true;
            GBE_AppendProtoVarIntField(output, 28u, 0u);
            continue;
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    if (!saw_team)
        GBE_AppendProtoVarIntField(output, 3u, owner_team <= 1u ? owner_team : 0u);

    if (!saw_slot)
        GBE_AppendProtoVarIntField(output, 7u, owner_slot);

    if (!saw_leaver_status)
        GBE_AppendProtoVarIntField(output, 16u, 0u);

    if (!saw_leaver_actions)
        GBE_AppendProtoVarIntField(output, 28u, 0u);

    return true;
}

static bool GBE_ParseDotaPracticeLobbySetDetailsBody(const uint8 *body, size_t body_size, GBE_DotaPracticeLobbyDetailsRequest &request)
{
    request = {};

    if (!body || body_size == 0)
        return false;

    uint64 value = 0;
    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 1), value)) {
        request.has_lobby_id = true;
        request.lobby_id = value;
    }

    if (GBE_ExtractProtoFieldBytes(body, body_size, GBE_FindProtoField(body, body_size, 2), request.room_name))
        request.has_room_name = true;

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 4), value)) {
        request.has_server_region = true;
        request.server_region = static_cast<uint32>(value);
    }

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 25), value)) {
        request.has_lan = true;
        request.lan = (value != 0);
    }

    if (GBE_ExtractProtoFieldBytes(body, body_size, GBE_FindProtoField(body, body_size, 48), request.lan_host_ping_location))
        request.has_lan_host_ping_location = true;

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 5), value)) {
        request.has_game_mode = true;
        request.game_mode = static_cast<uint32>(value);
    }

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 9), value)) {
        request.has_bot_difficulty_radiant = true;
        request.bot_difficulty_radiant = static_cast<uint32>(value);
    }

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 10), value)) {
        request.has_allow_cheats = true;
        request.allow_cheats = (value != 0);
    }

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 11), value)) {
        request.has_fill_with_bots = true;
        request.fill_with_bots = (value != 0);
    }

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 13), value)) {
        request.has_allow_spectating = true;
        request.allow_spectating = (value != 0);
    }

    if (GBE_ExtractProtoFieldBytes(body, body_size, GBE_FindProtoField(body, body_size, 15), request.pass_key))
        request.has_pass_key = true;

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 33), value)) {
        request.has_visibility = true;
        request.visibility = static_cast<uint32>(value);
    }

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 43), value)) {
        request.has_bot_difficulty_dire = true;
        request.bot_difficulty_dire = static_cast<uint32>(value);
    }

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 44), value)) {
        request.has_bot_radiant = true;
        request.bot_radiant = value;
    }

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 45), value)) {
        request.has_bot_dire = true;
        request.bot_dire = value;
    }

    return request.has_lobby_id;
}

static bool GBE_ParseDotaPracticeLobbySetTeamSlotBody(const uint8 *body, size_t body_size, GBE_DotaPracticeLobbySetTeamSlotRequest &request)
{
    request = {};

    uint64 value = 0;
    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 1), value)) {
        request.has_team = true;
        request.team = static_cast<uint32>(value);
    }

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 2), value)) {
        request.has_slot = true;
        request.slot = static_cast<uint32>(value);
    }

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 3), value)) {
        request.has_bot_difficulty = true;
        request.bot_difficulty = static_cast<uint32>(value);
    }

    return request.has_team || request.has_slot || request.has_bot_difficulty;
}

static bool GBE_ParseDotaPracticeLobbyCreateBody(const uint8 *body, size_t body_size, GBE_DotaPracticeLobbyCreateRequest &request)
{
    request = {};

    if (!body || body_size == 0)
        return false;

    if (GBE_ExtractProtoFieldBytes(body, body_size, GBE_FindProtoField(body, body_size, 5), request.pass_key))
        request.has_pass_key = true;

    std::string lobby_details_raw;
    if (GBE_ExtractProtoFieldBytes(body, body_size, GBE_FindProtoField(body, body_size, 7), lobby_details_raw)) {
        request.has_lobby_details = GBE_ParseDotaPracticeLobbySetDetailsBody(
            reinterpret_cast<const uint8 *>(lobby_details_raw.data()),
            lobby_details_raw.size(),
            request.lobby_details
        );
    }

    return request.has_lobby_details || request.has_pass_key;
}

static bool GBE_ParseDotaPracticeLobbyJoinBroadcastChannelBody(const uint8 *body, size_t body_size, GBE_DotaPracticeLobbyBroadcastChannelRequest &request)
{
    request = {};

    uint64 value = 0;
    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 1), value)) {
        request.has_channel = true;
        request.channel = static_cast<uint32>(value);
    }

    if (GBE_ExtractProtoFieldBytes(body, body_size, GBE_FindProtoField(body, body_size, 2), request.description))
        request.has_description = true;
    if (GBE_ExtractProtoFieldBytes(body, body_size, GBE_FindProtoField(body, body_size, 3), request.country_code))
        request.has_country_code = true;
    if (GBE_ExtractProtoFieldBytes(body, body_size, GBE_FindProtoField(body, body_size, 4), request.language_code))
        request.has_language_code = true;

    return request.has_channel;
}

static bool GBE_ParseDotaLobbyUpdateBroadcastChannelInfoBody(const uint8 *body, size_t body_size, GBE_DotaPracticeLobbyBroadcastChannelRequest &request)
{
    request = {};

    uint64 value = 0;
    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 1), value)) {
        request.has_channel = true;
        request.channel = static_cast<uint32>(value);
    }

    if (GBE_ExtractProtoFieldBytes(body, body_size, GBE_FindProtoField(body, body_size, 2), request.country_code))
        request.has_country_code = true;
    if (GBE_ExtractProtoFieldBytes(body, body_size, GBE_FindProtoField(body, body_size, 3), request.description))
        request.has_description = true;
    if (GBE_ExtractProtoFieldBytes(body, body_size, GBE_FindProtoField(body, body_size, 4), request.language_code))
        request.has_language_code = true;

    return request.has_channel;
}

static bool GBE_ParseDotaPracticeLobbyCloseBroadcastChannelBody(const uint8 *body, size_t body_size, GBE_DotaPracticeLobbyBroadcastChannelRequest &request)
{
    request = {};

    uint64 value = 0;
    if (!GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 1), value))
        return false;

    request.has_channel = true;
    request.channel = static_cast<uint32>(value);
    return true;
}

static bool GBE_ParseDotaJoinChatChannelBody(const uint8 *body, size_t body_size, GBE_DotaJoinChatChannelRequest &request)
{
    request = {};

    if (!body || body_size == 0)
        return false;

    uint64 value = 0;
    if (GBE_ExtractProtoFieldBytes(body, body_size, GBE_FindProtoField(body, body_size, 2), request.channel_name))
        request.has_channel_name = true;

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 4), value)) {
        request.has_channel_type = true;
        request.channel_type = static_cast<uint32>(value);
    }

    return request.has_channel_name;
}

static bool GBE_ParseDotaLeaveChatChannelBody(const uint8 *body, size_t body_size, GBE_DotaLeaveChatChannelRequest &request)
{
    request = {};

    if (!body || body_size == 0)
        return false;

    uint64 value = 0;
    if (!GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 1), value))
        return false;

    request.has_channel_id = true;
    request.channel_id = value;
    return true;
}

static bool GBE_ParseDirectProtoContext(const void *pubData, uint32 cubData, ProtoBufMsgHeader_t &hdr, CMsgProtoBufHeader &protohdr, const uint8 *&body, size_t &body_size)
{
    hdr = {};
    protohdr.Clear();
    body = nullptr;
    body_size = 0;

    if (!pubData || cubData < sizeof(ProtoBufMsgHeader_t))
        return false;

    const uint8 *bytes = reinterpret_cast<const uint8 *>(pubData);
    std::memcpy(&hdr, bytes, sizeof(hdr));

    const size_t body_offset = sizeof(hdr) + hdr.m_cubProtoBufExtHdr;
    if (body_offset > cubData)
        return false;

    if (hdr.m_cubProtoBufExtHdr != 0 && !protohdr.ParseFromArray(bytes + sizeof(hdr), hdr.m_cubProtoBufExtHdr))
        return false;

    body = bytes + body_offset;
    body_size = static_cast<size_t>(cubData - body_offset);
    return true;
}

static bool GBE_PatchDotaTemplateIdentifiers(
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
        std::vector<uint8> encoded_steam_id;
        if (!GBE_EncodeVarUint64WithExpectedSize(steam_id, GBE_kOldDotaSteamIdVarint.size(), encoded_steam_id))
            return false;
        if (!GBE_FindAndOverwriteBytes(message, GBE_VectorFromBytes(GBE_kOldDotaSteamIdVarint.data(), GBE_kOldDotaSteamIdVarint.size()), encoded_steam_id))
            return false;
    }

    return true;
}

static bool GBE_PatchDotaLobbyTemplateIdentifiers(std::string &message, uint32 account_id, uint64 steam_id, uint64 lobby_id)
{
    (void)account_id;
    std::vector<uint8> encoded_lobby_id;
    if (!GBE_EncodeVarUint64WithExpectedSize(lobby_id, GBE_kOldDotaLobbyIdVarint.size(), encoded_lobby_id))
        return false;
    const std::vector<uint8> old_lobby_id = GBE_VectorFromBytes(GBE_kOldDotaLobbyIdVarint.data(), GBE_kOldDotaLobbyIdVarint.size());
    const size_t lobby_id_match_count = GBE_CountBytePatternMatches(message, old_lobby_id);
    if (!GBE_FindAndOverwriteBytes(message, GBE_VectorFromBytes(GBE_kOldDotaLobbyIdVarint.data(), GBE_kOldDotaLobbyIdVarint.size()), encoded_lobby_id)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed replacing lobby_id bytes lobby_id=%llu", static_cast<unsigned long long>(lobby_id));
        return false;
    }

    std::string steam_id_fixed64_raw;
    GBE_AppendLittleEndian64(steam_id_fixed64_raw, steam_id);
    const std::vector<uint8> old_steam_id_fixed64 = GBE_VectorFromBytes(GBE_kOldDotaSteamIdFixed64.data(), GBE_kOldDotaSteamIdFixed64.size());
    const size_t steam_id_fixed64_match_count = GBE_CountBytePatternMatches(message, old_steam_id_fixed64);
    if (!GBE_FindAndOverwriteBytes(message, GBE_VectorFromBytes(GBE_kOldDotaSteamIdFixed64.data(), GBE_kOldDotaSteamIdFixed64.size()), GBE_VectorFromBytes(reinterpret_cast<const uint8 *>(steam_id_fixed64_raw.data()), steam_id_fixed64_raw.size()))) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed replacing steam_id fixed64 bytes steam_id=%llu", static_cast<unsigned long long>(steam_id));
        return false;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Patched template LobbyID matches=%zu SteamIDFixed64 matches=%zu body_prefix=%s",
        lobby_id_match_count,
        steam_id_fixed64_match_count,
        GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(message.data()), message.size(), 32).c_str()
    );
    return true;
}

static bool GBE_RewriteDotaLobbyTemplateObject2004(
    const std::string &input,
    uint32 account_id,
    uint64 steam_id,
    bool rewrite_runtime_fields,
    uint32 lobby_state,
    uint32 lobby_game_state,
    uint64 server_id,
    uint64 match_id,
    uint32 game_start_time,
    const std::string &connect,
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
    const std::string &pass_key,
    std::string &output)
{
    output.clear();
    bool saw_state = false;
    bool saw_connect = false;
    bool saw_server_id = false;
    bool saw_game_state = false;
    bool saw_match_id = false;
    bool saw_game_start_time = false;
    bool saw_lan = false;
    bool saw_lan_host_ping_location = false;

    size_t offset = 0;
    while (offset < input.size()) {
        uint32 field_number = 0;
        uint32 wire_type = 0;
        size_t field_offset = 0;
        size_t value_offset = 0;
        size_t value_size = 0;
        size_t field_end = 0;
        if (!GBE_ReadNextProtoField(
                reinterpret_cast<const uint8 *>(input.data()),
                input.size(),
                offset,
                field_number,
                wire_type,
                field_offset,
                value_offset,
                value_size,
                field_end))
            return false;

        if (field_number == 3u && wire_type == 0u) {
            GBE_AppendProtoVarIntField(output, 3u, game_mode);
            continue;
        }

        if (field_number == 4u && wire_type == 0u) {
            saw_state = true;
            if (rewrite_runtime_fields) {
                GBE_AppendProtoVarIntField(output, 4u, lobby_state);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 5u && wire_type == 2u) {
            saw_connect = true;
            if (rewrite_runtime_fields) {
                GBE_AppendProtoBytesField(output, 5u, connect);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 6u && wire_type == 1u) {
            saw_server_id = true;
            if (rewrite_runtime_fields) {
                GBE_AppendProtoFixed64Field(output, 6u, server_id);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 13u && wire_type == 0u) {
            GBE_AppendProtoVarIntField(output, 13u, allow_cheats ? 1u : 0u);
            continue;
        }

        if (field_number == 11u && wire_type == 1u) {
            GBE_AppendProtoFixed64Field(output, 11u, steam_id);
            continue;
        }

        if (field_number == 14u && wire_type == 0u) {
            GBE_AppendProtoVarIntField(output, 14u, fill_with_bots ? 1u : 0u);
            continue;
        }

        if (field_number == 16u && wire_type == 2u) {
            GBE_AppendProtoBytesField(output, 16u, room_name);
            continue;
        }

        if (field_number == 21u && wire_type == 0u) {
            GBE_AppendProtoVarIntField(output, 21u, server_region);
            continue;
        }

        if (field_number == 22u && wire_type == 0u) {
            saw_game_state = true;
            if (rewrite_runtime_fields) {
                GBE_AppendProtoVarIntField(output, 22u, lobby_game_state);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 31u && wire_type == 0u) {
            GBE_AppendProtoVarIntField(output, 31u, allow_spectating ? 1u : 0u);
            continue;
        }

        if (field_number == 30u && wire_type == 0u) {
            saw_match_id = true;
            if (rewrite_runtime_fields) {
                GBE_AppendProtoVarIntField(output, 30u, match_id);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 36u && wire_type == 0u) {
            GBE_AppendProtoVarIntField(output, 36u, bot_difficulty_radiant);
            continue;
        }

        if (field_number == 39u && wire_type == 2u) {
            GBE_AppendProtoBytesField(output, 39u, pass_key);
            continue;
        }

        if (field_number == 57u && wire_type == 0u) {
            saw_lan = true;
            GBE_AppendProtoVarIntField(output, 57u, lan ? 1u : 0u);
            continue;
        }

        if (field_number == 75u && wire_type == 0u) {
            GBE_AppendProtoVarIntField(output, 75u, visibility);
            continue;
        }

        if (field_number == 87u && wire_type == 0u) {
            saw_game_start_time = true;
            if (rewrite_runtime_fields) {
                GBE_AppendProtoVarIntField(output, 87u, game_start_time);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 93u && wire_type == 0u) {
            GBE_AppendProtoVarIntField(output, 93u, bot_difficulty_dire);
            continue;
        }

        if (field_number == 94u && wire_type == 0u) {
            GBE_AppendProtoVarIntField(output, 94u, bot_radiant);
            continue;
        }

        if (field_number == 95u && wire_type == 0u) {
            GBE_AppendProtoVarIntField(output, 95u, bot_dire);
            continue;
        }

        if (field_number == 109u && wire_type == 2u) {
            saw_lan_host_ping_location = true;
            if (!lan_host_ping_location.empty())
                GBE_AppendProtoBytesField(output, 109u, lan_host_ping_location);
            continue;
        }

        if (field_number == 120u && wire_type == 2u) {
            if (!rewrite_runtime_fields) {
                output.append(input.data() + field_offset, field_end - field_offset);
                continue;
            }

            std::string rewritten_member;
            if (!GBE_RewriteDotaLobbyTemplateMemberObject(
                    std::string(input.data() + value_offset, value_size),
                    account_id,
                    steam_id,
                    owner_team,
                    owner_slot,
                    rewritten_member))
                return false;
            GBE_AppendProtoBytesField(output, 120u, rewritten_member);
            continue;
        }

        if (field_number == 121u && wire_type == 0u) {
            output.append(input.data() + field_offset, field_end - field_offset);
            continue;
        }

        if ((field_number == 122u || field_number == 123u || field_number == 124u) && wire_type == 0u) {
            output.append(input.data() + field_offset, field_end - field_offset);
            continue;
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    if (rewrite_runtime_fields && !saw_state)
        GBE_AppendProtoVarIntField(output, 4u, lobby_state);
    if (rewrite_runtime_fields && !saw_connect && !connect.empty())
        GBE_AppendProtoBytesField(output, 5u, connect);
    if (rewrite_runtime_fields && !saw_server_id && server_id != 0)
        GBE_AppendProtoFixed64Field(output, 6u, server_id);
    if (!saw_lan)
        GBE_AppendProtoVarIntField(output, 57u, lan ? 1u : 0u);
    if (rewrite_runtime_fields && !saw_game_state)
        GBE_AppendProtoVarIntField(output, 22u, lobby_game_state);
    if (rewrite_runtime_fields && !saw_match_id && match_id != 0)
        GBE_AppendProtoVarIntField(output, 30u, match_id);
    if (!saw_lan_host_ping_location && !lan_host_ping_location.empty())
        GBE_AppendProtoBytesField(output, 109u, lan_host_ping_location);
    if (rewrite_runtime_fields && !saw_game_start_time && game_start_time != 0)
        GBE_AppendProtoVarIntField(output, 87u, game_start_time);

    return true;
}

static bool GBE_RewriteDotaLobbyTemplateObject2015(const std::string &, std::string &output)
{
    output.clear();
    return true;
}

static bool GBE_RewriteDotaLobbyTemplateObject2014(const std::string &input, const std::string &player_name, std::string &output)
{
    output.clear();

    size_t offset = 0;
    while (offset < input.size()) {
        uint32 field_number = 0;
        uint32 wire_type = 0;
        size_t field_offset = 0;
        size_t value_offset = 0;
        size_t value_size = 0;
        size_t field_end = 0;
        if (!GBE_ReadNextProtoField(
                reinterpret_cast<const uint8 *>(input.data()),
                input.size(),
                offset,
                field_number,
                wire_type,
                field_offset,
                value_offset,
                value_size,
                field_end))
            return false;

        if (field_number == 1u && wire_type == 2u) {
            std::string rewritten_member;
            size_t member_offset = 0;
            while (member_offset < value_size) {
                uint32 member_field = 0;
                uint32 member_wire = 0;
                size_t member_field_offset = 0;
                size_t member_value_offset = 0;
                size_t member_value_size = 0;
                size_t member_field_end = 0;
                if (!GBE_ReadNextProtoField(
                        reinterpret_cast<const uint8 *>(input.data()) + value_offset,
                        value_size,
                        member_offset,
                        member_field,
                        member_wire,
                        member_field_offset,
                        member_value_offset,
                        member_value_size,
                        member_field_end))
                    return false;

                if (member_field == 1u && member_wire == 2u) {
                    GBE_AppendProtoBytesField(rewritten_member, 1u, player_name);
                    continue;
                }

                rewritten_member.append(input.data() + value_offset + member_field_offset, member_field_end - member_field_offset);
            }

            GBE_AppendProtoBytesField(output, 1u, rewritten_member);
            continue;
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    return true;
}

static bool GBE_RewriteDotaLobbyTemplateObject2016(
    const std::string &input,
    uint32 account_id,
    uint64 steam_id,
    uint32 owner_team,
    uint32 owner_slot,
    std::string &output)
{
    (void)account_id;
    (void)owner_team;
    (void)owner_slot;
    output.clear();

    size_t offset = 0;
    while (offset < input.size()) {
        uint32 field_number = 0;
        uint32 wire_type = 0;
        size_t field_offset = 0;
        size_t value_offset = 0;
        size_t value_size = 0;
        size_t field_end = 0;
        if (!GBE_ReadNextProtoField(
                reinterpret_cast<const uint8 *>(input.data()),
                input.size(),
                offset,
                field_number,
                wire_type,
                field_offset,
                value_offset,
                value_size,
                field_end))
            return false;

        if (field_number == 1u && wire_type == 2u) {
            std::string rewritten_member;
            size_t member_offset = 0;
            while (member_offset < value_size) {
                uint32 member_field = 0;
                uint32 member_wire = 0;
                size_t member_field_offset = 0;
                size_t member_value_offset = 0;
                size_t member_value_size = 0;
                size_t member_field_end = 0;
                if (!GBE_ReadNextProtoField(
                        reinterpret_cast<const uint8 *>(input.data()) + value_offset,
                        value_size,
                        member_offset,
                        member_field,
                        member_wire,
                        member_field_offset,
                        member_value_offset,
                        member_value_size,
                        member_field_end))
                    return false;

                if (member_field == 1u && member_wire == 1u) {
                    GBE_AppendProtoFixed64Field(rewritten_member, 1u, steam_id);
                    continue;
                }

                rewritten_member.append(input.data() + value_offset + member_field_offset, member_field_end - member_field_offset);
            }

            GBE_AppendProtoBytesField(output, 1u, rewritten_member);
            continue;
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    return true;
}

static bool GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
    std::string &message,
    uint32 account_id,
    uint64 steam_id,
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
    bool rewrite_empty_2015,
    const std::string &pass_key)
{
    if (message.size() < sizeof(ProtoBufMsgHeader_t))
        return false;

    ProtoBufMsgHeader_t hdr{};
    std::memcpy(&hdr, message.data(), sizeof(hdr));
    const size_t body_offset = sizeof(hdr) + hdr.m_cubProtoBufExtHdr;
    if (body_offset > message.size())
        return false;

    const std::string body = message.substr(body_offset);
    std::string rewritten_body;

    size_t offset = 0;
    while (offset < body.size()) {
        uint32 field_number = 0;
        uint32 wire_type = 0;
        size_t field_offset = 0;
        size_t value_offset = 0;
        size_t value_size = 0;
        size_t field_end = 0;
        if (!GBE_ReadNextProtoField(
                reinterpret_cast<const uint8 *>(body.data()),
                body.size(),
                offset,
                field_number,
                wire_type,
                field_offset,
                value_offset,
                value_size,
                field_end))
            return false;

        if (field_number != 2u || wire_type != 2u) {
            rewritten_body.append(body.data() + field_offset, field_end - field_offset);
            continue;
        }

        const std::string subscribed = body.substr(value_offset, value_size);
        uint64 type_id = 0;
        if (!GBE_ExtractProtoFieldUint64(
                reinterpret_cast<const uint8 *>(subscribed.data()),
                subscribed.size(),
                GBE_FindProtoField(reinterpret_cast<const uint8 *>(subscribed.data()), subscribed.size(), 1u),
                type_id)) {
            rewritten_body.append(body.data() + field_offset, field_end - field_offset);
            continue;
        }

        const bool should_rewrite_type = rewrite_runtime_fields
            ? (type_id == 2004u || type_id == 2014u || type_id == 2016u || (rewrite_empty_2015 && type_id == 2015u))
            : (type_id == 2004u || type_id == 2014u);
        if (!should_rewrite_type) {
            rewritten_body.append(body.data() + field_offset, field_end - field_offset);
            continue;
        }

        std::string rewritten_subscribed;
        size_t subscribed_offset = 0;
        while (subscribed_offset < subscribed.size()) {
            uint32 subscribed_field = 0;
            uint32 subscribed_wire = 0;
            size_t subscribed_field_offset = 0;
            size_t subscribed_value_offset = 0;
            size_t subscribed_value_size = 0;
            size_t subscribed_field_end = 0;
            if (!GBE_ReadNextProtoField(
                    reinterpret_cast<const uint8 *>(subscribed.data()),
                    subscribed.size(),
                    subscribed_offset,
                    subscribed_field,
                    subscribed_wire,
                    subscribed_field_offset,
                    subscribed_value_offset,
                    subscribed_value_size,
                    subscribed_field_end))
                return false;

            if (subscribed_field == 2u && subscribed_wire == 2u) {
                std::string rewritten_object;
                const std::string object_data = subscribed.substr(subscribed_value_offset, subscribed_value_size);
                const bool ok = (type_id == 2004u)
                    ? GBE_RewriteDotaLobbyTemplateObject2004(
                        object_data,
                        account_id,
                        steam_id,
                        rewrite_runtime_fields,
                        lobby_state,
                        lobby_game_state,
                        server_id,
                        match_id,
                        game_start_time,
                        connect,
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
                        pass_key,
                        rewritten_object)
                    : (type_id == 2014u)
                        ? GBE_RewriteDotaLobbyTemplateObject2014(object_data, player_name, rewritten_object)
                    : (type_id == 2015u)
                            ? GBE_RewriteDotaLobbyTemplateObject2015(object_data, rewritten_object)
                            : GBE_RewriteDotaLobbyTemplateObject2016(object_data, account_id, steam_id, owner_team, owner_slot, rewritten_object);
                if (!ok)
                    return false;
                GBE_AppendProtoBytesField(rewritten_subscribed, 2u, rewritten_object);
                continue;
            }

            rewritten_subscribed.append(subscribed.data() + subscribed_field_offset, subscribed_field_end - subscribed_field_offset);
        }

        GBE_AppendProtoBytesField(rewritten_body, 2u, rewritten_subscribed);
    }

    message.resize(body_offset);
    message.append(rewritten_body);
    return true;
}

static uint64 GBE_GenerateDotaLobbyId()
{
    std::random_device device;
    std::mt19937_64 generator(
        (static_cast<uint64>(device()) << 32) ^
        static_cast<uint64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );

    for (int attempt = 0; attempt < 64; ++attempt) {
        const uint64 candidate = (generator() & 0x00FFFFFFFFFFFFFFull) | 0x0002000000000000ull;
        std::vector<uint8> encoded;
        if (candidate != 0 && GBE_EncodeVarUint64WithExpectedSize(candidate, GBE_kOldDotaLobbyIdVarint.size(), encoded))
            return candidate;
    }

    return 29799760111995806ull;
}

static uint64 GBE_GenerateDotaChatChannelId()
{
    std::random_device device;
    std::mt19937_64 generator(
        (static_cast<uint64>(device()) << 32) ^
        static_cast<uint64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );

    const uint64 candidate = 0x10000ull + (generator() & 0x00000000000FFFFFull);
    return candidate != 0 ? candidate : 0x1664Eull;
}

static uint64 GBE_GenerateDotaMatchId()
{
    std::random_device device;
    std::mt19937_64 generator(
        (static_cast<uint64>(device()) << 32) ^
        static_cast<uint64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );

    for (int attempt = 0; attempt < 128; ++attempt) {
        const uint64 candidate = 0x100000000ull + (generator() & 0x00000003FFFFFFFFull);
        std::vector<uint8> encoded;
        if (candidate != 0 && GBE_EncodeVarUint64WithExpectedSize(candidate, GBE_kOldDotaPracticeLobbyMatchIdVarint.size(), encoded))
            return candidate;
    }

    return 8781757536ull;
}

static const char *GBE_GetDotaPracticeLobbyLaunchStageHex(size_t stage_index)
{
    switch (stage_index) {
        case 0: return GBE_kDotaPracticeLobbyLaunchStage1Hex;
        case 1: return GBE_kDotaPracticeLobbyLaunchStage2Hex;
        case 2: return GBE_kDotaPracticeLobbyLaunchStage3Hex;
        case 3: return GBE_kDotaPracticeLobbyLaunchStage4Hex;
        default: return nullptr;
    }
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

    std::string steam_id_fixed64_raw;
    GBE_AppendLittleEndian64(steam_id_fixed64_raw, steam_id);
    if (!GBE_FindAndOverwriteBytes(
            message,
            GBE_VectorFromBytes(GBE_kOldDotaSteamIdFixed64.data(), GBE_kOldDotaSteamIdFixed64.size()),
            GBE_VectorFromBytes(reinterpret_cast<const uint8 *>(steam_id_fixed64_raw.data()), steam_id_fixed64_raw.size()))) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Launch steam_id fixed64 patch failed stage=%s steam_id=%llu", stage_note ? stage_note : "", static_cast<unsigned long long>(steam_id));
        return false;
    }

    if (!GBE_TryPatchDotaAccountIdFixed32(message, account_id, "GC_DOTA_PATCH")) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Launch account_id fixed32 patch failed stage=%s account_id=%u", stage_note ? stage_note : "", account_id);
        return false;
    }

    std::vector<uint8> encoded_lobby_id;
    if (!GBE_EncodeVarUint64WithExpectedSize(lobby_id, GBE_kOldDotaPracticeLobbyLobbyIdVarint.size(), encoded_lobby_id)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Launch lobby_id size mismatch stage=%s lobby_id=%llu", stage_note ? stage_note : "", static_cast<unsigned long long>(lobby_id));
        return false;
    }
    if (!GBE_FindAndOverwriteBytes(message, GBE_VectorFromBytes(GBE_kOldDotaPracticeLobbyLobbyIdVarint.data(), GBE_kOldDotaPracticeLobbyLobbyIdVarint.size()), encoded_lobby_id)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Launch lobby_id patch failed stage=%s lobby_id=%llu", stage_note ? stage_note : "", static_cast<unsigned long long>(lobby_id));
        return false;
    }

    std::vector<uint8> encoded_match_id;
    if (!GBE_EncodeVarUint64WithExpectedSize(match_id, GBE_kOldDotaPracticeLobbyMatchIdVarint.size(), encoded_match_id)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Launch match_id size mismatch stage=%s match_id=%llu", stage_note ? stage_note : "", static_cast<unsigned long long>(match_id));
        return false;
    }
    if (!GBE_FindAndOverwriteBytes(message, GBE_VectorFromBytes(GBE_kOldDotaPracticeLobbyMatchIdVarint.data(), GBE_kOldDotaPracticeLobbyMatchIdVarint.size()), encoded_match_id)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Launch match_id patch failed stage=%s match_id=%llu", stage_note ? stage_note : "", static_cast<unsigned long long>(match_id));
        return false;
    }

    if (patch_server_id) {
        std::string server_id_raw;
        GBE_AppendLittleEndian64(server_id_raw, server_id);
        if (!GBE_FindAndOverwriteBytes(
                message,
                GBE_VectorFromBytes(GBE_kOldDotaPracticeLobbyServerIdFixed64.data(), GBE_kOldDotaPracticeLobbyServerIdFixed64.size()),
                GBE_VectorFromBytes(reinterpret_cast<const uint8 *>(server_id_raw.data()), server_id_raw.size()))) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Launch server_id patch failed stage=%s server_id=%llu", stage_note ? stage_note : "", static_cast<unsigned long long>(server_id));
            return false;
        }
    }

    if (patch_game_start_time) {
        std::vector<uint8> encoded_game_start_time;
        if (!GBE_EncodeVarUint64WithExpectedSize(game_start_time, GBE_kOldDotaPracticeLobbyGameStartTimeVarint.size(), encoded_game_start_time)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Launch game_start_time size mismatch stage=%s game_start_time=%u", stage_note ? stage_note : "", game_start_time);
            return false;
        }
        if (!GBE_FindAndOverwriteBytes(
                message,
                GBE_VectorFromBytes(GBE_kOldDotaPracticeLobbyGameStartTimeVarint.data(), GBE_kOldDotaPracticeLobbyGameStartTimeVarint.size()),
                encoded_game_start_time)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Launch game_start_time patch failed stage=%s game_start_time=%u", stage_note ? stage_note : "", game_start_time);
            return false;
        }
    }

    if (patch_connect) {
        if (connect.size() == std::strlen(GBE_kOldDotaPracticeLobbyConnect)) {
            if (!GBE_FindAndOverwriteString(message, GBE_kOldDotaPracticeLobbyConnect, connect)) {
                GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Launch connect patch failed stage=%s connect=%s", stage_note ? stage_note : "", connect.c_str());
                return false;
            }
        } else {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Launch connect size changed stage=%s size=%zu expected=%zu; skipping fixed-width overwrite and relying on proto rewrite",
                stage_note ? stage_note : "",
                connect.size(),
                std::strlen(GBE_kOldDotaPracticeLobbyConnect)
            );
        }
    }

    return true;
}

static bool GBE_BuildDotaPracticeLobbyLaunchStagePayload(
    size_t stage_index,
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
    const std::string &pass_key,
    std::string &message)
{
    const char *template_hex = GBE_GetDotaPracticeLobbyLaunchStageHex(stage_index);
    if (!template_hex)
        return false;
    std::string wrapped_message;
    if (!GBE_DecodeHexString(template_hex, wrapped_message))
        return false;

    if (wrapped_message.size() < 8)
        return false;

    const uint8 *bytes = reinterpret_cast<const uint8 *>(wrapped_message.data());
    uint32 outer_raw_emsg = 0;
    uint32 outer_header_length = 0;
    std::memcpy(&outer_raw_emsg, bytes, sizeof(outer_raw_emsg));
    std::memcpy(&outer_header_length, bytes + sizeof(outer_raw_emsg), sizeof(outer_header_length));

    if (GBE_GC_MaskedEMsg(outer_raw_emsg) != GBE_kEMsgClientFromGC)
        return false;

    const size_t outer_body_offset = 8u + outer_header_length;
    if (outer_body_offset > wrapped_message.size())
        return false;

    const uint8 *outer_body = bytes + outer_body_offset;
    const size_t outer_body_size = wrapped_message.size() - outer_body_offset;
    GBE_ProtoFieldView payload_field = GBE_FindProtoField(outer_body, outer_body_size, 3u);
    if (!payload_field.found || payload_field.wire_type != 2u || payload_field.value_size < 8u)
        return false;

    std::string inner_payload(
        reinterpret_cast<const char *>(outer_body + payload_field.value_offset),
        payload_field.value_size
    );

    if (!GBE_PatchDotaPracticeLobbyLaunchTemplate(
        inner_payload,
        account_id,
        steam_id,
        lobby_id,
        server_id,
        match_id,
        game_start_time,
        connect,
        stage_index >= 1,
        stage_index >= 1,
        stage_index >= 2,
        stage_index == 0 ? "7041 stage1" :
        stage_index == 1 ? "7041 stage2" :
        stage_index == 2 ? "7041 stage3" : "7041 stage4"
    ))
        return false;

    const uint8 *payload = reinterpret_cast<const uint8 *>(inner_payload.data());
    uint32 inner_raw_emsg = 0;
    std::memcpy(&inner_raw_emsg, payload, sizeof(inner_raw_emsg));
    if (GBE_GC_MaskedEMsg(inner_raw_emsg) != GBE_kDotaPracticeLobbyDetailsUpdate)
        return false;

    if (!GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
            inner_payload,
            account_id,
            steam_id,
            true,
            stage_index <= 1 ? 1u : 2u,
            stage_index == 3 ? 1u : 0u,
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
            stage_index == 3,
            pass_key))
        return false;

    message.swap(inner_payload);
    return true;
}

static bool GBE_BuildDotaPracticeLobbyLaunchPeripheralMessage(
    const char *template_hex,
    uint64 steam_id,
    uint64 lobby_id,
    uint64 server_id,
    bool patch_server_id,
    std::string &message)
{
    if (!GBE_DecodeHexString(template_hex, message))
        return false;

    std::string steam_id_fixed64_raw;
    GBE_AppendLittleEndian64(steam_id_fixed64_raw, steam_id);
    if (!GBE_FindAndOverwriteBytes(
            message,
            GBE_VectorFromBytes(GBE_kOldDotaSteamIdFixed64.data(), GBE_kOldDotaSteamIdFixed64.size()),
            GBE_VectorFromBytes(reinterpret_cast<const uint8 *>(steam_id_fixed64_raw.data()), steam_id_fixed64_raw.size())))
        return false;

    if (patch_server_id) {
        std::string server_id_raw;
        GBE_AppendLittleEndian64(server_id_raw, server_id);
        if (!GBE_FindAndOverwriteBytes(
                message,
                GBE_VectorFromBytes(GBE_kOldDotaPracticeLobbyServerIdFixed64.data(), GBE_kOldDotaPracticeLobbyServerIdFixed64.size()),
                GBE_VectorFromBytes(reinterpret_cast<const uint8 *>(server_id_raw.data()), server_id_raw.size())))
            return false;
    }

    const std::string old_lobby_id_text = GBE_kOldDotaPracticeLobbyLobbyIdText;
    const std::string new_lobby_id_text = std::to_string(lobby_id);
    if (old_lobby_id_text.size() == new_lobby_id_text.size())
        GBE_FindAndOverwriteString(message, old_lobby_id_text, new_lobby_id_text);

    return true;
}

static bool GBE_ExtractWrappedDotaDirectContext(const void *pubData, uint32 cubData, GBE_DotaWrappedDirectContext &context)
{
    context = {};

    if (!pubData || cubData < 8)
        return false;

    const uint8 *bytes = reinterpret_cast<const uint8 *>(pubData);
    uint32 outer_raw_emsg = 0;
    uint32 outer_header_length = 0;
    std::memcpy(&outer_raw_emsg, bytes, sizeof(outer_raw_emsg));
    std::memcpy(&outer_header_length, bytes + sizeof(outer_raw_emsg), sizeof(outer_header_length));

    if (GBE_GC_MaskedEMsg(outer_raw_emsg) != GBE_kEMsgClientToGC)
        return false;

    const size_t outer_header_offset = 8;
    const size_t outer_body_offset = outer_header_offset + outer_header_length;
    if (outer_body_offset > cubData)
        return false;

    const uint8 *outer_header = bytes + outer_header_offset;
    const uint8 *outer_body = bytes + outer_body_offset;
    const size_t outer_body_size = cubData - outer_body_offset;

    GBE_ProtoFieldView outer_session_field = GBE_FindProtoField(outer_header, outer_header_length, 2);
    if (!outer_session_field.found)
        return false;

    context.outer_session_field_raw.assign(
        reinterpret_cast<const char *>(outer_header + outer_session_field.value_offset),
        outer_session_field.value_size
    );

    GBE_ProtoFieldView payload_field = GBE_FindProtoField(outer_body, outer_body_size, 3);
    if (!payload_field.found || payload_field.wire_type != 2 || payload_field.value_size < 8)
        return false;

    const uint8 *payload = outer_body + payload_field.value_offset;
    uint32 inner_raw_emsg = 0;
    uint32 inner_header_length = 0;
    std::memcpy(&inner_raw_emsg, payload, sizeof(inner_raw_emsg));
    std::memcpy(&inner_header_length, payload + sizeof(inner_raw_emsg), sizeof(inner_header_length));

    const size_t inner_header_offset = 8;
    const size_t inner_body_offset = inner_header_offset + inner_header_length;
    if (inner_body_offset > payload_field.value_size)
        return false;

    const uint8 *inner_header = payload + inner_header_offset;
    context.inner_emsg = GBE_GC_MaskedEMsg(inner_raw_emsg);

    context.inner_body_raw.assign(
        reinterpret_cast<const char *>(payload + inner_body_offset),
        payload_field.value_size - inner_body_offset
    );

    GBE_ProtoFieldView request_job_field = GBE_FindProtoField(inner_header, inner_header_length, 10);
    if (!request_job_field.found)
        request_job_field = GBE_FindProtoField(inner_header, inner_header_length, 11);

    uint64 request_job_id = 0;
    if (GBE_ExtractProtoFieldUint64(inner_header, inner_header_length, request_job_field, request_job_id)) {
        context.request_job_id = request_job_id;
        context.has_request_job = true;
    }

    context.valid = true;
    return true;
}

static bool GBE_BuildWrappedDotaReplayMessage(const std::string &inner_payload, const std::string &outer_session_field_raw, uint64 steam_id, std::string &message)
{
    if (inner_payload.size() < sizeof(uint32))
        return false;

    uint32 inner_emsg_flagged = 0;
    std::memcpy(&inner_emsg_flagged, inner_payload.data(), sizeof(inner_emsg_flagged));

    std::string outer_body;
    GBE_AppendProtoVarIntField(outer_body, 1, GBE_kDotaAppId);
    GBE_AppendProtoVarIntField(outer_body, 2, inner_emsg_flagged);
    GBE_AppendProtoBytesField(outer_body, 3, inner_payload);

    std::string outer_header;
    GBE_AppendProtoFixed64Field(outer_header, 1, steam_id);
    GBE_AppendVarUint64(outer_header, (static_cast<uint64>(2) << 3) | 0u);
    outer_header.append(outer_session_field_raw);

    message.clear();
    GBE_AppendLittleEndian32(message, GBE_kEMsgClientFromGC | GBE_kProtoMask);
    GBE_AppendLittleEndian32(message, static_cast<uint32>(outer_header.size()));
    message.append(outer_header);
    message.append(outer_body);
    return true;
}

static bool GBE_BuildDotaPracticeLobbyResponsePayload(uint64 request_job_id, std::string &message)
{
    message.assign(reinterpret_cast<const char *>(GBE_kDotaPracticeLobbyResponseTemplate), sizeof(GBE_kDotaPracticeLobbyResponseTemplate));
    if (message.size() != sizeof(GBE_kDotaPracticeLobbyResponseTemplate) || message.size() < 17)
        return false;

    message[8] = static_cast<char>(0x59);
    std::memcpy(message.data() + 9, &request_job_id, sizeof(request_job_id));
    return true;
}

static bool GBE_BuildDotaZeroHeaderPayload(uint32 emsg, const std::string &body, std::string &message)
{
    message.clear();
    GBE_AppendLittleEndian32(message, emsg | GBE_kProtoMask);
    GBE_AppendLittleEndian32(message, 0u);
    message.append(body);
    return true;
}

static bool GBE_BuildDotaJobReplyPayload(uint32 emsg, uint64 request_job_id, const std::string &body, std::string &message)
{
    message.clear();
    GBE_AppendLittleEndian32(message, emsg | GBE_kProtoMask);
    GBE_AppendLittleEndian32(message, 9u);
    message.push_back(static_cast<char>(0x59));
    message.resize(17u);
    std::memcpy(message.data() + 9, &request_job_id, sizeof(request_job_id));
    message.append(body);
    return true;
}

static bool GBE_BuildDotaJobReplyOrZeroHeaderPayload(uint32 emsg, bool has_request_job, uint64 request_job_id, const std::string &body, std::string &message)
{
    if (has_request_job)
        return GBE_BuildDotaJobReplyPayload(emsg, request_job_id, body, message);

    return GBE_BuildDotaZeroHeaderPayload(emsg, body, message);
}

static bool GBE_BuildDota8728ResponsePayload(bool has_request_job, uint64 request_job_id, std::string &message)
{
    std::string body;
    GBE_AppendProtoVarIntField(body, 1u, 1u);
    return GBE_BuildDotaJobReplyOrZeroHeaderPayload(8728u, has_request_job, request_job_id, body, message);
}

static bool GBE_BuildDota8887ResponsePayload(bool has_request_job, uint64 request_job_id, std::string &message)
{
    std::string body;
    GBE_AppendProtoVarIntField(body, 1u, 1u);
    return GBE_BuildDotaJobReplyOrZeroHeaderPayload(8887u, has_request_job, request_job_id, body, message);
}

static bool GBE_BuildDota7428ResponsePayload(bool has_request_job, uint64 request_job_id, std::string &message)
{
    std::string update;
    GBE_AppendProtoVarIntField(update, 1u, 0u);

    std::string body;
    GBE_AppendProtoBytesField(body, 1u, update);
    return GBE_BuildDotaJobReplyOrZeroHeaderPayload(7428u, has_request_job, request_job_id, body, message);
}

static bool GBE_BuildDota8794ResponsePayload(bool has_request_job, uint64 request_job_id, std::string &message)
{
    std::string body;
    GBE_AppendProtoVarIntField(body, 1u, 1u);
    return GBE_BuildDotaJobReplyOrZeroHeaderPayload(8794u, has_request_job, request_job_id, body, message);
}

static bool GBE_BuildDota4524ResponsePayload(bool has_request_job, uint64 request_job_id, std::string &message)
{
    const float upload_rate_modifier = 1.0f;
    uint32 upload_rate_modifier_raw = 0;
    std::memcpy(&upload_rate_modifier_raw, &upload_rate_modifier, sizeof(upload_rate_modifier_raw));

    std::string body;
    GBE_AppendProtoFixed32Field(body, 1u, upload_rate_modifier_raw);
    return GBE_BuildDotaJobReplyOrZeroHeaderPayload(4524u, has_request_job, request_job_id, body, message);
}

static bool GBE_BuildDota7388MinimalResponsePayload(uint32 event_id, uint32 account_id, bool has_request_job, uint64 request_job_id, std::string &message)
{
    std::string body;
    GBE_AppendProtoVarIntField(body, 1u, 0u);
    GBE_AppendProtoVarIntField(body, 2u, 0u);
    GBE_AppendProtoVarIntField(body, 3u, event_id);
    GBE_AppendProtoVarIntField(body, 4u, 0u);
    GBE_AppendProtoVarIntField(body, 5u, 0u);
    GBE_AppendProtoVarIntField(body, 7u, account_id);
    GBE_AppendProtoVarIntField(body, 8u, 0u);
    return GBE_BuildDotaJobReplyOrZeroHeaderPayload(7388u, has_request_job, request_job_id, body, message);
}

static bool GBE_BuildDota7535ResponsePayload(uint32 account_id, bool has_request_job, uint64 request_job_id, std::string &message)
{
    std::string body;
    GBE_AppendProtoVarIntField(body, 1u, account_id);
    return GBE_BuildDotaJobReplyOrZeroHeaderPayload(7535u, has_request_job, request_job_id, body, message);
}

static bool GBE_BuildDota7504ResponsePayload(uint32 account_id, bool has_request_job, uint64 request_job_id, std::string &message)
{
    std::string emoticon_access;
    GBE_AppendProtoVarIntField(emoticon_access, 1u, account_id);

    std::string body;
    GBE_AppendProtoBytesField(body, 1u, emoticon_access);
    return GBE_BuildDotaJobReplyOrZeroHeaderPayload(7504u, has_request_job, request_job_id, body, message);
}

static bool GBE_BuildDota8096ResponsePayload(uint32 account_id, bool has_request_job, uint64 request_job_id, std::string &message)
{
    std::string body;
    GBE_AppendProtoVarIntField(body, 1u, account_id);
    GBE_AppendProtoVarIntField(body, 21u, 0u);
    return GBE_BuildDotaJobReplyOrZeroHeaderPayload(8096u, has_request_job, request_job_id, body, message);
}

static bool GBE_BuildDota8801ResponsePayload(bool has_request_job, uint64 request_job_id, std::string &message)
{
    std::string body;
    GBE_AppendProtoVarIntField(body, 1u, 1u);
    return GBE_BuildDotaJobReplyOrZeroHeaderPayload(8801u, has_request_job, request_job_id, body, message);
}

static bool GBE_BuildDota8880ResponsePayload(const GBE_DotaRankRequestShape &request_shape, bool has_request_job, uint64 request_job_id, std::string &message)
{
    std::string body;
    const uint32 result = (!request_shape.valid || !request_shape.has_rank_type || !GBE_IsDotaRankTypeSupported(request_shape.rank_type)) ? 2u : 0u;
    GBE_AppendProtoVarIntField(body, 1u, result);
    GBE_AppendProtoVarIntField(body, 2u, 0u);
    GBE_AppendProtoVarIntField(body, 3u, 0u);
    GBE_AppendProtoVarIntField(body, 4u, 0u);
    GBE_AppendProtoVarIntField(body, 5u, 0u);
    return GBE_BuildDotaJobReplyOrZeroHeaderPayload(8880u, has_request_job, request_job_id, body, message);
}

static bool GBE_BuildDota7451BatchPlayerResourcesResponsePayload(const std::vector<uint32> &account_ids, bool has_request_job, uint64 request_job_id, std::string &message)
{
    std::string body;
    for (uint32 account_id : account_ids) {
        std::string result;
        GBE_AppendProtoVarIntField(result, 1u, account_id);
        GBE_AppendProtoBytesField(body, 6u, result);
    }

    return GBE_BuildDotaJobReplyOrZeroHeaderPayload(7451u, has_request_job, request_job_id, body, message);
}

static bool GBE_BuildDota7034ConnectedPlayersResponsePayload(
    uint64 steam_id,
    uint32 lobby_state,
    uint32 game_state,
    uint32 owner_team,
    uint32 owner_slot,
    const GBE_Dota7034RequestShape &request_shape,
    bool has_request_job,
    uint64 request_job_id,
    std::string &message)
{
    const std::string leaver_state = GBE_BuildDota7034LeaverStatePayload(lobby_state, game_state);

    std::string player;
    GBE_AppendProtoFixed64Field(player, 1u, steam_id);
    GBE_AppendProtoBytesField(player, 3u, leaver_state);
    GBE_AppendProtoVarIntField(player, 4u, 0u);

    std::string draft;
    GBE_AppendProtoFixed64Field(draft, 1u, steam_id);
    GBE_AppendProtoVarIntField(draft, 2u, owner_team <= 1u ? owner_team : 0u);
    GBE_AppendProtoVarIntField(draft, 3u, owner_slot > 0u ? (owner_slot - 1u) : 0u);

    std::string body;
    GBE_AppendProtoBytesField(body, 1u, player);
    GBE_AppendProtoVarIntField(body, 2u, game_state);
    if (request_shape.has_first_blood_happened)
        GBE_AppendProtoVarIntField(body, 6u, request_shape.first_blood_happened);
    GBE_AppendProtoVarIntField(body, 8u, request_shape.has_send_reason ? request_shape.send_reason : 2u);
    if (request_shape.has_radiant_kills)
        GBE_AppendProtoVarIntField(body, 11u, request_shape.radiant_kills);
    if (request_shape.has_dire_kills)
        GBE_AppendProtoVarIntField(body, 12u, request_shape.dire_kills);
    if (request_shape.has_radiant_lead)
        GBE_AppendProtoVarIntField(body, 14u, request_shape.radiant_lead);
    if (request_shape.has_building_state)
        GBE_AppendProtoVarIntField(body, 15u, request_shape.building_state);
    GBE_AppendProtoBytesField(body, 16u, draft);

    if (request_shape.has_disconnected_player && (!request_shape.has_disconnected_steam_id || request_shape.disconnected_steam_id == steam_id)) {
        uint32 disconnected_lobby_state = request_shape.has_disconnected_lobby_state ? request_shape.disconnected_lobby_state : lobby_state;
        uint32 disconnected_game_state = request_shape.has_disconnected_game_state ? request_shape.disconnected_game_state : game_state;
        if (disconnected_lobby_state < lobby_state)
            disconnected_lobby_state = lobby_state;
        if (disconnected_game_state < game_state)
            disconnected_game_state = game_state;

        std::string disconnected_player;
        GBE_AppendProtoFixed64Field(disconnected_player, 1u, steam_id);
        GBE_AppendProtoBytesField(disconnected_player, 3u, GBE_BuildDota7034LeaverStatePayload(disconnected_lobby_state, disconnected_game_state));
        GBE_AppendProtoVarIntField(disconnected_player, 4u, 0u);
        GBE_AppendProtoBytesField(body, 7u, disconnected_player);
    }

    return GBE_BuildDotaJobReplyOrZeroHeaderPayload(7034u, has_request_job, request_job_id, body, message);
}

static bool GBE_BuildDotaJoinChatChannelResponsePayload(
    uint64 steam_id,
    uint64 channel_id,
    const std::string &channel_name,
    const std::string &player_name,
    uint32 channel_type,
    std::string &message)
{
    std::string body;
    GBE_AppendProtoVarIntField(body, 1u, 0u);
    GBE_AppendProtoBytesField(body, 2u, channel_name);
    GBE_AppendProtoFixed64Field(body, 3u, channel_id);
    GBE_AppendProtoVarIntField(body, 4u, 200u);

    std::string member;
    GBE_AppendProtoFixed64Field(member, 1u, steam_id);
    GBE_AppendProtoBytesField(member, 2u, player_name);
    GBE_AppendProtoVarIntField(member, 3u, 0u);
    GBE_AppendProtoVarIntField(member, 4u, 0u);
    GBE_AppendProtoBytesField(body, 5u, member);

    GBE_AppendProtoVarIntField(body, 6u, channel_type);
    GBE_AppendProtoVarIntField(body, 7u, 0u);
    GBE_AppendProtoVarIntField(body, 9u, 0u);
    GBE_AppendProtoVarIntField(body, 11u, 0u);
    return GBE_BuildDotaZeroHeaderPayload(GBE_kDotaJoinChatChannelResponse, body, message);
}

static bool GBE_BuildDotaOtherLeftChannelPayload(uint64 channel_id, uint64 steam_id, std::string &message)
{
    std::string body;
    GBE_AppendProtoFixed64Field(body, 1u, channel_id);
    GBE_AppendProtoFixed64Field(body, 2u, steam_id);
    return GBE_BuildDotaZeroHeaderPayload(GBE_kDotaOtherLeftChannel, body, message);
}

static bool GBE_BuildDotaLobbyCacheUnsubscribedPayload(uint64 lobby_id, std::string &message)
{
    std::string owner_soid;
    GBE_AppendProtoVarIntField(owner_soid, 1u, 3u);
    GBE_AppendProtoVarIntField(owner_soid, 2u, lobby_id);

    std::string body;
    GBE_AppendProtoBytesField(body, 2u, owner_soid);
    return GBE_BuildDotaZeroHeaderPayload(GBE_kDotaCacheUnsubscribed, body, message);
}

static bool GBE_BuildDotaDestroyLobbyResponsePayload(uint64 request_job_id, std::string &message)
{
    std::string body;
    GBE_AppendProtoVarIntField(body, 1u, 0u);
    return GBE_BuildDotaJobReplyPayload(GBE_kDotaDestroyLobbyResponse, request_job_id, body, message);
}

static void GBE_BuildDotaPracticeLobbySOObjectData(
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
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    std::string &object_2015,
    std::string &object_2016,
    std::string &object_2004,
    std::string &object_2014)
{
    static const uint8 GBE_kDotaLobbyField62Value[] = { 0x08, 0xF5, 0x44, 0x12, 0x02, 0x08, 0x00 };

    object_2015.clear();
    {
        // Keep server-lobby member cardinality aligned with 2004/2014/2016.
        // CSODOTAServerLobbyMember is currently empty, so the minimal legal
        // representation for one member is a repeated field-1 entry with an
        // empty embedded message.
        GBE_AppendProtoBytesField(object_2015, 1, std::string());

        if (extra_startup_account_id != 0) {
            std::string startup_payload;
            GBE_AppendProtoVarIntField(startup_payload, 1u, extra_startup_account_id);

            std::string startup_message;
            GBE_AppendProtoVarIntField(startup_message, 1u, GBE_kDotaLobbyAdditionalAccountData);
            GBE_AppendProtoBytesField(startup_message, 2u, startup_payload);
            GBE_AppendProtoBytesField(object_2015, 2u, startup_message);
        }
    }

    {
        std::string member_bytes;
        GBE_AppendProtoFixed64Field(member_bytes, 1, steam_id);

        object_2016.clear();
        GBE_AppendProtoBytesField(object_2016, 1, member_bytes);
    }

    object_2004.clear();
    GBE_AppendProtoVarIntField(object_2004, 1, lobby_id);
    GBE_AppendProtoVarIntField(object_2004, 3, game_mode);
    GBE_AppendProtoVarIntField(object_2004, 4, lobby_state);
    if (!connect.empty())
        GBE_AppendProtoBytesField(object_2004, 5, connect);
    if (server_id != 0)
        GBE_AppendProtoFixed64Field(object_2004, 6, server_id);
    GBE_AppendProtoFixed64Field(object_2004, 11, steam_id);
    GBE_AppendProtoVarIntField(object_2004, 12, 1u);
    GBE_AppendProtoVarIntField(object_2004, 13, allow_cheats ? 1u : 0u);
    GBE_AppendProtoVarIntField(object_2004, 14, fill_with_bots ? 1u : 0u);
    GBE_AppendProtoBytesField(object_2004, 16, room_name);
    GBE_AppendProtoBytesField(object_2004, 17, std::string());
    GBE_AppendProtoBytesField(object_2004, 17, std::string());
    GBE_AppendProtoVarIntField(object_2004, 21, server_region);
    GBE_AppendProtoVarIntField(object_2004, 22, lobby_game_state);
    GBE_AppendProtoVarIntField(object_2004, 28, 0u);
    if (match_id != 0)
        GBE_AppendProtoVarIntField(object_2004, 30, match_id);
    GBE_AppendProtoVarIntField(object_2004, 31, allow_spectating ? 1u : 0u);
    GBE_AppendProtoVarIntField(object_2004, 36, bot_difficulty_radiant);
    GBE_AppendProtoBytesField(object_2004, 39, pass_key);
    GBE_AppendProtoVarIntField(object_2004, 42, 0u);
    GBE_AppendProtoVarIntField(object_2004, 43, 0u);
    GBE_AppendProtoVarIntField(object_2004, 44, 0u);
    GBE_AppendProtoVarIntField(object_2004, 46, 0u);
    GBE_AppendProtoVarIntField(object_2004, 47, 0u);
    GBE_AppendProtoVarIntField(object_2004, 48, 0u);
    GBE_AppendProtoVarIntField(object_2004, 51, 0u);
    GBE_AppendProtoVarIntField(object_2004, 53, 0u);
    GBE_AppendProtoVarIntField(object_2004, 57, lan ? 1u : 0u);
    if (has_broadcast_channel) {
        std::string broadcast_info;
        GBE_AppendProtoVarIntField(broadcast_info, 1, broadcast_channel_id);
        GBE_AppendProtoBytesField(broadcast_info, 2, broadcast_country_code);
        GBE_AppendProtoBytesField(broadcast_info, 3, broadcast_description);
        GBE_AppendProtoBytesField(broadcast_info, 4, broadcast_language_code);
        GBE_AppendProtoBytesField(object_2004, 58, broadcast_info);
    }
    GBE_AppendProtoBytesField(object_2004, 62, std::string(reinterpret_cast<const char *>(GBE_kDotaLobbyField62Value), sizeof(GBE_kDotaLobbyField62Value)));
    GBE_AppendProtoVarIntField(object_2004, 75, visibility);
    GBE_AppendProtoVarIntField(object_2004, 82, 0u);
    if (game_start_time != 0)
        GBE_AppendProtoVarIntField(object_2004, 87, game_start_time);
    GBE_AppendProtoVarIntField(object_2004, 88, 0u);
    GBE_AppendProtoVarIntField(object_2004, 93, bot_difficulty_dire);
    GBE_AppendProtoVarIntField(object_2004, 94, bot_radiant);
    GBE_AppendProtoVarIntField(object_2004, 95, bot_dire);
    GBE_AppendProtoVarIntField(object_2004, 97, 0u);
    if (!lan_host_ping_location.empty())
        GBE_AppendProtoBytesField(object_2004, 109, lan_host_ping_location);
    GBE_AppendProtoVarIntField(object_2004, 110, 0u);
    GBE_AppendProtoVarIntField(object_2004, 113, 0u);

    {
        std::string owner_state;
        GBE_AppendProtoFixed64Field(owner_state, 1, steam_id);
        GBE_AppendProtoVarIntField(owner_state, 3, owner_team);
        GBE_AppendProtoVarIntField(owner_state, 7, owner_slot);
        GBE_AppendProtoVarIntField(owner_state, 16, 0u);
        GBE_AppendProtoVarIntField(owner_state, 28, 0u);
        GBE_AppendProtoBytesField(object_2004, 120, owner_state);
    }

    GBE_AppendProtoVarIntField(object_2004, 121, 0u);
    GBE_AppendProtoVarIntField(object_2004, 127, 0u);
    GBE_AppendProtoVarIntField(object_2004, 128, GBE_kDotaLobbyField128Value);

    object_2014.clear();
    {
        std::string name_entry;
        GBE_AppendProtoBytesField(name_entry, 1, player_name);
        GBE_AppendProtoVarIntField(name_entry, 2, 0u);
        GBE_AppendProtoBytesField(object_2014, 1, name_entry);
    }
}

static bool GBE_BuildDotaPracticeLobbyCacheSubscribedPayload(
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
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    uint32 extra_startup_account_id,
    std::string &message)
{
    std::string object_2015;
    std::string object_2016;
    std::string object_2004;
    std::string object_2014;
    GBE_BuildDotaPracticeLobbySOObjectData(
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
        has_broadcast_channel,
        broadcast_channel_id,
        broadcast_country_code,
        broadcast_description,
        broadcast_language_code,
        pass_key,
        extra_startup_account_id,
        object_2015,
        object_2016,
        object_2004,
        object_2014);

    message.clear();
    {
        ProtoBufMsgHeader_t hdr{};
        hdr.m_EMsgFlagged = GBE_kDotaCacheSubscribed | GBE_kProtoMask;
        hdr.m_cubProtoBufExtHdr = 0;
        ser_var<ProtoBufMsgHeader_t>(message, hdr);
    }

    CMsgSOCacheSubscribed protomsg;
    protomsg.set_owner(steam_id);

    auto object_2004_entry = protomsg.add_objects();
    object_2004_entry->set_type_id(2004);
    object_2004_entry->add_object_data(object_2004);

    auto object_2014_entry = protomsg.add_objects();
    object_2014_entry->set_type_id(2014);
    object_2014_entry->add_object_data(object_2014);

    auto object_2015_entry = protomsg.add_objects();
    object_2015_entry->set_type_id(2015);
    object_2015_entry->add_object_data(object_2015);

    auto object_2016_entry = protomsg.add_objects();
    object_2016_entry->set_type_id(2016);
    object_2016_entry->add_object_data(object_2016);

    protomsg.AppendToString(&message);
    return true;
}

static bool GBE_BuildDotaPracticeLobbyDetailsUpdatePayload(
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
    bool has_broadcast_channel,
    uint32 broadcast_channel_id,
    const std::string &broadcast_country_code,
    const std::string &broadcast_description,
    const std::string &broadcast_language_code,
    const std::string &pass_key,
    std::string &message)
{
    std::string object_2015;
    std::string object_2016;
    std::string object_2004;
    std::string object_2014;
    std::string body;

    GBE_BuildDotaPracticeLobbySOObjectData(
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
        has_broadcast_channel,
        broadcast_channel_id,
        broadcast_country_code,
        broadcast_description,
        broadcast_language_code,
        pass_key,
        0,
        object_2015,
        object_2016,
        object_2004,
        object_2014);

    {
        std::string update;
        GBE_AppendProtoVarIntField(update, 1, 2015u);
        GBE_AppendProtoBytesField(update, 2, object_2015);
        GBE_AppendProtoBytesField(body, 2, update);
    }

    {
        std::string update;
        GBE_AppendProtoVarIntField(update, 1, 2016u);
        GBE_AppendProtoBytesField(update, 2, object_2016);
        GBE_AppendProtoBytesField(body, 2, update);
    }

    {
        std::string update;
        GBE_AppendProtoVarIntField(update, 1, 2004u);
        GBE_AppendProtoBytesField(update, 2, object_2004);
        GBE_AppendProtoBytesField(body, 2, update);
    }

    {
        std::string update;
        GBE_AppendProtoVarIntField(update, 1, 2014u);
        GBE_AppendProtoBytesField(update, 2, object_2014);
        GBE_AppendProtoBytesField(body, 2, update);
    }

    GBE_AppendProtoFixed64Field(body, 3, GBE_kDotaLobbyDetailsTimestamp);

    {
        std::string lobby_ref;
        GBE_AppendProtoVarIntField(lobby_ref, 1, 3u);
        GBE_AppendProtoVarIntField(lobby_ref, 2, lobby_id);
        GBE_AppendProtoBytesField(body, 6, lobby_ref);
    }

    message.clear();
    GBE_AppendLittleEndian32(message, GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask);
    GBE_AppendLittleEndian32(message, 0u);
    message.append(body);
    return true;
}

static bool GBE_BuildDotaDirectReplayMessage(
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

    if (message.size() < sizeof(ProtoBufMsgHeader_t))
        return false;

    ProtoBufMsgHeader_t hdr{};
    std::memcpy(&hdr, message.data(), sizeof(hdr));
    if (message.size() < sizeof(hdr) + hdr.m_cubProtoBufExtHdr)
        return false;

    CMsgProtoBufHeader protohdr;
    if (hdr.m_cubProtoBufExtHdr != 0 && !protohdr.ParseFromArray(message.data() + sizeof(hdr), hdr.m_cubProtoBufExtHdr))
        return false;

    if (has_target_job) {
        protohdr.set_job_id_target(target_job);
    } else {
        protohdr.clear_job_id_target();
    }
    protohdr.clear_job_id_source();

    const size_t old_header_size = hdr.m_cubProtoBufExtHdr;
    const char *body_ptr = message.data() + sizeof(ProtoBufMsgHeader_t) + old_header_size;
    const size_t serialized_body_size = message.size() - sizeof(ProtoBufMsgHeader_t) - old_header_size;

    std::string updated;
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

    GBE_ProtoFieldView outer_session_field = GBE_FindProtoField(outer_header, outer_header_length, 2);
    if (!outer_session_field.found) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "missing outer session field");
        return false;
    }
    context.outer_session_field_raw.assign(
        reinterpret_cast<const char *>(outer_header + outer_session_field.value_offset),
        outer_session_field.value_size
    );

    GBE_ProtoFieldView payload_field = GBE_FindProtoField(outer_body, outer_body_size, 3);
    if (!payload_field.found || payload_field.wire_type != 2 || payload_field.value_size < 8) {
        GBE_GC_DebugLog(
            "GC_DOTA_HELLO",
            "invalid payload field found=%d wire=%u size=%zu",
            payload_field.found ? 1 : 0,
            payload_field.wire_type,
            payload_field.value_size
        );
        return false;
    }

    const uint8 *payload = outer_body + payload_field.value_offset;
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
    if (inner_body_offset > payload_field.value_size) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "inner body offset overflow header_len=%u payload_size=%zu", inner_header_length, payload_field.value_size);
        return false;
    }

    const uint8 *inner_header = payload + inner_header_offset;
    const uint8 *inner_body = payload + inner_body_offset;
    const size_t inner_body_size = payload_field.value_size - inner_body_offset;

    GBE_ProtoFieldView version_field = GBE_FindProtoField(inner_body, inner_body_size, 1);
    uint64 parsed_version = 0;
    if (!GBE_ExtractProtoFieldUint64(inner_body, inner_body_size, version_field, parsed_version)) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "failed to extract version field");
        return false;
    }

    context.version = static_cast<uint32>(parsed_version);

    if (inner_header_length > 0) {
        GBE_ProtoFieldView source_job_field = GBE_FindProtoField(inner_header, inner_header_length, 11);
        uint64 source_job = 0;
        if (GBE_ExtractProtoFieldUint64(inner_header, inner_header_length, source_job_field, source_job)) {
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

    if (!pubData || cubData < sizeof(ProtoBufMsgHeader_t)) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "direct path invalid input pubData=%p cubData=%u", pubData, cubData);
        return false;
    }

    const char *cursor = reinterpret_cast<const char *>(pubData);
    const char *end = cursor + cubData;
    ProtoBufMsgHeader_t hdr = deser_var<ProtoBufMsgHeader_t>(cursor);

    if ((end - cursor) < hdr.m_cubProtoBufExtHdr) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "direct path proto header overflow ext=%u cubData=%u", hdr.m_cubProtoBufExtHdr, cubData);
        return false;
    }

    CMsgProtoBufHeader protohdr;
    if (!protohdr.ParseFromArray(cursor, hdr.m_cubProtoBufExtHdr)) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "direct path failed parsing CMsgProtoBufHeader ext=%u", hdr.m_cubProtoBufExtHdr);
        return false;
    }

    cursor += hdr.m_cubProtoBufExtHdr;
    const uint8 *body = reinterpret_cast<const uint8 *>(cursor);
    const size_t body_size = static_cast<size_t>(end - cursor);
    GBE_ProtoFieldView version_field = GBE_FindProtoField(body, body_size, 1);
    uint64 parsed_version = 0;
    if (!GBE_ExtractProtoFieldUint64(body, body_size, version_field, parsed_version)) {
        GBE_GC_DebugLog("GC_DOTA_HELLO", "direct path failed to extract version field");
        return false;
    }

    context.valid = true;
    context.version = static_cast<uint32>(parsed_version);
    if (protohdr.has_job_id_source()) {
        context.source_job_id = protohdr.job_id_source();
        context.has_source_job = true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_HELLO",
        "direct path parsed version=%u source_job=%llu has_source_job=%d body_size=%zu",
        context.version,
        static_cast<unsigned long long>(context.source_job_id),
        context.has_source_job ? 1 : 0,
        body_size
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

    if (!pubData || cubData < sizeof(ProtoBufMsgHeader_t)) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "direct path invalid input pubData=%p cubData=%u", pubData, cubData);
        return false;
    }

    const char *cursor = reinterpret_cast<const char *>(pubData);
    const char *end = cursor + cubData;
    ProtoBufMsgHeader_t hdr = deser_var<ProtoBufMsgHeader_t>(cursor);

    if ((end - cursor) < hdr.m_cubProtoBufExtHdr) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "direct path proto header overflow ext=%u cubData=%u", hdr.m_cubProtoBufExtHdr, cubData);
        return false;
    }

    CMsgProtoBufHeader protohdr;
    if (!protohdr.ParseFromArray(cursor, hdr.m_cubProtoBufExtHdr)) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "direct path failed parsing CMsgProtoBufHeader ext=%u", hdr.m_cubProtoBufExtHdr);
        return false;
    }

    cursor += hdr.m_cubProtoBufExtHdr;
    const uint8 *body = reinterpret_cast<const uint8 *>(cursor);
    const size_t body_size = static_cast<size_t>(end - cursor);

    uint64 active_version = 0;
    if (!GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 1), active_version)) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "direct path failed to extract active version body_size=%zu", body_size);
        return false;
    }

    uint64 min_allowed_version = active_version;
    GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 3), min_allowed_version);

    uint64 compatibility_value = 0;
    GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 6), compatibility_value);

    uint64 universe = 0;
    GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 7), universe);

    context.valid = true;
    context.active_version = static_cast<uint32>(active_version);
    context.min_allowed_version = static_cast<uint32>(min_allowed_version > 0xFFFFFFFFull ? 0xFFFFFFFFu : min_allowed_version);
    context.compatibility_value = compatibility_value;
    context.universe = static_cast<uint32>(universe > 0xFFFFFFFFull ? 0xFFFFFFFFu : universe);
    if (protohdr.has_job_id_source()) {
        context.source_job_id = protohdr.job_id_source();
        context.has_source_job = true;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_SERVER_HELLO",
        "direct path parsed active_version=%u min_allowed=%u compat=%llu universe=%u source_job=%llu has_source_job=%d body_size=%zu",
        context.active_version,
        context.min_allowed_version,
        static_cast<unsigned long long>(context.compatibility_value),
        context.universe,
        static_cast<unsigned long long>(context.source_job_id),
        context.has_source_job ? 1 : 0,
        body_size
    );
    return true;
}

static bool GBE_BuildDotaWelcomeBody(uint64 steam_id, uint32 account_id, const GBE_DotaHelloContext &context, std::string &inner_body)
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
        std::vector<uint8> encoded_version;
        if (!GBE_EncodeVarUint64WithExpectedSize(context.version, GBE_kOldDotaVersionVarint.size(), encoded_version)) {
            GBE_GC_DebugLog("GC_DOTA_WELCOME", "version varint size mismatch version=%u expected=%zu", context.version, GBE_kOldDotaVersionVarint.size());
            return false;
        }
        if (!GBE_FindAndOverwriteBytes(inner_body, GBE_VectorFromBytes(GBE_kOldDotaVersionVarint.data(), GBE_kOldDotaVersionVarint.size()), encoded_version)) {
            GBE_GC_DebugLog("GC_DOTA_WELCOME", "failed replacing version bytes version=%u", context.version);
            return false;
        }
    }

    if (!GBE_PatchDotaWelcomeAccountObjects(inner_body, account_id)) {
        GBE_GC_DebugLog("GC_DOTA_WELCOME", "failed patching welcome account-bound objects account_id=%u", account_id);
        return false;
    }

    {
        std::vector<uint8> encoded_steam_id;
        if (!GBE_EncodeVarUint64WithExpectedSize(steam_id, GBE_kOldDotaSteamIdVarint.size(), encoded_steam_id)) {
            GBE_GC_DebugLog("GC_DOTA_WELCOME", "steam_id varint size mismatch steam_id=%llu expected=%zu", static_cast<unsigned long long>(steam_id), GBE_kOldDotaSteamIdVarint.size());
            return false;
        }
        if (!GBE_FindAndOverwriteBytes(inner_body, GBE_VectorFromBytes(GBE_kOldDotaSteamIdVarint.data(), GBE_kOldDotaSteamIdVarint.size()), encoded_steam_id)) {
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
    if (!GBE_BuildDotaWelcomeBody(steam_id, account_id, context, inner_body))
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

static bool GBE_BuildDirectDotaServerWelcome(uint64 steam_id, uint32 app_id, const GBE_DotaServerHelloContext &context, std::string &message)
{
    if (!context.valid) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "invalid server hello context");
        return false;
    }

    ProtoBufMsgHeader_t hdr{};
    hdr.m_EMsgFlagged = EGCBaseClientMsg::k_EMsgGCServerWelcome | GBE_kProtoMask;

    CMsgProtoBufHeader protohdr;
    protohdr.set_client_steam_id(steam_id);
    protohdr.set_client_session_id(1);
    protohdr.set_source_app_id(app_id);
    if (context.has_source_job)
        protohdr.set_job_id_target(context.source_job_id);

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
        "built direct ServerWelcome active_version=%u min_allowed=%u target_job=%llu total=%zu",
        context.active_version,
        context.min_allowed_version,
        static_cast<unsigned long long>(context.has_source_job ? context.source_job_id : 0ull),
        message.size()
    );
    return true;
}

static bool GBE_BuildDotaClientWelcome(uint64 steam_id, uint32 app_id, uint32 account_id, const GBE_DotaHelloContext &context, std::string &message)
{
    std::string inner_body;
    if (!GBE_BuildDotaWelcomeBody(steam_id, account_id, context, inner_body))
        return false;

    std::string inner_header;
    if (context.has_source_job) {
        GBE_AppendProtoVarIntField(inner_header, 10, context.source_job_id);
        GBE_GC_DebugLog("GC_DOTA_WELCOME", "mirroring source_job=%llu into inner target_job", static_cast<unsigned long long>(context.source_job_id));
    }

    std::string inner_payload;
    GBE_AppendLittleEndian32(inner_payload, GBE_kEMsgGCClientWelcome | GBE_kProtoMask);
    GBE_AppendLittleEndian32(inner_payload, static_cast<uint32>(inner_header.size()));
    inner_payload.append(inner_header);
    inner_payload.append(inner_body);

    std::string outer_body;
    GBE_AppendProtoVarIntField(outer_body, 1, app_id);
    GBE_AppendProtoVarIntField(outer_body, 2, GBE_kEMsgGCClientWelcome | GBE_kProtoMask);
    GBE_AppendProtoBytesField(outer_body, 3, inner_payload);

    std::string outer_header;
    // The outer fixed64 steamid lives outside the inner template, so we rebuild that header directly.
    GBE_AppendProtoFixed64Field(outer_header, 1, steam_id);
    GBE_AppendVarUint64(outer_header, (static_cast<uint64>(2) << 3) | 0u);
    outer_header.append(context.outer_session_field_raw);

    message.clear();
    GBE_AppendLittleEndian32(message, GBE_kEMsgClientFromGC | GBE_kProtoMask);
    GBE_AppendLittleEndian32(message, static_cast<uint32>(outer_header.size()));
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

    std::string file_path = Local_Storage::get_game_settings_path() + gc_config_file;
    nlohmann::json gc_json;
    if (!local_storage->load_json(file_path, gc_json)) {
        if (settings->get_local_game_id().AppID() == GBE_kDotaAppId) {
            gc_profile = GC_PROFILE_DOTA2;
            GBE_GC_DebugLog("GC_CONFIG", "auto-enabled Dota2 GC profile for app %u", settings->get_local_game_id().AppID());
        }
        return;
    }

    try {
        std::string gc_profile_name = gc_json.value("gc_profile", std::string());
        std::transform(gc_profile_name.begin(), gc_profile_name.end(), gc_profile_name.begin(),
            [](auto c) { return std::tolower(c); });
        if (gc_profile_name == "tf2") {
            gc_profile = GC_PROFILE_TF2;
        } else if (gc_profile_name == "dota2" || gc_profile_name == "dota") {
            gc_profile = GC_PROFILE_DOTA2;
        } else if (gc_profile_name == "portal2") {
            // Portal 2 is pretty much entirely compatible with TF2 protobuf structs so we can just
            // make it an alias for TF2 profile.
            //gc_profile = GC_PROFILE_PORTAL2;
            gc_profile = GC_PROFILE_TF2;
            is_portal2 = true;
        } else {
            gc_profile = GC_PROFILE_INVALID;
        }

        gc_version = gc_json.value("gc_version", 0);
    } catch (std::exception &e) {
        const char *errorMessage = e.what();
        PRINT_DEBUG("error parsing GC config: %s", errorMessage);
        gc_version = 0;
        gc_profile = GC_PROFILE_INVALID;
    }

    if (gc_profile == GC_PROFILE_INVALID && settings->get_local_game_id().AppID() == GBE_kDotaAppId) {
        gc_profile = GC_PROFILE_DOTA2;
        GBE_GC_DebugLog("GC_CONFIG", "fell back to Dota2 GC profile for app %u", settings->get_local_game_id().AppID());
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

    GBE_local_lobby.state = message.lobby_state;
    GBE_local_lobby.game_state = message.lobby_game_state;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Applied queued state=%u game_state=%u msg=%u",
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        GBE_GC_MaskedEMsg(message.msg_type)
    );

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

    GBE_GC_DebugLog("GC_CALLBACK", "queued msg=%u size=%u and posted GCMessageAvailable_t", GBE_GC_MaskedEMsg(msg_type), static_cast<uint32>(message.size()));
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
    const char *p = reinterpret_cast<const char *>(input);
    const char *end = p + input_size;

    ProtoBufMsgHeader_t hdr{};
    CMsgProtoBufHeader protohdr;
    T protomsg;

    if (input_size < sizeof(ProtoBufMsgHeader_t))
        return { hdr, protohdr, protomsg, false };

    hdr = deser_var<ProtoBufMsgHeader_t>(p);

    if (!protohdr.ParseFromArray(p, hdr.m_cubProtoBufExtHdr))
        return { hdr, protohdr, protomsg, false };

    p += hdr.m_cubProtoBufExtHdr;

    int protomsg_size = static_cast<int>(end - p);
    if (!protomsg.ParseFromArray(p, protomsg_size))
        return { hdr, protohdr, protomsg, false };

    return { hdr, protohdr, protomsg, true };
}

bool Steam_Game_Coordinator::GBE_PatchDotaLoginCacheSubscribedInventory(std::string &message)
{
    if (message.size() < sizeof(ProtoBufMsgHeader_t))
        return false;

    ProtoBufMsgHeader_t hdr{};
    std::memcpy(&hdr, message.data(), sizeof(hdr));
    if (message.size() < sizeof(hdr) + hdr.m_cubProtoBufExtHdr)
        return false;

    const char *proto_header_ptr = message.data() + sizeof(hdr);
    const char *body_ptr = proto_header_ptr + hdr.m_cubProtoBufExtHdr;
    const size_t body_size = message.size() - sizeof(hdr) - hdr.m_cubProtoBufExtHdr;

    CMsgSOCacheSubscribed protomsg;
    if (!protomsg.ParseFromArray(body_ptr, static_cast<int>(body_size)))
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

    proto_item.set_contains_equipped_state(true);
    proto_item.set_contains_equipped_state_v2(true);

    for (const auto &[class_id, slot_id] : item.equip_states) {
        auto proto_equip = proto_item.add_equipped_state();
        proto_equip->set_new_class(class_id);
        proto_equip->set_new_slot(slot_id);
    }

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

void Steam_Game_Coordinator::callback_items_received(CSteamID steam_id, const std::vector<Econ_Item> &items)
{
    if (!gc_initialized)
        return;

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
    this->network->setCallback(CALLBACK_ID_USER_STATUS, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->run_every_runcb->add(&Steam_Game_Coordinator::steam_run_every_runcb, this);

    parse_gc_config();

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
    this->network->rmCallback(CALLBACK_ID_USER_STATUS, settings->get_local_steam_id(), &Steam_Game_Coordinator::steam_network_callback, this);
    this->run_every_runcb->remove(&Steam_Game_Coordinator::steam_run_every_runcb, this);
}

void Steam_Game_Coordinator::initialize_gc()
{
    if (!gc_enabled() || gc_initialized)
        return;

    gc_initialized = true;

    if (gc_profile == GC_PROFILE_DOTA2) {
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
    gc_initialized = false;
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
                    const char *value = attr.value_bytes.c_str();
                    json_attr["value_string"] = value;
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
            GBE_Dota7034RequestShape synthetic_connected_players_request{};
            synthetic_connected_players_request.has_send_reason = true;
            synthetic_connected_players_request.send_reason = 4u;

            std::string connected_players_message;
            if (GBE_BuildDota7034ConnectedPlayersResponsePayload(
                    owner_steam_id,
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state,
                    GBE_local_lobby.owner_team,
                    GBE_local_lobby.owner_slot,
                    synthetic_connected_players_request,
                    false,
                    0ull,
                    connected_players_message)) {
                GBE_GC_DebugLog(
                    "GC_DOTA_SYNC",
                    "queueing synthetic direct 7034 on client connect steam_id=%llu lobby_id=%llu state=%u game_state=%u team=%u slot=%u size=%zu",
                    static_cast<unsigned long long>(connected_steam_id),
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state,
                    GBE_local_lobby.owner_team,
                    GBE_local_lobby.owner_slot,
                    connected_players_message.size()
                );
                push_incoming_now(7034u | GBE_kProtoMask, connected_players_message);
            } else {
                GBE_GC_DebugLog(
                    "GC_DOTA_SYNC",
                    "failed building synthetic direct 7034 on client connect steam_id=%llu lobby_id=%llu state=%u game_state=%u team=%u slot=%u",
                    static_cast<unsigned long long>(connected_steam_id),
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state,
                    GBE_local_lobby.owner_team,
                    GBE_local_lobby.owner_slot
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

    remove_user_items(steam_id);
}

void Steam_Game_Coordinator::GBE_PushDotaLoginSyncMessages()
{
    if (GBE_dota_login_sync_sent)
        return;

    const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    const uint32 account_id = settings->get_local_steam_id().GetAccountID();

    std::string cache_subscribed_message;
    if (!GBE_BuildDotaDirectReplayMessage(
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

bool Steam_Game_Coordinator::GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay(const std::string &player_name, std::string &message)
{
    GBE_RestoreSharedDotaLobbyState("cache_template_replay");

    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0)
        return false;

    const uint64 steam_id = GBE_GetDotaLobbyOwnerSteamId();
    const uint32 account_id = GBE_GetDotaLobbyOwnerAccountId();
    if (steam_id == 0 || account_id == 0)
        return false;

    if (!GBE_BuildDotaDirectReplayMessage(
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

    if (!GBE_PatchDotaLobbyTemplateIdentifiers(message, account_id, steam_id, GBE_local_lobby.lobby_id))
        return false;

    return GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState(
        message,
        account_id,
        steam_id,
        false,
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        GBE_local_lobby.server_id,
        GBE_local_lobby.match_id,
        GBE_local_lobby.game_start_time,
        GBE_local_lobby.connect,
        player_name,
        GBE_local_lobby.room_name,
        GBE_local_lobby.game_mode,
        GBE_local_lobby.server_region,
        GBE_local_lobby.lan,
        GBE_local_lobby.lan_host_ping_location,
        GBE_local_lobby.allow_cheats,
        GBE_local_lobby.fill_with_bots,
        GBE_local_lobby.allow_spectating,
        GBE_local_lobby.visibility,
        GBE_local_lobby.bot_difficulty_radiant,
        GBE_local_lobby.bot_difficulty_dire,
        GBE_local_lobby.bot_radiant,
        GBE_local_lobby.bot_dire,
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot,
        false,
        GBE_local_lobby.pass_key);
}

bool Steam_Game_Coordinator::GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayload(const std::string &player_name, std::string &message)
{
    GBE_RestoreSharedDotaLobbyState("cache_payload");

    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0)
        return false;

    const uint64 steam_id = GBE_GetDotaLobbyOwnerSteamId();
    if (steam_id == 0)
        return false;

    return GBE_BuildDotaPracticeLobbyCacheSubscribedPayload(
        steam_id,
        GBE_local_lobby.lobby_id,
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        GBE_local_lobby.server_id,
        GBE_local_lobby.match_id,
        GBE_local_lobby.game_start_time,
        GBE_local_lobby.connect,
        player_name,
        GBE_local_lobby.room_name,
        GBE_local_lobby.game_mode,
        GBE_local_lobby.server_region,
        GBE_local_lobby.lan,
        GBE_local_lobby.lan_host_ping_location,
        GBE_local_lobby.allow_cheats,
        GBE_local_lobby.fill_with_bots,
        GBE_local_lobby.allow_spectating,
        GBE_local_lobby.visibility,
        GBE_local_lobby.bot_difficulty_radiant,
        GBE_local_lobby.bot_difficulty_dire,
        GBE_local_lobby.bot_radiant,
        GBE_local_lobby.bot_dire,
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot,
        GBE_local_lobby.has_broadcast_channel,
        GBE_local_lobby.broadcast_channel_id,
        GBE_local_lobby.broadcast_country_code,
        GBE_local_lobby.broadcast_description,
        GBE_local_lobby.broadcast_language_code,
        GBE_local_lobby.pass_key,
        0,
        message);
}

void Steam_Game_Coordinator::GBE_PublishSharedDotaLobbyState(const char *reason)
{
    if (is_server)
        return;

    GBE_shared_dota_lobby_state.valid = true;
    GBE_shared_dota_lobby_state.active = GBE_local_lobby.active;
    GBE_shared_dota_lobby_state.lobby_id = GBE_local_lobby.lobby_id;
    GBE_shared_dota_lobby_state.has_chat_channel = GBE_local_lobby.has_chat_channel;
    GBE_shared_dota_lobby_state.chat_channel_id = GBE_local_lobby.chat_channel_id;
    GBE_shared_dota_lobby_state.chat_channel_name = GBE_local_lobby.chat_channel_name;
    GBE_shared_dota_lobby_state.chat_channel_type = GBE_local_lobby.chat_channel_type;
    GBE_shared_dota_lobby_state.room_name = GBE_local_lobby.room_name;
    GBE_shared_dota_lobby_state.game_mode = GBE_local_lobby.game_mode;
    GBE_shared_dota_lobby_state.server_region = GBE_local_lobby.server_region;
    GBE_shared_dota_lobby_state.lan = GBE_local_lobby.lan;
    GBE_shared_dota_lobby_state.lan_host_ping_location = GBE_local_lobby.lan_host_ping_location;
    GBE_shared_dota_lobby_state.allow_cheats = GBE_local_lobby.allow_cheats;
    GBE_shared_dota_lobby_state.fill_with_bots = GBE_local_lobby.fill_with_bots;
    GBE_shared_dota_lobby_state.allow_spectating = GBE_local_lobby.allow_spectating;
    GBE_shared_dota_lobby_state.visibility = GBE_local_lobby.visibility;
    GBE_shared_dota_lobby_state.bot_difficulty_radiant = GBE_local_lobby.bot_difficulty_radiant;
    GBE_shared_dota_lobby_state.bot_difficulty_dire = GBE_local_lobby.bot_difficulty_dire;
    GBE_shared_dota_lobby_state.bot_radiant = GBE_local_lobby.bot_radiant;
    GBE_shared_dota_lobby_state.bot_dire = GBE_local_lobby.bot_dire;
    GBE_shared_dota_lobby_state.state = GBE_local_lobby.state;
    GBE_shared_dota_lobby_state.game_state = GBE_local_lobby.game_state;
    GBE_shared_dota_lobby_state.match_id = GBE_local_lobby.match_id;
    GBE_shared_dota_lobby_state.server_id = GBE_local_lobby.server_id;
    GBE_shared_dota_lobby_state.owner_steam_id = GBE_local_lobby.owner_steam_id;
    GBE_shared_dota_lobby_state.owner_account_id = GBE_local_lobby.owner_account_id;
    GBE_shared_dota_lobby_state.owner_name = GBE_local_lobby.owner_name;
    GBE_shared_dota_lobby_state.connect = GBE_local_lobby.connect;
    GBE_shared_dota_lobby_state.game_start_time = GBE_local_lobby.game_start_time;
    GBE_shared_dota_lobby_state.owner_team = GBE_local_lobby.owner_team;
    GBE_shared_dota_lobby_state.owner_slot = GBE_local_lobby.owner_slot;
    GBE_shared_dota_lobby_state.has_broadcast_channel = GBE_local_lobby.has_broadcast_channel;
    GBE_shared_dota_lobby_state.broadcast_channel_id = GBE_local_lobby.broadcast_channel_id;
    GBE_shared_dota_lobby_state.broadcast_country_code = GBE_local_lobby.broadcast_country_code;
    GBE_shared_dota_lobby_state.broadcast_description = GBE_local_lobby.broadcast_description;
    GBE_shared_dota_lobby_state.broadcast_language_code = GBE_local_lobby.broadcast_language_code;
    GBE_shared_dota_lobby_state.pass_key = GBE_local_lobby.pass_key;

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "published shared lobby this=%p shared_lobby=%p reason=%s active=%u lobby_id=%llu match_id=%llu owner_steam_id=%llu owner_account_id=%u state=%u game_state=%u team=%u slot=%u connect=%s",
        static_cast<void *>(this),
        static_cast<void *>(&GBE_shared_dota_lobby_state),
        reason ? reason : "unknown",
        GBE_local_lobby.active ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.owner_steam_id),
        GBE_local_lobby.owner_account_id,
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot,
        GBE_local_lobby.connect.c_str()
    );
}

void Steam_Game_Coordinator::GBE_RestoreSharedDotaLobbyState(const char *reason)
{
    if (!GBE_shared_dota_lobby_state.valid) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "restore skipped this=%p shared_lobby=%p reason=%s valid=0",
            static_cast<void *>(this),
            static_cast<void *>(&GBE_shared_dota_lobby_state),
            reason ? reason : "unknown"
        );
        return;
    }

    if (!is_server) {
        if (!GBE_local_lobby.active || !GBE_shared_dota_lobby_state.active)
            return;
        if (GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.lobby_id != GBE_shared_dota_lobby_state.lobby_id)
            return;

        bool changed = false;
        const uint64 previous_server_id = GBE_local_lobby.server_id;
        const std::string previous_connect = GBE_local_lobby.connect;

        if (GBE_shared_dota_lobby_state.server_id != 0 && GBE_local_lobby.server_id != GBE_shared_dota_lobby_state.server_id) {
            GBE_local_lobby.server_id = GBE_shared_dota_lobby_state.server_id;
            changed = true;
        }

        if (!GBE_shared_dota_lobby_state.connect.empty() && GBE_local_lobby.connect != GBE_shared_dota_lobby_state.connect) {
            GBE_local_lobby.connect = GBE_shared_dota_lobby_state.connect;
            changed = true;
        }

        if (GBE_shared_dota_lobby_state.match_id != 0 && GBE_local_lobby.match_id != GBE_shared_dota_lobby_state.match_id) {
            GBE_local_lobby.match_id = GBE_shared_dota_lobby_state.match_id;
            changed = true;
        }

        if (GBE_shared_dota_lobby_state.game_start_time != 0 && GBE_local_lobby.game_start_time != GBE_shared_dota_lobby_state.game_start_time) {
            GBE_local_lobby.game_start_time = GBE_shared_dota_lobby_state.game_start_time;
            changed = true;
        }

        if (changed) {
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "adopted shared runtime on client this=%p shared_lobby=%p reason=%s lobby_id=%llu old_server_id=%llu new_server_id=%llu old_connect=%s new_connect=%s",
                static_cast<void *>(this),
                static_cast<void *>(&GBE_shared_dota_lobby_state),
                reason ? reason : "unknown",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(previous_server_id),
                static_cast<unsigned long long>(GBE_local_lobby.server_id),
                previous_connect.c_str(),
                GBE_local_lobby.connect.c_str()
            );
        }
        return;
    }

    GBE_local_lobby.active = GBE_shared_dota_lobby_state.active;
    GBE_local_lobby.lobby_id = GBE_shared_dota_lobby_state.lobby_id;
    GBE_local_lobby.has_chat_channel = GBE_shared_dota_lobby_state.has_chat_channel;
    GBE_local_lobby.chat_channel_id = GBE_shared_dota_lobby_state.chat_channel_id;
    GBE_local_lobby.chat_channel_name = GBE_shared_dota_lobby_state.chat_channel_name;
    GBE_local_lobby.chat_channel_type = GBE_shared_dota_lobby_state.chat_channel_type;
    GBE_local_lobby.room_name = GBE_shared_dota_lobby_state.room_name;
    GBE_local_lobby.game_mode = GBE_shared_dota_lobby_state.game_mode;
    GBE_local_lobby.server_region = GBE_shared_dota_lobby_state.server_region;
    GBE_local_lobby.lan = GBE_shared_dota_lobby_state.lan;
    GBE_local_lobby.lan_host_ping_location = GBE_shared_dota_lobby_state.lan_host_ping_location;
    GBE_local_lobby.allow_cheats = GBE_shared_dota_lobby_state.allow_cheats;
    GBE_local_lobby.fill_with_bots = GBE_shared_dota_lobby_state.fill_with_bots;
    GBE_local_lobby.allow_spectating = GBE_shared_dota_lobby_state.allow_spectating;
    GBE_local_lobby.visibility = GBE_shared_dota_lobby_state.visibility;
    GBE_local_lobby.bot_difficulty_radiant = GBE_shared_dota_lobby_state.bot_difficulty_radiant;
    GBE_local_lobby.bot_difficulty_dire = GBE_shared_dota_lobby_state.bot_difficulty_dire;
    GBE_local_lobby.bot_radiant = GBE_shared_dota_lobby_state.bot_radiant;
    GBE_local_lobby.bot_dire = GBE_shared_dota_lobby_state.bot_dire;
    GBE_local_lobby.state = GBE_shared_dota_lobby_state.state;
    GBE_local_lobby.game_state = GBE_shared_dota_lobby_state.game_state;
    GBE_local_lobby.match_id = GBE_shared_dota_lobby_state.match_id;
    GBE_local_lobby.server_id = GBE_shared_dota_lobby_state.server_id;
    GBE_local_lobby.owner_steam_id = GBE_shared_dota_lobby_state.owner_steam_id;
    GBE_local_lobby.owner_account_id = GBE_shared_dota_lobby_state.owner_account_id;
    GBE_local_lobby.owner_name = GBE_shared_dota_lobby_state.owner_name;
    GBE_local_lobby.connect = GBE_shared_dota_lobby_state.connect;
    GBE_local_lobby.game_start_time = GBE_shared_dota_lobby_state.game_start_time;
    GBE_local_lobby.owner_team = GBE_shared_dota_lobby_state.owner_team;
    GBE_local_lobby.owner_slot = GBE_shared_dota_lobby_state.owner_slot;
    GBE_local_lobby.has_broadcast_channel = GBE_shared_dota_lobby_state.has_broadcast_channel;
    GBE_local_lobby.broadcast_channel_id = GBE_shared_dota_lobby_state.broadcast_channel_id;
    GBE_local_lobby.broadcast_country_code = GBE_shared_dota_lobby_state.broadcast_country_code;
    GBE_local_lobby.broadcast_description = GBE_shared_dota_lobby_state.broadcast_description;
    GBE_local_lobby.broadcast_language_code = GBE_shared_dota_lobby_state.broadcast_language_code;
    GBE_local_lobby.pass_key = GBE_shared_dota_lobby_state.pass_key;

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "restored shared lobby this=%p shared_lobby=%p reason=%s active=%u lobby_id=%llu match_id=%llu owner_steam_id=%llu owner_account_id=%u state=%u game_state=%u team=%u slot=%u connect=%s",
        static_cast<void *>(this),
        static_cast<void *>(&GBE_shared_dota_lobby_state),
        reason ? reason : "unknown",
        GBE_local_lobby.active ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.owner_steam_id),
        GBE_local_lobby.owner_account_id,
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot,
        GBE_local_lobby.connect.c_str()
    );
}

uint64 Steam_Game_Coordinator::GBE_GetDotaLobbyOwnerSteamId() const
{
    if (GBE_local_lobby.owner_steam_id != 0)
        return GBE_local_lobby.owner_steam_id;

    return settings->get_local_steam_id().ConvertToUint64();
}

uint32 Steam_Game_Coordinator::GBE_GetDotaLobbyOwnerAccountId() const
{
    if (GBE_local_lobby.owner_account_id != 0)
        return GBE_local_lobby.owner_account_id;

    return settings->get_local_steam_id().GetAccountID();
}

std::string Steam_Game_Coordinator::GBE_GetDotaLobbyOwnerName() const
{
    if (!GBE_local_lobby.owner_name.empty())
        return GBE_local_lobby.owner_name;

    return std::string(settings->get_local_name());
}

bool Steam_Game_Coordinator::GBE_TrySyncDotaLobbyServerIdFromGameServer(const char *reason)
{
    if (!is_server)
        return false;

    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.match_id == 0)
        return false;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_gameserver)
        return false;

    Steam_GameServer *game_server = steam_client->steam_gameserver;
    if (!game_server->BLoggedOn())
        return false;

    const uint64 server_id = game_server->GetSteamID().ConvertToUint64();
    if (server_id == 0 || GBE_local_lobby.server_id == server_id)
        return false;

    const uint64 previous_server_id = GBE_local_lobby.server_id;
    GBE_local_lobby.server_id = server_id;

    if (GBE_shared_dota_lobby_state.valid && GBE_shared_dota_lobby_state.lobby_id == GBE_local_lobby.lobby_id)
        GBE_shared_dota_lobby_state.server_id = server_id;

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "adopted game server SteamID as lobby server_id reason=%s lobby_id=%llu match_id=%llu old=%llu new=%llu",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(previous_server_id),
        static_cast<unsigned long long>(server_id)
    );

    if (previous_server_id == 0) {
        std::string launch_cache_message;
        const uint64 steam_id = GBE_GetDotaLobbyOwnerSteamId();
        const uint32 account_id = GBE_GetDotaLobbyOwnerAccountId();
        if (steam_id != 0 && account_id != 0 && GBE_BuildDotaPracticeLobbyCacheSubscribedPayload(
                steam_id,
                GBE_local_lobby.lobby_id,
                1u,
                0u,
                GBE_local_lobby.server_id,
                GBE_local_lobby.match_id,
                GBE_local_lobby.game_start_time,
                GBE_local_lobby.connect,
                GBE_GetDotaLobbyOwnerName(),
                GBE_local_lobby.room_name,
                GBE_local_lobby.game_mode,
                GBE_local_lobby.server_region,
                GBE_local_lobby.lan,
                GBE_local_lobby.lan_host_ping_location,
                GBE_local_lobby.allow_cheats,
                GBE_local_lobby.fill_with_bots,
                GBE_local_lobby.allow_spectating,
                GBE_local_lobby.visibility,
                GBE_local_lobby.bot_difficulty_radiant,
                GBE_local_lobby.bot_difficulty_dire,
                GBE_local_lobby.bot_radiant,
                GBE_local_lobby.bot_dire,
                GBE_local_lobby.owner_team,
                GBE_local_lobby.owner_slot,
                GBE_local_lobby.has_broadcast_channel,
                GBE_local_lobby.broadcast_channel_id,
                GBE_local_lobby.broadcast_country_code,
                GBE_local_lobby.broadcast_description,
                GBE_local_lobby.broadcast_language_code,
                GBE_local_lobby.pass_key,
                account_id,
                launch_cache_message)) {
            push_incoming_now(GBE_kDotaCacheSubscribed | GBE_kProtoMask, launch_cache_message);
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "queued launch CacheSubscribed after server_id sync reason=%s lobby_id=%llu match_id=%llu server_id=%llu size=%zu",
                reason ? reason : "unknown",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(GBE_local_lobby.match_id),
                static_cast<unsigned long long>(server_id),
                launch_cache_message.size()
            );
        } else {
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "failed building launch CacheSubscribed after server_id sync reason=%s lobby_id=%llu match_id=%llu server_id=%llu owner_steam_id=%llu owner_account_id=%u",
                reason ? reason : "unknown",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(GBE_local_lobby.match_id),
                static_cast<unsigned long long>(server_id),
                static_cast<unsigned long long>(steam_id),
                account_id
            );
        }
    }

    if (!GBE_SendDotaPracticeLobbyDetailsUpdate(false, nullptr, reason ? reason : "server_id_sync")) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "failed queueing direct 26 after server_id sync reason=%s lobby_id=%llu match_id=%llu server_id=%llu",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(server_id)
        );
        return false;
    }

    return true;
}

static bool GBE_ShouldPreferDotaLobbyConnectUpdate(const std::string &current_connect, const std::string &candidate_connect)
{
    if (candidate_connect.empty() || current_connect == candidate_connect)
        return false;

    if (current_connect.empty())
        return true;

    return current_connect == GBE_FormatDotaPracticeLobbyLoopbackConnect();
}

bool Steam_Game_Coordinator::GBE_HandleDotaDirectPostLoginRequest(uint32 unMsgType, const void *pubData, uint32 cubData)
{
    GBE_RestoreSharedDotaLobbyState("direct_post_login_request");

    const uint32 request_emsg = GBE_GC_MaskedEMsg(unMsgType);

    ProtoBufMsgHeader_t hdr{};
    CMsgProtoBufHeader protohdr;
    const uint8 *body = nullptr;
    size_t body_size = 0;
    if (!GBE_ParseDirectProtoContext(pubData, cubData, hdr, protohdr, body, body_size)) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed parsing direct request req=%u len=%u", request_emsg, cubData);
        return false;
    }

    const bool has_source_job = protohdr.has_job_id_source();
    const uint64 source_job = has_source_job ? protohdr.job_id_source() : 0ull;

    if (request_emsg == GBE_kDotaJoinChatChannel) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 7009 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );

        return GBE_HandleDotaJoinChatChannelRequest(
            std::string(reinterpret_cast<const char *>(body), body_size),
            false,
            nullptr
        );
    }

    if (request_emsg == GBE_kDotaPracticeLobbyCreate) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 7038 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );
        if (!has_source_job) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing request job for direct 7038 create request");
            return true;
        }

        return GBE_HandleDotaPracticeLobbyCreateRequest(
            std::string(reinterpret_cast<const char *>(body), body_size),
            source_job,
            false,
            nullptr
        );
    }

    if (request_emsg == GBE_kDotaPracticeLobbyLeave) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 7040 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );

        return GBE_HandleDotaPracticeLobbyLeaveRequest(false, nullptr);
    }

    if (request_emsg == GBE_kDotaPracticeLobbyLaunch) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 7041 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );

        return GBE_HandleDotaPracticeLobbyLaunchRequest(false, nullptr);
    }

    if (request_emsg == GBE_kDotaPracticeLobbySetDetails) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 7046 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );

        return GBE_HandleDotaPracticeLobbySetDetailsRequest(
            std::string(reinterpret_cast<const char *>(body), body_size),
            false,
            nullptr
        );
    }

    if (request_emsg == GBE_kDotaPracticeLobbySetTeamSlot) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 7047 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );

        return GBE_HandleDotaPracticeLobbySetTeamSlotRequest(
            std::string(reinterpret_cast<const char *>(body), body_size),
            source_job,
            has_source_job,
            false,
            nullptr
        );
    }

    if (request_emsg == GBE_kDotaPracticeLobbyJoinBroadcastChannel) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 7149 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );

        return GBE_HandleDotaPracticeLobbyJoinBroadcastChannelRequest(
            std::string(reinterpret_cast<const char *>(body), body_size),
            source_job,
            has_source_job,
            false,
            nullptr
        );
    }

    if (request_emsg == GBE_kDotaLobbyUpdateBroadcastChannelInfo) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 7367 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );

        return GBE_HandleDotaLobbyUpdateBroadcastChannelInfoRequest(
            std::string(reinterpret_cast<const char *>(body), body_size),
            false,
            nullptr
        );
    }

    if (request_emsg == GBE_kDotaPracticeLobbyCloseBroadcastChannel) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 8054 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );

        return GBE_HandleDotaPracticeLobbyCloseBroadcastChannelRequest(
            std::string(reinterpret_cast<const char *>(body), body_size),
            false,
            nullptr
        );
    }

    if (request_emsg == GBE_kDotaLeaveChatChannel) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 7272 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );

        return GBE_HandleDotaLeaveChatChannelRequest(
            std::string(reinterpret_cast<const char *>(body), body_size),
            false,
            nullptr
        );
    }

    if (request_emsg == GBE_kDotaDestroyLobbyRequest) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 8246 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );

        return GBE_HandleDotaDestroyLobbyRequest(source_job, has_source_job, false, nullptr);
    }

    const uint8 *template_bytes = nullptr;
    size_t template_size = 0;
    const char *template_hex = nullptr;
    uint32 response_emsg = 0;
    bool replace_account = false;
    bool replace_steam_id = false;
    const char *response_note = "";

    if (request_emsg == 8727) {
        std::string response_message;
        if (!GBE_BuildDota8728ResponsePayload(has_source_job, source_job, response_message)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, 8728u);
            return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu size=%zu note=%s",
            request_emsg,
            8728u,
            static_cast<unsigned long long>(source_job),
            response_message.size(),
            "8727->8728 minimal success"
        );
        push_incoming_now(8728u | GBE_kProtoMask, response_message);
        return true;
    }

    if (request_emsg == 8886) {
        std::string response_message;
        if (!GBE_BuildDota8887ResponsePayload(has_source_job, source_job, response_message)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, 8887u);
            return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu size=%zu note=%s",
            request_emsg,
            8887u,
            static_cast<unsigned long long>(source_job),
            response_message.size(),
            "8886->8887 minimal success"
        );
        push_incoming_now(8887u | GBE_kProtoMask, response_message);
        return true;
    }

    if (request_emsg == 7427) {
        std::string response_message;
        if (!GBE_BuildDota7428ResponsePayload(has_source_job, source_job, response_message)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, 7428u);
            return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu size=%zu note=%s",
            request_emsg,
            7428u,
            static_cast<unsigned long long>(source_job),
            response_message.size(),
            "7427->7428 minimal notifications response"
        );
        push_incoming_now(7428u | GBE_kProtoMask, response_message);
        return true;
    }

    if (request_emsg == 8793) {
        std::string response_message;
        if (!GBE_BuildDota8794ResponsePayload(has_source_job, source_job, response_message)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, 8794u);
            return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu size=%zu note=%s",
            request_emsg,
            8794u,
            static_cast<unsigned long long>(source_job),
            response_message.size(),
            "8793->8794 minimal success"
        );
        push_incoming_now(8794u | GBE_kProtoMask, response_message);
        return true;
    }

    if (request_emsg == 4523) {
        std::string response_message;
        if (!GBE_BuildDota4524ResponsePayload(has_source_job, source_job, response_message)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, 4524u);
            return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu size=%zu note=%s",
            request_emsg,
            4524u,
            static_cast<unsigned long long>(source_job),
            response_message.size(),
            "4523->4524 minimal upload_rate_modifier=1.0"
        );
        push_incoming_now(4524u | GBE_kProtoMask, response_message);
        return true;
    }

    if (request_emsg == 7534) {
        uint64 account_id_field = settings->get_local_steam_id().GetAccountID();
        GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 1), account_id_field);

        std::string response_message;
        if (!GBE_BuildDota7535ResponsePayload(static_cast<uint32>(account_id_field), has_source_job, source_job, response_message)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, 7535u);
            return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu size=%zu note=7534->7535 minimal profile card account_id=%u",
            request_emsg,
            7535u,
            static_cast<unsigned long long>(source_job),
            response_message.size(),
            static_cast<unsigned>(account_id_field)
        );
        push_incoming_now(7535u | GBE_kProtoMask, response_message);
        return true;
    }

    if (request_emsg == 7503) {
        const GBE_DotaEmptyRequestShape request_shape = GBE_ParseDotaEmptyRequestShape(body, body_size);
        const uint32 account_id = settings->get_local_steam_id().GetAccountID();
        std::string response_message;
        if (!GBE_BuildDota7504ResponsePayload(account_id, has_source_job, source_job, response_message)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, 7504u);
            return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu size=%zu note=7503->7504 parsed valid=%u fields=%u emoticon data account_id=%u",
            request_emsg,
            7504u,
            static_cast<unsigned long long>(source_job),
            response_message.size(),
            request_shape.valid ? 1u : 0u,
            request_shape.field_count,
            account_id
        );
        push_incoming_now(7504u | GBE_kProtoMask, response_message);
        return true;
    }

    if (request_emsg == 8095) {
        const GBE_DotaEmptyRequestShape request_shape = GBE_ParseDotaEmptyRequestShape(body, body_size);
        const uint32 account_id = settings->get_local_steam_id().GetAccountID();
        std::string response_message;
        if (!GBE_BuildDota8096ResponsePayload(account_id, has_source_job, source_job, response_message)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, 8096u);
            return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu size=%zu note=8095->8096 parsed valid=%u fields=%u conduct scorecard account_id=%u",
            request_emsg,
            8096u,
            static_cast<unsigned long long>(source_job),
            response_message.size(),
            request_shape.valid ? 1u : 0u,
            request_shape.field_count,
            account_id
        );
        push_incoming_now(8096u | GBE_kProtoMask, response_message);
        return true;
    }

    if (request_emsg == 8800) {
        const GBE_DotaEmptyRequestShape request_shape = GBE_ParseDotaEmptyRequestShape(body, body_size);
        std::string response_message;
        if (!GBE_BuildDota8801ResponsePayload(has_source_job, source_job, response_message)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, 8801u);
            return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu size=%zu note=8800->8801 parsed valid=%u fields=%u coaching summary success",
            request_emsg,
            8801u,
            static_cast<unsigned long long>(source_job),
            response_message.size(),
            request_shape.valid ? 1u : 0u,
            request_shape.field_count
        );
        push_incoming_now(8801u | GBE_kProtoMask, response_message);
        return true;
    }

    if (request_emsg == 8879) {
        const GBE_DotaRankRequestShape request_shape = GBE_ParseDotaRankRequestShape(body, body_size);
        std::string response_message;
        if (!GBE_BuildDota8880ResponsePayload(request_shape, has_source_job, source_job, response_message)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, 8880u);
            return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu size=%zu note=8879->8880 parsed valid=%u fields=%u has_rank_type=%u rank_type=%u",
            request_emsg,
            8880u,
            static_cast<unsigned long long>(source_job),
            response_message.size(),
            request_shape.valid ? 1u : 0u,
            request_shape.field_count,
            request_shape.has_rank_type ? 1u : 0u,
            request_shape.rank_type
        );
        push_incoming_now(8880u | GBE_kProtoMask, response_message);
        return true;
    }

    if (request_emsg == 7450) {
        std::vector<uint32> account_ids;
        if (!GBE_ExtractProtoPackedUint32Field(body, body_size, GBE_FindProtoField(body, body_size, 1), account_ids) || account_ids.empty())
            account_ids.push_back(settings->get_local_steam_id().GetAccountID());

        std::string response_message;
        if (!GBE_BuildDota7451BatchPlayerResourcesResponsePayload(account_ids, has_source_job, source_job, response_message)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, 7451u);
            return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu size=%zu note=7450->7451 minimal batch player resources accounts=%zu",
            request_emsg,
            7451u,
            static_cast<unsigned long long>(source_job),
            response_message.size(),
            account_ids.size()
        );
        push_incoming_now(7451u | GBE_kProtoMask, response_message);
        return true;
    }

    if (request_emsg == 7034) {
        const GBE_Dota7034RequestShape request_shape = GBE_ParseDota7034RequestShape(body, body_size);
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "parsed req=%u source_job=%llu body_size=%zu summary=%s",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatDota7034Summary(body, body_size).c_str()
        );

        std::string response_message;
        if (!GBE_BuildDota7034ConnectedPlayersResponsePayload(
                GBE_GetDotaLobbyOwnerSteamId(),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state,
                GBE_local_lobby.owner_team,
                GBE_local_lobby.owner_slot,
                request_shape,
                has_source_job,
                source_job,
                response_message)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, 7034u);
            return true;
        }

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
            "replying req=%u resp=%u source_job=%llu size=%zu note=7034 connected players game_state=%u team=%u slot=%u summary=%s",
            request_emsg,
            7034u,
            static_cast<unsigned long long>(source_job),
            response_message.size(),
            GBE_local_lobby.game_state,
            GBE_local_lobby.owner_team,
            GBE_local_lobby.owner_slot,
            GBE_FormatDota7034Summary(response_body, response_body_size).c_str()
        );
        push_incoming_now(7034u | GBE_kProtoMask, response_message);
        return true;
    }

    if (request_emsg == 7035) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "consumed req=%u source_job=%llu note=empty AbandonCurrentGame request body_size=%zu active=%u lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_local_lobby.active ? 1u : 0u,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id)
        );
        return true;
    }

    if (request_emsg == 4506) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "consumed req=%u source_job=%llu note=server available acknowledgement body_size=%zu active=%u lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_local_lobby.active ? 1u : 0u,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id)
        );
        return true;
    }

    if (request_emsg == 4511) {
        uint64 lobby_id = 0;
        GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 1u), lobby_id);

        const bool matches_local_lobby = (lobby_id != 0 && lobby_id == GBE_local_lobby.lobby_id);
        if (matches_local_lobby)
            GBE_TrySyncDotaLobbyServerIdFromGameServer("4511_lan_server_available");

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "consumed req=%u source_job=%llu note=lan server available notification lobby_id=%llu local_lobby_id=%llu matches_local=%u",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            static_cast<unsigned long long>(lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            matches_local_lobby ? 1u : 0u
        );
        return true;
    }

    if (request_emsg == 4508) {
        uint32 public_ip = 0;
        uint32 private_ip = 0;
        uint32 server_port = 0;
        uint32 tv_port = 0;
        uint32 server_type = 0;
        uint32 server_region = 0;
        uint32 relay_slots_max = 0;
        uint32 server_version = 0;
        uint32 server_cluster = 0;
        uint32 assigned_tv_port = 0;
        uint32 allow_custom_games = 0;
        uint32 build_version = 0;

        GBE_ExtractProtoFieldUint32(body, body_size, GBE_FindProtoField(body, body_size, 1u), public_ip);
        GBE_ExtractProtoFieldUint32(body, body_size, GBE_FindProtoField(body, body_size, 2u), private_ip);
        GBE_ExtractProtoFieldUint32(body, body_size, GBE_FindProtoField(body, body_size, 3u), server_port);
        GBE_ExtractProtoFieldUint32(body, body_size, GBE_FindProtoField(body, body_size, 4u), tv_port);
        GBE_ExtractProtoFieldUint32(body, body_size, GBE_FindProtoField(body, body_size, 7u), server_type);
        GBE_ExtractProtoFieldUint32(body, body_size, GBE_FindProtoField(body, body_size, 8u), server_region);
        GBE_ExtractProtoFieldUint32(body, body_size, GBE_FindProtoField(body, body_size, 13u), relay_slots_max);
        GBE_ExtractProtoFieldUint32(body, body_size, GBE_FindProtoField(body, body_size, 19u), server_version);
        GBE_ExtractProtoFieldUint32(body, body_size, GBE_FindProtoField(body, body_size, 20u), server_cluster);
        GBE_ExtractProtoFieldUint32(body, body_size, GBE_FindProtoField(body, body_size, 22u), assigned_tv_port);
        GBE_ExtractProtoFieldUint32(body, body_size, GBE_FindProtoField(body, body_size, 23u), allow_custom_games);
        GBE_ExtractProtoFieldUint32(body, body_size, GBE_FindProtoField(body, body_size, 24u), build_version);

        const uint32 connect_ip = private_ip != 0 ? private_ip : public_ip;
        const std::string runtime_connect = GBE_FormatDotaPracticeLobbyConnectFromIp(connect_ip);
        if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && GBE_ShouldPreferDotaLobbyConnectUpdate(GBE_local_lobby.connect, runtime_connect)) {
            const std::string previous_connect = GBE_local_lobby.connect;
            GBE_local_lobby.connect = runtime_connect;
            if (GBE_shared_dota_lobby_state.valid && GBE_shared_dota_lobby_state.lobby_id == GBE_local_lobby.lobby_id)
                GBE_shared_dota_lobby_state.connect = runtime_connect;

            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "adopted game server address as lobby connect reason=4508_game_server_info lobby_id=%llu previous=%s new=%s",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                previous_connect.c_str(),
                runtime_connect.c_str()
            );
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "consumed req=%u source_job=%llu note=game server info notification public_ip=%s private_ip=%s port=%u tv_port=%u assigned_tv_port=%u type=%u region=%u relay_slots=%u version=%u build=%u cluster=%u custom_games=%u",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            GBE_FormatIPv4(public_ip).c_str(),
            GBE_FormatIPv4(private_ip).c_str(),
            server_port,
            tv_port,
            assigned_tv_port,
            server_type,
            server_region,
            relay_slots_max,
            server_version,
            build_version,
            server_cluster,
            allow_custom_games
        );

        GBE_TrySyncDotaLobbyServerIdFromGameServer("4508_game_server_info");
        return true;
    }

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
        case 8676:
            template_bytes = GBE_kDota8677Template;
            template_size = sizeof(GBE_kDota8677Template);
            response_emsg = 8677;
            response_note = "8676->8677 + 8678 update";
            break;
        case 7387: {
            uint64 profile_selector = 0;
            if (!GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 1), profile_selector)) {
                GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed reading 7387 selector body_size=%zu", body_size);
                return true;
            }

            uint64 account_id_field = settings->get_local_steam_id().GetAccountID();
            GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 2), account_id_field);

            if (profile_selector == 0x20u) {
                template_bytes = GBE_kDota7388Profile20Template;
                template_size = sizeof(GBE_kDota7388Profile20Template);
                response_note = "7387 selector=0x20";
            } else if (profile_selector == 0x37u) {
                template_bytes = GBE_kDota7388Profile37Template;
                template_size = sizeof(GBE_kDota7388Profile37Template);
                response_note = "7387 selector=0x37";
            } else {
                std::string response_message;
                if (!GBE_BuildDota7388MinimalResponsePayload(
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
                    "replying req=%u resp=%u source_job=%llu size=%zu note=7387 selector=%llu minimal zero points account_id=%u",
                    request_emsg,
                    7388u,
                    static_cast<unsigned long long>(source_job),
                    response_message.size(),
                    static_cast<unsigned long long>(profile_selector),
                    static_cast<unsigned>(account_id_field)
                );
                push_incoming_now(7388u | GBE_kProtoMask, response_message);
                return true;
            }

            response_emsg = 7388;
            replace_account = true;
            break;
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
        default:
            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "no replay template req=%u source_job=%llu body_size=%zu body_prefix=%s",
                request_emsg,
                static_cast<unsigned long long>(source_job),
                body_size,
                GBE_FormatHexPrefix(body, body_size, 32).c_str()
            );
            return false;
    }

    std::string decoded_template;
    if (template_hex != nullptr) {
        if (!GBE_DecodeHexString(template_hex, decoded_template)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed decoding replay template req=%u resp=%u", request_emsg, response_emsg);
            return true;
        }

        template_bytes = reinterpret_cast<const uint8 *>(decoded_template.data());
        template_size = decoded_template.size();
    }

    std::string response_message;
    if (!GBE_BuildDotaDirectReplayMessage(
            template_bytes,
            template_size,
            settings->get_local_steam_id().GetAccountID(),
            settings->get_local_steam_id().ConvertToUint64(),
            replace_account,
            replace_steam_id,
            has_source_job,
            source_job,
            request_emsg,
            response_emsg,
            body_size,
            response_note,
            response_message)) {
        GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building replay req=%u resp=%u", request_emsg, response_emsg);
        return true;
    }

    GBE_GC_DebugLog("GC_DOTA_DIRECT", "replying req=%u resp=%u source_job=%llu size=%zu note=%s", request_emsg, response_emsg, static_cast<unsigned long long>(source_job), response_message.size(), response_note);
    push_incoming_now(response_emsg | GBE_kProtoMask, response_message);

    if (request_emsg == 8676) {
        std::string followup_message;
        if (!GBE_BuildDotaDirectReplayMessage(
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
        push_incoming_now(8678u | GBE_kProtoMask, followup_message);
    }

    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyCreateRequest(const std::string &request_body, uint64 request_job_id, bool wrapped, const std::string *outer_session_field_raw)
{
    GBE_local_lobby.active = true;
    GBE_local_lobby.lobby_id = GBE_GenerateDotaLobbyId();
    GBE_local_lobby.has_chat_channel = false;
    GBE_local_lobby.chat_channel_id = 0;
    GBE_local_lobby.chat_channel_name.clear();
    GBE_local_lobby.chat_channel_type = 0;
    GBE_local_lobby.room_name.clear();
    GBE_local_lobby.game_mode = 0;
    GBE_local_lobby.server_region = 0;
    GBE_local_lobby.lan = true;
    GBE_local_lobby.lan_host_ping_location.clear();
    GBE_local_lobby.allow_cheats = false;
    GBE_local_lobby.fill_with_bots = true;
    GBE_local_lobby.allow_spectating = false;
    GBE_local_lobby.visibility = 0;
    GBE_local_lobby.bot_difficulty_radiant = 0;
    GBE_local_lobby.bot_difficulty_dire = 4;
    GBE_local_lobby.bot_radiant = 0;
    GBE_local_lobby.bot_dire = 0;
    GBE_local_lobby.state = 0;
    GBE_local_lobby.game_state = 0;
    GBE_local_lobby.match_id = 0;
    GBE_local_lobby.server_id = 0;
    GBE_local_lobby.owner_steam_id = settings->get_local_steam_id().ConvertToUint64();
    GBE_local_lobby.owner_account_id = settings->get_local_steam_id().GetAccountID();
    GBE_local_lobby.owner_name = std::string(settings->get_local_name());
    GBE_local_lobby.connect.clear();
    GBE_local_lobby.game_start_time = 0;
    GBE_local_lobby.owner_team = 0;
    GBE_local_lobby.owner_slot = 1;
    GBE_local_lobby.has_broadcast_channel = false;
    GBE_local_lobby.broadcast_channel_id = 0;
    GBE_local_lobby.broadcast_country_code.clear();
    GBE_local_lobby.broadcast_description.clear();
    GBE_local_lobby.broadcast_language_code.clear();
    GBE_local_lobby.pass_key.clear();

    GBE_DotaPracticeLobbyCreateRequest request{};
    if (GBE_ParseDotaPracticeLobbyCreateBody(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        if (request.has_lobby_details) {
            const GBE_DotaPracticeLobbyDetailsRequest &details = request.lobby_details;
            if (details.has_room_name)
                GBE_local_lobby.room_name = details.room_name;
            if (details.has_server_region)
                GBE_local_lobby.server_region = details.server_region;
            if (details.has_lan)
                GBE_local_lobby.lan = details.lan;
            if (details.has_lan_host_ping_location)
                GBE_local_lobby.lan_host_ping_location = details.lan_host_ping_location;
            if (details.has_game_mode)
                GBE_local_lobby.game_mode = details.game_mode;
            if (details.has_bot_difficulty_radiant)
                GBE_local_lobby.bot_difficulty_radiant = details.bot_difficulty_radiant;
            if (details.has_allow_cheats)
                GBE_local_lobby.allow_cheats = details.allow_cheats;
            if (details.has_fill_with_bots)
                GBE_local_lobby.fill_with_bots = details.fill_with_bots;
            if (details.has_allow_spectating)
                GBE_local_lobby.allow_spectating = details.allow_spectating;
            if (details.has_visibility)
                GBE_local_lobby.visibility = details.visibility;
            if (details.has_bot_difficulty_dire)
                GBE_local_lobby.bot_difficulty_dire = details.bot_difficulty_dire;
            if (details.has_bot_radiant)
                GBE_local_lobby.bot_radiant = details.bot_radiant;
            if (details.has_bot_dire)
                GBE_local_lobby.bot_dire = details.bot_dire;
            if (details.has_pass_key)
                GBE_local_lobby.pass_key = details.pass_key;
        }

        if (request.has_pass_key && GBE_local_lobby.pass_key.empty())
            GBE_local_lobby.pass_key = request.pass_key;
    }

    GBE_PublishSharedDotaLobbyState("7038_create");

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] State creating path=%s request_job=%llu NewLobbyID=%llu room=%s server_region=%u lan=%u lan_ping=%s mode=%u pass_len=%zu",
        wrapped ? "wrapped" : "direct",
        static_cast<unsigned long long>(request_job_id),
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        GBE_local_lobby.room_name.c_str(),
        GBE_local_lobby.server_region,
        GBE_local_lobby.lan ? 1u : 0u,
        GBE_local_lobby.lan_host_ping_location.c_str(),
        GBE_local_lobby.game_mode,
        GBE_local_lobby.pass_key.size()
    );

    std::string response_24;
    const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    if (!GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay(GBE_GetDotaLobbyOwnerName(), response_24)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building template 24 cache update for LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    std::string response_7055;
    if (!GBE_BuildDotaPracticeLobbyResponsePayload(request_job_id, response_7055)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7055 payload for LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    if (wrapped) {
        if (!outer_session_field_raw) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
            return true;
        }

        std::string wrapped_24;
        if (!GBE_BuildWrappedDotaReplayMessage(response_24, *outer_session_field_raw, steam_id, wrapped_24)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 24 cache update for LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
            return true;
        }

        push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_24);
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Sent wrapped 24 cache update with NewLobbyID=%llu size=%zu body_prefix=%s packet_prefix=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            wrapped_24.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(response_24.data()), response_24.size(), 32).c_str(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(wrapped_24.data()), wrapped_24.size(), 32).c_str()
        );

        std::string wrapped_7055;
        if (!GBE_BuildWrappedDotaReplayMessage(response_7055, *outer_session_field_raw, steam_id, wrapped_7055)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 7055 payload for LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
            return true;
        }

        push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_7055);
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Sent wrapped 7055 with NewLobbyID=%llu request_job=%llu size=%zu body_prefix=%s packet_prefix=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(request_job_id),
            wrapped_7055.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(response_7055.data()), response_7055.size(), 32).c_str(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(wrapped_7055.data()), wrapped_7055.size(), 32).c_str()
        );

    } else {
        push_incoming_now(GBE_kDotaCacheSubscribed | GBE_kProtoMask, response_24);
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Sent direct 24 cache update with NewLobbyID=%llu size=%zu body_prefix=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            response_24.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(response_24.data()), response_24.size(), 32).c_str()
        );

        push_incoming_now(GBE_kDotaPracticeLobbyResponse | GBE_kProtoMask, response_7055);
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Sent direct 7055 with NewLobbyID=%llu request_job=%llu size=%zu body_prefix=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(request_job_id),
            response_7055.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(response_7055.data()), response_7055.size(), 32).c_str()
        );
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Skipping initial 26 details update for 7038 to match official create flow LobbyID=%llu",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id)
    );

    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaJoinChatChannelRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7009 because no local lobby is active");
        return true;
    }

    GBE_DotaJoinChatChannelRequest request{};
    if (!GBE_ParseDotaJoinChatChannelBody(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7009 body_size=%zu body_prefix=%s",
            request_body.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    GBE_local_lobby.has_chat_channel = true;
    if (GBE_local_lobby.chat_channel_id == 0)
        GBE_local_lobby.chat_channel_id = GBE_GenerateDotaChatChannelId();
    GBE_local_lobby.chat_channel_name = request.channel_name;
    GBE_local_lobby.chat_channel_type = request.has_channel_type ? request.channel_type : 3u;
    GBE_PublishSharedDotaLobbyState("7009_join_chat");

    std::string response_7010;
    if (!GBE_BuildDotaJoinChatChannelResponsePayload(
            settings->get_local_steam_id().ConvertToUint64(),
            GBE_local_lobby.chat_channel_id,
            GBE_local_lobby.chat_channel_name,
            std::string(settings->get_local_name()),
            GBE_local_lobby.chat_channel_type,
            response_7010)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7010 payload for LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    if (wrapped) {
        if (!outer_session_field_raw) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 7010 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
            return true;
        }

        std::string wrapped_7010;
        if (!GBE_BuildWrappedDotaReplayMessage(response_7010, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_7010)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 7010 payload for LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
            return true;
        }

        push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_7010);
    } else {
        push_incoming_now(GBE_kDotaJoinChatChannelResponse | GBE_kProtoMask, response_7010);
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Chat channel joined. name=%s channel_id=%llu channel_type=%u wrapped=%d",
        GBE_local_lobby.chat_channel_name.c_str(),
        static_cast<unsigned long long>(GBE_local_lobby.chat_channel_id),
        GBE_local_lobby.chat_channel_type,
        wrapped ? 1 : 0
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyLeaveRequest(bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7040 because no local lobby is active");
        return true;
    }

    const bool preserve_lobby_for_launch =
        !wrapped &&
        GBE_local_lobby.match_id == 0 &&
        GBE_local_lobby.server_id == 0 &&
        GBE_local_lobby.game_start_time == 0;

    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    std::string response_25;
    if (!GBE_BuildDotaLobbyCacheUnsubscribedPayload(lobby_id, response_25)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 payload for 7040 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
        return true;
    }

    if (wrapped) {
        if (!outer_session_field_raw) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 7040 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
            return true;
        }

        std::string wrapped_25;
        if (!GBE_BuildWrappedDotaReplayMessage(response_25, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_25)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 25 payload for 7040 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
            return true;
        }

        push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_25);
    } else {
        push_incoming_now(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25);
    }

    if (preserve_lobby_for_launch) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Preserving local lobby after direct 7040 for pending launch LobbyID=%llu owner_steam_id=%llu state=%u game_state=%u",
            static_cast<unsigned long long>(lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.owner_steam_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state
        );
        return true;
    }

    GBE_local_lobby.active = false;
    GBE_local_lobby.lobby_id = 0;
    GBE_local_lobby.room_name.clear();
    GBE_local_lobby.game_mode = 0;
    GBE_local_lobby.server_region = 0;
    GBE_local_lobby.lan = true;
    GBE_local_lobby.lan_host_ping_location.clear();
    GBE_local_lobby.allow_cheats = false;
    GBE_local_lobby.fill_with_bots = true;
    GBE_local_lobby.allow_spectating = false;
    GBE_local_lobby.visibility = 0;
    GBE_local_lobby.bot_difficulty_radiant = 0;
    GBE_local_lobby.bot_difficulty_dire = 4;
    GBE_local_lobby.bot_radiant = 0;
    GBE_local_lobby.bot_dire = 0;
    GBE_local_lobby.state = 0;
    GBE_local_lobby.game_state = 0;
    GBE_local_lobby.match_id = 0;
    GBE_local_lobby.server_id = 0;
    GBE_local_lobby.owner_steam_id = 0;
    GBE_local_lobby.owner_account_id = 0;
    GBE_local_lobby.owner_name.clear();
    GBE_local_lobby.connect.clear();
    GBE_local_lobby.game_start_time = 0;
    GBE_local_lobby.owner_team = 0;
    GBE_local_lobby.owner_slot = 1;
    GBE_local_lobby.has_broadcast_channel = false;
    GBE_local_lobby.broadcast_channel_id = 0;
    GBE_local_lobby.broadcast_country_code.clear();
    GBE_local_lobby.broadcast_description.clear();
    GBE_local_lobby.broadcast_language_code.clear();
    GBE_local_lobby.pass_key.clear();
    GBE_PublishSharedDotaLobbyState("7040_leave");

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Lobby left. unsubscribed LobbyID=%llu wrapped=%d chat_channel_id=%llu",
        static_cast<unsigned long long>(lobby_id),
        wrapped ? 1 : 0,
        static_cast<unsigned long long>(GBE_local_lobby.chat_channel_id)
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyLaunchRequest(bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7041 because no local lobby is active");
        return true;
    }

    if (wrapped && !outer_session_field_raw) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 7041 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    const uint32 account_id = settings->get_local_steam_id().GetAccountID();

    GBE_local_lobby.match_id = GBE_GenerateDotaMatchId();
    GBE_local_lobby.server_id = 0;
    GBE_local_lobby.connect = GBE_FormatDotaPracticeLobbyConnectFromIp(network ? network->getOwnIP() : 0);
    GBE_local_lobby.game_start_time = static_cast<uint32>(std::time(nullptr));
    GBE_PublishSharedDotaLobbyState("7041_launch_init");

    std::array<std::string, 4> stage_messages;
    for (size_t stage_index = 0; stage_index < stage_messages.size(); ++stage_index) {
        if (!GBE_BuildDotaPracticeLobbyLaunchStagePayload(
                stage_index,
                account_id,
                steam_id,
                GBE_local_lobby.lobby_id,
                GBE_local_lobby.server_id,
                GBE_local_lobby.match_id,
                GBE_local_lobby.game_start_time,
                GBE_local_lobby.connect,
                GBE_local_lobby.owner_name,
                GBE_local_lobby.room_name,
                GBE_local_lobby.game_mode,
                GBE_local_lobby.server_region,
                GBE_local_lobby.lan,
                GBE_local_lobby.lan_host_ping_location,
                GBE_local_lobby.allow_cheats,
                GBE_local_lobby.fill_with_bots,
                GBE_local_lobby.allow_spectating,
                GBE_local_lobby.visibility,
                GBE_local_lobby.bot_difficulty_radiant,
                GBE_local_lobby.bot_difficulty_dire,
                GBE_local_lobby.bot_radiant,
                GBE_local_lobby.bot_dire,
                GBE_local_lobby.owner_team,
                GBE_local_lobby.owner_slot,
                GBE_local_lobby.pass_key,
                stage_messages[stage_index])) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Failed building 7041 launch stage=%zu LobbyID=%llu match_id=%llu server_id=%llu connect=%s",
                stage_index + 1,
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(GBE_local_lobby.match_id),
                static_cast<unsigned long long>(GBE_local_lobby.server_id),
                GBE_local_lobby.connect.c_str()
            );
            return true;
        }
    }

    // Host captures show three UpdateMultiple packets landing before TicketAuthComplete,
    // with only one PersonaState inserted between the 2nd and 3rd deferred updates.
    const std::array<double, 4> stage_delays = { 0.0, 0.115, 0.116, 0.118 };
    const std::array<uint32, 4> stage_states = { 1u, 1u, 2u, 2u };
    const std::array<uint32, 4> stage_game_states = { 0u, 0u, 0u, 1u };
    for (size_t stage_index = 0; stage_index < stage_messages.size(); ++stage_index) {
        std::string outbound_message = stage_messages[stage_index];
        if (wrapped) {
            std::string wrapped_message;
            if (!GBE_BuildWrappedDotaReplayMessage(outbound_message, *outer_session_field_raw, steam_id, wrapped_message)) {
                GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 7041 launch stage=%zu LobbyID=%llu", stage_index + 1, static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
                return true;
            }
            outbound_message.swap(wrapped_message);
        }

        if (stage_index == 0) {
            push_incoming_now(
                (wrapped ? GBE_kEMsgClientFromGC : GBE_kDotaPracticeLobbyDetailsUpdate) | GBE_kProtoMask,
                outbound_message,
                true,
                stage_states[stage_index],
                stage_game_states[stage_index]
            );
        } else {
            push_incoming(
                (wrapped ? GBE_kEMsgClientFromGC : GBE_kDotaPracticeLobbyDetailsUpdate) | GBE_kProtoMask,
                outbound_message,
                stage_delays[stage_index],
                true,
                stage_states[stage_index],
                stage_game_states[stage_index]
            );
        }

        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Sent 7041 launch stage=%zu path=%s LobbyID=%llu match_id=%llu server_id=%llu game_start=%u connect=%s size=%zu body_prefix=%s",
            stage_index + 1,
            wrapped ? "wrapped" : "direct",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id),
            GBE_local_lobby.game_start_time,
            GBE_local_lobby.connect.c_str(),
            outbound_message.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(outbound_message.data()), outbound_message.size(), 32).c_str()
        );
    }

    static const std::array<GBE_DotaPracticeLobbyLaunchPeripheralTemplate, 13> peripheral_templates = {{
        { GBE_kSteamPersonaState, GBE_kDotaPracticeLobbyLaunchPersonaStateInitHex, 0.05, false },
        { GBE_kSteamServersAvailable, GBE_kDotaPracticeLobbyLaunchServersAvailableHex, 0.06, false },
        { GBE_kSteamAuthListAck, GBE_kDotaPracticeLobbyLaunchAuthListAckStage1Hex, 0.07, false },
        { GBE_kSteamGameConnectTokens, GBE_kDotaPracticeLobbyLaunchGameConnectTokensStage1Hex, 0.08, false },
        { GBE_kSteamPersonaState, GBE_kDotaPracticeLobbyLaunchPersonaStateSetupHex, 0.09, false },
        { GBE_kSteamAuthListAck, GBE_kDotaPracticeLobbyLaunchAuthListAckStage2Hex, 0.10, false },
        { GBE_kSteamGameConnectTokens, GBE_kDotaPracticeLobbyLaunchGameConnectTokensStage2Hex, 0.11, false },
        { GBE_kSteamPersonaState, GBE_kDotaPracticeLobbyLaunchPersonaStateRunHex, 0.117, false },
        { GBE_kSteamTicketAuthComplete, GBE_kDotaPracticeLobbyLaunchTicketAuthCompleteHex, 0.119, true },
        { GBE_kSteamGameConnectTokens, GBE_kDotaPracticeLobbyLaunchGameConnectTokensStage3Hex, 0.120, false },
        { GBE_kSteamPersonaState, GBE_kDotaPracticeLobbyLaunchPersonaStateServerRunHex, 0.121, true },
        { GBE_kSteamPersonaState, GBE_kDotaPracticeLobbyLaunchPersonaStatePrivateLobbyHex, 0.122, false },
        { GBE_kSteamPersonaState, GBE_kDotaPracticeLobbyLaunchPersonaStateRunHex, 0.123, false },
    }};

    for (const auto &peripheral_template : peripheral_templates) {
        std::string peripheral_message;
        if (!GBE_BuildDotaPracticeLobbyLaunchPeripheralMessage(
                peripheral_template.hex,
                steam_id,
                GBE_local_lobby.lobby_id,
                GBE_local_lobby.server_id,
                peripheral_template.patch_server_id,
                peripheral_message)) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Failed building 7041 peripheral emsg=%u LobbyID=%llu server_id=%llu",
                peripheral_template.emsg,
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(GBE_local_lobby.server_id)
            );
            return true;
        }

        push_incoming(peripheral_template.emsg | GBE_kProtoMask, peripheral_message, peripheral_template.delay);
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Sent 7041 peripheral emsg=%u LobbyID=%llu delay=%.2f size=%zu body_prefix=%s",
            peripheral_template.emsg,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            peripheral_template.delay,
            peripheral_message.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(peripheral_message.data()), peripheral_message.size(), 24).c_str()
        );
    }

    return true;
}

bool Steam_Game_Coordinator::GBE_SendDotaPracticeLobbyDetailsUpdate(bool wrapped, const std::string *outer_session_field_raw, const char *reason)
{
    GBE_RestoreSharedDotaLobbyState(reason ? reason : "details_update");

    std::string response_26;
    if (!GBE_BuildDotaPracticeLobbyDetailsUpdatePayload(
            GBE_GetDotaLobbyOwnerSteamId(),
            GBE_local_lobby.lobby_id,
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            GBE_local_lobby.server_id,
            GBE_local_lobby.match_id,
            GBE_local_lobby.game_start_time,
            GBE_local_lobby.connect,
            GBE_GetDotaLobbyOwnerName(),
            GBE_local_lobby.room_name,
            GBE_local_lobby.game_mode,
            GBE_local_lobby.server_region,
            GBE_local_lobby.lan,
            GBE_local_lobby.lan_host_ping_location,
            GBE_local_lobby.allow_cheats,
            GBE_local_lobby.fill_with_bots,
            GBE_local_lobby.allow_spectating,
            GBE_local_lobby.visibility,
            GBE_local_lobby.bot_difficulty_radiant,
            GBE_local_lobby.bot_difficulty_dire,
            GBE_local_lobby.bot_radiant,
            GBE_local_lobby.bot_dire,
            GBE_local_lobby.owner_team,
            GBE_local_lobby.owner_slot,
            GBE_local_lobby.has_broadcast_channel,
            GBE_local_lobby.broadcast_channel_id,
            GBE_local_lobby.broadcast_country_code,
            GBE_local_lobby.broadcast_description,
            GBE_local_lobby.broadcast_language_code,
            GBE_local_lobby.pass_key,
            response_26)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 26 details update for LobbyID=%llu reason=%s", static_cast<unsigned long long>(GBE_local_lobby.lobby_id), reason ? reason : "unknown");
        return false;
    }

    if (wrapped) {
        if (!outer_session_field_raw) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 26 LobbyID=%llu reason=%s", static_cast<unsigned long long>(GBE_local_lobby.lobby_id), reason ? reason : "unknown");
            return false;
        }

        std::string wrapped_26;
        if (!GBE_BuildWrappedDotaReplayMessage(response_26, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_26)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 26 details update for LobbyID=%llu reason=%s", static_cast<unsigned long long>(GBE_local_lobby.lobby_id), reason ? reason : "unknown");
            return false;
        }

        push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_26);
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Sent wrapped 26 details update LobbyID=%llu reason=%s size=%zu body_prefix=%s packet_prefix=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            reason ? reason : "unknown",
            wrapped_26.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(response_26.data()), response_26.size(), 32).c_str(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(wrapped_26.data()), wrapped_26.size(), 32).c_str()
        );
    } else {
        push_incoming_now(GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask, response_26);
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Sent direct 26 details update LobbyID=%llu reason=%s size=%zu body_prefix=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            reason ? reason : "unknown",
            response_26.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(response_26.data()), response_26.size(), 32).c_str()
        );
    }

    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbySetDetailsRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7046 because no local lobby is active");
        return true;
    }

    GBE_DotaPracticeLobbyDetailsRequest request{};
    if (!GBE_ParseDotaPracticeLobbySetDetailsBody(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7046 body_size=%zu body_prefix=%s",
            request_body.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    if (request.has_lobby_id && request.lobby_id != GBE_local_lobby.lobby_id) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] 7046 LobbyID mismatch request=%llu local=%llu, keeping local state",
            static_cast<unsigned long long>(request.lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id)
        );
    }

    if (request.has_room_name)
        GBE_local_lobby.room_name = request.room_name;
    if (request.has_server_region)
        GBE_local_lobby.server_region = request.server_region;
    if (request.has_lan)
        GBE_local_lobby.lan = request.lan;
    if (request.has_lan_host_ping_location)
        GBE_local_lobby.lan_host_ping_location = request.lan_host_ping_location;
    if (request.has_game_mode)
        GBE_local_lobby.game_mode = request.game_mode;
    if (request.has_bot_difficulty_radiant)
        GBE_local_lobby.bot_difficulty_radiant = request.bot_difficulty_radiant;
    if (request.has_allow_cheats)
        GBE_local_lobby.allow_cheats = request.allow_cheats;
    if (request.has_fill_with_bots)
        GBE_local_lobby.fill_with_bots = request.fill_with_bots;
    if (request.has_allow_spectating)
        GBE_local_lobby.allow_spectating = request.allow_spectating;
    if (request.has_pass_key)
        GBE_local_lobby.pass_key = request.pass_key;
    if (request.has_visibility)
        GBE_local_lobby.visibility = request.visibility;
    if (request.has_bot_difficulty_dire)
        GBE_local_lobby.bot_difficulty_dire = request.bot_difficulty_dire;
    if (request.has_bot_radiant)
        GBE_local_lobby.bot_radiant = request.bot_radiant;
    if (request.has_bot_dire)
        GBE_local_lobby.bot_dire = request.bot_dire;
    GBE_PublishSharedDotaLobbyState("7046_set_details");

    if (!GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7046"))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Room details updated. mode=%u server_region=%u lan=%u lan_ping=%s cheats=%u bots=%u spectating=%u visibility=%u bot_diff_r=%u bot_diff_d=%u bot_radiant=%llu bot_dire=%llu name=%s password_len=%zu",
        GBE_local_lobby.game_mode,
        GBE_local_lobby.server_region,
        GBE_local_lobby.lan ? 1u : 0u,
        GBE_local_lobby.lan_host_ping_location.c_str(),
        GBE_local_lobby.allow_cheats ? 1u : 0u,
        GBE_local_lobby.fill_with_bots ? 1u : 0u,
        GBE_local_lobby.allow_spectating ? 1u : 0u,
        GBE_local_lobby.visibility,
        GBE_local_lobby.bot_difficulty_radiant,
        GBE_local_lobby.bot_difficulty_dire,
        static_cast<unsigned long long>(GBE_local_lobby.bot_radiant),
        static_cast<unsigned long long>(GBE_local_lobby.bot_dire),
        GBE_local_lobby.room_name.c_str(),
        GBE_local_lobby.pass_key.size()
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbySetTeamSlotRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7047 because no local lobby is active");
        return true;
    }

    GBE_DotaPracticeLobbySetTeamSlotRequest request{};
    if (!GBE_ParseDotaPracticeLobbySetTeamSlotBody(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7047 body_size=%zu body_prefix=%s",
            request_body.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    if (request.has_team)
        GBE_local_lobby.owner_team = request.team;
    if (request.has_slot)
        GBE_local_lobby.owner_slot = request.slot;
    if (request.has_bot_difficulty) {
        if (GBE_local_lobby.owner_team == 1u)
            GBE_local_lobby.bot_difficulty_dire = request.bot_difficulty;
        else
            GBE_local_lobby.bot_difficulty_radiant = request.bot_difficulty;
    }
    GBE_PublishSharedDotaLobbyState("7047_set_team_slot");

    if (!GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7047"))
        return true;

    if (has_request_job) {
        std::string response_7055;
        if (!GBE_BuildDotaPracticeLobbyResponsePayload(request_job_id, response_7055)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7055 payload for 7047 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
            return true;
        }

        if (wrapped) {
            if (!outer_session_field_raw) {
                GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 7047 7055 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
                return true;
            }

            std::string wrapped_7055;
            if (!GBE_BuildWrappedDotaReplayMessage(response_7055, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_7055)) {
                GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 7055 payload for 7047 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
                return true;
            }

            push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_7055);
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Sent wrapped 7055 ack for 7047 LobbyID=%llu request_job=%llu size=%zu body_prefix=%s packet_prefix=%s",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(request_job_id),
                wrapped_7055.size(),
                GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(response_7055.data()), response_7055.size(), 32).c_str(),
                GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(wrapped_7055.data()), wrapped_7055.size(), 32).c_str()
            );
        } else {
            push_incoming_now(GBE_kDotaPracticeLobbyResponse | GBE_kProtoMask, response_7055);
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Sent direct 7055 ack for 7047 LobbyID=%llu request_job=%llu size=%zu body_prefix=%s",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(request_job_id),
                response_7055.size(),
                GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(response_7055.data()), response_7055.size(), 32).c_str()
            );
        }
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Team slot updated. team=%u slot=%u bot_diff_req=%u has_bot_diff=%d",
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot,
        request.bot_difficulty,
        request.has_bot_difficulty ? 1 : 0
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaLeaveChatChannelRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    GBE_DotaLeaveChatChannelRequest request{};
    if (!GBE_ParseDotaLeaveChatChannelBody(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7272 body_size=%zu body_prefix=%s",
            request_body.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    const uint64 channel_id = request.channel_id != 0 ? request.channel_id : GBE_local_lobby.chat_channel_id;
    if (channel_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7272 because no chat channel is active");
        return true;
    }

    std::string response_7014;
    if (!GBE_BuildDotaOtherLeftChannelPayload(channel_id, settings->get_local_steam_id().ConvertToUint64(), response_7014)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7014 payload for channel=%llu", static_cast<unsigned long long>(channel_id));
        return true;
    }

    if (wrapped) {
        if (!outer_session_field_raw) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 7272 channel=%llu", static_cast<unsigned long long>(channel_id));
            return true;
        }

        std::string wrapped_7014;
        if (!GBE_BuildWrappedDotaReplayMessage(response_7014, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_7014)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 7014 payload for channel=%llu", static_cast<unsigned long long>(channel_id));
            return true;
        }

        push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_7014);
    } else {
        push_incoming_now(GBE_kDotaOtherLeftChannel | GBE_kProtoMask, response_7014);
    }

    GBE_local_lobby.has_chat_channel = false;
    GBE_local_lobby.chat_channel_id = 0;
    GBE_local_lobby.chat_channel_name.clear();
    GBE_local_lobby.chat_channel_type = 0;
    GBE_PublishSharedDotaLobbyState("7272_leave_chat");

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Chat channel left. channel=%llu wrapped=%d",
        static_cast<unsigned long long>(channel_id),
        wrapped ? 1 : 0
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyJoinBroadcastChannelRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7149 because no local lobby is active");
        return true;
    }

    GBE_DotaPracticeLobbyBroadcastChannelRequest request{};
    if (!GBE_ParseDotaPracticeLobbyJoinBroadcastChannelBody(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7149 body_size=%zu body_prefix=%s",
            request_body.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    GBE_local_lobby.has_broadcast_channel = true;
    GBE_local_lobby.broadcast_channel_id = request.channel;
    GBE_local_lobby.broadcast_country_code = request.has_country_code ? request.country_code : std::string();
    GBE_local_lobby.broadcast_description = request.has_description ? request.description : std::string();
    GBE_local_lobby.broadcast_language_code = request.has_language_code ? request.language_code : std::string();
    GBE_PublishSharedDotaLobbyState("7149_join_broadcast");

    if (!GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7149"))
        return true;

    if (has_request_job) {
        std::string response_7055;
        if (!GBE_BuildDotaPracticeLobbyResponsePayload(request_job_id, response_7055)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7055 payload for 7149 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
            return true;
        }

        if (wrapped) {
            if (!outer_session_field_raw) {
                GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 7149 7055 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
                return true;
            }

            std::string wrapped_7055;
            if (!GBE_BuildWrappedDotaReplayMessage(response_7055, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_7055)) {
                GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 7055 payload for 7149 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
                return true;
            }

            push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_7055);
        } else {
            push_incoming_now(GBE_kDotaPracticeLobbyResponse | GBE_kProtoMask, response_7055);
        }
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Broadcast channel joined. channel=%u country=%s description=%s language=%s",
        GBE_local_lobby.broadcast_channel_id,
        GBE_local_lobby.broadcast_country_code.c_str(),
        GBE_local_lobby.broadcast_description.c_str(),
        GBE_local_lobby.broadcast_language_code.c_str()
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaLobbyUpdateBroadcastChannelInfoRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7367 because no local lobby is active");
        return true;
    }

    GBE_DotaPracticeLobbyBroadcastChannelRequest request{};
    if (!GBE_ParseDotaLobbyUpdateBroadcastChannelInfoBody(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7367 body_size=%zu body_prefix=%s",
            request_body.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    GBE_local_lobby.has_broadcast_channel = true;
    GBE_local_lobby.broadcast_channel_id = request.channel;
    if (request.has_country_code)
        GBE_local_lobby.broadcast_country_code = request.country_code;
    if (request.has_description)
        GBE_local_lobby.broadcast_description = request.description;
    if (request.has_language_code)
        GBE_local_lobby.broadcast_language_code = request.language_code;
    GBE_PublishSharedDotaLobbyState("7367_update_broadcast");

    if (!GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7367"))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Broadcast channel info updated. channel=%u country=%s description=%s language=%s",
        GBE_local_lobby.broadcast_channel_id,
        GBE_local_lobby.broadcast_country_code.c_str(),
        GBE_local_lobby.broadcast_description.c_str(),
        GBE_local_lobby.broadcast_language_code.c_str()
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyCloseBroadcastChannelRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 8054 because no local lobby is active");
        return true;
    }

    GBE_DotaPracticeLobbyBroadcastChannelRequest request{};
    if (!GBE_ParseDotaPracticeLobbyCloseBroadcastChannelBody(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 8054 body_size=%zu body_prefix=%s",
            request_body.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    GBE_local_lobby.has_broadcast_channel = false;
    GBE_local_lobby.broadcast_channel_id = request.channel;
    GBE_local_lobby.broadcast_country_code.clear();
    GBE_local_lobby.broadcast_description.clear();
    GBE_local_lobby.broadcast_language_code.clear();
    GBE_PublishSharedDotaLobbyState("8054_close_broadcast");

    if (!GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "8054"))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Broadcast channel closed. channel=%u",
        request.channel
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaDestroyLobbyRequest(uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 8246 because no local lobby is active");
        return true;
    }

    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    std::string response_25;
    if (!GBE_BuildDotaLobbyCacheUnsubscribedPayload(lobby_id, response_25)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 payload for 8246 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
        return true;
    }

    if (wrapped) {
        if (!outer_session_field_raw) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 8246 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
            return true;
        }

        std::string wrapped_25;
        if (!GBE_BuildWrappedDotaReplayMessage(response_25, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_25)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 25 payload for 8246 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
            return true;
        }

        push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_25);
    } else {
        push_incoming_now(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25);
    }

    if (has_request_job) {
        std::string response_8247;
        if (!GBE_BuildDotaDestroyLobbyResponsePayload(request_job_id, response_8247)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 8247 payload for LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
            return true;
        }

        if (wrapped) {
            std::string wrapped_8247;
            if (!GBE_BuildWrappedDotaReplayMessage(response_8247, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_8247)) {
                GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 8247 payload for LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
                return true;
            }

            push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_8247);
        } else {
            push_incoming_now(GBE_kDotaDestroyLobbyResponse | GBE_kProtoMask, response_8247);
        }
    }

    GBE_local_lobby.active = false;
    GBE_local_lobby.lobby_id = 0;
    GBE_local_lobby.room_name.clear();
    GBE_local_lobby.game_mode = 0;
    GBE_local_lobby.server_region = 0;
    GBE_local_lobby.lan = true;
    GBE_local_lobby.lan_host_ping_location.clear();
    GBE_local_lobby.allow_cheats = false;
    GBE_local_lobby.fill_with_bots = true;
    GBE_local_lobby.allow_spectating = false;
    GBE_local_lobby.visibility = 0;
    GBE_local_lobby.bot_difficulty_radiant = 0;
    GBE_local_lobby.bot_difficulty_dire = 4;
    GBE_local_lobby.bot_radiant = 0;
    GBE_local_lobby.bot_dire = 0;
    GBE_local_lobby.state = 0;
    GBE_local_lobby.game_state = 0;
    GBE_local_lobby.match_id = 0;
    GBE_local_lobby.server_id = 0;
    GBE_local_lobby.owner_steam_id = 0;
    GBE_local_lobby.owner_account_id = 0;
    GBE_local_lobby.owner_name.clear();
    GBE_local_lobby.connect.clear();
    GBE_local_lobby.game_start_time = 0;
    GBE_local_lobby.owner_team = 0;
    GBE_local_lobby.owner_slot = 1;
    GBE_local_lobby.has_broadcast_channel = false;
    GBE_local_lobby.broadcast_channel_id = 0;
    GBE_local_lobby.broadcast_country_code.clear();
    GBE_local_lobby.broadcast_description.clear();
    GBE_local_lobby.broadcast_language_code.clear();
    GBE_local_lobby.pass_key.clear();
    GBE_PublishSharedDotaLobbyState("8246_destroy");

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Lobby destroyed. unsubscribed LobbyID=%llu wrapped=%d request_job=%llu has_job=%d chat_channel_id=%llu",
        static_cast<unsigned long long>(lobby_id),
        wrapped ? 1 : 0,
        static_cast<unsigned long long>(request_job_id),
        has_request_job ? 1 : 0,
        static_cast<unsigned long long>(GBE_local_lobby.chat_channel_id)
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaWrappedPostLoginRequest(const void *pubData, uint32 cubData)
{
    GBE_DotaWrappedDirectContext context{};
    if (!GBE_ExtractWrappedDotaDirectContext(pubData, cubData, context))
        return false;

    if (context.inner_emsg != GBE_kDotaJoinChatChannel)
        if (context.inner_emsg != GBE_kDotaPracticeLobbyCreate)
            if (context.inner_emsg != GBE_kDotaPracticeLobbyLeave)
                if (context.inner_emsg != GBE_kDotaPracticeLobbyLaunch)
                    if (context.inner_emsg != GBE_kDotaPracticeLobbySetDetails)
                        if (context.inner_emsg != GBE_kDotaPracticeLobbySetTeamSlot)
                            if (context.inner_emsg != GBE_kDotaPracticeLobbyJoinBroadcastChannel)
                                if (context.inner_emsg != GBE_kDotaLobbyUpdateBroadcastChannelInfo)
                                    if (context.inner_emsg != GBE_kDotaLeaveChatChannel)
                                        if (context.inner_emsg != GBE_kDotaPracticeLobbyCloseBroadcastChannel)
                                            if (context.inner_emsg != GBE_kDotaDestroyLobbyRequest)
                                                return false;

    if (context.inner_emsg == GBE_kDotaJoinChatChannel) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 7009 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaJoinChatChannelRequest(context.inner_body_raw, true, &context.outer_session_field_raw);
    }

    if (context.inner_emsg == GBE_kDotaPracticeLobbyCreate) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 7038 has_job=%d request_job=%llu session_raw_size=%zu",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size()
        );

        if (!context.has_request_job) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing request job for 7038 create request");
            return true;
        }

        return GBE_HandleDotaPracticeLobbyCreateRequest(
            context.inner_body_raw,
            context.request_job_id,
            true,
            &context.outer_session_field_raw
        );
    }

    if (context.inner_emsg == GBE_kDotaPracticeLobbySetDetails)
    {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 7046 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaPracticeLobbySetDetailsRequest(context.inner_body_raw, true, &context.outer_session_field_raw);
    }

    if (context.inner_emsg == GBE_kDotaPracticeLobbyLeave) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 7040 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaPracticeLobbyLeaveRequest(true, &context.outer_session_field_raw);
    }

    if (context.inner_emsg == GBE_kDotaPracticeLobbyLaunch) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 7041 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaPracticeLobbyLaunchRequest(true, &context.outer_session_field_raw);
    }

    if (context.inner_emsg == GBE_kDotaPracticeLobbyJoinBroadcastChannel) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 7149 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaPracticeLobbyJoinBroadcastChannelRequest(
            context.inner_body_raw,
            context.request_job_id,
            context.has_request_job,
            true,
            &context.outer_session_field_raw
        );
    }

    if (context.inner_emsg == GBE_kDotaLobbyUpdateBroadcastChannelInfo) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 7367 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaLobbyUpdateBroadcastChannelInfoRequest(
            context.inner_body_raw,
            true,
            &context.outer_session_field_raw
        );
    }

    if (context.inner_emsg == GBE_kDotaPracticeLobbyCloseBroadcastChannel) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 8054 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaPracticeLobbyCloseBroadcastChannelRequest(
            context.inner_body_raw,
            true,
            &context.outer_session_field_raw
        );
    }

    if (context.inner_emsg == GBE_kDotaLeaveChatChannel) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 7272 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaLeaveChatChannelRequest(
            context.inner_body_raw,
            true,
            &context.outer_session_field_raw
        );
    }

    if (context.inner_emsg == GBE_kDotaDestroyLobbyRequest) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 8246 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaDestroyLobbyRequest(
            context.request_job_id,
            context.has_request_job,
            true,
            &context.outer_session_field_raw
        );
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Received wrapped 7047 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
        context.has_request_job ? 1 : 0,
        static_cast<unsigned long long>(context.request_job_id),
        context.outer_session_field_raw.size(),
        GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
    );

    return GBE_HandleDotaPracticeLobbySetTeamSlotRequest(
        context.inner_body_raw,
        context.request_job_id,
        context.has_request_job,
        true,
        &context.outer_session_field_raw
    );
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

        if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0) {
            std::string lobby_cache_message;
            if (GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayload(GBE_GetDotaLobbyOwnerName(), lobby_cache_message)) {
                GBE_GC_DebugLog(
                    "GC_DOTA_SERVER_HELLO",
                    "queueing direct lobby CacheSubscribed after ServerWelcome lobby_id=%llu state=%u game_state=%u team=%u slot=%u size=%zu",
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state,
                    GBE_local_lobby.owner_team,
                    GBE_local_lobby.owner_slot,
                    lobby_cache_message.size()
                );
                push_incoming_now(GBE_kDotaCacheSubscribed | GBE_kProtoMask, lobby_cache_message);
            } else {
                GBE_GC_DebugLog(
                    "GC_DOTA_SERVER_HELLO",
                    "failed building direct lobby CacheSubscribed after ServerWelcome lobby_id=%llu state=%u game_state=%u team=%u slot=%u",
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state,
                    GBE_local_lobby.owner_team,
                    GBE_local_lobby.owner_slot
                );
            }

            if (is_server) {
                GBE_Dota7034RequestShape synthetic_connected_players_request{};
                synthetic_connected_players_request.has_send_reason = true;
                synthetic_connected_players_request.send_reason = 4u;

                std::string connected_players_message;
                if (GBE_BuildDota7034ConnectedPlayersResponsePayload(
                        GBE_GetDotaLobbyOwnerSteamId(),
                        GBE_local_lobby.state,
                        GBE_local_lobby.game_state,
                        GBE_local_lobby.owner_team,
                        GBE_local_lobby.owner_slot,
                        synthetic_connected_players_request,
                        false,
                        0ull,
                        connected_players_message)) {
                    GBE_GC_DebugLog(
                        "GC_DOTA_SERVER_HELLO",
                        "queueing synthetic direct 7034 after ServerWelcome lobby_id=%llu state=%u game_state=%u team=%u slot=%u size=%zu",
                        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                        GBE_local_lobby.state,
                        GBE_local_lobby.game_state,
                        GBE_local_lobby.owner_team,
                        GBE_local_lobby.owner_slot,
                        connected_players_message.size()
                    );
                    push_incoming_now(7034u | GBE_kProtoMask, connected_players_message);
                } else {
                    GBE_GC_DebugLog(
                        "GC_DOTA_SERVER_HELLO",
                        "failed building synthetic direct 7034 after ServerWelcome lobby_id=%llu state=%u game_state=%u team=%u slot=%u",
                        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                        GBE_local_lobby.state,
                        GBE_local_lobby.game_state,
                        GBE_local_lobby.owner_team,
                        GBE_local_lobby.owner_slot
                    );
                }
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
        : GBE_BuildDotaClientWelcome(steam_id, app_id, account_id, hello_context, welcome_message);

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
        *pcubMsgSize = 0;
        return false;
    }

    GC_Message &message = incoming_messages.front();
    *pcubMsgSize = static_cast<uint32>(message.msg_body.size());

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
    GBE_GC_DebugLog("GC_RETRIEVE", "queued_emsg=%u queue_size=%zu cubDest=%u", queued_emsg, incoming_messages.size(), cubDest);

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
    incoming_messages.pop();

    GBE_GC_DebugLog("GC_RETRIEVE", "returned_emsg=%u size=%u", GBE_GC_MaskedEMsg(*punMsgType), outsize);

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
                break;
                }
            }
        }
    }
}

void Steam_Game_Coordinator::RunCallbacks()
{
    if (delay_init && welcome_received && check_timedout(welcome_time, 0.2)) {
        delay_init = false;
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
