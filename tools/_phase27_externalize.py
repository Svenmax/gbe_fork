#!/usr/bin/env python3
"""Phase 2.7 Step 3: Externalize the `deser_var` template (List Y, 1 symbol).

`deser_var` is a file-scope `static` template function in steam_game_coordinator.cpp
referenced from BOTH the inventory/item target functions (5 call sites) AND code
that stays in the main file (2 call sites in handle_respawn). Because it is a
template, it cannot be made `extern` like a regular function — the definition
must be visible at instantiation point in every TU. Following the pattern set
in Phase 2.3a for `ser_var`, we MOVE the template definition to the internal
header `gbe_dota_gc_internal.h` (as `template <class T> inline`), and delete
the definition from the main .cpp.

No default arguments are involved, so no C2572 risk.

Files modified:
  - dll/gbe_dota_gc_internal.h : add `#include <cstring>` and `deser_var` template
  - dll/steam_game_coordinator.cpp : remove the `deser_var` template definition
"""
import re
import sys

HEADER_PATH = "/workspace/dll/gbe_dota_gc_internal.h"
CPP_PATH = "/workspace/dll/steam_game_coordinator.cpp"

# --- Template definition to insert into the header (mirrors ser_var style) ---
HEADER_INCLUDE_CSTRING = "#include <cstring>"
HEADER_DESER_VAR_BLOCK = """// Template deserializer (moved here so both TUs can instantiate it).
template <class T>
inline T deser_var(const char *&p)
{
    T output;
    memcpy(&output, p, sizeof(T));
    p += sizeof(T);
    return output;
}
"""


def main():
    # === 1. Patch header ===
    with open(HEADER_PATH, "r") as f:
        hdr = f.read()
    hdr_lines = hdr.split("\n")
    print(f"Header before: {len(hdr_lines)} lines")

    # 1a. Add #include <cstring> right after #include <array> (if not already present)
    if HEADER_INCLUDE_CSTRING in hdr:
        print(f"  [skip] {HEADER_INCLUDE_CSTRING} already present in header")
    else:
        for i, line in enumerate(hdr_lines):
            if line.strip() == "#include <array>":
                hdr_lines.insert(i + 1, HEADER_INCLUDE_CSTRING)
                print(f"  [hdr] inserted #include <cstring> after line {i+1} ({line})")
                break
        else:
            print("  [ERROR] could not find #include <array> anchor", file=sys.stderr)
            sys.exit(1)

    # 1b. Insert deser_var template block right after the ser_var template's closing brace.
    #     ser_var looks like:
    #       template <class T>
    #       inline void ser_var(std::string &buf, const T &input)
    #       {
    #           buf.append(reinterpret_cast<const char *>(&input), sizeof(T));
    #       }
    #     We anchor on the line `inline void ser_var(` then scan forward to the
    #     matching closing `}` at column 0, then insert after the following blank line.
    ser_anchor = None
    for i, line in enumerate(hdr_lines):
        if line.startswith("inline void ser_var("):
            ser_anchor = i
            break
    if ser_anchor is None:
        print("  [ERROR] could not find `inline void ser_var(` anchor in header", file=sys.stderr)
        sys.exit(1)
    # Find closing brace of ser_var (first line that is exactly "}")
    ser_close = None
    for j in range(ser_anchor, len(hdr_lines)):
        if hdr_lines[j] == "}":
            ser_close = j
            break
    if ser_close is None:
        print("  [ERROR] could not find ser_var closing brace", file=sys.stderr)
        sys.exit(1)
    # Insert deser_var block after ser_close + 1 (preserve one blank line separator)
    insert_at = ser_close + 1
    # Ensure there's a blank line before the new block
    block = HEADER_DESER_VAR_BLOCK.rstrip("\n").split("\n")
    hdr_lines[insert_at:insert_at] = block
    print(f"  [hdr] inserted deser_var template after line {ser_close+1}")

    hdr_new = "\n".join(hdr_lines)
    with open(HEADER_PATH, "w") as f:
        f.write(hdr_new)
    print(f"Header after:  {len(hdr_lines)} lines (+{len(hdr_lines) - (len(hdr.split(chr(10))))})")

    # === 2. Patch main .cpp — remove deser_var template definition ===
    with open(CPP_PATH, "r") as f:
        cpp = f.read()
    cpp_lines = cpp.split("\n")
    print(f"\nMain .cpp before: {len(cpp_lines)} lines")

    # Locate the template definition:
    #   template <class T>
    #   static T deser_var(const char *&p)
    #   {
    #       ...
    #   }
    tpl_idx = None
    for i, line in enumerate(cpp_lines):
        if line.strip() == "template <class T>":
            # Check the next non-blank line is the deser_var signature
            if i + 1 < len(cpp_lines) and "deser_var" in cpp_lines[i + 1] and "static" in cpp_lines[i + 1]:
                tpl_idx = i
                break
    if tpl_idx is None:
        print("  [ERROR] could not find `template <class T>` + `static T deser_var` block", file=sys.stderr)
        sys.exit(1)

    # Find matching closing brace (first line that is exactly "}" starting from tpl_idx)
    tpl_close = None
    for j in range(tpl_idx, len(cpp_lines)):
        if cpp_lines[j] == "}":
            tpl_close = j
            break
    if tpl_close is None:
        print("  [ERROR] could not find deser_var closing brace", file=sys.stderr)
        sys.exit(1)

    # Print what we're removing for verification
    print(f"  [cpp] removing lines {tpl_idx+1}-{tpl_close+1} (deser_var template):")
    for k in range(tpl_idx, tpl_close + 1):
        print(f"        {k+1:5d}: {cpp_lines[k]}")

    # Remove the template block PLUS one trailing blank line (if present) to keep
    # spacing tidy: the preceding function (ser_varstring) already has a blank line
    # after its closing brace.
    end_excl = tpl_close + 1
    if end_excl < len(cpp_lines) and cpp_lines[end_excl].strip() == "":
        end_excl += 1  # also consume one trailing blank line
    del cpp_lines[tpl_idx:end_excl]

    cpp_new = "\n".join(cpp_lines)
    with open(CPP_PATH, "w") as f:
        f.write(cpp_new)
    removed = end_excl - tpl_idx
    print(f"\nMain .cpp after:  {len(cpp_lines)} lines (-{removed})")

    # === 3. Sanity: ensure deser_var no longer defined in .cpp, but still referenced ===
    remaining_defs = [i for i, l in enumerate(cpp_lines) if "deser_var" in l]
    print(f"\n  [check] 'deser_var' still appears in .cpp at lines: {[i+1 for i in remaining_defs]}")
    print(f"  [check] (these should be call sites only, no `static T deser_var` definition)")

    print("\nDone. Phase 2.7 externalize step complete.")
    print("Next: write _phase27_extract.py to move 20 member functions + ser_varstring (List X).")


if __name__ == "__main__":
    main()
