"""Made-up Linux programs and libraries for the tests: just what tools/elf_binaries.py reads (the
processor, the libraries needed, the RUNPATH and the versions asked for). Nothing in them can run.

Programas y bibliotecas de Linux inventados para las pruebas: solo lo que lee
tools/elf_binaries.py. Nada en ellos se puede correr.
"""

import struct
from collections.abc import Mapping, Sequence
from pathlib import Path

MACHINE_NUMBERS = {"x64": 0x3E, "arm64": 0xB7}
HEADER_SIZE = 64
PROGRAM_HEADER_SIZE = 56
DT_NEEDED, DT_STRTAB, DT_RUNPATH, DT_VERNEED, DT_VERNEEDNUM = 1, 5, 29, 0x6FFFFFFE, 0x6FFFFFFF


def write_elf_binary(
    path: Path,
    machine: str,
    needed: Sequence[str] = (),
    runpath: str = "",
    versions: Mapping[str, Sequence[str]] | None = None,
) -> Path:
    """Writes at `path` a 64-bit program for `machine` ("x64" or "arm64") that needs `needed`, looks
    for them in `runpath` and asks each library of `versions` for those versions. The whole file is
    one loaded segment at address 0, so addresses and offsets are the same."""
    versions = dict(versions or {})
    strings = bytearray(b"\0")

    def string(text: str) -> int:
        offset = len(strings)
        strings.extend(text.encode() + b"\0")
        return offset

    needed_offsets = [string(name) for name in needed]
    runpath_offset = string(runpath) if runpath else None
    version_records = [
        (string(library), [string(name) for name in names]) for library, names in versions.items()
    ]

    tags = [(DT_NEEDED, offset) for offset in needed_offsets]
    if runpath_offset is not None:
        tags.append((DT_RUNPATH, runpath_offset))
    tags += [(DT_STRTAB, 0), (DT_VERNEED, 0), (DT_VERNEEDNUM, len(version_records)), (0, 0)]
    dynamic_offset = HEADER_SIZE + 2 * PROGRAM_HEADER_SIZE
    dynamic_size = 16 * len(tags)
    verneed_offset = dynamic_offset + dynamic_size
    verneed = bytearray()
    for index, (library, names) in enumerate(version_records):
        record_size = 16 + 16 * len(names)
        following = record_size if index + 1 < len(version_records) else 0
        verneed += struct.pack("<HHIII", 1, len(names), library, 16, following)
        for position, name in enumerate(names):
            next_auxiliary = 16 if position + 1 < len(names) else 0
            verneed += struct.pack("<IHHII", 0, 0, position + 2, name, next_auxiliary)
    strings_offset = verneed_offset + len(verneed)
    tags = [
        (tag, strings_offset if tag == DT_STRTAB else verneed_offset if tag == DT_VERNEED else value)
        for tag, value in tags
    ]
    if not version_records:
        tags = [(tag, value) for tag, value in tags if tag not in (DT_VERNEED, DT_VERNEEDNUM)]
        tags += [(0, 0)] * 2  # keep the size: the table ends at the first zero
    total = strings_offset + len(strings)

    header = bytearray(HEADER_SIZE)
    header[:4] = b"\x7fELF"
    header[4], header[5], header[6] = 2, 1, 1
    struct.pack_into("<HHI", header, 0x10, 3, MACHINE_NUMBERS[machine], 1)
    struct.pack_into("<Q", header, 0x20, HEADER_SIZE)
    struct.pack_into("<HHHH", header, 0x34, HEADER_SIZE, PROGRAM_HEADER_SIZE, 2, 0)
    load = struct.pack("<IIQQQQQQ", 1, 5, 0, 0, 0, total, total, 0x1000)
    dynamic = struct.pack("<IIQQQQQQ", 2, 6, dynamic_offset, dynamic_offset, 0, dynamic_size, dynamic_size, 8)
    body = b"".join(struct.pack("<qQ", tag, value) for tag, value in tags)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(bytes(header) + load + dynamic + body + bytes(verneed) + bytes(strings))
    return path
