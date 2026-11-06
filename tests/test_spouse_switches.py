"""Tests for tools/spouse_switches.py, which finds where an event script picks the spouse's lines.

Pruebas de tools/spouse_switches.py, que encuentra dónde un script de evento elige las frases del cónyuge.

The scripts are built by hand (tests/synthetic_script.py), so no game data is needed.
tests/test_jamie_special_days.py compares mods/jamie_marriage's lists with what the tool finds on the disc.
"""

import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))

from spouse_switches import partner_bytes, show_message_sites, spouse_cases  # noqa: E402
from synthetic_script import (  # noqa: E402
    HEADER_SIZE,
    OP_CALL,
    OP_CALL_BUILTIN,
    OP_CALL_BUILTIN_VALUE,
    OP_ENTER,
    OP_JUMP,
    OP_JUMP_IF_ZERO,
    OP_LOCAL_GET,
    OP_PUSH_INT,
    OP_RETURN,
    build_script,
    instruction,
)

from lsb import parse_script  # noqa: E402

OP_LOCAL_SET = 0x07
OP_EQUAL = 0x25
OP_NOT_EQUAL = 0x26
GET_GAME_VALUE_BUILTIN = 55
SHOW_MESSAGE_BUILTIN = 51
SPOUSE_GAME_VALUE = 38


@dataclass(frozen=True)
class Label:
    """Where a jump of the code lands."""

    name: str


@dataclass(frozen=True)
class Jump:
    """A jump, or a call, to a label: its operand is the distance from the jump to the label."""

    opcode: int
    label: str


JUMP_BYTES = 2  # the opcode and a distance of one byte (-64 to 63), which every jump of these scripts has


def assemble(*parts: bytes | Label | Jump) -> bytes:
    """The code of `parts`, with the distance of each jump worked out from where its label is."""
    label_offsets = {}
    offset = 0
    for part in parts:
        if isinstance(part, Label):
            label_offsets[part.name] = offset
        else:
            offset += JUMP_BYTES if isinstance(part, Jump) else len(part)
    code = b""
    for part in parts:
        if isinstance(part, Jump):
            encoded = instruction(part.opcode, label_offsets[part.label] - len(code))
            assert len(encoded) == JUMP_BYTES, f"the jump to {part.label} is too far for this assembler"
            code += encoded
        elif not isinstance(part, Label):
            code += part
    return code


def push(value: int) -> bytes:
    return instruction(OP_PUSH_INT, value)


def read_the_spouse_into(local: int) -> bytes:
    """`local = the spouse's number`, as the scripts of the special days read it."""
    return (
        push(SPOUSE_GAME_VALUE)
        + instruction(OP_CALL_BUILTIN_VALUE, GET_GAME_VALUE_BUILTIN)
        + set_local(local)
    )


def set_local(local: int) -> bytes:
    return instruction(OP_LOCAL_SET, local)


def show(message: int) -> bytes:
    return push(message) + instruction(OP_CALL_BUILTIN, SHOW_MESSAGE_BUILTIN)


def compare(local: int, partner: int, comparison: int, label: str) -> list[bytes | Jump]:
    """`LOCAL_GET local; PUSH_INT partner; <comparison>; JUMP_IF_ZERO label`."""
    return [
        instruction(OP_LOCAL_GET, local) + push(partner) + bytes([comparison]),
        Jump(OP_JUMP_IF_ZERO, label),
    ]


def cases_of(code: bytes):
    return spouse_cases(parse_script(build_script(code)))


# main: local 0 = the spouse; a chain for partners 3 and 4, each with its message; then a message for anyone.
ONE_CHAIN = [
    instruction(OP_ENTER, 1) + read_the_spouse_into(0),
    *compare(0, 3, OP_NOT_EQUAL, "case_3"),
    *compare(0, 4, OP_NOT_EQUAL, "case_4"),
    Jump(OP_JUMP, "after_the_cases"),
    Label("case_3"),
    show(1),
    Jump(OP_JUMP, "after_the_cases"),
    Label("case_4"),
    show(2),
    Jump(OP_JUMP, "after_the_cases"),
    Label("after_the_cases"),
    show(9),
    instruction(OP_RETURN, 1),
]


def test_each_partner_of_a_chain_has_its_number_and_its_case():
    code = assemble(*ONE_CHAIN)
    cases = cases_of(code)
    assert [(case.chain, case.partner, case.messages) for case in cases] == [(0, 3, (1,)), (0, 4, (2,))]
    comparison_with_4 = code.index(instruction(OP_LOCAL_GET, 0) + push(4) + bytes([OP_NOT_EQUAL]))
    number_4 = comparison_with_4 + len(instruction(OP_LOCAL_GET, 0)) + 1  # the byte after PUSH_INT's opcode
    assert cases[1].number_file_offset == HEADER_SIZE + number_4
    assert partner_bytes(cases, 4) == [cases[1].number_file_offset]
    assert all(case.copy_file_offset is None for case in cases)


def test_a_message_after_the_cases_belongs_to_none_of_them():
    script = parse_script(build_script(assemble(*ONE_CHAIN)))
    sites = show_message_sites(script)
    assert [message for _, message in sites] == [1, 2, 9]
    offset_of_9 = sites[-1][0]
    assert not any(offset_of_9 in case.case_code for case in spouse_cases(script))


def test_an_equal_comparison_has_its_case_right_after_its_jump():
    code = assemble(
        instruction(OP_ENTER, 1) + read_the_spouse_into(0),
        *compare(0, 4, OP_EQUAL, "not_4"),
        show(2),
        Jump(OP_JUMP, "end"),
        Label("not_4"),
        show(7),
        Label("end"),
        instruction(OP_RETURN, 1),
    )
    assert [(case.partner, case.messages) for case in cases_of(code)] == [(4, (2,))]


def test_a_copy_of_the_spouse_is_a_spouse_too_and_its_number_is_one_of_the_partners_bytes():
    # local 1 = the spouse, through a chain that copies the partner's number; then a chain on local 1.
    code = assemble(
        instruction(OP_ENTER, 2) + read_the_spouse_into(0),
        *compare(0, 3, OP_NOT_EQUAL, "copy_3"),
        *compare(0, 4, OP_NOT_EQUAL, "copy_4"),
        Jump(OP_JUMP, "copied"),
        Label("copy_3"),
        push(3) + set_local(1),
        Jump(OP_JUMP, "copied"),
        Label("copy_4"),
        push(4) + set_local(1),
        Jump(OP_JUMP, "copied"),
        Label("copied"),
        *compare(1, 4, OP_NOT_EQUAL, "says_4"),
        Jump(OP_JUMP, "end"),
        Label("says_4"),
        show(24),
        Jump(OP_JUMP, "end"),
        Label("end"),
        instruction(OP_RETURN, 2),
    )
    cases = cases_of(code)
    assert [(case.chain, case.partner, case.messages) for case in cases] == [
        (0, 3, ()),
        (0, 4, ()),
        (1, 4, (24,)),
    ]
    copy = cases[1].copy_file_offset
    assert copy is not None and code[copy - HEADER_SIZE - 1 : copy - HEADER_SIZE + 2] == push(4) + bytes(
        [OP_LOCAL_SET]
    )
    assert partner_bytes(cases, 4) == sorted([cases[1].number_file_offset, copy, cases[2].number_file_offset])


def test_a_local_that_does_not_hold_the_spouse_is_no_chain():
    code = assemble(
        instruction(OP_ENTER, 2) + read_the_spouse_into(0) + push(4) + set_local(1),
        *compare(1, 4, OP_NOT_EQUAL, "case_4"),
        Jump(OP_JUMP, "end"),
        Label("case_4"),
        show(2),
        Label("end"),
        instruction(OP_RETURN, 2),
    )
    assert cases_of(code) == []


def test_the_same_local_in_another_function_is_another_variable():
    # main reads the spouse into its local 0 and calls sub, which compares its own local 0.
    code = assemble(
        instruction(OP_ENTER, 1) + read_the_spouse_into(0),
        Jump(OP_CALL, "sub"),
        instruction(OP_RETURN, 1),
        Label("sub"),
        instruction(OP_ENTER, 1),
        *compare(0, 4, OP_NOT_EQUAL, "case_4"),
        Jump(OP_JUMP, "end"),
        Label("case_4"),
        show(2),
        Label("end"),
        instruction(OP_RETURN, 1),
    )
    assert cases_of(code) == []


def test_a_number_of_more_than_one_byte_is_not_a_partner():
    # The mod writes Jamie's number over one byte, so a comparison whose number takes two is left out.
    code = assemble(
        instruction(OP_ENTER, 1) + read_the_spouse_into(0),
        *compare(0, 100, OP_NOT_EQUAL, "case_100"),
        Jump(OP_JUMP, "end"),
        Label("case_100"),
        show(2),
        Label("end"),
        instruction(OP_RETURN, 1),
    )
    assert cases_of(code) == []
