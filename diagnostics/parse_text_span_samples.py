#!/usr/bin/env python3
"""Parse four guarded kernel-text sample pages and their restore receipts."""
import argparse
import hashlib
import json
import re
from pathlib import Path

EXPECTED = [
    (0x400000, 0x040000001de001a1),
    (0x600000, 0x040000001e0001a1),
    (0x800000, 0x040000001e2001a1),
    (0xa00000, 0x040000001e4001a1),
]
BEGIN = re.compile(r"^SAMPLE_BEGIN fw=0x13600007 text=0x([0-9a-f]{16}) "
                   r"root=0x([0-9a-f]{16}) boot_sec=([0-9]+) boot_usec=([0-9]+) count=4$")
PAGE = re.compile(r"^PAGE index=([0-3]) off=0x([0-9a-f]{6}) pde=0x([0-9a-f]{16})$")
DATA = re.compile(r"^DATA ([0-9a-f]{4}) ([0-9a-f]{128})$")


def parse(raw: bytes) -> tuple[bytes, dict]:
    lines = raw.decode("utf-8", errors="replace").splitlines()
    starts = [(i, BEGIN.fullmatch(line)) for i, line in enumerate(lines)]
    starts = [(i, item) for i, item in starts if item]
    if len(starts) != 1:
        raise ValueError("expected one complete SAMPLE_BEGIN")
    start, begin = starts[0]
    text_base = int(begin.group(1), 16)
    root = int(begin.group(2), 16)
    sec, usec = int(begin.group(3)), int(begin.group(4))
    if not root or sec <= 0 or usec >= 1_000_000:
        raise ValueError("invalid boot provenance")
    for index, (_, original) in enumerate(EXPECTED):
        modified = original & ~(1 << 58)
        clear = f"SAMPLE_CLEAR index={index} write_rc=0 read_rc=0 value=0x{modified:016x}"
        restore = f"SAMPLE_RESTORE index={index} write_rc=0 read_rc=0 value=0x{original:016x}"
        if clear not in lines[:start] or restore not in lines[:start]:
            raise ValueError(f"missing clear or restore readback for {index}")
        if lines.index(clear) > lines.index(restore):
            raise ValueError(f"restore order wrong for {index}")
    cursor = start + 1
    pages = []
    for index, (offset, original) in enumerate(EXPECTED):
        if cursor >= len(lines):
            raise ValueError("truncated sample header")
        item = PAGE.fullmatch(lines[cursor])
        if (not item or int(item.group(1)) != index or
            int(item.group(2), 16) != offset or
            int(item.group(3), 16) != original):
            raise ValueError(f"wrong sample header {index}")
        cursor += 1
        chunks = []
        for chunk_index in range(64):
            if cursor >= len(lines):
                raise ValueError("truncated sample data")
            chunk = DATA.fullmatch(lines[cursor])
            if not chunk or int(chunk.group(1), 16) != chunk_index * 64:
                raise ValueError(f"wrong sample {index} chunk {chunk_index}")
            chunks.append(bytes.fromhex(chunk.group(2)))
            cursor += 1
        pages.append(b"".join(chunks))
    if cursor >= len(lines) or lines[cursor] != "SAMPLE_END count=4 bytes=16384":
        raise ValueError("missing exact end marker")
    data = b"".join(pages)
    report = {
        "firmware": "13.60 (0x13600007)",
        "text_base": f"0x{text_base:016x}",
        "pmap_root": f"0x{root:016x}",
        "console_boot_time": {"seconds": sec, "microseconds": usec},
        "length": len(data),
        "sha256": hashlib.sha256(data).hexdigest(),
        "pages": [
            {"index": index, "offset": f"0x{offset:x}",
             "virtual_address": f"0x{text_base + offset:016x}",
             "original_pde": f"0x{original:016x}",
             "sha256": hashlib.sha256(pages[index]).hexdigest()}
            for index, (offset, original) in enumerate(EXPECTED)
        ],
    }
    return data, report


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("log", type=Path)
    ap.add_argument("output", type=Path)
    args = ap.parse_args()
    raw = args.log.read_bytes()
    data, report = parse(raw)
    args.output.write_bytes(data)
    report["source_log"] = str(args.log)
    report["source_log_sha256"] = hashlib.sha256(raw).hexdigest()
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
