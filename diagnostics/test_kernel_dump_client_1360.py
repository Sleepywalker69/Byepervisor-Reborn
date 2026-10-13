import hashlib
import importlib.util
import socket
import tempfile
import threading
import unittest
import zlib
from pathlib import Path


MODULE_PATH = Path(__file__).with_name("kernel_dump_client_1360.py")
SPEC = importlib.util.spec_from_file_location("kernel_dump_client_1360", MODULE_PATH)
client = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(client)


def info_frame():
    return client.INFO.pack(
        b"K136DMP1", 1, 0x13600007, 123456, 42,
        0xFFFFFFFF80000000, 0xFFFFFFFF80010000,
        0x10000, 0x10000, 0x12345000, 0x41505F434F4D504C,
    )


class ProtocolTests(unittest.TestCase):
    def test_struct_sizes(self):
        self.assertEqual((client.INFO.size, client.REQUEST.size,
                          client.RESPONSE.size), (80, 20, 32))

    def test_info_rejects_bad_stage(self):
        left, right = socket.socketpair()
        try:
            frame = bytearray(info_frame())
            frame[-8:] = b"\0" * 8
            right.sendall(frame)
            with self.assertRaisesRegex(client.CaptureError, "AP-complete"):
                client.read_info(left)
        finally:
            left.close()
            right.close()

    def test_chunk_accepts_exact_crc(self):
        left, right = socket.socketpair()
        data = bytes(range(256)) * 16

        def server():
            request = client.receive_exact(right, client.REQUEST.size)
            self.assertEqual(client.REQUEST.unpack(request),
                             (b"REQ1", 1, 0, 4096))
            right.sendall(client.RESPONSE.pack(
                b"RES1", 0, 0, len(data), zlib.crc32(data), 0))
            right.sendall(data)
            right.close()

        worker = threading.Thread(target=server)
        worker.start()
        try:
            self.assertEqual(client.request_chunk(left, 1, 0, 4096), data)
        finally:
            left.close()
            worker.join()

    def test_chunk_rejects_crc(self):
        left, right = socket.socketpair()

        def server():
            client.receive_exact(right, client.REQUEST.size)
            right.sendall(client.RESPONSE.pack(b"RES1", 0, 0, 4096, 1, 0))
            right.sendall(b"\0" * 4096)
            right.close()

        worker = threading.Thread(target=server)
        worker.start()
        try:
            with self.assertRaisesRegex(client.CaptureError, "CRC"):
                client.request_chunk(left, 1, 0, 4096)
        finally:
            left.close()
            worker.join()

    def test_chunk_rejects_console_error_without_body(self):
        left, right = socket.socketpair()

        def server():
            client.receive_exact(right, client.REQUEST.size)
            right.sendall(client.RESPONSE.pack(b"RES1", 4, 0, 0, 0, 0xBEEF))
            right.close()

        worker = threading.Thread(target=server)
        worker.start()
        try:
            with self.assertRaisesRegex(client.CaptureError, "do not skip"):
                client.request_chunk(left, 1, 0, 4096)
        finally:
            left.close()
            worker.join()

    def test_saved_chunk_hash_is_rechecked(self):
        with tempfile.TemporaryDirectory() as temp:
            root = Path(temp)
            info = {"boot_sec": 123}
            data = b"a" * 4096
            (root / "kernel-text.part").write_bytes(data)
            state = {
                "info": info,
                "chunks": {
                    "text": [{"offset": 0, "length": 4096,
                              "sha256": hashlib.sha256(data).hexdigest()}],
                    "data": [],
                },
                "status": "incomplete",
            }
            client.atomic_json(root / "capture-state.json", state)
            self.assertEqual(client.validate_saved(root, info), state)
            (root / "kernel-text.part").write_bytes(b"b" * 4096)
            with self.assertRaisesRegex(client.CaptureError, "verification failed"):
                client.validate_saved(root, info)


if __name__ == "__main__":
    unittest.main()
