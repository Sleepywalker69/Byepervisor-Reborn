"""Capture the 13.60 receipt before and after Payload Manager autoload.

Start this host-side watcher before launching Y2JB. It only sends the pinned,
read-only V3 status ELF through an existing elfldr. It never starts Byepervisor
or changes the manager configuration. A late or contaminated early window is
reported as such rather than treated as a clean before/after comparison.
"""

import argparse
import hashlib
import json
import re
import socket
import time
import urllib.error
import urllib.request
from datetime import datetime, timezone
from pathlib import Path

PINNED_ELF_BYTES = 111_024
PINNED_ELF_SHA256 = "3b17d7e24c44b2341e3a65b133f74f052d95540ba453becd58378aaa2f35e4a5"
STATUS_RE = re.compile(
    r"BPR_STATUS_V3 ok=([01]) fw=([0-9a-f]{8}) boot=([0-9]+\.[0-9]{6}) "
    r"base=([0-9a-f]{16}) root=([0-9a-f]{16}) stage=([0-9a-f]{16}) "
    r"hook=([0-9a-f]{16}) arm=([0-9a-f]{16}) complete=([0-9a-f]{16}) "
    r"gate=([01]) ist=([01])"
)


def utc_now():
    return datetime.now(timezone.utc).isoformat()


def manager_get(host, route):
    req = urllib.request.Request(f"http://{host}:8084{route}", method="GET")
    with urllib.request.urlopen(req, timeout=1) as response:
        if response.status != 200:
            raise ValueError(f"{route}: HTTP {response.status}")
        return response.read(8192)


def autoload_status(host):
    data = json.loads(manager_get(host, "/autoload_status"))
    if not isinstance(data, dict):
        raise ValueError("autoload status is not an object")
    for key in ("remaining_ms", "total", "done"):
        if type(data.get(key)) is not int:
            raise ValueError(f"invalid {key}")
    if data["remaining_ms"] < -1 or data["total"] < 0 or data["done"] < 0:
        raise ValueError("negative autoload counter")
    if data["done"] > data["total"]:
        raise ValueError("done exceeds total")
    return data


def early_window(state):
    """Allow the first read only during a real pending countdown."""
    return state["done"] == 0 and state["remaining_ms"] >= 2500


def early_uncontaminated(before, after):
    """Reject a result if autoload may have started while the ELF ran."""
    return early_window(before) and after["done"] == 0 and after["remaining_ms"] > 0


def parse_receipt(raw):
    decoded = raw.decode("ascii", "replace").strip()
    match = STATUS_RE.fullmatch(decoded)
    if not match:
        raise ValueError(f"unexpected status output: {decoded[:200]!r}")
    fields = ("ok", "fw", "boot", "base", "root", "stage", "hook", "arm",
              "complete", "gate", "ist")
    return dict(zip(fields, match.groups()))


def send_status_elf(host, elf):
    with socket.create_connection((host, 9021), timeout=3) as conn:
        conn.settimeout(12)
        conn.sendall(elf)
        conn.shutdown(socket.SHUT_WR)
        chunks = []
        total = 0
        while total < 4096:
            try:
                part = conn.recv(4096 - total)
            except (ConnectionResetError, TimeoutError):
                break
            if not part:
                break
            chunks.append(part)
            total += len(part)
    return b"".join(chunks)


def save(output_dir, report):
    (output_dir / "watch-report.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8")


def capture(output_dir, name, host, elf, report):
    raw = send_status_elf(host, elf)
    (output_dir / f"{name}-status.txt").write_bytes(raw)
    result = {"utc": utc_now(), "bytes": len(raw),
              "sha256": hashlib.sha256(raw).hexdigest(),
              "fields": parse_receipt(raw)}
    report[name] = result
    save(output_dir, report)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--host", required=True)
    parser.add_argument("--status-elf", type=Path, required=True)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    elf = args.status_elf.read_bytes()
    if len(elf) != PINNED_ELF_BYTES or hashlib.sha256(elf).hexdigest() != PINNED_ELF_SHA256:
        parser.error("status ELF size/hash does not match the pinned read-only build")
    args.output_dir.mkdir(parents=True, exist_ok=False)
    report = {"start_utc": utc_now(), "host": args.host,
              "status_elf_sha256": PINNED_ELF_SHA256, "phase": "preflight"}
    save(args.output_dir, report)
    try:
        try:
            version = manager_get(args.host, "/version")
        except (OSError, urllib.error.URLError):
            version = None
        if version is not None:
            raise RuntimeError("port 8084 was already open before Y2JB; fresh boot required")
        report["phase"] = "watching_for_manager"
        save(args.output_dir, report)
        print("READY: launch PPSA01651, then wake after the planned Rest Mode interval", flush=True)
        deadline = time.monotonic() + 300
        while time.monotonic() < deadline:
            try:
                version = manager_get(args.host, "/version")
                if version == b"0.5.2":
                    break
            except (OSError, urllib.error.URLError):
                pass
            time.sleep(0.1)
        else:
            raise TimeoutError("manager did not become ready within five minutes")
        report["manager_ready_utc"] = utc_now()
        report["phase"] = "looking_for_early_window"
        save(args.output_dir, report)
        deadline = time.monotonic() + 8
        while time.monotonic() < deadline:
            state = autoload_status(args.host)
            if early_window(state):
                report["autoload_before_early"] = state
                save(args.output_dir, report)
                capture(args.output_dir, "early", args.host, elf, report)
                after = autoload_status(args.host)
                report["autoload_after_early"] = after
                if not early_uncontaminated(state, after):
                    raise RuntimeError("early status overlapped autoload; comparison invalid")
                if report["early"]["fields"]["ok"] != "1":
                    raise RuntimeError("early receipt failed; no late status ELF sent")
                break
            if state["done"] > 0 or state.get("current") == "DONE":
                raise RuntimeError("missed early window; no status ELF sent")
            time.sleep(0.1)
        else:
            raise TimeoutError("no clean early window within eight seconds")
        report["phase"] = "waiting_for_autoload"
        save(args.output_dir, report)
        deadline = time.monotonic() + 90
        while time.monotonic() < deadline:
            state = autoload_status(args.host)
            if state["total"] > 0 and state["done"] == state["total"] and state.get("current") == "DONE":
                report["autoload_after_all"] = state
                break
            time.sleep(0.25)
        else:
            raise TimeoutError("manager did not report autoload complete")
        time.sleep(2)
        capture(args.output_dir, "late", args.host, elf, report)
        for field in ("fw", "base", "root", "arm", "complete"):
            if report["early"]["fields"][field] != report["late"]["fields"][field]:
                raise RuntimeError(f"early and late receipts disagree on {field}")
        report["phase"] = "complete"
    except Exception as exc:
        report["phase"] = "stopped"
        report["error"] = f"{type(exc).__name__}: {exc}"
        raise
    finally:
        report["end_utc"] = utc_now()
        save(args.output_dir, report)
        print(json.dumps(report, indent=2), flush=True)


if __name__ == "__main__":
    main()
