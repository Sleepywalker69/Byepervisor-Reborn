"""Check durable arm recording when elfldr's socket resets across sleep."""

import json
import socket
import struct
import subprocess
import sys
import tempfile
import threading
import time
from pathlib import Path


def main():
    script = Path(__file__).resolve().parents[1] / "diagnostics/run_attempt_receipt_1360.py"
    with tempfile.TemporaryDirectory() as dirname, socket.socket() as server:
        root = Path(dirname)
        elf = root / "test.elf"
        elf.write_bytes(b"\x7fELFtest")
        log = root / "attempt.log"
        receipt = root / "attempt.json"
        server.bind(("127.0.0.1", 0))
        server.listen(1)
        port = server.getsockname()[1]

        def serve():
            conn, _ = server.accept()
            with conn:
                while conn.recv(4096):
                    pass
                conn.sendall(
                    b"BPR_ARM_V2 id=6ace30b47751d056 "
                    b"base=ffffffffcdc90000 root=00000000179c4000\n"
                )
                time.sleep(0.1)
                conn.setsockopt(socket.SOL_SOCKET, socket.SO_LINGER,
                                struct.pack("ii", 1, 0))

        thread = threading.Thread(target=serve, daemon=True)
        thread.start()
        proc = subprocess.run(
            [sys.executable, str(script), "--host", "127.0.0.1",
             "--port", str(port), "--elf", str(elf), "--log", str(log),
             "--receipt", str(receipt)],
            capture_output=True, text=True, timeout=10,
        )
        thread.join(timeout=2)
        assert proc.returncode == 0, proc.stderr
        saved = json.loads(receipt.read_text())
        assert saved["arm_id"] == "6ace30b47751d056"
        assert saved["kernel_text_base"] == "ffffffffcdc90000"
        assert saved["kernel_pmap_root"] == "00000000179c4000"
        assert saved["end_reason"].startswith("socket_error:"), saved
        assert b"BPR_ARM_V2" in log.read_bytes()
    print("attempt runner socket-reset receipt: passed")


if __name__ == "__main__":
    main()
