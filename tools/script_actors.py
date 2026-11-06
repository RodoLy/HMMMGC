#!/usr/bin/env python3
"""Find where an event script names a character: as an actor of the scene, or as the one who speaks.

Encuentra dónde un script de evento nombra a un personaje: como actor de la escena, o como quien habla.

The built-ins that act on a scene's actors (place one, move it, play a motion, turn it, read where it
is...) take the actor as three values: a kind (1 for a character), the character number and a third value
(0 for characters). A script pushes its arguments before each call, so a pass over each statement with a
stack of where every value came from gives the arguments of every call. An argument slot of a built-in is
an actor slot when nearly every call on the disc passes such a handle there; the script's own functions
take an actor as an argument when they pass that argument on to an actor slot (Eve_conce10's sub 0x035F,
which turns an actor towards an angle, is one). Built-in 164 (fn_80058774) takes a bare character number:
it writes it into the message window (0x802DBA28 + 0xC), the character who speaks the next line.

Las funciones nativas que actúan sobre los actores de una escena (colocarlo, moverlo, darle una
animación, girarlo, leer dónde está...) reciben el actor como tres valores: un tipo (1 para un
personaje), el número de personaje y un tercer valor (0 para personajes). Un script empuja sus argumentos
antes de cada llamada, así que recorrer cada instrucción con una pila de dónde salió cada valor da los
argumentos de cada llamada. Una posición de argumento de una función nativa es de actor cuando casi todas
las llamadas del disco pasan ahí ese trío; las funciones propias del script reciben un actor cuando lo
pasan a una posición de actor (la sub 0x035F de Eve_conce10, que gira un actor hacia un ángulo, es una).
La función nativa 164 (fn_80058774) recibe un número de personaje solo: lo escribe en la ventana de
mensajes (0x802DBA28 + 0xC), el personaje que dice la siguiente frase.

How many values a built-in that returns one takes is not in the script, so it is worked out from the
statements of the disc where that call is the only unknown: a statement that ends by storing or testing
a value leaves exactly one value for it. mods/jamie_marriage uses this to put Jamie in the pregnancy and
birth scenes of another partner (tests/test_jamie_pregnancy_and_birth.py compares its lists with this
tool).

Cuántos valores toma una función nativa que devuelve uno no está en el script, así que se deduce de las
instrucciones del disco donde esa llamada es la única incógnita: una instrucción que termina guardando o
probando un valor le deja exactamente uno. mods/jamie_marriage usa esto para poner a Jamie en las escenas
de embarazo y parto de otra pareja (tests/test_jamie_pregnancy_and_birth.py compara sus listas con esta
herramienta).

Usage / Uso:
    python3 tools/script_actors.py Eve_conce01 4          # where Eve_conce01 names character 4
    python3 tools/script_actors.py Eve_conce01 4 --c      # the file offsets of the bytes, as a C array
"""

from __future__ import annotations

import argparse
import collections
import sys
from dataclasses import dataclass
from pathlib import Path

from lsb import BINARY_OPERATORS, UNARY_OPERATORS, Opcode, Script, load_script, resolve_script_path
from project import SCRIPT_DIRECTORY

CHARACTER_HANDLE_KIND = 1  # the first value of an actor handle when the actor is a character
LARGEST_HANDLE_KIND = 15  # the kinds seen on the disc are 1, 2, 5, 7, 8 and 9
ACTOR_SLOT_SHARE = 0.9  # an actor slot holds a handle in at least this share of a built-in's calls
FEWEST_CALLS_FOR_AN_ACTOR_SLOT = 3
SPEAKER_BUILTIN = 164  # fn_80058774: its one argument is the character who speaks the next line
PUSH_INT_OPERAND_BYTES = 1  # the operand of PUSH_INT n is one byte for -64 <= n < 64 (lsb.read_varint)
FIRST_ARGUMENT_LOCAL = -2  # LOCAL_GET -2 is the last argument of the function, -3 the one before...

# Instructions after which a statement's values have all been used.
CONSUMES_ONE_VALUE = frozenset({Opcode.LOCAL_SET, Opcode.GLOBAL_SET, Opcode.JUMP_IF_ZERO})
ENDS_A_STATEMENT = CONSUMES_ONE_VALUE | frozenset(
    {Opcode.CALL_BUILTIN, Opcode.CALL, Opcode.JUMP, Opcode.RETURN, Opcode.RETURN_VOID, Opcode.ENTER}
)
PUSHES_A_VALUE = frozenset(
    {
        Opcode.PUSH_INT,
        Opcode.PUSH_FLOAT,
        Opcode.PUSH_STRING,
        Opcode.LOCAL_REF,
        Opcode.LOCAL_GET,
        Opcode.GLOBAL_REF,
        Opcode.GLOBAL_GET,
        Opcode.PUSH_RESULT,
    }
)


@dataclass(frozen=True)
class Value:
    """Where a value on the stack came from: a PUSH_INT, a LOCAL_GET, or anything else."""

    push: object = None  # the lsb.Instruction of a PUSH_INT
    local: int | None = None  # n of LOCAL_GET n: a local variable, or an argument when n <= -2

    def integer(self) -> int | None:
        return int(self.push.operand) if self.push is not None else None

    @property
    def argument_local(self) -> int | None:
        """n when the value is an argument of the function (LOCAL_GET n with n <= -2), else None."""
        return self.local if self.local is not None and self.local <= FIRST_ARGUMENT_LOCAL else None

    def is_known(self) -> bool:
        return self.push is not None or self.local is not None


@dataclass(frozen=True)
class Call:
    instruction: object  # the lsb.Instruction of the call
    is_builtin: bool
    target: int  # the built-in's index, or the code offset of the called function
    arguments: tuple[Value, ...]
    function: int | None  # the code offset of the function that makes the call


@dataclass(frozen=True)
class CharacterReference:
    code_offset: int  # of the PUSH_INT that pushes the character number
    file_offset: int  # of its operand byte, the byte that holds the number
    how: str


def function_argument_counts(script: Script) -> dict[int, int]:
    """For each function of the script (where an ENTER starts it), how many arguments it takes.

    RETURN n releases the n locals and arguments, so the arguments are n minus what ENTER reserved.
    """
    instructions = script.instructions
    index_at = {instruction.offset: index for index, instruction in enumerate(instructions)}
    counts = {}
    for start in sorted(script.call_targets | {offset for _, offset in script.header.entries}):
        index = index_at.get(start)
        if index is None or instructions[index].opcode is not Opcode.ENTER:
            continue
        locals_reserved = int(instructions[index].operand)
        for instruction in instructions[index + 1 :]:
            if instruction.opcode is Opcode.RETURN:
                counts[start] = int(instruction.operand) - locals_reserved
                break
            if instruction.opcode is Opcode.RETURN_VOID:
                counts[start] = 0
                break
    return counts


def statements_of(script: Script) -> list[list]:
    """The instructions of the script, cut where a statement ends or a jump lands."""
    statements: list[list] = [[]]
    for instruction in script.instructions:
        if instruction.offset in script.jump_targets and statements[-1]:
            statements.append([])
        statements[-1].append(instruction)
        if instruction.opcode in ENDS_A_STATEMENT:
            statements.append([])
    return [statement for statement in statements if statement]


def value_builtin_arities(scripts: list[Script]) -> dict[int, int]:
    """How many values each built-in that returns a value takes, from the statements where it is solvable."""
    arities: dict[int, int] = {}
    statement_lists = [
        (script, statements_of(script), function_argument_counts(script)) for script in scripts
    ]
    while True:
        votes: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
        for _script, statements, argument_counts in statement_lists:
            for statement in statements:
                if statement[-1].opcode not in CONSUMES_ONE_VALUE:
                    continue
                depth = 0
                unknown = []
                for instruction in statement[:-1]:
                    opcode = instruction.opcode
                    if opcode in PUSHES_A_VALUE:
                        depth += 1
                    elif opcode in BINARY_OPERATORS:
                        depth -= 1
                    elif opcode is Opcode.CALL_BUILTIN_VALUE:
                        index = instruction.builtin_index
                        if index in arities:
                            depth += 1 - arities[index]
                        else:
                            unknown.append(index)
                            depth += 1
                    elif opcode is Opcode.CALL_VALUE:
                        depth += 1 - argument_counts.get(instruction.branch_target, 0)
                    elif opcode not in UNARY_OPERATORS:
                        unknown.append(None)  # something this pass does not follow
                # The last instruction takes one value, so the statement must leave exactly one.
                if len(unknown) == 1 and unknown[0] is not None and depth - 1 >= 0:
                    votes[unknown[0]][depth - 1] += 1
        new = {
            index: counter.most_common(1)[0][0] for index, counter in votes.items() if index not in arities
        }
        if not new:
            return arities
        arities.update(new)


def calls_of(script: Script, arities: dict[int, int]) -> list[Call]:
    """Every call of the script with the values it receives, for the calls whose values can be followed."""
    argument_counts = function_argument_counts(script)
    calls = []
    function = None  # the function the statements belong to, from its ENTER on
    for statement in statements_of(script):
        stack: list[Value] = []
        for instruction in statement:
            if instruction.offset in argument_counts:
                function = instruction.offset
            opcode = instruction.opcode
            if opcode is Opcode.PUSH_INT:
                stack.append(Value(push=instruction))
            elif opcode is Opcode.LOCAL_GET:
                stack.append(Value(local=int(instruction.operand)))
            elif opcode in PUSHES_A_VALUE:
                stack.append(Value())
            elif opcode in BINARY_OPERATORS:
                del stack[-2:]
                stack.append(Value())
            elif opcode in UNARY_OPERATORS:
                stack[-1:] = [Value()]
            elif opcode is Opcode.CALL_BUILTIN_VALUE:
                arity = arities.get(instruction.builtin_index)
                if arity is None or arity > len(stack):
                    break  # the rest of the statement cannot be followed
                arguments = tuple(stack[len(stack) - arity :])
                del stack[len(stack) - arity :]
                calls.append(Call(instruction, True, instruction.builtin_index, arguments, function))
                stack.append(Value())
            elif opcode is Opcode.CALL_BUILTIN:
                calls.append(Call(instruction, True, instruction.builtin_index, tuple(stack), function))
                stack = []
            elif opcode in (Opcode.CALL, Opcode.CALL_VALUE):
                target = instruction.branch_target
                count = argument_counts.get(target)
                if count is None or count > len(stack):
                    break  # the rest of the statement cannot be followed
                arguments = tuple(stack[len(stack) - count :])
                del stack[len(stack) - count :]
                calls.append(Call(instruction, False, target, arguments, function))
                if opcode is Opcode.CALL_VALUE:
                    stack.append(Value())
    return calls


def is_handle(values: tuple[Value, ...]) -> bool:
    """Whether three values look like an actor handle: a small kind, then a number, then a third value."""
    kind, number, third = values
    kind_value = kind.integer()
    return (
        kind_value is not None
        and 0 < kind_value <= LARGEST_HANDLE_KIND
        and number.is_known()
        and third.is_known()
    )


def actor_slots(scripts: list[Script], arities: dict[int, int]) -> dict[int, set[int]]:
    """For each built-in, the argument positions where an actor handle starts."""
    calls_seen: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    handles_seen: dict[int, collections.Counter] = collections.defaultdict(collections.Counter)
    for script in scripts:
        for call in calls_of(script, arities):
            if not call.is_builtin:
                continue
            for slot in range(len(call.arguments) - 2):
                calls_seen[call.target][slot] += 1
                if is_handle(call.arguments[slot : slot + 3]):
                    handles_seen[call.target][slot] += 1
    slots: dict[int, set[int]] = collections.defaultdict(set)
    for builtin, counter in calls_seen.items():
        for slot, total in counter.items():
            if handles_seen[builtin][slot] >= max(FEWEST_CALLS_FOR_AN_ACTOR_SLOT, ACTOR_SLOT_SHARE * total):
                slots[builtin].add(slot)
    return dict(slots)


def actor_arguments_of_functions(
    calls: list[Call], argument_counts: dict[int, int], slots: dict[int, set[int]]
) -> dict[int, set[int]]:
    """For each function of the script, which of its arguments are character numbers of actors."""
    actor_arguments: dict[int, set[int]] = collections.defaultdict(set)
    changed = True
    while changed:
        changed = False
        for call in calls:
            if call.function is None or call.function not in argument_counts:
                continue
            count = argument_counts[call.function]
            for position in number_positions(call, slots, actor_arguments):
                value = call.arguments[position]
                if value.argument_local is None:
                    continue
                argument = count + value.argument_local - FIRST_ARGUMENT_LOCAL - 1  # -2 is the last argument
                if 0 <= argument < count and argument not in actor_arguments[call.function]:
                    actor_arguments[call.function].add(argument)
                    changed = True
    return dict(actor_arguments)


def number_positions(
    call: Call, slots: dict[int, set[int]], actor_arguments: dict[int, set[int]]
) -> list[int]:
    """The argument positions of a call that hold the character number of an actor."""
    if call.is_builtin:
        return [slot + 1 for slot in slots.get(call.target, ()) if slot + 1 < len(call.arguments)]
    return [position for position in actor_arguments.get(call.target, ()) if position < len(call.arguments)]


def character_references(
    script: Script, character: int, arities: dict[int, int], slots: dict[int, set[int]]
) -> list[CharacterReference]:
    """Every place where the script names `character` as an actor or as the speaker of a line."""
    calls = calls_of(script, arities)
    argument_counts = function_argument_counts(script)
    actor_arguments = actor_arguments_of_functions(calls, argument_counts, slots)
    references = {}
    for call in calls:
        for position in number_positions(call, slots, actor_arguments):
            value = call.arguments[position]
            if value.integer() != character:
                continue
            if call.is_builtin and call.arguments[position - 1].integer() != CHARACTER_HANDLE_KIND:
                continue  # the same number as a handle of another kind is not the character
            how = (
                f"actor of built-in {call.target}" if call.is_builtin else f"actor of sub 0x{call.target:04X}"
            )
            references[value.push.offset] = (value.push, how)
        if call.is_builtin and call.target == SPEAKER_BUILTIN and call.arguments:
            value = call.arguments[0]
            if value.integer() == character:
                references[value.push.offset] = (value.push, f"speaker (built-in {SPEAKER_BUILTIN})")
    result = []
    for code_offset in sorted(references):
        push, how = references[code_offset]
        if len(push.encoded) != 1 + PUSH_INT_OPERAND_BYTES:
            raise ValueError(f"the character number at code offset 0x{code_offset:X} takes more than a byte")
        result.append(CharacterReference(code_offset, script.file_offset(code_offset + 1), how))
    return result


def disc_scripts(directory: Path = SCRIPT_DIRECTORY) -> list[Script]:
    """The event scripts of the disc, which the inference of arities and actor slots learns from."""
    return [load_script(path) for path in sorted(directory.glob("*.lsb"))]


def main() -> int:
    parser = argparse.ArgumentParser(description="Find where an event script names a character.")
    parser.add_argument("script", help="script name (Eve_conce01) or path to a .lsb file")
    parser.add_argument("character", type=int, help="the character number (Jamie is 2)")
    parser.add_argument("--c", action="store_true", help="print the file offsets as the body of a C array")
    parser.add_argument(
        "--directory", type=Path, default=SCRIPT_DIRECTORY, help=f"default: {SCRIPT_DIRECTORY}"
    )
    arguments = parser.parse_args()
    if not arguments.directory.is_dir():
        parser.error(f"{arguments.directory} not found; extract your disc first (tools/extract_disc.py)")
    scripts = disc_scripts(arguments.directory)
    arities = value_builtin_arities(scripts)
    slots = actor_slots(scripts, arities)
    script = load_script(resolve_script_path(arguments.script, arguments.directory))
    references = character_references(script, arguments.character, arities, slots)
    if arguments.c:
        offsets = [f"0x{reference.file_offset:04X}" for reference in references]
        print(",\n".join(", ".join(offsets[index : index + 8]) for index in range(0, len(offsets), 8)))
        return 0
    for reference in references:
        print(f"code 0x{reference.code_offset:04X}  file 0x{reference.file_offset:04X}  {reference.how}")
    print(f"{len(references)} references to character {arguments.character}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
