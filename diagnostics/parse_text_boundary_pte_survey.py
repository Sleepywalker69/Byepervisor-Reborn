#!/usr/bin/env python3
"""Validate the two 4 KiB-mapped edges of the 13.60 text range."""
import argparse
import hashlib
import json
import re
from pathlib import Path

BEGIN = re.compile(r"^BOUNDARY_BEGIN fw=0x13600007 text=0x([0-9a-f]{16}) "
                   r"root=0x([0-9a-f]{16}) boot_sec=([0-9]+) boot_usec=([0-9]+) count=704$")
ENTRY = re.compile(r"^PTE off=0x([0-9a-f]{6}) value=0x([0-9a-f]{16})$")


def parse(raw: bytes) -> dict:
    lines = raw.decode("utf-8", errors="replace").splitlines()
    starts = [(i, BEGIN.fullmatch(line)) for i, line in enumerate(lines)]
    starts = [(i, item) for i, item in starts if item]
    if len(starts) != 1:
        raise ValueError("expected exactly one BOUNDARY_BEGIN")
    start, begin = starts[0]
    text = int(begin.group(1), 16)
    root = int(begin.group(2), 16)
    sec = int(begin.group(3))
    usec = int(begin.group(4))
    if not root or sec <= 0 or usec >= 1_000_000:
        raise ValueError("invalid provenance fields")
    offsets = [i * 4096 for i in range(448)] + [0xbc0000 + i * 4096 for i in range(256)]
    values = []
    for index, offset in enumerate(offsets):
        if start + 1 + index >= len(lines):
            raise ValueError("truncated entry list")
        item = ENTRY.fullmatch(lines[start + 1 + index])
        if not item or int(item.group(1), 16) != offset:
            raise ValueError(f"wrong PTE offset at index {index}")
        values.append(int(item.group(2), 16))
    end = start + 1 + len(offsets)
    if end >= len(lines) or lines[end] != "BOUNDARY_END count=704":
        raise ValueError("missing exact end marker")
    counts = {
        "present": sum(bool(value & 1) for value in values),
        "xotext": sum(bool(value & (1 << 58)) for value in values),
        "writable": sum(bool(value & 2) for value in values),
        "nx": sum(bool(value & (1 << 63)) for value in values),
        "zero": sum(value == 0 for value in values),
    }
    contiguous_physical = all(
        (values[i] & 0x000ffffffffff000) ==
        ((values[0] & 0x000ffffffffff000) + offsets[i])
        for i in range(len(values))
    )
    return {
        "firmware": "13.60 (0x13600007)",
        "text_base": f"0x{text:016x}",
        "pmap_root": f"0x{root:016x}",
        "console_boot_time": {"seconds": sec, "microseconds": usec},
        "entry_count": len(values),
        "counts": counts,
        "physically_contiguous_within_each_edge": all(
            (values[i] & 0x000ffffffffff000) ==
            ((values[0] & 0x000ffffffffff000) + offsets[i])
            for i in range(448)
        ) and all(
            (values[i] & 0x000ffffffffff000) ==
            ((values[448] & 0x000ffffffffff000) + offsets[i] - offsets[448])
            for i in range(448, len(values))
        ),
        "continuous_from_first_edge_across_gap": contiguous_physical,
        "first_entry": f"0x{values[0]:016x}",
        "last_entry": f"0x{values[-1]:016x}",
        "source_sha256": hashlib.sha256(raw).hexdigest(),
    }


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("log", type=Path)
    args = ap.parse_args()
    print(json.dumps(parse(args.log.read_bytes()), indent=2))


if __name__ == "__main__":
    main()
