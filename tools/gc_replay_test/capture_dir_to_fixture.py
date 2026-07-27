#!/usr/bin/env python3
"""Convert a NetHook capture directory into a gc_replay_test fixture.

Input files are expected to use names like:

    003_in_5453_k_EMsgClientFromGC.bin

The generated fixture keeps the outer emsg from the filename and hex-encodes
the full binary payload. Labels preserve ordering, direction, and message name
so scenario fixtures can be reviewed against the capture summaries.
"""

import argparse
import os
import re
from pathlib import Path


CAPTURE_NAME = re.compile(
    r"^(?P<index>\d+)_(?P<direction>in|out)_(?P<emsg>\d+)_(?P<name>.+)\.bin$"
)


def parse_args() -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture_dir", type=Path)
    parser.add_argument("--output", "-o", type=Path)
    parser.add_argument(
        "--include-direct",
        action="store_true",
        help="include non-GC direct Steam messages as replay rows",
    )
    return parser.parse_args()


def fixture_lines(capture_dir: Path, include_direct: bool) -> list[str]:
    rows: list[tuple[int, str]] = []
    for path in sorted(capture_dir.glob("*.bin")):
        match = CAPTURE_NAME.match(path.name)
        if not match:
            continue

        emsg = int(match.group("emsg"))
        direction = match.group("direction")
        name = match.group("name")
        index = int(match.group("index"))
        if not include_direct and emsg not in (5452, 5453):
            continue

        payload = path.read_bytes().hex()
        label = f"{index:03d}_{direction}_{name}"
        rows.append((index, f"{emsg} {payload} {label}"))

    return [line for _, line in sorted(rows)]


def main() -> int:
    args = parse_args()
    if not args.capture_dir.is_dir():
        raise SystemExit(f"capture directory not found: {args.capture_dir}")

    lines = fixture_lines(args.capture_dir, args.include_direct)
    output = "\n".join(lines) + ("\n" if lines else "")
    if args.output:
        args.output.write_text(output)
    else:
        print(output, end="")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
