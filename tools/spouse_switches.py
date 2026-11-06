#!/usr/bin/env python3
"""Find where an event script chooses, by the spouse's number, what the spouse says and who is on stage.

Encuentra dónde un script de evento elige, por el número del cónyuge, lo que dice el cónyuge y quién sale.

Some events have one script for every spouse: the special days of a married couple (the morning when the
spouse asks the player to come home early, the evening of the wedding anniversary and of the child's
birthday, and the child's checkup). The script reads who the spouse is (game value 38, the character number
at save data + 0x22B8) into a local variable and then picks the lines with a chain of comparisons, one for
each partner the game knows (3 to 22):

    LOCAL_GET spouse; PUSH_INT 3; NOT_EQUAL; JUMP_IF_ZERO case_3
    LOCAL_GET spouse; PUSH_INT 4; NOT_EQUAL; JUMP_IF_ZERO case_4
    ...
    JUMP after_the_cases
    case_3: ... PUSH_INT 1; CALL_BUILTIN 51 (SHOW_MESSAGE) ...; JUMP after_the_cases

Eve_memo04 first copies the spouse into another local with such a chain (case_3: PUSH_INT 3; LOCAL_SET 0),
and then uses that copy as the actor of the scene and in chains of its own. A spouse with no comparison of
its own, such as Jamie (2), takes none of the cases: it says nothing, and the copy keeps the value it had.

This tool lists, for each partner, the bytes of the script that hold the partner's number (in the
comparisons and in the copies) and the messages that the partner's case of each chain shows.
mods/jamie_marriage writes Jamie's number over one partner's bytes when the game loads the script, so that
Jamie takes that partner's cases, and puts Jamie's lines in those messages (tests/test_jamie_special_days.py
compares the mod's lists with this tool, and tests/test_spouse_switches.py tests the tool itself).

Algunos eventos tienen un solo script para todos los cónyuges: los días especiales de un matrimonio (la
mañana en que el cónyuge le pide al jugador que vuelva temprano, la noche del aniversario de bodas y del
cumpleaños del hijo, y la revisión del hijo). El script lee quién es el cónyuge (el valor 38 del juego, el
número de personaje en los datos de la partida + 0x22B8) en una variable local y elige las frases con una
cadena de comparaciones, una por cada pareja que conoce el juego (de 3 a 22). Eve_memo04 primero copia al
cónyuge en otra variable con una cadena así, y usa la copia como actor de la escena y en cadenas propias.
Un cónyuge sin comparación propia, como Jamie (2), no entra en ningún caso: no dice nada, y la copia se
queda con el valor que tenía. Esta herramienta lista, para cada pareja, los bytes del script que guardan su
número (en las comparaciones y en las copias) y los mensajes que muestra su caso en cada cadena.
mods/jamie_marriage escribe el número de Jamie sobre los bytes de una pareja cuando el juego carga el
script, para que Jamie entre en los casos de esa pareja, y pone sus frases en esos mensajes
(tests/test_jamie_special_days.py compara las listas del mod con esta herramienta, y
tests/test_spouse_switches.py prueba la herramienta misma).

Usage / Uso:
    python3 tools/spouse_switches.py Eve_memo00          # every partner's comparisons and messages
    python3 tools/spouse_switches.py Eve_memo00 4        # partner 4's
    python3 tools/spouse_switches.py Eve_memo00 4 --c    # the file offsets of partner 4's bytes, in C
"""

from __future__ import annotations

import argparse
import sys
from dataclasses import dataclass
from pathlib import Path

from lsb import BUILTIN_NAMES, Instruction, Opcode, Script, load_script
from project import SCRIPT_DIRECTORY

SPOUSE_GAME_VALUE = 38  # what GET_GAME_VALUE (built-in 55) returns for it: the spouse's character number
GET_GAME_VALUE_BUILTIN = next(index for index, name in BUILTIN_NAMES.items() if name == "GET_GAME_VALUE")
SHOW_MESSAGE_BUILTIN = next(index for index, name in BUILTIN_NAMES.items() if name == "SHOW_MESSAGE")
COMPARISONS = (Opcode.NOT_EQUAL, Opcode.EQUAL)
COMPARISON_INSTRUCTIONS = 4  # LOCAL_GET, PUSH_INT, the comparison and JUMP_IF_ZERO
CASE_ENDS = (Opcode.JUMP, Opcode.RETURN, Opcode.RETURN_VOID)
PUSH_INT_OPERAND_BYTES = 1  # the operand of PUSH_INT n is one byte for -64 <= n < 64 (lsb.read_varint)


@dataclass(frozen=True)
class SpouseCase:
    """One comparison of a chain: the partner it tests, where its number is, and what its case does."""

    chain: int  # which chain of the script, counting from 0 in the order of the code
    partner: int
    number_file_offset: int  # the byte that holds the partner's number in the comparison
    case_code: range  # the code offsets of the case, from its first instruction to its jump or return
    messages: tuple[int, ...]  # the messages that the case shows, in order
    copy_file_offset: int | None  # the partner's number in `PUSH_INT n; LOCAL_SET k` when the case copies it


def resolve_any_case(name: str, directory: Path = SCRIPT_DIRECTORY) -> Path:
    """The script file of an event, whatever the case of its name on the disc (eve_memo00, Eve_memo04)."""
    wanted = (name if name.lower().endswith(".lsb") else name + ".lsb").lower()
    for path in directory.iterdir():
        if path.name.lower() == wanted:
            return path
    raise FileNotFoundError(f"no script {name!r} in {directory}")


def is_push_int(instruction: Instruction, value: int | None = None) -> bool:
    """A PUSH_INT whose operand takes one byte (and is `value`, when one is given)."""
    return (
        instruction.opcode is Opcode.PUSH_INT
        and len(instruction.encoded) == 1 + PUSH_INT_OPERAND_BYTES
        and (value is None or instruction.operand == value)
    )


def show_message_sites(script: Script) -> list[tuple[int, int]]:
    """(code offset of the call, message) for each `PUSH_INT message; CALL_BUILTIN 51 (SHOW_MESSAGE)`."""
    instructions = script.instructions
    return [
        (call.offset, int(pushed.operand))
        for pushed, call in zip(instructions, instructions[1:], strict=False)
        if call.opcode is Opcode.CALL_BUILTIN
        and call.builtin_index == SHOW_MESSAGE_BUILTIN
        and is_push_int(pushed)
    ]


class ScriptCode:
    """The instructions of a script with what the search needs: where each one is and its function."""

    def __init__(self, script: Script):
        self.script = script
        self.instructions: tuple[Instruction, ...] = script.instructions
        self.position_of = {instruction.offset: index for index, instruction in enumerate(self.instructions)}
        entries = {offset for _, offset in script.header.entries}
        self.function_starts = sorted(entries | set(script.call_targets))

    def function_of(self, position: int) -> int:
        """The start of the function that the instruction at `position` belongs to."""
        offset = self.instructions[position].offset
        return max(start for start in self.function_starts if start <= offset)

    def offset_of(self, position: int) -> int:
        """The code offset of the instruction at `position`, or the end of the code after the last one."""
        if position < len(self.instructions):
            return self.instructions[position].offset
        return self.script.code_size

    def operand_file_offset(self, position: int) -> int:
        return self.script.file_offset(self.instructions[position].offset + 1)


def locals_read_from_the_game(code: ScriptCode) -> set[tuple[int, int]]:
    """(function, local) for each `PUSH_INT 38; CALL_BUILTIN_VALUE 55; LOCAL_SET local`."""
    found = set()
    for position in range(len(code.instructions) - 2):
        pushed, call, store = code.instructions[position : position + 3]
        if (
            is_push_int(pushed, SPOUSE_GAME_VALUE)
            and call.opcode is Opcode.CALL_BUILTIN_VALUE
            and call.builtin_index == GET_GAME_VALUE_BUILTIN
            and store.opcode is Opcode.LOCAL_SET
        ):
            found.add((code.function_of(position), int(store.operand)))
    return found


def comparison_at(code: ScriptCode, position: int, spouse_locals: set[tuple[int, int]]) -> int | None:
    """The position where the comparison's case starts, if a comparison of a spouse local starts here."""
    if position + COMPARISON_INSTRUCTIONS > len(code.instructions):
        return None
    get, pushed, compare, jump = code.instructions[position : position + COMPARISON_INSTRUCTIONS]
    if not (
        get.opcode is Opcode.LOCAL_GET
        and (code.function_of(position), int(get.operand)) in spouse_locals
        and is_push_int(pushed)
        and compare.opcode in COMPARISONS
        and jump.opcode is Opcode.JUMP_IF_ZERO
    ):
        return None
    # NOT_EQUAL jumps to the case when the spouse is the partner (the test gives zero); EQUAL skips it.
    case_start = jump.branch_target if compare.opcode is Opcode.NOT_EQUAL else jump.end
    return code.position_of[case_start]


def end_of_the_case(code: ScriptCode, case: int) -> int:
    """The position of the jump or return that ends the case that starts at position `case`."""
    position = case
    while position < len(code.instructions) and code.instructions[position].opcode not in CASE_ENDS:
        position += 1
    return position


def copy_at_the_start_of(code: ScriptCode, case: int, partner: int) -> tuple[int, int] | None:
    """(position of the PUSH_INT, local) when the case starts by copying the partner's number into a local."""
    store = code.instructions[case + 1] if case + 1 < len(code.instructions) else None
    if (
        is_push_int(code.instructions[case], partner)
        and store is not None
        and store.opcode is Opcode.LOCAL_SET
    ):
        return case, int(store.operand)
    return None


def spouse_locals_of(code: ScriptCode) -> set[tuple[int, int]]:
    """The locals that hold the spouse: read from the game, or copied from one of those by a chain."""
    spouse_locals = locals_read_from_the_game(code)
    while True:
        copies = set()
        for position in range(len(code.instructions)):
            case = comparison_at(code, position, spouse_locals)
            if case is None:
                continue
            copy = copy_at_the_start_of(code, case, int(code.instructions[position + 1].operand))
            if copy is not None:
                copies.add((code.function_of(position), copy[1]))
        if copies <= spouse_locals:
            return spouse_locals
        spouse_locals |= copies


def spouse_cases(script: Script) -> list[SpouseCase]:
    """Every comparison of the spouse with a partner's number, in the order of the code.

    The comparisons of one chain come one right after the other, so a comparison that does not start where
    the previous one ended starts a new chain.
    """
    code = ScriptCode(script)
    spouse_locals = spouse_locals_of(code)
    sites = show_message_sites(script)
    cases = []
    chain = -1
    end_of_the_previous_comparison = None
    for position in range(len(code.instructions)):
        case = comparison_at(code, position, spouse_locals)
        if case is None:
            continue
        if position != end_of_the_previous_comparison:
            chain += 1
        end_of_the_previous_comparison = position + COMPARISON_INSTRUCTIONS
        partner = int(code.instructions[position + 1].operand)
        case_code = range(code.offset_of(case), code.offset_of(end_of_the_case(code, case)))
        copy = copy_at_the_start_of(code, case, partner)
        cases.append(
            SpouseCase(
                chain=chain,
                partner=partner,
                number_file_offset=code.operand_file_offset(position + 1),
                case_code=case_code,
                messages=tuple(message for offset, message in sites if offset in case_code),
                copy_file_offset=code.operand_file_offset(copy[0]) if copy is not None else None,
            )
        )
    return cases


def partner_bytes(cases: list[SpouseCase], partner: int) -> list[int]:
    """The file offsets of every byte that holds this partner's number: comparisons and copies."""
    offsets = []
    for case in cases:
        if case.partner == partner:
            offsets.append(case.number_file_offset)
            if case.copy_file_offset is not None:
                offsets.append(case.copy_file_offset)
    return sorted(offsets)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("script", help="script name (Eve_memo00) or path to a .lsb file")
    parser.add_argument("partner", nargs="?", type=int, help="only this partner's comparisons")
    parser.add_argument("--c", action="store_true", help="print the partner's bytes' offsets as a C array")
    arguments = parser.parse_args()
    path = Path(arguments.script)
    cases = spouse_cases(load_script(path if path.is_file() else resolve_any_case(arguments.script)))
    if arguments.c:
        if arguments.partner is None:
            parser.error("--c needs a partner")
        print(", ".join(f"0x{offset:04X}" for offset in partner_bytes(cases, arguments.partner)))
        return 0
    for case in cases:
        if arguments.partner not in (None, case.partner):
            continue
        messages = ", ".join(str(message) for message in case.messages) or "none"
        copy = f", copies it at 0x{case.copy_file_offset:04X}" if case.copy_file_offset is not None else ""
        print(
            f"chain {case.chain}: partner {case.partner:2d} at file offset 0x{case.number_file_offset:04X},"
            f" shows messages {messages}{copy}"
        )
    return 0


if __name__ == "__main__":
    sys.exit(main())
