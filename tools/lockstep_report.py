#!/usr/bin/env python3
"""Sort the divergences of a lockstep-verifier run into causes.

Clasifica por causa las divergencias de una corrida del verificador lockstep.

ModernGekko can check the recompiled game against Dolphin's own CPU
interpreter while it plays. With STATICRECOMP_LOCKSTEP=1 in the environment,
the first time each block of recompiled code runs the verifier runs the same
block again on the interpreter, from the same registers and memory, and
compares the registers, the memory the block wrote and its hardware (MMIO)
writes. Every mismatch becomes a "[lockstep] DIVERGE" line in the log, and
when the game closes a "[lockstep] summary:" line gives the counters.

A long run prints hundreds of mismatches and most are not recompilation
bugs. This tool sorts them by cause and says which ones a person still has
to read:

  fpscr_result_flags  only the FPSCR differs, and only in the floating-point
                      result flags (FPRF, FR and FI), which every
                      floating-point instruction rewrites. The game only
                      saves and restores the FPSCR (docs/VALIDATION.md).
  end_inside_loop     the block ends inside a loop of its function. The
                      recompiled code leaves an inlined loop when a cycle
                      budget runs out, so it and the interpreter can stop at
                      the same address on different iterations of the same
                      loop. A limit of the verifier, not of the recompiler.
  control_flow        the interpreter never reached the address where the
                      recompiled block ended: it raised an exception first.
                      Needs a person.
  unexplained         everything else. Needs a person.

The first two causes are "known". The classification is conservative but it
is a heuristic: `end_inside_loop` says the divergence can be an iteration
artefact, not that it is one, so the report lists each distinct end address
once for a person to confirm.

A verifier built with patches/moderngekko/lockstep-verifier.patch does not
produce the last two artefacts to begin with: it lets the interpreter run on
past the end address and passes the block when the recompiled end state
equals the interpreter's state at any later visit. Each block that passed on
a later visit is printed as a "[lockstep] LATE-MATCH" line, and this tool
lists them apart from the divergences: they are the positive evidence that
a loop or recursion level was the only difference.

El ModernGekko puede comparar el juego recompilado con el intérprete de CPU
del propio Dolphin mientras se juega. Con STATICRECOMP_LOCKSTEP=1 en el
entorno, la primera vez que corre cada bloque recompilado el verificador
vuelve a correrlo en el intérprete, con los mismos registros y memoria, y
compara los registros, la memoria que escribió el bloque y sus escrituras al
hardware (MMIO). Cada diferencia queda en el log como una línea
"[lockstep] DIVERGE", y al cerrar el juego una línea "[lockstep] summary:"
da los contadores.

Una corrida larga imprime cientos de diferencias y casi todas no son errores
de la recompilación. Esta herramienta las clasifica por causa e indica cuáles
todavía tiene que leer una persona:

  fpscr_result_flags  solo difiere el FPSCR, y solo en las banderas de
                      resultado de punto flotante (FPRF, FR y FI), que
                      reescribe toda instrucción de punto flotante. El juego
                      solo guarda y restaura el FPSCR (docs/VALIDATION.md).
  end_inside_loop     el bloque termina dentro de un bucle de su función. El
                      código recompilado sale de un bucle en línea cuando se
                      agota un presupuesto de ciclos, así que él y el
                      intérprete pueden detenerse en la misma dirección en
                      iteraciones distintas del mismo bucle. Es un límite del
                      verificador, no del recompilador.
  control_flow        el intérprete nunca llegó a la dirección donde terminó
                      el bloque recompilado: antes lanzó una excepción. Lo
                      revisa una persona.
  unexplained         todo lo demás. Lo revisa una persona.

Las dos primeras causas son "conocidas". La clasificación es conservadora
pero es una heurística: `end_inside_loop` dice que la diferencia puede ser un
artefacto de iteración, no que lo sea, así que el reporte lista cada
dirección final distinta una sola vez para que una persona la confirme.

Un verificador compilado con patches/moderngekko/lockstep-verifier.patch
no produce esos dos últimos artefactos: deja que el intérprete siga más allá
de la dirección final y da el bloque por bueno si el estado final recompilado
es igual al del intérprete en cualquier visita posterior. Cada bloque que
pasó en una visita posterior sale como una línea "[lockstep] LATE-MATCH", y
esta herramienta las lista aparte de las divergencias: son la prueba
positiva de que lo único distinto era una iteración de bucle o un nivel de
recursión.

Usage / Uso:
    python3 tools/lockstep_report.py game.log
    python3 tools/lockstep_report.py game.log --show end_inside_loop --limit 100

Exit status / Código de salida:
    0  every divergence has a known cause (or there are none)
       toda divergencia tiene una causa conocida (o no hay ninguna)
    1  some divergences need a person, or the log is incomplete
       alguna divergencia necesita a una persona, o el log está incompleto
    2  the log has no verifier output
       el log no tiene salida del verificador
"""

import argparse
import re
import sys
from collections import Counter
from dataclasses import dataclass, field
from enum import Enum
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from dol import Dol  # noqa: E402
from function_finder import INSTRUCTION_SIZE  # noqa: E402
from ppc import Instruction  # noqa: E402
from project import MAIN_DOL_PATH, SYMBOLS_PATH  # noqa: E402
from symbols import SymbolTable  # noqa: E402
from textfiles import read_program_output  # noqa: E402

# -- What the verifier prints ------------------------------------------------
# (vendor/dolphin/Source/Core/Core/PowerPC/StaticRecomp/StaticRecompLockstep*.cpp
# in ModernGekko). In every comparison N is the recompiled ("native") code and
# I is the interpreter.

# [lockstep] DIVERGE #6 entry=0x801823D4 end=0x8018237C: r3:N=0x8030e6e8,I=0x8030e4bc fpscr:N=...
DIVERGENCE_LINE = re.compile(
    r"\[lockstep\] DIVERGE #(?P<number>\d+) entry=0x(?P<entry>[0-9A-Fa-f]+) "
    r"end=0x(?P<end>[0-9A-Fa-f]+):(?P<fields>.*)"
)
# [lockstep] LATE-MATCH #2 entry=0x800CED10 end=0x800CEA6C arrival=3 steps=412
LATE_MATCH_LINE = re.compile(
    r"\[lockstep\] LATE-MATCH #(?P<number>\d+) entry=0x(?P<entry>[0-9A-Fa-f]+) "
    r"end=0x(?P<end>[0-9A-Fa-f]+) arrival=(?P<arrival>\d+) steps=(?P<steps>\d+)"
)
# [lockstep] summary: checks=7593 reports=136 skipped_fallback=9 ...
SUMMARY_LINE = re.compile(r"\[lockstep\] summary:(?P<counters>.*)")
ENABLED_MARKER = "[lockstep] ENABLED"
COUNTER = re.compile(r"(?P<name>\w+)=(?P<value>-?\d+)")
# "N=0xa7b88104,I=0xa7b8c104": what a register comparison says about itself.
COMPARED_VALUES = re.compile(r"N=0x(?P<native>[0-9A-Fa-f]+),I=0x(?P<interpreter>[0-9A-Fa-f]+)")

FPSCR_FIELD = "fpscr"
CONTROL_FLOW_FIELD = "CTRLFLOW"  # printed instead of the comparisons when the interpreter got lost
# Printed bare (no colon) when the hardware reads of the two runs were not the same sequence.
MMIO_READ_SEQUENCE_FIELD = "mmio-read-seq-divergence"

# The FPSCR bits that every floating-point instruction overwrites with the
# outcome of that instruction (PowerPC 750CL manual, "Floating-Point Status
# and Control Register"): FPRF, the class and comparison code of the result
# (bits 12 to 16 counting from the least significant), FI, "fraction
# inexact", and FR, "fraction rounded". Bit masks, least significant bit = 0.
FPSCR_RESULT_CLASS_BITS = 0x0001F000
FPSCR_FRACTION_INEXACT_BIT = 0x00020000
FPSCR_FRACTION_ROUNDED_BIT = 0x00040000
FPSCR_RESULT_FLAGS = FPSCR_RESULT_CLASS_BITS | FPSCR_FRACTION_INEXACT_BIT | FPSCR_FRACTION_ROUNDED_BIT

# What the counters of the summary line mean, in the order they are shown.
SUMMARY_COUNTER_LABELS = (
    ("checks", "blocks checked against the interpreter"),
    ("reports", "divergences reported"),
    ("skipped_fallback", "blocks skipped (they ran through the instruction fallback)"),
    ("skipped_zero", "blocks skipped (zero cycles charged: nothing to line up)"),
    ("cap_hits", "blocks the interpreter could not finish within the step cap"),
    ("filtered", "divergences hidden by STATICRECOMP_LOCKSTEP_FILTER"),
    ("undercharges", "blocks that charged fewer cycles than the interpreter used"),
    ("late_matches", "blocks that matched the interpreter only at a later visit to their end address"),
)

DEFAULT_LINES_PER_CAUSE = 40
LONGEST_FIELDS_TEXT_SHOWN = 300
EXIT_ALL_EXPLAINED = 0
EXIT_NEEDS_A_PERSON = 1
EXIT_NOT_A_VERIFIER_LOG = 2


class Cause(Enum):
    FPSCR_RESULT_FLAGS = "fpscr_result_flags"
    END_INSIDE_LOOP = "end_inside_loop"
    CONTROL_FLOW = "control_flow"
    UNEXPLAINED = "unexplained"


KNOWN_CAUSES = (Cause.FPSCR_RESULT_FLAGS, Cause.END_INSIDE_LOOP)


@dataclass(frozen=True)
class Divergence:
    number: int  # the "#n" of the log line
    entry_address: int  # where the recompiled block starts
    end_address: int  # where it handed control back to the dispatcher
    fields: dict[str, str]  # name of each differing field -> what the log says, e.g. "N=0x1,I=0x2"
    fields_text: str  # the same, exactly as printed


@dataclass(frozen=True)
class LateMatch:
    """A block that passed, but only on a later visit of the interpreter to its end address.

    The recompiled block handed control back to the dispatcher at
    `end_address` after some number of loop iterations or recursion levels.
    The interpreter reached the same address once per iteration or level, and
    the recompiled end state equalled its state at the `arrival`-th visit.
    Visit 1 would be an ordinary pass and is not printed."""

    number: int  # the "#n" of the log line
    entry_address: int  # where the recompiled block starts
    end_address: int  # where it handed control back to the dispatcher
    arrival: int  # which visit of the interpreter to end_address matched (2 or more)
    steps: int  # instructions the interpreter ran to get there


@dataclass
class VerifierRun:
    """Everything a log says about one run of the verifier."""

    divergences: list[Divergence]
    # Counters of the final "summary" line. None when the log has none, which
    # means the game did not shut down cleanly and its counters are lost.
    summary: dict[str, int] | None
    verifier_was_enabled: bool
    late_matches: list[LateMatch] = field(default_factory=list)

    @property
    def log_is_complete(self) -> bool:
        """True when the log holds every divergence the verifier counted."""
        return self.summary is not None and self.summary.get("reports") == len(self.divergences)

    @property
    def has_verifier_output(self) -> bool:
        return (
            self.verifier_was_enabled
            or self.summary is not None
            or bool(self.divergences)
            or bool(self.late_matches)
        )


def parse_fields(fields_text: str) -> dict[str, str]:
    """Split what follows "end=0x...:" into {field name: comparison text}.

    Fields are separated by spaces and look like "r3:N=0x1,I=0x2",
    "mem[0x80001000]:N=0x00,I=0x01" or "CTRLFLOW:N_end=...". A few tokens are
    not fields of their own and are skipped: the "N@0x0c008000/4" details that
    follow an "mmio#" field. "mmio-read-seq-divergence" is the one field with
    no colon.
    """
    fields = {}
    for token in fields_text.split():
        if ":" in token:
            name, comparison = token.split(":", 1)
            fields[name] = comparison
        elif token == MMIO_READ_SEQUENCE_FIELD:
            fields[token] = ""
    return fields


def parse_log(text: str) -> VerifierRun:
    divergences = []
    late_matches = []
    summary = None
    for line in text.splitlines():
        divergence_match = DIVERGENCE_LINE.search(line)
        if divergence_match:
            fields_text = divergence_match["fields"].strip()
            divergences.append(
                Divergence(
                    number=int(divergence_match["number"]),
                    entry_address=int(divergence_match["entry"], 16),
                    end_address=int(divergence_match["end"], 16),
                    fields=parse_fields(fields_text),
                    fields_text=fields_text,
                )
            )
            continue
        late_match = LATE_MATCH_LINE.search(line)
        if late_match:
            late_matches.append(
                LateMatch(
                    number=int(late_match["number"]),
                    entry_address=int(late_match["entry"], 16),
                    end_address=int(late_match["end"], 16),
                    arrival=int(late_match["arrival"]),
                    steps=int(late_match["steps"]),
                )
            )
            continue
        summary_match = SUMMARY_LINE.search(line)
        if summary_match:
            summary = {m["name"]: int(m["value"]) for m in COUNTER.finditer(summary_match["counters"])}
    return VerifierRun(divergences, summary, ENABLED_MARKER in text, late_matches)


def only_result_flags_of_fpscr_differ(divergence: Divergence) -> bool:
    """True when the FPSCR is the only field that differs and the bits that
    differ are all result flags."""
    if set(divergence.fields) != {FPSCR_FIELD}:
        return False
    values = COMPARED_VALUES.fullmatch(divergence.fields[FPSCR_FIELD])
    if values is None:
        return False
    differing_bits = int(values["native"], 16) ^ int(values["interpreter"], 16)
    return differing_bits & ~FPSCR_RESULT_FLAGS == 0


def ends_inside_loop(dol: Dol, symbols: SymbolTable, end_address: int) -> bool:
    """True when `end_address` lies inside a loop of its function.

    That is, when an instruction at or after the address, in the same
    function, jumps back to the address or to something before it. Only
    direct jumps and conditional branches count (`b`, `bc`, `bdnz`): a call
    (`bl`) returns, and a jump out of the function is a tail call, not a loop.
    Returns False when no function symbol holds the address, because then the
    search cannot be bounded.
    """
    function = symbols.containing(end_address)
    if function is None or function.kind != "function":
        return False
    function_end = function.address + function.size
    for address in range(end_address, function_end, INSTRUCTION_SIZE):
        instruction = Instruction(address, dol.u32(address))
        if not (instruction.is_jump or instruction.is_conditional_branch):
            continue
        target = instruction.branch_target()
        if target is not None and function.address <= target <= end_address:
            return True
    return False


def classify(divergence: Divergence, dol: Dol, symbols: SymbolTable) -> Cause:
    if CONTROL_FLOW_FIELD in divergence.fields:
        return Cause.CONTROL_FLOW
    if only_result_flags_of_fpscr_differ(divergence):
        return Cause.FPSCR_RESULT_FLAGS
    if ends_inside_loop(dol, symbols, divergence.end_address):
        return Cause.END_INSIDE_LOOP
    return Cause.UNEXPLAINED


def classify_all(run: VerifierRun, dol: Dol, symbols: SymbolTable) -> list[tuple[Divergence, Cause]]:
    return [(divergence, classify(divergence, dol, symbols)) for divergence in run.divergences]


def where(symbols: SymbolTable, address: int) -> str:
    """`0x8016EF3C (OSInit+0x10)`, or the bare address when no symbol holds it."""
    name = symbols.describe(address)
    return f"0x{address:08X}" if name.startswith("0x") else f"0x{address:08X} ({name})"


def shortened(text: str) -> str:
    if len(text) <= LONGEST_FIELDS_TEXT_SHOWN:
        return text
    return f"{text[:LONGEST_FIELDS_TEXT_SHOWN]} ... ({len(text) - LONGEST_FIELDS_TEXT_SHOWN} more characters)"


def format_counters(summary: dict[str, int] | None) -> list[str]:
    if summary is None:
        return ["  no summary line: the game did not shut down cleanly, so the counters are lost"]
    lines = [f"  {summary[name]:>7}  {label}" for name, label in SUMMARY_COUNTER_LABELS if name in summary]
    if summary.get("undercharges"):
        lines.append(f"           (the largest shortfall was {summary.get('max_deficit', 0)} cycles)")
    return lines


def format_divergence(divergence: Divergence, symbols: SymbolTable) -> str:
    return (
        f"  #{divergence.number} entry={where(symbols, divergence.entry_address)} "
        f"end={where(symbols, divergence.end_address)}: {shortened(divergence.fields_text)}"
    )


def format_distinct_ends(divergences: list[Divergence], symbols: SymbolTable, limit: int) -> list[str]:
    """One line per distinct end address with how many divergences it has."""
    counts = Counter(divergence.end_address for divergence in divergences)
    lines = [f"  {count:>4} x end={where(symbols, address)}" for address, count in sorted(counts.items())]
    if len(lines) > limit:
        lines = [*lines[:limit], f"  ... and {len(lines) - limit} more end addresses"]
    return lines


def format_late_matches(late_matches: list[LateMatch], symbols: SymbolTable, limit: int) -> list[str]:
    """One line per distinct end address: how many blocks matched there and at which visits."""
    matches_by_end: dict[int, list[LateMatch]] = {}
    for late_match in late_matches:
        matches_by_end.setdefault(late_match.end_address, []).append(late_match)
    lines = []
    for address, matches in sorted(matches_by_end.items()):
        first_visit = min(match.arrival for match in matches)
        last_visit = max(match.arrival for match in matches)
        visits = f"{first_visit}" if first_visit == last_visit else f"{first_visit} to {last_visit}"
        lines.append(f"  {len(matches):>4} x end={where(symbols, address)} (matched at visit {visits})")
    if len(lines) > limit:
        lines = [*lines[:limit], f"  ... and {len(lines) - limit} more end addresses"]
    return lines


def format_report(
    log_name: str,
    run: VerifierRun,
    classified: list[tuple[Divergence, Cause]],
    symbols: SymbolTable,
    causes_to_show: tuple[Cause, ...] = (),
    lines_per_cause: int = DEFAULT_LINES_PER_CAUSE,
) -> str:
    """The text report for the divergences of `run` after `classify_all`."""
    by_cause = {
        cause: [d for d, divergence_cause in classified if divergence_cause is cause] for cause in Cause
    }
    lines = [f"Lockstep report for {log_name}", "", "Verifier counters:", *format_counters(run.summary), ""]

    lines.append(f"  {'cause':<20} {'divergences':>11} {'distinct end addresses':>24}")
    for cause in Cause:
        distinct_ends = len({d.end_address for d in by_cause[cause]})
        lines.append(f"  {cause.value:<20} {len(by_cause[cause]):>11} {distinct_ends:>24}")
    lines.append("")

    if run.late_matches:
        lines.append("Blocks that passed only at a later visit to their end address (not divergences):")
        lines += format_late_matches(run.late_matches, symbols, lines_per_cause)
        lines.append("")

    if by_cause[Cause.END_INSIDE_LOOP]:
        lines.append("Blocks that end inside a loop (confirm each end address once by hand):")
        lines += format_distinct_ends(by_cause[Cause.END_INSIDE_LOOP], symbols, lines_per_cause)
        lines.append("")

    needs_a_person = [cause for cause in Cause if cause not in KNOWN_CAUSES and by_cause[cause]]
    causes_to_list = needs_a_person + [cause for cause in causes_to_show if cause not in needs_a_person]
    for cause in causes_to_list:
        shown = by_cause[cause][:lines_per_cause]
        lines.append(f"{cause.value} ({len(by_cause[cause])}):")
        lines += [format_divergence(divergence, symbols) for divergence in shown]
        if len(by_cause[cause]) > len(shown):
            lines.append(f"  ... and {len(by_cause[cause]) - len(shown)} more (raise --limit to see them)")
        lines.append("")

    lines += [*format_verdict(run, needs_a_person), ""]
    return "\n".join(lines)


def format_verdict(run: VerifierRun, causes_needing_a_person: list[Cause]) -> list[str]:
    lines = []
    if run.summary is not None and not run.log_is_complete:
        lines.append(
            f"WARNING: the verifier counted {run.summary.get('reports')} divergences but the log "
            f"holds {len(run.divergences)} lines; the report may be missing some."
        )
    if run.summary is None:
        lines.append("WARNING: no summary line, so the log may be cut short.")
    if causes_needing_a_person:
        lines.append("Verdict: some divergences need a person to read them (listed above).")
    elif not run.divergences:
        lines.append(
            "Verdict: no divergences; the recompiled code matched the interpreter everywhere it was checked."
        )
    else:
        lines.append("Verdict: every divergence has a known cause.")
    return lines


def exit_status(run: VerifierRun, classified: list[tuple[Divergence, Cause]]) -> int:
    if not run.has_verifier_output:
        return EXIT_NOT_A_VERIFIER_LOG
    any_needs_a_person = any(cause not in KNOWN_CAUSES for _, cause in classified)
    return EXIT_NEEDS_A_PERSON if any_needs_a_person or not run.log_is_complete else EXIT_ALL_EXPLAINED


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(
        description="Sort the divergences in a lockstep-verifier log by cause.",
    )
    parser.add_argument("log", type=Path, help="the game's log from a run with STATICRECOMP_LOCKSTEP=1")
    parser.add_argument("--dol", type=Path, default=MAIN_DOL_PATH, help=f"default: {MAIN_DOL_PATH}")
    parser.add_argument("--symbols", type=Path, default=SYMBOLS_PATH, help=f"default: {SYMBOLS_PATH}")
    parser.add_argument(
        "--show",
        action="append",
        choices=[cause.value for cause in Cause],
        default=[],
        help="also print the full lines of this cause (needs-a-person causes are always printed)",
    )
    parser.add_argument(
        "--limit",
        type=int,
        default=DEFAULT_LINES_PER_CAUSE,
        help=f"most lines printed per cause (default: {DEFAULT_LINES_PER_CAUSE})",
    )
    arguments = parser.parse_args(argv)

    if not arguments.log.is_file():
        parser.error(f"{arguments.log} not found")
    if not arguments.dol.is_file():
        parser.error(f"{arguments.dol} not found; extract your disc first (tools/extract_disc.py)")

    run = parse_log(read_program_output(arguments.log))
    if not run.has_verifier_output:
        print(
            f"{arguments.log} has no verifier output; run the game with STATICRECOMP_LOCKSTEP=1 "
            "(see docs/VALIDATION.md).",
            file=sys.stderr,
        )
        return EXIT_NOT_A_VERIFIER_LOG

    dol = Dol(arguments.dol)
    symbols = SymbolTable.read(arguments.symbols)
    classified = classify_all(run, dol, symbols)
    causes_to_show = tuple(Cause(name) for name in arguments.show)
    print(format_report(arguments.log.name, run, classified, symbols, causes_to_show, arguments.limit))
    return exit_status(run, classified)


if __name__ == "__main__":
    sys.exit(main())
