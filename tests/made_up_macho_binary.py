"""Made-up macOS programs for the tests: just what tools/macho_binaries.py reads (the processor, the
oldest macOS, the libraries loaded), alone or in a universal file. Nothing in them can run.

Programas de macOS inventados para las pruebas: solo lo que lee tools/macho_binaries.py. Nada en
ellos se puede correr.
"""

import struct
from collections.abc import Sequence
from pathlib import Path

CPU_TYPES = {"arm64": 0x0100000C, "x64": 0x01000007}


def macho(processor: str, libraries: Sequence[str] = (), minimum_macos: tuple[int, int] = (12, 0)) -> bytes:
    """The bytes of a 64-bit program for `processor` that needs `minimum_macos` and loads `libraries`."""
    commands = struct.pack(
        "<IIIIII", 0x32, 24, 1, (minimum_macos[0] << 16) | (minimum_macos[1] << 8), 15 << 16, 0
    )  # LC_BUILD_VERSION, macOS
    for library in libraries:
        name = library.encode() + b"\0"
        name += bytes(-(24 + len(name)) % 8)
        commands += (
            struct.pack("<IIIIII", 0xC, 24 + len(name), 24, 2, 0x10000, 0x10000) + name
        )  # LC_LOAD_DYLIB
    header = struct.pack(
        "<IIIIIIII", 0xFEEDFACF, CPU_TYPES[processor], 0, 2, 1 + len(libraries), len(commands), 0, 0
    )
    return header + commands


def write_macho_binary(
    path: Path, processor: str, libraries: Sequence[str] = (), minimum_macos: tuple[int, int] = (12, 0)
) -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(macho(processor, libraries, minimum_macos))
    return path


def write_universal_binary(path: Path, slices: Sequence[bytes], processors: Sequence[str]) -> Path:
    """A universal file with one slice per processor, each at a 4 KiB boundary."""
    header = struct.pack(">II", 0xCAFEBABE, len(slices))
    body = b""
    offset = 0x1000
    for data, processor in zip(slices, processors, strict=True):
        header += struct.pack(">IIIII", CPU_TYPES[processor], 0, offset, len(data), 12)
        body += data + bytes(-len(data) % 0x1000)
        offset += len(data) + (-len(data) % 0x1000)
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(header + bytes(0x1000 - len(header)) + body)
    return path
