"""One-run elfldr sender that durably timestamps the arm line and output."""

import argparse
import hashlib
import json
import re
import socket
from datetime import datetime, timezone
from pathlib import Path

ARM = re.compile(
    rb"^BPR_ARM_V2 id=([0-9a-f]{16}) base=([0-9a-f]{16}) root=([0-9a-f]{16})$"
)


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--host", required=True)
    ap.add_argument("--port", type=int, default=9021)
    ap.add_argument("--elf", type=Path, required=True)
    ap.add_argument("--log", type=Path, required=True)
    ap.add_argument("--receipt", type=Path, required=True)
    args = ap.parse_args()
    payload = args.elf.read_bytes()
    if not payload.startswith(b"\x7fELF"):
        raise SystemExit("payload is not an ELF")
    if args.log.exists() or args.receipt.exists():
        raise SystemExit("output exists; choose fresh paths")

    record = {
        "payload_sha256": hashlib.sha256(payload).hexdigest(),
        "payload_bytes": len(payload),
        "launch_utc": utc_now(),
        "arm_id": None,
        "kernel_text_base": None,
        "kernel_pmap_root": None,
        "arm_utc": None,
        "end_utc": None,
        "end_reason": None,
    }
    pending = b""
    with socket.create_connection((args.host, args.port), timeout=10) as conn:
        conn.settimeout(180)
        conn.sendall(payload)
        conn.shutdown(socket.SHUT_WR)
        with args.log.open("xb") as log:
            while True:
                try:
                    chunk = conn.recv(4096)
                except socket.timeout:
                    record["end_reason"] = "idle_timeout"
                    break
                except OSError as exc:
                    record["end_reason"] = f"socket_error:{exc.errno}"
                    break
                if not chunk:
                    record["end_reason"] = "socket_closed"
                    break
                log.write(chunk)
                log.flush()
                pending += chunk
                while b"\n" in pending:
                    line, pending = pending.split(b"\n", 1)
                    line = line.rstrip(b"\r")
                    print(f"[{utc_now()}] {line.decode(errors='replace')}", flush=True)
                    match = ARM.fullmatch(line)
                    if match:
                        if record["arm_id"] is not None:
                            record["end_reason"] = "duplicate_arm"
                            break
                        record["arm_id"] = match.group(1).decode("ascii")
                        record["kernel_text_base"] = match.group(2).decode("ascii")
                        record["kernel_pmap_root"] = match.group(3).decode("ascii")
                        record["arm_utc"] = utc_now()
                        args.receipt.write_text(json.dumps(record, indent=2) + "\n")
                        print("ARMED: payload requests Rest Mode automatically", flush=True)
                if record["end_reason"] == "duplicate_arm":
                    break
            if pending:
                print(f"[{utc_now()}] partial: {pending.decode(errors='replace')}", flush=True)
    record["end_utc"] = utc_now()
    args.receipt.write_text(json.dumps(record, indent=2) + "\n")
    print(f"END {record['end_reason']}; arm={record['arm_id']}", flush=True)


if __name__ == "__main__":
    main()
