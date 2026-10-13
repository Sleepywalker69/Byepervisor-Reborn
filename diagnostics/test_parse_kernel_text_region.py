import unittest

from parse_kernel_text_region import parse


def sample():
    base = 0xffffffffddce0000
    pages = [bytes([i]) * 4096 for i in range(16)]
    lines = [
        "REGION_PREFLIGHT fw=0x13600007",
        "REGION_BOOT rc=0 len=16 sec=1791890000 usec=1",
        "REGION_CLEAR write_rc=0 read_rc=0 value=0x000000001dc001a1",
        "REGION_RESTORE write_rc=0 read_rc=0 value=0x040000001dc001a1",
        f"REGION_BEGIN fw=0x13600007 base=0x{base:016x} pages=16 bytes=65536 "
        "root=0x0000000021894000 pde_slot=0xffff800000000000 boot_sec=1791890000 boot_usec=1",
    ]
    for index, page in enumerate(pages):
        lines.append(f"PAGE {index:02d} va=0x{base + index * 4096:016x}")
        lines += [f"DATA {offset:04x} {page[offset:offset + 64].hex()}"
                  for offset in range(0, 4096, 64)]
    lines.append("REGION_END pages=16 bytes=65536")
    return "\n".join(lines).encode(), pages


class ParseTest(unittest.TestCase):
    def test_complete(self):
        raw, pages = sample()
        region, report = parse(raw, pages[8])
        self.assertEqual(region, b"".join(pages))
        self.assertEqual(len(report["pages"]), 16)

    def test_truncated(self):
        raw, pages = sample()
        with self.assertRaises(ValueError):
            parse(raw.rsplit(b"\n", 1)[0], pages[8])

    def test_wrong_order(self):
        raw, pages = sample()
        with self.assertRaises(ValueError):
            parse(raw.replace(b"PAGE 09", b"PAGE 08"), pages[8])

    def test_overlap_mismatch(self):
        raw, pages = sample()
        with self.assertRaises(ValueError):
            parse(raw, bytes(4096))

    def test_boot_mismatch(self):
        raw, pages = sample()
        with self.assertRaises(ValueError):
            parse(raw.replace(b"REGION_BOOT rc=0 len=16 sec=1791890000",
                              b"REGION_BOOT rc=0 len=16 sec=1791890001"), pages[8])


if __name__ == "__main__":
    unittest.main()
