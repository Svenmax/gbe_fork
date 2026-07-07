#!/usr/bin/env python3
"""Phase 2.5 hotfix: remove redefined default arguments from 3 function
definitions in steam_game_coordinator.cpp.

Phase 2.5 externalized 6 static symbols. For 3 of them, the function
definitions in the main file contain a default argument
(`custom_game = nullptr`) that is ALSO present in the extern declaration
added to gbe_dota_gc_internal.h. C++ forbids specifying a default argument
in more than one declaration; MSVC reports error C2572.

The 3 offending definitions (line numbers are 1-based, current file state):
  L1785  GBE_ReplayDotaPracticeLobbyOfficial26Payload
  L2565  GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedTemplate
  L2643  GBE_AdaptDotaPracticeLobbyDetailsUpdatePayload

Fix: strip ` = nullptr` from the trailing parameter in each of these 3
definitions. The header declaration keeps the default argument, so callers
can still omit it. The other 4 occurrences of the same line pattern belong
to static functions that were NOT externalized (no header declaration
exists for them), so their definitions legitimately keep the default
argument — we must NOT touch those.

This script locates the 3 target lines by (line number, content) and
rewrites only them.
"""
import sys

CPP_PATH = "/workspace/dll/steam_game_coordinator.cpp"

# 1-based line numbers of the offending default-argument definitions.
# Verified against build log of run #963 (commit 4294660) on MSVC.
TARGET_LINES = {1785, 2565, 2643}

EXPECTED_OLD_SUFFIX = "const GBE_DotaCustomGameDetails *custom_game = nullptr)"
NEW_SUFFIX = "const GBE_DotaCustomGameDetails *custom_game)"


def main():
    with open(CPP_PATH, "r") as f:
        lines = f.readlines()

    changed = 0
    for idx, line in enumerate(lines):
        lineno = idx + 1
        if lineno not in TARGET_LINES:
            continue
        stripped = line.rstrip("\n")
        if not stripped.endswith(EXPECTED_OLD_SUFFIX):
            print(f"ERROR: line {lineno} does not end with expected suffix")
            print(f"  got: {stripped}")
            sys.exit(1)
        new_line = stripped[:len(stripped) - len(EXPECTED_OLD_SUFFIX)] + NEW_SUFFIX + "\n"
        lines[idx] = new_line
        changed += 1
        print(f"  fixed L{lineno}: {stripped.strip()}")

    if changed != len(TARGET_LINES):
        print(f"ERROR: expected to fix {len(TARGET_LINES)} lines, fixed {changed}")
        sys.exit(1)

    with open(CPP_PATH, "w") as f:
        f.writelines(lines)

    print(f"\nFixed {changed} default-argument redefinitions in {CPP_PATH}")


if __name__ == "__main__":
    main()
