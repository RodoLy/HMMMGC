#!/usr/bin/env python3
"""Read what a Linux program needs to start: its processor, the shared libraries it names, where
it looks for them, and the newest version of glibc it asks for.

Lee lo que un programa de Linux necesita para arrancar: su procesador, las bibliotecas
compartidas que nombra, dónde las busca y la versión más nueva de glibc que pide.

The Linux package is assembled on one computer and run on others, so tools/package_native_unix.py
checks with this that each of its programs would start on a player's computer: that it is for the
package's processor, that every library it needs is beside it (where its RUNPATH says, as
$ORIGIN/...) or is one that every Linux desktop has, and how new a glibc it needs. It reads the
files itself (the ELF format: the header, the program headers, the dynamic section and the
version needs), so it works on any computer and in the tests, without readelf.

Only 64-bit little-endian files are read: x86-64 and ARM64, the package's processors.

El paquete de Linux se arma en una computadora y se corre en otras, así que
tools/package_native_unix.py comprueba con esto que cada programa arrancaría: que es para el
procesador del paquete, que cada biblioteca que necesita está a su lado o es de las que trae todo
Linux de escritorio, y qué glibc necesita.
"""

import re
import struct
from collections.abc import Iterable, Sequence
from dataclasses import dataclass, field
from pathlib import Path

ELF_MAGIC = b"\x7fELF"
ELF_64_BIT = 2
LITTLE_ENDIAN = 1
MACHINES = {0x3E: "x64", 0xB7: "arm64"}  # e_machine: EM_X86_64, EM_AARCH64

PT_LOAD = 1
PT_DYNAMIC = 2
DT_NULL = 0
DT_NEEDED = 1
DT_STRTAB = 5
DT_RPATH = 15
DT_RUNPATH = 29
DT_VERNEED = 0x6FFFFFFE
DT_VERNEEDNUM = 0x6FFFFFFF

# The libraries that every Linux desktop has (glibc, GCC's runtime, zlib and zstd), which a program
# of the package may need without carrying them.
SYSTEM_LIBRARIES = frozenset(
    {
        "libc.so.6",
        "libm.so.6",
        "libdl.so.2",
        "libpthread.so.0",
        "librt.so.1",
        "libutil.so.1",
        "ld-linux-x86-64.so.2",
        "ld-linux-aarch64.so.1",
        "libstdc++.so.6",
        "libgcc_s.so.1",
        "libz.so.1",
        "libzstd.so.1",
    }
)
GLIBC_VERSION = re.compile(r"GLIBC_(\d+)\.(\d+)(?:\.(\d+))?")


class ElfError(Exception):
    """The file is not a program that can be read, or would not start."""


@dataclass
class ElfBinary:
    path: Path
    machine: str  # "x64" or "arm64"
    needed: list[str] = field(default_factory=list)
    runpath: list[str] = field(default_factory=list)  # the folders of DT_RUNPATH (or DT_RPATH)
    versions: dict[str, list[str]] = field(default_factory=dict)  # library -> versions it asks of it

    @property
    def glibc(self) -> tuple[int, ...]:
        """The newest glibc version that it asks for, (0,) when it asks for none."""
        newest: tuple[int, ...] = (0,)
        for names in self.versions.values():
            for name in names:
                if match := GLIBC_VERSION.fullmatch(name):
                    newest = max(newest, tuple(int(part) for part in match.groups() if part is not None))
        return newest


def string_at(data: bytes, offset: int) -> str:
    end = data.index(b"\0", offset)
    return data[offset:end].decode("utf-8", "replace")


def read_elf_binary(path: Path) -> ElfBinary:
    data = path.read_bytes()
    if data[:4] != ELF_MAGIC:
        raise ElfError(f"{path} is not a Linux program (ELF)")
    if data[4] != ELF_64_BIT or data[5] != LITTLE_ENDIAN:
        raise ElfError(f"{path} is not a 64-bit little-endian program")
    (machine,) = struct.unpack_from("<H", data, 0x12)
    if machine not in MACHINES:
        raise ElfError(f"{path} is for an unknown processor ({machine:#x})")
    (program_headers,) = struct.unpack_from("<Q", data, 0x20)
    header_size, header_count = struct.unpack_from("<HH", data, 0x36)

    loads: list[tuple[int, int, int]] = []  # (address, offset, size in the file)
    dynamic: tuple[int, int] | None = None
    for index in range(header_count):
        kind, _, offset, address, _, file_size = struct.unpack_from(
            "<IIQQQQ", data, program_headers + index * header_size
        )
        if kind == PT_LOAD:
            loads.append((address, offset, file_size))
        elif kind == PT_DYNAMIC:
            dynamic = (offset, file_size)
    binary = ElfBinary(path, MACHINES[machine])
    if dynamic is None:
        return binary  # a static program needs nothing

    def file_offset(address: int) -> int:
        for start, offset, size in loads:
            if start <= address < start + size:
                return offset + address - start
        raise ElfError(f"{path}: the address {address:#x} is in no part of the file")

    entries: list[tuple[int, int]] = []
    for position in range(dynamic[0], dynamic[0] + dynamic[1], 16):
        tag, value = struct.unpack_from("<qQ", data, position)
        if tag == DT_NULL:
            break
        entries.append((tag, value))
    values = dict(entries)
    if DT_STRTAB not in values:
        raise ElfError(f"{path} has a dynamic section without strings")
    strings = file_offset(values[DT_STRTAB])
    binary.needed = [string_at(data, strings + value) for tag, value in entries if tag == DT_NEEDED]
    for tag in (DT_RUNPATH, DT_RPATH):
        if tag in values:
            binary.runpath = [part for part in string_at(data, strings + values[tag]).split(":") if part]
            break
    if DT_VERNEED in values:
        position = file_offset(values[DT_VERNEED])
        for _ in range(values.get(DT_VERNEEDNUM, 0)):
            _, count, library, first, following = struct.unpack_from("<HHIII", data, position)
            names = binary.versions.setdefault(string_at(data, strings + library), [])
            auxiliary = position + first
            for _ in range(count):
                _, _, _, name, next_auxiliary = struct.unpack_from("<IHHII", data, auxiliary)
                names.append(string_at(data, strings + name))
                auxiliary += next_auxiliary
            if following == 0:
                break
            position += following
    return binary


def library_folders(binary: ElfBinary) -> list[Path]:
    """The folders where the program looks for its libraries first: its RUNPATH, with $ORIGIN as
    its own folder. A folder of another computer is not one."""
    folders = []
    for entry in binary.runpath:
        for origin in ("$ORIGIN", "${ORIGIN}"):
            entry = entry.replace(origin, str(binary.path.parent))
        if entry.startswith(str(binary.path.parent.anchor)) and "$" not in entry:
            folders.append(Path(entry))
    return folders


def check_programs(
    programs: Sequence[Path], machine: str, system_libraries: Iterable[str] = SYSTEM_LIBRARIES
) -> tuple[int, ...]:
    """Checks that each program, and each library of the package that it needs, is for `machine`
    and finds all it needs; returns the newest glibc that any of them asks for. Stops with an
    ElfError naming the first problem."""
    system = frozenset(system_libraries)
    pending = [Path(program) for program in programs]
    seen: set[Path] = set()
    newest: tuple[int, ...] = (0,)
    while pending:
        path = pending.pop()
        if path.resolve() in seen:
            continue
        seen.add(path.resolve())
        binary = read_elf_binary(path.resolve())
        binary.path = path  # $ORIGIN is the folder of the name it is run by, before links
        if binary.machine != machine:
            raise ElfError(f"{path} is for {binary.machine}, and the package for {machine}")
        newest = max(newest, binary.glibc)
        folders = library_folders(binary)
        for library in binary.needed:
            found = next((folder / library for folder in folders if (folder / library).exists()), None)
            if found is not None:
                pending.append(found)
            elif library not in system:
                raise ElfError(f"{path} needs {library}, which is not beside it and not part of Linux")
    return newest
