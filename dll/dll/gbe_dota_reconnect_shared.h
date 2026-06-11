#ifndef __INCLUDED_GBE_DOTA_RECONNECT_SHARED_H__
#define __INCLUDED_GBE_DOTA_RECONNECT_SHARED_H__

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <atomic>

// Shared state for Dota 2 LAN reconnect interception.
// Written by steam_game_coordinator.cpp, read by steam_networking_socketsserialized.cpp.

struct GBE_DotaReconnectContext {
    uint64_t server_id;       // lobby server_steamid (AnonGameServer)
    uint32_t lobby_state;     // lobby state (2 = RUN)
    uint32_t game_state;      // lobby game_state (2 = in-game, 3 = post-game)
    uint64_t custom_game_id;  // arcade/custom game id; zero for ordinary practice lobby
    char connect[128];        // LAN endpoint e.g. "172.19.60.153:27015"
    uint64_t owner_steam_id;  // lobby owner for GameRichPresenceJoinRequested
};

inline bool GBE_DotaReconnectContextIsStarted(const GBE_DotaReconnectContext &ctx)
{
    return ctx.lobby_state >= 2u || ctx.game_state >= 2u;
}

// Get current Dota lobby reconnect context.
// Returns false if no active lobby or game not started.
bool GBE_GetDotaReconnectContext(GBE_DotaReconnectContext *out);

// Flag indicating player has disconnected (CancelAuthTicket called)
// and is eligible for reconnect interception.
extern std::atomic<bool> GBE_dota_reconnect_eligible;

// Lightweight debug log for reconnect subsystem (writes to gbe_gc_debug.log)
inline void GBE_ReconnectLog(const char *scope, const char *fmt, ...)
{
    FILE *f = std::fopen("C:\\Users\\Public\\gbe_gc_debug.log", "a");
    if (!f) return;
    std::fprintf(f, "[%s] ", scope);
    va_list args;
    va_start(args, fmt);
    std::vfprintf(f, fmt, args);
    va_end(args);
    std::fprintf(f, "\n");
    std::fclose(f);
}

#endif // __INCLUDED_GBE_DOTA_RECONNECT_SHARED_H__
