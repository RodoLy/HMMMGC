"""Minimal reader for GameCube/Wii DOL executables.

Lector mínimo de ejecutables DOL de GameCube/Wii.

A DOL file is a 0x100-byte header followed by up to 7 text (code) and
11 data sections. The header stores, for each section, its offset in the
file, its load address in RAM and its size. This module only does what the
other tools in this folder need: translate RAM addresses to file offsets and
read values at a given address.

Un DOL es una cabecera de 0x100 bytes seguida de hasta 7 secciones de texto
(código) y 11 de datos. La cabecera guarda, para cada sección, su offset en
el archivo, su dirección en RAM y su tamaño. Este módulo solo hace lo que
necesitan las demás herramientas: traducir direcciones de RAM a offsets del
archivo y leer valores en una dirección.
"""

import struct
from dataclasses import dataclass
from pathlib import Path


@dataclass
class Section:
    name: str
    file_offset: int
    address: int
    size: int

    @property
    def end(self) -> int:
        return self.address + self.size

    @property
    def is_text(self) -> bool:
        return self.name.startswith("text")

    def contains(self, address: int) -> bool:
        return self.address <= address < self.end


class Dol:
    def __init__(self, path: Path):
        self.path = Path(path)
        self.data = self.path.read_bytes()

        offsets = struct.unpack(">18I", self.data[0x00:0x48])
        addresses = struct.unpack(">18I", self.data[0x48:0x90])
        sizes = struct.unpack(">18I", self.data[0x90:0xD8])
        self.bss_address, self.bss_size, self.entry = struct.unpack(">3I", self.data[0xD8:0xE4])

        self.sections: list[Section] = []
        for i in range(18):
            if sizes[i] == 0:
                continue
            name = f"text{i}" if i < 7 else f"data{i - 7}"
            self.sections.append(Section(name, offsets[i], addresses[i], sizes[i]))

    # -- address helpers ---------------------------------------------------

    def section_at(self, address: int) -> Section | None:
        for section in self.sections:
            if section.contains(address):
                return section
        return None

    def is_code(self, address: int) -> bool:
        section = self.section_at(address)
        return section is not None and section.is_text

    def is_mapped(self, address: int) -> bool:
        """True for addresses inside a section or inside .bss."""
        if self.section_at(address) is not None:
            return True
        return self.bss_address <= address < self.bss_address + self.bss_size

    def offset_of(self, address: int) -> int:
        section = self.section_at(address)
        if section is None:
            raise ValueError(f"address 0x{address:08X} is not inside any section")
        return section.file_offset + (address - section.address)

    def address_of(self, file_offset: int) -> int | None:
        for section in self.sections:
            if section.file_offset <= file_offset < section.file_offset + section.size:
                return section.address + (file_offset - section.file_offset)
        return None

    # -- reading -----------------------------------------------------------

    def read(self, address: int, size: int) -> bytes:
        start = self.offset_of(address)
        return self.data[start : start + size]

    def u32(self, address: int) -> int:
        return struct.unpack(">I", self.read(address, 4))[0]

    def u16(self, address: int) -> int:
        return struct.unpack(">H", self.read(address, 2))[0]

    def f32(self, address: int) -> float:
        return struct.unpack(">f", self.read(address, 4))[0]

    def c_string(self, address: int, limit: int = 256) -> str | None:
        """Return the NUL-terminated ASCII/Shift-JIS string at address, or None."""
        if self.section_at(address) is None:
            return None
        raw = self.read(address, limit).split(b"\0", 1)[0]
        if not raw:
            return None
        try:
            text = raw.decode("ascii")
        except UnicodeDecodeError:
            try:
                text = raw.decode("shift_jis")
            except UnicodeDecodeError:
                return None
        if not all(ch.isprintable() or ch in "\n\t" for ch in text):
            return None
        return text

    def find_bytes(self, needle: bytes) -> list[int]:
        """Every RAM address where needle appears inside a section."""
        hits = []
        start = 0
        while True:
            pos = self.data.find(needle, start)
            if pos < 0:
                return hits
            address = self.address_of(pos)
            if address is not None:
                hits.append(address)
            start = pos + 1

    def text_sections(self) -> list[Section]:
        return [s for s in self.sections if s.is_text]
