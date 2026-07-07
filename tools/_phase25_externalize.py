#!/usr/bin/env python3
"""Phase 2.5 Step 3: Externalize 6 static symbols in steam_game_coordinator.cpp.

Per the coupling report (tools/_phase25_coupling_report.txt), 6 file-scope
`static` symbols are referenced from BOTH the Phase 2.5 target functions
(lobby-snapshot/build helpers to be moved to a new TU) AND from other code
that stays in the main file:

  - 2 List Y symbols (shared between target and stay-behind code):
      * GBE_kDotaOfficial032PracticeLobby26Hex            (constexpr var, L271)
      * GBE_ReplayDotaPracticeLobbyOfficial26Payload      (function,    L1752)

  - 4 List X symbols that have transitive dependencies on List Z statics
    (so they CANNOT move as `static` to the new TU — their definitions must
    stay in the main file with external linkage, and only the 10 target
    *member* functions move):
      * GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedTemplate  (L2535)
      * GBE_AdaptDotaPracticeLobbyDetailsUpdatePayload            (L2608)
      * GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl (L5258)
      * GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl        (L5402)

Transformations:
  - `static constexpr const char *NAME =` -> `extern const char *NAME =`
  - `static bool NAME(`                  -> `bool NAME(`

Definitions stay in steam_game_coordinator.cpp. This script also appends
the matching extern declarations to dll/gbe_dota_gc_internal.h before
the `#endif` guard, following the Phase 2.3a / 2.4a convention.
"""
import sys

CPP_PATH = "/workspace/dll/steam_game_coordinator.cpp"
HDR_PATH = "/workspace/dll/gbe_dota_gc_internal.h"


def main():
    with open(CPP_PATH, "r") as f:
        content = f.read()
    original_len = len(content)
    original_lines = content.count("\n")

    # (old, new, expected_count, label)
    # Each symbol occurs exactly once (verified: no forward declarations).
    replacements = [
        # --- List Y: constexpr variable -> extern const ---
        ("static constexpr const char *GBE_kDotaOfficial032PracticeLobby26Hex =",
         "extern const char *GBE_kDotaOfficial032PracticeLobby26Hex =", 1,
         "GBE_kDotaOfficial032PracticeLobby26Hex (List Y, constexpr var)"),

        # --- List Y: function -> drop `static ` prefix ---
        ("static bool GBE_ReplayDotaPracticeLobbyOfficial26Payload(",
         "bool GBE_ReplayDotaPracticeLobbyOfficial26Payload(", 1,
         "GBE_ReplayDotaPracticeLobbyOfficial26Payload (List Y, function)"),

        # --- List X -> Y (transitive dep on List Z): functions stay, drop `static ` ---
        ("static bool GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedTemplate(",
         "bool GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedTemplate(", 1,
         "GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedTemplate (List X->Y)"),

        ("static bool GBE_AdaptDotaPracticeLobbyDetailsUpdatePayload(",
         "bool GBE_AdaptDotaPracticeLobbyDetailsUpdatePayload(", 1,
         "GBE_AdaptDotaPracticeLobbyDetailsUpdatePayload (List X->Y)"),

        ("static bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl(",
         "bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl(", 1,
         "GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl (List X->Y)"),

        ("static bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl(",
         "bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl(", 1,
         "GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl (List X->Y)"),
    ]

    for old, new, expected_count, label in replacements:
        actual = content.count(old)
        if actual != expected_count:
            print(f"ERROR: expected {expected_count} occurrence(s) of '{label}' but found {actual}")
            print(f"  Pattern: {old[:80]}")
            sys.exit(1)
        content = content.replace(old, new)
        print(f"  OK: {label} ({actual} replacement)")

    with open(CPP_PATH, "w") as f:
        f.write(content)

    new_lines = content.count("\n")
    print(f"\nModified {CPP_PATH}: {original_len} -> {len(content)} chars, "
          f"{original_lines} -> {new_lines} lines (line count must be unchanged)")

    if new_lines != original_lines:
        print("ERROR: line count changed — externalize must only edit prefixes, not add/remove lines.")
        sys.exit(1)

    # --- Update the internal header: append extern declarations before #endif ---
    with open(HDR_PATH, "r") as f:
        hdr = f.read()

    # Guard against double-application.
    if "GBE_ReplayDotaPracticeLobbyOfficial26Payload" in hdr:
        print(f"ERROR: {HDR_PATH} already contains Phase 2.5 declarations. Aborting.")
        sys.exit(1)

    new_decls = """
// --- Phase 2.5: shared lobby-snapshot/build helpers (externalized) ---
// These symbols are used by both the main GC TU and the lobby-snapshot TU
// (gbe_dota_lobby_snapshot_coordinator.cpp). Definitions remain in
// steam_game_coordinator.cpp. The two *Impl helpers and the two replay
// helpers have transitive dependencies on List Z statics that stay in the
// main file, so their definitions cannot move with the target member
// functions — only external linkage is granted here.
extern const char *GBE_kDotaOfficial032PracticeLobby26Hex;

bool GBE_ReplayDotaPracticeLobbyOfficial26Payload(
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
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game = nullptr);

bool GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedTemplate(
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
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game = nullptr);

bool GBE_AdaptDotaPracticeLobbyDetailsUpdatePayload(
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
    std::string &message,
    const GBE_DotaCustomGameDetails *custom_game = nullptr);

bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl(
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
    const GBE_DotaCustomGameDetails *custom_game,
    std::string &message);

bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl(
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
    const GBE_DotaCustomGameDetails *custom_game,
    std::string &message);

"""

    endif_marker = "#endif // __INCLUDED_GBE_DOTA_GC_INTERNAL_H__"
    if endif_marker not in hdr:
        print(f"ERROR: could not find #endif marker in {HDR_PATH}")
        sys.exit(1)
    hdr = hdr.replace(endif_marker, new_decls + endif_marker)

    with open(HDR_PATH, "w") as f:
        f.write(hdr)
    print(f"Updated {HDR_PATH}")

    print("\nPhase 2.5 externalization complete.")


if __name__ == "__main__":
    main()
