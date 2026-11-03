#!/usr/bin/env python3
"""Export the functions of symbols.txt as a symbol map for DolRecomp.

Exporta las funciones de symbols.txt como mapa de símbolos para DolRecomp.

DolRecomp (github.com/ExpansionPak/DolRecomp) accepts `--map file` and uses
every address in it as a function entry and partition point, and every name
for the generated C symbols. Its parser (src/analysis/symbol_map.c) accepts a
plain "address size name" line, which is what this writes.

Only functions are exported. Data objects that live inside text sections
(_rom_copy_info, strings) must stay out: DolRecomp would take their
addresses as code entry points.

DolRecomp acepta `--map archivo` y usa cada dirección como punto de entrada
y de partición, y cada nombre para los símbolos del C generado. Solo se
exportan funciones: los datos dentro de secciones de código se quedan fuera,
porque DolRecomp los tomaría como código.

Usage / Uso:
    python3 tools/export_dolrecomp_map.py [--symbols config/G4AEE9/symbols.txt] [--out build/G4AEE9.map]
    dolrecomp --gamecube --map build/G4AEE9.map orig/G4AEE9/sys/main.dol build
"""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from project import BUILD_DIRECTORY, GAME_ID, SYMBOLS_PATH  # noqa: E402
from symbols import SymbolTable  # noqa: E402

DEFAULT_MAP_PATH = BUILD_DIRECTORY / f"{GAME_ID}.map"


def dolrecomp_map_lines(table: SymbolTable) -> list[str]:
    return [
        f"{symbol.address:08X} {symbol.size:08X} {symbol.name}"
        for symbol in table
        if symbol.kind == "function"
    ]


def main() -> int:
    parser = argparse.ArgumentParser(description="Export symbols.txt functions as a DolRecomp symbol map.")
    parser.add_argument("--symbols", type=Path, default=SYMBOLS_PATH, help=f"default: {SYMBOLS_PATH}")
    parser.add_argument("--out", type=Path, default=DEFAULT_MAP_PATH, help=f"default: {DEFAULT_MAP_PATH}")
    arguments = parser.parse_args()

    table = SymbolTable.read(arguments.symbols)
    for line_number, line in table.unparsed_lines:
        print(f"warning: {arguments.symbols}:{line_number} could not be parsed: {line}")
    if not len(table):
        parser.error(f"no symbols found in {arguments.symbols}")

    lines = dolrecomp_map_lines(table)
    arguments.out.parent.mkdir(parents=True, exist_ok=True)
    arguments.out.write_text("\n".join(lines) + "\n")
    print(f"wrote {arguments.out}: {len(lines)} functions")
    return 0


if __name__ == "__main__":
    sys.exit(main())
