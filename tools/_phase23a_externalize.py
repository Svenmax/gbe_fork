#!/usr/bin/env python3
"""Phase 2.3a: Externalize 26 List B static symbols in steam_game_coordinator.cpp.

- Functions: remove `static ` prefix (gives external linkage)
- Mutable variables: remove `static ` prefix
- Const arrays: replace `static const` -> `extern const` (external linkage for const)
- Constexpr data: demote `static constexpr` -> `extern const` (external linkage for const)
- Template ser_var: remove from .cpp (will live in header)

Also appends declarations to gbe_dota_gc_internal.h.
"""
import sys
import os

CPP_PATH = "/workspace/dll/steam_game_coordinator.cpp"
HDR_PATH = "/workspace/dll/gbe_dota_gc_internal.h"

def main():
    with open(CPP_PATH, "r") as f:
        content = f.read()

    original_len = len(content)
    edits = []  # (old, new, expected_count, label)

    # --- Functions: remove `static ` prefix (all occurrences) ---
    func_replacements = [
        ("static bool GBE_PushDotaPlayerEquippedItemsCacheToGC(",
         "bool GBE_PushDotaPlayerEquippedItemsCacheToGC(", 2,
         "GBE_PushDotaPlayerEquippedItemsCacheToGC (fwd decl + def)"),
        ("static bool GBE_RewriteAccountIdVarintInDirectProtoBody(",
         "bool GBE_RewriteAccountIdVarintInDirectProtoBody(", 1,
         "GBE_RewriteAccountIdVarintInDirectProtoBody"),
        ("static bool GBE_TryPatchDotaAccountIdVarint(",
         "bool GBE_TryPatchDotaAccountIdVarint(", 1,
         "GBE_TryPatchDotaAccountIdVarint"),
        ("static bool GBE_TryPatchDotaAccountIdFixed32(",
         "bool GBE_TryPatchDotaAccountIdFixed32(", 1,
         "GBE_TryPatchDotaAccountIdFixed32"),
        ("static std::string GBE_DotaCustomGameDisplayName(",
         "std::string GBE_DotaCustomGameDisplayName(", 1,
         "GBE_DotaCustomGameDisplayName"),
        ("static bool GBE_PatchDotaTemplateIdentifiers(",
         "bool GBE_PatchDotaTemplateIdentifiers(", 1,
         "GBE_PatchDotaTemplateIdentifiers"),
        ("static bool GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage(",
         "bool GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage(", 1,
         "GBE_PrepareDotaPracticeLobbyLaunchPeripheralMessage"),
        ("static bool GBE_PrepareDotaPersonaStatePeripheralMessage(",
         "bool GBE_PrepareDotaPersonaStatePeripheralMessage(", 1,
         "GBE_PrepareDotaPersonaStatePeripheralMessage"),
        ("static void GBE_LogDotaResponsePacket(",
         "void GBE_LogDotaResponsePacket(", 1,
         "GBE_LogDotaResponsePacket"),
        ("static bool GBE_PrepareDotaDirectReplayMessage(",
         "bool GBE_PrepareDotaDirectReplayMessage(", 1,
         "GBE_PrepareDotaDirectReplayMessage"),
    ]

    # --- Mutable variables: remove `static ` ---
    mut_replacements = [
        ("static bool GBE_pending_dota_normal_signout_finalize_after_25 = false;",
         "bool GBE_pending_dota_normal_signout_finalize_after_25 = false;", 1,
         "GBE_pending_dota_normal_signout_finalize_after_25"),
        ("static uint64 GBE_pending_dota_normal_signout_finalize_lobby_id = 0;",
         "uint64 GBE_pending_dota_normal_signout_finalize_lobby_id = 0;", 1,
         "GBE_pending_dota_normal_signout_finalize_lobby_id"),
        ("static GBE_DotaLootListData GBE_vpk_loot_data;",
         "GBE_DotaLootListData GBE_vpk_loot_data;", 1,
         "GBE_vpk_loot_data"),
    ]

    # --- Const arrays: `static const` -> `extern const` ---
    arr_replacements = [
        ("static const std::array<uint8, 4> GBE_kOldDotaAccountIdVarint",
         "extern const std::array<uint8, 4> GBE_kOldDotaAccountIdVarint", 1,
         "GBE_kOldDotaAccountIdVarint"),
        ("static const std::array<uint8, 9> GBE_kOldDotaSteamIdVarint",
         "extern const std::array<uint8, 9> GBE_kOldDotaSteamIdVarint", 1,
         "GBE_kOldDotaSteamIdVarint"),
        ("static const std::array<uint8, 8> GBE_kOldDotaLobbyIdVarint",
         "extern const std::array<uint8, 8> GBE_kOldDotaLobbyIdVarint", 1,
         "GBE_kOldDotaLobbyIdVarint"),
        ("static const std::array<uint8, 8> GBE_kOldDotaSteamIdFixed64",
         "extern const std::array<uint8, 8> GBE_kOldDotaSteamIdFixed64", 1,
         "GBE_kOldDotaSteamIdFixed64"),
        ("static const std::array<uint8, 8> GBE_kOldDotaPersonaSteamIdFixed64",
         "extern const std::array<uint8, 8> GBE_kOldDotaPersonaSteamIdFixed64", 1,
         "GBE_kOldDotaPersonaSteamIdFixed64"),
        ("static const std::array<uint8, 4> GBE_kOldDotaAccountIdFixed32",
         "extern const std::array<uint8, 4> GBE_kOldDotaAccountIdFixed32", 1,
         "GBE_kOldDotaAccountIdFixed32"),
        ("static const std::array<uint8, 5> GBE_kOldDotaPracticeLobbyMatchIdVarint",
         "extern const std::array<uint8, 5> GBE_kOldDotaPracticeLobbyMatchIdVarint", 1,
         "GBE_kOldDotaPracticeLobbyMatchIdVarint"),
        ("static const std::array<uint8, 8> GBE_kOldDotaPracticeLobbyServerIdFixed64",
         "extern const std::array<uint8, 8> GBE_kOldDotaPracticeLobbyServerIdFixed64", 1,
         "GBE_kOldDotaPracticeLobbyServerIdFixed64"),
    ]

    # --- Constexpr data: demote `static constexpr` -> `extern const` ---
    cexpr_replacements = [
        ("static constexpr const char *GBE_kOldDotaPracticeLobbyLobbyIdText =",
         "extern const char *GBE_kOldDotaPracticeLobbyLobbyIdText =", 1,
         "GBE_kOldDotaPracticeLobbyLobbyIdText"),
        ("static constexpr const char *GBE_kOldDotaPracticeLobbyLobbyIdTextAlt =",
         "extern const char *GBE_kOldDotaPracticeLobbyLobbyIdTextAlt =", 1,
         "GBE_kOldDotaPracticeLobbyLobbyIdTextAlt"),
        ("static constexpr uint32 GBE_kSteamTicketAuthComplete =",
         "extern const uint32 GBE_kSteamTicketAuthComplete =", 1,
         "GBE_kSteamTicketAuthComplete"),
        ("static constexpr const char *GBE_kDotaAbandonPersonaStateInitHex =",
         "extern const char *GBE_kDotaAbandonPersonaStateInitHex =", 1,
         "GBE_kDotaAbandonPersonaStateInitHex"),
    ]

    # --- Template ser_var: remove the 5-line block ---
    ser_var_block = (
        "template <class T>\n"
        "static void ser_var(std::string &buf, const T &input)\n"
        "{\n"
        "    buf.append(reinterpret_cast<const char *>(&input), sizeof(T));\n"
        "}\n"
    )

    all_replacements = (func_replacements + mut_replacements +
                        arr_replacements + cexpr_replacements)

    # Apply replacements
    for old, new, expected_count, label in all_replacements:
        actual_count = content.count(old)
        if actual_count != expected_count:
            print(f"ERROR: expected {expected_count} occurrence(s) of '{label}' but found {actual_count}")
            print(f"  Pattern: {old[:80]}...")
            sys.exit(1)
        content = content.replace(old, new)
        print(f"  OK: {label} ({actual_count} replacement(s))")

    # Remove ser_var template block (with surrounding blank lines cleaned up)
    sv_count = content.count(ser_var_block)
    if sv_count != 1:
        print(f"ERROR: expected 1 occurrence of ser_var template block but found {sv_count}")
        sys.exit(1)
    # Remove the block and one preceding blank line if present
    content = content.replace("\n" + ser_var_block, "\n", 1)
    print("  OK: ser_var template removed")

    with open(CPP_PATH, "w") as f:
        f.write(content)
    print(f"\nModified {CPP_PATH}: {original_len} -> {len(content)} chars")

    # --- Now update the header ---
    with open(HDR_PATH, "r") as f:
        hdr = f.read()

    # Check we haven't already applied this
    if "GBE_PushDotaPlayerEquippedItemsCacheToGC" in hdr:
        print("ERROR: header already contains List B declarations. Aborting.")
        sys.exit(1)

    new_decls = '''
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

'''

    # Insert before the #endif
    endif_marker = "#endif // __INCLUDED_GBE_DOTA_GC_INTERNAL_H__"
    if endif_marker not in hdr:
        print("ERROR: could not find #endif marker in header")
        sys.exit(1)
    hdr = hdr.replace(endif_marker, new_decls + endif_marker)

    with open(HDR_PATH, "w") as f:
        f.write(hdr)
    print(f"Updated {HDR_PATH}")

    print("\nPhase 2.3a externalization complete.")

if __name__ == "__main__":
    main()
