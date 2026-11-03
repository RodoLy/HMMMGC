"""Read and write symbols.txt in decomp-toolkit's syntax.

Lee y escribe symbols.txt con la sintaxis de decomp-toolkit.

symbols.txt is the single source of truth for names and sizes, the same
role it plays in every dtk-based GameCube/Wii decomp. One symbol per line:

symbols.txt es la única fuente de verdad para nombres y tamaños, igual que
en todos los decomps de GameCube/Wii basados en dtk. Un símbolo por línea:

    OSInit = .text:0x8016EF2C; // type:function size:0x4E0
    jumptable_80202AB4 = .data:0x80202AB4; // type:object size:0x30 scope:local

Names that people give by hand are kept when the file is regenerated;
generated placeholders (fn_XXXXXXXX, ...) are replaced freely.

Los nombres puestos a mano se conservan al regenerar el archivo; los
genéricos (fn_XXXXXXXX, ...) se reemplazan libremente.
"""

import bisect
import re
from collections.abc import Iterable
from dataclasses import dataclass
from pathlib import Path

from dol import Dol

# DOL section -> the section name CodeWarrior gave it. makedol always writes
# them in this order; the content confirms each one (data1 is a table of
# function/size/extab triples, data2 and data3 are NULL-terminated function
# lists, and _rom_copy_info lists the same addresses). See docs/FUNCTION_MAP.md.
SECTION_NAMES = {
    "text0": ".init",
    "text1": ".text",
    "data0": "extab",
    "data1": "extabindex",
    "data2": ".ctors",
    "data3": ".dtors",
    "data4": ".rodata",
    "data5": ".data",
    "data6": ".sdata",
    "data7": ".sdata2",
}

# Prefixes of names the tools make up. Anything else was chosen by a person
# (or by a naming rule with evidence) and must survive regeneration.
GENERATED_NAME_PREFIXES = ("fn_", "jumptable_", "string_", "lbl_")

SYMBOL_LINE = re.compile(
    r"^(?P<name>\S+) = (?P<section>\S+):0x(?P<address>[0-9A-Fa-f]+);"
    r" // type:(?P<kind>\w+)"
    r"(?: size:0x(?P<size>[0-9A-Fa-f]+))?"
    r"(?: scope:(?P<scope>\w+))?"
)


@dataclass
class Symbol:
    name: str
    section: str
    address: int
    kind: str  # "function" or "object"
    size: int
    scope: str | None = None

    @property
    def has_generated_name(self) -> bool:
        return self.name.startswith(GENERATED_NAME_PREFIXES)

    def to_line(self) -> str:
        line = f"{self.name} = {self.section}:0x{self.address:08X}; // type:{self.kind} size:0x{self.size:X}"
        if self.scope:
            line += f" scope:{self.scope}"
        return line


def section_name_for(dol: Dol, address: int) -> str:
    """Section name for an address, including .bss/.sbss which the DOL does
    not store as sections."""
    section = dol.section_at(address)
    if section is not None:
        return SECTION_NAMES[section.name]
    return ".bss"


class SymbolTable:
    def __init__(self, symbols: Iterable[Symbol] = ()):
        self.by_address: dict[int, Symbol] = {}
        self._sorted_addresses: list[int] | None = None  # built on first lookup
        for symbol in symbols:
            self.add(symbol)

    def add(self, symbol: Symbol) -> None:
        self.by_address[symbol.address] = symbol
        self._sorted_addresses = None

    def __iter__(self):
        return iter(sorted(self.by_address.values(), key=lambda symbol: symbol.address))

    def __len__(self) -> int:
        return len(self.by_address)

    def by_name(self, name: str) -> Symbol | None:
        for symbol in self.by_address.values():
            if symbol.name == name:
                return symbol
        return None

    def describe(self, address: int) -> str:
        """`name` for a symbol's start, `name+0x10` inside it, else the
        bare address."""
        if self._sorted_addresses is None:
            self._sorted_addresses = sorted(self.by_address)
        addresses = self._sorted_addresses
        position = bisect.bisect_right(addresses, address) - 1
        if position >= 0:
            symbol = self.by_address[addresses[position]]
            if address == symbol.address:
                return symbol.name
            if address < symbol.address + symbol.size:
                return f"{symbol.name}+0x{address - symbol.address:X}"
        return f"0x{address:08X}"

    def keep_names_from(self, previous: "SymbolTable") -> list[str]:
        """Copy hand-given names from an earlier symbols.txt onto the same
        addresses. Returns warnings for names that no longer match anything."""
        warnings = []
        for address, old in previous.by_address.items():
            if old.has_generated_name:
                continue
            current = self.by_address.get(address)
            if current is None:
                warnings.append(f"{old.name} at 0x{address:08X} no longer starts a symbol")
                continue
            if not current.has_generated_name and current.name != old.name:
                warnings.append(f"0x{address:08X}: keeping {old.name}, a naming rule suggests {current.name}")
            current.name = old.name
        return warnings

    @classmethod
    def read(cls, path: Path) -> "SymbolTable":
        table = cls()
        if not path.is_file():
            return table
        for line in path.read_text().splitlines():
            match = SYMBOL_LINE.match(line)
            if match is None:
                continue
            table.add(
                Symbol(
                    name=match["name"],
                    section=match["section"],
                    address=int(match["address"], 16),
                    kind=match["kind"],
                    size=int(match["size"] or "0", 16),
                    scope=match["scope"],
                )
            )
        return table

    def write(self, path: Path) -> None:
        path.write_text("".join(symbol.to_line() + "\n" for symbol in self))
