"""Made-up Windows programs and DLLs for the tests: just the headers that say the processor and the
DLLs imported, which is what tools/windows_binaries.py reads. Nothing in them can run.

Programas y DLL de Windows inventados para las pruebas: solo los encabezados que dicen el procesador
y las DLL que importan. Nada en ellos se puede correr.
"""

import struct
from collections.abc import Sequence
from pathlib import Path

MACHINE_NUMBERS = {"x64": 0x8664, "arm64": 0xAA64, "x86": 0x014C}
SECTION_ADDRESS = 0x1000  # where the only section is in memory
SECTION_OFFSET = 0x200  # and in the file
OPTIONAL_HEADER_SIZE = 240  # of a 64-bit program: 112 bytes and 16 data directories of 8


def write_windows_binary(path: Path, machine: str, imports: Sequence[str] = ()) -> Path:
    """Writes at `path` the headers of a program for `machine` ("x64", "arm64" or "x86") that
    imports `imports`, with its import table in its only section."""
    table_size = 20 * (len(imports) + 1)  # one entry per DLL and one of zeros
    entries = b""
    names = b""
    for name in imports:
        name_address = SECTION_ADDRESS + table_size + len(names)
        entries += struct.pack("<IIIII", 0, 0, 0, name_address, 0)
        names += name.encode("ascii") + b"\0"
    section = entries + bytes(20) + names

    dos_header = bytearray(64)
    dos_header[:2] = b"MZ"
    struct.pack_into("<I", dos_header, 0x3C, len(dos_header))
    file_header = struct.pack("<HHIIIHH", MACHINE_NUMBERS[machine], 1, 0, 0, 0, OPTIONAL_HEADER_SIZE, 0x22)
    optional_header = bytearray(OPTIONAL_HEADER_SIZE)
    struct.pack_into("<H", optional_header, 0, 0x020B)
    struct.pack_into("<I", optional_header, 108, 16)  # the number of data directories
    struct.pack_into("<II", optional_header, 112 + 8, SECTION_ADDRESS, table_size)  # the import table
    section_header = struct.pack(
        "<8sIIIIIIHHI", b".idata", len(section), SECTION_ADDRESS, len(section), SECTION_OFFSET, 0, 0, 0, 0, 0
    )
    headers = bytes(dos_header) + b"PE\0\0" + file_header + bytes(optional_header) + section_header
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(headers.ljust(SECTION_OFFSET, b"\0") + section)
    return path
