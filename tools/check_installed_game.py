"""Start the game the way the launcher does, and check that it runs and closes.

Arranca el juego como lo hace el launcher, y comprueba que corre y que se cierra.

When the player presses Play, the launcher starts `moderngekko-run` with the extracted game and
the player's folder, and nothing else. The runner then looks for the game's recompiled module in
`<the player's folder>/StaticRecompModules`, where `play.ps1` put it. This tool does the same,
except that it draws nothing and opens no window, so that a machine with no screen and no GPU can
run it. It waits until the game is running, keeps it running for a few seconds, asks it to quit
and checks that it quits by itself with exit code 0.

It is how the workflow of the Windows package checks, with a made-up game and no screen, that the
module `play.ps1` installed is found, loads and starts. Run with only the folders of Windows on
the path, it also shows that the module needs no DLL of the compiler that built it.

With `--expect-mod ID` (repeat it for several mods) it also checks that the runner loaded each of
those mods. The runner says `mod loaded: <id> <version>` for every mod it loads and `mod rejected:
<where>: <why>` for one it cannot, and play.ps1 puts its mods in the player's folder, where the
runner looks for them. So it shows that a mod built by the compiler of the package is one that the
runner can load.

Cuando el jugador pulsa Play, el launcher arranca `moderngekko-run` con el juego extraído y la
carpeta del jugador, y nada más. El runner busca entonces el módulo recompilado del juego en
`<la carpeta del jugador>/StaticRecompModules`, donde lo puso `play.ps1`. Esta herramienta hace lo
mismo, salvo que no dibuja nada ni abre ventana, así que una máquina sin pantalla ni GPU puede
correrla. Espera a que el juego esté en marcha, lo deja corriendo unos segundos, le pide que se
cierre y comprueba que se cierra solo con código de salida 0.

Así comprueba el workflow del paquete de Windows, con un juego inventado y sin pantalla, que el
módulo que instaló `play.ps1` se encuentra, se carga y arranca. Corrida con solo las carpetas de
Windows en la `PATH`, muestra además que el módulo no necesita ninguna DLL del compilador que lo
construyó.

Con `--expect-mod ID` (repítelo para varios mods) comprueba además que el runner cargó cada uno de
esos mods. El runner dice `mod loaded: <id> <versión>` por cada mod que carga y `mod rejected:
<dónde>: <por qué>` por uno que no puede, y play.ps1 pone sus mods en la carpeta del jugador, donde
el runner los busca. Así muestra que un mod construido con el compilador del paquete es uno que el
runner sabe cargar.

Usage / Uso:
    python3 tools/check_installed_game.py --runner bin/moderngekko-run \\
        --game GAME_FOLDER --user-dir PLAYER_FOLDER --work-directory WORK_FOLDER \\
        [--expect-mod jamie_marriage]
"""

import argparse
import re
import shutil
import subprocess
import sys
import time
from collections.abc import Sequence
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from moderngekko_automation import AutomationClient, CommandResult  # noqa: E402
from textfiles import read_program_output  # noqa: E402

POLL_SECONDS = 0.5
# ModernGekko has to start, load the module and boot the game; on a slow machine that takes a while.
DEFAULT_START_TIMEOUT_SECONDS = 180.0
DEFAULT_SECONDS_RUNNING = 10.0
SHUTDOWN_TIMEOUT_SECONDS = 60.0
LOG_LINES_SHOWN_ON_FAILURE = 40
# What the runner writes for each mod it loads ("mod loaded: jamie_marriage 0.1.0"), and for each one it
# cannot ("mod rejected: <where>: <why>").
MOD_LOADED_LINE = re.compile(r"mod loaded: (\S+) ")
MOD_REJECTED_LINE_START = "mod rejected:"


class GameCheckError(Exception):
    """The game did not start, did not stay running, or did not close; the message says which."""


def runner_command(runner: Path, game: Path, user_directory: Path, automation_directory: Path) -> list[str]:
    """What the launcher runs, plus the options that keep the game from drawing and from opening a window.

    There is no `--module` on purpose: the launcher gives none, so the runner has to find the
    module that `play.ps1` installed in the player's folder.
    """
    return [
        str(runner),
        "--game", str(game),
        "--user-dir", str(user_directory),
        "--graphics", "Null",  # nothing needs drawing
        "--mute",
        "--headless",  # and no window is opened
        "--automation-dir", str(automation_directory),
    ]  # fmt: skip


def end_of_log(log_path: Path) -> str:
    """The last lines of the runner's log, to show with the reason of a failure."""
    if not log_path.is_file():
        return "(the runner wrote no log)"
    lines = read_program_output(log_path).splitlines()
    return "\n".join(lines[-LOG_LINES_SHOWN_ON_FAILURE:]) or "(the log is empty)"


def failure_with_log(reason: str, log_path: Path) -> GameCheckError:
    return GameCheckError(f"{reason}\nThe end of {log_path}:\n{end_of_log(log_path)}")


def wait_until_running(
    client: AutomationClient, runner_process: subprocess.Popen, timeout_seconds: float, log_path: Path
) -> None:
    deadline = time.monotonic() + timeout_seconds
    while time.monotonic() < deadline:
        if runner_process.poll() is not None:
            raise failure_with_log(
                f"moderngekko-run exited with code {runner_process.returncode} before the game was running "
                "(is the module in the player's folder, and does it load?)",
                log_path,
            )
        if client.status().get("state") == "running":
            return
        time.sleep(POLL_SECONDS)
    raise failure_with_log(f"the game was not running after {timeout_seconds:g} seconds", log_path)


def stay_running(runner_process: subprocess.Popen, seconds: float, log_path: Path) -> None:
    """The game must not crash soon after it starts, which is when a module that loads badly would."""
    deadline = time.monotonic() + seconds
    while time.monotonic() < deadline:
        if runner_process.poll() is not None:
            raise failure_with_log(
                f"moderngekko-run exited with code {runner_process.returncode} while the game was running",
                log_path,
            )
        time.sleep(POLL_SECONDS)


def close_the_game(client: AutomationClient, runner_process: subprocess.Popen, log_path: Path) -> None:
    """Ask the game to quit, as the launcher's window would, and check that it closes without an error."""
    if client.stop() is not CommandResult.PROCESSED:
        raise failure_with_log("the game did not accept the command to quit", log_path)
    try:
        exit_code = runner_process.wait(timeout=SHUTDOWN_TIMEOUT_SECONDS)
    except subprocess.TimeoutExpired:
        raise failure_with_log(
            f"the game did not close within {SHUTDOWN_TIMEOUT_SECONDS:g} seconds of being asked to quit",
            log_path,
        ) from None
    if exit_code != 0:
        raise failure_with_log(f"the game closed with exit code {exit_code}", log_path)


def check_mods_were_loaded(expected_mods: Sequence[str], log_path: Path) -> None:
    """Each of `expected_mods` must be one that the runner says it loaded; a failure quotes the rejections."""
    if not expected_mods:
        return
    log_text = read_program_output(log_path) if log_path.is_file() else ""
    loaded_mods = set(MOD_LOADED_LINE.findall(log_text))
    not_loaded = [mod_id for mod_id in expected_mods if mod_id not in loaded_mods]
    if not_loaded:
        rejections = [line for line in log_text.splitlines() if line.startswith(MOD_REJECTED_LINE_START)]
        said = "\n".join(rejections) if rejections else "(it did not reject any mod either)"
        raise GameCheckError(
            f"the runner did not load the mod {', '.join(not_loaded)}; "
            f"it loaded: {', '.join(sorted(loaded_mods)) or 'none'}. What it said about mods:\n{said}\n"
            f"The end of {log_path}:\n{end_of_log(log_path)}"
        )


def check_installed_game(
    runner: Path,
    game: Path,
    user_directory: Path,
    work_directory: Path,
    start_timeout_seconds: float = DEFAULT_START_TIMEOUT_SECONDS,
    seconds_running: float = DEFAULT_SECONDS_RUNNING,
    expected_mods: Sequence[str] = (),
) -> None:
    """Start the game, check that it runs for `seconds_running` and closes; raise GameCheckError if not.

    With `expected_mods`, the runner must also have loaded each of those mods from the player's folder.
    """
    # The folder is made anew, so that the status of an earlier run cannot be taken for this one's.
    shutil.rmtree(work_directory, ignore_errors=True)
    automation_directory = work_directory / "automation"
    (automation_directory / "commands").mkdir(parents=True)
    log_path = work_directory / "game.log"
    client = AutomationClient(
        automation_directory, command_timeout_seconds=start_timeout_seconds, poll_seconds=POLL_SECONDS
    )
    command = runner_command(runner, game, user_directory, automation_directory)
    with log_path.open("wb") as log:
        try:
            runner_process = subprocess.Popen(command, stdout=log, stderr=subprocess.STDOUT)
        except OSError as problem:
            raise GameCheckError(f"{runner} could not be started: {problem}") from problem
        try:
            wait_until_running(client, runner_process, start_timeout_seconds, log_path)
            stay_running(runner_process, seconds_running, log_path)
            close_the_game(client, runner_process, log_path)
        finally:
            if runner_process.poll() is None:
                runner_process.kill()
                runner_process.wait()
    # The runner has finished, so its log is complete.
    check_mods_were_loaded(expected_mods, log_path)


def main(argv: list[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument(
        "--runner", type=Path, required=True, help="moderngekko-run, of the package or of a build"
    )
    parser.add_argument(
        "--game", type=Path, required=True, help="the extracted game: the folder with sys/ and files/"
    )
    parser.add_argument(
        "--user-dir",
        type=Path,
        required=True,
        help="the player's folder, where play.ps1 installed the module",
    )
    parser.add_argument(
        "--work-directory",
        type=Path,
        required=True,
        help="where the runner's log and automation folder go (it is emptied first)",
    )
    parser.add_argument(
        "--start-timeout",
        type=float,
        default=DEFAULT_START_TIMEOUT_SECONDS,
        help=f"seconds to wait for the game to be running (default: {DEFAULT_START_TIMEOUT_SECONDS:g})",
    )
    parser.add_argument(
        "--seconds-running",
        type=float,
        default=DEFAULT_SECONDS_RUNNING,
        help=f"seconds the game must stay running before it is closed (default: {DEFAULT_SECONDS_RUNNING:g})",
    )
    parser.add_argument(
        "--expect-mod",
        action="append",
        default=[],
        metavar="ID",
        help="a mod that the runner must load from the player's folder; repeat it for several",
    )
    arguments = parser.parse_args(argv)
    try:
        check_installed_game(
            arguments.runner,
            arguments.game,
            arguments.user_dir,
            arguments.work_directory,
            arguments.start_timeout,
            arguments.seconds_running,
            arguments.expect_mod,
        )
    except GameCheckError as problem:
        print(f"error: {problem}", file=sys.stderr)
        return 1
    print(f"The game started from the module in {arguments.user_dir}, ran and closed normally.")
    if arguments.expect_mod:
        print(f"It loaded the mods: {', '.join(arguments.expect_mod)}.")
    return 0


if __name__ == "__main__":
    sys.exit(main())
