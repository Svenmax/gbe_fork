#ifndef __INCLUDED_GBE_DOTA_GC_INTERNAL_H__
#define __INCLUDED_GBE_DOTA_GC_INTERNAL_H__

#include <cstdint>
#include <string>
#include <vector>

// Steam SDK typedefs (uint8/uint16/uint32/uint64). This header uses those
// typedefs in its declarations, so it must be self-contained. The minimal
// steamtypes.h is included (rather than the heavier steamclientpublic.h) so
// that pure payload-helper TUs (gbe_dota_payload_wire_helpers.cpp,
// gbe_dota_payload_lobby_helpers.cpp, gbe_dota_payload_item_helpers.cpp) can
// include this header without pulling in the full Steam SDK chain.
#include <steam/steamtypes.h>

#include "gbe_dota_types.h"
#include "gbe_dota_payload_lobby_helpers.h"
#include "gbe_dota_payload_wire_helpers.h"
#include "dll/gbe_dota_reconnect_shared.h"

// Shared internal GC helpers used across steam_game_coordinator.cpp
// and its split companion translation units.
// These were file-scope static helpers before the lobby-state split;
// they now have external linkage so the split TUs can call them.

// Logging helpers (defined in steam_game_coordinator.cpp)
void GBE_GC_DebugLog(const char *scope, const char *fmt, ...);
const char *GBE_DescribeDotaLaunchPhase(uint32 phase);
void GBE_LogDotaSOCacheSubscribedSummary(const char *tag, const char *label, const std::string &message);

// Shared lobby state cache (defined in steam_game_coordinator.cpp)
struct GBE_SharedDotaLobbyState;
namespace gbe::dota_lobby_state {
class Store;
}
gbe::dota_lobby_state::Store &GBE_GetSharedDotaLobbyStateStore();
GBE_SharedDotaLobbyState GBE_GetSharedDotaLobbyStateSnapshot();
bool GBE_HasSharedDotaLobbyState();
uint64 GBE_GetSharedDotaLobbyIdOrZero();
uint64 GBE_GetSharedDotaGenericLobbyIdOrZero();
void GBE_ClearSharedDotaLobbyState();
void GBE_ClearSharedDotaLobbyForRuntimeReset();


#include <array>
#include <cstring>

// Forward declarations (avoid heavy includes in this internal header).
class Steam_Game_Coordinator;
class CSteamID;
struct Econ_Item;
class Settings;
struct GBE_DotaLootListData;

// --- Phase 2.3a: shared helpers promoted from file-scope static to external ---
// These symbols are used by both the main GC TU and the handler TU
// (gbe_dota_handlers.cpp). Definitions remain in steam_game_coordinator.cpp.

// Template serializer (moved here so both TUs can instantiate it).
template <class T>
inline void ser_var(std::string &buf, const T &input)
{
    buf.append(reinterpret_cast<const char *>(&input), sizeof(T));
}
// Template deserializer (moved here so both TUs can instantiate it).
template <class T>
inline T deser_var(const char *&p)
{
    T output;
    memcpy(&output, p, sizeof(T));
    p += sizeof(T);
    return output;
}

// Shared VPK loot data cache. Keep mutation centralized in steam_game_coordinator.cpp.
const GBE_DotaLootListData &GBE_GetDotaVpkLootData();
void GBE_SetDotaVpkLootData(GBE_DotaLootListData &&loot_data);

// Shared const data tables
extern const std::array<uint8, 4> GBE_kOldDotaAccountIdVarint;
extern const std::array<uint8, 9> GBE_kOldDotaSteamIdVarint;
extern const std::array<uint8, 8> GBE_kOldDotaLobbyIdVarint;
extern const std::array<uint8, 8> GBE_kOldDotaSteamIdFixed64;
extern const std::array<uint8, 8> GBE_kOldDotaPersonaSteamIdFixed64;
extern const std::array<uint8, 4> GBE_kOldDotaAccountIdFixed32;
extern const std::array<uint8, 5> GBE_kOldDotaPracticeLobbyMatchIdVarint;
extern const std::array<uint8, 8> GBE_kOldDotaPracticeLobbyServerIdFixed64;
extern const char * const GBE_kOldDotaPracticeLobbyLobbyIdText;
extern const char * const GBE_kOldDotaPracticeLobbyLobbyIdTextAlt;
extern const uint32 GBE_kSteamTicketAuthComplete;
extern const char * const GBE_kDotaAbandonPersonaStateInitHex;

// Shared helper functions (defined in steam_game_coordinator.cpp)
bool GBE_PushDotaPlayerEquippedItemsCacheToGC(
    Steam_Game_Coordinator *target_gc,
    const CSteamID &player_steam_id,
    const std::vector<Econ_Item> &source_items,
    bool unsubscribe_first,
    const char *reason);

bool GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage(
    const char *template_hex,
    uint64 steam_id,
    uint64 lobby_id,
    uint64 server_id,
    bool patch_server_id,
    std::string &message);

bool GBE_PrepareDotaPersonaStatePeripheralMessage(
    const char *template_hex,
    uint64 steam_id,
    uint64 lobby_id,
    std::string &message);

void GBE_LogDotaResponsePacket(
    const char *reason,
    uint32 inner_emsg,
    bool wrapped,
    const std::string &inner_payload,
    const std::string &outbound_payload,
    uint64 lobby_id,
    uint32 lobby_state,
    uint32 lobby_game_state);

// Shared server-hello cache accessors (defined in steam_game_coordinator.cpp).
bool GBE_HasLastDotaServerHelloContext();
const GBE_DotaServerHelloContext &GBE_GetLastDotaServerHelloContext();
void GBE_SetLastDotaServerHelloContext(const GBE_DotaServerHelloContext &context);
void GBE_ClearLastDotaServerHelloContext();


// --- Phase 2.5: shared lobby-snapshot/build helpers (externalized) ---
// These symbols are used by both the main GC TU and the lobby-snapshot TU
// (gbe_dota_lobby_snapshot_coordinator.cpp). Definitions remain in
// steam_game_coordinator.cpp. The two *Impl helpers and the two replay
// helpers have transitive dependencies on List Z statics that stay in the
// main file, so their definitions cannot move with the target member
// functions — only external linkage is granted here.
extern const char * const GBE_kDotaOfficial032PracticeLobby26Hex;


// --- Phase 2.9: payload-helper statics promoted to external ---
// These symbols were file-scope static helpers in steam_game_coordinator.cpp
// and are now defined in gbe_dota_gc_payload_helpers.cpp. The main GC TU
// still references them, so they have external linkage with extern
// declarations here. List X helpers (only referenced from other moved
// helpers) remain `static` inside the payload-helpers TU and are not
// declared here.

// Forward declaration for the lobby-objects builder signature below.
namespace gbe::gc_message { struct DotaPracticeLobbyObjects; }

extern const char * const GBE_kDotaPracticeLobbyLaunchCacheSubscribedOfficialHex;
extern const uint8 GBE_kDotaPracticeLobbyCacheSubscribedTemplate[312];

void GBE_LogGCProtoBoundary(const char *scope, const char *direction, void *self, bool is_server, uint32 emsg, const void *data, uint32 size);

// Note: GBE_AdaptDotaPracticeLobbyCacheSubscribedPayload,
// GBE_AdaptDotaPracticeLobbyDetailsUpdatePurePayload, and
// GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedFromWrappedTemplate were
// removed from this header (Phase 3.5.3). They are only referenced from within
// their defining TU (gbe_dota_payload_lobby_helpers.cpp) and have been marked
// `static` there. Keep gbe_dota_gc_internal.h limited to genuine cross-TU
// declarations.

#endif // __INCLUDED_GBE_DOTA_GC_INTERNAL_H__
