#!/usr/bin/env python3
"""Comprehensive audit of the GC refactor.

Checks:
  1. Every GBE_* declaration in gbe_dota_gc_internal.h has a matching
     definition somewhere in the GC TUs (find zombie declarations).
  2. Every GBE_* free-function definition in main file + payload_helpers TU
     has a declaration in the header (find under-exposed definitions).
  3. Doc line numbers in REFACTOR_TODO.md match actual code.
"""
import os
import re
import glob

INTERNAL_H = "/workspace/dll/gbe_dota_gc_internal.h"
MAIN_CPP = "/workspace/dll/steam_game_coordinator.cpp"
TODO_MD = "/workspace/REFACTOR_TODO.md"
GC_TUS = sorted(glob.glob("/workspace/dll/gbe_dota_*.cpp")) + [MAIN_CPP]


def read(path):
    with open(path, "r", encoding="utf-8", errors="replace") as f:
        return f.read()


def extract_header_decls(header_text):
    """Extract GBE_* identifiers from declaration lines in header."""
    decls = set()
    for m in re.finditer(r"\b(GBE_\w+)\b", header_text):
        name = m.group(1)
        # Skip struct/typedef names that are types, not functions/vars.
        # We only care about callable extern symbols; keep all and filter later.
        decls.add(name)
    return decls


def extract_defined_symbols(tu_paths):
    """Extract GBE_* symbols that are DEFINED (have a body) in the TUs.

    A definition is a line starting with a type and containing GBE_* followed
    by '(' and later a '{' on the same or following lines. We approximate:
    match lines like `bool GBE_Foo(` or `extern const ... GBE_Foo =` that are
    NOT terminated with ';' on the first line (i.e., multi-line def) OR are
    single-line `= ...` assignments.
    """
    defined = {}  # name -> (file, lineno)
    for path in tu_paths:
        lines = read(path).splitlines()
        n = len(lines)
        for i, ln in enumerate(lines):
            # function definition: "<type> GBE_Foo(" not ending with ';'
            m = re.match(r"^[A-Za-z_][\w:&*\s<>,]*\b(GBE_\w+)\s*\(", ln)
            if m and not ln.rstrip().endswith(";"):
                name = m.group(1)
                # Confirm there's a '{' within next few lines (definition body).
                for j in range(i, min(i + 40, n)):
                    if "{" in lines[j]:
                        defined[name] = (os.path.basename(path), i + 1)
                        break
                    if lines[j].rstrip().endswith(";"):
                        break  # declaration, not definition
                continue
            # extern const var definition: "extern const ... GBE_Foo ="
            m = re.match(r"^extern\s+.*\b(GBE_\w+)\s*=", ln)
            if m:
                defined[m.group(1)] = (os.path.basename(path), i + 1)
                continue
            # non-extern const var definition that's a List Y (has extern in header)
            # e.g. "const char *GBE_Foo = " (no extern prefix but externally linked via header)
            # Skip these; they're internal. We only track extern-prefixed or function defs.
        # member function definitions: "Type Steam_Game_Coordinator::GBE_Foo("
        for i, ln in enumerate(lines):
            m = re.match(r"^[A-Za-z_][\w:&*\s<>,]*\bSteam_Game_Coordinator::(GBE_\w+)\s*\(", ln)
            if m and not ln.rstrip().endswith(";"):
                name = m.group(1)
                for j in range(i, min(i + 40, n)):
                    if "{" in lines[j]:
                        defined[name] = (os.path.basename(path), i + 1)
                        break
                    if lines[j].rstrip().endswith(";"):
                        break
    return defined


def main():
    header_text = read(INTERNAL_H)
    header_decls = extract_header_decls(header_text)
    # Filter header decls to only function/variable-like (exclude pure type names
    # that appear only in struct/using/typedef). Heuristic: a decl is "external
    # symbol" if it appears on a line starting with a type keyword or 'extern'.
    real_decls = set()
    for ln in header_text.splitlines():
        s = ln.strip()
        if re.match(r"^(extern\s+|bool\s+|void\s+|std::string\s+|uint\d+\s+|const\s+|int\s+|size_t\s+)", s):
            for m in re.finditer(r"\b(GBE_\w+)\b", s):
                real_decls.add(m.group(1))
        # also template/struct member decls are not extern symbols; skip.

    defined = extract_defined_symbols(GC_TUS)

    print("=" * 70)
    print("AUDIT 1: Header declarations WITHOUT any definition (zombie decls)")
    print("=" * 70)
    zombies = sorted(real_decls - set(defined.keys()))
    if not zombies:
        print("  (none) - all header declarations have a definition")
    else:
        for z in zombies:
            print(f"  GBE_{z[4:]}: declared in header but NO definition found in any TU")
    print()

    print("=" * 70)
    print("AUDIT 2: Definitions WITHOUT header declaration (under-exposed)")
    print("=" * 70)
    # Only check free functions and extern vars in main + payload_helpers,
    # since those are the "shared" TUs. Member functions are visible via class header.
    underexposed = []
    for name, (f, ln) in sorted(defined.items(), key=lambda x: (x[1][0], x[1][1])):
        if name not in real_decls:
            # Check if it's a member function (visible via class header, OK)
            # or a static (internal, OK). We approximate: if defined in main/payload
            # TU as a free function and not in header, flag it.
            if f in ("steam_game_coordinator.cpp", "gbe_dota_gc_payload_helpers.cpp"):
                underexposed.append((name, f, ln))
    if not underexposed:
        print("  (none) - all shared free-function/extern-var definitions are header-declared")
    else:
        for name, f, ln in underexposed:
            print(f"  GBE_{name[4:]}: defined at {f}:{ln} but NOT in header")
    print()

    print("=" * 70)
    print("AUDIT 3: Doc line-number accuracy (REFACTOR_TODO.md vs actual)")
    print("=" * 70)
    main_lines = read(MAIN_CPP).splitlines()
    todo_text = read(TODO_MD)
    # Extract doc-claimed member function positions: "L<digits>  <name>"
    doc_fns = {}
    for m in re.finditer(r"^L(\d+)\s+(\S+)", todo_text, re.MULTILINE):
        doc_fns[m.group(2)] = int(m.group(1))
    mismatches = []
    for fn, claimed_line in doc_fns.items():
        actual = None
        for i, ln in enumerate(main_lines):
            if re.search(r"\bSteam_Game_Coordinator::" + re.escape(fn) + r"\s*\(", ln):
                actual = i + 1
                break
        if actual is None:
            mismatches.append((fn, claimed_line, "NOT FOUND in main file"))
        elif abs(actual - claimed_line) > 2:
            mismatches.append((fn, claimed_line, f"actual L{actual}"))
    if not mismatches:
        print(f"  All {len(doc_fns)} documented line numbers match (within +-2 lines)")
    else:
        for fn, claimed, msg in mismatches:
            print(f"  {fn}: doc says L{claimed}, {msg}")
    print()

    print("=" * 70)
    print("SUMMARY")
    print("=" * 70)
    print(f"  Header extern/function declarations: {len(real_decls)}")
    print(f"  Definitions found across all TUs:    {len(defined)}")
    print(f"  Zombie declarations (no def):        {len(zombies)}")
    print(f"  Under-exposed definitions:           {len(underexposed)}")
    print(f"  Doc line-number mismatches:          {len(mismatches)}")


if __name__ == "__main__":
    main()
