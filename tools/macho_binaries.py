#!/usr/bin/env python3
"""Read what a macOS program needs to start: its processors, the oldest macOS it runs on, and the
libraries it loads.

Lee lo que un programa de macOS necesita para arrancar: sus procesadores, el macOS más viejo en el
que corre y las bibliotecas que carga.

tools/package_native_unix.py checks with this that each program of the Mac package is for the
package's processor (or a universal program that has it), that every library it loads is part of
macOS or is in the package, and which macOS it needs at least. It reads the files itself (the
Mach-O format: the header, or the list of a universal file, and the load commands), so it works on
any computer and in the tests.

tools/package_native_unix.py comprueba con esto que cada programa del paquete de Mac es para el
procesador del paquete, que cada biblioteca que carga es de macOS o está en el paquete, y qué macOS
necesita como mínimo.
"""

import struct
from collections.abc import Sequence
from dataclasses import dataclass, field
from pathlib import Path

MACH_O_64 = 0xFEEDFACF  # a 64-bit program, as its first four bytes read in little-endian
UNIVERSAL = 0xCAFEBABE  # a universal file, as its first four bytes read in big-endian
PROCESSORS = {0x0100000C: "arm64", 0x01000007: "x64"}  # CPU_TYPE_ARM64, CPU_TYPE_X86_64

LC_LOAD_DYLIB = 0xC
LC_LOAD_WEAK_DYLIB = 0x80000018
LC_REEXPORT_DYLIB = 0x8000001F
LC_VERSION_MIN_MACOSX = 0x24
LC_BUILD_VERSION = 0x32
LOADED_LIBRARIES = (LC_LOAD_DYLIB, LC_LOAD_WEAK_DYLIB, LC_REEXPORT_DYLIB)
PLATFORM_MACOS = 1

# Where the libraries that are part of macOS are.
SYSTEM_FOLDERS = ("/usr/lib/", "/System/Library/")


class MachOError(Exception):
    """The file is not a program that can be read, or would not start."""


@dataclass
class MachOBinary:
    path: Path
    processor: str
    minimum_macos: tuple[int, int, int] = (0, 0, 0)
    libraries: list[str] = field(default_factory=list)


def version(encoded: int) -> tuple[int, int, int]:
    """A version as the load commands keep it: xxxx.yy.zz in nibbles."""
    return encoded >> 16, (encoded >> 8) & 0xFF, encoded & 0xFF


def read_one(path: Path, data: bytes, offset: int) -> MachOBinary:
    magic, cpu_type, _, _, command_count, _, _, _ = struct.unpack_from("<IIIIIIII", data, offset)
    if magic != MACH_O_64:
        raise MachOError(f"{path} is not a 64-bit macOS program")
    if cpu_type not in PROCESSORS:
        raise MachOError(f"{path} is for an unknown processor ({cpu_type:#x})")
    binary = MachOBinary(path, PROCESSORS[cpu_type])
    position = offset + 32
    for _ in range(command_count):
        command, size = struct.unpack_from("<II", data, position)
        if command in LOADED_LIBRARIES:
            (name_offset,) = struct.unpack_from("<I", data, position + 8)
            name = data[position + name_offset : position + size].split(b"\0", 1)[0]
            binary.libraries.append(name.decode("utf-8", "replace"))
        elif command == LC_BUILD_VERSION:
            platform, minimum = struct.unpack_from("<II", data, position + 8)
            if platform == PLATFORM_MACOS:
                binary.minimum_macos = version(minimum)
        elif command == LC_VERSION_MIN_MACOSX:
            (minimum,) = struct.unpack_from("<I", data, position + 8)
            binary.minimum_macos = version(minimum)
        position += size
    return binary


def read_macho_binary(path: Path, processor: str) -> MachOBinary:
    """The part of the file at `path` for `processor` ("arm64" or "x64"): the whole file, or its
    slice when it is a universal file. Stops with a MachOError when it has none for it."""
    data = path.read_bytes()
    if len(data) >= 8 and struct.unpack_from(">I", data)[0] == UNIVERSAL:
        (count,) = struct.unpack_from(">I", data, 4)
        found = []
        for index in range(count):
            cpu_type, _, offset, _, _ = struct.unpack_from(">IIIII", data, 8 + 20 * index)
            found.append(PROCESSORS.get(cpu_type, f"{cpu_type:#x}"))
            if PROCESSORS.get(cpu_type) == processor:
                return read_one(path, data, offset)
        raise MachOError(f"{path} is for {', '.join(found)}, and the package for {processor}")
    if len(data) < 32 or struct.unpack_from("<I", data)[0] != MACH_O_64:
        raise MachOError(f"{path} is not a 64-bit macOS program")
    binary = read_one(path, data, 0)
    if binary.processor != processor:
        raise MachOError(f"{path} is for {binary.processor}, and the package for {processor}")
    return binary


def check_programs(programs: Sequence[Path], processor: str) -> tuple[int, int, int]:
    """Checks that each program is for `processor` and that every library it loads is part of macOS
    or is beside it (@executable_path, @loader_path or @rpath, which the package's programs only use
    for their own libraries); returns the newest of the oldest macOS versions they need."""
    newest = (0, 0, 0)
    for program in programs:
        binary = read_macho_binary(program, processor)
        newest = max(newest, binary.minimum_macos)
        for library in binary.libraries:
            if library.startswith(SYSTEM_FOLDERS):
                continue
            if library.startswith(("@executable_path/", "@loader_path/", "@rpath/")):
                name = library.split("/")[-1]
                if not any(path.name == name for path in program.parent.parent.rglob(name)):
                    raise MachOError(f"{program} loads {library}, which the package does not have")
                continue
            raise MachOError(f"{program} loads {library}, which is not part of macOS")
    return newest
