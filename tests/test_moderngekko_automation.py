"""The client of ModernGekko's automation folder. No game needed.

El cliente de la carpeta de automatización de ModernGekko. Sin el juego.

A thread plays the part of `moderngekko-run`: it takes the commands in the order of their
names and moves each one to `processed/` or `failed/`, like the real one.
"""

import sys
import threading
import time
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from moderngekko_automation import AutomationClient, CommandResult  # noqa: E402

FAST = {"command_timeout_seconds": 5.0, "poll_seconds": 0.005}
GUEST_MEMORY_START = 0x80600000
GUEST_MEMORY = bytes(range(16))


def parse_command(path: Path) -> dict[str, str]:
    return dict(line.split("=", 1) for line in path.read_text().splitlines())


class FakeGame:
    """What the client talks to. Commands it does not know go to `failed/`, as in the real game."""

    def __init__(self, folder: Path):
        self.folder = folder
        self.received: list[dict[str, str]] = []
        self.answers_to_memory_reads = True
        self._asked_to_quit = threading.Event()
        self._thread = threading.Thread(target=self._watch_the_commands_folder, daemon=True)

    def start(self) -> None:
        self._thread.start()

    def close(self) -> None:
        self._asked_to_quit.set()
        self._thread.join(timeout=5)

    def _watch_the_commands_folder(self) -> None:
        while not self._asked_to_quit.is_set():
            for path in sorted((self.folder / "commands").glob("*.txt")):
                self._handle_command(path)
            time.sleep(0.002)

    def _handle_command(self, path: Path) -> None:
        fields = parse_command(path)
        self.received.append(fields)
        known = fields["command"] in ("read_memory", "stop", "pause", "resume")
        if fields["command"] == "read_memory" and self.answers_to_memory_reads:
            offset = int(fields["address"], 16) - GUEST_MEMORY_START
            Path(fields["path"]).write_bytes(GUEST_MEMORY[offset : offset + int(fields["size"])])
        destination = self.folder / ("processed" if known else "failed")
        destination.mkdir(exist_ok=True)
        path.rename(destination / path.name)


@pytest.fixture
def folder(tmp_path) -> Path:
    return tmp_path / "automation"


@pytest.fixture
def game(folder):
    folder.mkdir()
    fake = FakeGame(folder)
    fake.start()
    yield fake
    fake.close()


@pytest.fixture
def client(folder) -> AutomationClient:
    return AutomationClient(folder, **FAST)


# -- Sending commands --------------------------------------------------------------------------


def test_a_command_the_game_handles_is_reported_as_processed(client, game):
    assert client.send({"command": "stop"}) is CommandResult.PROCESSED
    assert game.received == [{"command": "stop"}]


def test_a_command_the_game_rejects_is_reported_as_failed(client, game):
    assert client.send({"command": "dance"}) is CommandResult.FAILED


def test_without_a_game_the_wait_ends_with_a_time_out(folder):
    impatient = AutomationClient(folder, command_timeout_seconds=0.05, poll_seconds=0.005)
    assert impatient.send({"command": "stop"}) is CommandResult.TIMED_OUT


def test_commands_are_numbered_one_after_another_so_the_game_runs_them_in_order(client, game, folder):
    client.send({"command": "pause"})
    client.send({"command": "stop"})
    assert [fields["command"] for fields in game.received] == ["pause", "stop"]
    assert sorted(path.name for path in (folder / "processed").glob("*.txt")) == ["0001.txt", "0002.txt"]


def test_numbering_goes_on_after_the_commands_of_an_earlier_session(client, game, folder):
    (folder / "processed").mkdir()
    (folder / "processed" / "0007.txt").write_text("command=stop\n")
    (folder / "failed").mkdir()
    (folder / "failed" / "0009.txt").write_text("command=dance\n")
    client.send({"command": "stop"})
    assert (folder / "processed" / "0010.txt").is_file()


def test_no_half_written_command_is_left_behind(client, game, folder):
    client.send({"command": "stop"})
    assert not list(folder.glob("*.tmp"))
    assert not list((folder / "commands").iterdir())


# -- Status ------------------------------------------------------------------------------------


def test_the_status_is_empty_before_the_game_writes_it(client, folder):
    folder.mkdir()
    assert client.status() == {}


def test_the_status_is_read_as_key_value_lines(client, folder):
    folder.mkdir()
    (folder / "status.txt").write_text("state=running\nframe_count=120\nlast_error=\n")
    assert client.status() == {"state": "running", "frame_count": "120", "last_error": ""}


# -- Reading memory ----------------------------------------------------------------------------


def test_memory_is_read_from_the_file_the_game_writes(client, game, tmp_path):
    answer = client.read_memory(GUEST_MEMORY_START + 4, 4, tmp_path / "answer.bin")
    assert answer == GUEST_MEMORY[4:8]
    assert game.received[0]["address"] == "0x80600004"
    assert game.received[0]["size"] == "4"


def test_an_old_answer_is_never_taken_for_the_new_one(client, game, tmp_path):
    scratch_file = tmp_path / "answer.bin"
    scratch_file.write_bytes(b"\xff\xff\xff\xff")  # left over from an earlier read, same size
    game.answers_to_memory_reads = False
    impatient = AutomationClient(client.folder, command_timeout_seconds=0.1, poll_seconds=0.005)
    assert impatient.read_memory(GUEST_MEMORY_START, 4, scratch_file) is None


def test_a_read_the_game_rejects_gives_none(client, folder, tmp_path):
    # No game thread: the command stays unanswered, so the read gives up.
    impatient = AutomationClient(folder, command_timeout_seconds=0.05, poll_seconds=0.005)
    assert impatient.read_memory(GUEST_MEMORY_START, 4, tmp_path / "answer.bin") is None


def test_stop_asks_the_game_to_quit(client, game):
    assert client.stop() is CommandResult.PROCESSED
    assert game.received == [{"command": "stop"}]
