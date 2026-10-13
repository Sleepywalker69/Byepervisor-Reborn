#!/usr/bin/env python3
"""Receive bounded 13.60 kernel mapping captures from kernel_dump_server_1360."""

import argparse
import hashlib
import json
import os
import socket
import struct
import time
import zlib
from pathlib import Path

INFO = struct.Struct("<8sIIQQQQQQQQ")
REQUEST = struct.Struct("<4sIQI")
RESPONSE = struct.Struct("<4sIQIIQ")
CHUNK = 65536
SEGMENTS = (("text", 1), ("data", 2))


class CaptureError(RuntimeError):
    pass


def receive_exact(sock: socket.socket, length: int) -> bytes:
    out = bytearray()
    while len(out) < length:
        part = sock.recv(length - len(out))
        if not part:
            raise CaptureError(f"connection closed after {len(out)}/{length} bytes")
        out.extend(part)
    return bytes(out)


def read_info(sock: socket.socket) -> dict:
    values = INFO.unpack(receive_exact(sock, INFO.size))
    (magic, version, firmware, boot_sec, boot_usec, text_base, data_base,
     text_length, data_length, root, stage) = values
    if magic != b"K136DMP1" or version != 1 or firmware != 0x13600007:
        raise CaptureError("unexpected server protocol or firmware")
    if not (0 < text_length <= 0x8000000 and 0 < data_length <= 0x8000000):
        raise CaptureError("invalid segment length")
    if text_base + text_length != data_base:
        raise CaptureError("text/data boundary mismatch")
    if (text_length | data_length | text_base | data_base) & 0xFFF:
        raise CaptureError("unaligned segment")
    if stage != 0x41505F434F4D504C:
        raise CaptureError("AP-complete stage missing")
    return {
        "firmware": "13.60 (0x13600007)",
        "boot_sec": boot_sec,
        "boot_usec": boot_usec,
        "text_base": f"0x{text_base:016x}",
        "data_base": f"0x{data_base:016x}",
        "text_length": text_length,
        "data_length": data_length,
        "pmap_root": f"0x{root:016x}",
        "stage": f"0x{stage:016x}",
        "protocol": 1,
    }


def request_chunk(sock: socket.socket, segment: int, offset: int,
                  length: int) -> bytes:
    if segment not in (1, 2) or offset < 0 or offset & 0xFFF:
        raise CaptureError("invalid local request")
    if not 0 < length <= CHUNK or length & 0xFFF:
        raise CaptureError("invalid local chunk length")
    sock.sendall(REQUEST.pack(b"REQ1", segment, offset, length))
    magic, status, got_offset, got_length, crc, detail = RESPONSE.unpack(
        receive_exact(sock, RESPONSE.size))
    if magic != b"RES1" or got_offset != offset:
        raise CaptureError("response framing mismatch")
    if status:
        raise CaptureError(
            f"console rejected segment={segment} offset=0x{offset:x} "
            f"status={status} detail=0x{detail:x}; stop, do not skip"
        )
    if got_length != length:
        raise CaptureError("response length mismatch")
    data = receive_exact(sock, length)
    if zlib.crc32(data) != crc:
        raise CaptureError("chunk CRC mismatch")
    return data


def atomic_json(path: Path, value: dict) -> None:
    pending = path.with_suffix(path.suffix + ".tmp")
    pending.write_text(json.dumps(value, indent=2, sort_keys=True) + "\n",
                       encoding="utf-8")
    os.replace(pending, path)


def sha256_file(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        while block := stream.read(1024 * 1024):
            digest.update(block)
    return digest.hexdigest()


def validate_saved(out: Path, info: dict) -> dict:
    state_path = out / "capture-state.json"
    if not state_path.exists():
        if any((out / f"kernel-{name}.part").exists() for name, _ in SEGMENTS):
            raise CaptureError("partial file exists without state manifest")
        return {"info": info, "chunks": {name: [] for name, _ in SEGMENTS},
                "status": "incomplete"}
    state = json.loads(state_path.read_text(encoding="utf-8"))
    if state.get("info") != info or state.get("status") != "incomplete":
        raise CaptureError("saved capture belongs to another boot or is complete")
    for name, _ in SEGMENTS:
        records = state["chunks"][name]
        path = out / f"kernel-{name}.part"
        if records and not path.exists():
            raise CaptureError(f"missing {path.name}")
        expected_offset = 0
        with path.open("rb") if path.exists() else open(os.devnull, "rb") as stream:
            for record in records:
                if record["offset"] != expected_offset or not 0 < record["length"] <= CHUNK:
                    raise CaptureError(f"noncontiguous saved {name} chunks")
                data = stream.read(record["length"])
                if len(data) != record["length"] or hashlib.sha256(data).hexdigest() != record["sha256"]:
                    raise CaptureError(f"saved {name} chunk verification failed")
                expected_offset += record["length"]
            if stream.read(1):
                raise CaptureError(f"unverified trailing data in {path.name}")
    return state


def capture_all(sock: socket.socket, out: Path, info: dict) -> dict:
    out.mkdir(parents=True, exist_ok=True)
    state = validate_saved(out, info)
    state_path = out / "capture-state.json"
    atomic_json(state_path, state)
    for name, segment in SEGMENTS:
        length = info[f"{name}_length"]
        records = state["chunks"][name]
        offset = sum(item["length"] for item in records)
        path = out / f"kernel-{name}.part"
        with path.open("ab") as stream:
            while offset < length:
                size = min(CHUNK, length - offset)
                data = request_chunk(sock, segment, offset, size)
                stream.write(data)
                stream.flush()
                record = {"offset": offset, "length": size,
                          "sha256": hashlib.sha256(data).hexdigest()}
                records.append(record)
                atomic_json(state_path, state)
                offset += size
                if offset % (1024 * 1024) == 0 or offset == length:
                    print(f"{name}: {offset}/{length} bytes", flush=True)
        if path.stat().st_size != length:
            raise CaptureError(f"{name} size mismatch")
    for name, _ in SEGMENTS:
        source = out / f"kernel-{name}.part"
        state[f"{name}_sha256"] = sha256_file(source)
        state[f"{name}_bytes"] = source.stat().st_size
    state["status"] = "complete"
    state["host_completed_unix"] = int(time.time())
    atomic_json(state_path, state)
    for name, _ in SEGMENTS:
        os.replace(out / f"kernel-{name}.part", out / f"kernel-{name}.bin")
    return state


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--port", type=int, default=9003)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--expected-boot", type=int)
    parser.add_argument("--pilot-text-offset", type=lambda x: int(x, 0))
    args = parser.parse_args()
    with socket.create_connection((args.host, args.port), timeout=20) as sock:
        sock.settimeout(120)
        info = read_info(sock)
        print(json.dumps(info, sort_keys=True), flush=True)
        if args.expected_boot is not None and info["boot_sec"] != args.expected_boot:
            raise CaptureError("console boot differs from expected")
        if args.pilot_text_offset is not None:
            offset = args.pilot_text_offset
            if offset < 0 or offset + CHUNK > info["text_length"]:
                raise CaptureError("pilot offset outside text")
            data = request_chunk(sock, 1, offset, CHUNK)
            args.output.mkdir(parents=True, exist_ok=True)
            path = args.output / f"pilot-text-{offset:08x}.bin"
            if path.exists():
                raise CaptureError(f"refusing to overwrite {path}")
            path.write_bytes(data)
            receipt = {"info": info, "offset": offset, "length": len(data),
                       "sha256": hashlib.sha256(data).hexdigest()}
            atomic_json(args.output / f"pilot-text-{offset:08x}.json", receipt)
            print(f"pilot: {path} sha256={receipt['sha256']}", flush=True)
        else:
            state = capture_all(sock, args.output, info)
            print(f"complete: text={state['text_sha256']} data={state['data_sha256']}",
                  flush=True)
        sock.sendall(REQUEST.pack(b"QUIT", 0, 0, 0))
    return 0


if __name__ == "__main__":
    try:
        raise SystemExit(main())
    except (OSError, ValueError, KeyError, CaptureError) as exc:
        raise SystemExit(f"capture failed: {exc}") from exc
