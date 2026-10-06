#!/usr/bin/env python3
"""Phase 2.7 build fix: Move GCMsgHdrEx_t struct definition to the header.

The build failed (C2079/C2027) because gbe_dota_inventory_coordinator.cpp uses
`GCMsgHdrEx_t hdr = parse_msg_header(p);` (by value), but only had a forward
declaration from steam_game_coordinator.h. The full definition lived in
steam_game_coordinator.cpp inside a #pragma pack region.

Fix:
  1. In dll/dll/steam_game_coordinator.h: replace the forward declaration
     `struct GCMsgHdrEx_t;` with the full struct definition, wrapped in
     #pragma pack(push, 1) / #pragma pack(pop). JobID_t is already available
     here (the header uses it in method signatures; pulled in via base.h).
  2. In dll/steam_game_coordinator.cpp: remove the GCMsgHdrEx_t comment block
     + struct definition only. KEEP GCMsgHdr_t, #pragma pack(push,1), and
     #pragma pack(pop) intact.

GCMsgHdr_t stays in the .cpp (only used there: sizeof at L3577/L3582).
"""
import sys

HEADER_PATH = "/workspace/dll/dll/steam_game_coordinator.h"
CPP_PATH = "/workspace/dll/steam_game_coordinator.cpp"

# The full struct definition to insert into the header (replacing forward decl).
HEADER_STRUCT_BLOCK = """#pragma pack( push, 1 )
struct GCMsgHdrEx_t
{
    uint32  m_eMsg;                     // The message type
    uint64  m_ulSteamID;                // User's SteamID
    uint16  m_nHdrVersion;
    JobID_t m_JobIDTarget;
    JobID_t m_JobIDSource;
};
#pragma pack(pop)"""


def main():
    # === 1. Patch header: replace forward declaration with full definition ===
    with open(HEADER_PATH, "r") as f:
        hdr = f.read()
    hdr_lines = hdr.split("\n")
    print(f"Header before: {len(hdr_lines)} lines")

    fwd_idx = None
    for i, line in enumerate(hdr_lines):
        if line.strip() == "struct GCMsgHdrEx_t;":
            fwd_idx = i
            break
    if fwd_idx is None:
        print("  [ERROR] could not find `struct GCMsgHdrEx_t;` forward declaration", file=sys.stderr)
        sys.exit(1)

    print(f"  [hdr] replacing forward declaration at line {fwd_idx+1} with full struct definition")
    block_lines = HEADER_STRUCT_BLOCK.split("\n")
    hdr_lines[fwd_idx:fwd_idx+1] = block_lines

    hdr_new = "\n".join(hdr_lines)
    with open(HEADER_PATH, "w") as f:
        f.write(hdr_new)
    print(f"Header after:  {len(hdr_lines)} lines (+{len(block_lines) - 1})")

    # === 2. Patch main .cpp: remove the GCMsgHdrEx_t definition ===
    with open(CPP_PATH, "r") as f:
        cpp = f.read()
    cpp_lines = cpp.split("\n")
    print(f"\nMain .cpp before: {len(cpp_lines)} lines")

    # Find the GCMsgHdrEx_t struct definition
    struct_idx = None
    for i, line in enumerate(cpp_lines):
        if line.strip() == "struct GCMsgHdrEx_t":
            struct_idx = i
            break
    if struct_idx is None:
        print("  [ERROR] could not find `struct GCMsgHdrEx_t` definition in .cpp", file=sys.stderr)
        sys.exit(1)

    # Find the closing brace of the struct. For a struct, the closing line is `};`
    # (not just `}`). Scan for the first line whose stripped content is `};`.
    struct_close = None
    for j in range(struct_idx, len(cpp_lines)):
        if cpp_lines[j].strip() == "};":
            struct_close = j
            break
    if struct_close is None:
        print("  [ERROR] could not find GCMsgHdrEx_t closing `};`", file=sys.stderr)
        sys.exit(1)

    # Scan backwards from struct_idx to find the contiguous comment block
    # (lines starting with `//`). Stop at the first non-comment line.
    comment_start = struct_idx
    while comment_start > 0 and cpp_lines[comment_start - 1].startswith("//"):
        comment_start -= 1

    # Also consume one trailing blank line after the closing `};` (if present)
    end_excl = struct_close + 1
    if end_excl < len(cpp_lines) and cpp_lines[end_excl].strip() == "":
        end_excl += 1

    # Print what we're removing
    print(f"  [cpp] removing lines {comment_start+1}-{end_excl} (GCMsgHdrEx_t comment + struct):")
    for k in range(comment_start, end_excl):
        print(f"        {k+1:5d}: {cpp_lines[k]}")

    del cpp_lines[comment_start:end_excl]

    cpp_new = "\n".join(cpp_lines)
    with open(CPP_PATH, "w") as f:
        f.write(cpp_new)
    removed = end_excl - comment_start
    print(f"\nMain .cpp after:  {len(cpp_lines)} lines (-{removed})")

    # === 3. Verify GCMsgHdr_t and #pragma pack are still intact ===
    has_pack_push = any("pragma pack" in l and "push" in l for l in cpp_lines)
    has_pack_pop = any("pragma pack" in l and "pop" in l for l in cpp_lines)
    has_gcmsg = any(l.strip() == "struct GCMsgHdr_t" for l in cpp_lines)
    has_gcmsgex_def = any(l.strip() == "struct GCMsgHdrEx_t" for l in cpp_lines)
    # Check the line after GCMsgHdr_t's closing }; — should be blank then #pragma pack(pop)
    print(f"\n  [check] #pragma pack(push) present:  {has_pack_push}")
    print(f"  [check] #pragma pack(pop) present:    {has_pack_pop}")
    print(f"  [check] struct GCMsgHdr_t present:    {has_gcmsg}")
    print(f"  [check] struct GCMsgHdrEx_t REMOVED:  {not has_gcmsgex_def}")

    # Show context around where GCMsgHdrEx_t was removed
    gcmsg_idx = None
    for i, line in enumerate(cpp_lines):
        if line.strip() == "struct GCMsgHdr_t":
            gcmsg_idx = i
            break
    if gcmsg_idx is not None:
        print(f"\n  [context] lines around GCMsgHdr_t (now at line {gcmsg_idx+1}):")
        for k in range(max(0, gcmsg_idx - 2), min(len(cpp_lines), gcmsg_idx + 12)):
            print(f"        {k+1:5d}: {cpp_lines[k]}")

    print("\nDone. Phase 2.7 struct visibility fix complete.")


if __name__ == "__main__":
    main()
