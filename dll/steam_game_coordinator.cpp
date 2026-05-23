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
static constexpr uint32 GBE_kProtoMask = 0x80000000u;
static constexpr uint32 GBE_kEMsgClientToGC = 5452u;
static constexpr uint32 GBE_kEMsgClientFromGC = 5453u;
static constexpr uint32 GBE_kEMsgGCClientHello = 4006u;
static constexpr uint32 GBE_kEMsgGCServerHello = 4007u;
static constexpr uint32 GBE_kEMsgGCClientWelcome = 4004u;
static constexpr uint32 GBE_kGCInvitationCreated = 4502u;
static constexpr uint32 GBE_kGCInviteToLobby = 4512u;
static constexpr uint32 GBE_kGCLobbyInviteResponse = 4513u;
static constexpr uint32 GBE_kDotaAppId = 570u;
static constexpr uint32 GBE_kDotaCacheSubscribed = 24u;
static constexpr uint32 GBE_kDotaCacheUnsubscribed = 25u;
static constexpr uint32 GBE_kDotaPracticeLobbyDetailsUpdate = 26u;
static constexpr uint32 GBE_kDotaCacheSubscriptionCheck = 27u;
static constexpr uint32 GBE_kDotaCacheSubscriptionRefresh = 28u;
static constexpr uint32 GBE_kDotaCacheSubscribedUpToDate = 29u;
static constexpr uint32 GBE_kDotaGameMatchSignOut = 7004u;
static constexpr uint32 GBE_kDotaGameMatchSignOutResponse = 7005u;
static constexpr uint32 GBE_kDotaJoinChatChannel = 7009u;
static constexpr uint32 GBE_kDotaJoinChatChannelResponse = 7010u;
static constexpr uint32 GBE_kDotaOtherJoinedChannel = 7013u;
static constexpr uint32 GBE_kDotaOtherLeftChannel = 7014u;
static constexpr uint32 GBE_kDotaAbandonCurrentGame = 7035u;
static constexpr uint32 GBE_kDotaSubmitPlayerReportV2 = 7082u;
static constexpr uint32 GBE_kDotaSubmitPlayerReportResponseV2 = 7083u;
static constexpr uint32 GBE_kDotaGameMatchSignOutPermissionRequest = 7381u;
static constexpr uint32 GBE_kDotaGameMatchSignOutPermissionResponse = 7382u;
static constexpr uint32 GBE_kDotaLobbyAdditionalAccountData = 8869u;
static constexpr uint32 GBE_kDotaPracticeLobbyCreate = 7038u;
static constexpr uint32 GBE_kDotaPracticeLobbyLeave = 7040u;
static constexpr uint32 GBE_kDotaPracticeLobbyLaunch = 7041u;
static constexpr uint32 GBE_kDotaPracticeLobbyJoin = 7044u;
static constexpr uint32 GBE_kDotaPracticeLobbySetDetails = 7046u;
static constexpr uint32 GBE_kDotaPracticeLobbySetTeamSlot = 7047u;
static constexpr uint32 GBE_kDotaPracticeLobbyResponse = 7055u;
static constexpr uint32 GBE_kDotaPracticeLobbyKick = 7081u;
static constexpr uint32 GBE_kDotaPopup = 7102u;
static constexpr uint32 GBE_kDotaFriendPracticeLobbyListRequest = 7111u;
static constexpr uint32 GBE_kDotaFriendPracticeLobbyListResponse = 7112u;
static constexpr uint32 GBE_kDotaPracticeLobbyJoinResponse = 7113u;
static constexpr uint32 GBE_kDotaLeaveChatChannel = 7272u;
static constexpr uint32 GBE_kDotaChatMessage = 7273u;
static constexpr uint32 GBE_kDotaPracticeLobbyJoinBroadcastChannel = 7149u;
static constexpr uint32 GBE_kDotaLobbyUpdateBroadcastChannelInfo = 7367u;
static constexpr uint32 GBE_kDotaDestroyLobbyRequest = 8246u;
static constexpr uint32 GBE_kDotaDestroyLobbyResponse = 8247u;
static constexpr uint32 GBE_kDotaPracticeLobbyCloseBroadcastChannel = 8054u;
static constexpr uint32 GBE_kDotaLobbyList = 8011u;
static constexpr uint32 GBE_kDotaLobbyListResponse = 8012u;
static constexpr uint32 GBE_kDotaSOUpdateMultiple = 6146u;
static constexpr size_t GBE_kDotaWelcomeInnerBodyOffset = 48u;
static constexpr const char *GBE_kGcDebugLogPath = "C:\\Users\\Public\\gbe_gc_debug.log";
static constexpr uint64 GBE_kDotaLobbyDetailsTimestamp = 0x0069E7F5C567E78Bull;
static constexpr uint32 GBE_kDotaLobbyField128Value = 1776809986u;
static constexpr uint32 GBE_kDotaTeamGoodGuys = 0u;
static constexpr uint32 GBE_kDotaTeamBadGuys = 1u;
static constexpr uint32 GBE_kDotaTeamPlayerPool = 4u;
static constexpr const char *GBE_kDotaGenericLobbyMarkerKey = "gbe_dota_practice_lobby";
static constexpr const char *GBE_kDotaGenericLobbyMarkerValue = "1";
static constexpr const char *GBE_kDotaGenericLobbyDotaLobbyIdKey = "gbe_dota_lobby_id";
static constexpr const char *GBE_kDotaGenericLobbyRoomNameKey = "gbe_dota_room_name";
static constexpr const char *GBE_kDotaGenericLobbyGameModeKey = "gbe_dota_game_mode";
static constexpr const char *GBE_kDotaGenericLobbyServerRegionKey = "gbe_dota_server_region";
static constexpr const char *GBE_kDotaGenericLobbyLanPingKey = "gbe_dota_lan_ping";
static constexpr const char *GBE_kDotaGenericLobbyPassKeyKey = "gbe_dota_pass_key";
static constexpr const char *GBE_kDotaGenericLobbyOwnerSteamIdKey = "gbe_dota_owner_steam_id";
static constexpr const char *GBE_kDotaGenericLobbyOwnerAccountIdKey = "gbe_dota_owner_account_id";
static constexpr const char *GBE_kDotaGenericLobbyOwnerNameKey = "gbe_dota_owner_name";
static constexpr const char *GBE_kDotaGenericLobbyStateKey = "gbe_dota_state";
static constexpr const char *GBE_kDotaGenericLobbyGameStateKey = "gbe_dota_game_state";
static constexpr const char *GBE_kDotaGenericLobbyMatchIdKey = "gbe_dota_match_id";
static constexpr const char *GBE_kDotaGenericLobbyServerIdKey = "gbe_dota_server_id";
static constexpr const char *GBE_kDotaGenericLobbyConnectKey = "gbe_dota_connect";
static constexpr const char *GBE_kDotaGenericLobbyGameStartTimeKey = "gbe_dota_game_start_time";
static constexpr const char *GBE_kDotaGenericLobbyMemberTeamKey = "gbe_dota_member_team";
static constexpr const char *GBE_kDotaGenericLobbyMemberSlotKey = "gbe_dota_member_slot";
static constexpr const char *GBE_kDotaGenericLobbyMemberHeroKey = "gbe_dota_member_hero";
static constexpr const char *GBE_kDotaGenericLobbyMemberConnectedKey = "gbe_dota_member_connected";
static constexpr const char *GBE_kDotaGenericLobbyMemberNameKey = "gbe_dota_member_name";

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
    uint32 owner_hero_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
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
    std::string &object_2014);

static void GBE_BuildDotaServerStaticLobbyObject2016(
    uint32 account_id,
    uint64 steam_id,
    uint32 game_mode,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::string &object_2016);

static const char GBE_kDotaOfficialLobbyStartupAccountDataTemplate[] =
    "\010\365\355\206A\022\274\001\n\005\010\002\020\300\014\n\005\010\005\020\310\001"
    "\n\004\010\012\020d\n\004\010\013\020d\n\005\010\014\020\336\002"
    "\n\004\010\"\020d\n\004\010#\0202\n\005\010%\020\356\005"
    "\n\005\010(\020\300\014\n\004\010*\0202\n\005\010,\020\333\003"
    "\n\005\010/\020\254\002\n\005\0105\020\336\002\n\004\010E\020d"
    "\n\004\010K\020d\n\005\010Q\020\330\004\n\005\010S\020\333\003"
    "\n\005\010T\020\275\025\n\005\010U\020\226\001\n\004\010h\0202"
    "\n\005\010\303\002\020\001\n\005\010\220\003\020\001"
    "\n\005\010\221\003\020\001\n\005\010\222\003\020\001"
    "\n\005\010\232\003\020\006\n\005\010\315\003\020\003"
    "\n\005\010\316\003\020\010\n\005\010\317\003\020\026"
    "\032\006\010\206\001\020\206\001\032\006\010\321\017\020\322\017"
    "\032\006\010\211\047\020\212\047\032\006\010\221N\020\222N"
    "\032\006\010\371U\020\372U\032\006\010\341]\020\342]"
    "\032\010\010\321\211\002\020\322\211\002"
    "\032\010\010\271\221\002\020\272\221\002"
    "\032\010\010\211\241\002\020\212\241\002"
    "\032\010\010\301\270\002\020\302\270\002"
    "\032\010\010\221\310\002\020\222\310\002"
    "\032\010\010\341\327\002\020\342\327\002"
    "\032\010\010\231\357\002\020\232\357\002"
    "\032\010\010\211\236\003\020\212\236\003"
    "\032\010\010\211\233\004\020\212\233\004"
    "\032\010\010\371\311\004\020\372\311\004"
    "\032\010\010\351\370\004\020\352\370\004"
    "\032\010\010\271\210\005\020\272\210\005"
    "\032\010\010\241\220\005\020\242\220\005"
    "\032\010\010\211\230\005\020\212\230\005"
    "\032\010\010\301\254\006\020\302\254\006";

struct GBE_SharedDotaLobbyState {
    bool valid{};
    bool active{};
    uint64 lobby_id{};
    uint64 generic_lobby_id{};
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
    uint32 owner_hero_id{};
    bool owner_connected{};
    std::vector<GBE_DotaLobbyMemberState> members;
    uint32 launch_phase{};
    bool launch_4511_seen{};
    bool has_broadcast_channel{};
    uint32 broadcast_channel_id{};
    std::string broadcast_country_code;
    std::string broadcast_description;
    std::string broadcast_language_code;
    std::string pass_key;
    bool has_cache_version{};
    uint64 cache_version{};
    bool has_cache_service_id{};
    uint32 cache_service_id{};
    std::vector<uint32> cache_service_list;
    bool has_cache_sync_version{};
    uint64 cache_sync_version{};
};

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

static GBE_SharedDotaLobbyState GBE_shared_dota_lobby_state;
static bool GBE_pending_dota_normal_signout_finalize_after_25 = false;
static uint64 GBE_pending_dota_normal_signout_finalize_lobby_id = 0;

enum : uint32 {
    GBE_kDotaLaunchPhaseNone = 0u,
    GBE_kDotaLaunchPhaseRequested = 1u,
    GBE_kDotaLaunchPhaseSetupSynced = 2u,
    GBE_kDotaLaunchPhaseRunQueued = 3u,
};

static const char *GBE_DescribeDotaLaunchPhase(uint32 phase)
{
    switch (phase) {
        case GBE_kDotaLaunchPhaseRequested:
            return "requested";
        case GBE_kDotaLaunchPhaseSetupSynced:
            return "serversetup_synced";
        case GBE_kDotaLaunchPhaseRunQueued:
            return "run_queued";
        default:
            return "none";
    }
}

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
static const std::array<uint8, 8> GBE_kOldDotaPersonaSteamIdFixed64 = { 0x91, 0x1D, 0xDF, 0x05, 0x01, 0x00, 0x10, 0x01 };
static const std::array<uint8, 4> GBE_kOldDotaAccountIdFixed32 = { 0xF5, 0xB6, 0x21, 0x08 };
static const std::array<uint8, 8> GBE_kOldDotaPracticeLobbyLobbyIdVarint = { 0x83, 0xCF, 0xA2, 0xB4, 0xA2, 0xFF, 0xF9, 0x34 };
static const std::array<uint8, 5> GBE_kOldDotaPracticeLobbyMatchIdVarint = { 0xDF, 0xF8, 0xBB, 0xDB, 0x20 };
static const std::array<uint8, 8> GBE_kOldDotaPracticeLobbyServerIdFixed64 = { 0x01, 0x7C, 0x58, 0xCA, 0x8F, 0xC1, 0x40, 0x01 };
static const std::array<uint8, 5> GBE_kOldDotaPracticeLobbyGameStartTimeVarint = { 0xAE, 0xBB, 0xA3, 0xCF, 0x06 };
static constexpr const char *GBE_kOldDotaPracticeLobbyConnect = "117.157.79.194:27015 10.110.4.21:27015";
static constexpr const char *GBE_kOldDotaPracticeLobbyLobbyIdText = "29809934128949123";
static constexpr const char *GBE_kOldDotaPracticeLobbyLobbyIdTextAlt = "29822498642855090";
static constexpr const char *GBE_kLocalDotaPracticeLobbyLoopbackEndpoint = "127.0.0.1:27015";

static std::string GBE_NormalizeDotaPracticeLobbyConnect(const std::string &connect)
{
    const size_t first = connect.find_first_not_of(" \t\r\n");
    if (first == std::string::npos)
        return std::string();

    const size_t last = connect.find_first_of(" \t\r\n", first);
    return connect.substr(first, last == std::string::npos ? std::string::npos : last - first);
}

static std::string GBE_BuildDotaPracticeLobbyConnectPair(const std::string &endpoint)
{
    return GBE_NormalizeDotaPracticeLobbyConnect(endpoint);
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

static uint32 GBE_ParseDotaPracticeLobbyConnectIPv4(const std::string &connect)
{
    const std::string endpoint = GBE_NormalizeDotaPracticeLobbyConnect(connect);
    unsigned int octet1 = 0;
    unsigned int octet2 = 0;
    unsigned int octet3 = 0;
    unsigned int octet4 = 0;
    unsigned int port = 0;
    if (std::sscanf(endpoint.c_str(), "%u.%u.%u.%u:%u", &octet1, &octet2, &octet3, &octet4, &port) != 5)
        return 0;

    if (octet1 > 255u || octet2 > 255u || octet3 > 255u || octet4 > 255u || port == 0u || port > 65535u)
        return 0;

    return (octet1 << 24) | (octet2 << 16) | (octet3 << 8) | octet4;
}

static std::string GBE_GetDotaPracticeLobbyFirstConnectEndpoint(const std::string &connect)
{
    return GBE_NormalizeDotaPracticeLobbyConnect(connect);
}

static uint64 GBE_BuildDotaPracticeLobbyIpServerId(uint32 ip)
{
    if (ip == 0u)
        return 0ull;

    return static_cast<uint64>(ip);
}

static constexpr uint32 GBE_kSteamGamesPlayedWithDataBlob = 5410u;
static constexpr uint32 GBE_kSteamAuthList = 5432u;
static constexpr uint32 GBE_kSteamPersonaState = 766u;
static constexpr uint32 GBE_kSteamTicketAuthComplete = 5429u;
static constexpr uint32 GBE_kDotaConductScore = 12000u;
static constexpr uint32 GBE_kDotaBehaviorLevel = 4u;

static constexpr const char *GBE_kDotaAbandonPersonaStatePrivateLobbyPostgameHex =
    "fe0200800f00000009911ddf050100100110c9dbfdd20408dfe60112ee0309911ddf0501001001100118ba04300138017a0a636c6f7665726c6f7665c9010000000000000000fa01140000000000000000000000000000000000000000e802a6c7dacf06f00281c8dacf06f802a6c7dacf06ba0300c1033a02000000000000e20300ba04200a06737461747573121623444f54415f52505f505249564154455f4c4f424259ba04270a0d737465616d5f646973706c6179121623444f54415f52505f505249564154455f4c4f424259ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131ba041e0a057061727479121570617274795f73746174653a20494e5f4d41544348ba0492010a056c6f6262791288016c6f6262795f69643a203239383232343938363432383535303930206c6f6262795f73746174653a2052554e2067616d655f6d6f64653a20444f54415f47414d454d4f44455f4150206d656d6265725f636f756e743a2031206d61785f6d656d6265725f636f756e743a203130206e616d653a20226565656522206c6f6262795f747970653a2031c1040000000000000000c9040000000000000000f80400800500880500980501";
static constexpr const char *GBE_kDotaAbandonPersonaStatePrivateLobbyNoLobbyHex =
    "fe0200800f00000009911ddf050100100110c9dbfdd20408dfe60112d80209911ddf0501001001100118ba04300138017a0a636c6f7665726c6f7665c9010000000000000000fa01140000000000000000000000000000000000000000e802a6c7dacf06f00281c8dacf06f802a6c7dacf06ba0300c1033a02000000000000e20300ba04200a06737461747573121623444f54415f52505f505249564154455f4c4f424259ba04270a0d737465616d5f646973706c6179121623444f54415f52505f505249564154455f4c4f424259ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131ba041e0a057061727479121570617274795f73746174653a20494e5f4d41544348c1040000000000000000c9040000000000000000f80400800500880500980501";
static constexpr const char *GBE_kDotaAbandonPersonaStateInitHex =
    "fe0200800f00000009911ddf050100100110c9dbfdd20408dfe60112a50209911ddf0501001001100118ba04300138017a0a636c6f7665726c6f7665c9010000000000000000fa01140000000000000000000000000000000000000000e802a6c7dacf06f00281c8dacf06f802a6c7dacf06ba0300c1033a02000000000000e20300ba04170a06737461747573120d23444f54415f52505f494e4954ba041e0a0d737465616d5f646973706c6179120d23444f54415f52505f494e4954ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131c1040000000000000000c9040000000000000000f80400800500880500980501";
static constexpr const char *GBE_kDotaLaunchPersonaStateInitServerSetupHex =
    "fe0200800f00000009911ddf050100100110c9dbfdd20408dfe60112c30309911ddf0501001001100118ba04300138017a0a636c6f7665726c6f7665c9010000000000000000fa01140000000000000000000000000000000000000000e802a6c7dacf06f00281c8dacf06f802a6c7dacf06ba0300c1033a02000000000000e20300ba04170a06737461747573120d23444f54415f52505f494e4954ba041e0a0d737465616d5f646973706c6179120d23444f54415f52505f494e4954ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131ba049a010a056c6f6262791290016c6f6262795f69643a203239383232343938363432383535303930206c6f6262795f73746174653a2053455256455253455455502067616d655f6d6f64653a20444f54415f47414d454d4f44455f4150206d656d6265725f636f756e743a2031206d61785f6d656d6265725f636f756e743a203130206e616d653a20226565656522206c6f6262795f747970653a2031c1040000000000000000c9040000000000000000f80400800500880500980501";
static constexpr const char *GBE_kDotaLaunchPersonaStateFindingMatchServerSetupHex =
    "fe0200800f00000009911ddf050100100110c9dbfdd204080812f20309911ddf0501001001100118ba04300138017a0a636c6f7665726c6f7665c9010e14ce4bfcc14001fa01140000000000000000000000000000000000000000e802a6c7dacf06f00281c8dacf06f80200ba0300c1033a02000000000000e20300ba04200a06737461747573121623444f54415f52505f46494e44494e475f4d41544348ba04270a0d737465616d5f646973706c6179121623444f54415f52505f46494e44494e475f4d41544348ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131ba041e0a057061727479121570617274795f73746174653a20494e5f4d41544348ba049a010a056c6f6262791290016c6f6262795f69643a203239383232343938363432383535303930206c6f6262795f73746174653a2053455256455253455455502067616d655f6d6f64653a20444f54415f47414d454d4f44455f4150206d656d6265725f636f756e743a2031206d61785f6d656d6265725f636f756e743a203130206e616d653a20226565656522206c6f6262795f747970653a2031c1040000000000000000c9040000000000000000f80400800500880500980501";
static constexpr const char *GBE_kDotaLaunchPersonaStateFindingMatchRunHex =
    "fe0200800f00000009911ddf050100100110c9dbfdd20408dfe60112ee0309911ddf0501001001100118ba04300138017a0a636c6f7665726c6f7665c9010000000000000000fa01140000000000000000000000000000000000000000e802a6c7dacf06f00281c8dacf06f802a6c7dacf06ba0300c1033a02000000000000e20300ba04200a06737461747573121623444f54415f52505f46494e44494e475f4d41544348ba04270a0d737465616d5f646973706c6179121623444f54415f52505f46494e44494e475f4d41544348ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131ba041e0a057061727479121570617274795f73746174653a20494e5f4d41544348ba0492010a056c6f6262791288016c6f6262795f69643a203239383232343938363432383535303930206c6f6262795f73746174653a2052554e2067616d655f6d6f64653a20444f54415f47414d454d4f44455f4150206d656d6265725f636f756e743a2031206d61785f6d656d6265725f636f756e743a203130206e616d653a20226565656522206c6f6262795f747970653a2031c1040000000000000000c9040000000000000000f80400800500880500980501";
static constexpr const char *GBE_kDotaLaunchPersonaStatePrivateLobbyRunHex =
    "fe0200800f00000009911ddf050100100110c9dbfdd20408dfe60112ee0309911ddf0501001001100118ba04300138017a0a636c6f7665726c6f7665c9010000000000000000fa01140000000000000000000000000000000000000000e802a6c7dacf06f00281c8dacf06f802a6c7dacf06ba0300c1033a02000000000000e20300ba04200a06737461747573121623444f54415f52505f505249564154455f4c4f424259ba04270a0d737465616d5f646973706c6179121623444f54415f52505f505249564154455f4c4f424259ba040f0a0a6e756d5f706172616d73120130ba04120a0d4576656e744c6576656c5f3236120130ba04120a0d4576656e744c6576656c5f3339120130ba04120a0d4576656e744c6576656c5f3536120131ba04120a0d4576656e744c6576656c5f3535120131ba041e0a057061727479121570617274795f73746174653a20494e5f4d41544348ba0492010a056c6f6262791288016c6f6262795f69643a203239383232343938363432383535303930206c6f6262795f73746174653a2052554e2067616d655f6d6f64653a20444f54415f47414d454d4f44455f4150206d656d6265725f636f756e743a2031206d61785f6d656d6265725f636f756e743a203130206e616d653a20226565656522206c6f6262795f747970653a2031c1040000000000000000c9040000000000000000f80400800500880500980501";
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
static constexpr const char *GBE_kDotaOfficial032PracticeLobby26Hex =
    "4d15008014000000090eac2b7cdec1400110dbc3fdeafdffffffff0108ba04109a808080081a80041a00008000000000120508dd0f120012e90108d40f12e30108d6f9ac9f95a6fc34180120022a273138322e34322e3232342e31333a3237303135203139322e3136382e342e3136383a3237303135310eac2b7cdec1400159f5b621080100100160016800700082010531313131318a010240008a01024000a80100b0010ae00100f001bbca9fe020f80100a00203d00200d80200e00200f00200f80200800300980300a80300c80301f2030708f54412020800880400d80400900500b805f7e6cbcf06c00500e80503f00500f80500880600b80600c00637f00600880700c2070d09f5b621080100100118003801c80700f807008008d5e6cbcf06121208de0f120d0a090a075376656e6d61781000120708df0f12020a0012cf0108e00f12c9010a3509f5b62108010010014800580060e1ac8b84d0854068008501000000e085014128d9f585015706000098010098010098010098010015000000001a300813122c08f5ed864110001800200038006000d00100d80100e00100fa0106080f100a180afa0108081c10e80718e8071a1c081a121808f5ed864110001800200138006000d00100d80100e001001a1c0827121808f5ed864110001800200138006000d00100d80100e001001a1d0838121908f5ed864110e8071800200138016000d00100d80100e00100190439f15331f16900320b080310d6f9ac9f95a6fc34";
static constexpr const char *GBE_kDotaOfficial8745TemplateHex =
    "4d15008014000000090eac2b7cdec1400110dbc3fdeafdffffffff0108ba0410a9c48080081a112922008009000000592100000000000000";

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

static constexpr const char *GBE_kDotaPracticeLobbyLaunchCacheSubscribedPreludeHex =
    "4d15008014000000090eac2b7cdec1400110dbc3fdeafdffffffff0108ba041098808080081a2a180000800000000019"
    "fafc724931f16900220c080110f5ed86c19080808801300139b692f15331f16900";

static constexpr const char *GBE_kDotaPracticeLobbyLaunchCacheSubscribedLargePreludeHex =
    "4d15008014000000090eac2b7cdec1400110dbc3fdeafdffffffff0108ba041098808080081ae9311800008000000000"
    "128d10080112190895a3edce0610f5ed8641180120982b780092010408011005121908ca8da2801010f5ed8641180520"
    "b8307800920104084a1006121908eaa5c18d1010f5ed8641180620f72d780092010408321001121d08b5ccc18d1010f5"
    "ed8641180420fb26400148067800920104080f1000121d0898e6c18d1010f5ed8641180a20b92a400148067800920104"
    "08231002121e0899e6c18d1010f5ed8641180d20d44e40014806780092010508e80710041236088fc3c1c01010f5ed86"
    "41180e208b32621b08011a170a0b1800220028bb1730003a0010011800200028f2e606780092010408661003124208e8"
    "e8d1d71010f5ed8641180f20a82e62090890031a0400000000621c08011a180a0b1800220028bb1730003a0010011884"
    "abc01920002800780092010408581000121d08d7f391821210f5ed8641181020ba2a4001480678009201040823100412"
    "19088eb2f9831210f5ed86411811208430780092010408301001121908e09d8f841210f5ed8641181620883078009201"
    "04080910021226088efd86931b10f5ed8641181320f91f4001620908d5011a0401000000780092010408351003121d08"
    "e48f88931b10f5ed8641181220cb2640014806780092010408101004122708d88f9ac32210f5ed864118904e20ea2f40"
    "01620908d5011a04010000007800920104084910011235089a8cd5c32210f5ed8641188f4e20fa264001480662150801"
    "1a110a0b1800220028ba1730003a00100118007800920104080f1001122708b6fdfcd32210f5ed8641188d4e20a72b40"
    "01620908d5011a04010000007800920104084b100312450899a9a9e72610f5ed8641188c4e20992d4001620908d5011a"
    "0401000000621c08011a180a0b1800220028bb1730003a00100118d88cb628200028007800920104084a1004122708cf"
    "8fedea2d10f5ed8641188b4e20b1254001620908d5011a04010000007800920104083a1000122708ed879f8b2e10f5ed"
    "8641188a4e20a12a4001620908d5011a04010000007800920104086310041227089dd182802f10f5ed864118884e20ce"
    "2b4001620908d5011a0401000000780092010408081000122708bda98fa73710f5ed864118874e20da284001620908d5"
    "011a04010000007800920104084c1000122708eda6ffc44810f5ed864118fe4d20da2c4001620908d5011a0401000000"
    "780092010408371003122708ddb198d85e10f5ed864118fd4d209b244001620908d5011a04010000007800920104083f"
    "1003122708a5cc8bb65f10f5ed864118fc4d20e9254001620908d5011a0401000000780092010408341003122d08a589"
    "99b85f10f5ed864118f84d209d604824621608011a120a0b1800220028c41730003a0010cd0118007800121e0895869a"
    "b85f10f5ed864118f74d20f86140014822780092010408541007124d089d869ab85f10f5ed864118f64d20e66a400148"
    "22622d08011a290a22180022176e70635f646f74615f6865726f5f6f6772655f6d61676928d21730003a00108a02185c"
    "78009201040854100a121e0895dd9bb85f10f5ed864118ee4d20bc9401481f780092010508e8071002121408cde09bb8"
    "5f10f5ed864118ea4d20fc89017800122808f5e99bb85f10f5ed864118f14d20a3b9014808620908d5011a0401000000"
    "7800920104082e1001122808bd86f6a16010f5ed864118e44d20c49401481f620908d5011a0401000000780092010408"
    "04100712280885b7e5a36010f5ed864118ec4d20cb98014822620908d5011a0401000000780092010408511000122808"
    "8db7e5a36010f5ed864118e34d20ca98014822620908d5011a040100000078009201040851100312280895b7e5a36010"
    "f5ed864118e24d20c998014822620908d5011a04010000007800920104085110011228089db7e5a36010f5ed864118e1"
    "4d20c898014822620908d5011a0401000000780092010408511004122908a5b7e5a36010f5ed864118e04d20c7980148"
    "22620908d5011a0401000000780092010508e8071006122808adb7e5a36010f5ed864118df4d20c698014822620908d5"
    "011a04010000007800920104085110021228089d82e6a36010f5ed864118eb4d20b4b1014822620908d5011a04010000"
    "007800920104083b1000122808a582e6a36010f5ed864118de4d20b5b1014822620908d5011a04010000007800920104"
    "083b1001122808ad82e6a36010f5ed864118dd4d20b6b1014822620908d5011a04010000007800920104083b10041228"
    "08b582e6a36010f5ed864118dc4d20f1b5014822620908d5011a04010000007800920104083b1002122808bd82e6a360"
    "10f5ed864118db4d20f2b5014822620908d5011a04010000007800920104083b1003124f088580eba36010f5ed864118"
    "00208705620908ac031a0454000000623008011a2c0a22180322176e70635f646f74615f6865726f5f6f6772655f6d61"
    "676928db1730003a0010992b18b6fdbe027800121f08c5a7f1a36010f5ed864118e74d20f88e01400148227800920104"
    "08021000121f08cda7f1a36010f5ed864118da4d20f78e0140014822780092010408021001121f08d5a7f1a36010f5ed"
    "864118d94d20f68e0140014822780092010408021002121f08dda7f1a36010f5ed864118d84d20f58e01400148227800"
    "92010408021003121f08e5a7f1a36010f5ed864118d74d20f48e0140014822780092010408021005122708add8eea361"
    "10f5ed864118fa4d20fd1f4001620908d5011a04010000007800920104081d1004121c088dd5fd9a6b10f5ed864118fb"
    "4d209a20481f780092010408171002121f088591cbe07910f5ed864118f54d20e1950240034807780092010408541002"
    "12aa2108da0f122f08f5ed8641101318c90138c2fcc3bf0640004801500158006000700078038001aff1a9a505880102"
    "900190c9a4b30f122f08f5ed8641101318ca0138c2fcc3bf0640004801500158006008700078038001eb8faba80d8801"
    "029001b68dd0d30c122f08f5ed8641101318cb0138c2fcc3bf0640004801500158006000700078038001f1cda4e20388"
    "0102900192ff8a9f05122f08f5ed8641101318f5033888df90ac0640004818500158006001700078038001a9ebf9d00e"
    "8801059001e092cea903122f08f5ed8641101318f6033888df90ac06400048185001580060007000780380018f8df5f2"
    "058801059001e78ea28b0b122f08f5ed8641101318f7033888df90ac064000481850015800600070007803800182d895"
    "c3048801059001b8cad0ee01122f08f5ed8641101318e90738c489c0ab064000481650015800600070007803800191ea"
    "b8bb0b88010a900196baaab20d122f08f5ed8641101318ea0738c489c0ab06400048165001580060007000780380018b"
    "ef8fe80288010a9001c2ee8b870b122f08f5ed8641101318eb0738c489c0ab0640004816500158006001700078038001"
    "b99aa1a50e88010a9001abd8e2e70e122f08f5ed8641101318cd08389bfdb1ab06400048155001580060007000780380"
    "018893babf0f88010b90018aa187df0c122e08f5ed8641101318ce08389bfdb1ab064000481550015800600070007803"
    "800195b0afa90d88010b9001c4c5a723122f08f5ed8641101318cf08389bfdb1ab064000481550015800600070007803"
    "80018bef8fe80288010b9001c2ee8b870b122f08f5ed8641101318b109388fb89bab0640004814500158006000700078"
    "038001a2bbfca20c88010c9001d2de8df60b122f08f5ed8641101318b209388fb89bab06400048145001580060017001"
    "78038001c596e98c0988010c900199b3cdb702122f08f5ed8641101318b309388fb89bab064000481450015800600070"
    "0078038001b093fa8c0f88010c9001cdd4adb008122f08f5ed8641101318a50d38c2c0969d064000480b500158006000"
    "700078038001a2bbfca20c8801119001d2de8df60b122f08f5ed8641101318a60d38c2c0969d064000480b5001580060"
    "00700078038001d9bdbdb302880111900196def6a20e122f08f5ed8641101318a70d38c2c0969d064000480b50015800"
    "6000700078038001e3fecffc0188011190019eebb78406122f08f5ed8641101318b51038e697e39b0640004806500158"
    "006000700078038001e3f5cfa2048801159001fdb9828803122f08f5ed8641101318b61038e697e39b06400048065001"
    "58006000700078038001f4f4879e0b8801159001c9ded1ce04122f08f5ed8641101318b71038e697e39b064000480650"
    "0158006000700078038001f4efb1ac0e8801159001f0b090a70e122f08f5ed8641101318c91a38f5ff97ab0640004813"
    "500158006001700078038001b1fbcbf00d8801229001bb9193ed0c122f08f5ed8641101318ca1a38f5ff97ab06400048"
    "1350015800600070007803800190e891f2078801229001bef3ada208122f08f5ed8641101318cb1a38f5ff97ab064000"
    "4813500158006000700078038001d9bdbdb302880122900196def6a20e122f08f5ed8641101318ad1b389bd5a99d0640"
    "004805500158006000700078038001f8a99b89018801239001969cb1850f122f08f5ed8641101318ae1b389bd5a99d06"
    "40004805500158006000700078038001a097c3c70b8801239001cb95e59f0a122f08f5ed8641101318af1b389bd5a99d"
    "0640004805500158006000700078038001968fe7b6088801239001d4a29c8607122f08f5ed8641101318f51c38e0f2f7"
    "aa0640004810500158006000700078038001b7c7c3fd0a880125900185d5adec0e122f08f5ed8641101318f61c38e0f2"
    "f7aa0640014810500158006002700378038001c1bfff910b88012590019aad93b30f122f08f5ed8641101318f71c38e0"
    "f2f7aa06400048105001580060007000780380018f8df5f2058801259001e78ea28b0b122f08f5ed8641101318a11f38"
    "b8e7c2bf0640004803500158006001700078038001c5fcbee1058801289001c5fcbee105122f08f5ed8641101318a21f"
    "38b8e7c2bf0640004803500158006006700078038001e5b6c399068801289001e5b4d5c308122f08f5ed8641101318a3"
    "1f38b8e7c2bf0640004803500158006000700078038001f4ade1b8048801289001b8a3ca9908122f08f5ed8641101318"
    "e920388a98e39b0640004807500158006000700078038001c8d1cdb50e88012a9001c3dfd5950b122f08f5ed86411013"
    "18ea20388a98e39b0640004807500158006000700078038001ada283c00588012a9001cab5ff830f122f08f5ed864110"
    "1318eb20388a98e39b0640004807500158006000700078038001b093fa8c0f88012a9001cdd4adb008122f08f5ed8641"
    "101318b12238a1b2ccab0640004817500158006000700078038001c289f5bb0888012c9001a093ee9908122f08f5ed86"
    "41101318b22238a1b2ccab0640004817500158006000700078038001ac858cca0e88012c9001a086d09b08122f08f5ed"
    "8641101318b32238a1b2ccab0640004817500158006001700278038001d9bdbdb30288012c900196def6a20e122f08f5"
    "ed8641101318dd2438b9adc2bf064000481d500158006000700078038001e2a5c4dd0a88012f9001ead5da8505122f08"
    "f5ed8641101318de2438b9adc2bf064000481d500158006000700078038001968fe7b60888012f9001d4a29c8607122f"
    "08f5ed8641101318df2438b9adc2bf064000481d500158006001700078038001f1cda4e20388012f900192ff8a9f0512"
    "2f08f5ed8641101318b529389efbb19c064000480a500158006001700178038001ecbfb8d10e8801359001ecefefd20a"
    "122f08f5ed8641101318b629389efbb19c064000480a500158006000700078038001efa7bdd70c880135900181db9491"
    "03122f08f5ed8641101318b729389efbb19c064000480a500158006000700078038001bfd8f7cc07880135900184b6f2"
    "a10c122f08f5ed8641101318b93038aab699ac064000481a500158006000700078038001d9bdbdb30288013e900196de"
    "f6a20e122f08f5ed8641101318ba3038aab699ac064000481a500158006000700078038001dcf9c8890f88013e9001d2"
    "e792d00c122f08f5ed8641101318bb3038aab699ac064000481a500158006000700078038001e8a195d90d88013e9001"
    "d1ab9ee603122f08f5ed86411013189d313891fdab9d064000480c50015800600070007803800194d7b3e00488013f90"
    "0180e9e1d804122f08f5ed86411013189e313891fdab9d064000480c500158006000700078038001b291cea00e88013f"
    "9001cddbe7c809122f08f5ed86411013189f313891fdab9d064000480c5001580060007000780380018893babf0f8801"
    "3f90018aa187df0c122e08f5ed8641101318f53538b1b9b19c0640004808500158006000700078038001bfaedefc0b88"
    "01459001fee68b36122f08f5ed8641101318f63538b1b9b19c0640004808500158006000700078038001f1cda4e20388"
    "0145900192ff8a9f05122e08f5ed8641101318f73538b1b9b19c064000480850015800600170007803800198ecd4b40c"
    "880145900192d68f4e122f08f5ed8641101318cd3a38e2ff94ac0640004812500158006000700078038001d9bdbdb302"
    "88014b900196def6a20e122f08f5ed8641101318ce3a38e2ff94ac064000481250015800600070007803800182b1c3b5"
    "0588014b9001b98c83ca04122f08f5ed8641101318cf3a38e2ff94ac0640004812500158006000700078038001b0d2c2"
    "a40988014b9001e286fcc601122f08f5ed8641101318a53f3887b0f7aa064000480f500158006000700078038001e8a1"
    "95d90d8801519001d1ab9ee603122f08f5ed8641101318a63f3887b0f7aa064000480f50015800600070007803800196"
    "8fe7b6088801519001d4a29c8607122f08f5ed8641101318a73f3887b0f7aa064001480f500158006001700378038001"
    "96fa9f9d0c8801519001fdc88cf002122f08f5ed8641101318ed4038d2a7ddaa064000480e5001580060007000780380"
    "01f4ade1b8048801539001b8a3ca9908122f08f5ed8641101318ee4038d2a7ddaa064000480e50015800600170027803"
    "8001a8fad4e4038801539001b390b6c50e122f08f5ed8641101318ef4038d2a7ddaa064000480e500158006000700078"
    "038001f5ffe3a90988015390019ca18bae0a122f08f5ed8641101318d1413887cfc2bf06400048025001580060147000"
    "78038001bf90a7b30f8801549001c0f684ab0e122f08f5ed8641101318d2413887cfc2bf064001480250015800601270"
    "0378038001d9f8fea8038801549001e0f3ec8505122f08f5ed8641101318d3413887cfc2bf0640004802500158006000"
    "700078038001b4b8f9b30e8801549001c6f3e7c008122f08f5ed8641101318b54238e8caccab06400048095001580060"
    "007000780380019fd3af9b0d8801559001babd8aec03122e08f5ed8641101318b64238e8caccab064000480950015800"
    "600170007803800198bfd585048801559001fab2ce29122f08f5ed8641101318b74238e8caccab064000480950015800"
    "6000700078038001f1cda4e203880155900192ff8a9f05122f08f5ed8641101318e54b38a2ea93ac0640004819500158"
    "006000700078038001ddf7a8c3038801619001d4e099df03122f08f5ed8641101318e64b38a2ea93ac06400048195001"
    "5800600070007803800182d895c3048801619001b8cad0ee01122f08f5ed8641101318e74b38a2ea93ac064000481950"
    "01580060007000780380019b85b8a5088801619001fac3ccbc01122f08f5ed8641101318a15138878a97ab0640004811"
    "500158006000700078038001e8a195d90d8801689001d1ab9ee603122e08f5ed8641101318a25138878a97ab06400048"
    "1150015800600070007803800195b0afa90d8801689001c4c5a723122e08f5ed8641101318a35138878a97ab06400048"
    "1150015800600070007803800198ecd4b40c880168900192d68f4e122f08f5ed8641101318e952389cd29bac06400048"
    "1c500158006000700078038001a097c3c70b88016a9001cb95e59f0a122f08f5ed8641101318ea52389cd29bac064000"
    "481c5001580060007000780380018893babf0f88016a90018aa187df0c122f08f5ed8641101318eb52389cd29bac0640"
    "00481c500158006000700078038001c3b1d3c10e88016a9001c690838101122f08f5ed8641101318895938c194e19b06"
    "40004804500158006000700078038001e8a195d90d8801729001d1ab9ee603122f08f5ed86411013188a5938c194e19b"
    "0640004804500158006000700078038001d9bdbdb302880172900196def6a20e122f08f5ed86411013188b5938c194e1"
    "9b06400048045001580060007000780380019bec9ff8048801729001c1deb08503123008f5ed8641101318816438eedf"
    "99ac064000481b500158006000700078038001e6fae5980e880180019001f291f8e50e123008f5ed8641101318826438"
    "eedf99ac064000481b5001580060007000780380019f96aedc01880180019001f5c9c5d40e123008f5ed864110131883"
    "6438eedf99ac064000481b5001580060007000780380019fc4b2fa0e8801800190019fc4b2fa0e123008f5ed86411013"
    "18e96b38d5aee9a9064000480d500158006000700078038001a2bbfca20c88018a019001d2de8df60b123008f5ed8641"
    "101318ea6b38d5aee9a9064000480d500158006000700078038001d9bdbdb30288018a01900196def6a20e123008f5ed"
    "8641101318eb6b38d5aee9a9064000480d500158006000700078038001b2efa2d80a88018a019001fa8492980219a715"
    "e32931f16909220c080110f5ed86c190808088012801300039b692f15331f16900";

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
    uint64 client_steam_id{};
    bool has_client_steam_id{};
    int32 client_session_id{};
    bool has_client_session_id{};
    uint32 source_app_id{};
    bool has_source_app_id{};
    uint32 gc_msg_src{};
    bool has_gc_msg_src{};
    uint32 gc_dir_index_source{};
    bool has_gc_dir_index_source{};
};

static GBE_DotaServerHelloContext GBE_last_dota_server_hello_context;

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

struct GBE_DotaPracticeLobbyJoinRequest
{
    bool has_lobby_id{};
    uint64 lobby_id{};
    bool has_pass_key{};
    std::string pass_key;
};

struct GBE_DotaInviteToLobbyRequest
{
    bool has_steam_id{};
    uint64 steam_id{};
    bool has_client_version{};
    uint32 client_version{};
};

struct GBE_DotaLobbyInviteResponseRequest
{
    bool has_lobby_id{};
    uint64 lobby_id{};
    bool has_accept{};
    bool accept{};
    bool has_client_version{};
    uint32 client_version{};
    bool has_custom_game_crc{};
    uint64 custom_game_crc{};
    bool has_custom_game_timestamp{};
    uint32 custom_game_timestamp{};
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

struct GBE_DotaPracticeLobbyKickRequest
{
    bool has_account_id{};
    uint32 account_id{};
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

struct GBE_DotaChatMessageRequest
{
    bool has_account_id{};
    uint32 account_id{};
    bool has_channel_id{};
    uint64 channel_id{};
    bool has_persona_name{};
    std::string persona_name;
    bool has_text{};
    std::string text;
};

static void GBE_GC_DebugLog(const char *scope, const char *fmt, ...)
{
    const char *log_scope = scope ? scope : "GC";
    if (std::strcmp(log_scope, "GC_SEND") == 0 ||
        std::strcmp(log_scope, "GC_SEND_DOTA") == 0 ||
        std::strcmp(log_scope, "GC_DOTA_SYNC") == 0 ||
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

static bool GBE_RewriteProtoVarIntFields(
    const std::string &input,
    const std::vector<uint32> &field_numbers,
    uint64 value,
    std::string &output,
    bool *rewrote = nullptr)
{
    output.clear();
    if (rewrote)
        *rewrote = false;

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

        if (wire_type == 0u && std::find(field_numbers.begin(), field_numbers.end(), field_number) != field_numbers.end()) {
            GBE_AppendProtoVarIntField(output, field_number, value);
            if (rewrote)
                *rewrote = true;
            continue;
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    return true;
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

static std::string GBE_FormatHex(const uint8 *data, size_t size)
{
    if (!data || size == 0)
        return std::string();

    std::ostringstream stream;
    stream << std::hex << std::setfill('0');
    for (size_t i = 0; i < size; ++i) {
        if (i != 0)
            stream << ' ';
        stream << std::setw(2) << static_cast<unsigned int>(data[i]);
    }
    return stream.str();
}

static void GBE_LogHexDump(const char *tag, const char *label, const std::string &message, size_t bytes_per_line)
{
    if (!tag || !label || message.empty() || bytes_per_line == 0)
        return;

    GBE_GC_DebugLog(tag, "%s size=%zu", label, message.size());
    for (size_t offset = 0; offset < message.size(); offset += bytes_per_line) {
        const size_t chunk_size = std::min(bytes_per_line, message.size() - offset);
        GBE_GC_DebugLog(
            tag,
            "%s chunk offset=%zu size=%zu hex=%s",
            label,
            offset,
            chunk_size,
            GBE_FormatHex(reinterpret_cast<const uint8 *>(message.data()) + offset, chunk_size).c_str()
        );
    }
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

static bool GBE_BuildDotaLobbyAdditionalStartupAccountDataPayload(uint32 account_id, std::string &payload)
{
    payload.clear();
    if (account_id == 0)
        return true;

    GBE_AppendProtoVarIntField(payload, 1u, account_id);
    return true;
}

static bool GBE_AppendDotaLobbyAdditionalStartupAccountMessage(std::string &object_2015, uint32 account_id)
{
    if (account_id == 0)
        return true;

    std::string startup_payload;
    if (!GBE_BuildDotaLobbyAdditionalStartupAccountDataPayload(account_id, startup_payload))
        return false;

    std::string startup_message;
    GBE_AppendProtoVarIntField(startup_message, 1u, GBE_kDotaLobbyAdditionalAccountData);
    GBE_AppendProtoBytesField(startup_message, 2u, startup_payload);
    GBE_AppendProtoBytesField(object_2015, 2u, startup_message);
    return true;
}

static std::string GBE_BuildDotaLobbyTeamDetailsPayload(bool is_home_team)
{
    std::string team_details;
    // Keep scratch-built 2004.field17 structurally non-empty so prelaunch
    // updates do not collapse official team-details/completion state.
    GBE_AppendProtoVarIntField(team_details, 8u, 0u);
    GBE_AppendProtoVarIntField(team_details, 17u, is_home_team ? 1u : 0u);
    return team_details;
}

static void GBE_BuildDotaStaticLobbyObject2014(const std::string &player_name, const std::vector<GBE_DotaLobbyMemberState> &members, std::string &object_2014)
{
    object_2014.clear();

    const size_t member_count = std::max<size_t>(members.size(), 1u);
    for (size_t i = 0; i < member_count; ++i) {
        std::string name_entry;
        GBE_AppendProtoBytesField(name_entry, 1u, i == 0 ? player_name : std::string());
        GBE_AppendProtoVarIntField(name_entry, 2u, 0u);
        GBE_AppendProtoBytesField(object_2014, 1u, name_entry);
    }
}

static bool GBE_BuildDotaServerLobbyObject2015(size_t member_count, uint32 extra_startup_account_id, std::string &object_2015)
{
    object_2015.clear();

    // CSODOTAServerLobbyMember is empty, but official SO payloads keep this
    // repeated field aligned with the visible lobby member count.
    const size_t effective_member_count = std::max<size_t>(member_count, 1u);
    for (size_t i = 0; i < effective_member_count; ++i)
        GBE_AppendProtoBytesField(object_2015, 1u, std::string());
    return GBE_AppendDotaLobbyAdditionalStartupAccountMessage(object_2015, extra_startup_account_id);
}

static void GBE_AppendDotaLobbyEventPeriodicResource(
    std::string &account_points,
    uint32 periodic_resource_id,
    uint32 remaining,
    uint32 max)
{
    std::string resource;
    GBE_AppendProtoVarIntField(resource, 1u, periodic_resource_id);
    GBE_AppendProtoVarIntField(resource, 2u, remaining);
    GBE_AppendProtoVarIntField(resource, 3u, max);
    GBE_AppendProtoBytesField(account_points, 31u, resource);
}

static void GBE_AppendDotaLobbyEventAccountPoints(
    std::string &event_points,
    uint32 account_id,
    uint32 normal_points,
    uint32 premium_points,
    bool owned,
    uint32 event_level,
    bool include_periodic_resources)
{
    std::string account_points;
    GBE_AppendProtoVarIntField(account_points, 1u, account_id);
    GBE_AppendProtoVarIntField(account_points, 2u, normal_points);
    GBE_AppendProtoVarIntField(account_points, 3u, premium_points);
    GBE_AppendProtoVarIntField(account_points, 4u, owned ? 1u : 0u);
    GBE_AppendProtoVarIntField(account_points, 7u, event_level);
    GBE_AppendProtoVarIntField(account_points, 12u, 0u);
    GBE_AppendProtoVarIntField(account_points, 26u, 0u);
    GBE_AppendProtoVarIntField(account_points, 27u, 0u);
    GBE_AppendProtoVarIntField(account_points, 28u, 0u);
    if (include_periodic_resources) {
        GBE_AppendDotaLobbyEventPeriodicResource(account_points, 15u, 10u, 10u);
        GBE_AppendDotaLobbyEventPeriodicResource(account_points, 28u, 1000u, 1000u);
    }

    GBE_AppendProtoBytesField(event_points, 2u, account_points);
}

static void GBE_AppendDotaLobbyEventPoints(
    std::string &object_2016,
    uint32 event_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    uint32 owner_account_id,
    uint32 normal_points,
    uint32 premium_points,
    bool owner_owned,
    bool non_owner_owned,
    uint32 event_level,
    bool include_periodic_resources)
{
    std::string event_points;
    GBE_AppendProtoVarIntField(event_points, 1u, event_id);
    for (const GBE_DotaLobbyMemberState &member : members) {
        if (member.steam_id == 0ull)
            continue;
        const uint32 member_account_id = member.account_id != 0u ? member.account_id : CSteamID((uint64)member.steam_id).GetAccountID();
        if (member_account_id == 0u)
            continue;
        GBE_AppendDotaLobbyEventAccountPoints(
            event_points,
            member_account_id,
            normal_points,
            premium_points,
            member_account_id == owner_account_id ? owner_owned : non_owner_owned,
            event_level,
            include_periodic_resources);
    }
    GBE_AppendProtoBytesField(object_2016, 3u, event_points);
}

static void GBE_BuildDotaServerStaticLobbyObject2016(uint32 account_id, uint64 steam_id, uint32 game_mode, std::string &object_2016)
{
    std::vector<GBE_DotaLobbyMemberState> members;
    GBE_DotaLobbyMemberState owner{};
    owner.steam_id = steam_id;
    owner.account_id = account_id;
    members.push_back(owner);
    GBE_BuildDotaServerStaticLobbyObject2016(account_id, steam_id, game_mode, members, object_2016);
}

static void GBE_BuildDotaServerStaticLobbyObject2016(
    uint32 account_id,
    uint64 steam_id,
    uint32 game_mode,
    const std::vector<GBE_DotaLobbyMemberState> &members,
    std::string &object_2016)
{
    object_2016.clear();

    std::vector<GBE_DotaLobbyMemberState> effective_members = members;
    if (effective_members.empty()) {
        GBE_DotaLobbyMemberState owner{};
        owner.steam_id = steam_id;
        owner.account_id = account_id;
        effective_members.push_back(owner);
    }

    bool wrote_owner_event_points = false;
    bool include_event_points = false;
    for (const GBE_DotaLobbyMemberState &member : effective_members) {
        const uint64 member_steam_id = member.steam_id;
        if (member_steam_id == 0ull) {
            GBE_AppendProtoBytesField(object_2016, 1u, std::string(1u, '\0'));
            continue;
        }

        if (member.connected)
            include_event_points = true;

        const uint32 member_account_id = member.account_id != 0u ? member.account_id : CSteamID((uint64)member_steam_id).GetAccountID();

        std::string member_bytes;
        GBE_AppendProtoFixed64Field(member_bytes, 1u, member_steam_id);
        GBE_AppendProtoVarIntField(member_bytes, 9u, 0u);
        GBE_AppendProtoVarIntField(member_bytes, 11u, 0u);
        GBE_AppendProtoFixed64Field(member_bytes, 12u, 0ull);
        GBE_AppendProtoVarIntField(member_bytes, 13u, 0u);
        if (game_mode != 2u && member.connected)
            GBE_AppendProtoFixed32Field(member_bytes, 16u, 0u);
        for (size_t i = 0; i < 4; ++i)
            GBE_AppendProtoVarIntField(member_bytes, 19u, 0u);
        GBE_AppendProtoBytesField(object_2016, 1u, member_bytes);

        if (!wrote_owner_event_points && member_account_id == account_id)
            wrote_owner_event_points = true;
    }

    if (account_id != 0u)
        wrote_owner_event_points = true;

    GBE_AppendProtoFixed32Field(object_2016, 2u, 0u);

    if (include_event_points && account_id != 0u && wrote_owner_event_points) {
        GBE_AppendDotaLobbyEventPoints(object_2016, 19u, effective_members, account_id, 0u, 0u, true, false, 0u, true);
        GBE_AppendDotaLobbyEventPoints(object_2016, 26u, effective_members, account_id, 0u, 0u, true, true, 0u, false);
        GBE_AppendDotaLobbyEventPoints(object_2016, 39u, effective_members, account_id, 0u, 0u, true, true, 0u, false);
        GBE_AppendDotaLobbyEventPoints(object_2016, 56u, effective_members, account_id, 1000u, 0u, true, true, 1u, false);
    }
}

static void GBE_BuildDotaLobbyMemberObject2004(const GBE_DotaLobbyMemberState &member, uint32 lobby_state, uint32 lobby_game_state, std::string &member_state)
{
    member_state.clear();
    if (member.steam_id == 0ull) {
        member_state.assign(1u, '\0');
        return;
    }

    GBE_AppendProtoFixed64Field(member_state, 1u, member.steam_id);
    if (member.hero_id != 0u)
        GBE_AppendProtoVarIntField(member_state, 2u, member.hero_id);
    GBE_AppendProtoVarIntField(member_state, 3u, member.team);
    if (member.slot != 0u)
        GBE_AppendProtoVarIntField(member_state, 7u, member.slot);
    const bool member_leaver_disconnected = !member.connected && lobby_state == 2u && lobby_game_state >= 1u;
    if (member_leaver_disconnected) {
        GBE_AppendProtoVarIntField(member_state, 16u, 1u);
        if (lobby_state == 2u && lobby_game_state >= 1u)
            GBE_AppendProtoVarIntField(member_state, 28u, 0u);
    }
}

static std::vector<GBE_DotaLobbyMemberState> GBE_BuildDotaLobbyMembers(
    uint64 owner_steam_id,
    uint32 owner_account_id,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    bool owner_connected,
    const std::vector<GBE_DotaLobbyMemberState> &members)
{
    std::vector<GBE_DotaLobbyMemberState> result;

    auto append_or_update = [&result](GBE_DotaLobbyMemberState member) {
        if (member.steam_id == 0ull) {
            result.push_back(member);
            return;
        }
        if (member.account_id == 0u)
            member.account_id = CSteamID((uint64)member.steam_id).GetAccountID();
        for (GBE_DotaLobbyMemberState &existing : result) {
            if (existing.steam_id == member.steam_id) {
                existing = member;
                return;
            }
        }
        result.push_back(member);
    };

    bool has_owner = false;
    for (const GBE_DotaLobbyMemberState &member : members) {
        if (member.steam_id == owner_steam_id) {
            has_owner = true;
            break;
        }
    }

    if (!has_owner) {
        GBE_DotaLobbyMemberState owner{};
        owner.steam_id = owner_steam_id;
        owner.account_id = owner_account_id;
        owner.team = owner_team;
        owner.slot = owner_slot;
        owner.hero_id = owner_hero_id;
        owner.connected = owner_connected;
        append_or_update(owner);
    }

    for (GBE_DotaLobbyMemberState member : members) {
        if (member.steam_id == owner_steam_id) {
            member.account_id = owner_account_id;
            member.team = owner_team;
            member.slot = owner_slot;
            member.hero_id = owner_hero_id;
            member.connected = owner_connected;
        } else if (member.team == GBE_kDotaTeamPlayerPool) {
            member.slot = 0u;
        }
        append_or_update(member);
    }

    return result;
}

static bool GBE_DotaLobbyMembersEqual(
    const std::vector<GBE_DotaLobbyMemberState> &left,
    const std::vector<GBE_DotaLobbyMemberState> &right)
{
    if (left.size() != right.size())
        return false;

    for (size_t i = 0; i < left.size(); ++i) {
        if (left[i].steam_id != right[i].steam_id ||
                left[i].account_id != right[i].account_id ||
                left[i].team != right[i].team ||
                left[i].slot != right[i].slot ||
                left[i].hero_id != right[i].hero_id ||
                left[i].connected != right[i].connected)
            return false;
    }

    return true;
}

static bool GBE_DotaLobbyMembersContainSteamId(const std::vector<GBE_DotaLobbyMemberState> &members, uint64 steam_id)
{
    if (steam_id == 0ull)
        return false;

    for (const GBE_DotaLobbyMemberState &member : members) {
        if (member.steam_id == steam_id)
            return true;
    }

    return false;
}

static bool GBE_FindDotaLobbyMemberIndex(const std::vector<GBE_DotaLobbyMemberState> &members, uint64 steam_id, size_t &index)
{
    if (steam_id == 0ull)
        return false;

    for (size_t i = 0; i < members.size(); ++i) {
        if (members[i].steam_id == steam_id) {
            index = i;
            return true;
        }
    }

    return false;
}

static void GBE_PreserveDotaLobbyOwnerTransferSlots(
    std::vector<GBE_DotaLobbyMemberState> &members,
    const std::vector<GBE_DotaLobbyMemberState> &previous_members,
    uint64 previous_owner_steam_id,
    uint64 new_owner_steam_id)
{
    if (previous_owner_steam_id == 0ull || new_owner_steam_id == 0ull || previous_owner_steam_id == new_owner_steam_id)
        return;

    size_t previous_owner_index = 0;
    size_t previous_new_owner_index = 0;
    if (!GBE_FindDotaLobbyMemberIndex(previous_members, previous_owner_steam_id, previous_owner_index) ||
            !GBE_FindDotaLobbyMemberIndex(previous_members, new_owner_steam_id, previous_new_owner_index))
        return;

    size_t current_new_owner_index = 0;
    if (!GBE_FindDotaLobbyMemberIndex(members, new_owner_steam_id, current_new_owner_index))
        return;

    const GBE_DotaLobbyMemberState new_owner = members[current_new_owner_index];
    std::vector<GBE_DotaLobbyMemberState> reordered(std::max(previous_members.size(), previous_new_owner_index + 1));
    std::vector<bool> occupied(reordered.size(), false);

    if (previous_owner_index < reordered.size()) {
        reordered[previous_owner_index] = GBE_DotaLobbyMemberState{};
        occupied[previous_owner_index] = true;
    }

    reordered[previous_new_owner_index] = new_owner;
    occupied[previous_new_owner_index] = true;

    for (const GBE_DotaLobbyMemberState &member : members) {
        if (member.steam_id == 0ull || member.steam_id == new_owner_steam_id || member.steam_id == previous_owner_steam_id)
            continue;

        size_t previous_index = 0;
        if (GBE_FindDotaLobbyMemberIndex(previous_members, member.steam_id, previous_index)) {
            if (previous_index >= reordered.size()) {
                reordered.resize(previous_index + 1);
                occupied.resize(previous_index + 1, false);
            }
            if (!occupied[previous_index]) {
                reordered[previous_index] = member;
                occupied[previous_index] = true;
                continue;
            }
        }

        reordered.push_back(member);
        occupied.push_back(true);
    }

    while (!reordered.empty() && reordered.back().steam_id == 0ull)
        reordered.pop_back();

    members = reordered;
}

static void GBE_UpsertDotaLobbyMember(std::vector<GBE_DotaLobbyMemberState> &members, const GBE_DotaLobbyMemberState &member)
{
    if (member.steam_id == 0ull)
        return;

    for (GBE_DotaLobbyMemberState &existing : members) {
        if (existing.steam_id == member.steam_id) {
            existing = member;
            return;
        }
    }

    members.push_back(member);
}

static bool GBE_IsDotaPracticeLobbyPrelaunchState(uint64 server_id, uint64 match_id, uint32 game_start_time, const std::string &connect)
{
    return server_id == 0ull && match_id == 0ull && game_start_time == 0u && connect.empty();
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

static bool GBE_RewriteDotaAccountBoundObjectData(const std::string &input, int type_id, uint32 account_id, std::string &output)
{
    std::string account_output;
    account_output.clear();
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
            GBE_AppendProtoVarIntField(account_output, 1u, account_id);
            continue;
        }

        account_output.append(input.data() + field_offset, field_end - field_offset);
    }

    if (!saw_account_id)
        GBE_AppendProtoVarIntField(account_output, 1u, account_id);

    if (type_id != 2002) {
        output.swap(account_output);
        return true;
    }

    bool rewrote_conduct_score = false;
    std::string conduct_output;
    if (!GBE_RewriteProtoVarIntFields(account_output, { 72u }, GBE_kDotaConductScore, conduct_output, &rewrote_conduct_score))
        return false;
    if (!rewrote_conduct_score)
        GBE_AppendProtoVarIntField(conduct_output, 72u, GBE_kDotaConductScore);

    // Clear ban/disable/restriction fields from the welcome account SO;
    // stale donor timestamps cause Dota to show restriction popups.
    // NOTE: field 69 (account_flags) must NOT be cleared -- the client checks
    // bit 0/1 to determine if the account is verified; zeroing it triggers
    // the "unable to verify" (VAC) popup and blocks lobby invite acceptance.
    // Fields cleared:
    //   18  low_priority_until_date
    //   20  prevent_text_chat_until_date
    //   21  prevent_voice_until_date
    //   38  account_disabled_until_date
    //   39  account_disabled_count
    //   41  match_disabled_until_date
    //   42  match_disabled_count
    //   48  low_priority_games_remaining
    //   86  prevent_public_text_chat_until_date
    //   89  ranked_matchmaking_ban_until_date
    //  105  custom_game_disabled_until_date
    //  122  prevent_new_player_chat_until_date
    return GBE_RewriteProtoVarIntFields(conduct_output, { 18u, 20u, 21u, 38u, 39u, 41u, 42u, 48u, 86u, 89u, 105u, 122u }, 0u, output, nullptr);
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
                if (!GBE_RewriteDotaAccountBoundObjectData(object->object_data(data_index), type_id, account_id, rewritten_object))
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

static std::string GBE_FormatDotaLobbyMemberStateSummary(const std::string &input)
{
    const uint8 *data = reinterpret_cast<const uint8 *>(input.data());
    const size_t size = input.size();

    uint64 steam_id = 0;
    uint32 account_id = 0;
    uint32 hero_id = 0;
    uint32 team = 0;
    uint32 slot = 0;
    uint32 leaver_status = 0;
    uint32 leaver_actions = 0;

    const bool has_steam_id = GBE_ExtractProtoFieldUint64(data, size, GBE_FindProtoField(data, size, 1u), steam_id);
    const bool has_account_id = GBE_ExtractProtoFieldUint32(data, size, GBE_FindProtoField(data, size, 55u), account_id);
    const bool has_hero_id = GBE_ExtractProtoFieldUint32(data, size, GBE_FindProtoField(data, size, 2u), hero_id);
    const bool has_team = GBE_ExtractProtoFieldUint32(data, size, GBE_FindProtoField(data, size, 3u), team);
    const bool has_slot = GBE_ExtractProtoFieldUint32(data, size, GBE_FindProtoField(data, size, 7u), slot);
    const bool has_leaver_status = GBE_ExtractProtoFieldUint32(data, size, GBE_FindProtoField(data, size, 16u), leaver_status);
    const bool has_leaver_actions = GBE_ExtractProtoFieldUint32(data, size, GBE_FindProtoField(data, size, 28u), leaver_actions);

    char buffer[320];
    std::snprintf(
        buffer,
        sizeof(buffer),
        "steam_id=%llu account_id=%u hero_id=%u team=%u slot=%u leaver_status=%u leaver_actions=%u flags[steam_id=%u account_id=%u hero_id=%u team=%u slot=%u leaver_status=%u leaver_actions=%u]",
        static_cast<unsigned long long>(steam_id),
        account_id,
        hero_id,
        team,
        slot,
        leaver_status,
        leaver_actions,
        has_steam_id ? 1u : 0u,
        has_account_id ? 1u : 0u,
        has_hero_id ? 1u : 0u,
        has_team ? 1u : 0u,
        has_slot ? 1u : 0u,
        has_leaver_status ? 1u : 0u,
        has_leaver_actions ? 1u : 0u);
    return std::string(buffer);
}

static std::string GBE_FormatDotaStaticLobbyMemberSummary(const std::string &input)
{
    const uint8 *data = reinterpret_cast<const uint8 *>(input.data());
    const size_t size = input.size();

    std::string name;
    uint64 party_id = 0;

    const bool has_name = GBE_ExtractProtoFieldBytes(data, size, GBE_FindProtoField(data, size, 1u), name);
    const bool has_party_id = GBE_ExtractProtoFieldUint64(data, size, GBE_FindProtoField(data, size, 2u), party_id);

    char buffer[320];
    std::snprintf(
        buffer,
        sizeof(buffer),
        "name=%s party_id=%llu flags[name=%u party_id=%u]",
        has_name ? name.c_str() : "",
        static_cast<unsigned long long>(party_id),
        has_name ? 1u : 0u,
        has_party_id ? 1u : 0u);
    return std::string(buffer);
}

static std::string GBE_FormatDotaServerStaticLobbyMemberSummary(const std::string &input)
{
    const uint8 *data = reinterpret_cast<const uint8 *>(input.data());
    const size_t size = input.size();

    uint64 steam_id = 0;
    uint32 rank_tier = 0;
    uint32 coach_rating = 0;
    uint32 favorite_team_packed_lo = 0;

    const bool has_steam_id = GBE_ExtractProtoFieldUint64(data, size, GBE_FindProtoField(data, size, 1u), steam_id);
    const bool has_rank_tier = GBE_ExtractProtoFieldUint32(data, size, GBE_FindProtoField(data, size, 3u), rank_tier);
    const bool has_coach_rating = GBE_ExtractProtoFieldUint32(data, size, GBE_FindProtoField(data, size, 7u), coach_rating);
    const bool has_favorite_team_packed = GBE_ExtractProtoFieldUint32(data, size, GBE_FindProtoField(data, size, 12u), favorite_team_packed_lo);

    char buffer[256];
    std::snprintf(
        buffer,
        sizeof(buffer),
        "steam_id=%llu rank_tier=%u coach_rating=%u favorite_team_packed_lo=%u flags[steam_id=%u rank_tier=%u coach_rating=%u favorite_team_packed=%u]",
        static_cast<unsigned long long>(steam_id),
        rank_tier,
        coach_rating,
        favorite_team_packed_lo,
        has_steam_id ? 1u : 0u,
        has_rank_tier ? 1u : 0u,
        has_coach_rating ? 1u : 0u,
        has_favorite_team_packed ? 1u : 0u);
    return std::string(buffer);
}

static std::string GBE_FormatProtoRepeatedVarIntFieldSummary(const std::string &input, uint32 target_field)
{
    const uint8 *data = reinterpret_cast<const uint8 *>(input.data());
    const size_t size = input.size();

    std::ostringstream stream;
    bool first = true;
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

        if (field_number == target_field && wire_type == 0u) {
            uint64 value = 0;
            size_t field_value_offset = value_offset;
            if (GBE_ReadVarUint64(data, size, field_value_offset, value)) {
                if (!first)
                    stream << ',';
                first = false;
                stream << value;
            }
        }
    }

    if (first)
        return "-";

    return stream.str();
}

static std::string GBE_FormatDotaLobbyAuxFieldSummary(const std::string &input)
{
    std::ostringstream stream;
    stream << "121=[" << GBE_FormatProtoRepeatedVarIntFieldSummary(input, 121u)
           << "] 122=[" << GBE_FormatProtoRepeatedVarIntFieldSummary(input, 122u)
           << "] 123=[" << GBE_FormatProtoRepeatedVarIntFieldSummary(input, 123u)
           << "] 124=[" << GBE_FormatProtoRepeatedVarIntFieldSummary(input, 124u)
           << "] 132=[" << GBE_FormatProtoRepeatedVarIntFieldSummary(input, 132u)
           << ']';
    return stream.str();
}

static uint32 GBE_CountProtoRepeatedBytesField(const std::string &input, uint32 target_field)
{
    const uint8 *data = reinterpret_cast<const uint8 *>(input.data());
    const size_t size = input.size();

    uint32 count = 0;
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

        if (field_number == target_field && wire_type == 2u)
            ++count;

        offset = field_end;
    }

    return count;
}

static std::string GBE_FormatProtoTopLevelFieldSummary(const uint8 *data, size_t size)
{
    if (!data || size == 0)
        return "empty";

    std::ostringstream stream;
    bool first = true;
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

        if (!first)
            stream << ',';
        first = false;
        stream << field_number << ':' << wire_type << ':' << value_size;

        if (wire_type == 0u) {
            uint64 value = 0;
            size_t field_value_offset = value_offset;
            if (GBE_ReadVarUint64(data, size, field_value_offset, value))
                stream << '=' << value;
        } else if (wire_type == 1u && value_size == 8u) {
            uint64 value = 0;
            std::memcpy(&value, data + value_offset, sizeof(value));
            stream << '=' << value;
        } else if (wire_type == 5u && value_size == 4u) {
            uint32 value = 0;
            std::memcpy(&value, data + value_offset, sizeof(value));
            stream << '=' << value;
        }
    }

    if (first)
        return "empty";

    return stream.str();
}

static std::string GBE_FormatProtoFieldLayoutSummary(const std::string &input)
{
    std::ostringstream stream;
    const uint8 *data = reinterpret_cast<const uint8 *>(input.data());
    const size_t size = input.size();

    size_t offset = 0;
    bool first = true;
    while (offset < size) {
        uint32 field_number = 0;
        uint32 wire_type = 0;
        size_t field_offset = 0;
        size_t value_offset = 0;
        size_t value_size = 0;
        size_t field_end = 0;
        if (!GBE_ReadNextProtoField(data, size, offset, field_number, wire_type, field_offset, value_offset, value_size, field_end))
            break;

        if (!first)
            stream << ',';
        first = false;
        stream << field_number << ':' << wire_type << ':' << value_size;
    }

    return stream.str();
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
    uint64 connected_steam_id{};
    uint32 connected_hero_id{};
    uint64 draft_steam_id{};
    uint32 draft_team{};
    uint32 draft_team_slot{};
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
    bool has_connected_player{};
    bool has_connected_steam_id{};
    bool has_connected_hero_id{};
    bool has_draft{};
    bool has_draft_steam_id{};
    bool has_draft_team{};
    bool has_draft_team_slot{};
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

    std::string connected_player_raw;
    if (GBE_ExtractProtoFieldBytes(data, size, GBE_FindProtoField(data, size, 1u), connected_player_raw) && !connected_player_raw.empty()) {
        shape.has_connected_player = true;

        const uint8 *player_data = reinterpret_cast<const uint8 *>(connected_player_raw.data());
        const size_t player_size = connected_player_raw.size();
        uint64 connected_steam_id = 0;
        if (GBE_ExtractProtoFieldUint64(player_data, player_size, GBE_FindProtoField(player_data, player_size, 1u), connected_steam_id)) {
            shape.connected_steam_id = connected_steam_id;
            shape.has_connected_steam_id = true;
        }

        uint32 connected_hero_id = 0;
        if (GBE_ExtractProtoFieldUint32(player_data, player_size, GBE_FindProtoField(player_data, player_size, 2u), connected_hero_id)) {
            shape.connected_hero_id = connected_hero_id;
            shape.has_connected_hero_id = true;
        }
    }

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

    std::string draft_raw;
    if (GBE_ExtractProtoFieldBytes(data, size, GBE_FindProtoField(data, size, 16u), draft_raw) && !draft_raw.empty()) {
        shape.has_draft = true;

        const uint8 *draft_data = reinterpret_cast<const uint8 *>(draft_raw.data());
        const size_t draft_size = draft_raw.size();
        uint64 draft_steam_id = 0;
        if (GBE_ExtractProtoFieldUint64(draft_data, draft_size, GBE_FindProtoField(draft_data, draft_size, 1u), draft_steam_id)) {
            shape.draft_steam_id = draft_steam_id;
            shape.has_draft_steam_id = true;
        }

        uint32 draft_team = 0;
        if (GBE_ExtractProtoFieldUint32(draft_data, draft_size, GBE_FindProtoField(draft_data, draft_size, 2u), draft_team)) {
            shape.draft_team = draft_team;
            shape.has_draft_team = true;
        }

        uint32 draft_team_slot = 0;
        if (GBE_ExtractProtoFieldUint32(draft_data, draft_size, GBE_FindProtoField(draft_data, draft_size, 3u), draft_team_slot)) {
            shape.draft_team_slot = draft_team_slot;
            shape.has_draft_team_slot = true;
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

static bool GBE_IsDotaDireTeam(uint32 team)
{
    return team == GBE_kDotaTeamBadGuys || team == 3u;
}

static bool GBE_RewriteDotaLobbyTemplateMemberObject(
    const std::string &input,
    uint32 account_id,
    uint64 steam_id,
    uint32 owner_team,
    uint32 owner_slot,
    uint32 owner_hero_id,
    bool force_connected_leaver_state,
    std::string &output)
{
    output.clear();
    bool saw_team = false;
    bool saw_slot = false;
    bool saw_hero_id = false;
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

        if (field_number == 2u && wire_type == 0u) {
            saw_hero_id = true;
            if (owner_hero_id != 0u)
                GBE_AppendProtoVarIntField(output, 2u, owner_hero_id);
            continue;
        }

        if (field_number == 3u && wire_type == 0u) {
            saw_team = true;
            GBE_AppendProtoVarIntField(output, 3u, owner_team);
            continue;
        }

        if (field_number == 7u && wire_type == 0u) {
            saw_slot = true;
            GBE_AppendProtoVarIntField(output, 7u, owner_slot);
            continue;
        }

        if (field_number == 16u && wire_type == 5u) {
            saw_leaver_status = true;
            if (force_connected_leaver_state) {
                GBE_AppendProtoFixed32Field(output, 16u, 0u);
                continue;
            }
        }

        if (field_number == 28u && wire_type == 0u) {
            saw_leaver_actions = true;
            if (force_connected_leaver_state) {
                GBE_AppendProtoVarIntField(output, 28u, 0u);
                continue;
            }
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    if (!saw_team)
        GBE_AppendProtoVarIntField(output, 3u, owner_team);

    if (!saw_slot)
        GBE_AppendProtoVarIntField(output, 7u, owner_slot);

    if (!saw_hero_id && owner_hero_id != 0u)
        GBE_AppendProtoVarIntField(output, 2u, owner_hero_id);

    if (force_connected_leaver_state && !saw_leaver_status)
        GBE_AppendProtoFixed32Field(output, 16u, 0u);

    if (force_connected_leaver_state && !saw_leaver_actions)
        GBE_AppendProtoVarIntField(output, 28u, 0u);

    return true;
}

static bool GBE_RewriteDotaServerStaticLobbyMemberObject(
    const std::string &input,
    uint32 account_id,
    uint64 steam_id,
    std::string &output)
{
    std::string account_rewritten_input = input;
    if (account_id != 0) {
        std::string rewritten_account_fields;
        size_t replacement_count = 0;
        if (!GBE_RewriteProtoVarintBytesRecursive(
                input,
                GBE_VectorFromBytes(GBE_kOldDotaAccountIdVarint.data(), GBE_kOldDotaAccountIdVarint.size()),
                1u,
                account_id,
                rewritten_account_fields,
                replacement_count))
            return false;
        if (replacement_count > 0)
            account_rewritten_input.swap(rewritten_account_fields);
    }

    output.clear();
    bool saw_steam_id = false;

    size_t offset = 0;
    while (offset < account_rewritten_input.size()) {
        uint32 field_number = 0;
        uint32 wire_type = 0;
        size_t field_offset = 0;
        size_t value_offset = 0;
        size_t value_size = 0;
        size_t field_end = 0;
        if (!GBE_ReadNextProtoField(
                reinterpret_cast<const uint8 *>(account_rewritten_input.data()),
                account_rewritten_input.size(),
                offset,
                field_number,
                wire_type,
                field_offset,
                value_offset,
                value_size,
                field_end))
            return false;

        if (field_number == 1u && wire_type == 1u) {
            saw_steam_id = true;
            GBE_AppendProtoFixed64Field(output, 1u, steam_id);
            continue;
        }

        output.append(account_rewritten_input.data() + field_offset, field_end - field_offset);
    }

    if (saw_steam_id)
        return true;

    GBE_AppendProtoFixed64Field(output, 1u, steam_id);
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

static bool GBE_ParseDotaPracticeLobbyKickBody(const uint8 *body, size_t body_size, GBE_DotaPracticeLobbyKickRequest &request)
{
    request = {};

    if (!body || body_size == 0)
        return false;

    uint64 value = 0;
    if (!GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 3), value))
        return false;

    request.has_account_id = true;
    request.account_id = static_cast<uint32>(value);
    return true;
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

static bool GBE_ParseDotaPracticeLobbyJoinBody(const uint8 *body, size_t body_size, GBE_DotaPracticeLobbyJoinRequest &request)
{
    request = {};

    if (!body || body_size == 0)
        return false;

    uint64 value = 0;
    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 1), value)) {
        request.has_lobby_id = true;
        request.lobby_id = value;
    }

    if (GBE_ExtractProtoFieldBytes(body, body_size, GBE_FindProtoField(body, body_size, 3), request.pass_key))
        request.has_pass_key = true;

    return request.has_lobby_id || request.has_pass_key;
}

static bool GBE_ParseDotaInviteToLobbyBody(const uint8 *body, size_t body_size, GBE_DotaInviteToLobbyRequest &request)
{
    request = {};

    if (!body || body_size == 0)
        return false;

    uint64 value = 0;
    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 1), value)) {
        request.has_steam_id = true;
        request.steam_id = value;
    }

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 2), value)) {
        request.has_client_version = true;
        request.client_version = static_cast<uint32>(value);
    }

    return request.has_steam_id;
}

static bool GBE_ParseDotaLobbyInviteResponseBody(const uint8 *body, size_t body_size, GBE_DotaLobbyInviteResponseRequest &request)
{
    request = {};

    if (!body || body_size == 0)
        return false;

    uint64 value = 0;
    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 1), value)) {
        request.has_lobby_id = true;
        request.lobby_id = value;
    }

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 2), value)) {
        request.has_accept = true;
        request.accept = value != 0;
    }

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 3), value)) {
        request.has_client_version = true;
        request.client_version = static_cast<uint32>(value);
    }

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 6), value)) {
        request.has_custom_game_crc = true;
        request.custom_game_crc = value;
    }

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 7), value)) {
        request.has_custom_game_timestamp = true;
        request.custom_game_timestamp = static_cast<uint32>(value);
    }

    return request.has_lobby_id || request.has_accept;
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

static bool GBE_ParseDotaChatMessageBody(const uint8 *body, size_t body_size, GBE_DotaChatMessageRequest &request)
{
    request = {};

    if (!body || body_size == 0)
        return false;

    uint64 value = 0;
    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 1), value)) {
        request.has_account_id = true;
        request.account_id = static_cast<uint32>(value);
    }

    if (GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 2), value)) {
        request.has_channel_id = true;
        request.channel_id = value;
    }

    if (GBE_ExtractProtoFieldBytes(body, body_size, GBE_FindProtoField(body, body_size, 3), request.persona_name))
        request.has_persona_name = true;

    if (GBE_ExtractProtoFieldBytes(body, body_size, GBE_FindProtoField(body, body_size, 4), request.text))
        request.has_text = true;

    return request.has_channel_id && request.has_text;
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

static bool GBE_ShouldTraceGCProtoBoundary(uint32 emsg)
{
    switch (emsg) {
        case 24u:
        case 4005u:
        case 4511u:
        case 7034u:
        case 7450u:
        case 7451u:
            return true;
        default:
            return false;
    }
}

static void GBE_LogGCProtoBoundary(const char *scope, const char *direction, void *self, bool is_server, uint32 emsg, const void *data, uint32 size)
{
    if (!scope || !direction || !data || size < sizeof(ProtoBufMsgHeader_t) || !GBE_ShouldTraceGCProtoBoundary(emsg))
        return;

    ProtoBufMsgHeader_t hdr{};
    CMsgProtoBufHeader protohdr;
    const uint8 *body = nullptr;
    size_t body_size = 0;
    if (!GBE_ParseDirectProtoContext(data, size, hdr, protohdr, body, body_size)) {
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
        hdr.m_cubProtoBufExtHdr,
        body_size,
        protohdr.has_job_id_source() ? 1u : 0u,
        static_cast<unsigned long long>(protohdr.has_job_id_source() ? protohdr.job_id_source() : 0ull),
        protohdr.has_job_id_target() ? 1u : 0u,
        static_cast<unsigned long long>(protohdr.has_job_id_target() ? protohdr.job_id_target() : 0ull),
        static_cast<unsigned long long>(protohdr.has_client_steam_id() ? protohdr.client_steam_id() : 0ull),
        protohdr.has_client_session_id() ? protohdr.client_session_id() : 0,
        protohdr.has_source_app_id() ? protohdr.source_app_id() : 0u
    );
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
        if (!GBE_EncodeVarUint64WithExpectedSize(steam_id, GBE_kOldDotaSteamIdVarint.size(), encoded_steam_id)) {
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

        const std::vector<uint8> old_steam_id_varint = GBE_VectorFromBytes(GBE_kOldDotaSteamIdVarint.data(), GBE_kOldDotaSteamIdVarint.size());
        const size_t steam_id_match_count = GBE_CountBytePatternMatches(message, old_steam_id_varint);
        if (steam_id_match_count != 0 && !GBE_FindAndOverwriteBytes(message, old_steam_id_varint, encoded_steam_id))
            return false;

        if (steam_id_match_count == 0) {
            GBE_GC_DebugLog(
                "GC_DOTA_PATCH",
                "steam_id template rewrite skipped; donor does not expose expected varint req=%u resp=%u note=%s steam_id=%llu expected_size=%zu",
                request_emsg,
                response_emsg,
                context_note ? context_note : "",
                static_cast<unsigned long long>(steam_id),
                GBE_kOldDotaSteamIdVarint.size());
        }
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

static bool GBE_PatchDotaLobbyTemplateIdentifiersIfPresent(std::string &message, uint64 steam_id, uint64 lobby_id)
{
    std::vector<uint8> encoded_lobby_id;
    if (!GBE_EncodeVarUint64WithExpectedSize(lobby_id, GBE_kOldDotaLobbyIdVarint.size(), encoded_lobby_id))
        return false;

    const std::vector<uint8> old_lobby_id = GBE_VectorFromBytes(GBE_kOldDotaLobbyIdVarint.data(), GBE_kOldDotaLobbyIdVarint.size());
    const size_t lobby_id_match_count = GBE_CountBytePatternMatches(message, old_lobby_id);
    if (lobby_id_match_count != 0 && !GBE_FindAndOverwriteBytes(message, old_lobby_id, encoded_lobby_id)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed replacing optional lobby_id bytes lobby_id=%llu", static_cast<unsigned long long>(lobby_id));
        return false;
    }

    std::string steam_id_fixed64_raw;
    GBE_AppendLittleEndian64(steam_id_fixed64_raw, steam_id);
    const std::vector<uint8> old_steam_id_fixed64 = GBE_VectorFromBytes(GBE_kOldDotaSteamIdFixed64.data(), GBE_kOldDotaSteamIdFixed64.size());
    const size_t steam_id_fixed64_match_count = GBE_CountBytePatternMatches(message, old_steam_id_fixed64);
    if (steam_id_fixed64_match_count != 0 && !GBE_FindAndOverwriteBytes(
            message,
            old_steam_id_fixed64,
            GBE_VectorFromBytes(reinterpret_cast<const uint8 *>(steam_id_fixed64_raw.data()), steam_id_fixed64_raw.size()))) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed replacing optional steam_id fixed64 bytes steam_id=%llu", static_cast<unsigned long long>(steam_id));
        return false;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Patched optional template identifiers LobbyID matches=%zu SteamIDFixed64 matches=%zu body_prefix=%s",
        lobby_id_match_count,
        steam_id_fixed64_match_count,
        GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(message.data()), message.size(), 32).c_str()
    );
    return true;
}

static bool GBE_ForceDotaLobbyCacheOwnerSOID(std::string &message, uint64 lobby_id)
{
    if (message.size() < sizeof(ProtoBufMsgHeader_t))
        return false;

    ProtoBufMsgHeader_t hdr{};
    std::memcpy(&hdr, message.data(), sizeof(hdr));
    const size_t body_offset = sizeof(hdr) + hdr.m_cubProtoBufExtHdr;
    if (body_offset > message.size())
        return false;

    CMsgSOCacheSubscribed protomsg;
    if (!protomsg.ParseFromArray(message.data() + body_offset, static_cast<int>(message.size() - body_offset)))
        return false;

    protomsg.clear_owner();
    CMsgSOIDOwner *owner_soid = protomsg.mutable_owner_soid();
    owner_soid->set_type(3u);
    owner_soid->set_id(lobby_id);

    std::string updated = message.substr(0, body_offset);
    protomsg.AppendToString(&updated);
    message.swap(updated);
    return true;
}

static bool GBE_ForceDotaLobbyUpdateOwnerSOID(std::string &message, uint64 lobby_id)
{
    if (message.size() < sizeof(ProtoBufMsgHeader_t))
        return false;

    ProtoBufMsgHeader_t hdr{};
    std::memcpy(&hdr, message.data(), sizeof(hdr));
    const size_t body_offset = sizeof(hdr) + hdr.m_cubProtoBufExtHdr;
    if (body_offset > message.size())
        return false;

    CMsgSOMultipleObjects protomsg;
    if (!protomsg.ParseFromArray(message.data() + body_offset, static_cast<int>(message.size() - body_offset)))
        return false;

    protomsg.clear_owner();
    CMsgSOIDOwner *owner_soid = protomsg.mutable_owner_soid();
    owner_soid->set_type(3u);
    owner_soid->set_id(lobby_id);

    std::string updated = message.substr(0, body_offset);
    protomsg.AppendToString(&updated);
    message.swap(updated);
    return true;
}

static void GBE_LogDotaSOMultipleObjectsSummary(const char *tag, const char *label, const std::string &message)
{
    if (message.size() < sizeof(ProtoBufMsgHeader_t))
        return;

    ProtoBufMsgHeader_t hdr{};
    std::memcpy(&hdr, message.data(), sizeof(hdr));
    const size_t body_offset = sizeof(hdr) + hdr.m_cubProtoBufExtHdr;
    if (body_offset > message.size())
        return;

    CMsgSOMultipleObjects protomsg;
    if (!protomsg.ParseFromArray(message.data() + body_offset, static_cast<int>(message.size() - body_offset)))
        return;

    GBE_GC_DebugLog(
        tag,
        "%s owner_type=%u owner_id=%llu objects=%d",
        label ? label : "dota_so_summary",
        protomsg.has_owner_soid() ? protomsg.owner_soid().type() : 0u,
        static_cast<unsigned long long>(protomsg.has_owner_soid() ? protomsg.owner_soid().id() : 0ull),
        protomsg.objects_size()
    );

    for (int object_index = 0; object_index < protomsg.objects_size(); ++object_index) {
        const auto &object = protomsg.objects(object_index);
        GBE_GC_DebugLog(
            tag,
            "%s object[%d] type=%d object_data_size=%zu",
            label ? label : "dota_so_summary",
            object_index,
            object.type_id(),
            object.object_data().size()
        );

        if (object.type_id() == 2004) {
            const std::string &object_data = object.object_data();
            const uint8 *object_bytes = reinterpret_cast<const uint8 *>(object_data.data());
            const size_t object_size = object_data.size();
            uint64 lobby_id = 0;
            uint32 lobby_state = 0;
            std::string connect;
            uint64 server_id = 0;
            uint32 game_state = 0;
            uint64 match_id = 0;
            uint32 game_start_time = 0;
            std::string owner_state;
            const uint32 team_details_count = GBE_CountProtoRepeatedBytesField(object_data, 17u);
            const bool has_lobby_id = GBE_ExtractProtoFieldUint64(object_bytes, object_size, GBE_FindProtoField(object_bytes, object_size, 1u), lobby_id);
            const bool has_lobby_state = GBE_ExtractProtoFieldUint32(object_bytes, object_size, GBE_FindProtoField(object_bytes, object_size, 4u), lobby_state);
            const bool has_connect = GBE_ExtractProtoFieldBytes(object_bytes, object_size, GBE_FindProtoField(object_bytes, object_size, 5u), connect);
            const bool has_server_id = GBE_ExtractProtoFieldUint64(object_bytes, object_size, GBE_FindProtoField(object_bytes, object_size, 6u), server_id);
            const bool has_game_state = GBE_ExtractProtoFieldUint32(object_bytes, object_size, GBE_FindProtoField(object_bytes, object_size, 22u), game_state);
            const bool has_match_id = GBE_ExtractProtoFieldUint64(object_bytes, object_size, GBE_FindProtoField(object_bytes, object_size, 30u), match_id);
            const bool has_game_start_time = GBE_ExtractProtoFieldUint32(object_bytes, object_size, GBE_FindProtoField(object_bytes, object_size, 87u), game_start_time);
            const bool has_owner_state = GBE_ExtractProtoFieldBytes(object_bytes, object_size, GBE_FindProtoField(object_bytes, object_size, 120u), owner_state);
            GBE_GC_DebugLog(
                tag,
                "%s object[%d] type=2004 lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu game_start_time=%u connect=%s team_details=%u flags[lobby_id=%u state=%u game_state=%u match_id=%u server_id=%u start_time=%u connect=%u]",
                label ? label : "dota_so_summary",
                object_index,
                static_cast<unsigned long long>(lobby_id),
                lobby_state,
                game_state,
                static_cast<unsigned long long>(match_id),
                static_cast<unsigned long long>(server_id),
                game_start_time,
                has_connect ? connect.c_str() : "",
                team_details_count,
                has_lobby_id ? 1u : 0u,
                has_lobby_state ? 1u : 0u,
                has_game_state ? 1u : 0u,
                has_match_id ? 1u : 0u,
                has_server_id ? 1u : 0u,
                has_game_start_time ? 1u : 0u,
                has_connect ? 1u : 0u
            );
            if (has_owner_state) {
                GBE_GC_DebugLog(
                    tag,
                    "%s object[%d] type=2004 owner_state{%s}",
                    label ? label : "dota_so_summary",
                    object_index,
                    GBE_FormatDotaLobbyMemberStateSummary(owner_state).c_str()
                );
            }
            continue;
        }

        if (object.type_id() == 2016) {
            const std::string &object_data = object.object_data();
            const uint8 *object_bytes = reinterpret_cast<const uint8 *>(object_data.data());
            const size_t object_size = object_data.size();
            uint32 member_count = 0;
            std::string first_member;
            size_t offset = 0;
            while (offset < object_size) {
                uint32 field_number = 0;
                uint32 wire_type = 0;
                size_t field_offset = 0;
                size_t value_offset = 0;
                size_t value_size = 0;
                size_t field_end = 0;
                if (!GBE_ReadNextProtoField(object_bytes, object_size, offset, field_number, wire_type, field_offset, value_offset, value_size, field_end))
                    break;
                if (field_number == 1u && wire_type == 2u) {
                    ++member_count;
                    if (first_member.empty())
                        first_member.assign(object_data.data() + value_offset, value_size);
                }
                offset = field_end;
            }

            GBE_GC_DebugLog(
                tag,
                "%s object[%d] type=2016 member_count=%u",
                label ? label : "dota_so_summary",
                object_index,
                member_count
            );
            if (!first_member.empty()) {
                GBE_GC_DebugLog(
                    tag,
                    "%s object[%d] type=2016 member[0]{%s}",
                    label ? label : "dota_so_summary",
                    object_index,
                    GBE_FormatDotaServerStaticLobbyMemberSummary(first_member).c_str()
                );
            }
        }
    }
}

static void GBE_LogDotaSOCacheSubscribedSummary(const char *tag, const char *label, const std::string &message)
{
    if (message.size() < sizeof(ProtoBufMsgHeader_t))
        return;

    ProtoBufMsgHeader_t hdr{};
    std::memcpy(&hdr, message.data(), sizeof(hdr));
    const size_t body_offset = sizeof(hdr) + hdr.m_cubProtoBufExtHdr;
    if (body_offset > message.size())
        return;

    CMsgSOCacheSubscribed protomsg;
    if (!protomsg.ParseFromArray(message.data() + body_offset, static_cast<int>(message.size() - body_offset)))
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
                const uint32 team_details_count = GBE_CountProtoRepeatedBytesField(object_data, 17u);
                const bool has_connect = GBE_ExtractProtoFieldBytes(object_bytes, object_size, GBE_FindProtoField(object_bytes, object_size, 5u), connect);
                GBE_ExtractProtoFieldUint64(object_bytes, object_size, GBE_FindProtoField(object_bytes, object_size, 1u), lobby_id);
                GBE_ExtractProtoFieldUint32(object_bytes, object_size, GBE_FindProtoField(object_bytes, object_size, 4u), lobby_state);
                GBE_ExtractProtoFieldUint64(object_bytes, object_size, GBE_FindProtoField(object_bytes, object_size, 6u), server_id);
                GBE_ExtractProtoFieldUint32(object_bytes, object_size, GBE_FindProtoField(object_bytes, object_size, 22u), game_state);
                GBE_ExtractProtoFieldUint64(object_bytes, object_size, GBE_FindProtoField(object_bytes, object_size, 30u), match_id);
                GBE_ExtractProtoFieldUint32(object_bytes, object_size, GBE_FindProtoField(object_bytes, object_size, 87u), game_start_time);
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
                continue;
            }

            if (object.type_id() == 2014) {
                const uint32 member_count = GBE_CountProtoRepeatedBytesField(object_data, 1u);
                std::string first_member;
                if (!GBE_ExtractProtoFieldBytes(
                        reinterpret_cast<const uint8 *>(object_data.data()),
                        object_data.size(),
                        GBE_FindProtoField(reinterpret_cast<const uint8 *>(object_data.data()), object_data.size(), 1u),
                        first_member)) {
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
                        GBE_FormatDotaStaticLobbyMemberSummary(first_member).c_str()
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
                    uint32 field_number = 0;
                    uint32 wire_type = 0;
                    size_t field_offset = 0;
                    size_t value_offset = 0;
                    size_t value_size = 0;
                    size_t field_end = 0;
                    if (!GBE_ReadNextProtoField(object_bytes, object_size, offset, field_number, wire_type, field_offset, value_offset, value_size, field_end))
                        break;
                    if (field_number == 1u && wire_type == 2u) {
                        ++member_count;
                        if (first_member.empty())
                            first_member.assign(object_data.data() + value_offset, value_size);
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
                        GBE_FormatDotaServerStaticLobbyMemberSummary(first_member).c_str()
                    );
                }
            }
        }
    }
}

static bool GBE_RewriteDotaLobbyTemplateObject2004(
    const std::string &input,
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
    std::string &output)
{
    output.clear();
    bool saw_lobby_id = false;
    bool saw_state = false;
    bool saw_connect = false;
    bool saw_server_id = false;
    bool saw_game_state = false;
    bool saw_match_id = false;
    bool saw_game_start_time = false;
    bool saw_lan = false;
    bool saw_lan_host_ping_location = false;
    bool saw_room_name = false;

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
            saw_lobby_id = true;
            if (rewrite_runtime_fields) {
                GBE_AppendProtoVarIntField(output, 1u, lobby_id);
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

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
                GBE_AppendProtoBytesField(output, 5u, GBE_NormalizeDotaPracticeLobbyConnect(connect));
            } else {
                output.append(input.data() + field_offset, field_end - field_offset);
            }
            continue;
        }

        if (field_number == 6u && wire_type == 1u) {
            saw_server_id = true;
            if (rewrite_runtime_fields) {
                if (server_id != 0)
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

        if (field_number == 16u && wire_type == 2u) {
            saw_room_name = true;
            GBE_AppendProtoBytesField(output, 16u, room_name);
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
                        owner_hero_id,
                        false,
                        rewritten_member))
                    return false;
            GBE_AppendProtoBytesField(output, 120u, rewritten_member);
            continue;
        }

        if (field_number == 121u && wire_type == 0u) {
            output.append(input.data() + field_offset, field_end - field_offset);
            continue;
        }

        if ((field_number == 122u || field_number == 123u) && wire_type == 0u) {
            output.append(input.data() + field_offset, field_end - field_offset);
            continue;
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    if (rewrite_runtime_fields && !saw_lobby_id && lobby_id != 0)
        GBE_AppendProtoVarIntField(output, 1u, lobby_id);
    if (rewrite_runtime_fields && !saw_state)
        GBE_AppendProtoVarIntField(output, 4u, lobby_state);
    if (rewrite_runtime_fields && !saw_connect && !connect.empty())
        GBE_AppendProtoBytesField(output, 5u, GBE_NormalizeDotaPracticeLobbyConnect(connect));
    if (rewrite_runtime_fields && !saw_server_id && server_id != 0)
        GBE_AppendProtoFixed64Field(output, 6u, server_id);
    if (!saw_lan)
        GBE_AppendProtoVarIntField(output, 57u, lan ? 1u : 0u);
    if (!saw_room_name)
        GBE_AppendProtoBytesField(output, 16u, room_name);
    if (rewrite_runtime_fields && !saw_game_state)
        GBE_AppendProtoVarIntField(output, 22u, lobby_game_state);
    if (rewrite_runtime_fields && !saw_match_id && match_id != 0)
        GBE_AppendProtoVarIntField(output, 30u, match_id);
    if (!saw_lan_host_ping_location && !lan_host_ping_location.empty())
        GBE_AppendProtoBytesField(output, 109u, lan_host_ping_location);
    if (rewrite_runtime_fields && !saw_game_start_time && game_start_time != 0)
        GBE_AppendProtoVarIntField(output, 87u, game_start_time);

    GBE_GC_DebugLog(
        "GC_DOTA_PATCH",
        "2004 aux field summary input={%s} output={%s}",
        GBE_FormatDotaLobbyAuxFieldSummary(input).c_str(),
        GBE_FormatDotaLobbyAuxFieldSummary(output).c_str()
    );

    return true;
}

static bool GBE_RewriteDotaLobbyTemplateObject2015(
    const std::string &input,
    bool clear_existing_startup_data,
    uint32 extra_startup_account_id,
    std::string &output)
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

        if (clear_existing_startup_data && field_number == 2u && wire_type == 2u) {
            uint64 startup_type = 0;
            if (GBE_ExtractProtoFieldUint64(
                    reinterpret_cast<const uint8 *>(input.data()) + value_offset,
                    value_size,
                    GBE_FindProtoField(reinterpret_cast<const uint8 *>(input.data()) + value_offset, value_size, 1u),
                    startup_type)
                && startup_type == GBE_kDotaLobbyAdditionalAccountData)
                continue;
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    return GBE_AppendDotaLobbyAdditionalStartupAccountMessage(output, extra_startup_account_id);
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
    uint32 owner_hero_id,
    bool force_connected_leaver_state,
    std::string &output)
{
    (void)owner_team;
    (void)owner_slot;
    (void)owner_hero_id;
    (void)force_connected_leaver_state;

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
            if (!GBE_RewriteDotaServerStaticLobbyMemberObject(
                    std::string(input.data() + value_offset, value_size),
                    account_id,
                    steam_id,
                    rewritten_member))
                return false;

            GBE_GC_DebugLog(
                "GC_DOTA_PATCH",
                "2016 server static member rewrite synced steam_id input_layout=%s output_layout=%s input_summary={%s} output_summary={%s}",
                GBE_FormatProtoFieldLayoutSummary(std::string(input.data() + value_offset, value_size)).c_str(),
                GBE_FormatProtoFieldLayoutSummary(rewritten_member).c_str(),
                GBE_FormatDotaServerStaticLobbyMemberSummary(std::string(input.data() + value_offset, value_size)).c_str(),
                GBE_FormatDotaServerStaticLobbyMemberSummary(rewritten_member).c_str()
            );

            GBE_AppendProtoBytesField(output, 1u, rewritten_member);
            continue;
        }

        output.append(input.data() + field_offset, field_end - field_offset);
    }

    return true;
}

static uint32 GBE_GetDotaPracticeLobbyStartupAccountIdForState(uint32 account_id, uint32 lobby_state, uint32 lobby_game_state)
{
    if (account_id == 0)
        return 0;

    if (lobby_state == 1u && lobby_game_state == 0u)
        return account_id;

    if (lobby_state == 2u && lobby_game_state == 0u)
        return account_id;

    return 0;
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
    const std::string &pass_key)
{
    (void)account_id;

    if (message.size() < sizeof(ProtoBufMsgHeader_t))
        return false;

    ProtoBufMsgHeader_t hdr{};
    std::memcpy(&hdr, message.data(), sizeof(hdr));
    const size_t body_offset = sizeof(hdr) + hdr.m_cubProtoBufExtHdr;
    if (body_offset > message.size())
        return false;

    const std::string body = message.substr(body_offset);
    std::string rewritten_body;
    const uint32 scratch_startup_account_id = rewrite_2015 ? extra_startup_account_id : 0u;

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
        owner_hero_id,
        members,
        false,
        0u,
        std::string(),
        std::string(),
        std::string(),
        pass_key,
        scratch_startup_account_id,
        object_2015,
        object_2016,
        object_2004,
        object_2014);

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
            ? (type_id == 2004u || type_id == 2014u || type_id == 2015u || type_id == 2016u)
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
                switch (type_id) {
                case 2004u:
                    rewritten_object = object_2004;
                    break;
                case 2014u:
                    rewritten_object = object_2014;
                    break;
                case 2015u:
                    rewritten_object = object_2015;
                    break;
                case 2016u:
                    rewritten_object = object_2016;
                    break;
                default:
                    rewritten_object.assign(subscribed.data() + subscribed_value_offset, subscribed_value_size);
                    break;
                }
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

static uint64 GBE_ParseUint64OrZero(const char *text)
{
    if (!text || !*text)
        return 0;

    char *end = nullptr;
    const unsigned long long value = std::strtoull(text, &end, 10);
    if (!end || *end != '\0')
        return 0;
    return static_cast<uint64>(value);
}

static uint32 GBE_ParseUint32OrZero(const char *text)
{
    const uint64 value = GBE_ParseUint64OrZero(text);
    if (value > UINT32_MAX)
        return 0;
    return static_cast<uint32>(value);
}

static uint64 GBE_GenerateDotaChatChannelId()
{
    std::random_device device;
    std::mt19937_64 generator(
        (static_cast<uint64>(device()) << 32) ^
        static_cast<uint64>(std::chrono::high_resolution_clock::now().time_since_epoch().count())
    );

    const uint64 candidate = 0x62E000ull + (generator() & 0x0000000000000FFFull);
    return candidate != 0 ? candidate : 0x62E638ull;
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

static uint64 GBE_GenerateDotaSOChangeVersion(uint64 lobby_id, uint64 owner_id)
{
    static std::atomic<uint64> so_change_sequence{0};

    const uint64 sequence = so_change_sequence.fetch_add(1, std::memory_order_relaxed) + 1u;
    const uint64 owner_component = owner_id & 0xFFull;
    const uint64 version = lobby_id + 2781515ull + (sequence << 16) + owner_component;
    return version != 0 ? version : sequence;
}

static uint64 GBE_GenerateDotaLobbyInviteGid(uint64 lobby_id, uint64 invitee_steam_id, uint64 &cache_version)
{
    cache_version = GBE_GenerateDotaSOChangeVersion(lobby_id, invitee_steam_id);
    return cache_version > 2ull ? cache_version - 2ull : cache_version;
}

static bool GBE_ExtractWrappedClientFromGCPayload(
    const std::string &wrapped_message,
    uint32 expected_inner_emsg,
    std::string &inner_payload)
{
    inner_payload.clear();
    if (wrapped_message.size() < 8u)
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
    const GBE_ProtoFieldView payload_field = GBE_FindProtoField(outer_body, outer_body_size, 3u);
    if (!payload_field.found || payload_field.wire_type != 2u || payload_field.value_size < 8u)
        return false;

    inner_payload.assign(
        reinterpret_cast<const char *>(outer_body + payload_field.value_offset),
        payload_field.value_size
    );

    uint32 inner_raw_emsg = 0;
    std::memcpy(&inner_raw_emsg, inner_payload.data(), sizeof(inner_raw_emsg));
    return GBE_GC_MaskedEMsg(inner_raw_emsg) == expected_inner_emsg;
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

    std::vector<uint8> encoded_lobby_id;
    if (!GBE_EncodeVarUint64WithExpectedSize(lobby_id, GBE_kOldDotaPracticeLobbyLobbyIdVarint.size(), encoded_lobby_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Launch lobby_id size mismatch skipped stage=%s lobby_id=%llu; donor varint width differs",
            stage_note ? stage_note : "",
            static_cast<unsigned long long>(lobby_id)
        );
    } else if (!GBE_FindAndOverwriteBytes(message, GBE_VectorFromBytes(GBE_kOldDotaPracticeLobbyLobbyIdVarint.data(), GBE_kOldDotaPracticeLobbyLobbyIdVarint.size()), encoded_lobby_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Launch lobby_id patch skipped stage=%s lobby_id=%llu; donor does not expose expected template bytes",
            stage_note ? stage_note : "",
            static_cast<unsigned long long>(lobby_id)
        );
    }

    std::vector<uint8> encoded_match_id;
    if (!GBE_EncodeVarUint64WithExpectedSize(match_id, GBE_kOldDotaPracticeLobbyMatchIdVarint.size(), encoded_match_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Launch match_id size mismatch skipped stage=%s match_id=%llu; donor varint width differs",
            stage_note ? stage_note : "",
            static_cast<unsigned long long>(match_id)
        );
    } else if (!GBE_FindAndOverwriteBytes(message, GBE_VectorFromBytes(GBE_kOldDotaPracticeLobbyMatchIdVarint.data(), GBE_kOldDotaPracticeLobbyMatchIdVarint.size()), encoded_match_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Launch match_id patch skipped stage=%s match_id=%llu; donor does not expose expected template bytes",
            stage_note ? stage_note : "",
            static_cast<unsigned long long>(match_id)
        );
    }

    if (patch_server_id) {
        std::string server_id_raw;
        GBE_AppendLittleEndian64(server_id_raw, server_id);
        if (!GBE_FindAndOverwriteBytes(
                message,
                GBE_VectorFromBytes(GBE_kOldDotaPracticeLobbyServerIdFixed64.data(), GBE_kOldDotaPracticeLobbyServerIdFixed64.size()),
                GBE_VectorFromBytes(reinterpret_cast<const uint8 *>(server_id_raw.data()), server_id_raw.size()))) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Launch server_id patch skipped stage=%s server_id=%llu; donor does not expose expected template bytes",
                stage_note ? stage_note : "",
                static_cast<unsigned long long>(server_id)
            );
        }
    }

    if (patch_game_start_time) {
        std::vector<uint8> encoded_game_start_time;
        if (!GBE_EncodeVarUint64WithExpectedSize(game_start_time, GBE_kOldDotaPracticeLobbyGameStartTimeVarint.size(), encoded_game_start_time)) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Launch game_start_time size mismatch skipped stage=%s game_start_time=%u; donor varint width differs",
                stage_note ? stage_note : "",
                game_start_time
            );
        } else if (!GBE_FindAndOverwriteBytes(
                message,
                GBE_VectorFromBytes(GBE_kOldDotaPracticeLobbyGameStartTimeVarint.data(), GBE_kOldDotaPracticeLobbyGameStartTimeVarint.size()),
                encoded_game_start_time)) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Launch game_start_time patch skipped stage=%s game_start_time=%u; donor does not expose expected template bytes",
                stage_note ? stage_note : "",
                game_start_time
            );
        }
    }

    if (patch_connect) {
        if (connect.size() == std::strlen(GBE_kOldDotaPracticeLobbyConnect)) {
            if (!GBE_FindAndOverwriteString(message, GBE_kOldDotaPracticeLobbyConnect, connect)) {
                GBE_GC_DebugLog(
                    "GC_DOTA_LOBBY",
                    "[LOBBY] Launch connect patch skipped stage=%s connect=%s; donor does not expose expected template string",
                    stage_note ? stage_note : "",
                    connect.c_str()
                );
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

static bool GBE_BuildDotaPracticeLobbyOfficial26ReplayPayload(
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
    std::string &message)
{
    if (!wrapped_template_hex)
        return false;

    std::string wrapped_message;
    if (!GBE_DecodeHexString(wrapped_template_hex, wrapped_message))
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
            pass_key))
        return false;

    message.swap(inner_payload);
    return GBE_ForceDotaLobbyUpdateOwnerSOID(message, lobby_id);
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
    const std::vector<uint8> steam_id_fixed64_replacement = GBE_VectorFromBytes(reinterpret_cast<const uint8 *>(steam_id_fixed64_raw.data()), steam_id_fixed64_raw.size());
    const bool patched_steam_id =
        GBE_FindAndOverwriteBytes(
            message,
            GBE_VectorFromBytes(GBE_kOldDotaSteamIdFixed64.data(), GBE_kOldDotaSteamIdFixed64.size()),
            steam_id_fixed64_replacement) ||
        GBE_FindAndOverwriteBytes(
            message,
            GBE_VectorFromBytes(GBE_kOldDotaPersonaSteamIdFixed64.data(), GBE_kOldDotaPersonaSteamIdFixed64.size()),
            steam_id_fixed64_replacement);
    if (!patched_steam_id)
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

    const std::string new_lobby_id_text = std::to_string(lobby_id);
    const std::array<std::string, 2> old_lobby_id_texts = {
        GBE_kOldDotaPracticeLobbyLobbyIdText,
        GBE_kOldDotaPracticeLobbyLobbyIdTextAlt,
    };
    for (const std::string &old_lobby_id_text : old_lobby_id_texts) {
        if (old_lobby_id_text.size() == new_lobby_id_text.size())
            GBE_FindAndOverwriteString(message, old_lobby_id_text, new_lobby_id_text);
    }

    return true;
}

static bool GBE_BuildDotaPersonaStatePeripheralMessage(
    const char *template_hex,
    uint64 steam_id,
    uint64 lobby_id,
    std::string &message)
{
    return GBE_BuildDotaPracticeLobbyLaunchPeripheralMessage(template_hex, steam_id, lobby_id, 0u, false, message);
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

static std::string GBE_BuildDotaPracticeLobbyListEntryBody(
    uint64 lobby_id,
    uint32 account_id,
    const std::string &player_name,
    const std::string &room_name,
    uint32 game_mode,
    uint32 server_region,
    bool requires_pass_key,
    uint32 player_count,
    uint32 max_player_count,
    const std::string &lan_host_ping_location)
{
    std::string member;
    GBE_AppendProtoVarIntField(member, 1u, account_id);
    GBE_AppendProtoBytesField(member, 2u, player_name);

    std::string entry;
    GBE_AppendProtoVarIntField(entry, 1u, lobby_id);
    GBE_AppendProtoBytesField(entry, 5u, member);
    GBE_AppendProtoVarIntField(entry, 6u, requires_pass_key ? 1u : 0u);
    GBE_AppendProtoVarIntField(entry, 7u, account_id);
    GBE_AppendProtoBytesField(entry, 10u, room_name.empty() ? std::string("Lobby") : room_name);
    GBE_AppendProtoVarIntField(entry, 12u, game_mode);
    GBE_AppendProtoVarIntField(entry, 13u, 1u);
    GBE_AppendProtoVarIntField(entry, 14u, player_count == 0u ? 1u : player_count);
    GBE_AppendProtoVarIntField(entry, 16u, max_player_count == 0u ? 10u : max_player_count);
    GBE_AppendProtoVarIntField(entry, 17u, server_region);
    if (!lan_host_ping_location.empty())
        GBE_AppendProtoBytesField(entry, 20u, lan_host_ping_location);
    GBE_AppendProtoVarIntField(entry, 21u, 1u);
    GBE_AppendProtoVarIntField(entry, 22u, 0u);
    return entry;
}

static bool GBE_BuildDotaLobbyListResponsePayload(const std::string &entry, bool include_entry, std::string &message)
{
    std::string body;
    GBE_AppendProtoFixed64Field(body, 11u, UINT64_MAX);
    if (include_entry)
        GBE_AppendProtoBytesField(body, 1u, entry);
    return GBE_BuildDotaZeroHeaderPayload(GBE_kDotaLobbyListResponse, body, message);
}

static bool GBE_BuildDotaLobbyListResponsePayload(const std::vector<std::string> &entries, std::string &message)
{
    std::string body;
    GBE_AppendProtoFixed64Field(body, 11u, UINT64_MAX);
    for (const std::string &entry : entries)
        GBE_AppendProtoBytesField(body, 1u, entry);
    return GBE_BuildDotaZeroHeaderPayload(GBE_kDotaLobbyListResponse, body, message);
}

static bool GBE_BuildDotaFriendPracticeLobbyListResponsePayload(const std::string &entry, bool include_entry, std::string &message)
{
    std::string body;
    if (include_entry)
        GBE_AppendProtoBytesField(body, 1u, entry);
    return GBE_BuildDotaZeroHeaderPayload(GBE_kDotaFriendPracticeLobbyListResponse, body, message);
}

static bool GBE_BuildDotaFriendPracticeLobbyListResponsePayload(const std::vector<std::string> &entries, std::string &message)
{
    std::string body;
    for (const std::string &entry : entries)
        GBE_AppendProtoBytesField(body, 1u, entry);
    return GBE_BuildDotaZeroHeaderPayload(GBE_kDotaFriendPracticeLobbyListResponse, body, message);
}

static bool GBE_BuildDotaPracticeLobbyJoinResponsePayload(bool has_request_job, uint64 request_job_id, uint32 result, std::string &message)
{
    std::string body;
    GBE_AppendProtoVarIntField(body, 1u, result);
    return GBE_BuildDotaJobReplyOrZeroHeaderPayload(GBE_kDotaPracticeLobbyJoinResponse, has_request_job, request_job_id, body, message);
}

static bool GBE_BuildDotaInvitationCreatedPayload(uint64 group_id, uint64 steam_id, bool user_offline, std::string &message)
{
    std::string body;
    if (group_id != 0)
        GBE_AppendProtoVarIntField(body, 1u, group_id);
    if (steam_id != 0)
        GBE_AppendProtoFixed64Field(body, 2u, steam_id);
    GBE_AppendProtoVarIntField(body, 3u, user_offline ? 1u : 0u);
    return GBE_BuildDotaZeroHeaderPayload(GBE_kGCInvitationCreated, body, message);
}

static bool GBE_BuildDotaLobbyInviteCacheSubscribedPayload(
    uint64 lobby_id,
    uint64 inviter_steam_id,
    uint64 invitee_steam_id,
    const std::string &inviter_name,
    const std::vector<std::pair<uint64, std::string>> &members,
    std::string &message)
{
    if (lobby_id == 0 || inviter_steam_id == 0 || invitee_steam_id == 0)
        return false;

    std::string invite_object;
    GBE_AppendProtoVarIntField(invite_object, 1u, lobby_id);
    GBE_AppendProtoFixed64Field(invite_object, 2u, inviter_steam_id);
    GBE_AppendProtoBytesField(invite_object, 3u, inviter_name.empty() ? std::string("Lobby Host") : inviter_name);

    bool added_member = false;
    for (const auto &member : members) {
        if (member.first == 0)
            continue;
        std::string member_object;
        std::string member_name = member.second;
        if (member_name.empty() && member.first == inviter_steam_id)
            member_name = inviter_name;
        GBE_AppendProtoBytesField(member_object, 1u, member_name.empty() ? std::string("Lobby Host") : member_name);
        GBE_AppendProtoFixed64Field(member_object, 2u, member.first);
        GBE_AppendProtoBytesField(invite_object, 4u, member_object);
        added_member = true;
    }
    if (!added_member) {
        std::string member_object;
        GBE_AppendProtoBytesField(member_object, 1u, inviter_name.empty() ? std::string("Lobby Host") : inviter_name);
        GBE_AppendProtoFixed64Field(member_object, 2u, inviter_steam_id);
        GBE_AppendProtoBytesField(invite_object, 4u, member_object);
    }

    uint64 cache_version = 0;
    const uint64 invite_gid = GBE_GenerateDotaLobbyInviteGid(lobby_id, invitee_steam_id, cache_version);
    GBE_AppendProtoVarIntField(invite_object, 5u, 0u);
    GBE_AppendProtoFixed64Field(invite_object, 6u, invite_gid);
    GBE_AppendProtoFixed64Field(invite_object, 7u, 0u);
    GBE_AppendProtoFixed32Field(invite_object, 8u, 0u);

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Built 2011 lobby invite lobby_id=%llu invitee=%llu inviter=%llu invite_gid=%llu cache_version=%llu members=%zu",
        static_cast<unsigned long long>(lobby_id),
        static_cast<unsigned long long>(invitee_steam_id),
        static_cast<unsigned long long>(inviter_steam_id),
        static_cast<unsigned long long>(invite_gid),
        static_cast<unsigned long long>(cache_version),
        members.size());

    message.clear();
    {
        ProtoBufMsgHeader_t hdr{};
        hdr.m_EMsgFlagged = GBE_kDotaCacheSubscribed | GBE_kProtoMask;
        hdr.m_cubProtoBufExtHdr = 0;
        ser_var<ProtoBufMsgHeader_t>(message, hdr);
    }

    std::string owner_soid;
    GBE_AppendProtoVarIntField(owner_soid, 1u, 4u);
    GBE_AppendProtoVarIntField(owner_soid, 2u, invitee_steam_id);

    std::string invite_entry;
    GBE_AppendProtoVarIntField(invite_entry, 1u, 2011u);
    GBE_AppendProtoBytesField(invite_entry, 2u, invite_object);

    std::string body;
    GBE_AppendProtoBytesField(body, 2u, invite_entry);
    GBE_AppendProtoFixed64Field(body, 3u, cache_version);
    GBE_AppendProtoBytesField(body, 4u, owner_soid);
    message.append(body);
    return true;
}

static bool GBE_IsDotaLobbyInviteCacheSubscribedPayload(const std::string &message)
{
    if (message.size() < sizeof(ProtoBufMsgHeader_t))
        return false;

    ProtoBufMsgHeader_t hdr{};
    std::memcpy(&hdr, message.data(), sizeof(hdr));
    if (GBE_GC_MaskedEMsg(hdr.m_EMsgFlagged) != GBE_kDotaCacheSubscribed)
        return false;

    const size_t body_offset = sizeof(ProtoBufMsgHeader_t) + hdr.m_cubProtoBufExtHdr;
    if (body_offset > message.size())
        return false;

    CMsgSOCacheSubscribed protomsg;
    if (!protomsg.ParseFromArray(message.data() + body_offset, static_cast<int>(message.size() - body_offset)))
        return false;

    for (int object_index = 0; object_index < protomsg.objects_size(); ++object_index) {
        const auto &object = protomsg.objects(object_index);
        if (object.type_id() == 2011 && object.object_data_size() > 0)
            return true;
    }

    return false;
}

static bool GBE_BuildDotaOtherJoinedChannelPayload(uint64 channel_id, const std::string &persona_name, uint64 steam_id, std::string &message)
{
    std::string body;
    GBE_AppendProtoFixed64Field(body, 1u, channel_id);
    if (!persona_name.empty())
        GBE_AppendProtoBytesField(body, 2u, persona_name);
    GBE_AppendProtoFixed64Field(body, 3u, steam_id);
    GBE_AppendProtoVarIntField(body, 5u, 0u);
    return GBE_BuildDotaZeroHeaderPayload(GBE_kDotaOtherJoinedChannel, body, message);
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

static bool GBE_BuildDotaGameMatchSignOutPermissionResponsePayload(bool has_request_job, uint64 request_job_id, std::string &message)
{
    std::string body;
    GBE_AppendProtoVarIntField(body, 1u, 1u);
    return GBE_BuildDotaJobReplyOrZeroHeaderPayload(GBE_kDotaGameMatchSignOutPermissionResponse, has_request_job, request_job_id, body, message);
}

static bool GBE_BuildDotaGameMatchSignOutResponsePayload(uint64 match_id, uint32 duration, bool has_request_job, uint64 request_job_id, std::string &message)
{
    std::string body;
    if (match_id != 0)
        GBE_AppendProtoVarIntField(body, 1u, match_id);
    GBE_AppendProtoFixed32Field(body, 2u, static_cast<uint32>(std::time(nullptr)));
    GBE_AppendProtoVarIntField(body, 5u, 0u);
    GBE_AppendProtoFixed32Field(body, 7u, 0u);

    std::string signout_summary;
    GBE_AppendProtoVarIntField(signout_summary, 1u, duration);
    GBE_AppendProtoBytesField(body, 8u, signout_summary);

    std::string zero_summary;
    GBE_AppendProtoVarIntField(zero_summary, 1u, 0u);
    GBE_AppendProtoBytesField(body, 9u, zero_summary);
    GBE_AppendProtoBytesField(body, 10u, zero_summary);
    GBE_AppendProtoBytesField(body, 14u, zero_summary);
    GBE_AppendProtoBytesField(body, 15u, zero_summary);
    return GBE_BuildDotaJobReplyOrZeroHeaderPayload(GBE_kDotaGameMatchSignOutResponse, has_request_job, request_job_id, body, message);
}

static bool GBE_BuildDotaSubmitPlayerReportResponseV2Payload(const uint8 *request_body, size_t request_body_size, bool has_request_job, uint64 request_job_id, std::string &message)
{
    uint64 target_account_id = 0;
    GBE_ExtractProtoFieldUint64(request_body, request_body_size, GBE_FindProtoField(request_body, request_body_size, 1u), target_account_id);

    std::string body;
    if (target_account_id != 0)
        GBE_AppendProtoVarIntField(body, 1u, target_account_id);

    size_t offset = 0;
    while (offset < request_body_size) {
        uint32 field_number = 0;
        uint32 wire_type = 0;
        size_t field_offset = 0;
        size_t value_offset = 0;
        size_t value_size = 0;
        size_t field_end = 0;
        if (!GBE_ReadNextProtoField(request_body, request_body_size, offset, field_number, wire_type, field_offset, value_offset, value_size, field_end))
            break;
        if (field_number == 2u && wire_type == 0) {
            GBE_ProtoFieldView reason_field{};
            reason_field.found = true;
            reason_field.field_number = field_number;
            reason_field.wire_type = wire_type;
            reason_field.value_offset = value_offset;
            reason_field.value_size = value_size;
            uint64 reason = 0;
            if (GBE_ExtractProtoFieldUint64(request_body, request_body_size, reason_field, reason))
                GBE_AppendProtoVarIntField(body, 2u, reason);
        }
        offset = field_end;
    }

    GBE_AppendProtoVarIntField(body, 5u, 1u);
    return GBE_BuildDotaJobReplyOrZeroHeaderPayload(GBE_kDotaSubmitPlayerReportResponseV2, has_request_job, request_job_id, body, message);
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

static bool GBE_BuildDota2582LookupAccountNameResponsePayload(uint32 account_id, const std::string &account_name, bool has_request_job, uint64 request_job_id, std::string &message)
{
    std::string body;
    GBE_AppendProtoVarIntField(body, 1u, account_id);
    GBE_AppendProtoBytesField(body, 2u, account_name);
    return GBE_BuildDotaJobReplyPayload(2582u, has_request_job ? request_job_id : UINT64_MAX, body, message);
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
    GBE_AppendProtoVarIntField(body, 17u, GBE_kDotaConductScore);
    GBE_AppendProtoVarIntField(body, 18u, GBE_kDotaConductScore);
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
        GBE_AppendProtoVarIntField(result, 6u, 0u);
        GBE_AppendProtoVarIntField(result, 9u, GBE_kDotaBehaviorLevel);
        GBE_AppendProtoVarIntField(result, 10u, GBE_kDotaBehaviorLevel);
        GBE_AppendProtoVarIntField(result, 14u, GBE_kDotaConductScore);
        GBE_AppendProtoVarIntField(result, 15u, GBE_kDotaConductScore);
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
    const std::vector<GBE_DotaLobbyMemberState> &members,
    const GBE_Dota7034RequestShape &request_shape,
    bool has_request_job,
    uint64 request_job_id,
    std::string &message)
{
    const std::string leaver_state = GBE_BuildDota7034LeaverStatePayload(lobby_state, game_state);
    const bool include_draft = (lobby_state >= 2u && game_state >= 2u);
    std::vector<uint64> connected_steam_ids;
    std::vector<uint64> disconnected_steam_ids;

    std::string body;

    auto append_connected_player = [&](uint64 player_steam_id, uint32 hero_id, uint32 team, uint32 slot) {
        if (player_steam_id == 0ull || std::find(connected_steam_ids.begin(), connected_steam_ids.end(), player_steam_id) != connected_steam_ids.end())
            return;

        std::string player;
        GBE_AppendProtoFixed64Field(player, 1u, player_steam_id);
        if (request_shape.has_connected_steam_id && request_shape.connected_steam_id == player_steam_id && request_shape.has_connected_hero_id && request_shape.connected_hero_id != 0u)
            GBE_AppendProtoVarIntField(player, 2u, request_shape.connected_hero_id);
        else if (hero_id != 0u)
            GBE_AppendProtoVarIntField(player, 2u, hero_id);
        GBE_AppendProtoBytesField(player, 3u, leaver_state);
        GBE_AppendProtoVarIntField(player, 4u, 0u);
        GBE_AppendProtoBytesField(body, 1u, player);
        connected_steam_ids.push_back(player_steam_id);

        if (include_draft) {
            std::string draft;
            GBE_AppendProtoFixed64Field(draft, 1u, player_steam_id);
            GBE_AppendProtoVarIntField(draft, 2u, team);
            GBE_AppendProtoVarIntField(draft, 3u, slot > 0u ? (slot - 1u) : 0u);
            GBE_AppendProtoBytesField(body, 16u, draft);
        }
    };

    auto append_disconnected_player = [&](uint64 player_steam_id, uint32 disconnected_lobby_state, uint32 disconnected_game_state) {
        if (player_steam_id == 0ull || std::find(disconnected_steam_ids.begin(), disconnected_steam_ids.end(), player_steam_id) != disconnected_steam_ids.end())
            return;

        std::string disconnected_player;
        GBE_AppendProtoFixed64Field(disconnected_player, 1u, player_steam_id);
        GBE_AppendProtoBytesField(disconnected_player, 3u, GBE_BuildDota7034LeaverStatePayload(disconnected_lobby_state, disconnected_game_state));
        GBE_AppendProtoVarIntField(disconnected_player, 4u, 0u);
        GBE_AppendProtoBytesField(body, 7u, disconnected_player);
        disconnected_steam_ids.push_back(player_steam_id);
    };

    if (request_shape.has_connected_player || request_shape.has_disconnected_player) {
        if (request_shape.has_connected_steam_id && request_shape.connected_steam_id != 0ull) {
            uint32 hero_id = 0u;
            uint32 team = owner_team;
            uint32 slot = owner_slot;
            for (const GBE_DotaLobbyMemberState &member : members) {
                if (member.steam_id == request_shape.connected_steam_id) {
                    hero_id = member.hero_id;
                    team = member.team;
                    slot = member.slot;
                    break;
                }
            }
            append_connected_player(request_shape.connected_steam_id, hero_id, team, slot);
        }

        if (request_shape.has_disconnected_steam_id && request_shape.disconnected_steam_id != 0ull) {
            const uint32 disconnected_lobby_state = request_shape.has_disconnected_lobby_state ? request_shape.disconnected_lobby_state : lobby_state;
            const uint32 disconnected_game_state = request_shape.has_disconnected_game_state ? request_shape.disconnected_game_state : game_state;
            append_disconnected_player(request_shape.disconnected_steam_id, disconnected_lobby_state, disconnected_game_state);
        }
    } else {
        append_connected_player(steam_id, 0u, owner_team, owner_slot);
        for (const GBE_DotaLobbyMemberState &member : members) {
            if (member.steam_id == 0ull || member.steam_id == steam_id)
                continue;

            if (member.connected)
                append_connected_player(member.steam_id, member.hero_id, member.team, member.slot);
            else
                append_disconnected_player(member.steam_id, lobby_state, game_state);
        }
    }

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
    if (request_shape.has_disconnected_player && (!request_shape.has_disconnected_steam_id || request_shape.disconnected_steam_id == steam_id)) {
        uint32 disconnected_lobby_state = request_shape.has_disconnected_lobby_state ? request_shape.disconnected_lobby_state : lobby_state;
        uint32 disconnected_game_state = request_shape.has_disconnected_game_state ? request_shape.disconnected_game_state : game_state;
        if (disconnected_lobby_state < lobby_state)
            disconnected_lobby_state = lobby_state;
        if (disconnected_game_state < game_state)
            disconnected_game_state = game_state;

        append_disconnected_player(steam_id, disconnected_lobby_state, disconnected_game_state);
    }

    return GBE_BuildDotaJobReplyOrZeroHeaderPayload(7034u, has_request_job, request_job_id, body, message);
}

static bool GBE_BuildDotaJoinChatChannelResponsePayload(
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
    std::string body;
    GBE_AppendProtoVarIntField(body, 1u, 0u);
    GBE_AppendProtoBytesField(body, 2u, channel_name);
    GBE_AppendProtoFixed64Field(body, 3u, channel_id);
    GBE_AppendProtoVarIntField(body, 4u, 200u);

    std::vector<uint64> written_members;
    auto append_channel_member = [&](uint64 member_steam_id, const std::string &member_name) {
        if (member_steam_id == 0 || std::find(written_members.begin(), written_members.end(), member_steam_id) != written_members.end())
            return;

        const std::string effective_member_name = member_name.empty() ? std::to_string(member_steam_id) : member_name;
        std::string member;
        GBE_AppendProtoFixed64Field(member, 1u, member_steam_id);
        GBE_AppendProtoBytesField(member, 2u, effective_member_name);
        GBE_AppendProtoVarIntField(member, 3u, 0u);
        GBE_AppendProtoVarIntField(member, 4u, 0u);
        GBE_AppendProtoBytesField(body, 5u, member);
        written_members.push_back(member_steam_id);
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Added 7010 chat member channel_id=%llu steam_id=%llu persona=%s",
            static_cast<unsigned long long>(channel_id),
            static_cast<unsigned long long>(member_steam_id),
            effective_member_name.c_str()
        );
    };

    auto resolve_member_name = [&](const GBE_DotaLobbyMemberState &channel_member) -> std::string {
        if (channel_member.steam_id == steam_id)
            return player_name;
        if (channel_member.steam_id == owner_steam_id && !owner_name.empty())
            return owner_name;

        Steam_Client *steam_client = get_steam_client();
        if (steam_client && steam_client->steam_matchmaking && generic_lobby_id != 0ull) {
            CSteamID generic_lobby((uint64)generic_lobby_id);
            CSteamID member_id((uint64)channel_member.steam_id);
            if (generic_lobby.IsLobby() && member_id.IsValid()) {
                const char *generic_name = steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby, member_id, GBE_kDotaGenericLobbyMemberNameKey);
                if (generic_name && generic_name[0] != '\0')
                    return std::string(generic_name);
            }
        }

        if (steam_client && steam_client->steam_friends) {
            const char *friend_name = steam_client->steam_friends->GetFriendPersonaName(CSteamID((uint64)channel_member.steam_id));
            if (friend_name && friend_name[0] != '\0' && std::string(friend_name) != "Unknown User")
                return std::string(friend_name);
        }

        return std::to_string(channel_member.steam_id);
    };

    for (const GBE_DotaLobbyMemberState &channel_member : channel_members) {
        if (channel_member.steam_id == steam_id) {
            append_channel_member(channel_member.steam_id, player_name);
            break;
        }
    }
    append_channel_member(steam_id, player_name);

    for (const GBE_DotaLobbyMemberState &channel_member : channel_members) {
        if (channel_member.steam_id == steam_id)
            continue;
        append_channel_member(channel_member.steam_id, resolve_member_name(channel_member));
    }

    GBE_AppendProtoVarIntField(body, 6u, channel_type);
    GBE_AppendProtoVarIntField(body, 7u, 0u);
    GBE_AppendProtoVarIntField(body, 9u, 0u);
    GBE_AppendProtoVarIntField(body, 11u, 0u);
    return GBE_BuildDotaZeroHeaderPayload(GBE_kDotaJoinChatChannelResponse, body, message);
}

static bool GBE_BuildDotaPostGameJoinChatChannelResponsePayload(
    uint64 steam_id,
    uint64 channel_id,
    const std::string &channel_name,
    const std::string &player_name,
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

    GBE_AppendProtoVarIntField(body, 6u, 18u);
    GBE_AppendProtoVarIntField(body, 7u, 0u);
    GBE_AppendProtoVarIntField(body, 8u, 1u);
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

static bool GBE_BuildDotaChatMessagePayload(
    const std::string &request_body,
    const GBE_DotaChatMessageRequest &request,
    uint64 channel_id,
    uint32 account_id,
    const std::string &persona_name,
    std::string &message)
{
    (void)request;
    std::string body;
    bool saw_channel_id = false;
    bool saw_persona_name = false;

    GBE_AppendProtoVarIntField(body, 1u, account_id);

    size_t offset = 0;
    while (offset < request_body.size()) {
        uint32 field_number = 0;
        uint32 wire_type = 0;
        size_t field_offset = 0;
        size_t value_offset = 0;
        size_t value_size = 0;
        size_t field_end = 0;
        if (!GBE_ReadNextProtoField(
                reinterpret_cast<const uint8 *>(request_body.data()),
                request_body.size(),
                offset,
                field_number,
                wire_type,
                field_offset,
                value_offset,
                value_size,
                field_end))
            return false;

        if (field_number == 1u) {
            continue;
        }

        if (field_number == 2u && wire_type == 0u) {
            saw_channel_id = true;
            GBE_AppendProtoVarIntField(body, 2u, channel_id);
            continue;
        }

        if (field_number == 3u) {
            saw_persona_name = true;
            if (!persona_name.empty())
                GBE_AppendProtoBytesField(body, 3u, persona_name);
            else
                body.append(request_body.data() + field_offset, field_end - field_offset);
            continue;
        }

        body.append(request_body.data() + field_offset, field_end - field_offset);
    }

    if (!saw_channel_id)
        GBE_AppendProtoVarIntField(body, 2u, channel_id);
    if (!saw_persona_name && !persona_name.empty())
        GBE_AppendProtoBytesField(body, 3u, persona_name);

    return GBE_BuildDotaZeroHeaderPayload(GBE_kDotaChatMessage, body, message);
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

static bool GBE_BuildDotaLobbyCacheUnsubscribedPayload(uint64 lobby_id, std::string &message)
{
    std::string owner_soid;
    GBE_AppendProtoVarIntField(owner_soid, 1u, 3u);
    GBE_AppendProtoVarIntField(owner_soid, 2u, lobby_id);

    std::string body;
    GBE_AppendProtoBytesField(body, 2u, owner_soid);
    return GBE_BuildDotaZeroHeaderPayload(GBE_kDotaCacheUnsubscribed, body, message);
}

static bool GBE_BuildDotaSOOwnerCacheUnsubscribedPayload(uint32 owner_type, uint64 owner_id, std::string &message)
{
    std::string owner_soid;
    GBE_AppendProtoVarIntField(owner_soid, 1u, owner_type);
    GBE_AppendProtoVarIntField(owner_soid, 2u, owner_id);

    std::string body;
    GBE_AppendProtoBytesField(body, 2u, owner_soid);
    return GBE_BuildDotaZeroHeaderPayload(GBE_kDotaCacheUnsubscribed, body, message);
}

static bool GBE_BuildDotaRemoveLobbyInvitePayload(uint64 lobby_id, uint64 owner_steam_id, std::string &message)
{
    std::string invite_key;
    GBE_AppendProtoVarIntField(invite_key, 1u, lobby_id);

    std::string removed_object;
    GBE_AppendProtoVarIntField(removed_object, 1u, 2011u);
    GBE_AppendProtoBytesField(removed_object, 2u, invite_key);

    std::string owner_soid;
    GBE_AppendProtoVarIntField(owner_soid, 1u, 4u);
    GBE_AppendProtoVarIntField(owner_soid, 2u, owner_steam_id);

    std::string body;
    GBE_AppendProtoFixed64Field(body, 3u, GBE_GenerateDotaSOChangeVersion(lobby_id, owner_steam_id));
    GBE_AppendProtoBytesField(body, 5u, removed_object);
    GBE_AppendProtoBytesField(body, 6u, owner_soid);
    return GBE_BuildDotaZeroHeaderPayload(GBE_kDotaPracticeLobbyDetailsUpdate, body, message);
}

static bool GBE_BuildDotaLobbyCacheSubscribedUpToDatePayload(uint64 lobby_id, std::string &message)
{
    std::string owner_soid;
    GBE_AppendProtoVarIntField(owner_soid, 1u, 3u);
    GBE_AppendProtoVarIntField(owner_soid, 2u, lobby_id);

    std::string body;
    GBE_AppendProtoBytesField(body, 2u, owner_soid);
    return GBE_BuildDotaZeroHeaderPayload(GBE_kDotaCacheSubscribedUpToDate, body, message);
}

static bool GBE_BuildDotaLobbyCacheSubscribedUpToDatePayload(
    uint64 lobby_id,
    bool has_version,
    uint64 version,
    bool has_service_id,
    uint32 service_id,
    const std::vector<uint32> &service_list,
    bool has_sync_version,
    uint64 sync_version,
    std::string &message)
{
    std::string owner_soid;
    GBE_AppendProtoVarIntField(owner_soid, 1u, 3u);
    GBE_AppendProtoVarIntField(owner_soid, 2u, lobby_id);

    std::string body;
    if (has_version)
        GBE_AppendProtoFixed64Field(body, 1u, version);
    GBE_AppendProtoBytesField(body, 2u, owner_soid);
    if (has_service_id)
        GBE_AppendProtoVarIntField(body, 3u, service_id);
    for (uint32 listed_service_id : service_list)
        GBE_AppendProtoVarIntField(body, 4u, listed_service_id);
    if (has_sync_version)
        GBE_AppendProtoFixed64Field(body, 5u, sync_version);
    return GBE_BuildDotaZeroHeaderPayload(GBE_kDotaCacheSubscribedUpToDate, body, message);
}

static bool GBE_BuildDotaPracticeLobbyKickedPopupPayload(std::string &message)
{
    std::string body;
    GBE_AppendProtoVarIntField(body, 1u, 1u);
    return GBE_BuildDotaZeroHeaderPayload(GBE_kDotaPopup, body, message);
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
    uint32 owner_hero_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
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

    const std::vector<GBE_DotaLobbyMemberState> effective_members = GBE_BuildDotaLobbyMembers(
        steam_id,
        extra_startup_account_id,
        owner_team,
        owner_slot,
        owner_hero_id,
        true,
        members);

    if (!GBE_BuildDotaServerLobbyObject2015(effective_members.size(), extra_startup_account_id, object_2015))
        object_2015.clear();

    GBE_BuildDotaServerStaticLobbyObject2016(extra_startup_account_id, steam_id, game_mode, effective_members, object_2016);

    const std::string normalized_connect = GBE_NormalizeDotaPracticeLobbyConnect(connect);

    object_2004.clear();
    GBE_AppendProtoVarIntField(object_2004, 1, lobby_id);
    GBE_AppendProtoVarIntField(object_2004, 3, game_mode);
    GBE_AppendProtoVarIntField(object_2004, 4, lobby_state);
    if (!normalized_connect.empty())
        GBE_AppendProtoBytesField(object_2004, 5, normalized_connect);
    if (server_id != 0)
        GBE_AppendProtoFixed64Field(object_2004, 6, server_id);
    GBE_AppendProtoFixed64Field(object_2004, 11, steam_id);
    GBE_AppendProtoVarIntField(object_2004, 12, 1u);
    GBE_AppendProtoVarIntField(object_2004, 13, allow_cheats ? 1u : 0u);
    GBE_AppendProtoVarIntField(object_2004, 14, fill_with_bots ? 1u : 0u);
    GBE_AppendProtoBytesField(object_2004, 16, room_name);
    if (lobby_state != 0u) {
        GBE_AppendProtoBytesField(object_2004, 17, GBE_BuildDotaLobbyTeamDetailsPayload(true));
        GBE_AppendProtoBytesField(object_2004, 17, GBE_BuildDotaLobbyTeamDetailsPayload(false));
    }
    GBE_AppendProtoVarIntField(object_2004, 21, server_region);
    if (lobby_state != 0u || lobby_game_state != 0u)
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
    if (lobby_state == 2u && lobby_game_state >= 1u)
        GBE_AppendProtoVarIntField(object_2004, 65, 0u);
    if (lobby_state == 3u && lobby_game_state == 6u)
        GBE_AppendProtoVarIntField(object_2004, 70, 2u);
    GBE_AppendProtoVarIntField(object_2004, 75, visibility);
    GBE_AppendProtoVarIntField(object_2004, 82, 0u);
    if (game_start_time != 0)
        GBE_AppendProtoVarIntField(object_2004, 87, game_start_time);
    GBE_AppendProtoVarIntField(object_2004, 88, 0u);
    GBE_AppendProtoVarIntField(object_2004, 93, bot_difficulty_dire);
    GBE_AppendProtoVarIntField(object_2004, 94, bot_radiant);
    GBE_AppendProtoVarIntField(object_2004, 95, bot_dire);
    GBE_AppendProtoVarIntField(object_2004, 97, 0u);
    if (lobby_state != 0u) {
        GBE_AppendProtoVarIntField(object_2004, 103, 0u);
        GBE_AppendProtoVarIntField(object_2004, 104, 0u);
    }
    if (!lan_host_ping_location.empty())
        GBE_AppendProtoBytesField(object_2004, 109, lan_host_ping_location);
    GBE_AppendProtoVarIntField(object_2004, 110, 0u);
    if (lobby_state == 3u && lobby_game_state == 6u && game_start_time != 0) {
        const uint32 now = static_cast<uint32>(std::time(nullptr));
        GBE_AppendProtoVarIntField(object_2004, 111, now > game_start_time ? (now - game_start_time) : 0u);
    }
    GBE_AppendProtoVarIntField(object_2004, 113, 0u);

    for (const GBE_DotaLobbyMemberState &member : effective_members) {
        std::string member_state;
        GBE_BuildDotaLobbyMemberObject2004(member, lobby_state, lobby_game_state, member_state);
        if (!member_state.empty())
            GBE_AppendProtoBytesField(object_2004, 120, member_state);
    }

    for (size_t i = 0; i < effective_members.size(); ++i) {
        if (effective_members[i].steam_id != 0ull) {
            GBE_AppendProtoVarIntField(object_2004, 121, static_cast<uint64>(i));
        } else {
            GBE_AppendProtoVarIntField(object_2004, 123, static_cast<uint64>(i));
        }
    }
    GBE_AppendProtoVarIntField(object_2004, 127, 0u);
    GBE_AppendProtoVarIntField(object_2004, 128, GBE_kDotaLobbyField128Value);

    GBE_BuildDotaStaticLobbyObject2014(player_name, effective_members, object_2014);
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
    uint32 owner_hero_id,
    const std::vector<GBE_DotaLobbyMemberState> &members,
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
        owner_hero_id,
        members,
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
    auto *owner_soid = protomsg.mutable_owner_soid();
    owner_soid->set_type(3u);
    owner_soid->set_id(lobby_id);

    auto object_2004_entry = protomsg.add_objects();
    object_2004_entry->set_type_id(2004);
    object_2004_entry->add_object_data(object_2004);

    auto object_2015_entry = protomsg.add_objects();
    object_2015_entry->set_type_id(2015);
    object_2015_entry->add_object_data(object_2015);

    auto object_2013_entry = protomsg.add_objects();
    object_2013_entry->set_type_id(2013);
    object_2013_entry->add_object_data(std::string());

    auto object_2014_entry = protomsg.add_objects();
    object_2014_entry->set_type_id(2014);
    object_2014_entry->add_object_data(object_2014);

    auto object_2016_entry = protomsg.add_objects();
    object_2016_entry->set_type_id(2016);
    object_2016_entry->add_object_data(object_2016);

    protomsg.AppendToString(&message);
    return true;
}

static bool GBE_BuildDotaPracticeLobbyDetailsUpdatePurePayload(
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
    std::string &message)
{
    if (steam_id == 0 || lobby_id == 0)
        return false;

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
        owner_hero_id,
        members,
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
        hdr.m_EMsgFlagged = GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask;
        hdr.m_cubProtoBufExtHdr = 0;
        ser_var<ProtoBufMsgHeader_t>(message, hdr);
    }

    CMsgSOMultipleObjects protomsg;
    auto *owner_soid = protomsg.mutable_owner_soid();
    owner_soid->set_type(3u);
    owner_soid->set_id(lobby_id);

    auto object_2014_entry = protomsg.add_objects();
    object_2014_entry->set_type_id(2014);
    object_2014_entry->set_object_data(object_2014);

    if (include_server_lobby_placeholder) {
        auto object_2013_entry = protomsg.add_objects();
        object_2013_entry->set_type_id(2013);
        object_2013_entry->set_object_data(std::string());
    }

    auto object_2015_entry = protomsg.add_objects();
    object_2015_entry->set_type_id(2015);
    object_2015_entry->set_object_data(object_2015);

    auto object_2004_entry = protomsg.add_objects();
    object_2004_entry->set_type_id(2004);
    object_2004_entry->set_object_data(object_2004);

    auto object_2016_entry = protomsg.add_objects();
    object_2016_entry->set_type_id(2016);
    object_2016_entry->set_object_data(object_2016);

    protomsg.AppendToString(&message);
    return true;
}

static bool GBE_BuildDotaPracticeLobbyLaunchCacheSubscribedTemplateReplayFromWrappedTemplate(
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
    std::string &message)
{
    std::string wrapped_message;
    if (!wrapped_template_hex || !GBE_DecodeHexString(wrapped_template_hex, wrapped_message)) {
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
        pass_key)) {
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

static bool GBE_BuildDotaPracticeLobbyLaunchCacheSubscribedPreludeTemplateReplay(
    uint32 account_id,
    uint64 steam_id,
    uint64 lobby_id,
    std::string &message)
{
    return GBE_BuildDotaPracticeLobbyLaunchCacheSubscribedTemplateReplayFromWrappedTemplate(
        GBE_kDotaPracticeLobbyLaunchCacheSubscribedPreludeHex,
        "practice lobby launch prelude cache template",
        false,
        false,
        false,
        false,
        account_id,
        steam_id,
        lobby_id,
        0u,
        0u,
        0ull,
        0ull,
        0u,
        std::string(),
        std::string(),
        std::string(),
        0u,
        0u,
        false,
        std::string(),
        false,
        false,
        false,
        0u,
        0u,
        0u,
        0ull,
        0ull,
        0u,
        0u,
        0u,
        std::string(),
        0u,
        message);
}

static bool GBE_BuildDotaPracticeLobbyLaunchCacheSubscribedLargePreludeTemplateReplay(
    uint32 account_id,
    uint64 steam_id,
    uint64 lobby_id,
    std::string &message)
{
    return GBE_BuildDotaPracticeLobbyLaunchCacheSubscribedTemplateReplayFromWrappedTemplate(
        GBE_kDotaPracticeLobbyLaunchCacheSubscribedLargePreludeHex,
        "practice lobby launch large prelude cache template",
        false,
        false,
        false,
        false,
        account_id,
        steam_id,
        lobby_id,
        0u,
        0u,
        0ull,
        0ull,
        0u,
        std::string(),
        std::string(),
        std::string(),
        0u,
        0u,
        false,
        std::string(),
        false,
        false,
        false,
        0u,
        0u,
        0u,
        0ull,
        0ull,
        0u,
        0u,
        0u,
        std::string(),
        0u,
        message);
}

static bool GBE_BuildDotaPracticeLobbyLaunchCacheSubscribedTemplateReplay(
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
    std::string &message)
{
    (void)extra_startup_account_id;

    return GBE_BuildDotaPracticeLobbyLaunchCacheSubscribedTemplateReplayFromWrappedTemplate(
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
        message);
}

static bool GBE_BuildDotaPracticeLobbyDetailsUpdatePayload(
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
    std::string &message)
{
    (void)has_broadcast_channel;
    (void)broadcast_channel_id;
    (void)broadcast_country_code;
    (void)broadcast_description;
    (void)broadcast_language_code;

    const uint32 startup_account_id = GBE_GetDotaPracticeLobbyStartupAccountIdForState(account_id, lobby_state, lobby_game_state);

    if (GBE_IsDotaPracticeLobbyPrelaunchState(server_id, match_id, game_start_time, connect)) {
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
            owner_hero_id,
            members,
            has_broadcast_channel,
            broadcast_channel_id,
            broadcast_country_code,
            broadcast_description,
            broadcast_language_code,
            pass_key,
            startup_account_id,
            object_2015,
            object_2016,
            object_2004,
            object_2014);

        {
            std::string update;
            GBE_AppendProtoVarIntField(update, 1, 2014u);
            GBE_AppendProtoBytesField(update, 2, object_2014);
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
            GBE_AppendProtoVarIntField(update, 1, 2015u);
            GBE_AppendProtoBytesField(update, 2, object_2015);
            GBE_AppendProtoBytesField(body, 2, update);
        }

        {
            std::string update;
            GBE_AppendProtoVarIntField(update, 1, 2013u);
            GBE_AppendProtoBytesField(update, 2, std::string());
            GBE_AppendProtoBytesField(body, 2, update);
        }

        {
            std::string update;
            GBE_AppendProtoVarIntField(update, 1, 2004u);
            GBE_AppendProtoBytesField(update, 2, object_2004);
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
        return GBE_ForceDotaLobbyUpdateOwnerSOID(message, lobby_id);
    }

    if (GBE_BuildDotaPracticeLobbyDetailsUpdatePurePayload(
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
            message)) {
        return true;
    }

    return GBE_BuildDotaPracticeLobbyOfficial26ReplayPayload(
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
        message);
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

    CMsgServerHello protomsg;
    if (!protomsg.ParseFromArray(body, static_cast<int>(body_size)) || !protomsg.has_version()) {
        GBE_GC_DebugLog("GC_DOTA_SERVER_HELLO", "direct path failed parsing CMsgServerHello body_size=%zu", body_size);
        return false;
    }

    const uint32 version = protomsg.version();

    context.valid = true;
    context.active_version = version;
    context.min_allowed_version = version;
    context.compatibility_value = 0;
    context.universe = 0;
    if (protohdr.has_client_steam_id()) {
        context.client_steam_id = protohdr.client_steam_id();
        context.has_client_steam_id = true;
    }
    if (protohdr.has_client_session_id()) {
        context.client_session_id = protohdr.client_session_id();
        context.has_client_session_id = true;
    }
    if (protohdr.has_source_app_id()) {
        context.source_app_id = protohdr.source_app_id();
        context.has_source_app_id = true;
    }
    if (protohdr.has_job_id_source()) {
        context.source_job_id = protohdr.job_id_source();
        context.has_source_job = true;
    }
    if (protohdr.has_gc_msg_src()) {
        context.gc_msg_src = static_cast<uint32>(protohdr.gc_msg_src());
        context.has_gc_msg_src = true;
    }
    if (protohdr.has_gc_dir_index_source()) {
        context.gc_dir_index_source = protohdr.gc_dir_index_source();
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

    GBE_local_lobby.state = message.lobby_state;
    GBE_local_lobby.game_state = message.lobby_game_state;

    if (GBE_local_lobby.state == 1u && GBE_local_lobby.game_state == 0u && GBE_HasDotaLaunchServerSetupSync()) {
        if (GBE_local_lobby.launch_phase < GBE_kDotaLaunchPhaseSetupSynced)
            GBE_local_lobby.launch_phase = GBE_kDotaLaunchPhaseSetupSynced;
    } else if (GBE_local_lobby.state == 2u && GBE_local_lobby.game_state == 0u) {
        if (GBE_local_lobby.launch_phase < GBE_kDotaLaunchPhaseRunQueued)
            GBE_local_lobby.launch_phase = GBE_kDotaLaunchPhaseRunQueued;
    }

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
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "skipping generic CacheSubscribed for active dota owner steam_id=%llu lobby_id=%llu state=%u game_state=%u launch_phase=%s items=%zu",
                static_cast<unsigned long long>(steam_id.ConvertToUint64()),
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state,
                GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase),
                items.size()
            );
            return;
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
    GBE_dota_private_lobby_snapshot_replayed = false;
    GBE_last_dota_launch_state_pushed_game_state = 0;
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
        if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && disconnected_steam_id != 0 && disconnected_steam_id == owner_steam_id) {
            GBE_local_lobby.owner_connected = false;
            GBE_PublishSharedDotaLobbyState("owner_disconnected");

            suppress_user_item_unsubscribe = true;
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "preserving owner inventory cache across dota reconnect steam_id=%llu lobby_id=%llu state=%u game_state=%u launch_phase=%s abandon_postgame=%u",
                static_cast<unsigned long long>(disconnected_steam_id),
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state,
                GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase),
                GBE_local_lobby.abandon_postgame_active ? 1u : 0u
            );
        } else if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && disconnected_steam_id != 0) {
            if (GBE_SetDotaLobbyMemberConnected(disconnected_steam_id, false)) {
                GBE_PublishSharedDotaLobbyState("member_disconnected");
                GBE_GC_DebugLog(
                    "GC_DOTA_SYNC",
                    "marked Dota lobby member disconnected steam_id=%llu lobby_id=%llu state=%u game_state=%u members=%zu",
                    static_cast<unsigned long long>(disconnected_steam_id),
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state,
                    GBE_local_lobby.members.size()
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

static bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl(
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
    std::string &message)
{
    if (steam_id == 0 || account_id == 0 || lobby_id == 0)
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
        pass_key);
}

bool Steam_Game_Coordinator::GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay(const std::string &player_name, std::string &message)
{
    GBE_LocalLobby lobby{};
    if (!GBE_CaptureCurrentDotaLobbyState("cache_template_replay", lobby))
        return false;

    return GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay(lobby, player_name, message);
}

bool Steam_Game_Coordinator::GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay(const GBE_LocalLobby &lobby, const std::string &player_name, std::string &message)
{
    const uint64 owner_steam_id = lobby.owner_steam_id != 0 ? lobby.owner_steam_id : GBE_GetDotaLobbyOwnerSteamId();
    const uint32 owner_account_id = lobby.owner_account_id != 0 ? lobby.owner_account_id : GBE_GetDotaLobbyOwnerAccountId();

    return GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl(
        owner_steam_id,
        owner_account_id,
        lobby.lobby_id,
        lobby.state,
        lobby.game_state,
        lobby.server_id,
        lobby.match_id,
        lobby.game_start_time,
        lobby.connect,
        player_name,
        lobby.room_name,
        lobby.game_mode,
        lobby.server_region,
        lobby.lan,
        lobby.lan_host_ping_location,
        lobby.allow_cheats,
        lobby.fill_with_bots,
        lobby.allow_spectating,
        lobby.visibility,
        lobby.bot_difficulty_radiant,
        lobby.bot_difficulty_dire,
        lobby.bot_radiant,
        lobby.bot_dire,
        lobby.owner_team,
        lobby.owner_slot,
        lobby.owner_hero_id,
        lobby.pass_key,
        lobby.members,
        false,
        false,
        owner_account_id,
        message);
}

static bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl(
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
    std::string &message)
{
    if (steam_id == 0 || lobby_id == 0)
        return false;

    return GBE_BuildDotaPracticeLobbyCacheSubscribedPayload(
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
        message);
}

bool Steam_Game_Coordinator::GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayload(const std::string &player_name, std::string &message)
{
    GBE_LocalLobby lobby{};
    if (!GBE_CaptureCurrentDotaLobbyState("cache_payload", lobby))
        return false;

    return GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayload(lobby, player_name, message);
}

bool Steam_Game_Coordinator::GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayload(const GBE_LocalLobby &lobby, const std::string &player_name, std::string &message)
{
    const uint64 owner_steam_id = lobby.owner_steam_id != 0 ? lobby.owner_steam_id : GBE_GetDotaLobbyOwnerSteamId();
    const uint32 owner_account_id = lobby.owner_account_id != 0 ? lobby.owner_account_id : GBE_GetDotaLobbyOwnerAccountId();

    return GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl(
        owner_steam_id,
        lobby.lobby_id,
        lobby.state,
        lobby.game_state,
        lobby.server_id,
        lobby.match_id,
        lobby.game_start_time,
        lobby.connect,
        player_name,
        lobby.room_name,
        lobby.game_mode,
        lobby.server_region,
        lobby.lan,
        lobby.lan_host_ping_location,
        lobby.allow_cheats,
        lobby.fill_with_bots,
        lobby.allow_spectating,
        lobby.visibility,
        lobby.bot_difficulty_radiant,
        lobby.bot_difficulty_dire,
        lobby.bot_radiant,
        lobby.bot_dire,
        lobby.owner_team,
        lobby.owner_slot,
        lobby.owner_hero_id,
        lobby.members,
        lobby.has_broadcast_channel,
        lobby.broadcast_channel_id,
        lobby.broadcast_country_code,
        lobby.broadcast_description,
        lobby.broadcast_language_code,
        lobby.pass_key,
        owner_account_id,
        message);
}

bool Steam_Game_Coordinator::GBE_BuildCurrentDotaPracticeLobbyDetailsUpdate(const GBE_LocalLobby &lobby, const std::string &player_name, std::string &message)
{
    const uint64 owner_steam_id = lobby.owner_steam_id != 0 ? lobby.owner_steam_id : GBE_GetDotaLobbyOwnerSteamId();
    const uint32 owner_account_id = lobby.owner_account_id != 0 ? lobby.owner_account_id : GBE_GetDotaLobbyOwnerAccountId();

    return GBE_BuildDotaPracticeLobbyDetailsUpdatePayload(
        owner_steam_id,
        owner_account_id,
        lobby.lobby_id,
        lobby.state,
        lobby.game_state,
        lobby.server_id,
        lobby.match_id,
        lobby.game_start_time,
        lobby.connect,
        player_name,
        lobby.room_name,
        lobby.game_mode,
        lobby.server_region,
        lobby.lan,
        lobby.lan_host_ping_location,
        lobby.allow_cheats,
        lobby.fill_with_bots,
        lobby.allow_spectating,
        lobby.visibility,
        lobby.bot_difficulty_radiant,
        lobby.bot_difficulty_dire,
        lobby.bot_radiant,
        lobby.bot_dire,
        lobby.owner_team,
        lobby.owner_slot,
        lobby.owner_hero_id,
        lobby.members,
        lobby.has_broadcast_channel,
        lobby.broadcast_channel_id,
        lobby.broadcast_country_code,
        lobby.broadcast_description,
        lobby.broadcast_language_code,
        lobby.pass_key,
        message);
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

    for (GBE_DotaLobbyMemberState &member : GBE_local_lobby.members) {
        if (member.steam_id != steam_id)
            continue;
        if (member.connected != connected) {
            member.connected = connected;
            changed = true;
        }
        return changed;
    }

    if (connected && steam_id != GBE_local_lobby.owner_steam_id) {
        GBE_DotaLobbyMemberState member{};
        member.steam_id = steam_id;
        member.account_id = CSteamID((uint64)steam_id).GetAccountID();
        member.team = GBE_kDotaTeamPlayerPool;
        member.connected = true;
        GBE_UpsertDotaLobbyMember(GBE_local_lobby.members, member);
        return true;
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
    if (GBE_ParseDotaPracticeLobbyConnectIPv4(GBE_local_lobby.connect) == 0u && !GBE_local_lobby.lan)
        return false;

    uint32 remote_count = 0u;
    uint32 connected_remote_count = 0u;
    const uint64 owner_steam_id = GBE_GetDotaLobbyOwnerSteamId();
    for (const GBE_DotaLobbyMemberState &member : GBE_local_lobby.members) {
        if (member.steam_id == 0ull || member.steam_id == owner_steam_id)
            continue;
        ++remote_count;
        if (member.connected)
            ++connected_remote_count;
    }

    if (remote_count_out)
        *remote_count_out = remote_count;
    if (connected_remote_count_out)
        *connected_remote_count_out = connected_remote_count;

    return remote_count != 0u && connected_remote_count < remote_count;
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
    return
        GBE_local_lobby.active &&
        GBE_local_lobby.lobby_id != 0 &&
        GBE_local_lobby.match_id != 0 &&
        GBE_local_lobby.game_start_time != 0 &&
        !GBE_local_lobby.connect.empty();
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

bool Steam_Game_Coordinator::GBE_TryAdvanceDotaLaunchToRun(const char *note, uint32 trigger_emsg, uint64 source_job, const char *reason)
{
    if (!GBE_HasDotaLaunchServerSetupSync())
        return false;

    if (GBE_local_lobby.launch_phase < GBE_kDotaLaunchPhaseSetupSynced)
        return false;

    GBE_MarkDotaLaunchPhase(GBE_kDotaLaunchPhaseRunQueued, reason ? reason : "launch_run_queued");

    return GBE_TryQueueDotaRuntimeLobbyDetailsUpdate(
        note ? note : "runtime packet after launch run gate",
        trigger_emsg,
        source_job,
        2u,
        0u);
}

bool Steam_Game_Coordinator::GBE_CaptureCurrentDotaLobbyState(const char *reason, GBE_LocalLobby &snapshot, bool restore_shared)
{
    if (restore_shared)
        GBE_RestoreSharedDotaLobbyState(reason);

    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0)
        return false;

    if (GBE_local_lobby.generic_lobby_id != 0) {
        Steam_Client *steam_client = get_steam_client();
        if (steam_client && steam_client->steam_matchmaking) {
            CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
            if (generic_lobby_id.IsLobby()) {
                const std::string generic_lobby_state_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyStateKey);
                const std::string generic_lobby_game_state_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyGameStateKey);
                const std::string generic_match_id_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyMatchIdKey);
                const std::string generic_server_id_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyServerIdKey);
                const std::string generic_connect = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyConnectKey);
                const std::string generic_game_start_time_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyGameStartTimeKey);
                if (!generic_lobby_state_raw.empty())
                    GBE_local_lobby.state = GBE_ParseUint32OrZero(generic_lobby_state_raw.c_str());
                if (!generic_lobby_game_state_raw.empty())
                    GBE_local_lobby.game_state = GBE_ParseUint32OrZero(generic_lobby_game_state_raw.c_str());
                const uint64 generic_match_id = GBE_ParseUint64OrZero(generic_match_id_raw.c_str());
                if (!generic_match_id_raw.empty() && (generic_match_id != 0ull || GBE_local_lobby.match_id == 0ull))
                    GBE_local_lobby.match_id = generic_match_id;
                const uint64 generic_server_id = GBE_ParseUint64OrZero(generic_server_id_raw.c_str());
                if (!generic_server_id_raw.empty() && (generic_server_id != 0ull || GBE_local_lobby.server_id == 0ull || GBE_local_lobby.match_id == 0ull))
                    GBE_local_lobby.server_id = generic_server_id;

                if (!generic_connect.empty())
                    GBE_local_lobby.connect = GBE_NormalizeDotaPracticeLobbyConnect(generic_connect);
                if (!generic_game_start_time_raw.empty())
                    GBE_local_lobby.game_start_time = GBE_ParseUint32OrZero(generic_game_start_time_raw.c_str());

                const bool repaired_owner = steam_client->steam_matchmaking->RepairLobbyOwnerIfMissing(generic_lobby_id, reason ? reason : "capture_current_lobby_state");
                if (repaired_owner) {
                    GBE_GC_DebugLog(
                        "GC_DOTA_LOBBY",
                        "[LOBBY] Repaired missing generic lobby owner before Dota snapshot reason=%s dota_lobby_id=%llu generic_lobby_id=%llu",
                        reason ? reason : "capture_current_lobby_state",
                        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id)
                    );
                }
                GBE_AdoptDotaGenericLobbyOwnerIfNeeded(reason ? reason : "capture_current_lobby_state");

                std::vector<GBE_DotaLobbyMemberState> members;
                const uint64 local_steam_id = settings->get_local_steam_id().ConvertToUint64();
                const std::vector<CSteamID> generic_members = steam_client->steam_matchmaking->GetLobbyMemberListSnapshot(generic_lobby_id);
                const bool preserve_launched_lan_members =
                    GBE_local_lobby.lan &&
                    GBE_local_lobby.match_id != 0ull &&
                    GBE_local_lobby.state >= 1u &&
                    !generic_members.empty();
                bool owner_in_generic_members = false;
                for (const CSteamID &member_id : generic_members) {
                    if (!member_id.IsValid())
                        continue;
                    if (member_id.ConvertToUint64() == GBE_local_lobby.owner_steam_id)
                        owner_in_generic_members = true;

                    GBE_DotaLobbyMemberState member{};
                    member.steam_id = member_id.ConvertToUint64();
                    member.account_id = member_id.GetAccountID();
                    member.connected = GBE_local_lobby.state == 3u;
                    if (member.steam_id == GBE_local_lobby.owner_steam_id) {
                        member.account_id = GBE_local_lobby.owner_account_id;
                        member.team = GBE_local_lobby.owner_team;
                        member.slot = GBE_local_lobby.owner_slot;
                        member.hero_id = GBE_local_lobby.owner_hero_id;
                        member.connected = GBE_local_lobby.owner_connected || GBE_local_lobby.state == 3u;
                        if (member.steam_id != local_steam_id) {
                            const char *owner_team_raw = steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberTeamKey);
                            if (!std::string(owner_team_raw ? owner_team_raw : "").empty()) {
                                member.team = GBE_ParseUint32OrZero(owner_team_raw);
                                GBE_local_lobby.owner_team = member.team;
                            }
                            const char *owner_slot_raw = steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberSlotKey);
                            if (!std::string(owner_slot_raw ? owner_slot_raw : "").empty()) {
                                member.slot = GBE_ParseUint32OrZero(owner_slot_raw);
                                GBE_local_lobby.owner_slot = member.slot;
                            }
                            member.hero_id = GBE_ParseUint32OrZero(steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberHeroKey));
                            member.connected = GBE_ParseUint32OrZero(steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberConnectedKey)) != 0u;
                            GBE_local_lobby.owner_hero_id = member.hero_id;
                            GBE_local_lobby.owner_connected = member.connected;
                        }
                    } else {
                        const char *member_team_raw = steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberTeamKey);
                        member.team = std::string(member_team_raw ? member_team_raw : "").empty() ? GBE_kDotaTeamPlayerPool : GBE_ParseUint32OrZero(member_team_raw);
                        member.slot = GBE_ParseUint32OrZero(steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberSlotKey));
                        member.hero_id = GBE_ParseUint32OrZero(steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberHeroKey));
                        member.connected = GBE_ParseUint32OrZero(steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberConnectedKey)) != 0u;
                        if (preserve_launched_lan_members && !member.connected) {
                            for (const GBE_DotaLobbyMemberState &existing : GBE_local_lobby.members) {
                                if (existing.steam_id == member.steam_id && existing.connected) {
                                    member.connected = true;
                                    break;
                                }
                            }
                        }
                    }
                    GBE_UpsertDotaLobbyMember(members, member);
                }

                for (const GBE_DotaLobbyMemberState &existing : GBE_local_lobby.members) {
                    const bool missing_from_generic =
                        existing.steam_id != 0ull &&
                        !GBE_DotaLobbyMembersContainSteamId(members, existing.steam_id);
                    if (generic_members.empty() || existing.steam_id == local_steam_id || (preserve_launched_lan_members && missing_from_generic)) {
                        GBE_DotaLobbyMemberState preserved = existing;
                        if (preserve_launched_lan_members && missing_from_generic && preserved.steam_id != GBE_local_lobby.owner_steam_id)
                            preserved.connected = false;
                        GBE_UpsertDotaLobbyMember(members, preserved);
                    }
                }

                GBE_DotaLobbyMemberState owner{};
                owner.steam_id = GBE_local_lobby.owner_steam_id;
                owner.account_id = GBE_local_lobby.owner_account_id;
                owner.team = GBE_local_lobby.owner_team;
                owner.slot = GBE_local_lobby.owner_slot;
                owner.hero_id = GBE_local_lobby.owner_hero_id;
                owner.connected = GBE_local_lobby.owner_connected || GBE_local_lobby.state == 3u;
                if (owner_in_generic_members || generic_members.empty())
                    GBE_UpsertDotaLobbyMember(members, owner);

                GBE_local_lobby.members = members;
            }
        }
    }

    snapshot = GBE_local_lobby;
    return true;
}

bool Steam_Game_Coordinator::GBE_CaptureCurrentDotaLobbyStateWithPreviousSlots(
    const char *reason,
    const std::vector<GBE_DotaLobbyMemberState> &previous_members,
    uint64 previous_owner_steam_id,
    GBE_LocalLobby &snapshot)
{
    if (!GBE_CaptureCurrentDotaLobbyState(reason, snapshot, false))
        return false;

    const uint64 new_owner_steam_id = GBE_local_lobby.owner_steam_id;
    const size_t before_count = GBE_local_lobby.members.size();
    GBE_PreserveDotaLobbyOwnerTransferSlots(GBE_local_lobby.members, previous_members, previous_owner_steam_id, new_owner_steam_id);
    if (!GBE_DotaLobbyMembersEqual(snapshot.members, GBE_local_lobby.members)) {
        snapshot.members = GBE_local_lobby.members;
        if (is_server)
            GBE_PublishSharedDotaLobbyState(reason ? reason : "owner_transfer_preserve_slots");

        size_t previous_owner_index = 0;
        size_t new_owner_index = 0;
        const bool has_previous_owner_index = GBE_FindDotaLobbyMemberIndex(previous_members, previous_owner_steam_id, previous_owner_index);
        const bool has_new_owner_index = GBE_FindDotaLobbyMemberIndex(GBE_local_lobby.members, new_owner_steam_id, new_owner_index);
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Preserved owner transfer member slots reason=%s lobby_id=%llu old_owner=%llu new_owner=%llu old_owner_index=%lld new_owner_index=%lld before_members=%zu after_members=%zu",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(previous_owner_steam_id),
            static_cast<unsigned long long>(new_owner_steam_id),
            has_previous_owner_index ? static_cast<long long>(previous_owner_index) : -1ll,
            has_new_owner_index ? static_cast<long long>(new_owner_index) : -1ll,
            before_count,
            GBE_local_lobby.members.size()
        );
    }

    return true;
}

void Steam_Game_Coordinator::GBE_RecordDotaLobbyCacheSubscriptionState(const std::string &message, const char *reason)
{
    if (message.size() < sizeof(ProtoBufMsgHeader_t))
        return;

    ProtoBufMsgHeader_t hdr{};
    std::memcpy(&hdr, message.data(), sizeof(hdr));
    const size_t body_offset = sizeof(hdr) + hdr.m_cubProtoBufExtHdr;
    if (body_offset > message.size())
        return;

    CMsgSOCacheSubscribed protomsg;
    if (!protomsg.ParseFromArray(message.data() + body_offset, static_cast<int>(message.size() - body_offset)))
        return;

    if (!protomsg.has_owner_soid() || protomsg.owner_soid().type() != 3u || protomsg.owner_soid().id() == 0)
        return;

    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && protomsg.owner_soid().id() != GBE_local_lobby.lobby_id)
        return;

    GBE_local_lobby.has_cache_version = protomsg.has_version();
    GBE_local_lobby.cache_version = protomsg.has_version() ? protomsg.version() : 0ull;
    GBE_local_lobby.has_cache_service_id = protomsg.has_service_id();
    GBE_local_lobby.cache_service_id = protomsg.has_service_id() ? protomsg.service_id() : 0u;
    GBE_local_lobby.cache_service_list.assign(protomsg.service_list().begin(), protomsg.service_list().end());
    GBE_local_lobby.has_cache_sync_version = protomsg.has_sync_version();
    GBE_local_lobby.cache_sync_version = protomsg.has_sync_version() ? protomsg.sync_version() : 0ull;

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "recorded lobby CacheSubscribed metadata reason=%s owner_id=%llu version_present=%u version=%llu service_id_present=%u service_id=%u service_list_count=%zu sync_version_present=%u sync_version=%llu",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(protomsg.owner_soid().id()),
        GBE_local_lobby.has_cache_version ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.cache_version),
        GBE_local_lobby.has_cache_service_id ? 1u : 0u,
        GBE_local_lobby.cache_service_id,
        GBE_local_lobby.cache_service_list.size(),
        GBE_local_lobby.has_cache_sync_version ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.cache_sync_version)
    );

    GBE_LogDotaSOCacheSubscribedSummary("GC_DOTA_SYNC", reason ? reason : "record_cache_subscribed_metadata", message);

    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0)
        GBE_PublishSharedDotaLobbyState(reason ? reason : "record_cache_subscribed_metadata");
}

void Steam_Game_Coordinator::GBE_PublishSharedDotaLobbyState(const char *reason)
{
    if (GBE_ShouldSuppressDotaAbandonedLobby(GBE_local_lobby.lobby_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "publish skipped for suppressed abandoned lobby reason=%s this=%p is_server=%u lobby_id=%llu active=%u state=%u game_state=%u",
            reason ? reason : "unknown",
            static_cast<void *>(this),
            is_server ? 1u : 0u,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.active ? 1u : 0u,
            GBE_local_lobby.state,
            GBE_local_lobby.game_state
        );
        return;
    }

    GBE_shared_dota_lobby_state.valid = true;
    GBE_shared_dota_lobby_state.active = GBE_local_lobby.active;
    GBE_shared_dota_lobby_state.lobby_id = GBE_local_lobby.lobby_id;
    GBE_shared_dota_lobby_state.generic_lobby_id = GBE_local_lobby.generic_lobby_id;
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
    if (is_server) {
        GBE_shared_dota_lobby_state.state = GBE_local_lobby.state;
        GBE_shared_dota_lobby_state.game_state = GBE_local_lobby.game_state;
        GBE_shared_dota_lobby_state.server_id = GBE_local_lobby.server_id;
    } else {
        if (GBE_local_lobby.state > GBE_shared_dota_lobby_state.state)
            GBE_shared_dota_lobby_state.state = GBE_local_lobby.state;
        if (GBE_local_lobby.game_state > GBE_shared_dota_lobby_state.game_state)
            GBE_shared_dota_lobby_state.game_state = GBE_local_lobby.game_state;
        if (GBE_local_lobby.server_id != 0)
            GBE_shared_dota_lobby_state.server_id = GBE_local_lobby.server_id;
    }
    GBE_shared_dota_lobby_state.match_id = GBE_local_lobby.match_id;
    GBE_shared_dota_lobby_state.owner_steam_id = GBE_local_lobby.owner_steam_id;
    GBE_shared_dota_lobby_state.owner_account_id = GBE_local_lobby.owner_account_id;
    GBE_shared_dota_lobby_state.owner_name = GBE_local_lobby.owner_name;
    GBE_shared_dota_lobby_state.connect = GBE_local_lobby.connect;
    GBE_shared_dota_lobby_state.game_start_time = GBE_local_lobby.game_start_time;
    GBE_shared_dota_lobby_state.owner_team = GBE_local_lobby.owner_team;
    GBE_shared_dota_lobby_state.owner_slot = GBE_local_lobby.owner_slot;
    GBE_shared_dota_lobby_state.owner_hero_id = GBE_local_lobby.owner_hero_id;
    GBE_shared_dota_lobby_state.owner_connected = GBE_local_lobby.owner_connected;
    GBE_shared_dota_lobby_state.members = GBE_local_lobby.members;
    GBE_shared_dota_lobby_state.launch_phase = GBE_local_lobby.launch_phase;
    GBE_shared_dota_lobby_state.launch_4511_seen = GBE_local_lobby.launch_4511_seen;
    GBE_shared_dota_lobby_state.has_broadcast_channel = GBE_local_lobby.has_broadcast_channel;
    GBE_shared_dota_lobby_state.broadcast_channel_id = GBE_local_lobby.broadcast_channel_id;
    GBE_shared_dota_lobby_state.broadcast_country_code = GBE_local_lobby.broadcast_country_code;
    GBE_shared_dota_lobby_state.broadcast_description = GBE_local_lobby.broadcast_description;
    GBE_shared_dota_lobby_state.broadcast_language_code = GBE_local_lobby.broadcast_language_code;
    GBE_shared_dota_lobby_state.pass_key = GBE_local_lobby.pass_key;
    GBE_shared_dota_lobby_state.has_cache_version = GBE_local_lobby.has_cache_version;
    GBE_shared_dota_lobby_state.cache_version = GBE_local_lobby.cache_version;
    GBE_shared_dota_lobby_state.has_cache_service_id = GBE_local_lobby.has_cache_service_id;
    GBE_shared_dota_lobby_state.cache_service_id = GBE_local_lobby.cache_service_id;
    GBE_shared_dota_lobby_state.cache_service_list = GBE_local_lobby.cache_service_list;
    GBE_shared_dota_lobby_state.has_cache_sync_version = GBE_local_lobby.has_cache_sync_version;
    GBE_shared_dota_lobby_state.cache_sync_version = GBE_local_lobby.cache_sync_version;

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "published shared lobby this=%p shared_lobby=%p reason=%s active=%u lobby_id=%llu generic_lobby_id=%llu match_id=%llu owner_steam_id=%llu owner_account_id=%u state=%u game_state=%u launch_phase=%s team=%u slot=%u connect=%s",
        static_cast<void *>(this),
        static_cast<void *>(&GBE_shared_dota_lobby_state),
        reason ? reason : "unknown",
        GBE_local_lobby.active ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.owner_steam_id),
        GBE_local_lobby.owner_account_id,
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase),
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot,
        GBE_local_lobby.connect.c_str()
    );

    if (is_server)
        GBE_PublishDotaPracticeLobbyMetadata(reason ? reason : "shared_lobby_state");
}

bool Steam_Game_Coordinator::GBE_MaybeNotifyDotaPracticeLobbyMembersChanged(const char *reason)
{
    if (is_server || gc_profile != GC_PROFILE_DOTA2)
        return false;
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.generic_lobby_id == 0)
        return false;
    if (GBE_local_lobby.state > 2u)
        return false;

    const std::vector<GBE_DotaLobbyMemberState> previous_members = GBE_local_lobby.members;
    const uint64 previous_owner_steam_id = GBE_local_lobby.owner_steam_id;
    const uint32 previous_owner_account_id = GBE_local_lobby.owner_account_id;
    const std::string previous_owner_name = GBE_local_lobby.owner_name;
    const uint32 previous_state = GBE_local_lobby.state;
    const uint32 previous_game_state = GBE_local_lobby.game_state;
    const uint64 previous_match_id = GBE_local_lobby.match_id;
    const uint64 previous_server_id = GBE_local_lobby.server_id;
    const std::string previous_connect = GBE_local_lobby.connect;
    const uint32 previous_game_start_time = GBE_local_lobby.game_start_time;
    GBE_LocalLobby lobby{};
    if (!GBE_CaptureCurrentDotaLobbyStateWithPreviousSlots(reason ? reason : "generic_lobby_members_changed", previous_members, previous_owner_steam_id, lobby))
        return false;
    const bool owner_changed =
        previous_owner_steam_id != GBE_local_lobby.owner_steam_id ||
        previous_owner_account_id != GBE_local_lobby.owner_account_id ||
        previous_owner_name != GBE_local_lobby.owner_name;
    const bool runtime_changed =
        previous_state != GBE_local_lobby.state ||
        previous_game_state != GBE_local_lobby.game_state ||
        previous_match_id != GBE_local_lobby.match_id ||
        previous_server_id != GBE_local_lobby.server_id ||
        previous_connect != GBE_local_lobby.connect ||
        previous_game_start_time != GBE_local_lobby.game_start_time;
    std::vector<GBE_DotaLobbyMemberState> joined_members;
    for (const GBE_DotaLobbyMemberState &member : GBE_local_lobby.members) {
        if (member.steam_id == 0ull)
            continue;
        bool existed = false;
        for (const GBE_DotaLobbyMemberState &previous_member : previous_members) {
            if (previous_member.steam_id == member.steam_id) {
                existed = true;
                break;
            }
        }
        if (!existed)
            joined_members.push_back(member);
    }
    if (!owner_changed && !runtime_changed && GBE_DotaLobbyMembersEqual(previous_members, GBE_local_lobby.members))
        return false;

    if (is_server)
        GBE_PublishSharedDotaLobbyState(reason ? reason : "generic_lobby_members_changed");
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Detected generic lobby member/owner/runtime change LobbyID=%llu generic_lobby_id=%llu old_members=%zu new_members=%zu old_owner=%llu new_owner=%llu old_state=%u new_state=%u old_game_state=%u new_game_state=%u old_server_id=%llu new_server_id=%llu old_connect=%s new_connect=%s reason=%s",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        previous_members.size(),
        GBE_local_lobby.members.size(),
        static_cast<unsigned long long>(previous_owner_steam_id),
        static_cast<unsigned long long>(GBE_local_lobby.owner_steam_id),
        previous_state,
        GBE_local_lobby.state,
        previous_game_state,
        GBE_local_lobby.game_state,
        static_cast<unsigned long long>(previous_server_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id),
        previous_connect.c_str(),
        GBE_local_lobby.connect.c_str(),
        reason ? reason : "generic_lobby_members_changed"
    );

    const uint64 local_steam_id = settings ? settings->get_local_steam_id().ConvertToUint64() : 0ull;
    const bool local_owner_lan_launch =
        local_steam_id != 0ull &&
        GBE_local_lobby.owner_steam_id != 0ull &&
        local_steam_id == GBE_local_lobby.owner_steam_id &&
        GBE_local_lobby.lan &&
        GBE_local_lobby.match_id != 0ull;

    std::string response_26;
    const bool sent_details_update = GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(lobby, GBE_GetDotaLobbyOwnerName(), response_26, is_server || local_owner_lan_launch);
    if (sent_details_update) {
        const bool peer_lan_direct_launch =
            local_steam_id != 0ull &&
            GBE_local_lobby.owner_steam_id != 0ull &&
            local_steam_id != GBE_local_lobby.owner_steam_id &&
            GBE_local_lobby.lan &&
            GBE_local_lobby.match_id != 0ull &&
            GBE_local_lobby.state == 2u &&
            GBE_local_lobby.server_id == 0ull &&
            GBE_ParseDotaPracticeLobbyConnectIPv4(GBE_GetDotaPracticeLobbyFirstConnectEndpoint(GBE_local_lobby.connect)) != 0u;

        if (peer_lan_direct_launch) {
            GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(reason ? reason : "generic_lobby_peer_lan_direct_launch");
        } else {
            push_incoming_now(GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask, response_26);
            if (runtime_changed)
                GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(reason ? reason : "generic_lobby_runtime_changed");
        }
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] %s direct 26 details update from preserved member snapshot LobbyID=%llu reason=%s size=%zu body_prefix=%s",
            peer_lan_direct_launch ? "Suppressed peer LAN" : "Sent",
            static_cast<unsigned long long>(lobby.lobby_id),
            reason ? reason : "generic_lobby_members_changed",
            response_26.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(response_26.data()), response_26.size(), 32).c_str()
        );
    } else {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed building preserved member snapshot 26 details update for LobbyID=%llu reason=%s",
            static_cast<unsigned long long>(lobby.lobby_id),
            reason ? reason : "generic_lobby_members_changed"
        );
    }

    if (GBE_local_lobby.has_chat_channel && GBE_local_lobby.chat_channel_id != 0) {
        for (const GBE_DotaLobbyMemberState &joined_member : joined_members) {
            std::string joined_name;
            Steam_Client *steam_client = get_steam_client();
            if (steam_client && steam_client->steam_matchmaking && GBE_local_lobby.generic_lobby_id != 0ull) {
                const char *generic_name = steam_client->steam_matchmaking->GetLobbyMemberData(
                    CSteamID((uint64)GBE_local_lobby.generic_lobby_id),
                    CSteamID((uint64)joined_member.steam_id),
                    GBE_kDotaGenericLobbyMemberNameKey);
                if (generic_name && generic_name[0] != '\0')
                    joined_name = generic_name;
            }
            if (joined_name.empty() && steam_client && steam_client->steam_friends) {
                const char *friend_name = steam_client->steam_friends->GetFriendPersonaName(CSteamID((uint64)joined_member.steam_id));
                if (friend_name && friend_name[0] != '\0' && std::string(friend_name) != "Unknown User")
                    joined_name = friend_name;
            }
            if (joined_name.empty())
                joined_name = "Lobby Member";

            std::string response_7013;
            if (GBE_BuildDotaOtherJoinedChannelPayload(GBE_local_lobby.chat_channel_id, joined_name, joined_member.steam_id, response_7013)) {
                push_incoming_now(GBE_kDotaOtherJoinedChannel | GBE_kProtoMask, response_7013);
                GBE_GC_DebugLog(
                    "GC_DOTA_LOBBY",
                    "[LOBBY] Sent 7013 other joined channel LobbyID=%llu channel_id=%llu steam_id=%llu name=%s reason=%s",
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    static_cast<unsigned long long>(GBE_local_lobby.chat_channel_id),
                    static_cast<unsigned long long>(joined_member.steam_id),
                    joined_name.c_str(),
                    reason ? reason : "generic_lobby_members_changed"
                );
            }
        }
    }

    if (GBE_local_lobby.has_chat_channel && GBE_local_lobby.chat_channel_id != 0) {
        GBE_LocalLobby chat_snapshot = lobby;
        chat_snapshot.members.clear();
        for (const GBE_DotaLobbyMemberState &member : lobby.members) {
            if (member.steam_id != 0ull)
                chat_snapshot.members.push_back(member);
        }

        std::string response_7010;
        if (GBE_BuildDotaJoinChatChannelResponsePayload(
                settings->get_local_steam_id().ConvertToUint64(),
                chat_snapshot.generic_lobby_id,
                chat_snapshot.chat_channel_id,
                chat_snapshot.chat_channel_name,
                std::string(settings->get_local_name()),
                chat_snapshot.members,
                chat_snapshot.owner_steam_id,
                chat_snapshot.owner_name,
                chat_snapshot.chat_channel_type,
                response_7010)) {
            push_incoming_now(GBE_kDotaJoinChatChannelResponse | GBE_kProtoMask, response_7010);
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Refreshed chat channel members after lobby member change LobbyID=%llu channel_id=%llu members=%zu reason=%s",
                static_cast<unsigned long long>(chat_snapshot.lobby_id),
                static_cast<unsigned long long>(chat_snapshot.chat_channel_id),
                chat_snapshot.members.size(),
                reason ? reason : "generic_lobby_members_changed"
            );
        } else {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Failed refreshing chat channel members after lobby member change LobbyID=%llu channel_id=%llu reason=%s",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(GBE_local_lobby.chat_channel_id),
                reason ? reason : "generic_lobby_members_changed"
            );
        }
    }

    return sent_details_update;
}

bool Steam_Game_Coordinator::GBE_MaybeHandleDotaPracticeLobbyKicked(const char *reason)
{
    if (is_server || gc_profile != GC_PROFILE_DOTA2)
        return false;
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.generic_lobby_id == 0)
        return false;
    const bool after_chat_leave = std::strcmp(reason ? reason : "", "7272_leave_chat") == 0;
    if (!GBE_local_lobby.has_chat_channel && !after_chat_leave)
        return false;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking)
        return false;

    CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
    if (!generic_lobby_id.IsLobby())
        return false;

    const uint64 local_steam_id = settings->get_local_steam_id().ConvertToUint64();
    const std::vector<CSteamID> generic_members = steam_client->steam_matchmaking->GetLobbyMemberListSnapshot(generic_lobby_id);
    bool still_in_generic_lobby = false;
    for (const CSteamID &member_id : generic_members) {
        if (member_id.ConvertToUint64() == local_steam_id) {
            still_in_generic_lobby = true;
            break;
        }
    }
    if (still_in_generic_lobby) {
        GBE_local_lobby.seen_local_in_generic_lobby = true;
        return false;
    }

    if (!GBE_local_lobby.seen_local_in_generic_lobby) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Waiting for generic lobby join confirmation before treating local user as kicked. LobbyID=%llu generic_lobby_id=%llu reason=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64()),
            reason ? reason : "generic_lobby_members_changed"
        );
        return false;
    }

    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    std::string response_25;
    if (!GBE_BuildDotaLobbyCacheUnsubscribedPayload(lobby_id, response_25)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 payload after practice lobby kick LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
        return true;
    }

    std::string response_7102;
    if (!GBE_BuildDotaPracticeLobbyKickedPopupPayload(response_7102)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7102 popup after practice lobby kick LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
        return true;
    }

    push_incoming_now(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25);
    push_incoming_now(GBE_kDotaPopup | GBE_kProtoMask, response_7102, 0.01);
    ResetGCMemory("7081_kicked_from_lobby", false, false);
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Detected local user kicked from generic Dota lobby. queued 25 and 7102 LobbyID=%llu generic_lobby_id=%llu reason=%s",
        static_cast<unsigned long long>(lobby_id),
        static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64()),
        reason ? reason : "generic_lobby_members_changed"
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_AdoptDotaGenericLobbyOwnerIfNeeded(const char *reason)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.generic_lobby_id == 0)
        return false;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking)
        return false;

    CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
    if (!generic_lobby_id.IsLobby())
        return false;

    CSteamID generic_owner = steam_client->steam_matchmaking->GetLobbyOwner(generic_lobby_id);
    if (!generic_owner.IsValid() || generic_owner.ConvertToUint64() == 0 || generic_owner.ConvertToUint64() == GBE_local_lobby.owner_steam_id)
        return false;

    const uint64 previous_owner_steam_id = GBE_local_lobby.owner_steam_id;
    const uint64 new_owner_steam_id = generic_owner.ConvertToUint64();
    const uint64 local_steam_id = settings->get_local_steam_id().ConvertToUint64();

    if (!is_server &&
        local_steam_id != 0ull &&
        previous_owner_steam_id != 0ull &&
        previous_owner_steam_id != local_steam_id &&
        new_owner_steam_id == local_steam_id &&
        GBE_local_lobby.lan &&
        GBE_local_lobby.state == 2u &&
        GBE_local_lobby.match_id != 0ull &&
        !GBE_local_lobby.connect.empty()) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Ignored generic lobby owner adoption during launched LAN peer disconnect reason=%s dota_lobby_id=%llu generic_lobby_id=%llu dota_owner=%llu generic_owner=%llu local=%llu state=%u game_state=%u match_id=%llu connect=%s",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
            static_cast<unsigned long long>(previous_owner_steam_id),
            static_cast<unsigned long long>(new_owner_steam_id),
            static_cast<unsigned long long>(local_steam_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            GBE_local_lobby.connect.c_str()
        );
        return false;
    }

    GBE_DotaLobbyMemberState new_owner{};
    new_owner.steam_id = new_owner_steam_id;
    new_owner.account_id = generic_owner.GetAccountID();
    new_owner.team = GBE_kDotaTeamGoodGuys;
    new_owner.slot = 0u;
    new_owner.connected = GBE_local_lobby.state == 3u;

    for (const GBE_DotaLobbyMemberState &member : GBE_local_lobby.members) {
        if (member.steam_id != new_owner_steam_id)
            continue;
        new_owner = member;
        break;
    }

    GBE_local_lobby.owner_steam_id = new_owner_steam_id;
    GBE_local_lobby.owner_account_id = new_owner.account_id;
    GBE_local_lobby.owner_team = new_owner.team;
    GBE_local_lobby.owner_slot = new_owner.slot;
    GBE_local_lobby.owner_hero_id = new_owner.hero_id;
    GBE_local_lobby.owner_connected = new_owner.connected;
    if (new_owner_steam_id == local_steam_id) {
        GBE_local_lobby.owner_name = std::string(settings->get_local_name());
    } else {
        const char *owner_name = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerNameKey);
        GBE_local_lobby.owner_name = std::string(owner_name ? owner_name : "Lobby Host");
    }

    GBE_UpsertDotaLobbyMember(GBE_local_lobby.members, new_owner);

    if (new_owner_steam_id == local_steam_id) {
        GBE_PublishDotaPracticeLobbyLocalMemberData(reason ? reason : "adopt_generic_owner");
        GBE_PublishDotaPracticeLobbyMetadata(reason ? reason : "adopt_generic_owner");
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Adopted generic lobby owner reason=%s dota_lobby_id=%llu generic_lobby_id=%llu old_owner=%llu new_owner=%llu local_is_owner=%u owner_name=%s team=%u slot=%u",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        static_cast<unsigned long long>(previous_owner_steam_id),
        static_cast<unsigned long long>(new_owner_steam_id),
        new_owner_steam_id == local_steam_id ? 1u : 0u,
        GBE_local_lobby.owner_name.c_str(),
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot
    );
    return true;
}

void Steam_Game_Coordinator::GBE_PublishDotaPracticeLobbyLocalMemberData(const char *reason)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.generic_lobby_id == 0)
        return;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking)
        return;

    CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
    if (!generic_lobby_id.IsLobby())
        return;

    const uint64 local_steam_id = settings->get_local_steam_id().ConvertToUint64();
    const GBE_DotaLobbyMemberState *local_member = nullptr;
    for (const GBE_DotaLobbyMemberState &member : GBE_local_lobby.members) {
        if (member.steam_id == local_steam_id) {
            local_member = &member;
            break;
        }
    }
    if (!local_member)
        return;

    steam_client->steam_matchmaking->SetLobbyMemberData(generic_lobby_id, GBE_kDotaGenericLobbyMemberTeamKey, std::to_string(local_member->team).c_str());
    steam_client->steam_matchmaking->SetLobbyMemberData(generic_lobby_id, GBE_kDotaGenericLobbyMemberSlotKey, std::to_string(local_member->slot).c_str());
    steam_client->steam_matchmaking->SetLobbyMemberData(generic_lobby_id, GBE_kDotaGenericLobbyMemberHeroKey, std::to_string(local_member->hero_id).c_str());
    steam_client->steam_matchmaking->SetLobbyMemberData(generic_lobby_id, GBE_kDotaGenericLobbyMemberConnectedKey, local_member->connected ? "1" : "0");
    steam_client->steam_matchmaking->SetLobbyMemberData(generic_lobby_id, GBE_kDotaGenericLobbyMemberNameKey, std::string(settings->get_local_name()).c_str());

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Published generic lobby member data reason=%s dota_lobby_id=%llu generic_lobby_id=%llu steam_id=%llu team=%u slot=%u hero=%u connected=%u name=%s",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        static_cast<unsigned long long>(local_member->steam_id),
        local_member->team,
        local_member->slot,
        local_member->hero_id,
        local_member->connected ? 1u : 0u,
        settings->get_local_name()
    );
}

void Steam_Game_Coordinator::GBE_PublishDotaPracticeLobbyMetadata(const char *reason)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.generic_lobby_id == 0)
        return;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking)
        return;

    CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
    if (!generic_lobby_id.IsLobby())
        return;

    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyMarkerKey, GBE_kDotaGenericLobbyMarkerValue);
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyDotaLobbyIdKey, std::to_string(GBE_local_lobby.lobby_id).c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyRoomNameKey, (GBE_local_lobby.room_name.empty() ? std::string("Lobby") : GBE_local_lobby.room_name).c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyGameModeKey, std::to_string(GBE_local_lobby.game_mode).c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyServerRegionKey, std::to_string(GBE_local_lobby.server_region).c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyLanPingKey, GBE_local_lobby.lan_host_ping_location.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyPassKeyKey, GBE_local_lobby.pass_key.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerSteamIdKey, std::to_string(GBE_local_lobby.owner_steam_id).c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerAccountIdKey, std::to_string(GBE_local_lobby.owner_account_id).c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerNameKey, (GBE_local_lobby.owner_name.empty() ? std::string(settings->get_local_name()) : GBE_local_lobby.owner_name).c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyStateKey, std::to_string(GBE_local_lobby.state).c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyGameStateKey, std::to_string(GBE_local_lobby.game_state).c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyMatchIdKey, std::to_string(GBE_local_lobby.match_id).c_str());
    const std::string normalized_connect = GBE_NormalizeDotaPracticeLobbyConnect(GBE_local_lobby.connect);
    GBE_local_lobby.connect = normalized_connect;
    if (GBE_local_lobby.match_id == 0) {
        GBE_local_lobby.server_id = 0ull;
    }
    if (GBE_shared_dota_lobby_state.valid && GBE_shared_dota_lobby_state.lobby_id == GBE_local_lobby.lobby_id) {
        GBE_shared_dota_lobby_state.connect = normalized_connect;
        if (GBE_local_lobby.match_id == 0) {
            GBE_shared_dota_lobby_state.server_id = 0ull;
        }
    }

    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyServerIdKey, std::to_string(GBE_local_lobby.server_id).c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyConnectKey, normalized_connect.c_str());
    steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyGameStartTimeKey, std::to_string(GBE_local_lobby.game_start_time).c_str());

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Published generic lobby metadata reason=%s dota_lobby_id=%llu generic_lobby_id=%llu room=%s mode=%u region=%u pass_len=%zu state=%u game_state=%u match_id=%llu server_id=%llu connect=%s",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        GBE_local_lobby.room_name.c_str(),
        GBE_local_lobby.game_mode,
        GBE_local_lobby.server_region,
        GBE_local_lobby.pass_key.size(),
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id),
        normalized_connect.c_str()
    );
}

std::vector<Steam_Game_Coordinator::GBE_LocalLobby> Steam_Game_Coordinator::GBE_GetDotaGenericLobbySnapshots(const char *reason)
{
    std::vector<GBE_LocalLobby> snapshots;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking)
        return snapshots;

    steam_client->steam_matchmaking->RefreshLobbyCallbacksForDota();

    const std::vector<CSteamID> generic_lobbies = steam_client->steam_matchmaking->GetLobbyListSnapshot();
    for (const CSteamID &generic_lobby_id : generic_lobbies) {
        if (!generic_lobby_id.IsLobby())
            continue;

        const char *marker = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyMarkerKey);
        if (std::string(marker ? marker : "") != GBE_kDotaGenericLobbyMarkerValue)
            continue;

        const uint64 dota_lobby_id = GBE_ParseUint64OrZero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyDotaLobbyIdKey));
        if (dota_lobby_id == 0)
            continue;

        const bool repaired_owner = steam_client->steam_matchmaking->RepairLobbyOwnerIfMissing(generic_lobby_id, reason ? reason : "generic_lobby_snapshot");
        if (repaired_owner) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Repaired missing generic lobby owner while reading snapshot reason=%s dota_lobby_id=%llu generic_lobby_id=%llu",
                reason ? reason : "generic_lobby_snapshot",
                static_cast<unsigned long long>(dota_lobby_id),
                static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64())
            );
        }

        CSteamID generic_owner_id = steam_client->steam_matchmaking->GetLobbyOwner(generic_lobby_id);

        GBE_LocalLobby snapshot{};
        snapshot.active = true;
        snapshot.lobby_id = dota_lobby_id;
        snapshot.generic_lobby_id = generic_lobby_id.ConvertToUint64();
        snapshot.owner_steam_id = GBE_ParseUint64OrZero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerSteamIdKey));
        if (generic_owner_id.IsValid() && generic_owner_id.ConvertToUint64() != 0ull)
            snapshot.owner_steam_id = generic_owner_id.ConvertToUint64();
        snapshot.owner_account_id = GBE_ParseUint32OrZero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerAccountIdKey));
        if (generic_owner_id.IsValid() && generic_owner_id.ConvertToUint64() == snapshot.owner_steam_id)
            snapshot.owner_account_id = generic_owner_id.GetAccountID();
        snapshot.owner_name = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerNameKey);
        if (generic_owner_id.IsValid() && generic_owner_id.ConvertToUint64() == settings->get_local_steam_id().ConvertToUint64()) {
            snapshot.owner_name = std::string(settings->get_local_name());
            steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerSteamIdKey, std::to_string(snapshot.owner_steam_id).c_str());
            steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerAccountIdKey, std::to_string(snapshot.owner_account_id).c_str());
            steam_client->steam_matchmaking->SetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerNameKey, snapshot.owner_name.c_str());
        }
        snapshot.room_name = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyRoomNameKey);
        snapshot.game_mode = GBE_ParseUint32OrZero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyGameModeKey));
        snapshot.server_region = GBE_ParseUint32OrZero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyServerRegionKey));
        snapshot.state = GBE_ParseUint32OrZero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyStateKey));
        snapshot.game_state = GBE_ParseUint32OrZero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyGameStateKey));
        snapshot.match_id = GBE_ParseUint64OrZero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyMatchIdKey));
        const std::string generic_server_id_raw = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyServerIdKey);
        snapshot.server_id = GBE_ParseUint64OrZero(generic_server_id_raw.c_str());
        snapshot.connect = GBE_NormalizeDotaPracticeLobbyConnect(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyConnectKey));
        snapshot.game_start_time = GBE_ParseUint32OrZero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyGameStartTimeKey));
        if (snapshot.state == 0u)
            snapshot.state = 1u;
        snapshot.lan = true;
        snapshot.lan_host_ping_location = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyLanPingKey);
        snapshot.pass_key = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyPassKeyKey);
        snapshot.fill_with_bots = true;
        snapshot.allow_spectating = true;
        snapshot.bot_difficulty_dire = 4u;
        snapshot.owner_team = GBE_kDotaTeamGoodGuys;
        snapshot.owner_slot = 1u;
        snapshot.owner_connected = false;
        const std::vector<CSteamID> generic_members = steam_client->steam_matchmaking->GetLobbyMemberListSnapshot(generic_lobby_id);
        bool owner_in_generic_members = false;
        for (const CSteamID &member_id : generic_members) {
            if (!member_id.IsValid())
                continue;
            if (member_id.ConvertToUint64() == snapshot.owner_steam_id)
                owner_in_generic_members = true;

            GBE_DotaLobbyMemberState member{};
            member.steam_id = member_id.ConvertToUint64();
            member.account_id = member_id.GetAccountID();
            member.connected = false;
            if (snapshot.owner_steam_id != 0ull && member.steam_id == snapshot.owner_steam_id) {
                member.account_id = snapshot.owner_account_id != 0u ? snapshot.owner_account_id : member.account_id;
                member.team = snapshot.owner_team;
                member.slot = snapshot.owner_slot;
                member.hero_id = snapshot.owner_hero_id;
                const char *owner_team_raw = steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberTeamKey);
                if (!std::string(owner_team_raw ? owner_team_raw : "").empty()) {
                    member.team = GBE_ParseUint32OrZero(owner_team_raw);
                    snapshot.owner_team = member.team;
                }
                const char *owner_slot_raw = steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberSlotKey);
                if (!std::string(owner_slot_raw ? owner_slot_raw : "").empty()) {
                    member.slot = GBE_ParseUint32OrZero(owner_slot_raw);
                    snapshot.owner_slot = member.slot;
                }
                member.hero_id = GBE_ParseUint32OrZero(steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberHeroKey));
                member.connected = GBE_ParseUint32OrZero(steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberConnectedKey)) != 0u;
                snapshot.owner_hero_id = member.hero_id;
                snapshot.owner_connected = member.connected;
            } else {
                const char *member_team_raw = steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberTeamKey);
                member.team = std::string(member_team_raw ? member_team_raw : "").empty() ? GBE_kDotaTeamPlayerPool : GBE_ParseUint32OrZero(member_team_raw);
                member.slot = GBE_ParseUint32OrZero(steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberSlotKey));
                member.hero_id = GBE_ParseUint32OrZero(steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberHeroKey));
                member.connected = GBE_ParseUint32OrZero(steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberConnectedKey)) != 0u;
            }
            GBE_UpsertDotaLobbyMember(snapshot.members, member);
        }
        if (snapshot.owner_account_id == 0)
            snapshot.owner_account_id = settings->get_local_steam_id().GetAccountID();
        if (snapshot.owner_name.empty())
            snapshot.owner_name = "Lobby Host";
        if (snapshot.room_name.empty())
            snapshot.room_name = "Lobby";

        GBE_DotaLobbyMemberState owner{};
        owner.steam_id = snapshot.owner_steam_id;
        owner.account_id = snapshot.owner_account_id;
        owner.team = snapshot.owner_team;
        owner.slot = snapshot.owner_slot;
        owner.hero_id = snapshot.owner_hero_id;
        owner.connected = snapshot.owner_connected;
        if (owner_in_generic_members || generic_members.empty())
            GBE_UpsertDotaLobbyMember(snapshot.members, owner);

        snapshots.push_back(snapshot);
    }

    GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Read generic lobby snapshots reason=%s count=%zu", reason ? reason : "unknown", snapshots.size());
    return snapshots;
}

bool Steam_Game_Coordinator::GBE_FindDotaGenericLobbyByDotaLobbyId(uint64 dota_lobby_id, CSteamID &generic_lobby_id, GBE_LocalLobby *lobby_snapshot, const char *reason)
{
    const std::vector<GBE_LocalLobby> snapshots = GBE_GetDotaGenericLobbySnapshots(reason ? reason : "find_generic_by_dota_lobby_id");
    for (const GBE_LocalLobby &snapshot : snapshots) {
        if (snapshot.lobby_id != dota_lobby_id)
            continue;
        generic_lobby_id = CSteamID((uint64)snapshot.generic_lobby_id);
        if (lobby_snapshot)
            *lobby_snapshot = snapshot;
        return generic_lobby_id.IsLobby();
    }
    return false;
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

void Steam_Game_Coordinator::GBE_SyncSettingsLobbyFromGenericLobby(const char *reason)
{
    if (!settings)
        return;

    CSteamID target_lobby_id = k_steamIDNil;
    if (GBE_local_lobby.active && GBE_local_lobby.generic_lobby_id != 0) {
        CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
        if (generic_lobby_id.IsLobby())
            target_lobby_id = generic_lobby_id;
    }

    const CSteamID previous_lobby_id = settings->get_lobby();
    if (previous_lobby_id == target_lobby_id)
        return;

    settings->set_lobby(target_lobby_id);
    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "synced settings lobby from generic reason=%s active=%u dota_lobby_id=%llu generic_lobby_id=%llu old_settings_lobby=%llu new_settings_lobby=%llu",
        reason ? reason : "unknown",
        GBE_local_lobby.active ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        static_cast<unsigned long long>(previous_lobby_id.ConvertToUint64()),
        static_cast<unsigned long long>(target_lobby_id.ConvertToUint64())
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

    if (GBE_ShouldSuppressDotaAbandonedLobby(GBE_shared_dota_lobby_state.lobby_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "restore skipped for suppressed abandoned lobby this=%p shared_lobby=%p reason=%s lobby_id=%llu active=%u state=%u game_state=%u",
            static_cast<void *>(this),
            static_cast<void *>(&GBE_shared_dota_lobby_state),
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_shared_dota_lobby_state.lobby_id),
            GBE_shared_dota_lobby_state.active ? 1u : 0u,
            GBE_shared_dota_lobby_state.state,
            GBE_shared_dota_lobby_state.game_state
        );
        return;
    }

    if (!is_server) {
        if (!GBE_shared_dota_lobby_state.active)
            return;
        if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && GBE_local_lobby.lobby_id != GBE_shared_dota_lobby_state.lobby_id)
            return;

        if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
            GBE_local_lobby.active = GBE_shared_dota_lobby_state.active;
            GBE_local_lobby.lobby_id = GBE_shared_dota_lobby_state.lobby_id;
            GBE_local_lobby.generic_lobby_id = GBE_shared_dota_lobby_state.generic_lobby_id;
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
            GBE_local_lobby.server_id = GBE_shared_dota_lobby_state.match_id != 0ull ? GBE_shared_dota_lobby_state.server_id : 0ull;
            GBE_local_lobby.owner_steam_id = GBE_shared_dota_lobby_state.owner_steam_id;
            GBE_local_lobby.owner_account_id = GBE_shared_dota_lobby_state.owner_account_id;
            GBE_local_lobby.owner_name = GBE_shared_dota_lobby_state.owner_name;
            GBE_local_lobby.connect = GBE_NormalizeDotaPracticeLobbyConnect(GBE_shared_dota_lobby_state.connect);
            GBE_local_lobby.game_start_time = GBE_shared_dota_lobby_state.game_start_time;
            GBE_local_lobby.owner_team = GBE_shared_dota_lobby_state.owner_team;
            GBE_local_lobby.owner_slot = GBE_shared_dota_lobby_state.owner_slot;
            GBE_local_lobby.owner_hero_id = GBE_shared_dota_lobby_state.owner_hero_id;
            GBE_local_lobby.owner_connected = GBE_shared_dota_lobby_state.owner_connected;
            GBE_local_lobby.members = GBE_shared_dota_lobby_state.members;
            GBE_local_lobby.launch_phase = GBE_shared_dota_lobby_state.launch_phase;
            GBE_local_lobby.launch_4511_seen = GBE_shared_dota_lobby_state.launch_4511_seen;
            GBE_local_lobby.has_broadcast_channel = GBE_shared_dota_lobby_state.has_broadcast_channel;
            GBE_local_lobby.broadcast_channel_id = GBE_shared_dota_lobby_state.broadcast_channel_id;
            GBE_local_lobby.broadcast_country_code = GBE_shared_dota_lobby_state.broadcast_country_code;
            GBE_local_lobby.broadcast_description = GBE_shared_dota_lobby_state.broadcast_description;
            GBE_local_lobby.broadcast_language_code = GBE_shared_dota_lobby_state.broadcast_language_code;
            GBE_local_lobby.pass_key = GBE_shared_dota_lobby_state.pass_key;
            GBE_local_lobby.has_cache_version = GBE_shared_dota_lobby_state.has_cache_version;
            GBE_local_lobby.cache_version = GBE_shared_dota_lobby_state.cache_version;
            GBE_local_lobby.has_cache_service_id = GBE_shared_dota_lobby_state.has_cache_service_id;
            GBE_local_lobby.cache_service_id = GBE_shared_dota_lobby_state.cache_service_id;
            GBE_local_lobby.cache_service_list = GBE_shared_dota_lobby_state.cache_service_list;
            GBE_local_lobby.has_cache_sync_version = GBE_shared_dota_lobby_state.has_cache_sync_version;
            GBE_local_lobby.cache_sync_version = GBE_shared_dota_lobby_state.cache_sync_version;

            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "adopted full shared lobby on client this=%p shared_lobby=%p reason=%s lobby_id=%llu state=%u game_state=%u server_id=%llu",
                static_cast<void *>(this),
                static_cast<void *>(&GBE_shared_dota_lobby_state),
                reason ? reason : "unknown",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state,
                static_cast<unsigned long long>(GBE_local_lobby.server_id)
            );
            GBE_SyncSettingsLobbyFromGenericLobby(reason ? reason : "restore_client_full_adopt");
            GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(reason ? reason : "restore_client_full_adopt");
            GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot(reason ? reason : "restore_client_full_adopt");
            GBE_PushDotaLoginSyncMessages();
            return;
        }

        bool changed = false;
        if (GBE_local_lobby.generic_lobby_id != GBE_shared_dota_lobby_state.generic_lobby_id) {
            GBE_local_lobby.generic_lobby_id = GBE_shared_dota_lobby_state.generic_lobby_id;
            changed = true;
        }
        const uint64 previous_server_id = GBE_local_lobby.server_id;
        const std::string previous_connect = GBE_local_lobby.connect;

        if (GBE_local_lobby.server_id != 0ull && GBE_shared_dota_lobby_state.match_id == 0ull) {
            GBE_local_lobby.server_id = 0ull;
            changed = true;
        }

        const std::string shared_connect = GBE_NormalizeDotaPracticeLobbyConnect(GBE_shared_dota_lobby_state.connect);
        if (!shared_connect.empty() && GBE_local_lobby.connect != shared_connect) {
            GBE_local_lobby.connect = shared_connect;
            changed = true;
        }

        if (GBE_shared_dota_lobby_state.match_id != 0 && GBE_local_lobby.match_id != GBE_shared_dota_lobby_state.match_id) {
            GBE_local_lobby.match_id = GBE_shared_dota_lobby_state.match_id;
            changed = true;
        }

        if (GBE_local_lobby.state != GBE_shared_dota_lobby_state.state) {
            GBE_local_lobby.state = GBE_shared_dota_lobby_state.state;
            changed = true;
        }

        if (GBE_local_lobby.game_state != GBE_shared_dota_lobby_state.game_state) {
            GBE_local_lobby.game_state = GBE_shared_dota_lobby_state.game_state;
            changed = true;
        }

        if (GBE_shared_dota_lobby_state.game_start_time != 0 && GBE_local_lobby.game_start_time != GBE_shared_dota_lobby_state.game_start_time) {
            GBE_local_lobby.game_start_time = GBE_shared_dota_lobby_state.game_start_time;
            changed = true;
        }

        if (GBE_local_lobby.owner_connected != GBE_shared_dota_lobby_state.owner_connected) {
            GBE_local_lobby.owner_connected = GBE_shared_dota_lobby_state.owner_connected;
            changed = true;
        }

        if (GBE_local_lobby.launch_phase != GBE_shared_dota_lobby_state.launch_phase) {
            GBE_local_lobby.launch_phase = GBE_shared_dota_lobby_state.launch_phase;
            changed = true;
        }

        if (GBE_local_lobby.launch_4511_seen != GBE_shared_dota_lobby_state.launch_4511_seen) {
            GBE_local_lobby.launch_4511_seen = GBE_shared_dota_lobby_state.launch_4511_seen;
            changed = true;
        }

        if (GBE_local_lobby.owner_team != GBE_shared_dota_lobby_state.owner_team) {
            GBE_local_lobby.owner_team = GBE_shared_dota_lobby_state.owner_team;
            changed = true;
        }

        if (GBE_local_lobby.owner_slot != GBE_shared_dota_lobby_state.owner_slot) {
            GBE_local_lobby.owner_slot = GBE_shared_dota_lobby_state.owner_slot;
            changed = true;
        }

        if (GBE_shared_dota_lobby_state.owner_hero_id != 0 && GBE_local_lobby.owner_hero_id != GBE_shared_dota_lobby_state.owner_hero_id) {
            GBE_local_lobby.owner_hero_id = GBE_shared_dota_lobby_state.owner_hero_id;
            changed = true;
        }

        if (!GBE_DotaLobbyMembersEqual(GBE_local_lobby.members, GBE_shared_dota_lobby_state.members)) {
            GBE_local_lobby.members = GBE_shared_dota_lobby_state.members;
            changed = true;
        }

        if (GBE_local_lobby.has_cache_version != GBE_shared_dota_lobby_state.has_cache_version ||
                GBE_local_lobby.cache_version != GBE_shared_dota_lobby_state.cache_version) {
            GBE_local_lobby.has_cache_version = GBE_shared_dota_lobby_state.has_cache_version;
            GBE_local_lobby.cache_version = GBE_shared_dota_lobby_state.cache_version;
            changed = true;
        }

        if (GBE_local_lobby.has_cache_service_id != GBE_shared_dota_lobby_state.has_cache_service_id ||
                GBE_local_lobby.cache_service_id != GBE_shared_dota_lobby_state.cache_service_id) {
            GBE_local_lobby.has_cache_service_id = GBE_shared_dota_lobby_state.has_cache_service_id;
            GBE_local_lobby.cache_service_id = GBE_shared_dota_lobby_state.cache_service_id;
            changed = true;
        }

        if (GBE_local_lobby.cache_service_list != GBE_shared_dota_lobby_state.cache_service_list) {
            GBE_local_lobby.cache_service_list = GBE_shared_dota_lobby_state.cache_service_list;
            changed = true;
        }

        if (GBE_local_lobby.has_cache_sync_version != GBE_shared_dota_lobby_state.has_cache_sync_version ||
                GBE_local_lobby.cache_sync_version != GBE_shared_dota_lobby_state.cache_sync_version) {
            GBE_local_lobby.has_cache_sync_version = GBE_shared_dota_lobby_state.has_cache_sync_version;
            GBE_local_lobby.cache_sync_version = GBE_shared_dota_lobby_state.cache_sync_version;
            changed = true;
        }

        if (changed) {
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "adopted shared runtime on client this=%p shared_lobby=%p reason=%s lobby_id=%llu generic_lobby_id=%llu old_server_id=%llu new_server_id=%llu old_connect=%s new_connect=%s",
                static_cast<void *>(this),
                static_cast<void *>(&GBE_shared_dota_lobby_state),
                reason ? reason : "unknown",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
                static_cast<unsigned long long>(previous_server_id),
                static_cast<unsigned long long>(GBE_local_lobby.server_id),
                previous_connect.c_str(),
                GBE_local_lobby.connect.c_str()
            );
        }
        GBE_SyncSettingsLobbyFromGenericLobby(reason ? reason : "restore_client_runtime");
        GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(reason ? reason : "restore_client_runtime");
        GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot(reason ? reason : "restore_client_runtime");
        GBE_PushDotaLoginSyncMessages();
        return;
    }

    GBE_local_lobby.active = GBE_shared_dota_lobby_state.active;
    GBE_local_lobby.lobby_id = GBE_shared_dota_lobby_state.lobby_id;
    GBE_local_lobby.generic_lobby_id = GBE_shared_dota_lobby_state.generic_lobby_id;
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
    GBE_local_lobby.server_id = GBE_shared_dota_lobby_state.match_id != 0ull ? GBE_shared_dota_lobby_state.server_id : 0ull;
    GBE_local_lobby.owner_steam_id = GBE_shared_dota_lobby_state.owner_steam_id;
    GBE_local_lobby.owner_account_id = GBE_shared_dota_lobby_state.owner_account_id;
    GBE_local_lobby.owner_name = GBE_shared_dota_lobby_state.owner_name;
    GBE_local_lobby.connect = GBE_NormalizeDotaPracticeLobbyConnect(GBE_shared_dota_lobby_state.connect);
    GBE_local_lobby.game_start_time = GBE_shared_dota_lobby_state.game_start_time;
    GBE_local_lobby.owner_team = GBE_shared_dota_lobby_state.owner_team;
    GBE_local_lobby.owner_slot = GBE_shared_dota_lobby_state.owner_slot;
    GBE_local_lobby.owner_hero_id = GBE_shared_dota_lobby_state.owner_hero_id;
    GBE_local_lobby.owner_connected = GBE_shared_dota_lobby_state.owner_connected;
    GBE_local_lobby.members = GBE_shared_dota_lobby_state.members;
    GBE_local_lobby.launch_phase = GBE_shared_dota_lobby_state.launch_phase;
    GBE_local_lobby.has_broadcast_channel = GBE_shared_dota_lobby_state.has_broadcast_channel;
    GBE_local_lobby.broadcast_channel_id = GBE_shared_dota_lobby_state.broadcast_channel_id;
    GBE_local_lobby.broadcast_country_code = GBE_shared_dota_lobby_state.broadcast_country_code;
    GBE_local_lobby.broadcast_description = GBE_shared_dota_lobby_state.broadcast_description;
    GBE_local_lobby.broadcast_language_code = GBE_shared_dota_lobby_state.broadcast_language_code;
    GBE_local_lobby.pass_key = GBE_shared_dota_lobby_state.pass_key;

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "restored shared lobby this=%p shared_lobby=%p reason=%s active=%u lobby_id=%llu generic_lobby_id=%llu match_id=%llu owner_steam_id=%llu owner_account_id=%u state=%u game_state=%u team=%u slot=%u connect=%s",
        static_cast<void *>(this),
        static_cast<void *>(&GBE_shared_dota_lobby_state),
        reason ? reason : "unknown",
        GBE_local_lobby.active ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.owner_steam_id),
        GBE_local_lobby.owner_account_id,
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot,
        GBE_local_lobby.connect.c_str()
    );
    GBE_SyncSettingsLobbyFromGenericLobby(reason ? reason : "restore_server_or_full");
    GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(reason ? reason : "restore_server_or_full");
}

void Steam_Game_Coordinator::GBE_MaybeReplayCurrentDotaPrivateLobbySnapshot(const char *reason)
{
    if (is_server) {
        GBE_dota_private_lobby_snapshot_replayed = false;
        return;
    }

    GBE_LocalLobby lobby{};
    if (!GBE_CaptureCurrentDotaLobbyState(reason ? reason : "replay_current_private_lobby_snapshot", lobby, false)) {
        GBE_dota_private_lobby_snapshot_replayed = false;
        return;
    }

    if (GBE_ShouldSuppressDotaAbandonedLobby(lobby.lobby_id)) {
        GBE_dota_private_lobby_snapshot_replayed = false;
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipped replaying current private lobby snapshot for suppressed abandoned lobby reason=%s lobby_id=%llu state=%u game_state=%u",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(lobby.lobby_id),
            lobby.state,
            lobby.game_state
        );
        return;
    }

    const bool ready_for_private_lobby_snapshot =
        lobby.state == 2u &&
        lobby.game_state >= 2u;

    if (!ready_for_private_lobby_snapshot) {
        GBE_dota_private_lobby_snapshot_replayed = false;
        return;
    }

    if (GBE_dota_private_lobby_snapshot_replayed)
        return;

    const uint64 local_steam_id = settings ? settings->get_local_steam_id().ConvertToUint64() : 0ull;
    const bool local_owner_lan_launch =
        local_steam_id != 0ull &&
        lobby.owner_steam_id != 0ull &&
        local_steam_id == lobby.owner_steam_id &&
        lobby.lan &&
        lobby.match_id != 0ull;

    std::string response_24;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed(lobby, GBE_GetDotaLobbyOwnerName(), response_24, is_server || local_owner_lan_launch)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "failed building current private lobby snapshot 24 reason=%s lobby_id=%llu state=%u game_state=%u server_id=%llu",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(lobby.lobby_id),
            lobby.state,
            lobby.game_state,
            static_cast<unsigned long long>(lobby.server_id)
        );
        return;
    }

    std::string response_26;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(lobby, GBE_GetDotaLobbyOwnerName(), response_26, is_server || local_owner_lan_launch)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "failed building current private lobby snapshot 26 reason=%s lobby_id=%llu state=%u game_state=%u server_id=%llu",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(lobby.lobby_id),
            lobby.state,
            lobby.game_state,
            static_cast<unsigned long long>(lobby.server_id)
        );
        return;
    }

    GBE_RecordDotaLobbyCacheSubscriptionState(response_24, reason ? reason : "replay_current_private_lobby_snapshot");
    push_incoming_now(GBE_kDotaCacheSubscribed | GBE_kProtoMask, response_24);
    push_incoming_now(
        GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask,
        response_26,
        true,
        lobby.state,
        lobby.game_state
    );
    GBE_dota_private_lobby_snapshot_replayed = true;

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "replayed current private lobby snapshot reason=%s lobby_id=%llu state=%u game_state=%u server_id=%llu size24=%zu size26=%zu",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(lobby.lobby_id),
        lobby.state,
        lobby.game_state,
        static_cast<unsigned long long>(lobby.server_id),
        response_24.size(),
        response_26.size()
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

bool Steam_Game_Coordinator::GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed(const GBE_LocalLobby &lobby, const std::string &player_name, std::string &message, bool preserve_server_id)
{
    const uint64 owner_steam_id = lobby.owner_steam_id != 0 ? lobby.owner_steam_id : GBE_GetDotaLobbyOwnerSteamId();
    const uint32 owner_account_id = lobby.owner_account_id != 0 ? lobby.owner_account_id : GBE_GetDotaLobbyOwnerAccountId();
    const bool launch_started = lobby.match_id != 0;
    const std::string effective_player_name = player_name.empty() ? GBE_GetDotaLobbyOwnerName() : player_name;
    const std::string effective_connect = GBE_NormalizeDotaPracticeLobbyConnect(lobby.connect);
    uint64 effective_server_id = 0ull;
    if (preserve_server_id && launch_started) {
        effective_server_id = lobby.server_id;
        if (effective_server_id == 0ull && is_server && settings)
            effective_server_id = settings->get_local_steam_id().ConvertToUint64();
        if (effective_server_id == 0ull && !is_server && settings && owner_steam_id == settings->get_local_steam_id().ConvertToUint64()) {
            Steam_Client *steam_client = get_steam_client();
            if (steam_client && steam_client->settings_server)
                effective_server_id = steam_client->settings_server->get_local_steam_id().ConvertToUint64();
        }
    }
    GBE_LocalLobby effective_lobby = lobby;
    effective_lobby.server_id = effective_server_id;
    effective_lobby.connect = effective_connect;

    const bool launched_lan_with_remote_members = launch_started && lobby.lan && lobby.members.size() > 1;

    if ((!launch_started || launched_lan_with_remote_members) && lobby.members.size() > 1)
        return GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayload(effective_lobby, effective_player_name, message);

    if (launch_started && owner_steam_id != 0 && owner_account_id != 0) {
        if (GBE_BuildDotaPracticeLobbyLaunchCacheSubscribedTemplateReplay(
                owner_account_id,
                owner_steam_id,
                lobby.lobby_id,
                lobby.state,
                lobby.game_state,
                effective_server_id,
                lobby.match_id,
                lobby.game_start_time,
                effective_connect,
                effective_player_name,
                lobby.room_name,
                lobby.game_mode,
                lobby.server_region,
                lobby.lan,
                lobby.lan_host_ping_location,
                lobby.allow_cheats,
                lobby.fill_with_bots,
                lobby.allow_spectating,
                lobby.visibility,
                lobby.bot_difficulty_radiant,
                lobby.bot_difficulty_dire,
                lobby.bot_radiant,
                lobby.bot_dire,
                lobby.owner_team,
                lobby.owner_slot,
                lobby.owner_hero_id,
                lobby.pass_key,
                owner_account_id,
                message)) {
            return true;
        }
    }

    return GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplay(effective_lobby, effective_player_name, message);
}

bool Steam_Game_Coordinator::GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(const GBE_LocalLobby &lobby, const std::string &player_name, std::string &message, bool preserve_server_id)
{
    const uint64 owner_steam_id = lobby.owner_steam_id != 0 ? lobby.owner_steam_id : GBE_GetDotaLobbyOwnerSteamId();
    const uint32 owner_account_id = lobby.owner_account_id != 0 ? lobby.owner_account_id : GBE_GetDotaLobbyOwnerAccountId();
    const std::string effective_player_name = player_name.empty() ? GBE_GetDotaLobbyOwnerName() : player_name;
    const std::string effective_connect = GBE_NormalizeDotaPracticeLobbyConnect(lobby.connect);
    const uint32 startup_account_id = GBE_GetDotaPracticeLobbyStartupAccountIdForState(owner_account_id, lobby.state, lobby.game_state);
    uint64 effective_server_id = 0ull;
    if (preserve_server_id && lobby.match_id != 0) {
        effective_server_id = lobby.server_id;
        if (effective_server_id == 0ull && is_server && settings)
            effective_server_id = settings->get_local_steam_id().ConvertToUint64();
        if (effective_server_id == 0ull && !is_server && settings && owner_steam_id == settings->get_local_steam_id().ConvertToUint64()) {
            Steam_Client *steam_client = get_steam_client();
            if (steam_client && steam_client->settings_server)
                effective_server_id = steam_client->settings_server->get_local_steam_id().ConvertToUint64();
        }
    }
    GBE_LocalLobby effective_lobby = lobby;
    effective_lobby.server_id = effective_server_id;
    effective_lobby.connect = effective_connect;

    const bool launched_lan_with_remote_members = lobby.match_id != 0 && lobby.lan && lobby.members.size() > 1;

    if ((lobby.match_id == 0 || launched_lan_with_remote_members) && lobby.members.size() > 1)
        return GBE_BuildCurrentDotaPracticeLobbyDetailsUpdate(effective_lobby, effective_player_name, message);

    if (owner_steam_id != 0 && owner_account_id != 0) {
        if (GBE_BuildDotaPracticeLobbyOfficial26ReplayPayload(
                GBE_kDotaOfficial032PracticeLobby26Hex,
                "authoritative practice lobby 26",
                owner_account_id,
                owner_steam_id,
                lobby.lobby_id,
                effective_server_id,
                lobby.match_id,
                lobby.game_start_time,
                effective_connect,
                effective_player_name,
                lobby.room_name,
                lobby.game_mode,
                lobby.server_region,
                lobby.lan,
                lobby.lan_host_ping_location,
                lobby.allow_cheats,
                lobby.fill_with_bots,
                lobby.allow_spectating,
                lobby.visibility,
                lobby.bot_difficulty_radiant,
                lobby.bot_difficulty_dire,
                lobby.bot_radiant,
                lobby.bot_dire,
                lobby.owner_team,
                lobby.owner_slot,
                lobby.owner_hero_id,
                lobby.pass_key,
                lobby.state,
                lobby.game_state,
                startup_account_id != 0u,
                startup_account_id,
                message)) {
            return true;
        }
    }

    return GBE_BuildCurrentDotaPracticeLobbyDetailsUpdate(effective_lobby, effective_player_name, message);
}

uint64 Steam_Game_Coordinator::GBE_GetDotaLobbyOwnerSteamId() const
{
    if (GBE_local_lobby.owner_steam_id != 0)
        return GBE_local_lobby.owner_steam_id;

    if (GBE_shared_dota_lobby_state.owner_steam_id != 0)
        return GBE_shared_dota_lobby_state.owner_steam_id;

    return settings->get_local_steam_id().ConvertToUint64();
}

uint32 Steam_Game_Coordinator::GBE_GetDotaLobbyOwnerAccountId() const
{
    if (GBE_local_lobby.owner_account_id != 0)
        return GBE_local_lobby.owner_account_id;

    if (GBE_shared_dota_lobby_state.owner_account_id != 0)
        return GBE_shared_dota_lobby_state.owner_account_id;

    return settings->get_local_steam_id().GetAccountID();
}

std::string Steam_Game_Coordinator::GBE_GetDotaLobbyOwnerName() const
{
    if (!GBE_local_lobby.owner_name.empty())
        return GBE_local_lobby.owner_name;

    if (!GBE_shared_dota_lobby_state.owner_name.empty())
        return GBE_shared_dota_lobby_state.owner_name;

    return std::string(settings->get_local_name());
}

void Steam_Game_Coordinator::GBE_LeaveGenericLobby()
{
    GBE_ResetDotaPracticeLobbyLaunchPeripheralState();
    if (GBE_local_lobby.generic_lobby_id == 0) {
        GBE_SyncSettingsLobbyFromGenericLobby("leave_generic_lobby_noop");
        return;
    }

    Steam_Client *steam_client = get_steam_client();
    if (steam_client && steam_client->steam_matchmaking) {
        CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
        if (generic_lobby_id.IsLobby())
            steam_client->steam_matchmaking->LeaveLobby(generic_lobby_id);
    }

    GBE_local_lobby.generic_lobby_id = 0;
    GBE_SyncSettingsLobbyFromGenericLobby("leave_generic_lobby");
}

bool Steam_Game_Coordinator::GBE_SyncGenericLobbyGameServer(const char *reason)
{
    if (!is_server)
        return false;

    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.generic_lobby_id == 0 || GBE_local_lobby.server_id == 0)
        return false;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking || !steam_client->steam_gameserver)
        return false;

    Steam_GameServer *game_server = steam_client->steam_gameserver;
    if (!game_server->BLoggedOn())
        return false;

    uint32 lobby_ip = game_server->GetPublicIP_old();
    if (GBE_local_lobby.lan) {
        const uint32 connect_ip = GBE_ParseDotaPracticeLobbyConnectIPv4(GBE_local_lobby.connect);
        if (connect_ip != 0)
            lobby_ip = connect_ip;
        else if (network)
            lobby_ip = network->getOwnIP();
    }
    constexpr uint16 lobby_port = 27015u;
    CSteamID lobby_steam_id((uint64)GBE_local_lobby.generic_lobby_id);
    CSteamID gameserver_steam_id((uint64)GBE_local_lobby.server_id);
    const bool has_ip_server_id = GBE_local_lobby.server_id != 0ull && !gameserver_steam_id.IsValid() && GBE_ParseDotaPracticeLobbyConnectIPv4(GBE_local_lobby.connect) != 0u;
    if (!lobby_steam_id.IsLobby() || (!gameserver_steam_id.IsValid() && !has_ip_server_id))
        return false;

    uint32 previous_ip = 0;
    uint16 previous_port = 0;
    CSteamID previous_server_id = k_steamIDNil;
    const bool had_previous_gameserver = steam_client->steam_matchmaking->GetLobbyGameServer(
        lobby_steam_id,
        &previous_ip,
        &previous_port,
        &previous_server_id);

    if (had_previous_gameserver &&
            previous_server_id == gameserver_steam_id &&
            previous_ip == lobby_ip &&
            previous_port == lobby_port) {
        return false;
    }

    steam_client->steam_matchmaking->SetLobbyGameServer(
        lobby_steam_id,
        lobby_ip,
        lobby_port,
        gameserver_steam_id);

    if (steam_client->steam_user) {
        steam_client->steam_user->AdvertiseGame(gameserver_steam_id, lobby_ip, lobby_port);
    }

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "synced generic lobby gameserver reason=%s dota_lobby_id=%llu generic_lobby_id=%llu server_id=%llu ip=%s port=%u had_previous=%u previous_server_id=%llu previous_ip=%s previous_port=%u",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id),
        GBE_FormatIPv4(lobby_ip).c_str(),
        static_cast<unsigned>(lobby_port),
        had_previous_gameserver ? 1u : 0u,
        static_cast<unsigned long long>(previous_server_id.ConvertToUint64()),
        GBE_FormatIPv4(previous_ip).c_str(),
        static_cast<unsigned>(previous_port)
    );

    return true;
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

    uint32 connect_server_ip = GBE_ParseDotaPracticeLobbyConnectIPv4(GBE_local_lobby.connect);
    if (connect_server_ip == 0u && GBE_local_lobby.lan && network)
        connect_server_ip = network->getOwnIP();

    const uint64 derived_server_id = GBE_BuildDotaPracticeLobbyIpServerId(connect_server_ip);
    if (GBE_local_lobby.server_id == 0ull)
        return false;

    const uint64 previous_server_id = GBE_local_lobby.server_id;
    GBE_local_lobby.server_id = derived_server_id;
    if (GBE_shared_dota_lobby_state.valid && GBE_shared_dota_lobby_state.lobby_id == GBE_local_lobby.lobby_id)
        GBE_shared_dota_lobby_state.server_id = derived_server_id;

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "synced lobby server_id from connect endpoint reason=%s lobby_id=%llu match_id=%llu old=%llu derived=%llu lan_ip=%s",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(previous_server_id),
        static_cast<unsigned long long>(derived_server_id),
        GBE_FormatIPv4(connect_server_ip).c_str()
    );

    GBE_PublishSharedDotaLobbyState(reason ? reason : "server_id_clear");
    GBE_PushDotaLaunchStateToClientPeer(reason ? reason : "server_id_clear");
    return true;
}

static bool GBE_ShouldPreferDotaLobbyConnectUpdate(const std::string &current_connect, const std::string &candidate_connect)
{
    if (candidate_connect.empty() || current_connect == candidate_connect)
        return false;

    if (current_connect.empty())
        return true;

    const uint32 candidate_ip = GBE_ParseDotaPracticeLobbyConnectIPv4(candidate_connect);
    if (candidate_ip == 0u)
        return false;

    if (current_connect == GBE_FormatDotaPracticeLobbyLoopbackConnect())
        return true;

    return false;
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

    if (request_emsg == GBE_kDotaLobbyList) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 8011 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );

        return GBE_HandleDotaLobbyListRequest(has_source_job, source_job, false, nullptr);
    }

    if (request_emsg == GBE_kDotaFriendPracticeLobbyListRequest) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 7111 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );

        return GBE_HandleDotaFriendPracticeLobbyListRequest(false, nullptr);
    }

    if (request_emsg == GBE_kGCInviteToLobby) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 4512 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );

        return GBE_HandleDotaInviteToLobbyRequest(
            std::string(reinterpret_cast<const char *>(body), body_size),
            false,
            nullptr
        );
    }

    if (request_emsg == GBE_kGCLobbyInviteResponse) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 4513 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );

        return GBE_HandleDotaLobbyInviteResponseRequest(
            std::string(reinterpret_cast<const char *>(body), body_size),
            false,
            nullptr
        );
    }

    if (request_emsg == GBE_kDotaPracticeLobbyJoin) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 7044 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );

        return GBE_HandleDotaPracticeLobbyJoinRequest(
            std::string(reinterpret_cast<const char *>(body), body_size),
            source_job,
            has_source_job,
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

        return GBE_HandleDotaPracticeLobbyLaunchRequest(false, nullptr, has_source_job, source_job);
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

    if (request_emsg == GBE_kDotaPracticeLobbyKick) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 7081 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );

        return GBE_HandleDotaPracticeLobbyKickRequest(
            std::string(reinterpret_cast<const char *>(body), body_size),
            false,
            nullptr
        );
    }

    if (request_emsg == GBE_kDotaChatMessage) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received direct 7273 source_job=%llu body_size=%zu body_prefix=%s",
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );

        return GBE_HandleDotaChatMessageRequest(
            std::string(reinterpret_cast<const char *>(body), body_size),
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

    if (request_emsg == 2581) {
        uint64 account_id_field = settings->get_local_steam_id().GetAccountID();
        GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 1), account_id_field);

        std::string response_message;
        if (!GBE_BuildDota2582LookupAccountNameResponsePayload(
                static_cast<uint32>(account_id_field),
                std::string(settings->get_local_name()),
                has_source_job,
                source_job,
                response_message)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, 2582u);
            return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu size=%zu note=2581->2582 lookup account name account_id=%u",
            request_emsg,
            2582u,
            static_cast<unsigned long long>(source_job),
            response_message.size(),
            static_cast<unsigned>(account_id_field)
        );
        push_incoming_now(2582u | GBE_kProtoMask, response_message);
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
        if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0 && GBE_local_lobby.match_id != 0 && (GBE_local_lobby.server_id != 0 || !GBE_local_lobby.connect.empty())) {
            const GBE_Dota7034RequestShape request_shape = GBE_ParseDota7034RequestShape(body, body_size);
            bool queued_runtime_lobby_update = false;

            bool updated_owner_team_or_slot_from_7034 = false;
            if (request_shape.has_draft_steam_id && request_shape.draft_steam_id == GBE_GetDotaLobbyOwnerSteamId()) {
                if (request_shape.has_draft_team && GBE_local_lobby.owner_team != request_shape.draft_team) {
                    GBE_local_lobby.owner_team = request_shape.draft_team;
                    updated_owner_team_or_slot_from_7034 = true;
                }

                const uint32 draft_owner_slot = request_shape.has_draft_team_slot ? (request_shape.draft_team_slot + 1u) : 0u;
                if (draft_owner_slot != 0u && GBE_local_lobby.owner_slot != draft_owner_slot) {
                    GBE_local_lobby.owner_slot = draft_owner_slot;
                    updated_owner_team_or_slot_from_7034 = true;
                }
            }

            if (updated_owner_team_or_slot_from_7034) {
                GBE_GC_DebugLog(
                    "GC_DOTA_DIRECT",
                    "updated owner team/slot from 7034 draft team=%u slot=%u source_job=%llu state=%u game_state=%u",
                    GBE_local_lobby.owner_team,
                    GBE_local_lobby.owner_slot,
                    static_cast<unsigned long long>(source_job),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state
                );
                GBE_PublishSharedDotaLobbyState("7034_draft_team_slot");
            }

            if (request_shape.has_connected_steam_id && request_shape.connected_steam_id == GBE_GetDotaLobbyOwnerSteamId() && request_shape.has_connected_hero_id && request_shape.connected_hero_id != 0u && GBE_local_lobby.owner_hero_id != request_shape.connected_hero_id) {
                GBE_local_lobby.owner_hero_id = request_shape.connected_hero_id;
                GBE_GC_DebugLog(
                    "GC_DOTA_DIRECT",
                    "updated owner hero from 7034 request hero_id=%u source_job=%llu state=%u game_state=%u",
                    GBE_local_lobby.owner_hero_id,
                    static_cast<unsigned long long>(source_job),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state
                );
                GBE_PublishSharedDotaLobbyState("7034_connected_player_hero");
            }

            if (request_shape.has_connected_steam_id && request_shape.connected_steam_id != 0ull) {
                if (GBE_SetDotaLobbyMemberConnected(request_shape.connected_steam_id, true)) {
                    GBE_PublishSharedDotaLobbyState("7034_connected_player");
                    GBE_GC_DebugLog(
                        "GC_DOTA_DIRECT",
                        "marked connected player from 7034 steam_id=%llu source_job=%llu state=%u game_state=%u",
                        static_cast<unsigned long long>(request_shape.connected_steam_id),
                        static_cast<unsigned long long>(source_job),
                        GBE_local_lobby.state,
                        GBE_local_lobby.game_state
                    );
                }
            }

            if (request_shape.has_disconnected_steam_id && request_shape.disconnected_steam_id != 0ull) {
                if (GBE_SetDotaLobbyMemberConnected(request_shape.disconnected_steam_id, false)) {
                    GBE_PublishSharedDotaLobbyState("7034_disconnected_player");
                    GBE_GC_DebugLog(
                        "GC_DOTA_DIRECT",
                        "marked disconnected player from 7034 steam_id=%llu source_job=%llu state=%u game_state=%u",
                        static_cast<unsigned long long>(request_shape.disconnected_steam_id),
                        static_cast<unsigned long long>(source_job),
                        GBE_local_lobby.state,
                        GBE_local_lobby.game_state
                    );
                }
            }

            if (GBE_local_lobby.state == 1u &&
                GBE_local_lobby.game_state == 0u &&
                GBE_local_lobby.launch_phase >= GBE_kDotaLaunchPhaseSetupSynced &&
                GBE_local_lobby.launch_4511_seen) {
                if (GBE_TryAdvanceDotaLaunchToRun("runtime packet after matched 4511/7034", request_emsg, source_job, "7034_launch_run_after_4511"))
                    queued_runtime_lobby_update = true;
            }

            if (GBE_local_lobby.state == 1u && GBE_local_lobby.game_state == 0u) {
                const std::string request_summary = GBE_FormatDota7034Summary(body, body_size);
                GBE_GC_DebugLog(
                    "GC_DOTA_DIRECT",
                    "consumed req=%u source_job=%llu note=prelaunch 7034 observed before RUN apply active=%u lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu launch_phase=%s summary=%s",
                    request_emsg,
                    static_cast<unsigned long long>(source_job),
                    GBE_local_lobby.active ? 1u : 0u,
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state,
                    static_cast<unsigned long long>(GBE_local_lobby.match_id),
                    static_cast<unsigned long long>(GBE_local_lobby.server_id),
                    GBE_DescribeDotaLaunchPhase(GBE_local_lobby.launch_phase),
                    request_summary.c_str()
                );
            }

            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "consumed req=%u source_job=%llu note=runtime 7034 active=%u lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu summary=%s",
                request_emsg,
                static_cast<unsigned long long>(source_job),
                GBE_local_lobby.active ? 1u : 0u,
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                GBE_local_lobby.state,
                GBE_local_lobby.game_state,
                static_cast<unsigned long long>(GBE_local_lobby.match_id),
                static_cast<unsigned long long>(GBE_local_lobby.server_id),
                GBE_FormatDota7034Summary(body, body_size).c_str()
            );

            const bool request_advances_to_hero_selection = request_shape.has_game_state && request_shape.game_state >= 2u;
            const bool request_advances_to_strategy_time =
                (request_shape.has_game_state && request_shape.game_state >= 3u) ||
                (request_shape.has_send_reason && request_shape.send_reason == 10u);

            if (GBE_local_lobby.state == 2u && GBE_local_lobby.game_state == 1u) {
                if (!GBE_TryQueueDotaRuntimeLobbyDetailsUpdate("runtime packet after 8870/7034 wait_for_players", request_emsg, source_job, 2u, 1u))
                    return true;
                queued_runtime_lobby_update = true;
                if (request_advances_to_hero_selection) {
                    uint32 remote_count = 0u;
                    uint32 connected_remote_count = 0u;
                    if (GBE_ShouldHoldDotaLanLaunchForRemoteMembers(2u, &remote_count, &connected_remote_count)) {
                        GBE_GC_DebugLog(
                            "GC_DOTA_DIRECT",
                            "holding LAN hero_selection req=%u source_job=%llu remote_connected=%u remote_total=%u state=%u game_state=%u summary=%s",
                            request_emsg,
                            static_cast<unsigned long long>(source_job),
                            connected_remote_count,
                            remote_count,
                            GBE_local_lobby.state,
                            GBE_local_lobby.game_state,
                            GBE_FormatDota7034Summary(body, body_size).c_str()
                        );
                    } else if (GBE_TryQueueDotaRuntimeLobbyDetailsUpdate("runtime packet after 8870/7034 hero_selection", request_emsg, source_job, 2u, 2u)) {
                        queued_runtime_lobby_update = true;
                    }
                }
            }

            if (GBE_local_lobby.state == 2u && GBE_local_lobby.game_state == 0u &&
                    GBE_local_lobby.launch_phase >= GBE_kDotaLaunchPhaseRunQueued) {
                if (GBE_TryQueueDotaPrelaunch021("runtime wait_for_players after 7034", request_emsg, source_job))
                    queued_runtime_lobby_update = true;
            }

            if (GBE_local_lobby.state == 2u && GBE_local_lobby.game_state == 2u && request_advances_to_strategy_time) {
                uint32 remote_count = 0u;
                uint32 connected_remote_count = 0u;
                if (GBE_ShouldHoldDotaLanLaunchForRemoteMembers(3u, &remote_count, &connected_remote_count)) {
                    GBE_GC_DebugLog(
                        "GC_DOTA_DIRECT",
                        "holding LAN strategy_time req=%u source_job=%llu remote_connected=%u remote_total=%u state=%u game_state=%u summary=%s",
                        request_emsg,
                        static_cast<unsigned long long>(source_job),
                        connected_remote_count,
                        remote_count,
                        GBE_local_lobby.state,
                        GBE_local_lobby.game_state,
                        GBE_FormatDota7034Summary(body, body_size).c_str()
                    );
                } else if (GBE_TryQueueDotaRuntimeLobbyDetailsUpdate("runtime packet after 8330/7034 strategy_time", request_emsg, source_job, 2u, 3u)) {
                    queued_runtime_lobby_update = true;
                }
            }

            if (GBE_local_lobby.state == 2u &&
                GBE_local_lobby.game_state == 2u &&
                request_shape.has_game_state && request_shape.game_state == 2u &&
                request_shape.has_send_reason && request_shape.send_reason == 2u &&
                GBE_local_lobby.game_mode == 1u) {
                uint32 remote_count = 0u;
                uint32 connected_remote_count = 0u;
                if (GBE_ShouldHoldDotaLanLaunchForRemoteMembers(3u, &remote_count, &connected_remote_count)) {
                    GBE_GC_DebugLog(
                        "GC_DOTA_DIRECT",
                        "holding LAN AP fallback strategy_time req=%u source_job=%llu remote_connected=%u remote_total=%u state=%u game_state=%u summary=%s",
                        request_emsg,
                        static_cast<unsigned long long>(source_job),
                        connected_remote_count,
                        remote_count,
                        GBE_local_lobby.state,
                        GBE_local_lobby.game_state,
                        GBE_FormatDota7034Summary(body, body_size).c_str()
                    );
                } else if (GBE_TryQueueDotaRuntimeLobbyDetailsUpdate("runtime AP hero_selection fallback strategy_time", request_emsg, source_job, 2u, 3u, 1.0)) {
                    queued_runtime_lobby_update = true;
                }
            }

            if (GBE_local_lobby.state == 2u && GBE_local_lobby.game_state == 3u) {
                GBE_GC_DebugLog(
                    "GC_DOTA_DIRECT",
                    "consumed req=%u source_job=%llu note=skip synthetic official 032 follow-up and preserve strategy_time state=%u game_state=%u",
                    request_emsg,
                    static_cast<unsigned long long>(source_job),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state
                );
                queued_runtime_lobby_update = true;
            }

            if (!(GBE_local_lobby.state == 2u && GBE_local_lobby.game_state == 10u)) {
                if (GBE_SendDotaPracticeLobbyDetailsUpdate(false, nullptr, "7034_launch_poll")) {
                    GBE_GC_DebugLog(
                        "GC_DOTA_DIRECT",
                        "replying req=%u resp=%u source_job=%llu note=7034 direct poll uses runtime 26 fallback state=%u game_state=%u",
                        request_emsg,
                        GBE_kDotaPracticeLobbyDetailsUpdate,
                        static_cast<unsigned long long>(source_job),
                        GBE_local_lobby.state,
                        GBE_local_lobby.game_state
                    );
                    queued_runtime_lobby_update = true;
                }
            }

            if (queued_runtime_lobby_update) {
                GBE_GC_DebugLog(
                    "GC_DOTA_DIRECT",
                    "continuing req=%u source_job=%llu with connected players reply after runtime 26 updates state=%u game_state=%u",
                    request_emsg,
                    static_cast<unsigned long long>(source_job),
                    GBE_local_lobby.state,
                    GBE_local_lobby.game_state
                );
            }
        }

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
                GBE_local_lobby.members,
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

    if (request_emsg == 8744u) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "observed req=%u source_job=%llu body_size=%zu fields=%s body_prefix=%s",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            body_size,
            GBE_FormatProtoTopLevelFieldSummary(body, body_size).c_str(),
            GBE_FormatHexPrefix(body, body_size, 32).c_str()
        );
    }

    if (request_emsg == GBE_kDotaCacheSubscriptionRefresh) {
        uint64 requested_owner_type = 0;
        uint64 requested_owner_id = 0;
        std::string owner_soid;
        if (GBE_ExtractProtoFieldBytes(body, body_size, GBE_FindProtoField(body, body_size, 2u), owner_soid)) {
            GBE_ExtractProtoFieldUint64(
                reinterpret_cast<const uint8 *>(owner_soid.data()),
                owner_soid.size(),
                GBE_FindProtoField(reinterpret_cast<const uint8 *>(owner_soid.data()), owner_soid.size(), 1u),
                requested_owner_type);
            GBE_ExtractProtoFieldUint64(
                reinterpret_cast<const uint8 *>(owner_soid.data()),
                owner_soid.size(),
                GBE_FindProtoField(reinterpret_cast<const uint8 *>(owner_soid.data()), owner_soid.size(), 2u),
                requested_owner_id);
        }

        const bool matches_lobby_owner =
            GBE_local_lobby.active &&
            requested_owner_type == 3u &&
            requested_owner_id != 0 &&
            requested_owner_id == GBE_local_lobby.lobby_id;

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "observed req=%u source_job=%llu note=cache subscription refresh owner_type=%llu owner_id=%llu active=%u lobby_id=%llu state=%u game_state=%u body_prefix=%s",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            static_cast<unsigned long long>(requested_owner_type),
            static_cast<unsigned long long>(requested_owner_id),
            GBE_local_lobby.active ? 1u : 0u,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            GBE_FormatHexPrefix(body, body_size, 32).c_str()
        );

        if (matches_lobby_owner) {
            std::string response_message;
            if (!GBE_BuildDotaLobbyCacheSubscribedUpToDatePayload(
                    GBE_local_lobby.lobby_id,
                    GBE_local_lobby.has_cache_version,
                    GBE_local_lobby.cache_version,
                    GBE_local_lobby.has_cache_service_id,
                    GBE_local_lobby.cache_service_id,
                    GBE_local_lobby.cache_service_list,
                    GBE_local_lobby.has_cache_sync_version,
                    GBE_local_lobby.cache_sync_version,
                    response_message)) {
                GBE_GC_DebugLog(
                    "GC_DOTA_DIRECT",
                    "failed building reply req=%u resp=%u lobby_id=%llu",
                    request_emsg,
                    GBE_kDotaCacheSubscribedUpToDate,
                    static_cast<unsigned long long>(GBE_local_lobby.lobby_id)
                );
                return true;
            }

            GBE_GC_DebugLog(
                "GC_DOTA_DIRECT",
                "replying req=%u resp=%u source_job=%llu size=%zu note=cache subscription refresh acknowledged owner_type=%llu owner_id=%llu version_present=%u service_id_present=%u service_list_count=%zu sync_version_present=%u",
                request_emsg,
                GBE_kDotaCacheSubscribedUpToDate,
                static_cast<unsigned long long>(source_job),
                response_message.size(),
                static_cast<unsigned long long>(requested_owner_type),
                static_cast<unsigned long long>(requested_owner_id),
                GBE_local_lobby.has_cache_version ? 1u : 0u,
                GBE_local_lobby.has_cache_service_id ? 1u : 0u,
                GBE_local_lobby.cache_service_list.size(),
                GBE_local_lobby.has_cache_sync_version ? 1u : 0u
            );
            push_incoming_now(GBE_kDotaCacheSubscribedUpToDate | GBE_kProtoMask, response_message);
            return true;
        }
    }

    if (request_emsg == GBE_kDotaAbandonCurrentGame) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "handling req=%u source_job=%llu note=AbandonCurrentGame body_size=%zu active=%u lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu",
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
        return GBE_HandleDotaAbandonCurrentGameRequest(false, nullptr);
    }

    if (request_emsg == GBE_kDotaGameMatchSignOutPermissionRequest) {
        std::string response_message;
        if (!GBE_BuildDotaGameMatchSignOutPermissionResponsePayload(has_source_job, source_job, response_message)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, GBE_kDotaGameMatchSignOutPermissionResponse);
            return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu size=%zu note=signout permission granted",
            request_emsg,
            GBE_kDotaGameMatchSignOutPermissionResponse,
            static_cast<unsigned long long>(source_job),
            response_message.size()
        );
        push_incoming_now(GBE_kDotaGameMatchSignOutPermissionResponse | GBE_kProtoMask, response_message);
        return true;
    }

    if (request_emsg == GBE_kDotaGameMatchSignOut) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "handling req=%u source_job=%llu note=GameMatchSignOut body_size=%zu active=%u lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu",
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
        return GBE_HandleDotaGameMatchSignOutRequest(false, nullptr, has_source_job, source_job);
    }

    if (request_emsg == GBE_kDotaSubmitPlayerReportV2) {
        std::string response_message;
        if (!GBE_BuildDotaSubmitPlayerReportResponseV2Payload(body, body_size, has_source_job, source_job, response_message)) {
            GBE_GC_DebugLog("GC_DOTA_DIRECT", "failed building reply req=%u resp=%u", request_emsg, GBE_kDotaSubmitPlayerReportResponseV2);
            return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "replying req=%u resp=%u source_job=%llu size=%zu note=submit player report v2 success",
            request_emsg,
            GBE_kDotaSubmitPlayerReportResponseV2,
            static_cast<unsigned long long>(source_job),
            response_message.size()
        );
        push_incoming_now(GBE_kDotaSubmitPlayerReportResponseV2 | GBE_kProtoMask, response_message);
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

    if (request_emsg == GBE_kSteamTicketAuthComplete) {
        if (GBE_local_lobby.state == 1u && GBE_local_lobby.game_state == 0u && GBE_HasDotaLaunchServerSetupSync()) {
            if (GBE_TryAdvanceDotaLaunchToRun("runtime packet after 5429", request_emsg, source_job, "5429_launch_run"))
                return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "consumed req=%u source_job=%llu note=ticket auth complete body_size=%zu active=%u lobby_id=%llu state=%u game_state=%u match_id=%llu server_id=%llu",
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

    if (request_emsg == 8870) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "consumed req=%u source_job=%llu note=official 8870 launch marker without pending gate active=%u lobby_id=%llu state=%u game_state=%u",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            GBE_local_lobby.active ? 1u : 0u,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state
        );
        return true;
    }

    if (request_emsg == 4511) {
        uint64 lobby_id = 0;
        GBE_ExtractProtoFieldUint64(body, body_size, GBE_FindProtoField(body, body_size, 1u), lobby_id);

        const bool matches_local_lobby = (lobby_id != 0 && lobby_id == GBE_local_lobby.lobby_id);
        if (matches_local_lobby) {
            if (!GBE_local_lobby.launch_4511_seen) {
                GBE_local_lobby.launch_4511_seen = true;
                GBE_PublishSharedDotaLobbyState("4511_lan_server_available_seen");
            }
            GBE_TrySyncDotaLobbyServerIdFromGameServer("4511_lan_server_available");
        }

        if (matches_local_lobby && !incoming_messages.empty()) {
            GCMessageAvailable_t data{};
            data.m_nMessageSize = static_cast<uint32>(incoming_messages.front().msg_body.size());
            callbacks->addCBResult(data.k_iCallback, &data, sizeof(data), 0.0);
            GBE_GC_DebugLog(
                "GC_CALLBACK",
                "reposted GCMessageAvailable_t after 4511 lobby_id=%llu queued_emsg=%u queue_size=%zu size=%u",
                static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
                GBE_GC_MaskedEMsg(incoming_messages.front().msg_type),
                incoming_messages.size(),
                data.m_nMessageSize
            );
        }

        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "consumed req=%u source_job=%llu note=lan server available notification without launch gating lobby_id=%llu local_lobby_id=%llu matches_local=%u",
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

        const uint32 connect_ip = public_ip != 0 ? public_ip : private_ip;
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
        if (GBE_HasDotaLaunchServerSetupSync())
            GBE_MarkDotaLaunchPhase(GBE_kDotaLaunchPhaseSetupSynced, "4508_game_server_info");

        if (GBE_local_lobby.state == 1u && GBE_local_lobby.game_state == 0u && GBE_HasDotaLaunchServerSetupSync()) {
            if (GBE_TryAdvanceDotaLaunchToRun("runtime packet after 4508", request_emsg, source_job, "4508_launch_run"))
                return true;
        }

        return true;
    }

    if (request_emsg == GBE_kSteamGamesPlayedWithDataBlob && GBE_ShouldTrackDotaPracticeLobbyLateSteamChain()) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "consumed req=%u source_job=%llu note=late steam chain games played observed without synthetic followup active=%u lobby_id=%llu state=%u game_state=%u body_size=%zu body_prefix=%s",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            GBE_local_lobby.active ? 1u : 0u,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );
        return true;
    }

    if (request_emsg == GBE_kSteamAuthList && GBE_ShouldTrackDotaPracticeLobbyLateSteamChain()) {
        GBE_GC_DebugLog(
            "GC_DOTA_DIRECT",
            "consumed req=%u source_job=%llu note=late steam chain auth list observed without synthetic followup active=%u lobby_id=%llu state=%u game_state=%u body_size=%zu body_prefix=%s",
            request_emsg,
            static_cast<unsigned long long>(source_job),
            GBE_local_lobby.active ? 1u : 0u,
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            body_size,
            GBE_FormatHexPrefix(body, body_size, 48).c_str()
        );
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

    const bool mirror_source_job_to_target = has_source_job;

    std::string response_message;
    if (!GBE_BuildDotaDirectReplayMessage(
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
            GBE_FormatProtoTopLevelFieldSummary(response_body, response_body_size).c_str(),
            GBE_FormatHexPrefix(response_body, response_body_size, 32).c_str()
        );
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
    ResetGCMemory("7038_create", true, true);

    GBE_local_lobby = GBE_LocalLobby{};
    GBE_local_lobby.active = true;
    GBE_local_lobby.lobby_id = GBE_GenerateDotaLobbyId();
    GBE_local_lobby.lan = true;
    GBE_local_lobby.fill_with_bots = true;
    GBE_local_lobby.bot_difficulty_dire = 4;
    GBE_local_lobby.owner_steam_id = settings->get_local_steam_id().ConvertToUint64();
    GBE_local_lobby.owner_account_id = settings->get_local_steam_id().GetAccountID();
    GBE_local_lobby.owner_name = std::string(settings->get_local_name());
    GBE_local_lobby.owner_team = GBE_kDotaTeamGoodGuys;
    GBE_local_lobby.owner_slot = 1;
    GBE_DotaLobbyMemberState owner_member{};
    owner_member.steam_id = GBE_local_lobby.owner_steam_id;
    owner_member.account_id = GBE_local_lobby.owner_account_id;
    owner_member.team = GBE_local_lobby.owner_team;
    owner_member.slot = GBE_local_lobby.owner_slot;
    owner_member.connected = false;
    GBE_UpsertDotaLobbyMember(GBE_local_lobby.members, owner_member);

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

    Steam_Client *steam_client = get_steam_client();
    if (steam_client && steam_client->steam_matchmaking) {
        CSteamID generic_lobby_id = steam_client->steam_matchmaking->CreateLobbyImmediate(k_ELobbyTypeInvisible, 10);
        if (generic_lobby_id.IsLobby())
            GBE_local_lobby.generic_lobby_id = generic_lobby_id.ConvertToUint64();
    }

    GBE_SyncSettingsLobbyFromGenericLobby("7038_create");
    GBE_PublishSharedDotaLobbyState("7038_create");
    GBE_PublishDotaPracticeLobbyMetadata("7038_create");

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] State creating path=%s request_job=%llu NewLobbyID=%llu GenericLobbyID=%llu room=%s server_region=%u lan=%u lan_ping=%s mode=%u pass_len=%zu",
        wrapped ? "wrapped" : "direct",
        static_cast<unsigned long long>(request_job_id),
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
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

        GBE_RecordDotaLobbyCacheSubscriptionState(response_24, "7038_create_wrapped");
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
        GBE_RecordDotaLobbyCacheSubscriptionState(response_24, "7038_create_direct");
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

bool Steam_Game_Coordinator::GBE_HandleDotaLobbyListRequest(bool has_request_job, uint64 request_job_id, bool wrapped, const std::string *outer_session_field_raw)
{
    const bool finishing_leave = GBE_local_lobby.pending_leave_after_7040 && GBE_local_lobby.pending_leave_lobby_id != 0;
    const uint64 leaving_lobby_id = GBE_local_lobby.pending_leave_lobby_id;

    std::string response_25;
    if (finishing_leave && !GBE_BuildDotaLobbyCacheUnsubscribedPayload(leaving_lobby_id, response_25)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building delayed 25 for 7040 LobbyID=%llu", static_cast<unsigned long long>(leaving_lobby_id));
        return true;
    }

    std::vector<GBE_LocalLobby> lobby_snapshots;
    if (!finishing_leave)
        lobby_snapshots = GBE_GetDotaGenericLobbySnapshots("8011_lobby_list");
    if (!finishing_leave && GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0) {
        const uint64 local_lobby_id = GBE_local_lobby.lobby_id;
        const bool already_included = std::any_of(lobby_snapshots.begin(), lobby_snapshots.end(), [local_lobby_id](const GBE_LocalLobby &snapshot) {
            return snapshot.lobby_id == local_lobby_id;
        });
        if (!already_included)
            lobby_snapshots.push_back(GBE_local_lobby);
    }

    std::vector<std::string> entries;
    for (const GBE_LocalLobby &snapshot : lobby_snapshots) {
        entries.push_back(GBE_BuildDotaPracticeLobbyListEntryBody(
            snapshot.lobby_id,
            snapshot.owner_account_id != 0u ? snapshot.owner_account_id : settings->get_local_steam_id().GetAccountID(),
            snapshot.owner_name.empty() ? std::string(settings->get_local_name()) : snapshot.owner_name,
            snapshot.room_name,
            snapshot.game_mode,
            snapshot.server_region,
            !snapshot.pass_key.empty(),
            1u,
            10u,
            snapshot.lan_host_ping_location));
    }

    std::string response_8012;
    if (!GBE_BuildDotaLobbyListResponsePayload(entries, response_8012)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 8012 lobby list response active=%u finishing_leave=%u", GBE_local_lobby.active ? 1u : 0u, finishing_leave ? 1u : 0u);
        return true;
    }

    if (wrapped && !outer_session_field_raw) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 8012 lobby list");
        return true;
    }

    if (finishing_leave) {
        if (wrapped) {
            std::string wrapped_25;
            if (GBE_BuildWrappedDotaReplayMessage(response_25, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_25))
                push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_25);
        } else {
            push_incoming_now(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25);
        }
        ResetGCMemory("7040_leave_after_lobby_list", true, false);
    }

    if (wrapped) {
        std::string wrapped_8012;
        if (!GBE_BuildWrappedDotaReplayMessage(response_8012, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_8012)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 8012 lobby list response");
            return true;
        }
        push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_8012);
    } else {
        push_incoming_now(GBE_kDotaLobbyListResponse | GBE_kProtoMask, response_8012);
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Sent lobby list response 8012 entries=%zu finishing_leave=%u request_job=%llu has_job=%u wrapped=%d",
        entries.size(),
        finishing_leave ? 1u : 0u,
        static_cast<unsigned long long>(request_job_id),
        has_request_job ? 1u : 0u,
        wrapped ? 1 : 0);
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaFriendPracticeLobbyListRequest(bool wrapped, const std::string *outer_session_field_raw)
{
    std::vector<std::string> entries;
    const std::vector<GBE_LocalLobby> lobby_snapshots = GBE_GetDotaGenericLobbySnapshots("7111_friend_lobby_list");
    for (const GBE_LocalLobby &snapshot : lobby_snapshots) {
        entries.push_back(GBE_BuildDotaPracticeLobbyListEntryBody(
            snapshot.lobby_id,
            snapshot.owner_account_id != 0u ? snapshot.owner_account_id : settings->get_local_steam_id().GetAccountID(),
            snapshot.owner_name.empty() ? std::string(settings->get_local_name()) : snapshot.owner_name,
            snapshot.room_name,
            snapshot.game_mode,
            snapshot.server_region,
            !snapshot.pass_key.empty(),
            1u,
            10u,
            snapshot.lan_host_ping_location));
    }

    std::string response_7112;
    if (!GBE_BuildDotaFriendPracticeLobbyListResponsePayload(entries, response_7112)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7112 friend practice lobby list response");
        return true;
    }

    if (wrapped) {
        if (!outer_session_field_raw) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 7112 friend lobby list");
            return true;
        }

        std::string wrapped_7112;
        if (!GBE_BuildWrappedDotaReplayMessage(response_7112, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_7112)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 7112 friend lobby list response");
            return true;
        }
        push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_7112);
    } else {
        push_incoming_now(GBE_kDotaFriendPracticeLobbyListResponse | GBE_kProtoMask, response_7112);
    }

    GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Sent friend practice lobby list response 7112 entries=%zu wrapped=%d", entries.size(), wrapped ? 1 : 0);
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyJoinRequest(const std::string &request_body, uint64 request_job_id, bool has_request_job, bool wrapped, const std::string *outer_session_field_raw, bool send_join_response)
{
    GBE_DotaPracticeLobbyJoinRequest request{};
    if (!GBE_ParseDotaPracticeLobbyJoinBody(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7044 body_size=%zu body_prefix=%s",
            request_body.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), 48).c_str());
        return true;
    }

    CSteamID matched_generic_lobby_id = k_steamIDNil;
    GBE_LocalLobby matched_lobby{};
    const bool matched_generic_lobby = request.has_lobby_id && request.lobby_id != 0 &&
        GBE_FindDotaGenericLobbyByDotaLobbyId(request.lobby_id, matched_generic_lobby_id, &matched_lobby, "7044_join");

    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_local_lobby = GBE_LocalLobby{};
        GBE_local_lobby.active = true;
        GBE_local_lobby.lobby_id = request.has_lobby_id && request.lobby_id != 0 ? request.lobby_id : GBE_GenerateDotaLobbyId();
        GBE_local_lobby.owner_steam_id = settings->get_local_steam_id().ConvertToUint64();
        GBE_local_lobby.owner_account_id = settings->get_local_steam_id().GetAccountID();
        GBE_local_lobby.owner_name = std::string(settings->get_local_name());
        GBE_local_lobby.owner_team = GBE_kDotaTeamGoodGuys;
        GBE_local_lobby.owner_slot = 1u;
        GBE_local_lobby.game_mode = 2u;
        GBE_local_lobby.server_region = 15u;
        GBE_local_lobby.allow_spectating = true;
        GBE_local_lobby.bot_difficulty_dire = 4u;
        GBE_local_lobby.room_name = "Lobby";
    } else if (request.has_lobby_id && request.lobby_id != 0) {
        GBE_local_lobby.lobby_id = request.lobby_id;
    }

    if (matched_generic_lobby) {
        GBE_local_lobby.generic_lobby_id = matched_generic_lobby_id.ConvertToUint64();
        GBE_local_lobby.room_name = matched_lobby.room_name;
        GBE_local_lobby.game_mode = matched_lobby.game_mode;
        GBE_local_lobby.server_region = matched_lobby.server_region;
        GBE_local_lobby.lan = matched_lobby.lan;
        GBE_local_lobby.lan_host_ping_location = matched_lobby.lan_host_ping_location;
        GBE_local_lobby.pass_key = matched_lobby.pass_key;
        GBE_local_lobby.owner_account_id = matched_lobby.owner_account_id;
        GBE_local_lobby.owner_name = matched_lobby.owner_name;
        if (matched_lobby.owner_steam_id != 0ull)
            GBE_local_lobby.owner_steam_id = matched_lobby.owner_steam_id;
        GBE_local_lobby.owner_team = matched_lobby.owner_team;
        GBE_local_lobby.owner_slot = matched_lobby.owner_slot;
        GBE_local_lobby.members = matched_lobby.members;
        GBE_DotaLobbyMemberState owner_member{};
        owner_member.steam_id = GBE_local_lobby.owner_steam_id;
        owner_member.account_id = GBE_local_lobby.owner_account_id;
        owner_member.team = GBE_local_lobby.owner_team;
        owner_member.slot = GBE_local_lobby.owner_slot;
        owner_member.hero_id = GBE_local_lobby.owner_hero_id;
        owner_member.connected = GBE_local_lobby.owner_connected || GBE_local_lobby.state == 3u;
        GBE_UpsertDotaLobbyMember(GBE_local_lobby.members, owner_member);
        Steam_Client *steam_client = get_steam_client();
        if (steam_client && steam_client->steam_matchmaking)
            steam_client->steam_matchmaking->JoinLobby(matched_generic_lobby_id);
        GBE_SyncSettingsLobbyFromGenericLobby("7044_join_generic");
    }

    GBE_DotaLobbyMemberState local_member{};
    local_member.steam_id = settings->get_local_steam_id().ConvertToUint64();
    local_member.account_id = settings->get_local_steam_id().GetAccountID();
    local_member.team = GBE_kDotaTeamPlayerPool;
    local_member.slot = 0u;
    local_member.connected = false;
    for (const GBE_DotaLobbyMemberState &member : matched_lobby.members) {
        if (member.steam_id == local_member.steam_id) {
            GBE_local_lobby.seen_local_in_generic_lobby = true;
            break;
        }
    }
    GBE_UpsertDotaLobbyMember(GBE_local_lobby.members, local_member);

    if (request.has_pass_key)
        GBE_local_lobby.pass_key = request.pass_key;
    GBE_PublishDotaPracticeLobbyLocalMemberData("7044_join");
    GBE_PublishSharedDotaLobbyState("7044_join");

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Preparing 7044 cache update LobbyID=%llu owner_steam=%llu local_steam=%llu members=%zu matched_generic_members=%zu",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.owner_steam_id),
        static_cast<unsigned long long>(local_member.steam_id),
        GBE_local_lobby.members.size(),
        matched_lobby.members.size()
    );

    std::string response_24;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyCacheSubscribed(GBE_local_lobby, GBE_GetDotaLobbyOwnerName(), response_24)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 24 cache update for 7044 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    std::string response_7113;
    if (send_join_response) {
        if (!GBE_BuildDotaPracticeLobbyJoinResponsePayload(has_request_job, request_job_id, 0u, response_7113)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7113 join response LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
            return true;
        }
    }

    if (wrapped) {
        if (!outer_session_field_raw) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 7044 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
            return true;
        }

        std::string wrapped_24;
        std::string wrapped_7113;
        if (!GBE_BuildWrappedDotaReplayMessage(response_24, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_24) ||
            (send_join_response && !GBE_BuildWrappedDotaReplayMessage(response_7113, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_7113))) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 7044 responses LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
            return true;
        }
        GBE_RecordDotaLobbyCacheSubscriptionState(response_24, "7044_join_wrapped");
        push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_24);
        if (send_join_response)
            push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_7113);
    } else {
        GBE_RecordDotaLobbyCacheSubscriptionState(response_24, "7044_join_direct");
        push_incoming_now(GBE_kDotaCacheSubscribed | GBE_kProtoMask, response_24);
        if (send_join_response)
            push_incoming_now(GBE_kDotaPracticeLobbyJoinResponse | GBE_kProtoMask, response_7113);
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Joined practice lobby via 7044 LobbyID=%llu generic_lobby_id=%llu matched_generic=%u request_job=%llu has_job=%u pass_len=%zu wrapped=%d",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        matched_generic_lobby ? 1u : 0u,
        static_cast<unsigned long long>(request_job_id),
        has_request_job ? 1u : 0u,
        GBE_local_lobby.pass_key.size(),
        wrapped ? 1 : 0);
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaInviteToLobbyRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    GBE_DotaInviteToLobbyRequest request{};
    if (!GBE_ParseDotaInviteToLobbyBody(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 4512 body_size=%zu body_prefix=%s",
            request_body.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), 48).c_str());
        return true;
    }

    uint64 dota_lobby_id = GBE_local_lobby.lobby_id;
    CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
    if ((!GBE_local_lobby.active || dota_lobby_id == 0 || !generic_lobby_id.IsLobby()) && GBE_shared_dota_lobby_state.valid) {
        dota_lobby_id = GBE_shared_dota_lobby_state.lobby_id;
        generic_lobby_id = CSteamID((uint64)GBE_shared_dota_lobby_state.generic_lobby_id);
    }

    Steam_Client *steam_client = get_steam_client();
    bool sent_invite = false;
    bool sent_lobby_snapshot = false;
    if (steam_client && steam_client->steam_matchmaking && generic_lobby_id.IsLobby() && request.steam_id != 0) {
        sent_lobby_snapshot = steam_client->steam_matchmaking->SendLobbySnapshotToUserForDotaInvite(generic_lobby_id, CSteamID((uint64)request.steam_id));
        sent_invite = steam_client->steam_matchmaking->InviteUserToLobby(generic_lobby_id, CSteamID((uint64)request.steam_id));
    }

    bool sent_dota_invite = false;
    if (sent_invite && sent_lobby_snapshot && dota_lobby_id != 0)
        sent_dota_invite = true;

    std::string response_4502;
    if (!GBE_BuildDotaInvitationCreatedPayload(dota_lobby_id, request.steam_id, false, response_4502)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 4502 invite response dota_lobby_id=%llu", static_cast<unsigned long long>(dota_lobby_id));
        return true;
    }

    if (wrapped) {
        if (!outer_session_field_raw) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 4512 dota_lobby_id=%llu", static_cast<unsigned long long>(dota_lobby_id));
            return true;
        }

        std::string wrapped_4502;
        if (!GBE_BuildWrappedDotaReplayMessage(response_4502, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_4502)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 4502 invite response dota_lobby_id=%llu", static_cast<unsigned long long>(dota_lobby_id));
            return true;
        }
        push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_4502);
    } else {
        push_incoming_now(GBE_kGCInvitationCreated | GBE_kProtoMask, response_4502);
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Processed 4512 invite dota_lobby_id=%llu generic_lobby_id=%llu invitee=%llu client_version=%u sent_invite=%u sent_lobby_snapshot=%u sent_dota_invite=%u wrapped=%d",
        static_cast<unsigned long long>(dota_lobby_id),
        static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64()),
        static_cast<unsigned long long>(request.steam_id),
        request.has_client_version ? request.client_version : 0u,
        sent_invite ? 1u : 0u,
        sent_lobby_snapshot ? 1u : 0u,
        sent_dota_invite ? 1u : 0u,
        wrapped ? 1 : 0);
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaLobbyInviteResponseRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    GBE_DotaLobbyInviteResponseRequest request{};
    if (!GBE_ParseDotaLobbyInviteResponseBody(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 4513 body_size=%zu body_prefix=%s",
            request_body.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), 48).c_str());
        return true;
    }

    CSteamID matched_generic_lobby_id = k_steamIDNil;
    GBE_LocalLobby matched_lobby{};
    bool matched_generic_lobby = request.has_lobby_id && request.lobby_id != 0 &&
        GBE_FindDotaGenericLobbyByDotaLobbyId(request.lobby_id, matched_generic_lobby_id, &matched_lobby, "4513_invite_accept");

    Steam_Client *steam_client = get_steam_client();
    if (!matched_generic_lobby && steam_client && steam_client->steam_matchmaking && request.has_lobby_id && request.lobby_id != 0) {
        matched_generic_lobby_id = steam_client->steam_matchmaking->FindLobbyByDotaLobbyIdForInvite(
            request.lobby_id,
            GBE_kDotaGenericLobbyMarkerKey,
            GBE_kDotaGenericLobbyMarkerValue,
            GBE_kDotaGenericLobbyDotaLobbyIdKey);
        if (matched_generic_lobby_id.IsLobby()) {
            steam_client->steam_matchmaking->JoinLobby(matched_generic_lobby_id);
            steam_client->steam_matchmaking->RefreshLobbyCallbacksForDota();
            matched_generic_lobby = GBE_FindDotaGenericLobbyByDotaLobbyId(request.lobby_id, matched_generic_lobby_id, &matched_lobby, "4513_invite_accept_local_find");
        }
    }

    if (request.has_accept && !request.accept) {
        std::string response_remove_2011;
        if (request.has_lobby_id && request.lobby_id != 0 && GBE_BuildDotaRemoveLobbyInvitePayload(request.lobby_id, settings->get_local_steam_id().ConvertToUint64(), response_remove_2011)) {
            if (wrapped) {
                if (outer_session_field_raw) {
                    std::string wrapped_remove_2011;
                    if (GBE_BuildWrappedDotaReplayMessage(response_remove_2011, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_remove_2011))
                        push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_remove_2011);
                }
            } else {
                push_incoming_now(GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask, response_remove_2011);
            }
        }

        std::string response_25;
        if (GBE_BuildDotaSOOwnerCacheUnsubscribedPayload(4u, settings->get_local_steam_id().ConvertToUint64(), response_25)) {
            if (wrapped) {
                if (outer_session_field_raw) {
                    std::string wrapped_25;
                    if (GBE_BuildWrappedDotaReplayMessage(response_25, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_25))
                        push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_25);
                }
            } else {
                push_incoming_now(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25);
            }
        }

        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Processed declined 4513 invite response lobby_id=%llu matched_generic=%u generic_lobby_id=%llu client_version=%u wrapped=%d",
            static_cast<unsigned long long>(request.lobby_id),
            matched_generic_lobby ? 1u : 0u,
            static_cast<unsigned long long>(matched_generic_lobby_id.ConvertToUint64()),
            request.has_client_version ? request.client_version : 0u,
            wrapped ? 1 : 0);
        return true;
    }

    std::string join_body;
    if (request.has_lobby_id)
        GBE_AppendProtoVarIntField(join_body, 1u, request.lobby_id);

    if (!GBE_HandleDotaPracticeLobbyJoinRequest(join_body, 0u, false, wrapped, outer_session_field_raw, false))
        return false;

    std::string response_remove_2011;
    if (request.has_lobby_id && request.lobby_id != 0 && GBE_BuildDotaRemoveLobbyInvitePayload(request.lobby_id, settings->get_local_steam_id().ConvertToUint64(), response_remove_2011)) {
        if (wrapped) {
            if (outer_session_field_raw) {
                std::string wrapped_remove_2011;
                if (GBE_BuildWrappedDotaReplayMessage(response_remove_2011, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_remove_2011))
                    push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_remove_2011);
            }
        } else {
            push_incoming_now(GBE_kDotaPracticeLobbyDetailsUpdate | GBE_kProtoMask, response_remove_2011);
        }
    }

    std::string response_25;
    if (GBE_BuildDotaSOOwnerCacheUnsubscribedPayload(4u, settings->get_local_steam_id().ConvertToUint64(), response_25)) {
        if (wrapped) {
            if (outer_session_field_raw) {
                std::string wrapped_25;
                if (GBE_BuildWrappedDotaReplayMessage(response_25, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_25))
                    push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_25);
            }
        } else {
            push_incoming_now(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25);
        }
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Processed 4513 invite response lobby_id=%llu accept=%u matched_generic=%u generic_lobby_id=%llu client_version=%u wrapped=%d",
        static_cast<unsigned long long>(request.lobby_id),
        (!request.has_accept || request.accept) ? 1u : 0u,
        matched_generic_lobby ? 1u : 0u,
        static_cast<unsigned long long>(matched_generic_lobby_id.ConvertToUint64()),
        request.has_client_version ? request.client_version : 0u,
        wrapped ? 1 : 0);
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

    if (GBE_local_lobby.generic_lobby_id != 0) {
        Steam_Client *steam_client = get_steam_client();
        if (steam_client && steam_client->steam_matchmaking)
            steam_client->steam_matchmaking->RefreshLobbyCallbacksForDota();
    }

    GBE_PublishSharedDotaLobbyState("7009_join_chat");

    GBE_LocalLobby lobby_snapshot{};
    if (!GBE_CaptureCurrentDotaLobbyState("7009_join_chat", lobby_snapshot))
        lobby_snapshot = GBE_local_lobby;

    std::string response_7010;
    if (!GBE_BuildDotaJoinChatChannelResponsePayload(
            settings->get_local_steam_id().ConvertToUint64(),
            lobby_snapshot.generic_lobby_id,
            lobby_snapshot.chat_channel_id,
            lobby_snapshot.chat_channel_name,
            std::string(settings->get_local_name()),
            lobby_snapshot.members,
            lobby_snapshot.owner_steam_id,
            lobby_snapshot.owner_name,
            lobby_snapshot.chat_channel_type,
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
        "[LOBBY] Chat channel joined. name=%s channel_id=%llu channel_type=%u members=%zu wrapped=%d",
        GBE_local_lobby.chat_channel_name.c_str(),
        static_cast<unsigned long long>(GBE_local_lobby.chat_channel_id),
        GBE_local_lobby.chat_channel_type,
        lobby_snapshot.members.size(),
        wrapped ? 1 : 0
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaChatMessageRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7273 because no local lobby is active");
        return true;
    }

    GBE_DotaChatMessageRequest request{};
    if (!GBE_ParseDotaChatMessageBody(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7273 body_size=%zu body_prefix=%s",
            request_body.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    const uint64 channel_id = request.has_channel_id ? request.channel_id : GBE_local_lobby.chat_channel_id;
    const uint32 account_id = request.has_account_id ? request.account_id : settings->get_local_steam_id().GetAccountID();
    const std::string persona_name = request.has_persona_name ? request.persona_name : std::string(settings->get_local_name());

    std::string chat_7273;
    if (!GBE_BuildDotaChatMessagePayload(request_body, request, channel_id, account_id, persona_name, chat_7273)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7273 chat payload channel_id=%llu", static_cast<unsigned long long>(channel_id));
        return true;
    }
    (void)wrapped;
    (void)outer_session_field_raw;

    if (network && GBE_local_lobby.generic_lobby_id != 0) {
        auto steam_message = new Steam_Messages();
        steam_message->set_type(Steam_Messages::FRIEND_CHAT);
        steam_message->set_message(chat_7273);

        Common_Message msg{};
        msg.set_source_id(settings->get_local_steam_id().ConvertToUint64());
        msg.set_allocated_steam_messages(steam_message);
        network->sendToAll(&msg, true);
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Chat message relayed. channel_id=%llu account_id=%u persona=%s text_size=%zu wrapped=%d local_echo=0",
        static_cast<unsigned long long>(channel_id),
        account_id,
        persona_name.c_str(),
        request.text.size(),
        wrapped ? 1 : 0
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaFriendLobbyInviteMessage(Common_Message *msg)
{
    if (!msg || !msg->has_friend_messages() || !gc_initialized || gc_profile != GC_PROFILE_DOTA2)
        return false;

    if (msg->friend_messages().type() != Friend_Messages::LOBBY_INVITE)
        return false;

    CSteamID generic_lobby_id((uint64)msg->friend_messages().lobby_id());
    if (!generic_lobby_id.IsLobby())
        return false;

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking)
        return false;

    steam_client->steam_matchmaking->RefreshLobbyCallbacksForDota();

    const char *marker = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyMarkerKey);
    if (!marker || std::strcmp(marker, GBE_kDotaGenericLobbyMarkerValue) != 0)
        return false;

    const uint64 dota_lobby_id = GBE_ParseUint64OrZero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyDotaLobbyIdKey));
    if (dota_lobby_id == 0) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Ignoring friend lobby invite without Dota lobby id generic_lobby_id=%llu source=%llu",
            static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64()),
            static_cast<unsigned long long>(msg->source_id())
        );
        return true;
    }

    uint64 owner_steam_id = GBE_ParseUint64OrZero(steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerSteamIdKey));
    if (owner_steam_id == 0)
        owner_steam_id = msg->source_id();
    const uint64 inviter_steam_id = msg->source_id() != 0 ? msg->source_id() : owner_steam_id;

    const char *owner_name_value = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyOwnerNameKey);
    const std::string owner_name = owner_name_value && owner_name_value[0] != '\0'
        ? std::string(owner_name_value)
        : std::string("Lobby Host");

    auto get_invite_member_name = [&](uint64 steam_id) -> std::string {
        CSteamID member_id((uint64)steam_id);
        const char *member_name = steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby_id, member_id, GBE_kDotaGenericLobbyMemberNameKey);
        if (member_name && member_name[0] != '\0')
            return std::string(member_name);
        if (steam_id == owner_steam_id || steam_id == inviter_steam_id)
            return owner_name;
        return std::string("Lobby Host");
    };

    std::vector<std::pair<uint64, std::string>> invite_members;
    const std::string inviter_name = get_invite_member_name(inviter_steam_id);
    if (inviter_steam_id != 0)
        invite_members.emplace_back(inviter_steam_id, inviter_name);
    for (const auto &member_id : steam_client->steam_matchmaking->GetLobbyMemberListSnapshot(generic_lobby_id)) {
        const uint64 member_steam_id = member_id.ConvertToUint64();
        if (member_steam_id == 0 || member_steam_id == inviter_steam_id)
            continue;
        invite_members.emplace_back(member_steam_id, get_invite_member_name(member_steam_id));
    }

    const char *room_name_value = steam_client->steam_matchmaking->GetLobbyData(generic_lobby_id, GBE_kDotaGenericLobbyRoomNameKey);
    const std::string room_name = room_name_value && room_name_value[0] != '\0'
        ? std::string(room_name_value)
        : owner_name;

    std::string invite_24;
    if (!GBE_BuildDotaLobbyInviteCacheSubscribedPayload(
            dota_lobby_id,
            inviter_steam_id,
            settings->get_local_steam_id().ConvertToUint64(),
            inviter_name,
            invite_members,
            invite_24)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed building 2011 from friend lobby invite dota_lobby_id=%llu generic_lobby_id=%llu source=%llu",
            static_cast<unsigned long long>(dota_lobby_id),
            static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64()),
            static_cast<unsigned long long>(msg->source_id())
        );
        return true;
    }

    push_incoming_now(GBE_kDotaCacheSubscribed | GBE_kProtoMask, invite_24);
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Created 2011 lobby invite from friend invite dota_lobby_id=%llu generic_lobby_id=%llu source=%llu owner=%llu members=%zu room='%s'",
        static_cast<unsigned long long>(dota_lobby_id),
        static_cast<unsigned long long>(generic_lobby_id.ConvertToUint64()),
        static_cast<unsigned long long>(msg->source_id()),
        static_cast<unsigned long long>(owner_steam_id),
        invite_members.size(),
        room_name.c_str()
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaNetworkLobbyInviteMessage(Common_Message *msg)
{
    if (!msg || !msg->has_steam_messages() || !gc_initialized || gc_profile != GC_PROFILE_DOTA2)
        return false;

    if (msg->steam_messages().type() != Steam_Messages::FRIEND_CHAT)
        return false;

    const std::string &message = msg->steam_messages().message();
    if (!GBE_IsDotaLobbyInviteCacheSubscribedPayload(message))
        return false;

    push_incoming_now(GBE_kDotaCacheSubscribed | GBE_kProtoMask, message);
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Received network 2011 lobby invite source=%llu size=%zu",
        static_cast<unsigned long long>(msg->source_id()),
        message.size()
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaNetworkChatMessage(Common_Message *msg)
{
    if (!msg || !msg->has_steam_messages() || !gc_initialized || gc_profile != GC_PROFILE_DOTA2)
        return false;

    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0)
        return false;

    if (msg->steam_messages().type() != Steam_Messages::FRIEND_CHAT)
        return false;

    const std::string &message = msg->steam_messages().message();
    if (message.size() < 8u)
        return false;

    uint32 inner_emsg = 0;
    std::memcpy(&inner_emsg, message.data(), sizeof(inner_emsg));
    if (GBE_GC_MaskedEMsg(inner_emsg) != GBE_kDotaChatMessage)
        return false;

    const size_t body_offset = 8u;
    if (message.size() <= body_offset)
        return false;

    GBE_DotaChatMessageRequest request{};
    if (!GBE_ParseDotaChatMessageBody(
            reinterpret_cast<const uint8 *>(message.data() + body_offset),
            message.size() - body_offset,
            request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Ignoring malformed network 7273 chat source=%llu size=%zu",
            static_cast<unsigned long long>(msg->source_id()),
            message.size()
        );
        return true;
    }

    std::string local_channel_body;
    if (!GBE_RewriteProtoVarIntFields(
            message.substr(body_offset),
            { 2u },
            GBE_local_lobby.chat_channel_id,
            local_channel_body))
        return false;

    std::string sender_name;
    const uint64 sender_steam_id = msg->source_id();
    uint32 sender_account_id = request.has_account_id ? request.account_id : 0u;
    if (sender_account_id == 0u && sender_steam_id != 0ull)
        sender_account_id = CSteamID((uint64)sender_steam_id).GetAccountID();
    if (!request.has_persona_name) {
        if (sender_steam_id == settings->get_local_steam_id().ConvertToUint64()) {
            sender_name = std::string(settings->get_local_name());
        } else {
            Steam_Client *steam_client = get_steam_client();
            if (steam_client && steam_client->steam_matchmaking && GBE_local_lobby.generic_lobby_id != 0ull) {
                CSteamID generic_lobby((uint64)GBE_local_lobby.generic_lobby_id);
                CSteamID sender_id((uint64)sender_steam_id);
                if (generic_lobby.IsLobby() && sender_id.IsValid()) {
                    const char *generic_name = steam_client->steam_matchmaking->GetLobbyMemberData(generic_lobby, sender_id, GBE_kDotaGenericLobbyMemberNameKey);
                    if (generic_name && generic_name[0] != '\0')
                        sender_name = std::string(generic_name);
                }
            }

            for (const GBE_DotaLobbyMemberState &member : GBE_local_lobby.members) {
                if (!sender_name.empty())
                    break;
                if (member.steam_id != sender_steam_id && member.account_id != sender_account_id)
                    continue;

                CSteamID sender_id((uint64)(member.steam_id != 0ull ? member.steam_id : sender_steam_id));
                if (steam_client && steam_client->steam_friends) {
                    const char *friend_name = steam_client->steam_friends->GetFriendPersonaName(sender_id);
                    if (friend_name && friend_name[0] != '\0' && std::string(friend_name) != "Unknown User")
                        sender_name = std::string(friend_name);
                }
                break;
            }
        }

        if (!sender_name.empty())
            GBE_AppendProtoBytesField(local_channel_body, 3u, sender_name);
    }

    std::string local_channel_message;
    if (!GBE_BuildDotaZeroHeaderPayload(GBE_kDotaChatMessage, local_channel_body, local_channel_message))
        return false;

    push_incoming_now(GBE_kDotaChatMessage | GBE_kProtoMask, local_channel_message);
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Received network 7273 chat source=%llu size=%zu account_id=%u remote_channel=%llu local_channel=%llu text_size=%zu persona=%s",
        static_cast<unsigned long long>(msg->source_id()),
        message.size(),
        sender_account_id,
        static_cast<unsigned long long>(request.channel_id),
        static_cast<unsigned long long>(GBE_local_lobby.chat_channel_id),
        request.text.size(),
        (request.has_persona_name ? request.persona_name : sender_name).c_str()
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaAbandonCurrentGameRequest(bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7035 because no local lobby is active");
        return true;
    }

    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    const uint32 lobby_state = GBE_local_lobby.state;
    const uint32 lobby_game_state = GBE_local_lobby.game_state;
    const bool treat_as_current_game_disconnect =
        is_server &&
        GBE_local_lobby.owner_connected &&
        lobby_state == 2u &&
        (GBE_local_lobby.server_id != 0 || lobby_game_state >= 1u);
    const bool ready_for_abandon_teardown =
        lobby_state == 2u &&
        lobby_game_state >= 2u;
    if (!ready_for_abandon_teardown) {
        if (treat_as_current_game_disconnect) {
            std::string response_25;
            if (!GBE_BuildDotaLobbyCacheUnsubscribedPayload(lobby_id, response_25)) {
                GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 payload for current-game 7035 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
                return true;
            }

            GBE_pending_reset_after_cache_unsubscribed = true;
            GBE_pending_reset_after_cache_unsubscribed_lobby_id = lobby_id;
            GBE_MarkDotaAbandonedLobbySuppressed(lobby_id, "7035_current_game_disconnect");
            push_incoming_now(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25);

            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Treated 7035 as current-game disconnect. queued 25 and deferred reset until retrieval LobbyID=%llu state=%u game_state=%u owner_connected=%u",
                static_cast<unsigned long long>(lobby_id),
                lobby_state,
                lobby_game_state,
                GBE_local_lobby.owner_connected ? 1u : 0u
            );
            return true;
        }

        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Ignoring early 7035 before launch reaches a current-game stage LobbyID=%llu state=%u game_state=%u match_id=%llu server_id=%llu",
            static_cast<unsigned long long>(lobby_id),
            lobby_state,
            lobby_game_state,
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id)
        );
        return true;
    }

    if (wrapped && !outer_session_field_raw) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 7035 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    GBE_DiscardQueuedDotaLaunchMessagesForAbandon("7035_ready_for_abandon_teardown");
    GBE_MarkDotaAbandonedLobbySuppressed(lobby_id, "7035_ready_for_abandon_teardown");

    if (!GBE_QueueDotaPostGameTeardown("7035_abandon_current_game", wrapped, outer_session_field_raw, true, true, true))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Processed 7035. sent 25 and postgame 7010 wrapped=%d LobbyID=%llu",
        wrapped ? 1 : 0,
        static_cast<unsigned long long>(lobby_id)
    );
    return true;
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
    if (!GBE_BuildDotaLobbyCacheUnsubscribedPayload(lobby_id, response_25)) {
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
    if (!GBE_BuildDotaPostGameJoinChatChannelResponsePayload(
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
        if (wrapped) {
            std::string wrapped_message;
            if (!GBE_BuildWrappedDotaReplayMessage(payload, *outer_session_field_raw, steam_id, wrapped_message)) {
                GBE_GC_DebugLog(
                    "GC_DOTA_LOBBY",
                    "[LOBBY] Failed wrapping %s payload for postgame teardown LobbyID=%llu reason=%s",
                    label,
                    static_cast<unsigned long long>(lobby_id),
                    reason ? reason : "unknown"
                );
                return false;
            }

            push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_message);
            return true;
        }

        push_incoming_now(direct_emsg | GBE_kProtoMask, payload);
        return true;
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
    if (GBE_BuildDotaPersonaStatePeripheralMessage(GBE_kDotaAbandonPersonaStatePrivateLobbyNoLobbyHex, steam_id, lobby_id, no_lobby_persona)) {
        push_incoming_now(GBE_kSteamPersonaState | GBE_kProtoMask, no_lobby_persona);
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "queued postgame persona label=private_lobby_no_lobby lobby_id=%llu size=%zu reason=%s",
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

bool Steam_Game_Coordinator::GBE_HandleDotaGameMatchSignOutRequest(bool wrapped, const std::string *outer_session_field_raw, bool has_request_job, uint64 request_job_id)
{
    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    const uint64 match_id = GBE_local_lobby.match_id;
    const uint32 game_start_time = GBE_local_lobby.game_start_time;
    const uint32 duration = game_start_time != 0u ? static_cast<uint32>(std::time(nullptr)) - game_start_time : 0u;

    std::string response_7005;
    if (!GBE_BuildDotaGameMatchSignOutResponsePayload(match_id, duration, has_request_job, request_job_id, response_7005)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 7005 signout response request_job=%llu has_job=%d", static_cast<unsigned long long>(request_job_id), has_request_job ? 1 : 0);
        return true;
    }

    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0) {
        if (GBE_local_lobby.game_state < 6u)
            GBE_local_lobby.game_state = 6u;
        if (GBE_local_lobby.state < 2u)
            GBE_local_lobby.state = 2u;
        GBE_PublishSharedDotaLobbyState("7004_signout_post_game");
        GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7004_signout_run_post_game");
    }

    if (wrapped) {
        if (!outer_session_field_raw) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 7004 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
            return true;
        }

        std::string wrapped_7005;
        if (!GBE_BuildWrappedDotaReplayMessage(response_7005, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_7005)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 7005 signout response LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
            return true;
        }

        push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_7005);
    } else {
        push_incoming_now(GBE_kDotaGameMatchSignOutResponse | GBE_kProtoMask, response_7005);
    }

    if (GBE_local_lobby.active && GBE_local_lobby.lobby_id != 0) {
        GBE_QueueDotaPostGameTeardown("7004_signout_postgame", wrapped, outer_session_field_raw, false, false, false);
        GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7004_signout_postgame_state");

        std::string response_25;
        if (GBE_BuildDotaLobbyCacheUnsubscribedPayload(lobby_id, response_25)) {
            if (wrapped) {
                if (!outer_session_field_raw) {
                    GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 25 after 7004 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
                } else {
                    std::string wrapped_25;
                    if (GBE_BuildWrappedDotaReplayMessage(response_25, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_25)) {
                        push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_25);
                    } else {
                        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 25 after 7004 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
                    }
                }
            } else {
                push_incoming_now(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, response_25);
            }
            GBE_pending_dota_normal_signout_finalize_after_25 = true;
            GBE_pending_dota_normal_signout_finalize_lobby_id = lobby_id;
        } else {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 after 7004 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
        }
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Processed 7004 signout. replied 7005 and queued postgame teardown wrapped=%d LobbyID=%llu match_id=%llu request_job=%llu has_job=%d",
        wrapped ? 1 : 0,
        static_cast<unsigned long long>(lobby_id),
        static_cast<unsigned long long>(match_id),
        static_cast<unsigned long long>(request_job_id),
        has_request_job ? 1 : 0
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyLeaveRequest(bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7040 because no local lobby is active");
        return true;
    }

    if (GBE_local_lobby.state == 2u) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Ignoring stale 7040 during active runtime lobby LobbyID=%llu state=%u game_state=%u match_id=%llu server_id=%llu wrapped=%d",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id),
            wrapped ? 1 : 0
        );
        return true;
    }

    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    std::string response_25;
    if (!GBE_BuildDotaLobbyCacheUnsubscribedPayload(lobby_id, response_25)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 25 payload for 7040 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
        return true;
    }

    std::string outbound_message;
    uint32 outbound_emsg = GBE_kDotaCacheUnsubscribed;

    if (wrapped) {
        if (!outer_session_field_raw) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 7040 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
            return true;
        }

        if (!GBE_BuildWrappedDotaReplayMessage(response_25, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), outbound_message)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 25 payload for 7040 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
            return true;
        }
        outbound_emsg = GBE_kEMsgClientFromGC;
    } else {
        outbound_message = response_25;
    }

    push_incoming_now(outbound_emsg | GBE_kProtoMask, outbound_message);
    ResetGCMemory("7040_leave", true, false);

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Lobby leave requested. sent 25 and left generic lobby LobbyID=%llu wrapped=%d",
        static_cast<unsigned long long>(lobby_id),
        wrapped ? 1 : 0
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyLaunchRequest(bool wrapped, const std::string *outer_session_field_raw, bool has_request_job, uint64 request_job_id)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7041 because no local lobby is active");
        return true;
    }

    if (wrapped && !outer_session_field_raw) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 7041 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    GBE_ResetDotaPracticeLobbyLaunchPeripheralState();

    GBE_local_lobby.match_id = GBE_GenerateDotaMatchId();
    GBE_local_lobby.server_id = GBE_BuildDotaPracticeLobbyIpServerId(network ? network->getOwnIP() : 0u);
    {
        const std::string launch_connect = GBE_FormatDotaPracticeLobbyConnectFromIp(network ? network->getOwnIP() : 0);
        if (GBE_ShouldPreferDotaLobbyConnectUpdate(GBE_local_lobby.connect, launch_connect))
            GBE_local_lobby.connect = launch_connect;
    }
    GBE_local_lobby.game_start_time = static_cast<uint32>(std::time(nullptr));
    GBE_local_lobby.launch_phase = GBE_kDotaLaunchPhaseRequested;
    GBE_PublishSharedDotaLobbyState("7041_launch_init");

    std::string stage1_message;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(GBE_local_lobby, GBE_local_lobby.owner_name, stage1_message, true)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed building initial 26 after 7041 LobbyID=%llu match_id=%llu server_id=%llu connect=%s",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id),
            GBE_local_lobby.connect.c_str()
        );
        return true;
    }

    if (wrapped) {
        std::string wrapped_message;
        if (!GBE_BuildWrappedDotaReplayMessage(stage1_message, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_message)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping initial 26 after 7041 LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
            return true;
        }
        stage1_message.swap(wrapped_message);
    }

    push_incoming_now(
        (wrapped ? GBE_kEMsgClientFromGC : GBE_kDotaPracticeLobbyDetailsUpdate) | GBE_kProtoMask,
        stage1_message,
        true,
        1u,
        0u
    );

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Sent initial 26 after 7041 path=%s has_request_job=%d request_job=%llu LobbyID=%llu match_id=%llu server_id=%llu game_start=%u connect=%s size=%zu body_prefix=%s",
        wrapped ? "wrapped" : "direct",
        has_request_job ? 1 : 0,
        static_cast<unsigned long long>(request_job_id),
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id),
        GBE_local_lobby.game_start_time,
        GBE_local_lobby.connect.c_str(),
        stage1_message.size(),
        GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(stage1_message.data()), stage1_message.size(), 32).c_str()
    );

    GBE_UpdateDotaPracticeLobbyLaunchRichPresence("#DOTA_RP_INIT", "SERVERSETUP", false);
    GBE_MaybeQueueDotaPracticeLobbyLaunchPersonaState("#DOTA_RP_INIT", "SERVERSETUP", false, true, "7041_launch_init");

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Deferring remaining 7041 launch follow-ups until server_id sync LobbyID=%llu match_id=%llu server_id=%llu",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        static_cast<unsigned long long>(GBE_local_lobby.server_id)
    );

    return true;
}

bool Steam_Game_Coordinator::GBE_SendDotaPracticeLobbyDetailsUpdate(bool wrapped, const std::string *outer_session_field_raw, const char *reason)
{
    GBE_LocalLobby lobby{};
    if (!GBE_CaptureCurrentDotaLobbyState(reason ? reason : "details_update", lobby))
        return false;

    std::string response_26;
    if (!GBE_BuildAuthoritativeDotaPracticeLobbyDetailsUpdate(lobby, GBE_GetDotaLobbyOwnerName(), response_26)) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 26 details update for LobbyID=%llu reason=%s", static_cast<unsigned long long>(lobby.lobby_id), reason ? reason : "unknown");
        return false;
    }

    if (wrapped) {
        if (!outer_session_field_raw) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 26 LobbyID=%llu reason=%s", static_cast<unsigned long long>(lobby.lobby_id), reason ? reason : "unknown");
            return false;
        }

        std::string wrapped_26;
        if (!GBE_BuildWrappedDotaReplayMessage(response_26, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_26)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 26 details update for LobbyID=%llu reason=%s", static_cast<unsigned long long>(lobby.lobby_id), reason ? reason : "unknown");
            return false;
        }

        push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_26);
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Sent wrapped 26 details update LobbyID=%llu reason=%s size=%zu body_prefix=%s packet_prefix=%s",
            static_cast<unsigned long long>(lobby.lobby_id),
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
            static_cast<unsigned long long>(lobby.lobby_id),
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
    GBE_PublishDotaPracticeLobbyMetadata("7046_set_details");

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

    const uint64 local_steam_id = settings->get_local_steam_id().ConvertToUint64();
    const bool local_is_owner = local_steam_id != 0ull && local_steam_id == GBE_local_lobby.owner_steam_id;
    if (local_is_owner) {
        if (request.has_team)
            GBE_local_lobby.owner_team = request.team;
        if (request.has_slot)
            GBE_local_lobby.owner_slot = request.slot;
    }
    bool updated_local_member = false;
    for (GBE_DotaLobbyMemberState &member : GBE_local_lobby.members) {
        if (member.steam_id != local_steam_id)
            continue;
        if (request.has_team)
            member.team = request.team;
        if (request.has_slot)
            member.slot = request.slot;
        updated_local_member = true;
        break;
    }
    if (!updated_local_member) {
        GBE_DotaLobbyMemberState member{};
        member.steam_id = local_steam_id;
        member.account_id = settings->get_local_steam_id().GetAccountID();
        member.team = request.has_team ? request.team : GBE_kDotaTeamPlayerPool;
        member.slot = request.has_slot ? request.slot : 0u;
        member.connected = GBE_local_lobby.state == 3u;
        GBE_UpsertDotaLobbyMember(GBE_local_lobby.members, member);
    }
    if (request.has_bot_difficulty) {
        const uint32 bot_team = request.has_team ? request.team : GBE_local_lobby.owner_team;
        if (GBE_IsDotaDireTeam(bot_team))
            GBE_local_lobby.bot_difficulty_dire = request.bot_difficulty;
        else
            GBE_local_lobby.bot_difficulty_radiant = request.bot_difficulty;
    }
    GBE_PublishDotaPracticeLobbyLocalMemberData("7047_set_team_slot");
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
        "[LOBBY] Team slot updated. local_team=%u local_slot=%u owner_team=%u owner_slot=%u bot_diff_req=%u has_bot_diff=%d",
        request.has_team ? request.team : 0u,
        request.has_slot ? request.slot : 0u,
        GBE_local_lobby.owner_team,
        GBE_local_lobby.owner_slot,
        request.bot_difficulty,
        request.has_bot_difficulty ? 1 : 0
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaPracticeLobbyKickRequest(const std::string &request_body, bool wrapped, const std::string *outer_session_field_raw)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.generic_lobby_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7081 because no local lobby is active");
        return true;
    }

    const uint64 local_steam_id = settings->get_local_steam_id().ConvertToUint64();
    if (local_steam_id != GBE_local_lobby.owner_steam_id) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7081 because local user is not Dota lobby owner LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    GBE_DotaPracticeLobbyKickRequest request{};
    if (!GBE_ParseDotaPracticeLobbyKickBody(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), request)) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed parsing 7081 body_size=%zu body_prefix=%s",
            request_body.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(request_body.data()), request_body.size(), 48).c_str()
        );
        return true;
    }

    if (request.account_id == 0u || request.account_id == settings->get_local_steam_id().GetAccountID()) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7081 invalid target account_id=%u LobbyID=%llu", request.account_id, static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    uint64 kicked_steam_id = 0ull;
    for (const GBE_DotaLobbyMemberState &member : GBE_local_lobby.members) {
        const uint32 member_account_id = member.account_id != 0u ? member.account_id : CSteamID((uint64)member.steam_id).GetAccountID();
        if (member.steam_id != 0ull && member_account_id == request.account_id) {
            kicked_steam_id = member.steam_id;
            break;
        }
    }

    if (kicked_steam_id == 0ull) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7081 because target account_id=%u is not in Dota lobby LobbyID=%llu", request.account_id, static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_matchmaking) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7081 because matchmaking is unavailable LobbyID=%llu", static_cast<unsigned long long>(GBE_local_lobby.lobby_id));
        return true;
    }

    GBE_LocalLobby before_lobby = GBE_local_lobby;
    for (GBE_DotaLobbyMemberState &member : before_lobby.members) {
        const uint32 member_account_id = member.account_id != 0u ? member.account_id : CSteamID((uint64)member.steam_id).GetAccountID();
        if (member.steam_id != 0ull && member_account_id == request.account_id) {
            member = GBE_DotaLobbyMemberState{};
            break;
        }
    }

    CSteamID generic_lobby_id((uint64)GBE_local_lobby.generic_lobby_id);
    const bool kicked = steam_client->steam_matchmaking->KickLobbyMemberForDota(generic_lobby_id, CSteamID((uint64)kicked_steam_id));
    if (!kicked) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Failed kicking generic lobby member for 7081 LobbyID=%llu generic_lobby_id=%llu target_account=%u target_steam=%llu",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
            request.account_id,
            static_cast<unsigned long long>(kicked_steam_id)
        );
        return true;
    }

    GBE_local_lobby = before_lobby;
    GBE_PublishSharedDotaLobbyState("7081_kick_member");

    if (!GBE_SendDotaPracticeLobbyDetailsUpdate(wrapped, outer_session_field_raw, "7081"))
        return true;

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Kicked practice lobby member target_account=%u target_steam=%llu LobbyID=%llu generic_lobby_id=%llu wrapped=%d",
        request.account_id,
        static_cast<unsigned long long>(kicked_steam_id),
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.generic_lobby_id),
        wrapped ? 1 : 0
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

    const uint64 local_channel_id = GBE_local_lobby.chat_channel_id;
    const uint64 channel_id = request.channel_id != 0 ? request.channel_id : local_channel_id;
    const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    const uint64 pre_postgame_channel_id = GBE_local_lobby.abandon_pre_postgame_chat_channel_id;
    const bool leaving_postgame_channel =
        GBE_local_lobby.abandon_postgame_active &&
        GBE_local_lobby.has_chat_channel &&
        local_channel_id != 0 &&
        GBE_local_lobby.chat_channel_type == 18u;
    const bool matches_current_postgame_channel = channel_id == local_channel_id;
    const bool matches_pre_postgame_channel = pre_postgame_channel_id != 0 && channel_id == pre_postgame_channel_id;
    if (!GBE_local_lobby.active || !GBE_local_lobby.has_chat_channel) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Replying 7014 for stale 7272 after lobby reset without restoring lobby state. request_channel=%llu active=%u has_chat=%u local_channel=%llu",
            static_cast<unsigned long long>(channel_id),
            GBE_local_lobby.active ? 1u : 0u,
            GBE_local_lobby.has_chat_channel ? 1u : 0u,
            static_cast<unsigned long long>(local_channel_id)
        );

        if (channel_id == 0) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring stale 7272 after lobby reset because no request channel is available");
            return true;
        }

        std::string stale_response_7014;
        if (!GBE_BuildDotaOtherLeftChannelPayload(channel_id, settings->get_local_steam_id().ConvertToUint64(), stale_response_7014)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building stale 7014 payload for channel=%llu", static_cast<unsigned long long>(channel_id));
            return true;
        }

        if (wrapped) {
            if (!outer_session_field_raw) {
                GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for stale 7272 channel=%llu", static_cast<unsigned long long>(channel_id));
                return true;
            }

            std::string wrapped_stale_7014;
            if (!GBE_BuildWrappedDotaReplayMessage(stale_response_7014, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), wrapped_stale_7014)) {
                GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping stale 7014 payload for channel=%llu", static_cast<unsigned long long>(channel_id));
                return true;
            }

            push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_stale_7014);
        } else {
            push_incoming_now(GBE_kDotaOtherLeftChannel | GBE_kProtoMask, stale_response_7014);
        }

        return true;
    }
    if (channel_id == 0) {
        GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Ignoring 7272 because no chat channel is active");
        return true;
    }

    const bool leaving_non_current_channel_during_abandon =
        leaving_postgame_channel &&
        !matches_current_postgame_channel &&
        matches_pre_postgame_channel;
    const bool leaving_legacy_channel_after_signout =
        leaving_postgame_channel &&
        !matches_current_postgame_channel &&
        pre_postgame_channel_id == 0;
    if (leaving_legacy_channel_after_signout) {
        std::string response_7010_postgame;
        if (GBE_BuildDotaPostGameJoinChatChannelResponsePayload(
                steam_id,
                local_channel_id,
                GBE_local_lobby.chat_channel_name,
                std::string(settings->get_local_name()),
                response_7010_postgame)) {
            if (wrapped) {
                if (!outer_session_field_raw) {
                    GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for postgame 7010 after signout channel=%llu", static_cast<unsigned long long>(channel_id));
                    return true;
                }

                std::string wrapped_7010_postgame;
                if (GBE_BuildWrappedDotaReplayMessage(response_7010_postgame, *outer_session_field_raw, steam_id, wrapped_7010_postgame)) {
                    push_incoming_now(GBE_kEMsgClientFromGC | GBE_kProtoMask, wrapped_7010_postgame);
                } else {
                    GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping postgame 7010 after signout channel=%llu", static_cast<unsigned long long>(channel_id));
                }
            } else {
                push_incoming_now(GBE_kDotaJoinChatChannelResponse | GBE_kProtoMask, response_7010_postgame);
            }
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Queued postgame 7010 after normal signout legacy 7272 request_channel=%llu post_channel=%llu lobby_id=%llu",
                static_cast<unsigned long long>(channel_id),
                static_cast<unsigned long long>(local_channel_id),
                static_cast<unsigned long long>(lobby_id)
            );
        } else {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building postgame 7010 after signout channel=%llu", static_cast<unsigned long long>(channel_id));
        }
    }
    if (leaving_non_current_channel_during_abandon) {
        GBE_pending_dota_abandon_finalize_after_7014 = true;
        GBE_pending_dota_abandon_finalize_lobby_id = lobby_id;
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Handling pre-postgame 7272 during abandon teardown (replying 7014, reset after retrieval). request_channel=%llu current_postgame_channel=%llu pre_postgame_channel=%llu matched_pre=%u lobby_id=%llu",
            static_cast<unsigned long long>(channel_id),
            static_cast<unsigned long long>(local_channel_id),
            static_cast<unsigned long long>(GBE_local_lobby.abandon_pre_postgame_chat_channel_id),
            matches_pre_postgame_channel ? 1u : 0u,
            static_cast<unsigned long long>(lobby_id)
        );
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

    if (leaving_postgame_channel) {
        if (!matches_current_postgame_channel) {
            GBE_GC_DebugLog(
                "GC_DOTA_LOBBY",
                "[LOBBY] Handled non-current 7272 during postgame teardown. request_channel=%llu local_channel=%llu pre_postgame_channel=%llu",
                static_cast<unsigned long long>(request.channel_id),
                static_cast<unsigned long long>(local_channel_id),
                static_cast<unsigned long long>(pre_postgame_channel_id)
            );
            return true;
        }

        GBE_UpdateDotaPracticeLobbyLaunchRichPresence("#DOTA_RP_INIT", "SERVERSETUP", false, false);

        // During host disconnect from hero selection, the real client can still be unwinding
        // server/game-rules state after postgame chat leaves. Clearing the entire local/generic
        // lobby snapshot here is too early and can race later disconnect teardown.
        GBE_local_lobby.has_chat_channel = false;
        GBE_local_lobby.chat_channel_id = 0;
        GBE_local_lobby.chat_channel_name.clear();
        GBE_local_lobby.chat_channel_type = 0;
        GBE_local_lobby.abandon_pre_postgame_chat_channel_id = 0;

        std::string persona_message;
        if (!GBE_BuildDotaPersonaStatePeripheralMessage(GBE_kDotaAbandonPersonaStateInitHex, steam_id, lobby_id, persona_message)) {
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "failed building abandon persona label=7272_init lobby_id=%llu",
                static_cast<unsigned long long>(lobby_id)
            );
        } else {
            push_incoming_now(GBE_kSteamPersonaState | GBE_kProtoMask, persona_message);
            GBE_GC_DebugLog(
                "GC_DOTA_SYNC",
                "queued abandon persona label=7272_init lobby_id=%llu size=%zu",
                static_cast<unsigned long long>(lobby_id),
                persona_message.size()
            );
        }

        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Deferred full lobby reset after postgame 7272 to avoid racing disconnect teardown LobbyID=%llu state=%u game_state=%u match_id=%llu server_id=%llu",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state,
            static_cast<unsigned long long>(GBE_local_lobby.match_id),
            static_cast<unsigned long long>(GBE_local_lobby.server_id)
        );
    } else {
        GBE_local_lobby.has_chat_channel = false;
        GBE_local_lobby.chat_channel_id = 0;
        GBE_local_lobby.chat_channel_name.clear();
        GBE_local_lobby.chat_channel_type = 0;
    }

    GBE_PublishSharedDotaLobbyState("7272_leave_chat");
    if (!GBE_MaybeHandleDotaPracticeLobbyKicked("7272_leave_chat")) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Chat channel left. channel=%llu wrapped=%d",
            static_cast<unsigned long long>(channel_id),
            wrapped ? 1 : 0
        );
        return true;
    }

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

    std::string outbound_25 = response_25;
    uint32 outbound_25_emsg = GBE_kDotaCacheUnsubscribed;

    if (wrapped) {
        if (!outer_session_field_raw) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Missing wrapped session context for 8246 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
            return true;
        }

        if (!GBE_BuildWrappedDotaReplayMessage(response_25, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), outbound_25)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 25 payload for 8246 LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
            return true;
        }
        outbound_25_emsg = GBE_kEMsgClientFromGC;
    }

    std::string outbound_8247;
    uint32 outbound_8247_emsg = wrapped ? GBE_kEMsgClientFromGC : GBE_kDotaDestroyLobbyResponse;
    if (has_request_job) {
        std::string response_8247;
        if (!GBE_BuildDotaDestroyLobbyResponsePayload(request_job_id, response_8247)) {
            GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed building 8247 payload for LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
            return true;
        }

        if (wrapped) {
            if (!GBE_BuildWrappedDotaReplayMessage(response_8247, *outer_session_field_raw, settings->get_local_steam_id().ConvertToUint64(), outbound_8247)) {
                GBE_GC_DebugLog("GC_DOTA_LOBBY", "[LOBBY] Failed wrapping 8247 payload for LobbyID=%llu", static_cast<unsigned long long>(lobby_id));
                return true;
            }
        } else {
            outbound_8247 = response_8247;
        }
    }

    ResetGCMemory("8246_destroy", true, true);
    push_incoming_now(outbound_25_emsg | GBE_kProtoMask, outbound_25);
    if (has_request_job)
        push_incoming_now(outbound_8247_emsg | GBE_kProtoMask, outbound_8247);

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Lobby destroyed. unsubscribed LobbyID=%llu wrapped=%d request_job=%llu has_job=%d",
        static_cast<unsigned long long>(lobby_id),
        wrapped ? 1 : 0,
        static_cast<unsigned long long>(request_job_id),
        has_request_job ? 1 : 0
    );
    return true;
}

bool Steam_Game_Coordinator::GBE_HandleDotaWrappedPostLoginRequest(const void *pubData, uint32 cubData)
{
    GBE_DotaWrappedDirectContext context{};
    if (!GBE_ExtractWrappedDotaDirectContext(pubData, cubData, context))
        return false;

    if (context.inner_emsg != GBE_kDotaJoinChatChannel)
        if (context.inner_emsg != GBE_kDotaAbandonCurrentGame)
        if (context.inner_emsg != GBE_kDotaGameMatchSignOut)
        if (context.inner_emsg != GBE_kDotaPracticeLobbyCreate)
        if (context.inner_emsg != GBE_kDotaLobbyList)
        if (context.inner_emsg != GBE_kDotaFriendPracticeLobbyListRequest)
        if (context.inner_emsg != GBE_kDotaPracticeLobbyJoin)
        if (context.inner_emsg != GBE_kDotaPracticeLobbyLeave)
        if (context.inner_emsg != GBE_kDotaPracticeLobbyLaunch)
        if (context.inner_emsg != GBE_kDotaPracticeLobbySetDetails)
        if (context.inner_emsg != GBE_kDotaPracticeLobbySetTeamSlot)
        if (context.inner_emsg != GBE_kDotaPracticeLobbyKick)
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

    if (context.inner_emsg == GBE_kDotaAbandonCurrentGame) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 7035 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaAbandonCurrentGameRequest(true, &context.outer_session_field_raw);
    }

    if (context.inner_emsg == GBE_kDotaGameMatchSignOut) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 7004 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaGameMatchSignOutRequest(true, &context.outer_session_field_raw, context.has_request_job, context.request_job_id);
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

    if (context.inner_emsg == GBE_kDotaLobbyList) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 8011 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaLobbyListRequest(context.has_request_job, context.request_job_id, true, &context.outer_session_field_raw);
    }

    if (context.inner_emsg == GBE_kDotaFriendPracticeLobbyListRequest) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 7111 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaFriendPracticeLobbyListRequest(true, &context.outer_session_field_raw);
    }

    if (context.inner_emsg == GBE_kGCInviteToLobby) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 4512 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaInviteToLobbyRequest(context.inner_body_raw, true, &context.outer_session_field_raw);
    }

    if (context.inner_emsg == GBE_kGCLobbyInviteResponse) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 4513 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaLobbyInviteResponseRequest(context.inner_body_raw, true, &context.outer_session_field_raw);
    }

    if (context.inner_emsg == GBE_kDotaPracticeLobbyJoin) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 7044 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaPracticeLobbyJoinRequest(
            context.inner_body_raw,
            context.request_job_id,
            context.has_request_job,
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

        return GBE_HandleDotaPracticeLobbyLaunchRequest(true, &context.outer_session_field_raw, context.has_request_job, context.request_job_id);
    }

    if (context.inner_emsg == GBE_kDotaPracticeLobbyKick) {
        GBE_GC_DebugLog(
            "GC_DOTA_LOBBY",
            "[LOBBY] Received wrapped 7081 has_job=%d request_job=%llu session_raw_size=%zu body_prefix=%s",
            context.has_request_job ? 1 : 0,
            static_cast<unsigned long long>(context.request_job_id),
            context.outer_session_field_raw.size(),
            GBE_FormatHexPrefix(reinterpret_cast<const uint8 *>(context.inner_body_raw.data()), context.inner_body_raw.size(), 48).c_str()
        );

        return GBE_HandleDotaPracticeLobbyKickRequest(
            context.inner_body_raw,
            true,
            &context.outer_session_field_raw
        );
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

void Steam_Game_Coordinator::GBE_ResetDotaPracticeLobbyLaunchPeripheralState()
{
    GBE_last_dota_direct_connect_callback_signature.clear();
}

bool Steam_Game_Coordinator::GBE_ShouldTrackDotaPracticeLobbyLateSteamChain() const
{
    return
        GBE_local_lobby.active &&
        GBE_local_lobby.lobby_id != 0;
}

void Steam_Game_Coordinator::GBE_UpdateDotaPracticeLobbyLaunchRichPresence(const char *status, const char *lobby_state, bool include_party, bool include_lobby)
{
    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_friends)
        return;

    char lobby_value[512] = {};
    if (include_lobby) {
        const char *room_name = GBE_local_lobby.room_name.empty() ? "" : GBE_local_lobby.room_name.c_str();
        std::snprintf(
            lobby_value,
            sizeof(lobby_value),
            "lobby_id: %llu lobby_state: %s game_mode: DOTA_GAMEMODE_AP member_count: 1 max_member_count: 10 name: \"%s\" lobby_type: 1",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            lobby_state ? lobby_state : "SERVERSETUP",
            room_name
        );
    }

    steam_client->steam_friends->SetRichPresence("status", status ? status : "");
    steam_client->steam_friends->SetRichPresence("steam_display", status ? status : "");
    steam_client->steam_friends->SetRichPresence("num_params", "0");
    steam_client->steam_friends->SetRichPresence("EventLevel_26", "0");
    steam_client->steam_friends->SetRichPresence("EventLevel_39", "0");
    steam_client->steam_friends->SetRichPresence("EventLevel_56", "1");
    steam_client->steam_friends->SetRichPresence("EventLevel_55", "1");
    const std::string direct_connect_endpoint = include_party
        ? GBE_GetDotaPracticeLobbyFirstConnectEndpoint(GBE_local_lobby.connect)
        : std::string();
    if (!direct_connect_endpoint.empty()) {
        const std::string connect_command = std::string("+connect ") + direct_connect_endpoint;
        steam_client->steam_friends->SetRichPresence("connect", connect_command.c_str());
    } else {
        steam_client->steam_friends->SetRichPresence("connect", nullptr);
    }
    if (include_lobby) {
        steam_client->steam_friends->SetRichPresence("lobby", lobby_value);
    } else {
        steam_client->steam_friends->SetRichPresence("lobby", nullptr);
    }
    if (include_party) {
        steam_client->steam_friends->SetRichPresence("party", "party_state: IN_MATCH");
    } else {
        steam_client->steam_friends->SetRichPresence("party", nullptr);
    }

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "updated local launch rich presence status=%s lobby_state=%s include_party=%u include_lobby=%u lobby_id=%llu connect=%s",
        status ? status : "",
        lobby_state ? lobby_state : "",
        include_party ? 1u : 0u,
        include_lobby ? 1u : 0u,
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        direct_connect_endpoint.c_str()
    );
}

void Steam_Game_Coordinator::GBE_ClearDotaPracticeLobbyLaunchRichPresence()
{
    GBE_last_dota_launch_persona_signature.clear();
    GBE_last_dota_direct_connect_callback_signature.clear();

    Steam_Client *steam_client = get_steam_client();
    if (!steam_client || !steam_client->steam_friends)
        return;

    steam_client->steam_friends->SetRichPresence("status", nullptr);
    steam_client->steam_friends->SetRichPresence("steam_display", nullptr);
    steam_client->steam_friends->SetRichPresence("num_params", nullptr);
    steam_client->steam_friends->SetRichPresence("EventLevel_26", nullptr);
    steam_client->steam_friends->SetRichPresence("EventLevel_39", nullptr);
    steam_client->steam_friends->SetRichPresence("EventLevel_56", nullptr);
    steam_client->steam_friends->SetRichPresence("EventLevel_55", nullptr);
    steam_client->steam_friends->SetRichPresence("connect", nullptr);
    steam_client->steam_friends->SetRichPresence("lobby", nullptr);
    steam_client->steam_friends->SetRichPresence("party", nullptr);
}

void Steam_Game_Coordinator::GBE_MaybeQueueDotaPracticeLobbyDirectConnectCallback(const char *reason)
{
    if (is_server || gc_profile != GC_PROFILE_DOTA2 || !callbacks)
        return;

    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0 || !GBE_local_lobby.lan)
        return;

    if (GBE_local_lobby.state != 2u || GBE_local_lobby.match_id == 0)
        return;

    const std::string endpoint = GBE_GetDotaPracticeLobbyFirstConnectEndpoint(GBE_local_lobby.connect);
    const uint32 endpoint_ip = GBE_ParseDotaPracticeLobbyConnectIPv4(endpoint);
    if (endpoint.empty() || endpoint_ip == 0u)
        return;

    const uint64 local_steam_id = settings ? settings->get_local_steam_id().ConvertToUint64() : 0ull;
    const uint64 owner_steam_id = GBE_local_lobby.owner_steam_id != 0 ? GBE_local_lobby.owner_steam_id : GBE_GetDotaLobbyOwnerSteamId();
    if (local_steam_id != 0ull && owner_steam_id != 0ull && local_steam_id == owner_steam_id) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipping owner direct connect callback reason=%s lobby_id=%llu endpoint=%s owner=%llu",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            endpoint.c_str(),
            static_cast<unsigned long long>(owner_steam_id)
        );
        return;
    }

    const uint32 local_ip = network ? network->getOwnIP() : 0u;
    if (local_ip != 0u && local_ip == endpoint_ip) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipping self-IP direct connect callback reason=%s lobby_id=%llu endpoint=%s local_ip=%s",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            endpoint.c_str(),
            GBE_FormatIPv4(local_ip).c_str()
        );
        return;
    }

    std::string signature;
    signature.reserve(96);
    signature.append(std::to_string(GBE_local_lobby.lobby_id));
    signature.push_back('|');
    signature.append(std::to_string(GBE_local_lobby.match_id));
    signature.push_back('|');
    signature.append(endpoint);
    if (signature == GBE_last_dota_direct_connect_callback_signature) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipping duplicate direct connect callback reason=%s lobby_id=%llu endpoint=%s",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            endpoint.c_str()
        );
        return;
    }

    GameServerChangeRequested_t server_change{};
    std::strncpy(server_change.m_rgchServer, endpoint.c_str(), sizeof(server_change.m_rgchServer) - 1);
    callbacks->addCBResult(server_change.k_iCallback, &server_change, sizeof(server_change), 0.0);

    const std::string connect_command = std::string("+connect ") + endpoint;
    GameRichPresenceJoinRequested_t rich_join{};
    rich_join.m_steamIDFriend = CSteamID(owner_steam_id);
    std::strncpy(rich_join.m_rgchConnect, connect_command.c_str(), sizeof(rich_join.m_rgchConnect) - 1);
    callbacks->addCBResult(rich_join.k_iCallback, &rich_join, sizeof(rich_join), 0.25);

    GBE_last_dota_direct_connect_callback_signature = signature;
    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "queued direct connect callbacks reason=%s lobby_id=%llu match_id=%llu endpoint=%s command=%s",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        static_cast<unsigned long long>(GBE_local_lobby.match_id),
        endpoint.c_str(),
        connect_command.c_str()
    );
}

void Steam_Game_Coordinator::GBE_MaybeQueueDotaPracticeLobbyLaunchPersonaState(const char *status, const char *lobby_state, bool include_party, bool include_lobby, const char *reason)
{
    if (is_server || gc_profile != GC_PROFILE_DOTA2 || !status || !lobby_state || !include_lobby || GBE_local_lobby.lobby_id == 0) {
        if (!include_lobby)
            GBE_last_dota_launch_persona_signature.clear();
        return;
    }

    const char *template_hex = nullptr;
    if (std::strcmp(status, "#DOTA_RP_INIT") == 0 && std::strcmp(lobby_state, "SERVERSETUP") == 0 && !include_party) {
        template_hex = GBE_kDotaLaunchPersonaStateInitServerSetupHex;
    } else if (std::strcmp(status, "#DOTA_RP_FINDING_MATCH") == 0 && std::strcmp(lobby_state, "SERVERSETUP") == 0 && include_party) {
        template_hex = GBE_kDotaLaunchPersonaStateFindingMatchServerSetupHex;
    } else if (std::strcmp(status, "#DOTA_RP_FINDING_MATCH") == 0 && std::strcmp(lobby_state, "RUN") == 0 && include_party) {
        template_hex = GBE_kDotaLaunchPersonaStateFindingMatchRunHex;
    } else if (std::strcmp(status, "#DOTA_RP_PRIVATE_LOBBY") == 0 && std::strcmp(lobby_state, "RUN") == 0 && include_party) {
        template_hex = GBE_kDotaLaunchPersonaStatePrivateLobbyRunHex;
    }

    if (!template_hex) {
        GBE_last_dota_launch_persona_signature.clear();
        return;
    }

    std::string signature;
    signature.reserve(96);
    signature.append(status);
    signature.push_back('|');
    signature.append(lobby_state);
    signature.push_back('|');
    signature.push_back(include_party ? '1' : '0');
    signature.push_back('|');
    signature.append(std::to_string(GBE_local_lobby.lobby_id));

    if (signature == GBE_last_dota_launch_persona_signature) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipping duplicate launch persona reason=%s lobby_id=%llu signature=%s",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            signature.c_str()
        );
        return;
    }

    std::string persona_message;
    const uint64 steam_id = settings ? settings->get_local_steam_id().ConvertToUint64() : 0ull;
    if (steam_id == 0 || !GBE_BuildDotaPersonaStatePeripheralMessage(template_hex, steam_id, GBE_local_lobby.lobby_id, persona_message)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "failed building launch persona reason=%s lobby_id=%llu status=%s lobby_state=%s",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            status,
            lobby_state
        );
        return;
    }

    push_incoming_now(GBE_kSteamPersonaState | GBE_kProtoMask, persona_message);
    GBE_last_dota_launch_persona_signature = signature;
    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "queued launch persona reason=%s lobby_id=%llu status=%s lobby_state=%s size=%zu",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        status,
        lobby_state,
        persona_message.size()
    );
}

void Steam_Game_Coordinator::GBE_ReapplyDotaPracticeLobbyLaunchRichPresence(const char *reason)
{
    if (!GBE_local_lobby.active || GBE_local_lobby.lobby_id == 0)
        return;

    if (GBE_ShouldSuppressDotaAbandonedLobby(GBE_local_lobby.lobby_id)) {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "skipped reapplying launch rich presence for suppressed abandoned lobby reason=%s lobby_id=%llu state=%u game_state=%u",
            reason ? reason : "unknown",
            static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
            GBE_local_lobby.state,
            GBE_local_lobby.game_state
        );
        return;
    }

    const char *status = nullptr;
    const char *lobby_state = nullptr;
    bool include_party = false;
    const bool should_present_private_lobby =
        !GBE_local_lobby.abandon_postgame_active &&
        GBE_local_lobby.state == 2u &&
        GBE_local_lobby.game_state >= 1u;

    if (GBE_local_lobby.abandon_postgame_active) {
        status = "#DOTA_RP_PRIVATE_LOBBY";
        lobby_state = "RUN";
        include_party = true;
    } else if (GBE_local_lobby.state == 2u) {
        if (should_present_private_lobby) {
            status = "#DOTA_RP_PRIVATE_LOBBY";
        } else {
            status = "#DOTA_RP_FINDING_MATCH";
        }
        lobby_state = "RUN";
        include_party = true;
    } else if (GBE_local_lobby.state == 1u && GBE_local_lobby.game_state == 0u) {
        if (GBE_local_lobby.server_id != 0) {
            status = "#DOTA_RP_FINDING_MATCH";
            lobby_state = "SERVERSETUP";
            include_party = true;
        } else {
            status = "#DOTA_RP_INIT";
            lobby_state = "SERVERSETUP";
            include_party = false;
        }
    }

    if (!status || !lobby_state)
        return;

    GBE_GC_DebugLog(
        "GC_DOTA_SYNC",
        "reapplying launch rich presence reason=%s lobby_id=%llu state=%u game_state=%u server_id=%llu status=%s lobby_state=%s include_party=%u",
        reason ? reason : "unknown",
        static_cast<unsigned long long>(GBE_local_lobby.lobby_id),
        GBE_local_lobby.state,
        GBE_local_lobby.game_state,
        static_cast<unsigned long long>(GBE_local_lobby.server_id),
        status,
        lobby_state,
        include_party ? 1u : 0u
    );

    GBE_UpdateDotaPracticeLobbyLaunchRichPresence(status, lobby_state, include_party, !GBE_local_lobby.abandon_postgame_active);
    GBE_MaybeQueueDotaPracticeLobbyLaunchPersonaState(status, lobby_state, include_party, !GBE_local_lobby.abandon_postgame_active, reason);
    GBE_MaybeQueueDotaPracticeLobbyDirectConnectCallback(reason);
}

void Steam_Game_Coordinator::GBE_FinalizeDotaAbandonAfterOtherLeftChannel(uint64 consumed_lobby_id, const char *reason)
{
    if (is_server || gc_profile != GC_PROFILE_DOTA2 || !GBE_local_lobby.abandon_postgame_active)
        return;
    if (GBE_local_lobby.lobby_id == 0 || GBE_local_lobby.lobby_id != consumed_lobby_id)
        return;

    const uint64 lobby_id = GBE_local_lobby.lobby_id;
    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Resetting abandon teardown state after 7014 retrieval LobbyID=%llu reason=%s",
        static_cast<unsigned long long>(lobby_id),
        reason ? reason : "unknown"
    );
    ResetGCMemory(reason ? reason : "7014_abandon_finalize", true, false);
}

void Steam_Game_Coordinator::GBE_FinalizeDotaNormalSignoutAfterCacheUnsubscribed(uint64 consumed_lobby_id, const char *reason)
{
    if (gc_profile != GC_PROFILE_DOTA2)
        return;

    if (!settings)
        return;

    const uint64 steam_id = settings->get_local_steam_id().ConvertToUint64();
    const uint64 shared_lobby_id = GBE_shared_dota_lobby_state.lobby_id;
    const uint64 lobby_id = consumed_lobby_id != 0 ? consumed_lobby_id : shared_lobby_id;
    const GBE_LocalLobby postgame_lobby = GBE_local_lobby;
    Steam_Game_Coordinator *client_target = this;
    if (is_server) {
        Steam_Client *steam_client = get_steam_client();
        if (steam_client && steam_client->steam_game_coordinator)
            client_target = steam_client->steam_game_coordinator;
    }

    GBE_GC_DebugLog(
        "GC_DOTA_LOBBY",
        "[LOBBY] Finalizing normal signout after 25 retrieval LobbyID=%llu shared_lobby=%llu reason=%s",
        static_cast<unsigned long long>(lobby_id),
        static_cast<unsigned long long>(shared_lobby_id),
        reason ? reason : "unknown"
    );

    if (settings->get_lobby().ConvertToUint64() != 0)
        settings->set_lobby(k_steamIDNil);

    if (client_target && !client_target->is_server && client_target != this) {
        client_target->GBE_ResetDotaPracticeLobbyLaunchPeripheralState();
        if (postgame_lobby.active && postgame_lobby.lobby_id != 0)
            client_target->GBE_local_lobby = postgame_lobby;
        client_target->GBE_last_dota_launch_state_pushed_game_state = 0;
    }
    GBE_ResetDotaPracticeLobbyLaunchPeripheralState();
    GBE_local_lobby = GBE_LocalLobby{};
    GBE_last_dota_launch_state_pushed_game_state = 0;
    GBE_shared_dota_lobby_state = GBE_SharedDotaLobbyState{};

    if (client_target && client_target->gc_profile == GC_PROFILE_DOTA2) {
        client_target->GBE_ClearDotaPracticeLobbyLaunchRichPresence();

        if (client_target != this && lobby_id != 0) {
            std::string client_response_25;
            if (GBE_BuildDotaLobbyCacheUnsubscribedPayload(lobby_id, client_response_25)) {
                client_target->push_incoming_now(GBE_kDotaCacheUnsubscribed | GBE_kProtoMask, client_response_25);
                GBE_GC_DebugLog(
                    "GC_DOTA_SYNC",
                    "queued normal signout client cache unsubscribe label=25 lobby_id=%llu size=%zu",
                    static_cast<unsigned long long>(lobby_id),
                    client_response_25.size()
                );
            } else {
                GBE_GC_DebugLog(
                    "GC_DOTA_SYNC",
                    "failed building normal signout client cache unsubscribe label=25 lobby_id=%llu",
                    static_cast<unsigned long long>(lobby_id)
                );
            }
        }
    }

    std::string persona_message;
    if (GBE_BuildDotaPersonaStatePeripheralMessage(GBE_kDotaAbandonPersonaStateInitHex, steam_id, lobby_id, persona_message)) {
        if (client_target && client_target->gc_profile == GC_PROFILE_DOTA2)
            client_target->push_incoming_now(GBE_kSteamPersonaState | GBE_kProtoMask, persona_message);
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "queued normal signout persona label=25_init lobby_id=%llu size=%zu",
            static_cast<unsigned long long>(lobby_id),
            persona_message.size()
        );
    } else {
        GBE_GC_DebugLog(
            "GC_DOTA_SYNC",
            "failed building normal signout persona label=25_init lobby_id=%llu",
            static_cast<unsigned long long>(lobby_id)
        );
    }
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

    if (!GBE_MaybeHandleDotaPracticeLobbyKicked("run_callbacks_generic_lobby_members_changed"))
        GBE_MaybeNotifyDotaPracticeLobbyMembersChanged("run_callbacks_generic_lobby_members_changed");

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
