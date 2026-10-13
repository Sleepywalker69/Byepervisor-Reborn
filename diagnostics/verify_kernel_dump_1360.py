#!/usr/bin/env python3
"""Independently verify local dump files, chunk receipts, and map summaries."""

import argparse
import hashlib
import json
import re
from pathlib import Path


def digest(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(1024 * 1024):
            h.update(block)
    return h.hexdigest()


def check_segment(root: Path, state: dict, name: str) -> dict:
    path = root / f"kernel-{name}.bin"
    expected_len = state["info"][f"{name}_length"]
    if path.stat().st_size != expected_len:
        raise ValueError(f"{name} length mismatch")
    offset = 0
    with path.open("rb") as stream:
        for index, record in enumerate(state["chunks"][name]):
            if record["offset"] != offset or not 0 < record["length"] <= 65536:
                raise ValueError(f"{name} chunk {index} offset/length invalid")
            block = stream.read(record["length"])
            if len(block) != record["length"]:
                raise ValueError(f"{name} chunk {index} truncated")
            if hashlib.sha256(block).hexdigest() != record["sha256"]:
                raise ValueError(f"{name} chunk {index} hash mismatch")
            offset += len(block)
        if stream.read(1) or offset != expected_len:
            raise ValueError(f"{name} chunk coverage incomplete")
    full_hash = digest(path)
    if full_hash != state[f"{name}_sha256"]:
        raise ValueError(f"{name} full hash mismatch")
    return {"path": str(path), "length": expected_len, "sha256": full_hash,
            "chunks": len(state["chunks"][name])}


def parse_map(path: Path) -> tuple[dict, dict]:
    lines = path.read_text(encoding="utf-8").splitlines()
    if not lines or not lines[0].startswith("MAP_BEGIN ") or not lines[-1].startswith("MAP_END "):
        raise ValueError(f"incomplete map inventory: {path}")
    begin = dict(re.findall(r"(\w+)=([^ ]+)", lines[0]))
    rows = {}
    for line in lines[1:-1]:
        fields = dict(re.findall(r"(\w+)=([^ ]+)", line))
        if "va" not in fields:
            raise ValueError(f"unframed map row: {line}")
        va = int(fields["va"], 16)
        if va in rows:
            raise ValueError("duplicate map window")
        rows[va] = (line.split(" ", 1)[0], fields)
    end = dict(re.findall(r"(\w+)=([^ ]+)", lines[-1]))
    if int(end["windows"]) + int(end["gaps"]) != len(rows):
        raise ValueError("map window count mismatch")
    return begin, rows


def normalized_entry(value: str) -> int:
    # Accessed and dirty are hardware-dynamic; preserve all other bits.
    return int(value, 16) & ~0x60


def check_maps(pre: Path, post: Path, info: dict) -> dict:
    before, b_rows = parse_map(pre)
    after, a_rows = parse_map(post)
    for key in ("boot", "text", "data", "root", "stage", "start", "end"):
        if before[key] != after[key]:
            raise ValueError(f"map {key} changed")
    if before["boot"] != f"{info['boot_sec']}.{info['boot_usec']:06d}":
        raise ValueError("map boot does not match capture")
    if before["text"] != info["text_base"] or before["data"] != info["data_base"]:
        raise ValueError("map bases do not match capture")
    if before["root"] != info["pmap_root"] or before["stage"] != info["stage"]:
        raise ValueError("map root or stage does not match capture")
    if b_rows.keys() != a_rows.keys():
        raise ValueError("map window set changed")
    text_start = int(info["text_base"], 16)
    text_end = int(info["data_base"], 16)
    compared = 0
    for va in sorted(b_rows):
        if va + 0x200000 <= text_start or va >= text_end:
            continue
        b_type, b = b_rows[va]
        a_type, a = a_rows[va]
        if b_type != a_type:
            raise ValueError(f"text mapping type changed at 0x{va:x}")
        if b_type == "MAP_2M":
            if normalized_entry(b["entry"]) != normalized_entry(a["entry"]):
                raise ValueError(f"text PDE changed at 0x{va:x}")
            if not int(a["entry"], 16) & (1 << 58):
                raise ValueError(f"text XOTEXT absent at 0x{va:x}")
        elif b_type == "MAP_4K":
            for key in ("present", "xo", "rw", "first_missing"):
                if b[key] != a[key]:
                    raise ValueError(f"text PTE summary {key} changed at 0x{va:x}")
            for key in ("pde", "first", "last"):
                if normalized_entry(b[key]) != normalized_entry(a[key]):
                    raise ValueError(f"text PTE summary {key} changed at 0x{va:x}")
        else:
            raise ValueError(f"text map gap at 0x{va:x}")
        compared += 1
    return {"text_windows_compared": compared,
            "pre_map_sha256": digest(pre), "post_map_sha256": digest(post),
            "limit": "4 KiB windows compare counts and boundary entries, not every PTE"}


def main() -> None:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--capture-dir", type=Path, required=True)
    p.add_argument("--pre-map", type=Path, required=True)
    p.add_argument("--post-map", type=Path, required=True)
    p.add_argument("--known-region", type=Path, required=True)
    p.add_argument("--known-offset", type=lambda x: int(x, 0), default=0x2A0000)
    args = p.parse_args()
    state = json.loads((args.capture_dir / "capture-state.json").read_text(encoding="utf-8"))
    if state.get("status") != "complete":
        raise ValueError("capture state is incomplete")
    text = check_segment(args.capture_dir, state, "text")
    data = check_segment(args.capture_dir, state, "data")
    known = args.known_region.read_bytes()
    if args.known_offset < 0 or args.known_offset + len(known) > text["length"]:
        raise ValueError("known overlap outside text")
    with (args.capture_dir / "kernel-text.bin").open("rb") as stream:
        stream.seek(args.known_offset)
        if stream.read(len(known)) != known:
            raise ValueError("known 64 KiB overlap differs")
    maps = check_maps(args.pre_map, args.post_map, state["info"])
    result = {"status": "verified", "boot": state["info"],
              "text": text, "data": data,
              "known_overlap_sha256": hashlib.sha256(known).hexdigest(),
              "maps": maps,
              "limits": ["Data is a non-atomic live mapped range ending at the first gap.",
                         "A raw memory capture is not a reconstructed kernel ELF."]}
    path = args.capture_dir / "verification.json"
    path.write_text(json.dumps(result, indent=2, sort_keys=True) + "\n", encoding="utf-8")
    print(json.dumps(result, indent=2, sort_keys=True))


if __name__ == "__main__":
    main()
