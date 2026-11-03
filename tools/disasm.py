#!/usr/bin/env python3
"""Print a readable listing of one function (or an address range) of main.dol.

Imprime un listado legible de una función (o un rango de direcciones) de main.dol.

Each line shows the address, the raw word, the instruction and, when it can
be worked out, what the instruction refers to: the name of a called
function, a string, or a global variable.

Cada línea muestra la dirección, la palabra cruda, la instrucción y, cuando
se puede deducir, a qué se refiere: la función llamada, un texto o una
variable global.

Usage / Uso:
    python3 tools/disasm.py orig/G4AEE9/sys/main.dol OSInit         # by name
    python3 tools/disasm.py orig/G4AEE9/sys/main.dol 0x801B4A98     # by address
    python3 tools/disasm.py orig/G4AEE9/sys/main.dol __start --count 20
"""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from dol import Dol  # noqa: E402
from function_finder import find_small_data_bases  # noqa: E402
from ppc import Instruction, format_instruction  # noqa: E402
from symbols import SymbolTable  # noqa: E402

DEFAULT_INSTRUCTION_COUNT = 32
LONGEST_STRING_SHOWN = 60


class Annotator:
    """Explains what each instruction points at. Remembers the high half
    loaded by `lis` so the following addi/lwz can be resolved."""

    def __init__(self, dol: Dol, symbols: SymbolTable):
        self.dol = dol
        self.symbols = symbols
        self.small_data_bases = find_small_data_bases(dol)
        self.high_halves: dict[int, int] = {}

    def comment_for(self, instruction: Instruction) -> str:
        target = instruction.branch_target()
        if target is not None:
            return self.symbols.describe(target)

        if instruction.is_load_immediate_shifted:
            self.high_halves[instruction.rd] = instruction.unsigned_immediate << 16
            return ""

        if not (instruction.is_add_immediate_to_register or instruction.is_memory_access):
            return ""
        if instruction.ra in self.small_data_bases:
            base = self.small_data_bases[instruction.ra]
        elif instruction.ra in self.high_halves:
            base = self.high_halves.pop(instruction.ra)
        else:
            return ""
        address = (base + instruction.signed_immediate) & 0xFFFFFFFF

        text = self.dol.c_string(address, 80) if self.dol.section_at(address) else None
        if text and instruction.is_add_immediate_to_register:
            shown = text.replace("\n", "\\n")[:LONGEST_STRING_SHOWN]
            return f'{self.symbols.describe(address)} "{shown}"'
        return self.symbols.describe(address)


def main() -> int:
    parser = argparse.ArgumentParser(description="Disassemble a function of main.dol.")
    parser.add_argument("dol", type=Path)
    parser.add_argument("target", help="function name from symbols.txt, or a hex address")
    parser.add_argument("--count", type=int, help="number of instructions (default: the whole function)")
    parser.add_argument("--symbols", type=Path, default=Path("config/G4AEE9/symbols.txt"))
    arguments = parser.parse_args()

    dol = Dol(arguments.dol)
    symbols = SymbolTable.read(arguments.symbols)

    named = symbols.by_name(arguments.target)
    start = named.address if named else int(arguments.target, 16)

    count = arguments.count
    if count is None:
        symbol = symbols.by_address.get(start)
        count = symbol.size // 4 if symbol and symbol.kind == "function" else DEFAULT_INSTRUCTION_COUNT

    annotator = Annotator(dol, symbols)
    for index in range(count):
        address = start + 4 * index
        if not dol.is_code(address):
            break
        symbol = symbols.by_address.get(address)
        if symbol:
            print(f"\n{symbol.name}:")
        word = dol.u32(address)
        comment = annotator.comment_for(Instruction(address, word))
        line = f"  {address:08X}  {word:08X}  {format_instruction(word, address)}"
        print(f"{line:<60} # {comment}" if comment else line)
    return 0


if __name__ == "__main__":
    sys.exit(main())
