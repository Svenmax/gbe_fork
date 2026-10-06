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

// Dota 2 Game Coordinator protocol constants extracted from steam_game_coordinator.cpp.
// These are the GC emsg values, wire-format magic values, team ids, and the
// generic-lobby metadata key/value strings used to publish Dota lobby state
// across emulator instances. Kept at global scope (with the original GBE_k*
// names) so existing call sites are unchanged; C++17 inline constexpr gives
// them external linkage without ODR violations.

#ifndef GBE_DOTA_PROTOCOL_CONSTANTS_H
#define GBE_DOTA_PROTOCOL_CONSTANTS_H

#include <cstdint>
#include <cstddef>
#include "gbe_gc_message_utils.h"

// --- EMsg / GC protocol identifiers ----------------------------------------

inline constexpr std::uint32_t GBE_kProtoMask = gbe::gc_message::kProtoMask;
inline constexpr std::uint32_t GBE_kEMsgClientToGC = 5452u;
inline constexpr std::uint32_t GBE_kEMsgClientFromGC = 5453u;
inline constexpr std::uint32_t GBE_kEMsgGCClientHello = 4006u;
inline constexpr std::uint32_t GBE_kEMsgGCServerHello = 4007u;
inline constexpr std::uint32_t GBE_kEMsgGCClientWelcome = 4004u;
inline constexpr std::uint32_t GBE_kEMsgGCServerWelcome = 4005u;
inline constexpr std::uint32_t GBE_kGCInvitationCreated = 4502u;
inline constexpr std::uint32_t GBE_kGCInviteToLobby = 4512u;
inline constexpr std::uint32_t GBE_kGCLobbyInviteResponse = 4513u;
inline constexpr std::uint32_t GBE_kDotaAppId = 570u;
inline constexpr std::uint32_t GBE_kDotaCacheSubscribed = 24u;
inline constexpr std::uint32_t GBE_kDotaCacheUnsubscribed = 25u;
inline constexpr std::uint32_t GBE_kDotaPracticeLobbyDetailsUpdate = 26u;
inline constexpr std::uint32_t GBE_kDotaCacheSubscriptionCheck = 27u;
inline constexpr std::uint32_t GBE_kDotaCacheSubscriptionRefresh = 28u;
inline constexpr std::uint32_t GBE_kDotaCacheSubscribedUpToDate = 29u;
inline constexpr std::uint32_t GBE_kDotaGameMatchSignOut = 7004u;
inline constexpr std::uint32_t GBE_kDotaGameMatchSignOutResponse = 7005u;
inline constexpr std::uint32_t GBE_kDotaJoinChatChannel = 7009u;
inline constexpr std::uint32_t GBE_kDotaJoinChatChannelResponse = 7010u;
inline constexpr std::uint32_t GBE_kDotaOtherJoinedChannel = 7013u;
inline constexpr std::uint32_t GBE_kDotaOtherLeftChannel = 7014u;
inline constexpr std::uint32_t GBE_kDotaAbandonCurrentGame = 7035u;
inline constexpr std::uint32_t GBE_kDotaLeaverDetected = 7072u;
inline constexpr std::uint32_t GBE_kDotaSubmitPlayerReportV2 = 7082u;
inline constexpr std::uint32_t GBE_kDotaSubmitPlayerReportResponseV2 = 7083u;
inline constexpr std::uint32_t GBE_kDotaGameMatchSignOutPermissionRequest = 7381u;
inline constexpr std::uint32_t GBE_kDotaGameMatchSignOutPermissionResponse = 7382u;
inline constexpr std::uint32_t GBE_kDotaLobbyAdditionalAccountData = 8869u;
inline constexpr std::uint32_t GBE_kDotaPracticeLobbyCreate = 7038u;
inline constexpr std::uint32_t GBE_kDotaPracticeLobbyLeave = 7040u;
inline constexpr std::uint32_t GBE_kDotaPracticeLobbyLaunch = 7041u;
inline constexpr std::uint32_t GBE_kDotaCustomLobbyListRequest = 7042u;
inline constexpr std::uint32_t GBE_kDotaCustomLobbyListResponse = 7043u;
inline constexpr std::uint32_t GBE_kDotaPracticeLobbyJoin = 7044u;
inline constexpr std::uint32_t GBE_kDotaPracticeLobbySetDetails = 7046u;
inline constexpr std::uint32_t GBE_kDotaPracticeLobbySetTeamSlot = 7047u;
inline constexpr std::uint32_t GBE_kDotaPracticeLobbyResponse = 7055u;
inline constexpr std::uint32_t GBE_kDotaPracticeLobbyKick = 7081u;
inline constexpr std::uint32_t GBE_kDotaPopup = 7102u;
inline constexpr std::uint32_t GBE_kDotaFriendPracticeLobbyListRequest = 7111u;
inline constexpr std::uint32_t GBE_kDotaFriendPracticeLobbyListResponse = 7112u;
inline constexpr std::uint32_t GBE_kDotaPracticeLobbyJoinResponse = 7113u;
inline constexpr std::uint32_t GBE_kDotaJoinableCustomGameModesRequest = 7466u;
inline constexpr std::uint32_t GBE_kDotaJoinableCustomGameModesResponse = 7467u;
inline constexpr std::uint32_t GBE_kDotaJoinableCustomLobbiesRequest = 7468u;
inline constexpr std::uint32_t GBE_kDotaJoinableCustomLobbiesResponse = 7469u;
inline constexpr std::uint32_t GBE_kDotaTopCustomGamesList = 8024u;
inline constexpr std::uint32_t GBE_kDotaLeaveChatChannel = 7272u;
inline constexpr std::uint32_t GBE_kDotaChatMessage = 7273u;
inline constexpr std::uint32_t GBE_kDotaPracticeLobbyJoinBroadcastChannel = 7149u;
inline constexpr std::uint32_t GBE_kDotaLobbyUpdateBroadcastChannelInfo = 7367u;
inline constexpr std::uint32_t GBE_kDotaDestroyLobbyRequest = 8246u;
inline constexpr std::uint32_t GBE_kDotaDestroyLobbyResponse = 8247u;
inline constexpr std::uint32_t GBE_kDotaPracticeLobbyCloseBroadcastChannel = 8054u;
inline constexpr std::uint32_t GBE_kDotaFindTopSourceTVGames = 8009u;
inline constexpr std::uint32_t GBE_kDotaFindTopSourceTVGamesResponse = 8010u;
inline constexpr std::uint32_t GBE_kDotaCustomGameInfoRequest = 8020u;
inline constexpr std::uint32_t GBE_kDotaCustomGameInfoResponse = 8021u;
inline constexpr std::uint32_t GBE_kDotaLobbyList = 8011u;
inline constexpr std::uint32_t GBE_kDotaLobbyListResponse = 8012u;
inline constexpr std::uint32_t GBE_kDotaSOUpdateMultiple = 6146u;
inline constexpr std::uint32_t GBE_kDotaAddSocket = 1087u;
inline constexpr std::uint32_t GBE_kDotaAddSocketResponse = 1090u;
inline constexpr std::uint32_t GBE_kDotaSetItemStyle = 2577u;
inline constexpr std::uint32_t GBE_kDotaSetItemStyleResponse = 2578u;
inline constexpr std::uint32_t GBE_kDotaUnlockItemStyle = 2571u;
inline constexpr std::uint32_t GBE_kDotaUnlockItemStyleResponse = 2572u;

// --- Wire-format / misc magic values ---------------------------------------

inline constexpr std::size_t GBE_kDotaWelcomeInnerBodyOffset = 48u;
inline constexpr const char *GBE_kGcDebugLogPath = "C:\\Users\\Public\\gbe_gc_debug.log";
inline constexpr std::uint64_t GBE_kDotaLobbyDetailsTimestamp = 0x0069E7F5C567E78Bull;
inline constexpr std::uint32_t GBE_kDotaLobbyField128Value = 1776809986u;

// --- Dota team ids ---------------------------------------------------------

inline constexpr std::uint32_t GBE_kDotaTeamGoodGuys = 0u;
inline constexpr std::uint32_t GBE_kDotaTeamBadGuys = 1u;
inline constexpr std::uint32_t GBE_kDotaTeamPlayerPool = 4u;

// --- Generic-lobby metadata key/value strings ------------------------------
// These are written into a Steam generic lobby's metadata so other emulator
// instances on the LAN can discover and reconstruct the Dota practice lobby.

inline constexpr const char *GBE_kDotaGenericLobbyMarkerKey = "gbe_dota_practice_lobby";
inline constexpr const char *GBE_kDotaGenericLobbyMarkerValue = "1";
inline constexpr const char *GBE_kDotaGenericLobbyDotaLobbyIdKey = "gbe_dota_lobby_id";
inline constexpr const char *GBE_kDotaGenericLobbyRoomNameKey = "gbe_dota_room_name";
inline constexpr const char *GBE_kDotaGenericLobbyGameModeKey = "gbe_dota_game_mode";
inline constexpr const char *GBE_kDotaGenericLobbyServerRegionKey = "gbe_dota_server_region";
inline constexpr const char *GBE_kDotaGenericLobbyLanPingKey = "gbe_dota_lan_ping";
inline constexpr const char *GBE_kDotaGenericLobbyPassKeyKey = "gbe_dota_pass_key";
inline constexpr const char *GBE_kDotaGenericLobbyAllowCheatsKey = "gbe_dota_allow_cheats";
inline constexpr const char *GBE_kDotaGenericLobbyFillWithBotsKey = "gbe_dota_fill_with_bots";
inline constexpr const char *GBE_kDotaGenericLobbyAllowSpectatingKey = "gbe_dota_allow_spectating";
inline constexpr const char *GBE_kDotaGenericLobbyVisibilityKey = "gbe_dota_visibility";
inline constexpr const char *GBE_kDotaGenericLobbyBotDifficultyRadiantKey = "gbe_dota_bot_diff_radiant";
inline constexpr const char *GBE_kDotaGenericLobbyBotDifficultyDireKey = "gbe_dota_bot_diff_dire";
inline constexpr const char *GBE_kDotaGenericLobbyBotRadiantKey = "gbe_dota_bot_radiant";
inline constexpr const char *GBE_kDotaGenericLobbyBotDireKey = "gbe_dota_bot_dire";
inline constexpr const char *GBE_kDotaGenericLobbyCustomGameModeKey = "gbe_dota_custom_game_mode";
inline constexpr const char *GBE_kDotaGenericLobbyCustomMapNameKey = "gbe_dota_custom_map_name";
inline constexpr const char *GBE_kDotaGenericLobbyCustomDifficultyKey = "gbe_dota_custom_difficulty";
inline constexpr const char *GBE_kDotaGenericLobbyCustomGameIdKey = "gbe_dota_custom_game_id";
inline constexpr const char *GBE_kDotaGenericLobbyCustomMinPlayersKey = "gbe_dota_custom_min_players";
inline constexpr const char *GBE_kDotaGenericLobbyCustomMaxPlayersKey = "gbe_dota_custom_max_players";
inline constexpr const char *GBE_kDotaGenericLobbyCustomGameCrcKey = "gbe_dota_custom_game_crc";
inline constexpr const char *GBE_kDotaGenericLobbyCustomGameTimestampKey = "gbe_dota_custom_game_timestamp";
inline constexpr const char *GBE_kDotaGenericLobbyCustomGamePenaltiesKey = "gbe_dota_custom_game_penalties";
inline constexpr const char *GBE_kDotaGenericLobbyOwnerSteamIdKey = "gbe_dota_owner_steam_id";
inline constexpr const char *GBE_kDotaGenericLobbyOwnerAccountIdKey = "gbe_dota_owner_account_id";
inline constexpr const char *GBE_kDotaGenericLobbyOwnerNameKey = "gbe_dota_owner_name";
inline constexpr const char *GBE_kDotaGenericLobbyStateKey = "gbe_dota_state";
inline constexpr const char *GBE_kDotaGenericLobbyGameStateKey = "gbe_dota_game_state";
inline constexpr const char *GBE_kDotaGenericLobbyMatchIdKey = "gbe_dota_match_id";
inline constexpr const char *GBE_kDotaGenericLobbyServerIdKey = "gbe_dota_server_id";
inline constexpr const char *GBE_kDotaGenericLobbyConnectKey = "gbe_dota_connect";
inline constexpr const char *GBE_kDotaGenericLobbyGameStartTimeKey = "gbe_dota_game_start_time";
inline constexpr const char *GBE_kDotaGenericLobbyTvSecretCodeKey = "gbe_dota_tv_secret_code";
inline constexpr const char *GBE_kDotaGenericLobbyTvPortKey = "gbe_dota_tv_port";
inline constexpr const char *GBE_kDotaGenericLobbyMemberTeamKey = "gbe_dota_member_team";
inline constexpr const char *GBE_kDotaGenericLobbyMemberSlotKey = "gbe_dota_member_slot";
inline constexpr const char *GBE_kDotaGenericLobbyMemberHeroKey = "gbe_dota_member_hero";
inline constexpr const char *GBE_kDotaGenericLobbyMemberConnectedKey = "gbe_dota_member_connected";
inline constexpr const char *GBE_kDotaGenericLobbyMemberNameKey = "gbe_dota_member_name";

// --- Dota launch phase ids --------------------------------------------------

inline constexpr std::uint32_t GBE_kDotaLaunchPhaseNone = 0u;
inline constexpr std::uint32_t GBE_kDotaLaunchPhaseRequested = 1u;
inline constexpr std::uint32_t GBE_kDotaLaunchPhaseSetupSynced = 2u;
inline constexpr std::uint32_t GBE_kDotaLaunchPhaseRunQueued = 3u;
inline constexpr std::uint32_t GBE_kDotaLaunchPhaseLoaded = 4u;

#endif // GBE_DOTA_PROTOCOL_CONSTANTS_H
