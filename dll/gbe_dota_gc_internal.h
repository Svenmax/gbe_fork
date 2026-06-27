#ifndef __INCLUDED_GBE_DOTA_GC_INTERNAL_H__
#define __INCLUDED_GBE_DOTA_GC_INTERNAL_H__

#include <cstdint>
#include <string>
#include <vector>

#include "gbe_dota_types.h"
#include "dll/gbe_dota_reconnect_shared.h"

// Shared internal GC helpers used across steam_game_coordinator.cpp
// and its split companion translation units.
// These were file-scope static helpers before the lobby-state split;
// they now have external linkage so the split TUs can call them.

// Logging helpers (defined in steam_game_coordinator.cpp)
void GBE_GC_DebugLog(const char *scope, const char *fmt, ...);
const char *GBE_DescribeDotaLaunchPhase(uint32 phase);
void GBE_LogDotaSOCacheSubscribedSummary(const char *tag, const char *label, const std::string &message);

// Chat channel adaptation (defined in steam_game_coordinator.cpp)
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
    std::string &message);

// Reconnect context cache (defined in steam_game_coordinator.cpp)
extern bool GBE_recent_dota_reconnect_context_valid;
extern GBE_DotaReconnectContext GBE_recent_dota_reconnect_context;

#endif // __INCLUDED_GBE_DOTA_GC_INTERNAL_H__
