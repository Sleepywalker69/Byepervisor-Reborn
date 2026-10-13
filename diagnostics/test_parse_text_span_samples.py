import unittest

from parse_text_span_samples import EXPECTED, parse


def sample():
    text = 0xffffffffdda40000
    pages = [bytes([i + 1]) * 4096 for i in range(4)]
    lines = []
    for index, (_, original) in enumerate(EXPECTED):
        lines.append(f"SAMPLE_CLEAR index={index} write_rc=0 read_rc=0 value=0x{original & ~(1 << 58):016x}")
        lines.append(f"SAMPLE_RESTORE index={index} write_rc=0 read_rc=0 value=0x{original:016x}")
    lines.append(f"SAMPLE_BEGIN fw=0x13600007 text=0x{text:016x} root=0x0000000021894000 "
                 "boot_sec=1791890109 boot_usec=471453 count=4")
    for index, (offset, original) in enumerate(EXPECTED):
        lines.append(f"PAGE index={index} off=0x{offset:06x} pde=0x{original:016x}")
        page = pages[index]
        lines += [f"DATA {off:04x} {page[off:off + 64].hex()}"
                  for off in range(0, 4096, 64)]
    lines.append("SAMPLE_END count=4 bytes=16384")
    return "\n".join(lines).encode(), pages


class ParseTest(unittest.TestCase):
    def test_complete(self):
        raw, pages = sample()
        result, report = parse(raw)
        self.assertEqual(result, b"".join(pages))
        self.assertEqual(len(report["pages"]), 4)

    def test_truncated(self):
        raw, _ = sample()
        with self.assertRaises(ValueError):
            parse(raw.rsplit(b"\n", 1)[0])

    def test_missing_restore(self):
        raw, _ = sample()
        with self.assertRaises(ValueError):
            parse(raw.replace(b"SAMPLE_RESTORE index=2", b"RESTORE_MISSING index=2"))


if __name__ == "__main__":
    unittest.main()
