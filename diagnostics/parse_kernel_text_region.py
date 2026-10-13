#!/usr/bin/env python3
"""Validate a framed, bounded 13.60 text region before writing its bytes."""
import argparse
import hashlib
import json
import re
from pathlib import Path

BEGIN = re.compile(
    r"^REGION_BEGIN fw=0x13600007 base=0x([0-9a-f]{16}) pages=16 "
    r"bytes=65536 root=0x([0-9a-f]{16}) pde_slot=0x([0-9a-f]{16}) "
    r"boot_sec=([0-9]+) boot_usec=([0-9]+)$"
)
PAGE = re.compile(r"^PAGE ([0-9]{2}) va=0x([0-9a-f]{16})$")
DATA = re.compile(r"^DATA ([0-9a-f]{4}) ([0-9a-f]{128})$")
RESTORE = "REGION_RESTORE write_rc=0 read_rc=0 value=0x040000001dc001a1"
CLEAR = "REGION_CLEAR write_rc=0 read_rc=0 value=0x000000001dc001a1"
END = "REGION_END pages=16 bytes=65536"
ANCHOR_INDEX = 8
BOOT = re.compile(r"^REGION_BOOT rc=0 len=16 sec=([0-9]+) usec=([0-9]+)$")


def parse(raw: bytes, anchor: bytes) -> tuple[bytes, dict]:
    if len(anchor) != 4096:
        raise ValueError("anchor must be exactly 4096 bytes")
    lines = raw.decode("utf-8", errors="replace").splitlines()
    matches = [(i, BEGIN.fullmatch(line)) for i, line in enumerate(lines)]
    starts = [(i, match) for i, match in matches if match]
    if len(starts) != 1:
        raise ValueError(f"expected one valid REGION_BEGIN, found {len(starts)}")
    start, match = starts[0]
    if CLEAR not in lines[:start] or RESTORE not in lines[:start]:
        raise ValueError("clear and restoration readbacks must precede data")
    if lines.index(CLEAR) > lines.index(RESTORE):
        raise ValueError("restoration must follow permission change")
    base, root, pde_slot = (int(match.group(i), 16) for i in (1, 2, 3))
    boot_sec, boot_usec = (int(match.group(i)) for i in (4, 5))
    if boot_sec <= 0 or boot_usec >= 1_000_000 or base & 4095:
        raise ValueError("invalid boot time or unaligned base")
    boots = [BOOT.fullmatch(line) for line in lines[:start]]
    boots = [item for item in boots if item]
    if (len(boots) != 1 or
        int(boots[0].group(1)) != boot_sec or
        int(boots[0].group(2)) != boot_usec or
        "REGION_PREFLIGHT fw=0x13600007" not in lines[:start]):
        raise ValueError("boot provenance or firmware preflight mismatch")
    cursor = start + 1
    pages = []
    for index in range(16):
        if cursor >= len(lines):
            raise ValueError("truncated before page header")
        page_match = PAGE.fullmatch(lines[cursor])
        if not page_match or int(page_match.group(1)) != index or int(page_match.group(2), 16) != base + index * 4096:
            raise ValueError(f"invalid page header {index}")
        cursor += 1
        chunks = []
        for chunk_index in range(64):
            if cursor >= len(lines):
                raise ValueError("truncated page data")
            item = DATA.fullmatch(lines[cursor])
            if not item or int(item.group(1), 16) != chunk_index * 64:
                raise ValueError(f"invalid page {index} chunk {chunk_index}")
            chunks.append(bytes.fromhex(item.group(2)))
            cursor += 1
        pages.append(b"".join(chunks))
    if cursor >= len(lines) or lines[cursor] != END:
        raise ValueError("missing exact region end marker")
    if pages[ANCHOR_INDEX] != anchor:
        raise ValueError("overlap page differs from the independently saved capture")
    region = b"".join(pages)
    if len(region) != 65536:
        raise ValueError("region length mismatch")
    report = {
        "firmware": "13.60 (0x13600007)",
        "virtual_base": f"0x{base:016x}",
        "length": len(region),
        "sha256": hashlib.sha256(region).hexdigest(),
        "pmap_root": f"0x{root:016x}",
        "pde_slot": f"0x{pde_slot:016x}",
        "console_boot_time": {"seconds": boot_sec, "microseconds": boot_usec},
        "anchor_index": ANCHOR_INDEX,
        "anchor_sha256": hashlib.sha256(anchor).hexdigest(),
        "pages": [
            {"index": i, "virtual_address": f"0x{base + i * 4096:016x}",
             "sha256": hashlib.sha256(page).hexdigest()}
            for i, page in enumerate(pages)
        ],
    }
    return region, report


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("log", type=Path)
    ap.add_argument("anchor", type=Path)
    ap.add_argument("output", type=Path)
    args = ap.parse_args()
    raw = args.log.read_bytes()
    region, report = parse(raw, args.anchor.read_bytes())
    args.output.write_bytes(region)
    report["source_log"] = str(args.log)
    report["source_log_sha256"] = hashlib.sha256(raw).hexdigest()
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
