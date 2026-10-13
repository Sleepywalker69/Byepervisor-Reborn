import unittest

from parse_text_boundary_samples import EXPECTED, parse


def sample():
    text = 0xffffffffdda40000
    pages = [bytes([index + 1]) * 4096 for index in range(2)]
    lines = []
    for index, (_, original) in enumerate(EXPECTED):
        lines.append(f"EDGE_CLEAR index={index} write_rc=0 read_rc=0 value=0x{original & ~(1 << 58):016x}")
        lines.append(f"EDGE_RESTORE index={index} write_rc=0 read_rc=0 value=0x{original:016x}")
    lines.append(f"EDGE_BEGIN fw=0x13600007 text=0x{text:016x} root=0x0000000021894000 "
                 "boot_sec=1791890109 boot_usec=471453 count=2")
    for index, (offset, original) in enumerate(EXPECTED):
        lines.append(f"PAGE index={index} off=0x{offset:06x} pte=0x{original:016x}")
        page = pages[index]
        lines += [f"DATA {off:04x} {page[off:off + 64].hex()}"
                  for off in range(0, 4096, 64)]
    lines.append("EDGE_END count=2 bytes=8192")
    return "\n".join(lines).encode(), pages


class ParseTest(unittest.TestCase):
    def test_complete(self):
        raw, pages = sample()
        result, report = parse(raw)
        self.assertEqual(result, b"".join(pages))
        self.assertEqual(len(report["pages"]), 2)

    def test_truncated(self):
        raw, _ = sample()
        with self.assertRaises(ValueError):
            parse(raw.rsplit(b"\n", 1)[0])


if __name__ == "__main__":
    unittest.main()
