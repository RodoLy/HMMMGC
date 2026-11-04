"""The check that the installed game starts and closes. No game and no ModernGekko needed.

La comprobación de que el juego instalado arranca y se cierra. Sin el juego ni ModernGekko.

`moderngekko-run` is replaced by small scripts that behave the way the real one does when the game
runs, when it cannot start, when it crashes and when it will not close.
"""

import os
import stat
import sys
import textwrap
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import check_installed_game  # noqa: E402
from check_installed_game import GameCheckError  # noqa: E402

# What every fake runner starts with: it reads the folders it was given, as the real one does,
# and leaves its process number where the test can find it, to check that it is stopped.
FAKE_RUNNER_PRELUDE = """
    import os
    import sys
    import time
    from pathlib import Path

    arguments = sys.argv[1:]
    automation = Path(arguments[arguments.index("--automation-dir") + 1])
    (automation / "process_number.txt").write_text(str(os.getpid()))
    print("arguments:", " ".join(arguments), flush=True)
"""

# What the real runner does with a command: it moves the file to `processed/`.
ANSWERS_COMMANDS = """
    def answer_commands():
        '''Returns the name of the last command it answered, or None if there was none.'''
        answered = None
        for command in sorted((automation / "commands").glob("*.txt")):
            fields = dict(line.split("=", 1) for line in command.read_text().splitlines())
            (automation / "processed").mkdir(exist_ok=True)
            command.rename(automation / "processed" / command.name)
            answered = fields["command"]
        return answered
"""

# The game runs until it is told to stop, and then closes normally: what the real one does.
RUNS_AND_QUITS = (
    FAKE_RUNNER_PRELUDE
    + ANSWERS_COMMANDS
    + """
    (automation / "status.txt").write_text("state=running\\nframe_count=1\\n")
    while True:
        if answer_commands() == "stop":
            sys.exit(%(exit_code_after_stop)d)
        time.sleep(0.005)
"""
)

# The runner cannot start the game, for example because it finds no module.
EXITS_AT_ONCE = (
    FAKE_RUNNER_PRELUDE
    + """
    print("initialization failed: no module for game G4AEE9", file=sys.stderr, flush=True)
    sys.exit(1)
"""
)

# The game starts and then crashes.
CRASHES_AFTER_STARTING = (
    FAKE_RUNNER_PRELUDE
    + """
    (automation / "status.txt").write_text("state=running\\n")
    time.sleep(0.2)
    print("access violation", flush=True)
    sys.exit(3)
"""
)

# The runner never gets as far as running the game.
NEVER_STARTS = (
    FAKE_RUNNER_PRELUDE
    + """
    time.sleep(60)
"""
)

# The game runs but never looks at the commands it is given.
IGNORES_COMMANDS = (
    FAKE_RUNNER_PRELUDE
    + """
    (automation / "status.txt").write_text("state=running\\n")
    time.sleep(60)
"""
)

# The game takes the command to quit, and then does not quit.
ANSWERS_THE_COMMAND_TO_QUIT_BUT_STAYS = (
    FAKE_RUNNER_PRELUDE
    + ANSWERS_COMMANDS
    + """
    (automation / "status.txt").write_text("state=running\\n")
    while True:
        answer_commands()
        time.sleep(0.005)
"""
)


def write_script(path: Path, body: str) -> Path:
    """An executable Python script, to stand in for `moderngekko-run`."""
    path.write_text(f"#!{sys.executable}\n{textwrap.dedent(body)}")
    path.chmod(path.stat().st_mode | stat.S_IEXEC)
    return path


@pytest.fixture(autouse=True)
def quick_polling(monkeypatch):
    monkeypatch.setattr(check_installed_game, "POLL_SECONDS", 0.01)


@pytest.fixture
def folders(tmp_path) -> dict[str, Path]:
    game, user_directory = tmp_path / "game", tmp_path / "player"
    game.mkdir()
    user_directory.mkdir()
    return {"game": game, "user_directory": user_directory, "work_directory": tmp_path / "work"}


def run_with(runner_body: str, folders: dict[str, Path], tmp_path: Path, **options) -> None:
    runner = write_script(tmp_path / "moderngekko-run", runner_body)
    options = {"seconds_running": 0.1, "start_timeout_seconds": 10, **options}
    check_installed_game.check_installed_game(runner, **folders, **options)


def runs_and_quits(exit_code_after_stop: int = 0) -> str:
    return RUNS_AND_QUITS % {"exit_code_after_stop": exit_code_after_stop}


def assert_the_runner_was_stopped(folders: dict[str, Path]) -> None:
    """The check must not leave a game running behind it, whatever went wrong."""
    process_number = int((folders["work_directory"] / "automation" / "process_number.txt").read_text())
    with pytest.raises(ProcessLookupError):
        os.kill(process_number, 0)  # signal 0 only asks whether the process exists


# -- A game that starts, runs and closes -------------------------------------------------------------


def test_a_game_that_runs_and_closes_normally_passes(folders, tmp_path):
    run_with(runs_and_quits(), folders, tmp_path)


def test_the_game_is_started_the_way_the_launcher_does_with_nothing_drawn(folders, tmp_path):
    run_with(runs_and_quits(), folders, tmp_path)
    log = (folders["work_directory"] / "game.log").read_text()
    seen = log.splitlines()[0].split()[1:]
    assert seen[seen.index("--game") + 1] == str(folders["game"])
    assert seen[seen.index("--user-dir") + 1] == str(folders["user_directory"])
    assert seen[seen.index("--graphics") + 1] == "Null"
    assert "--headless" in seen
    assert "--mute" in seen


def test_the_module_is_never_given_so_that_the_runner_has_to_find_the_installed_one(folders, tmp_path):
    run_with(runs_and_quits(), folders, tmp_path)
    log = (folders["work_directory"] / "game.log").read_text()
    assert "--module" not in log


def test_the_status_of_an_earlier_run_is_not_taken_for_this_ones(folders, tmp_path):
    stale = folders["work_directory"] / "automation"
    stale.mkdir(parents=True)
    (stale / "status.txt").write_text("state=running\n")
    with pytest.raises(GameCheckError, match="was not running after 0.3 seconds"):
        run_with(NEVER_STARTS, folders, tmp_path, start_timeout_seconds=0.3)


# -- The ways it can fail, each said in words and with the end of the log ---------------------------------


def test_a_runner_that_cannot_start_the_game_is_reported_with_its_own_words(folders, tmp_path):
    with pytest.raises(GameCheckError) as problem:
        run_with(EXITS_AT_ONCE, folders, tmp_path)
    message = str(problem.value)
    assert "exited with code 1 before the game was running" in message
    assert "initialization failed: no module for game G4AEE9" in message


def test_a_game_that_never_runs_fails_when_the_time_to_start_is_over(folders, tmp_path):
    with pytest.raises(GameCheckError, match="was not running after 0.3 seconds"):
        run_with(NEVER_STARTS, folders, tmp_path, start_timeout_seconds=0.3)
    assert_the_runner_was_stopped(folders)


def test_a_game_that_crashes_soon_after_starting_fails(folders, tmp_path):
    with pytest.raises(GameCheckError, match="exited with code 3 while the game was running") as problem:
        run_with(CRASHES_AFTER_STARTING, folders, tmp_path, seconds_running=2)
    assert "access violation" in str(problem.value)


def test_a_game_that_closes_with_an_error_fails(folders, tmp_path):
    with pytest.raises(GameCheckError, match="closed with exit code 1"):
        run_with(runs_and_quits(exit_code_after_stop=1), folders, tmp_path)


def test_a_game_that_ignores_the_command_to_quit_fails_and_is_stopped(folders, tmp_path):
    with pytest.raises(GameCheckError, match="did not accept the command to quit"):
        run_with(IGNORES_COMMANDS, folders, tmp_path, start_timeout_seconds=0.5)
    assert_the_runner_was_stopped(folders)


def test_a_game_that_takes_the_command_to_quit_but_does_not_close_fails_and_is_stopped(
    folders, tmp_path, monkeypatch
):
    monkeypatch.setattr(check_installed_game, "SHUTDOWN_TIMEOUT_SECONDS", 0.3)
    with pytest.raises(GameCheckError, match="did not close within 0.3 seconds"):
        run_with(ANSWERS_THE_COMMAND_TO_QUIT_BUT_STAYS, folders, tmp_path)
    assert_the_runner_was_stopped(folders)


def test_a_runner_that_is_not_there_is_reported(folders, tmp_path):
    missing = tmp_path / "missing" / "moderngekko-run"
    with pytest.raises(GameCheckError, match="could not be started"):
        check_installed_game.check_installed_game(missing, **folders, seconds_running=0.1)


# -- The command line --------------------------------------------------------------------------------------


def command_line(runner: Path, folders: dict[str, Path]) -> list[str]:
    return [
        "--runner", str(runner),
        "--game", str(folders["game"]),
        "--user-dir", str(folders["user_directory"]),
        "--work-directory", str(folders["work_directory"]),
        "--seconds-running", "0.1",
    ]  # fmt: skip


def test_the_command_line_of_a_game_that_works_exits_with_zero(folders, tmp_path, capsys):
    runner = write_script(tmp_path / "moderngekko-run", runs_and_quits())
    assert check_installed_game.main(command_line(runner, folders)) == 0
    assert "ran and closed normally" in capsys.readouterr().out


def test_the_command_line_of_a_game_that_fails_exits_with_one_and_says_why(folders, tmp_path, capsys):
    runner = write_script(tmp_path / "moderngekko-run", EXITS_AT_ONCE)
    assert check_installed_game.main(command_line(runner, folders)) == 1
    assert "error: moderngekko-run exited with code 1" in capsys.readouterr().err
