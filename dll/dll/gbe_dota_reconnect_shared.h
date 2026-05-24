#ifndef __INCLUDED_GBE_DOTA_RECONNECT_SHARED_H__
#define __INCLUDED_GBE_DOTA_RECONNECT_SHARED_H__

#include <cstdint>
#include <atomic>

// Shared state for Dota 2 LAN reconnect interception.
// Written by steam_game_coordinator.cpp, read by steam_networking_socketsserialized.cpp.

struct GBE_DotaReconnectContext {
    uint64_t server_id;       // lobby server_steamid (AnonGameServer)
    uint32_t game_state;      // lobby game_state (2 = in-game, 3 = post-game)
    char connect[128];        // LAN endpoint e.g. "172.19.60.153:27015"
    uint64_t owner_steam_id;  // lobby owner for GameRichPresenceJoinRequested
};

// Get current Dota lobby reconnect context.
// Returns false if no active lobby or game not started.
bool GBE_GetDotaReconnectContext(GBE_DotaReconnectContext *out);

// Flag indicating player has disconnected (CancelAuthTicket called)
// and is eligible for reconnect interception.
extern std::atomic<bool> GBE_dota_reconnect_eligible;

#endif // __INCLUDED_GBE_DOTA_RECONNECT_SHARED_H__
