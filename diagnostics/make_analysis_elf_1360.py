"""Wrap verified raw 13.60 VM ranges for local IDA analysis.

This is a synthetic ELF container, not a recovered PS5 kernel ELF. It has no
original entry point, symbols, relocations, or section provenance.
"""

import argparse
import hashlib
import json
from pathlib import Path
import struct

PAGE = 0x1000
EHDR = struct.Struct("<16sHHIQQQIHHHHHH")
PHDR = struct.Struct("<IIQQQQQQ")
SHDR = struct.Struct("<IIQQQQIIQQ")


def align(value: int, multiple: int) -> int:
    return (value + multiple - 1) & ~(multiple - 1)


def checked_copy(src: Path, dst, expected_size: int, expected_hash: str) -> None:
    if src.stat().st_size != expected_size:
        raise ValueError(f"{src.name}: length differs from verified receipt")
    digest = hashlib.sha256()
    with src.open("rb") as stream:
        while chunk := stream.read(1 << 20):
            digest.update(chunk)
            dst.write(chunk)
    if digest.hexdigest() != expected_hash:
        raise ValueError(f"{src.name}: SHA-256 differs from verified receipt")


def build(folder: Path, output: Path) -> None:
    receipt = json.loads((folder / "verification.json").read_text())
    if receipt.get("status") != "verified":
        raise ValueError("capture receipt is not verified")
    boot = receipt["boot"]
    if boot["firmware"] != "13.60 (0x13600007)":
        raise ValueError("unexpected firmware")
    text_base = int(boot["text_base"], 0)
    data_base = int(boot["data_base"], 0)
    text_size = receipt["text"]["length"]
    data_size = receipt["data"]["length"]
    if (text_base % PAGE or data_base % PAGE or text_size % PAGE or
            data_size % PAGE or text_base + text_size != data_base or
            text_size != boot["text_length"] or data_size != boot["data_length"]):
        raise ValueError("capture ranges do not form the expected page-aligned image")
    if output.exists():
        raise FileExistsError(f"refusing to overwrite {output}")

    text_off = PAGE
    data_off = text_off + text_size
    names = b"\0.text\0.data\0.shstrtab\0"
    names_off = data_off + data_size
    shoff = align(names_off + len(names), 8)
    ident = b"\x7fELF" + bytes([2, 1, 1, 0]) + bytes(8)
    header = EHDR.pack(ident, 2, 62, 1, 0, EHDR.size, shoff, 0,
                       EHDR.size, PHDR.size, 2, SHDR.size, 4, 3)
    programs = [
        PHDR.pack(1, 5, text_off, text_base, 0, text_size, text_size, PAGE),
        PHDR.pack(1, 6, data_off, data_base, 0, data_size, data_size, PAGE),
    ]
    sections = [
        SHDR.pack(0, 0, 0, 0, 0, 0, 0, 0, 0, 0),
        SHDR.pack(1, 1, 6, text_base, text_off, text_size, 0, 0, 16, 0),
        SHDR.pack(7, 1, 3, data_base, data_off, data_size, 0, 0, 8, 0),
        SHDR.pack(13, 3, 0, 0, names_off, len(names), 0, 0, 1, 0),
    ]

    try:
        with output.open("xb") as out:
            out.write(header)
            for program in programs:
                out.write(program)
            out.write(bytes(text_off - out.tell()))
            checked_copy(folder / "kernel-text.bin", out, text_size,
                         receipt["text"]["sha256"])
            checked_copy(folder / "kernel-data.bin", out, data_size,
                         receipt["data"]["sha256"])
            out.write(names)
            out.write(bytes(shoff - out.tell()))
            for section in sections:
                out.write(section)
    except BaseException:
        output.unlink(missing_ok=True)
        raise
    print(f"synthetic analysis ELF: {output}")
    print(f"boot={boot['boot_sec']}.{boot['boot_usec']:06d} "
          f"text=0x{text_base:x}+0x{text_size:x} "
          f"data=0x{data_base:x}+0x{data_size:x}")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("capture_folder", type=Path)
    parser.add_argument("output", type=Path)
    args = parser.parse_args()
    build(args.capture_folder, args.output)
