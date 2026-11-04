#!/usr/bin/env python3
"""Check the recompiler against Dolphin's interpreter on every instruction form the game uses.

Comprueba el recompilador contra el intérprete de Dolphin en cada forma de instrucción que
usa el juego.

One command does what docs/VALIDATION.md describes. For each seed it

  1. builds a test program (a DOL) out of the game's own instructions (isa_conformance.py);
  2. recompiles it with `moderngekko-port`, the recompiler the game itself goes through;
  3. runs it with `moderngekko-run` and the lockstep verifier on; the program writes a marker
     into the guest's memory when every case has run, and this script waits for it;
  4. reads the verifier's log and says, form by form, how many cases were compared.

Nothing of the game's code runs: the test program takes the place of `main.dol`. The disc
folder only supplies the three small files of `sys/` that ModernGekko needs to recognise the
game. The runner must be built with patches/moderngekko/lockstep-verifier.patch.

Un comando hace lo que describe docs/VALIDATION.md. Por cada semilla (1) arma un programa de
prueba (un DOL) con las instrucciones del propio juego, (2) lo recompila con
`moderngekko-port`, el mismo recompilador por el que pasa el juego, (3) lo ejecuta con
`moderngekko-run` y el verificador lockstep activo (el programa escribe una marca en la
memoria del invitado al terminar todos los casos, y este script la espera) y (4) lee el log
del verificador y dice, forma por forma, cuántos casos se compararon.

Del juego no se ejecuta nada: el programa de prueba ocupa el lugar de `main.dol`. La carpeta
del disco solo aporta los tres archivos pequeños de `sys/` con los que ModernGekko reconoce el
juego. El runner debe estar compilado con patches/moderngekko/lockstep-verifier.patch.

Usage / Uso:
    python3 tools/run_isa_conformance.py                       # one seed, the default sample
    python3 tools/run_isa_conformance.py --seeds 1 2 3 4 5     # more random states
    python3 tools/run_isa_conformance.py --inject-fault lwz    # self-check: must report 1 case
"""

import argparse
import contextlib
import os
import shutil
import subprocess
import sys
import time
from collections.abc import Iterator
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from isa_conformance import (  # noqa: E402
    DONE_ADDRESS,
    DONE_VALUE,
    Candidate,
    Case,
    Evaluation,
    FormResult,
    build_program,
    collect_candidates,
    evaluate,
    format_cases,
)
from moderngekko_automation import AutomationClient  # noqa: E402

from dol import Dol  # noqa: E402
from project import BUILD_DIRECTORY, DISC_DIRECTORY, GAME_ID, SYMBOLS_PATH  # noqa: E402
from symbols import SymbolTable  # noqa: E402

# The small files of a disc's sys/ folder that ModernGekko reads to recognise the game.
DISC_FILES_TO_COPY = ("apploader.img", "bi2.bin", "boot.bin")

# The verifier options that docs/VALIDATION.md describes: compare at any arrival of a loop
# header, and ignore the FPSCR result flags.
VERIFIER_ENVIRONMENT = {
    "STATICRECOMP_LOCKSTEP": "1",
    "STATICRECOMP_LOCKSTEP_ANY_ARRIVAL": "1",
    "STATICRECOMP_LOCKSTEP_IGNORE_FPSCR_FLAGS": "1",
}

DEFAULT_TEMPLATE_DIRECTORY = Path.home() / "ModernGekko-Template"
DEFAULT_WORK_DIRECTORY = BUILD_DIRECTORY / "isa_conformance"
SCREEN = "640x480x24"  # the virtual screen when no display exists; nothing is drawn on it
POLL_SECONDS = 0.5
SHUTDOWN_TIMEOUT_SECONDS = 120.0

# -- ModernGekko -------------------------------------------------------------------------------


class SetupError(Exception):
    """Something this script needs is missing; the message says what to do."""


@dataclass(frozen=True)
class ModernGekkoTools:
    port: Path  # recompiles a game
    runner: Path  # runs it

    @classmethod
    def in_template(cls, template_directory: Path) -> "ModernGekkoTools":
        build_directory = template_directory / "lib" / "ModernGekko" / "build"
        tools = cls(build_directory / "moderngekko-port", build_directory / "moderngekko-run")
        for tool in (tools.port, tools.runner):
            if not tool.is_file():
                raise SetupError(f"{tool} not found. Build ModernGekko first: see docs/BUILDING.md.")
        return tools


def make_game_root(disc_directory: Path, program: Path, game_root: Path) -> None:
    """A game folder that ModernGekko takes for the real one, with the test program as main.dol."""
    sys_directory = game_root / "sys"
    sys_directory.mkdir(parents=True, exist_ok=True)
    (game_root / "files").mkdir(exist_ok=True)
    for name in DISC_FILES_TO_COPY:
        source = disc_directory / "sys" / name
        if not source.is_file():
            raise SetupError(f"{source} not found. Extract the disc first: tools/extract_disc.py.")
        shutil.copy2(source, sys_directory / name)
    shutil.copy2(program, sys_directory / "main.dol")


def recompile(tools: ModernGekkoTools, game_root: Path, output_directory: Path, toolchain: str) -> Path:
    """Run `moderngekko-port build` and return the recompiled module."""
    shutil.rmtree(output_directory, ignore_errors=True)
    log_path = output_directory.parent / f"{output_directory.name}.log"
    command = [
        str(tools.port),
        "build",
        str(game_root),
        "--toolchain",
        toolchain,
        "--output",
        str(output_directory),
    ]
    with log_path.open("w") as log:
        completed = subprocess.run(command, stdout=log, stderr=subprocess.STDOUT, check=False)
    if completed.returncode != 0:
        tail = "\n".join(log_path.read_text(errors="replace").splitlines()[-15:])
        raise SetupError(f"moderngekko-port build failed (full log: {log_path}):\n{tail}")
    modules = sorted((output_directory / GAME_ID).glob("*/g*_recomp.*"))
    modules = [module for module in modules if module.suffix in (".so", ".dll", ".dylib")]
    if not modules:
        raise SetupError(f"moderngekko-port build left no module under {output_directory}")
    return modules[0]


# -- Running the program -----------------------------------------------------------------------


@contextlib.contextmanager
def display_for_the_runner() -> Iterator[dict[str, str]]:
    """Environment variables that give the runner a screen: the user's, or a virtual one.

    ModernGekko opens a window even when it draws nothing, so a machine without a screen
    (a server, a container) needs a virtual one.
    """
    if os.environ.get("DISPLAY") or os.environ.get("WAYLAND_DISPLAY"):
        yield {}
        return
    if shutil.which("Xvfb") is None:
        raise SetupError("There is no display and Xvfb is not installed: sudo apt install xvfb")
    read_end, write_end = os.pipe()
    server = subprocess.Popen(
        ["Xvfb", "-displayfd", str(write_end), "-screen", "0", SCREEN, "-nolisten", "tcp"],
        pass_fds=[write_end],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )
    os.close(write_end)
    try:
        with os.fdopen(read_end) as announcement:
            display_number = announcement.readline().strip()  # Xvfb says which number it took
        if not display_number:
            raise SetupError("Xvfb did not start")
        yield {"DISPLAY": f":{display_number}"}
    finally:
        server.terminate()
        server.wait()


def wait_until_running(client: AutomationClient, runner: subprocess.Popen, timeout_seconds: float) -> None:
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        if runner.poll() is not None:
            raise SetupError(f"moderngekko-run exited with code {runner.returncode} before it started")
        if client.status().get("state") == "running":
            return
        time.sleep(POLL_SECONDS)
    raise SetupError("moderngekko-run did not start in time")


def wait_for_the_marker(
    client: AutomationClient, runner: subprocess.Popen, scratch_file: Path, timeout_seconds: float
) -> bool:
    """Wait until the test program writes DONE_VALUE into the guest's memory."""
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline and runner.poll() is None:
        if client.read_memory(DONE_ADDRESS, 4, scratch_file) == DONE_VALUE.to_bytes(4, "big"):
            return True
        time.sleep(POLL_SECONDS)
    return False


@dataclass(frozen=True)
class RunFiles:
    log: Path
    coverage: Path


def run_under_the_verifier(
    tools: ModernGekkoTools,
    game_root: Path,
    module: Path,
    work_directory: Path,
    timeout_seconds: float,
    extra_environment: dict[str, str],
) -> RunFiles:
    """Run the test program to its end, close the game normally and return the verifier's files.

    The game has to close normally: the verifier writes its summary and the coverage file
    when it shuts down.
    """
    shutil.rmtree(work_directory, ignore_errors=True)
    automation_directory = work_directory / "automation"
    (automation_directory / "commands").mkdir(parents=True)
    (work_directory / "user").mkdir()
    files = RunFiles(work_directory / "game.log", work_directory / "coverage.txt")
    client = AutomationClient(
        automation_directory, command_timeout_seconds=timeout_seconds, poll_seconds=POLL_SECONDS
    )
    with display_for_the_runner() as display:
        environment = {
            **os.environ,
            **display,
            **VERIFIER_ENVIRONMENT,
            "STATICRECOMP_LOCKSTEP_COVERAGE_FILE": str(files.coverage),
            **extra_environment,
        }
        command = [
            str(tools.runner),
            "--game", str(game_root),
            "--module", str(module),
            "--graphics", "Null",  # nothing needs drawing
            "--mute",
            "--automation-dir", str(automation_directory),
            "--user-dir", str(work_directory / "user"),
        ]  # fmt: skip
        with files.log.open("w") as log:
            runner = subprocess.Popen(command, env=environment, stdout=log, stderr=subprocess.STDOUT)
            try:
                wait_until_running(client, runner, timeout_seconds)
                finished = wait_for_the_marker(client, runner, work_directory / "marker.bin", timeout_seconds)
                if not finished:
                    raise SetupError(f"the test program did not finish; see {files.log}")
                client.stop()
                try:
                    runner.wait(timeout=SHUTDOWN_TIMEOUT_SECONDS)
                except subprocess.TimeoutExpired as not_closing:
                    raise SetupError("moderngekko-run did not close after `stop`") from not_closing
            finally:
                if runner.poll() is None:
                    runner.terminate()
                    runner.wait()
    return files


# -- One seed, and all of them -----------------------------------------------------------------


@dataclass
class SeedOutcome:
    seed: int
    cases: list[Case]
    evaluation: Evaluation


def run_seed(
    seed: int,
    arguments: argparse.Namespace,
    tools: ModernGekkoTools,
    candidates: dict[str, list[Candidate]],
) -> SeedOutcome:
    seed_directory = arguments.work_directory / f"seed_{seed}"
    seed_directory.mkdir(parents=True, exist_ok=True)
    program = build_program(candidates, seed, arguments.words_per_form, arguments.group_size)
    program_path = program.write(seed_directory / "program.dol")
    (seed_directory / "cases.tsv").write_text(format_cases(program.cases))
    print(f"seed {seed}: {len(program.cases):,} cases of {len({case.form for case in program.cases})} forms")

    game_root = seed_directory / "game"
    make_game_root(arguments.disc, program_path, game_root)
    print(f"seed {seed}: recompiling the test program ...", flush=True)
    module = recompile(tools, game_root, seed_directory / "module", arguments.toolchain)

    extra_environment = {}
    if arguments.inject_fault:
        faulty_case = next(case for case in program.cases if case.form == arguments.inject_fault)
        extra_environment["STATICRECOMP_LOCKSTEP_INJECT_FAULT"] = f"0x{faulty_case.address:08X}"
    print(f"seed {seed}: running under the lockstep verifier ...", flush=True)
    files = run_under_the_verifier(
        tools, game_root, module, seed_directory / "run", arguments.timeout, extra_environment
    )
    evaluation = evaluate(
        program.cases,
        files.log.read_text(errors="replace"),
        files.coverage.read_text() if files.coverage.is_file() else "",
    )
    (seed_directory / "report.txt").write_text(evaluation.report)
    return SeedOutcome(seed, program.cases, evaluation)


def format_summary(outcomes: list[SeedOutcome]) -> str:
    """The numbers of every seed added up, and the list of divergences."""
    total = FormResult()
    form_totals: dict[str, FormResult] = {}
    for outcome in outcomes:
        for form, result in outcome.evaluation.results.items():
            total.add(result)
            form_totals.setdefault(form, FormResult()).add(result)
    lines = [
        f"{len(outcomes)} seed(s): {total.cases:,} cases of {len(form_totals)} forms",
        f"  compared and equal:  {total.compared:,}",
        f"  diverged:            {total.diverged:,}",
        f"  not compared:        {total.not_compared:,}",
    ]
    never_compared = sorted(form for form, result in form_totals.items() if result.compared == 0)
    if never_compared:
        lines.append(f"Forms with no case compared and equal in any seed: {' '.join(never_compared)}")
    for outcome in outcomes:
        lines += [f"seed {outcome.seed}: {line}" for line in outcome.evaluation.divergence_lines]
    return "\n".join(lines) + "\n"


# -- Command line ------------------------------------------------------------------------------


def parse_arguments(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument(
        "--template",
        type=Path,
        default=Path(os.environ.get("MODERNGEKKO_TEMPLATE", DEFAULT_TEMPLATE_DIRECTORY)),
        help="where ModernGekko-Template is built (default: $MODERNGEKKO_TEMPLATE or ~/ModernGekko-Template)",
    )
    parser.add_argument(
        "--disc", type=Path, default=DISC_DIRECTORY, help=f"the extracted disc (default: {DISC_DIRECTORY})"
    )
    parser.add_argument("--symbols", type=Path, default=SYMBOLS_PATH, help="the game's function map")
    parser.add_argument(
        "--work-directory",
        type=Path,
        default=DEFAULT_WORK_DIRECTORY,
        help=f"where the programs, modules and logs go (default: {DEFAULT_WORK_DIRECTORY})",
    )
    parser.add_argument(
        "--toolchain",
        default="auto",
        choices=["auto", "gcc", "clang", "msvc"],
        help="the C compiler for the module (default: auto)",
    )
    parser.add_argument(
        "--seeds",
        type=int,
        nargs="+",
        default=[1],
        help="one run per seed; more seeds, more random states (default: 1)",
    )
    parser.add_argument(
        "--words-per-form", type=int, default=24, help="different words tried per form (default: 24)"
    )
    parser.add_argument(
        "--group-size", type=int, default=4, help="cases that share one machine state (default: 4)"
    )
    parser.add_argument(
        "--timeout",
        type=float,
        default=900.0,
        help="seconds to wait for the game to start, and then for the program to finish (default: 900)",
    )
    parser.add_argument(
        "--inject-fault",
        metavar="FORM",
        help="self-check: flip a bit of r3 in the first case of FORM; "
        "the verifier must report exactly that case",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    try:
        tools = ModernGekkoTools.in_template(arguments.template)
        candidates = collect_candidates(
            Dol(arguments.disc / "sys" / "main.dol"), SymbolTable.read(arguments.symbols)
        )
        if arguments.inject_fault and arguments.inject_fault not in candidates:
            raise SetupError(f"the game has no instruction form called {arguments.inject_fault!r}")
        outcomes = [run_seed(seed, arguments, tools, candidates) for seed in arguments.seeds]
    except SetupError as problem:
        print(f"error: {problem}", file=sys.stderr)
        return 2
    print()
    print(format_summary(outcomes), end="")
    if arguments.inject_fault:
        return report_self_check(arguments.inject_fault, outcomes)
    return 1 if any(outcome.evaluation.diverged_entries for outcome in outcomes) else 0


def report_self_check(form: str, outcomes: list[SeedOutcome]) -> int:
    """With a fault injected, the run is right when it reports the injected case and nothing else."""
    for outcome in outcomes:
        injected = next(case for case in outcome.cases if case.form == form)
        if outcome.evaluation.diverged_entries != {injected.address}:
            print(f"self-check FAILED (seed {outcome.seed}): the verifier did not report exactly the fault")
            return 1
    print("self-check passed: the verifier reported the injected case and nothing else")
    return 0


if __name__ == "__main__":
    sys.exit(main())
