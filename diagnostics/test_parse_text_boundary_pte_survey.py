import unittest

from parse_text_boundary_pte_survey import parse


def sample():
    text = 0xffffffffdda40000
    offsets = [i * 4096 for i in range(448)] + [0xbc0000 + i * 4096 for i in range(256)]
    lines = [f"BOUNDARY_BEGIN fw=0x13600007 text=0x{text:016x} "
             "root=0x0000000021894000 boot_sec=1791890109 boot_usec=471453 count=704"]
    for offset in offsets:
        entry = 0x040000001da40101 + offset
        lines.append(f"PTE off=0x{offset:06x} value=0x{entry:016x}")
    lines.append("BOUNDARY_END count=704")
    return "\n".join(lines).encode()


class ParseTest(unittest.TestCase):
    def test_complete(self):
        report = parse(sample())
        self.assertEqual(report["entry_count"], 704)
        self.assertEqual(report["counts"]["present"], 704)

    def test_truncated(self):
        with self.assertRaises(ValueError):
            parse(sample().rsplit(b"\n", 1)[0])

    def test_wrong_offset(self):
        with self.assertRaises(ValueError):
            parse(sample().replace(b"PTE off=0xbc0000", b"PTE off=0xbd0000"))


if __name__ == "__main__":
    unittest.main()
