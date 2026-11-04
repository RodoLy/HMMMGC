"""Steer a running ModernGekko through its automation folder.

Dirige un ModernGekko en marcha con su carpeta de automatización.

`moderngekko-run --automation-dir DIR` watches `DIR/commands/` for small text files of
`key=value` lines, runs them in the order of their names, moves each one to `DIR/processed/`
(or `DIR/failed/`) and keeps `DIR/status.txt` up to date. This module is a client for that
protocol: it queues a command, waits until the game has handled it and reads the answer.

The commands this project uses:

    command=read_memory   address=0x80600000  size=4  path=/tmp/answer.bin
    command=stop

`moderngekko-run --automation-dir DIR` vigila `DIR/commands/` en busca de archivos de texto
con líneas `clave=valor`, los ejecuta en el orden de sus nombres, mueve cada uno a
`DIR/processed/` (o `DIR/failed/`) y mantiene al día `DIR/status.txt`. Este módulo es un
cliente de ese protocolo: pone un comando en cola, espera a que el juego lo atienda y lee la
respuesta.
"""

import time
from dataclasses import dataclass
from enum import Enum
from pathlib import Path

from textfiles import read_program_output, write_text

COMMANDS_FOLDER = "commands"
PROCESSED_FOLDER = "processed"
FAILED_FOLDER = "failed"
STATUS_FILE = "status.txt"

# One frame of the game can take a second on a slow machine, and the first commands wait for
# the game to start, so the wait for an answer is generous.
DEFAULT_COMMAND_TIMEOUT_SECONDS = 180.0
DEFAULT_POLL_SECONDS = 0.5


class CommandResult(Enum):
    PROCESSED = "processed"
    FAILED = "failed"
    TIMED_OUT = "timed out"


@dataclass
class AutomationClient:
    folder: Path
    command_timeout_seconds: float = DEFAULT_COMMAND_TIMEOUT_SECONDS
    poll_seconds: float = DEFAULT_POLL_SECONDS

    def status(self) -> dict[str, str]:
        """The contents of status.txt (`state`, `frame_count`, ...); empty before the game writes it."""
        path = self.folder / STATUS_FILE
        if not path.is_file():
            return {}
        lines = read_program_output(path).splitlines()
        return {key: value for key, _, value in (line.partition("=") for line in lines)}

    def send(self, fields: dict[str, str]) -> CommandResult:
        """Queue one command and wait until ModernGekko has handled it."""
        number = self._next_command_number()
        name = f"{number:04d}.txt"
        commands = self.folder / COMMANDS_FOLDER
        commands.mkdir(parents=True, exist_ok=True)
        # Written next to the folder and then renamed into it, so the game never reads half a command.
        temporary = self.folder / f"{number:04d}.tmp"
        write_text(temporary, "".join(f"{key}={value}\n" for key, value in fields.items()))
        temporary.rename(commands / name)
        return self._wait_for_answer(name)

    def read_memory(self, address: int, size: int, scratch_file: Path) -> bytes | None:
        """Bytes of the guest's memory, or None if the game did not answer.

        ModernGekko writes the bytes to `scratch_file`, which is deleted first so that an old
        answer cannot be taken for the new one.
        """
        scratch_file.unlink(missing_ok=True)
        fields = {"command": "read_memory", "address": f"0x{address:08X}", "size": str(size)}
        if self.send({**fields, "path": str(scratch_file)}) is not CommandResult.PROCESSED:
            return None
        deadline = time.monotonic() + self.command_timeout_seconds
        while time.monotonic() < deadline:
            if scratch_file.is_file() and scratch_file.stat().st_size == size:
                return scratch_file.read_bytes()
            time.sleep(self.poll_seconds)
        return None

    def stop(self) -> CommandResult:
        """Ask the game to quit; it then writes whatever it writes on exit (logs, coverage)."""
        return self.send({"command": "stop"})

    def _next_command_number(self) -> int:
        numbers = [
            int(path.stem)
            for folder in (COMMANDS_FOLDER, PROCESSED_FOLDER, FAILED_FOLDER)
            for path in (self.folder / folder).glob("*.txt")
            if path.stem.isdigit()
        ]
        return max(numbers, default=0) + 1

    def _wait_for_answer(self, name: str) -> CommandResult:
        deadline = time.monotonic() + self.command_timeout_seconds
        while time.monotonic() < deadline:
            if (self.folder / PROCESSED_FOLDER / name).exists():
                return CommandResult.PROCESSED
            if (self.folder / FAILED_FOLDER / name).exists():
                return CommandResult.FAILED
            time.sleep(self.poll_seconds)
        return CommandResult.TIMED_OUT
