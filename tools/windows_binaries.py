#!/usr/bin/env python3
"""What a Windows program or DLL needs in order to start: the processor it is for and the DLLs it
imports, read from its headers (the PE format), and which DLLs have to go beside it.

Lo que un programa o una DLL de Windows necesita para arrancar: el procesador para el que es y las
DLL que importa, leídos de sus encabezados (el formato PE), y qué DLL tienen que ir a su lado.

The Windows packages are built on Linux, where their programs cannot be run. This is how the build
knows that each of them will start on a player's computer: everything it imports, and everything
that those import in turn, is either a part of Windows or a file that goes beside it. A DLL that is
neither (the Visual C++ runtime, which Dawn and nodtool use and which not every Windows has) would
only show up on the computer of a player without it, as "the program can't start because ... is
missing".

Los paquetes de Windows se arman en Linux, donde sus programas no se pueden correr. Así se sabe que
cada uno va a arrancar en la computadora de un jugador: todo lo que importa, y lo que eso importa,
es parte de Windows o va a su lado. Una DLL que no es ninguna de las dos cosas (el runtime de Visual
C++, que usan Dawn y nodtool y que no todo Windows tiene) solo aparecería en la computadora de un
jugador que no la tiene.
"""

import re
import struct
from collections.abc import Mapping, Sequence
from dataclasses import dataclass
from pathlib import Path

# The processors of the headers' "machine" field that the packages can be for.
MACHINES = {0x8664: "x64", 0xAA64: "arm64", 0x014C: "x86"}
PE32_PLUS = 0x020B  # the "magic" of a 64-bit optional header
# The tables of imported DLLs: (the number of the table among the headers' data directories, the
# size of each entry, where the entry keeps the address of the DLL's name). The import table, and
# the table of the DLLs that are loaded the first time they are used (delay load).
IMPORT_TABLES = ((1, 20, 12), (13, 32, 4))

# The DLLs of Windows itself, on every Windows 10 (1903 or newer) and 11, in lower case. Only what
# the packages' programs import or could import; a DLL that is not here and not beside the program
# stops the build, and if it is a part of Windows it is added here.
# fmt: off
WINDOWS_DLLS = frozenset((
    "advapi32.dll", "avrt.dll", "bcrypt.dll", "bcryptprimitives.dll", "cfgmgr32.dll", "combase.dll",
    "comctl32.dll", "comdlg32.dll", "crypt32.dll", "d2d1.dll", "d3d9.dll", "d3d11.dll", "d3d12.dll",
    "d3dcompiler_47.dll", "dbghelp.dll", "dcomp.dll", "dinput8.dll", "dnsapi.dll", "dsound.dll",
    "dwmapi.dll", "dwrite.dll", "dxcore.dll", "dxgi.dll", "gdi32.dll", "gdiplus.dll", "hid.dll",
    "imm32.dll", "iphlpapi.dll", "kernel32.dll", "kernelbase.dll", "mf.dll", "mfplat.dll",
    "mfreadwrite.dll", "mpr.dll", "msimg32.dll", "msvcrt.dll", "mswsock.dll", "ncrypt.dll",
    "netapi32.dll", "normaliz.dll", "ntdll.dll", "ole32.dll", "oleaut32.dll", "opengl32.dll",
    "powrprof.dll", "propsys.dll", "psapi.dll", "rpcrt4.dll", "secur32.dll", "setupapi.dll",
    "shcore.dll", "shell32.dll", "shlwapi.dll", "ucrtbase.dll", "user32.dll", "userenv.dll",
    "uxtheme.dll", "version.dll", "winhttp.dll", "wininet.dll", "winmm.dll", "wintrust.dll",
    "wldap32.dll", "ws2_32.dll", "wtsapi32.dll", "xinput1_4.dll", "xinput9_1_0.dll", "winspool.drv",
))
# fmt: on
# The "API sets", names that Windows maps to its own DLLs (api-ms-win-crt-*, the C runtime, ...).
WINDOWS_API_SET = re.compile(r"(api|ext)-ms-[a-z0-9-]+\.dll")


class WindowsBinaryError(Exception):
    """A file that is not a whole Windows program or DLL, or one that would not start."""


@dataclass(frozen=True)
class WindowsBinary:
    path: Path
    machine: str  # "x64", "arm64", "x86", or the number of another processor
    imports: tuple[str, ...]  # the DLLs it imports, as it names them


def read_windows_binary(path: Path) -> WindowsBinary:
    """The processor that the program or DLL at `path` is for and the DLLs it imports."""
    data = path.read_bytes()
    try:
        return _read(path, data)
    except (struct.error, ValueError, UnicodeDecodeError) as problem:
        raise WindowsBinaryError(f"{path} is not a whole Windows program or DLL ({problem})") from None


def _read(path: Path, data: bytes) -> WindowsBinary:
    (header,) = struct.unpack_from("<I", data, 0x3C)
    if data[:2] != b"MZ" or data[header : header + 4] != b"PE\0\0":
        raise WindowsBinaryError(f"{path} is not a Windows program or DLL")
    machine, section_count = struct.unpack_from("<HH", data, header + 4)
    (optional_header_size,) = struct.unpack_from("<H", data, header + 20)
    optional_header = header + 24
    (magic,) = struct.unpack_from("<H", data, optional_header)
    directories = optional_header + (112 if magic == PE32_PLUS else 96)
    (directory_count,) = struct.unpack_from("<I", data, directories - 4)
    # Each section: (its size in memory, its address in memory, its size in the file, its offset).
    section_table = optional_header + optional_header_size
    sections = [
        struct.unpack_from("<IIII", data, section_table + 40 * index + 8) for index in range(section_count)
    ]

    def file_offset(address: int) -> int:
        for memory_size, memory_address, file_size, offset in sections:
            if memory_address <= address < memory_address + max(memory_size, file_size):
                return offset + address - memory_address
        raise ValueError(f"the address {address:#x} is in none of its sections")

    imports: list[str] = []
    for table, entry_size, name_field in IMPORT_TABLES:
        if table >= directory_count:
            continue
        (address,) = struct.unpack_from("<I", data, directories + 8 * table)
        if not address:
            continue
        entry = file_offset(address)
        while any(data[entry : entry + entry_size]):  # the table ends with an entry of zeros
            (name_address,) = struct.unpack_from("<I", data, entry + name_field)
            start = file_offset(name_address)
            imports.append(data[start : data.index(b"\0", start)].decode("ascii"))
            entry += entry_size
    return WindowsBinary(path, MACHINES.get(machine, f"{machine:#06x}"), tuple(imports))


def is_part_of_windows(dll: str) -> bool:
    name = dll.lower()
    return name in WINDOWS_DLLS or WINDOWS_API_SET.fullmatch(name) is not None


def dlls_in(folder: Path) -> dict[str, Path]:
    """The DLLs of `folder` (not of its subfolders), by their names in lower case."""
    return {
        path.name.lower(): path
        for path in sorted(folder.iterdir())
        if path.suffix.lower() == ".dll" and path.is_file()
    }


def needed_dlls(binaries: Sequence[Path], available: Mapping[str, Path]) -> dict[str, Path]:
    """The DLLs of `available` (by their names in lower case) that `binaries` need beside them: the
    ones they import, the ones those import, and so on.

    Raises WindowsBinaryError when one of them is for another processor than the first of
    `binaries`, or when one imports a DLL that is neither in `available` nor a part of Windows."""
    machine = read_windows_binary(binaries[0]).machine
    needed: dict[str, Path] = {}
    pending = list(binaries)
    while pending:
        binary = read_windows_binary(pending.pop())
        if binary.machine != machine:
            raise WindowsBinaryError(
                f"{binary.path.name} is for {binary.machine}, and {binaries[0].name} for {machine}"
            )
        for dll in binary.imports:
            name = dll.lower()
            if name in needed or is_part_of_windows(name):
                continue
            if name not in available:
                raise WindowsBinaryError(
                    f"{binary.path.name} needs {dll}, which is not beside it and is not a part of Windows"
                )
            needed[name] = available[name]
            pending.append(available[name])
    return needed
