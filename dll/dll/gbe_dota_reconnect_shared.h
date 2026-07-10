#ifndef __INCLUDED_GBE_DOTA_RECONNECT_SHARED_H__
#define __INCLUDED_GBE_DOTA_RECONNECT_SHARED_H__

#include <cstdint>
#include <cstdio>
#include <cstdarg>
#include <atomic>
#include <string>

// Shared state for Dota 2 LAN reconnect interception.
// Written by steam_game_coordinator.cpp, read by steam_networking_socketsserialized.cpp.

struct GBE_DotaReconnectContext {
    uint64_t lobby_id;        // Dota lobby generation for reconnect deduplication
    uint64_t server_id;       // lobby server_steamid (AnonGameServer)
    uint32_t lobby_state;     // lobby state (2 = RUN)
    uint32_t game_state;      // lobby game_state (2 = in-game, 3 = post-game)
    uint64_t custom_game_id;  // arcade/custom game id; zero for ordinary practice lobby
    char connect[128];        // LAN endpoint e.g. "172.19.60.153:27015"
    uint64_t owner_steam_id;  // lobby owner for GameRichPresenceJoinRequested
};

struct GBE_DotaReconnectSharedStateSnapshot {
    bool valid{};
    bool active{};
    uint64_t lobby_id{};
    uint32_t lobby_state{};
    uint32_t game_state{};
    uint64_t server_id{};
    bool has_connect{};
    uint64_t custom_game_id{};
    bool owner_connected{};
    uint32_t launch_phase{};
    uint64_t owner_steam_id{};
    char connect[128]{};
};

struct GBE_DotaSerializedConnectionState {
    uint64_t lobby_id{};
    uint64_t last_post_server_id{};
    uint32_t retry_count{};
    uint32_t last_post_size{};
    uint64_t callback_server_id{};
    std::string callback_endpoint;
    uint64_t direct_connect_server_id{};
    std::string direct_connect_endpoint;

    void begin_lobby(uint64_t current_lobby_id)
    {
        if (lobby_id == current_lobby_id)
            return;
        *this = {};
        lobby_id = current_lobby_id;
    }

    void begin_server(uint64_t server_id)
    {
        if (last_post_server_id == server_id)
            return;
        last_post_server_id = server_id;
        retry_count = 0;
        last_post_size = 0;
        callback_server_id = 0;
        callback_endpoint.clear();
    }

    bool should_connect_direct(uint64_t server_id, const std::string &endpoint) const
    {
        return direct_connect_server_id != server_id || direct_connect_endpoint != endpoint;
    }

    void record_direct_connect(uint64_t server_id, const std::string &endpoint)
    {
        direct_connect_server_id = server_id;
        direct_connect_endpoint = endpoint;
    }

    bool engine_callback_queued(uint64_t server_id, const std::string &endpoint) const
    {
        return callback_server_id == server_id && callback_endpoint == endpoint;
    }

    void record_engine_callback(uint64_t server_id, const std::string &endpoint)
    {
        callback_server_id = server_id;
        callback_endpoint = endpoint;
    }
};

// Raw immutable scalar snapshot. Field values are copied as-is even when
// valid is false; use the valid-gated id helpers when invalid state should
// map lobby ids to zero.
struct GBE_DotaSharedLobbyScalarSnapshot {
    bool valid{};
    bool active{};
    uint64_t lobby_id{};
    uint64_t generic_lobby_id{};
    uint32_t lobby_state{};
    uint32_t game_state{};
};

inline bool GBE_DotaReconnectContextIsStarted(const GBE_DotaReconnectContext &ctx)
{
    return ctx.lobby_state >= 2u || ctx.game_state >= 2u;
}

// Get current Dota lobby reconnect context.
// Returns false if no active lobby or game not started.
bool GBE_GetDotaReconnectContext(GBE_DotaReconnectContext *out);
GBE_DotaSharedLobbyScalarSnapshot GBE_GetSharedDotaLobbyScalarSnapshot();
GBE_DotaReconnectSharedStateSnapshot GBE_GetSharedDotaReconnectStateSnapshot();
bool GBE_TryRecoverDotaReconnectContextFromGenericLobbies(uint64_t local_steam_id, GBE_DotaReconnectContext *out);
bool GBE_IsSharedDotaArcadeLobbyActive();
bool GBE_IsDotaArcadeLobbyActive();
bool GBE_GetRecentDotaReconnectContext(GBE_DotaReconnectContext *out);
void GBE_SetRecentDotaReconnectContext(const GBE_DotaReconnectContext &ctx);
void GBE_ClearRecentDotaReconnectContext();

bool GBE_IsDotaReconnectEligible();
void GBE_SetDotaReconnectEligible(bool eligible);
bool GBE_ConsumeDotaReconnectEligibility();

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
