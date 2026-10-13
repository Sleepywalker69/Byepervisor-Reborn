import unittest

from parse_kernel_text_page import parse


def sample():
    page = bytearray(4096)
    page[0xd60:0xd68] = bytes.fromhex("554889e5488b8740")
    lines = [
        "CAPTURE_RESTORE write_rc=0 read_rc=0 value=0x040000001dc001a1",
        "CAPTURE_BEGIN fw=0x13600007 va=0xffffffffddcea000 bytes=4096 known8_match=1",
    ]
    lines += [f"DATA {i:04x} {page[i:i + 64].hex()}" for i in range(0, 4096, 64)]
    lines += ["CAPTURE_END bytes=4096"]
    return "\n".join(lines).encode(), bytes(page)


class ParseTest(unittest.TestCase):
    def test_complete(self):
        log, expected = sample()
        page, va = parse(log)
        self.assertEqual(page, expected)
        self.assertEqual(va, 0xffffffffddcea000)

    def test_truncated(self):
        log, _ = sample()
        with self.assertRaises(ValueError):
            parse(log.rsplit(b"\n", 1)[0])

    def test_missing_restore(self):
        log, _ = sample()
        with self.assertRaises(ValueError):
            parse(log.replace(b"CAPTURE_RESTORE", b"RESTORE_MISSING"))

    def test_wrong_known_bytes(self):
        log, _ = sample()
        with self.assertRaises(ValueError):
            parse(log.replace(b"DATA 0d40 " + b"00" * 32 + b"554889e5488b8740",
                              b"DATA 0d40 " + b"00" * 40))


if __name__ == "__main__":
    unittest.main()
