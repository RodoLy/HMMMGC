#!/usr/bin/env python3
"""Say how much of the game the lockstep verifier really compared.

Dice qué parte del juego comparó de verdad el verificador lockstep.

"No divergences" only means something next to how much code was checked. The
verifier compares a block of recompiled code with Dolphin's interpreter the
first time the game runs it, so it only ever checks the code that the run
reached. Started with STATICRECOMP_LOCKSTEP_COVERAGE_FILE=coverage.txt (see
docs/VALIDATION.md), a verifier built with
patches/moderngekko/lockstep-any-arrival.patch writes, when the game closes,
the address of every instruction it compared and found equal. This tool turns
that file into numbers:

  * how many of the instructions in the game's functions were compared, and
    in how many functions at least one was;
  * which instruction forms (`add.`, `bl`, `ps_madd`, ...) the game's code
    contains and which of them the run never compared, because those are the
    parts of the recompiler that the run could not have caught a bug in;
  * the biggest functions the run never entered.

"Sin divergencias" solo significa algo junto a cuánto código se comparó. El
verificador compara un bloque de código recompilado con el intérprete de
Dolphin la primera vez que el juego lo ejecuta, así que solo revisa el código
al que llegó la corrida. Iniciado con STATICRECOMP_LOCKSTEP_COVERAGE_FILE=
coverage.txt (ver docs/VALIDATION.md), un verificador compilado con
patches/moderngekko/lockstep-any-arrival.patch escribe, al cerrar el juego,
la dirección de cada instrucción que comparó y encontró igual. Esta
herramienta convierte ese archivo en números:

  * cuántas de las instrucciones de las funciones del juego se compararon, y
    en cuántas funciones se comparó al menos una;
  * qué formas de instrucción (`add.`, `bl`, `ps_madd`, ...) tiene el código
    del juego y cuáles nunca se compararon en la corrida: son las partes del
    recompilador en las que la corrida no habría podido encontrar un error;
  * las funciones más grandes a las que la corrida nunca entró.

Several coverage files can be given; their addresses are merged, so runs
that visit different parts of the game add up.

Se pueden dar varios archivos de cobertura; se juntan sus direcciones, así
que corridas que visitan partes distintas del juego se suman.

Usage / Uso:
    python3 tools/lockstep_coverage.py coverage.txt
    python3 tools/lockstep_coverage.py run1.txt run2.txt --forms --limit 30
"""

import argparse
import re
import sys
from collections import defaultdict
from collections.abc import Iterable
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from dol import Dol  # noqa: E402
from function_finder import INSTRUCTION_SIZE  # noqa: E402
from ppc_mnemonics import ILLEGAL_FORM, form_name  # noqa: E402
from project import MAIN_DOL_PATH, SYMBOLS_PATH  # noqa: E402
from symbols import Symbol, SymbolTable  # noqa: E402

# One verified instruction per line, as the verifier writes it: 0x80003100
COVERAGE_LINE = re.compile(r"0x(?P<address>[0-9A-Fa-f]{8})")

DEFAULT_LINES_PER_LIST = 15
# A form compared at this few places or fewer is listed as thinly covered. A recompiler
# bug in a form tends to show on most operands, so one or two sites is thin but not nothing.
THINLY_COVERED_SITES = 2


@dataclass
class FormCoverage:
    sites: int = 0  # places in the game's functions that use this form
    verified_sites: int = 0  # how many of them the verifier compared


@dataclass
class Coverage:
    instructions: int = 0  # instruction words in the game's functions (padding and data excluded)
    verified_instructions: int = 0
    functions: int = 0
    functions_touched: int = 0  # functions in which at least one instruction was compared
    forms: dict[str, FormCoverage] = field(default_factory=lambda: defaultdict(FormCoverage))
    untouched_functions: list[Symbol] = field(default_factory=list)  # biggest first
    # Compared addresses that no function symbol holds: code the analysis did not find.
    verified_outside_functions: int = 0


def read_verified_addresses(texts: Iterable[str]) -> tuple[set[int], int]:
    """The addresses listed in coverage files, and how many non-empty lines were not addresses."""
    addresses = set()
    unreadable_lines = 0
    for text in texts:
        for line in text.splitlines():
            if not line.strip():
                continue
            match = COVERAGE_LINE.fullmatch(line.strip())
            if match is None:
                unreadable_lines += 1
            else:
                addresses.add(int(match["address"], 16))
    return addresses, unreadable_lines


def measure(dol: Dol, symbols: SymbolTable, verified_addresses: set[int]) -> Coverage:
    """Count, over every function symbol, the instructions the verifier compared."""
    coverage = Coverage()
    addresses_in_functions = set()
    for function in symbols:
        if function.kind != "function" or function.size == 0 or not dol.is_mapped(function.address):
            continue
        coverage.functions += 1
        function_was_touched = False
        for address in range(function.address, function.address + function.size, INSTRUCTION_SIZE):
            form = form_name(dol.u32(address))
            was_verified = address in verified_addresses
            if form == ILLEGAL_FORM and not was_verified:
                continue  # padding, or data stored in the code
            addresses_in_functions.add(address)
            coverage.instructions += 1
            coverage.forms[form].sites += 1
            if was_verified:
                function_was_touched = True
                coverage.verified_instructions += 1
                coverage.forms[form].verified_sites += 1
        if function_was_touched:
            coverage.functions_touched += 1
        else:
            coverage.untouched_functions.append(function)
    coverage.untouched_functions.sort(key=lambda function: (-function.size, function.address))
    coverage.verified_outside_functions = len(verified_addresses - addresses_in_functions)
    return coverage


def percent(part: int, whole: int) -> str:
    return f"{100 * part / whole:.1f}%" if whole else "n/a"


def format_forms_table(forms: dict[str, FormCoverage], names: list[str]) -> list[str]:
    lines = [f"  {'form':<12} {'sites in the game':>18} {'sites compared':>15}"]
    lines += [f"  {name:<12} {forms[name].sites:>18,} {forms[name].verified_sites:>15,}" for name in names]
    return lines


def format_report(
    coverage_names: list[str],
    coverage: Coverage,
    verified_address_count: int,
    show_all_forms: bool = False,
    lines_per_list: int = DEFAULT_LINES_PER_LIST,
) -> str:
    """The text report for one measurement."""
    forms = coverage.forms
    never_compared = sorted(
        (name for name, form in forms.items() if form.verified_sites == 0), key=lambda n: -forms[n].sites
    )
    thinly_compared = sorted(
        (name for name, form in forms.items() if 0 < form.verified_sites <= THINLY_COVERED_SITES),
        key=lambda name: (forms[name].verified_sites, -forms[name].sites),
    )

    lines = [f"Lockstep coverage of {', '.join(coverage_names)}", ""]
    lines.append("Instructions in the game's functions that the verifier compared:")
    lines.append(
        f"  {coverage.verified_instructions:>9,} of {coverage.instructions:,}"
        f"  ({percent(coverage.verified_instructions, coverage.instructions)})"
    )
    lines.append("Functions in which at least one instruction was compared:")
    lines.append(
        f"  {coverage.functions_touched:>9,} of {coverage.functions:,}"
        f"  ({percent(coverage.functions_touched, coverage.functions)})"
    )
    if coverage.verified_outside_functions:
        outside = coverage.verified_outside_functions
        lines.append(
            f"  ({outside:,} compared instructions are in no function of symbols.txt: code not found)"
        )
    lines.append("")

    lines.append(f"Instruction forms in the game's code: {len(forms)}")
    lines.append(f"  compared at least once:  {len(forms) - len(never_compared)}")
    lines.append(f"  never compared:          {len(never_compared)}")
    lines.append(f"  compared at {THINLY_COVERED_SITES} sites or fewer: {len(thinly_compared)}")
    lines.append("")
    if never_compared:
        lines.append("Forms the run never compared (a bug in these could not have shown up):")
        lines += format_forms_table(
            forms, never_compared if show_all_forms else never_compared[:lines_per_list]
        )
        if not show_all_forms and len(never_compared) > lines_per_list:
            lines.append(f"  ... and {len(never_compared) - lines_per_list} more (--forms lists them all)")
        lines.append("")
    if thinly_compared:
        lines.append(f"Forms compared at {THINLY_COVERED_SITES} sites or fewer:")
        lines += format_forms_table(
            forms, thinly_compared if show_all_forms else thinly_compared[:lines_per_list]
        )
        if not show_all_forms and len(thinly_compared) > lines_per_list:
            lines.append(f"  ... and {len(thinly_compared) - lines_per_list} more (--forms lists them all)")
        lines.append("")
    if show_all_forms:
        lines.append("Every form:")
        lines += format_forms_table(forms, sorted(forms, key=lambda name: -forms[name].sites))
        lines.append("")

    lines.append("Biggest functions the run never entered:")
    for function in coverage.untouched_functions[:lines_per_list]:
        size = function.size // INSTRUCTION_SIZE
        lines.append(f"  {size:>6,} instructions  {function.name} (0x{function.address:08X})")
    if len(coverage.untouched_functions) > lines_per_list:
        lines.append(f"  ... and {len(coverage.untouched_functions) - lines_per_list:,} more functions")
    lines.append(f"({verified_address_count:,} distinct compared instructions in the coverage files)")
    return "\n".join(lines) + "\n"


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Say how much of the game the lockstep verifier compared.",
    )
    parser.add_argument(
        "coverage",
        type=Path,
        nargs="+",
        help="file written by a run with STATICRECOMP_LOCKSTEP_COVERAGE_FILE (several are merged)",
    )
    parser.add_argument("--dol", type=Path, default=MAIN_DOL_PATH, help=f"default: {MAIN_DOL_PATH}")
    parser.add_argument("--symbols", type=Path, default=SYMBOLS_PATH, help=f"default: {SYMBOLS_PATH}")
    parser.add_argument(
        "--forms", action="store_true", help="list every instruction form, not just the thin ones"
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=DEFAULT_LINES_PER_LIST,
        help=f"most lines printed per list (default: {DEFAULT_LINES_PER_LIST})",
    )
    arguments = parser.parse_args(argv)

    for path in (*arguments.coverage, arguments.dol, arguments.symbols):
        if not path.is_file():
            parser.error(f"{path} not found")

    verified_addresses, unreadable_lines = read_verified_addresses(
        path.read_text(errors="replace") for path in arguments.coverage
    )
    if not verified_addresses:
        print("the coverage files hold no addresses; was the game closed cleanly?", file=sys.stderr)
        return 1
    if unreadable_lines:
        print(f"warning: {unreadable_lines} lines of the coverage files are not addresses", file=sys.stderr)

    coverage = measure(Dol(arguments.dol), SymbolTable.read(arguments.symbols), verified_addresses)
    print(
        format_report(
            [path.name for path in arguments.coverage],
            coverage,
            len(verified_addresses),
            arguments.forms,
            arguments.limit,
        ),
        end="",
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
