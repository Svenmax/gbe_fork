#!/usr/bin/env python3
"""Phase 2.12: Move 6 payload-rewrite free functions from main file to
gbe_dota_gc_payload_helpers.cpp (existing TU, Phase 2.9 product).

The 6 functions are payload-rewrite / template-replay helpers that belong
thematically with the 28 static helpers already in gbe_dota_gc_payload_helpers.cpp
(Phase 2.9). Moving them consolidates all payload-rewrite logic in one TU.

Target functions (all already declared in gbe_dota_gc_internal.h):
  Block 1 (L891-L1225, 4 functions, contiguous):
    - GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedTemplate  (L891-L962)
    - GBE_AdaptDotaPracticeLobbyDetailsUpdatePayload            (L964-L1127)
    - GBE_PrepareDotaDirectReplayMessage                        (L1129-L1173)
    - GBE_BuildDirectDotaServerWelcome                          (L1175-L1225)
  Block 2 (L1903-L2073, 2 functions, contiguous):
    - GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl (L1903-L1995)
    - GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl        (L1997-L2073)

Coupling analysis: List X = 0, List Y = 0.
  - All 6 functions already have declarations in gbe_dota_gc_internal.h
    (L153, L203, L254, L286, L323, L358).
  - Referenced symbols already header-visible:
    * GBE_PatchDotaTemplateIdentifiers (internal.h L118)
    * GBE_ReplayDotaPracticeLobbyOfficial26Payload (internal.h L219)
    * GBE_kDotaOfficial032PracticeLobby26Hex (internal.h L217 extern)
    * GBE_GC_DebugLog (internal.h L17)
    * GBE_ParseDirectProtoContext / GBE_DirectProtoContext (request_router.h L62/L74)
    * GBE_DotaServerHelloContext (internal.h L169 struct)
    * GBE_kProtoMask (protocol_constants.h L34)
    * ser_var (internal.h template)
    * Symbols in payload_helpers TU (Phase 2.9, all header-declared):
      GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedFromWrappedTemplate,
      GBE_ComposeDotaPracticeLobbySOObjects, GBE_ForceDotaLobbyUpdateOwnerSOID,
      GBE_AdaptDotaPracticeLobbyDetailsUpdatePurePayload,
      GBE_PatchDotaLobbyTemplateIdentifiers,
      GBE_PatchDotaPracticeLobbyCacheSubscribedTemplateState,
      GBE_kDotaPracticeLobbyCacheSubscribedTemplate,
      GBE_kDotaPracticeLobbyLaunchCacheSubscribedOfficialHex
    * gbe::dota_gc_wire::*, gbe::proto_wire::*, gbe::gc_message::* (namespaces)

Extraction strategy:
  - Append the 6 function bodies (Block 1 then Block 2) to the END of
    gbe_dota_gc_payload_helpers.cpp (after existing content, separated by a
    blank line). No header changes needed.
  - Remove both blocks from the main file. Block 1 is followed by a member
    function (GBE_DispatchDotaPostLoginRequest at L1227); Block 2 is followed
    by a member function (GBE_GetDotaJoinableCustomLobbiesHTTPJSON at L2082).
  - Handle the two blocks in reverse order (Block 2 first, then Block 1) to
    keep line numbers valid during deletion.
"""
import os
import re
import sys

MAIN_CPP = "/workspace/dll/steam_game_coordinator.cpp"
PAYLOAD_TU = "/workspace/dll/gbe_dota_gc_payload_helpers.cpp"
REPORT_PATH = "/workspace/tools/_phase32_coupling_report.txt"

# Exact line ranges (1-indexed inclusive), determined by manual inspection.
# Block 1: 4 contiguous functions L891-L1225
BLOCK_1 = (891, 1225)
# Block 2: 2 contiguous functions L1903-L2073
BLOCK_2 = (1903, 2073)

TARGET_FNS = [
    "GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedTemplate",
    "GBE_AdaptDotaPracticeLobbyDetailsUpdatePayload",
    "GBE_PrepareDotaDirectReplayMessage",
    "GBE_BuildDirectDotaServerWelcome",
    "GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl",
    "GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedPayloadImpl",
]


def read_lines(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read().splitlines(keepends=False)


def main():
    main_lines = read_lines(MAIN_CPP)
    payload_lines = read_lines(PAYLOAD_TU)
    orig_main = len(main_lines)
    orig_payload = len(payload_lines)

    report = []
    report.append("=" * 78)
    report.append("Phase 2.12: Move 6 payload free functions to payload_helpers TU")
    report.append("=" * 78)
    report.append("")
    report.append(f"Main file: {MAIN_CPP} ({orig_main} lines)")
    report.append(f"Payload TU: {PAYLOAD_TU} ({orig_payload} lines)")
    report.append("")
    report.append("Target blocks (1-indexed inclusive):")
    report.append(f"  Block 1: L{BLOCK_1[0]}-L{BLOCK_1[1]} ({BLOCK_1[1]-BLOCK_1[0]+1} lines, 4 fns)")
    report.append(f"  Block 2: L{BLOCK_2[0]}-L{BLOCK_2[1]} ({BLOCK_2[1]-BLOCK_2[0]+1} lines, 2 fns)")
    report.append("")

    # Verify block boundaries: first line of each block should be a function signature.
    b1_first = main_lines[BLOCK_1[0] - 1]
    b2_first = main_lines[BLOCK_2[0] - 1]
    report.append(f"Block 1 first line: {b1_first.strip()[:80]}")
    report.append(f"Block 2 first line: {b2_first.strip()[:80]}")
    if not b1_first.startswith("bool GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedTemplate"):
        report.append("ERROR: Block 1 start mismatch")
        print("\n".join(report))
        sys.exit(1)
    if not b2_first.startswith("bool GBE_BuildCurrentDotaPracticeLobbyCacheSubscribedTemplateReplayImpl"):
        report.append("ERROR: Block 2 start mismatch")
        print("\n".join(report))
        sys.exit(1)

    # Verify block ends: last line should be '}'.
    b1_last = main_lines[BLOCK_1[1] - 1].strip()
    b2_last = main_lines[BLOCK_2[1] - 1].strip()
    if b1_last != "}":
        report.append(f"ERROR: Block 1 end is not '}}': {b1_last!r}")
        print("\n".join(report))
        sys.exit(1)
    if b2_last != "}":
        report.append(f"ERROR: Block 2 end is not '}}': {b2_last!r}")
        print("\n".join(report))
        sys.exit(1)
    report.append("Block boundary check: OK")
    report.append("")

    # Extract block contents.
    block1 = main_lines[BLOCK_1[0] - 1:BLOCK_1[1]]
    block2 = main_lines[BLOCK_2[0] - 1:BLOCK_2[1]]

    # Verify all 6 function signatures are present in extracted blocks.
    extracted_text = "\n".join(block1 + [""] + block2)
    found = 0
    for fn in TARGET_FNS:
        pat = re.compile(r"\b" + re.escape(fn) + r"\s*\(")
        if pat.search(extracted_text):
            found += 1
    report.append(f"Function signatures found in extracted blocks: {found}/{len(TARGET_FNS)}")
    if found != len(TARGET_FNS):
        report.append("ERROR: not all functions found in blocks")
        print("\n".join(report))
        sys.exit(1)
    report.append("")

    # --- Append to payload TU ---
    # Strip trailing blank lines from payload TU, then add separator + blocks.
    while payload_lines and payload_lines[-1].strip() == "":
        payload_lines.pop()
    new_payload = payload_lines + ["", "// --- Phase 2.12: payload-rewrite free functions (moved from main file) ---", ""] + block1 + [""] + block2

    # --- Remove blocks from main file (reverse order: Block 2 first) ---
    # Block 2: remove L1903-L2073. Also consume trailing blank lines after it.
    # Block 1: remove L891-L1225. Also consume trailing blank lines after it.
    new_main = list(main_lines)

    # Remove Block 2 (higher line numbers first).
    b2_start_idx = BLOCK_2[0] - 1
    b2_end_idx = BLOCK_2[1]  # exclusive
    del new_main[b2_start_idx:b2_end_idx]
    # Consume up to 2 trailing blank lines after Block 2.
    while b2_start_idx < len(new_main) and new_main[b2_start_idx].strip() == "":
        del new_main[b2_start_idx]

    # Remove Block 1 (line numbers unchanged since Block 2 was after it).
    b1_start_idx = BLOCK_1[0] - 1
    b1_end_idx = BLOCK_1[1]  # exclusive
    del new_main[b1_start_idx:b1_end_idx]
    # Consume up to 2 trailing blank lines after Block 1.
    while b1_start_idx < len(new_main) and new_main[b1_start_idx].strip() == "":
        del new_main[b1_start_idx]

    # Write files.
    with open(PAYLOAD_TU, "w", encoding="utf-8") as f:
        f.write("\n".join(new_payload) + "\n")
    with open(MAIN_CPP, "w", encoding="utf-8") as f:
        f.write("\n".join(new_main) + "\n")

    # Verify: none of the 6 functions remain as definitions in main file.
    remaining = 0
    for fn in TARGET_FNS:
        pat = re.compile(r"^[A-Za-z_][\w:&*\s<>,]*\b" + re.escape(fn) + r"\s*\(")
        if any(pat.match(ln) for ln in new_main):
            remaining += 1
    # Verify: all 6 functions are now defined in payload TU.
    in_payload = 0
    for fn in TARGET_FNS:
        pat = re.compile(r"^[A-Za-z_][\w:&*\s<>,]*\b" + re.escape(fn) + r"\s*\(")
        if any(pat.match(ln) for ln in new_payload):
            in_payload += 1

    report.append("")
    report.append(f"Original main file:     {orig_main} lines")
    report.append(f"New main file:          {len(new_main)} lines (delta {len(new_main) - orig_main})")
    report.append(f"Original payload TU:    {orig_payload} lines")
    report.append(f"New payload TU:         {len(new_payload)} lines (delta {len(new_payload) - orig_payload})")
    report.append(f"Definitions moved to payload TU: {in_payload}/{len(TARGET_FNS)}")
    report.append(f"Definitions remaining in main: {remaining} (expected 0)")

    with open(REPORT_PATH, "w", encoding="utf-8") as f:
        f.write("\n".join(report) + "\n")
    print("\n".join(report[-10:]))
    if remaining != 0 or in_payload != len(TARGET_FNS):
        print("ERROR: verification failed", file=sys.stderr)
        sys.exit(1)
    print("OK")


if __name__ == "__main__":
    main()
