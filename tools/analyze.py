#!/usr/bin/env python3
"""Build config/G4AEE9/symbols.txt from main.dol.

Genera config/G4AEE9/symbols.txt a partir de main.dol.

Runs the function finder, names what can be named with evidence (the
Nintendo SDK init chain), keeps every name a person already gave in the
existing symbols.txt, prints the checks that prove the map is consistent,
and writes the result.

Ejecuta el buscador de funciones, nombra lo que se puede nombrar con
evidencia (la cadena de init del SDK de Nintendo), conserva los nombres que
alguien ya puso a mano en symbols.txt, imprime las comprobaciones que
demuestran que el mapa es consistente y escribe el resultado.

Usage / Uso:
    python3 tools/analyze.py orig/G4AEE9/sys/main.dol [--out config/G4AEE9]
"""

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from dol import Dol  # noqa: E402
from function_finder import FunctionFinder, functions_by_source_file  # noqa: E402
from sdk_names import SdkNames, name_sdk_functions  # noqa: E402
from symbols import Symbol, SymbolTable, section_name_for  # noqa: E402


def build_symbol_table(dol: Dol, finder: FunctionFinder, sdk_names: SdkNames) -> SymbolTable:
    table = SymbolTable()
    for start, size in finder.function_sizes.items():
        name = sdk_names.functions.get(start, f"fn_{start:08X}")
        table.add(Symbol(name, section_name_for(dol, start), start, "function", size))
    for start, (size, name) in finder.data_in_code.items():
        table.add(Symbol(name, section_name_for(dol, start), start, "object", size))
    for address, name in sdk_names.variables.items():
        table.add(Symbol(name, section_name_for(dol, address), address, "object", 4))
    for table_address, case_targets in finder.jump_tables.items():
        table.add(
            Symbol(
                f"jumptable_{table_address:08X}",
                section_name_for(dol, table_address),
                table_address,
                "object",
                4 * len(case_targets),
                scope="local",
            )
        )
    return table


def print_checks(dol: Dol, finder: FunctionFinder) -> bool:
    """Print the consistency checks. True when all of them pass."""
    disagreements = finder.compare_with_exception_index()
    overlaps = finder.overlapping_functions()
    checked = len(finder.exception_index_sizes)
    covered_bytes = sum(finder.function_sizes.values())
    code_bytes = sum(section.size for section in dol.text_sections())

    print(f"functions found:        {len(finder.function_sizes)}")
    print(f"  listed in extabindex: {checked}")
    print(f"  called with bl:       {len(finder.call_targets)}")
    print(f"switch jump tables:     {len(finder.jump_tables)} (rejected: {len(finder.rejected_jump_tables)})")
    print(f"data inside code:       {len(finder.data_in_code)}")
    print(f"code covered:           0x{covered_bytes:X} of 0x{code_bytes:X} bytes (the rest is padding)")
    print(f"overlapping functions:  {len(overlaps)}")
    for start, following in overlaps[:10]:
        print(f"  0x{start:08X} (size 0x{finder.function_sizes[start]:X}) overlaps 0x{following:08X}")
    print(f"walker vs extabindex:   {checked - len(disagreements)}/{checked} agree")
    for start, real_size, walked_size in disagreements[:15]:
        print(f"  0x{start:08X}: real size 0x{real_size:X}, walker 0x{walked_size:X}")
    for bctr_address in finder.rejected_jump_tables[:10]:
        print(f"  rejected jump table at bctr 0x{bctr_address:08X}")
    return not disagreements and not overlaps and not finder.rejected_jump_tables


def print_sections(finder: FunctionFinder) -> None:
    """Exact section sizes from _rom_copy_info / _bss_init_info (the DOL
    header rounds them up)."""
    print("\nsections (exact sizes from the startup tables):")
    for _source, destination, size in finder.rom_copy_entries():
        print(f"  0x{destination:08X}  size 0x{size:06X}")
    for address, size in finder.bss_init_entries():
        print(f"  0x{address:08X}  size 0x{size:06X}  (zeroed at boot)")


def main() -> int:
    parser = argparse.ArgumentParser(description="Build symbols.txt from main.dol.")
    parser.add_argument("dol", type=Path, help="path to the extracted sys/main.dol")
    parser.add_argument(
        "--out",
        type=Path,
        default=Path("config/G4AEE9"),
        help="folder holding symbols.txt (default: config/G4AEE9)",
    )
    arguments = parser.parse_args()

    dol = Dol(arguments.dol)
    finder = FunctionFinder(dol)
    finder.run()
    all_checks_pass = print_checks(dol, finder)
    print_sections(finder)

    sdk_names = name_sdk_functions(dol, finder)
    print(f"\nSDK libraries identified: {', '.join(library.name for library in sdk_names.libraries)}")

    print("\nsource files named by asserts:")
    for file_name, functions in sorted(
        functions_by_source_file(dol, finder).items(), key=lambda item: item[1][0]
    ):
        print(f"  {file_name:22} {len(functions):3} functions  0x{functions[0]:08X} - 0x{functions[-1]:08X}")

    symbols_path = arguments.out / "symbols.txt"
    table = build_symbol_table(dol, finder, sdk_names)
    for warning in table.keep_names_from(SymbolTable.read(symbols_path)):
        print(f"warning: {warning}")
    named = sum(1 for symbol in table if symbol.kind == "function" and not symbol.has_generated_name)

    arguments.out.mkdir(parents=True, exist_ok=True)
    table.write(symbols_path)
    print(f"\nwrote {symbols_path}: {len(table)} symbols, {named} named functions")
    return 0 if all_checks_pass else 1


if __name__ == "__main__":
    sys.exit(main())
