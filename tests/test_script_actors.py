"""Tests for tools/script_actors.py, which finds where an event script names a character.

Pruebas de tools/script_actors.py, que encuentra dónde un script de evento nombra a un personaje.

The scripts are built by hand (tests/synthetic_script.py), and the built-ins' actor slots and arities are
either given or learnt from such scripts, so no game data is needed. tests/test_jamie_pregnancy_and_birth.py
compares mods/jamie_marriage's lists with what the tool finds on the disc.
"""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))

from script_actors import (  # noqa: E402
    actor_slots,
    character_references,
    value_builtin_arities,
)
from synthetic_script import (  # noqa: E402
    HEADER_SIZE,
    OP_CALL,
    OP_CALL_BUILTIN,
    OP_CALL_BUILTIN_VALUE,
    OP_ENTER,
    OP_LOCAL_GET,
    OP_PUSH_INT,
    OP_RETURN,
    build_script,
    instruction,
    push_float,
)

from lsb import parse_script  # noqa: E402

OP_LOCAL_SET = 0x07
CHARACTER_KIND = 1
CHARACTER = 7
MOTION_BUILTIN = 79  # plays a motion on an actor: (kind, number, 0, motion)
SHOW_MESSAGE_BUILTIN = 51
SPEAKER_BUILTIN = 164
SLOTS = {MOTION_BUILTIN: {0}}


def push(value: int) -> bytes:
    return instruction(OP_PUSH_INT, value)


def handle(kind: int, number: int) -> bytes:
    return push(kind) + push(number) + push(0)


def builtin(index: int) -> bytes:
    return instruction(OP_CALL_BUILTIN, index)


def main_function(body: bytes) -> bytes:
    return instruction(OP_ENTER, 0) + body + instruction(OP_RETURN, 0)


def references_in(code: bytes, slots=None, arities=None) -> list:
    script = parse_script(build_script(code))
    return character_references(script, CHARACTER, arities or {}, SLOTS if slots is None else slots)


def test_a_characters_handle_in_an_actor_slot_is_a_reference():
    before = instruction(OP_ENTER, 0) + push(CHARACTER_KIND)
    code = before + push(CHARACTER) + push(0) + push(3) + builtin(MOTION_BUILTIN) + instruction(OP_RETURN, 0)
    references = references_in(code)
    assert [reference.code_offset for reference in references] == [len(before)]
    assert references[0].file_offset == HEADER_SIZE + len(before) + 1  # the byte after the opcode
    assert "built-in 79" in references[0].how


def test_the_same_number_as_a_motion_or_a_message_is_not_a_reference():
    code = main_function(
        handle(CHARACTER_KIND, 2)
        + push(CHARACTER)
        + builtin(MOTION_BUILTIN)  # motion 7 of character 2
        + push(CHARACTER)
        + builtin(SHOW_MESSAGE_BUILTIN)  # message 7
    )
    assert references_in(code) == []


def test_a_handle_of_another_kind_is_not_the_character():
    code = main_function(handle(5, CHARACTER) + push(0) + builtin(MOTION_BUILTIN))
    assert references_in(code) == []


def test_the_argument_of_a_function_that_reaches_an_actor_slot_makes_its_calls_references():
    # sub(number, motion): plays the motion on character `number`. LOCAL_GET -3 is the first of two
    # arguments, -2 the second; RETURN 2 releases them, as ENTER 0 reserved no locals.
    sub = (
        instruction(OP_ENTER, 0)
        + push(CHARACTER_KIND)
        + instruction(OP_LOCAL_GET, -3)
        + push(0)
        + instruction(OP_LOCAL_GET, -2)
        + builtin(MOTION_BUILTIN)
        + instruction(OP_RETURN, 2)
    )
    head = instruction(OP_ENTER, 0) + push(CHARACTER) + push(45)
    tail = instruction(OP_RETURN, 0)
    # The sub goes right after main, and a call's operand is the distance from the call to its target.
    distance_to_the_sub = len(instruction(OP_CALL, 0)) + len(tail)
    main = head + instruction(OP_CALL, distance_to_the_sub) + tail
    references = references_in(main + sub)
    assert [reference.code_offset for reference in references] == [len(instruction(OP_ENTER, 0))]
    assert "sub 0x" in references[0].how


def test_the_speaker_built_in_takes_a_bare_character_number():
    before = instruction(OP_ENTER, 0)
    code = before + push(CHARACTER) + builtin(SPEAKER_BUILTIN) + instruction(OP_RETURN, 0)
    references = references_in(code)
    assert [reference.code_offset for reference in references] == [len(before)]
    assert "speaker" in references[0].how


def test_a_number_that_takes_more_than_one_byte_is_refused():
    script = parse_script(build_script(main_function(handle(CHARACTER_KIND, 121) + push(0) + builtin(79))))
    with pytest.raises(ValueError, match="more than a byte"):
        character_references(script, 121, {}, SLOTS)


def test_actor_slots_are_the_slots_where_nearly_every_call_passes_a_handle():
    motions = [
        parse_script(build_script(main_function(handle(CHARACTER_KIND, number) + push(4) + builtin(79))))
        for number in (2, 3, 16, 34)
    ]
    # A built-in that takes an effect number and a position, as built-in 90 does on the disc.
    effects = [
        parse_script(
            build_script(main_function(push(effect) + push_float(1.5) + push_float(0.5) + builtin(90)))
        )
        for effect in (95, 34, 11, 14)
    ]
    slots = actor_slots(motions + effects, {})
    assert slots.get(79) == {0}
    assert 90 not in slots


def test_the_arity_of_a_built_in_that_returns_a_value_comes_from_a_statement_that_stores_it():
    # local0 = builtin145(4, 5): the statement leaves one value for LOCAL_SET, so 145 takes two.
    store = main_function(
        push(4) + push(5) + instruction(OP_CALL_BUILTIN_VALUE, 145) + instruction(OP_LOCAL_SET, 0)
    )
    assert value_builtin_arities([parse_script(build_script(store))]) == {145: 2}


def test_a_value_built_in_inside_a_call_uses_the_arity_learnt_elsewhere():
    # builtin79(builtin221(), 7, 0, 2): with 221 taking nothing, the handle is (result of 221, 7, 0),
    # whose kind is not a character's, so 7 is no reference; with 221 unknown, the call is not followed.
    learn = main_function(instruction(OP_CALL_BUILTIN_VALUE, 221) + instruction(OP_LOCAL_SET, 0))
    call = main_function(
        instruction(OP_CALL_BUILTIN_VALUE, 221)
        + push(CHARACTER)
        + push(0)
        + push(2)
        + builtin(MOTION_BUILTIN)
    )
    arities = value_builtin_arities([parse_script(build_script(learn))])
    assert arities == {221: 0}
    assert references_in(call, arities=arities) == []
