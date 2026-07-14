#!/usr/bin/env python3
"""Phase 2.4a Part A: Delete 18 dead-code static symbols from steam_game_coordinator.cpp.

Dead FUNCTIONS (5): brace-tracked from signature line to matching closing brace.
Dead VARIABLES (13): from `static` start line to next line ending with `;`.

Deletions are processed bottom-to-top so earlier line indices stay valid.
After each definition, up to 2 immediately-following blank lines are also removed.
"""
import re
import sys

CPP_PATH = "/workspace/dll/steam_game_coordinator.cpp"

DEAD_FUNCTIONS = [
    "GBE_LogHexDump",
    "GBE_LogDotaSOMultipleObjectsSummary",
    "GBE_GenerateDotaLobbyInviteGid",
    "GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedPreludeTemplate",
    "GBE_ReplayDotaPracticeLobbyLaunchCacheSubscribedLargePreludeTemplate",
]

DEAD_VARIABLES = [
    "GBE_kDotaOfficialLobbyStartupAccountDataTemplate",
    "GBE_kLocalDotaPracticeLobbyLoopbackEndpoint",
    "GBE_kSteamPersonaState",
    "GBE_kDotaConductScore",
    "GBE_kDotaBehaviorLevel",
    "GBE_kDotaPlusOriginalStartDate",
    "GBE_kDotaPlusFlags",
    "GBE_kDotaPlusStatusActive",
    "GBE_kDotaAbandonPersonaStatePrivateLobbyPostgameHex",
    "GBE_kDotaPracticeLobbyLaunchCacheSubscribedPreludeHex",
    "GBE_kDotaPracticeLobbyLaunchCacheSubscribedLargePreludeHex",
    "GBE_kDotaPracticeLobbyResponseTemplate",
    "GBE_kDotaPracticeLobbySOUpdateTemplate",
]


def find_static_start(lines, symbol, is_function):
    """Find the line index of the `^static ... SYMBOL` definition.

    A function definition has `SYMBOL(` (symbol directly followed by `(`);
    a variable does not. This avoids false positives from `(` inside
    comments (e.g. `// (Plus launch era)`) or initializer expressions.
    """
    pat = re.compile(r"\b" + re.escape(symbol) + r"\s*\(")
    sym_pat = re.compile(r"\b" + re.escape(symbol) + r"\b")
    for i, line in enumerate(lines):
        if not re.match(r"^static\s", line):
            continue
        if not sym_pat.search(line):
            continue
        is_func = bool(pat.search(line))
        if is_func == is_function:
            return i
    raise RuntimeError(
        "Could not find %s start line for symbol %s" % (
            "function" if is_function else "variable", symbol))


def find_function_end(lines, start):
    """From the signature line, track brace depth (skipping strings/comments)
    until depth returns to 0. Return the index of the closing brace line."""
    depth = 0
    in_string = False
    in_char = False
    in_block_comment = False
    for i in range(start, len(lines)):
        line = lines[i]
        in_line_comment = False
        j = 0
        n = len(line)
        while j < n:
            c = line[j]
            nxt = line[j + 1] if j + 1 < n else ""
            if in_line_comment:
                break
            if in_block_comment:
                if c == "*" and nxt == "/":
                    in_block_comment = False
                    j += 2
                    continue
                j += 1
                continue
            if in_string:
                if c == "\\":
                    j += 2
                    continue
                if c == '"':
                    in_string = False
                    j += 1
                    continue
                j += 1
                continue
            if in_char:
                if c == "\\":
                    j += 2
                    continue
                if c == "'":
                    in_char = False
                    j += 1
                    continue
                j += 1
                continue
            if c == "/" and nxt == "/":
                in_line_comment = True
                break
            if c == "/" and nxt == "*":
                in_block_comment = True
                j += 2
                continue
            if c == '"':
                in_string = True
                j += 1
                continue
            if c == "'":
                in_char = True
                j += 1
                continue
            if c == "{":
                depth += 1
                j += 1
                continue
            if c == "}":
                depth -= 1
                j += 1
                if depth == 0:
                    return i
                continue
            j += 1
        if depth == 0:
            # Signature line had no opening brace yet; keep going.
            pass
    raise RuntimeError(
        "Could not find closing brace for function starting at line %d" % (start + 1))


def find_variable_end(lines, start):
    """From the start line, find the next line whose stripped content ends with ';'."""
    for i in range(start, len(lines)):
        if lines[i].rstrip().endswith(";"):
            return i
    raise RuntimeError(
        "Could not find terminating ';' for variable starting at line %d" % (start + 1))


def main():
    with open(CPP_PATH, "r") as f:
        raw = f.read()
    lines = raw.split("\n")
    original_count = len(lines)

    ranges = []  # (start, end, kind, symbol)
    for sym in DEAD_FUNCTIONS:
        s = find_static_start(lines, sym, is_function=True)
        e = find_function_end(lines, s)
        ranges.append((s, e, "function", sym))
    for sym in DEAD_VARIABLES:
        s = find_static_start(lines, sym, is_function=False)
        e = find_variable_end(lines, s)
        ranges.append((s, e, "variable", sym))

    # Extend each range to consume up to 2 trailing blank lines.
    extended = []
    for (s, e, kind, sym) in ranges:
        end = e
        consumed = 0
        while end + 1 < len(lines) and lines[end + 1].strip() == "" and consumed < 2:
            end += 1
            consumed += 1
        extended.append((s, end, kind, sym))

    # Sort descending by start so earlier indices remain valid during deletion.
    extended.sort(key=lambda r: r[0], reverse=True)

    print("=== Phase 2.4a Part A: dead-code deletion plan ===")
    total_removed = 0
    for (s, e, kind, sym) in extended:
        count = e - s + 1
        total_removed += count
        print("  [%s] %-58s lines %5d-%5d  (%d lines)" % (kind[0].upper(), sym, s + 1, e + 1, count))
    print("=== Total lines to remove: %d ===" % total_removed)
    print()

    # Build delete set and rebuild file.
    delete_set = set()
    for (s, e, _kind, _sym) in extended:
        for i in range(s, e + 1):
            delete_set.add(i)

    new_lines = [line for i, line in enumerate(lines) if i not in delete_set]

    with open(CPP_PATH, "w") as f:
        f.write("\n".join(new_lines))

    print("Original line count: %d" % original_count)
    print("New line count:      %d" % len(new_lines))
    print("Lines removed:       %d" % (original_count - len(new_lines)))


if __name__ == "__main__":
    main()
