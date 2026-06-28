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

// Shared lobby state cache (defined in steam_game_coordinator.cpp)
struct GBE_SharedDotaLobbyState;
extern GBE_SharedDotaLobbyState GBE_shared_dota_lobby_state;


#include <array>

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

// Shared mutable state
extern bool GBE_pending_dota_normal_signout_finalize_after_25;
extern uint64 GBE_pending_dota_normal_signout_finalize_lobby_id;
extern GBE_DotaLootListData GBE_vpk_loot_data;

// Shared const data tables
extern const std::array<uint8, 4> GBE_kOldDotaAccountIdVarint;
extern const std::array<uint8, 9> GBE_kOldDotaSteamIdVarint;
extern const std::array<uint8, 8> GBE_kOldDotaLobbyIdVarint;
extern const std::array<uint8, 8> GBE_kOldDotaSteamIdFixed64;
extern const std::array<uint8, 8> GBE_kOldDotaPersonaSteamIdFixed64;
extern const std::array<uint8, 4> GBE_kOldDotaAccountIdFixed32;
extern const std::array<uint8, 5> GBE_kOldDotaPracticeLobbyMatchIdVarint;
extern const std::array<uint8, 8> GBE_kOldDotaPracticeLobbyServerIdFixed64;
extern const char *GBE_kOldDotaPracticeLobbyLobbyIdText;
extern const char *GBE_kOldDotaPracticeLobbyLobbyIdTextAlt;
extern const uint32 GBE_kSteamTicketAuthComplete;
extern const char *GBE_kDotaAbandonPersonaStateInitHex;

// Shared helper functions (defined in steam_game_coordinator.cpp)
bool GBE_PushDotaPlayerEquippedItemsCacheToGC(
    Steam_Game_Coordinator *target_gc,
    const CSteamID &player_steam_id,
    const std::vector<Econ_Item> &source_items,
    bool unsubscribe_first,
    const char *reason);

bool GBE_RewriteAccountIdVarintInDirectProtoBody(
    std::string &message,
    uint32 account_id,
    size_t &replacement_count);

bool GBE_TryPatchDotaAccountIdVarint(
    std::string &message,
    uint32 account_id,
    const char *log_scope,
    uint32 request_emsg,
    uint32 response_emsg,
    size_t body_size,
    const char *context_note);

bool GBE_TryPatchDotaAccountIdFixed32(std::string &message, uint32 account_id, const char *log_scope);

std::string GBE_DotaCustomGameDisplayName(class Settings *settings, const GBE_DotaCustomGameDetails &custom_game, const std::string &fallback);

bool GBE_PatchDotaTemplateIdentifiers(
    std::string &message,
    uint32 account_id,
    uint64 steam_id,
    bool replace_account,
    bool replace_steam_id,
    uint32 request_emsg,
    uint32 response_emsg,
    size_t body_size,
    const char *context_note);

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
    std::string &message);

#endif // __INCLUDED_GBE_DOTA_GC_INTERNAL_H__
