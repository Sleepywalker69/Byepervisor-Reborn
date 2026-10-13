#!/usr/bin/env python3
"""Validate one framed 13.60 text-page capture before writing its bytes."""
import argparse
import hashlib
import json
import re
from pathlib import Path

BEGIN = re.compile(r"^CAPTURE_BEGIN fw=0x13600007 va=0x([0-9a-f]{16}) bytes=4096 known8_match=1$")
DATA = re.compile(r"^DATA ([0-9a-f]{4}) ([0-9a-f]{128})$")
KNOWN = bytes.fromhex("554889e5488b8740")


def parse(raw: bytes) -> tuple[bytes, int]:
    lines = raw.decode("utf-8", errors="replace").splitlines()
    starts = [(i, BEGIN.fullmatch(line)) for i, line in enumerate(lines)]
    starts = [(i, match) for i, match in starts if match]
    if len(starts) != 1:
        raise ValueError(f"expected one valid CAPTURE_BEGIN, found {len(starts)}")
    start, match = starts[0]
    va = int(match.group(1), 16)
    if len(lines) <= start + 65 or lines[start + 65] != "CAPTURE_END bytes=4096":
        raise ValueError("capture is truncated or lacks the exact end marker")
    if "CAPTURE_RESTORE write_rc=0 read_rc=0 value=0x040000001dc001a1" not in lines[:start]:
        raise ValueError("PDE restoration is not confirmed before capture output")
    chunks = []
    for number, line in enumerate(lines[start + 1:start + 65]):
        item = DATA.fullmatch(line)
        if not item or int(item.group(1), 16) != number * 64:
            raise ValueError(f"invalid DATA record {number}")
        chunks.append(bytes.fromhex(item.group(2)))
    page = b"".join(chunks)
    if len(page) != 4096 or page[0xd60:0xd68] != KNOWN:
        raise ValueError("length or known instruction bytes do not match")
    return page, va


def main() -> None:
    ap = argparse.ArgumentParser()
    ap.add_argument("log", type=Path)
    ap.add_argument("output", type=Path)
    args = ap.parse_args()
    page, va = parse(args.log.read_bytes())
    args.output.write_bytes(page)
    report = {
        "firmware": "13.60 (0x13600007)",
        "virtual_address": f"0x{va:016x}",
        "length": len(page),
        "sha256": hashlib.sha256(page).hexdigest(),
        "source_log": str(args.log),
        "source_log_sha256": hashlib.sha256(args.log.read_bytes()).hexdigest(),
        "known8_offset": "0xd60",
    }
    print(json.dumps(report, indent=2))


if __name__ == "__main__":
    main()
