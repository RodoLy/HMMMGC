"""Tests for tools/lsb.py, the .lsb script parser and disassembler.

Pruebas de tools/lsb.py, el lector y desensamblador de scripts .lsb.

Most tests build scripts by hand (tests/synthetic_script.py) and need no game
data. The last group checks what docs/SCRIPTS.md says about the real scripts
and the real main.dol; it only runs when HM_MM_DOL points to your main.dol
(the scripts sit next to it, in orig/G4AEE9/files/script):

La mayoría de las pruebas arma scripts a mano (tests/synthetic_script.py) y no
necesita datos del juego. El último grupo comprueba lo que docs/SCRIPTS.md
afirma sobre los scripts y el main.dol reales; solo corre si HM_MM_DOL apunta
a tu main.dol (los scripts están junto a él, en orig/G4AEE9/files/script):

    HM_MM_DOL=orig/G4AEE9/sys/main.dol python3 -m pytest
"""

import os
import struct
import sys
from collections import Counter
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))

from synthetic_script import (  # noqa: E402
    FIRST_UNASSIGNED_OPCODE,
    GLOBAL_SLOT_SIZE,
    HEADER_SIZE,
    OP_ADD,
    OP_CALL,
    OP_CALL_BUILTIN,
    OP_ENTER,
    OP_JUMP,
    OP_JUMP_IF_ZERO,
    OP_LOCAL_GET,
    OP_NOP,
    OP_PUSH_INT,
    OP_RETURN,
    OP_RETURN_VOID,
    ZERO_INT_SLOT,
    build_script,
    encode_varint,
    instruction,
    push_float,
)

from dol import (
    Dol,  # noqa: E402
    write_dol,  # noqa: E402
)
from lsb import (  # noqa: E402
    BUILTIN_COUNT_ADDRESS,
    BUILTIN_NAMES,
    BUILTIN_TABLE_ADDRESS,
    Opcode,
    ScriptError,
    check_directory,
    count_builtin_usage,
    describe_builtins,
    disassemble,
    format_float,
    load_script,
    main,
    parse_script,
    read_varint,
    resolve_script_path,
)
from ppc_encode import BLR, li  # noqa: E402
from symbols import Symbol, SymbolTable  # noqa: E402

# ---------------------------------------------------------------- integers

# Operands as they sit in real scripts (listings of Eve_rm01 and talk).
REAL_INTEGER_OPERANDS = [
    (b"\x7e", -2),  # LOCAL_GET -2: the last argument of a function
    (b"\x7d", -3),
    (b"\x01", 1),
    (b"\x80\x5b", 91),  # 91 does not fit in 7 signed bits, so it takes two bytes
    (b"\x81\x66", 230),  # the number of the flag that gates the ending
    (b"\x83\x32", 434),
    (b"\x88\x5f", 1119),
    (b"\xff\x01", -127),
    (b"\x97\x38", 3000),
]


@pytest.mark.parametrize(("encoded", "value"), REAL_INTEGER_OPERANDS)
def test_integer_operands_seen_in_real_scripts(encoded, value):
    assert read_varint(encoded, 0) == (value, len(encoded))
    assert encode_varint(value) == encoded  # the hand-made encoder agrees with the game's compiler


@pytest.mark.parametrize(
    "value", [0, 1, 63, 64, -1, -64, -65, 8191, 8192, -8192, -8193, 2**20, -(2**20), 2**31 - 1, -(2**31)]
)
def test_integer_operands_round_trip_around_every_length_boundary(value):
    encoded = encode_varint(value)
    assert read_varint(encoded, 0) == (value, len(encoded))


def test_integer_is_read_from_the_given_position_and_stops_at_its_last_byte():
    assert read_varint(b"\xaa\x81\x66\xbb", 1) == (230, 3)


def test_integer_cut_off_by_the_end_of_the_data():
    with pytest.raises(ScriptError, match="cut off"):
        read_varint(b"\x81", 0)


@pytest.mark.parametrize(
    ("single", "text"),
    [(0.1, "0.1"), (1.3, "1.3"), (0.01, "0.01"), (7.5, "7.5"), (0.0, "0"), (10.0, "10"), (-2.5, "-2.5")],
)
def test_floats_are_listed_with_the_shortest_text_that_reads_back(single, text):
    stored = struct.unpack(">f", struct.pack(">f", single))[0]  # what a script can actually hold
    assert format_float(stored) == text


# ------------------------------------------------------------ the header


def test_header_fields_entries_and_empty_string_table():
    code = instruction(OP_RETURN_VOID) + instruction(OP_ENTER, 0) + instruction(OP_RETURN, 0)
    script = parse_script(build_script(code, init=None, main=1, argument=0))
    assert script.header.init_entry is None
    assert script.header.main_entry == 1
    assert script.header.argument_entry == 0
    assert script.header.global_count == 0
    assert script.header.entries == [("main", 1), ("argument", 0)]
    assert script.string_table == b""
    assert script.code_offset == HEADER_SIZE


def test_global_slots_come_before_the_code():
    script = parse_script(build_script(instruction(OP_RETURN_VOID), global_count=4))
    assert script.code_offset == HEADER_SIZE + 4 * GLOBAL_SLOT_SIZE
    assert script.global_slots == (ZERO_INT_SLOT,) * 4
    assert script.file_offset(0) == script.code_offset
    assert [item.offset for item in script.instructions] == [0]


def test_bytes_after_the_code_are_the_string_table():
    script = parse_script(build_script(instruction(OP_RETURN_VOID), string_table=b"hello\0"))
    assert script.string_table == b"hello\0"
    assert len(script.instructions) == 1


def test_file_shorter_than_the_header_is_rejected():
    with pytest.raises(ScriptError, match="header"):
        parse_script(b"\xff" * (HEADER_SIZE - 1))


def test_string_table_offset_outside_the_file_is_rejected():
    data = bytearray(build_script(instruction(OP_RETURN_VOID)))
    struct.pack_into(">I", data, 0x0C, len(data) + 1)
    with pytest.raises(ScriptError, match="string table"):
        parse_script(bytes(data))


# ---------------------------------------------------------- instructions

OPCODES_WITHOUT_OPERAND = {0x00, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x30, 0x31, 0x32} | set(range(0x1B, 0x30))
OPCODES_WITH_FLOAT_OPERAND = {0x03}


@pytest.mark.parametrize("opcode", range(FIRST_UNASSIGNED_OPCODE))
def test_every_opcode_decodes_with_the_operand_the_interpreter_reads(opcode):
    if opcode in OPCODES_WITHOUT_OPERAND:
        encoded, operand = bytes([opcode]), None
    elif opcode in OPCODES_WITH_FLOAT_OPERAND:
        encoded, operand = push_float(0.5), 0.5
    else:
        encoded, operand = instruction(opcode, 300), 300  # two bytes: 0x82 0x2C
    script = parse_script(build_script(encoded + instruction(OP_RETURN_VOID)))
    decoded = script.instructions[0]
    assert decoded.opcode == opcode
    assert decoded.operand == operand
    assert decoded.encoded == encoded
    assert script.instructions[1].offset == len(encoded)


@pytest.mark.parametrize("opcode", [FIRST_UNASSIGNED_OPCODE, 0x7F, 0xFF])
def test_opcodes_the_interpreter_does_not_know_are_rejected(opcode):
    code = instruction(OP_NOP) + bytes([opcode])
    with pytest.raises(ScriptError, match=f"0x{opcode:02X} at code offset 0x1"):
        parse_script(build_script(code))


def test_float_operand_is_four_raw_big_endian_bytes():
    script = parse_script(build_script(bytes([0x03, 0x3F, 0xC0, 0x00, 0x00])))
    assert script.instructions[0].operand == 1.5


def test_float_cut_off_by_the_end_of_the_code():
    with pytest.raises(ScriptError, match="float"):
        parse_script(build_script(bytes([0x03, 0x3F, 0xC0])))


def test_integer_operand_cut_off_by_the_end_of_the_code():
    with pytest.raises(ScriptError, match="cut off"):
        parse_script(build_script(bytes([OP_PUSH_INT, 0x81])))


# --------------------------------------------- jumps, calls and built-ins


def test_jumps_and_calls_are_measured_from_their_own_opcode():
    code = b"".join(
        [
            instruction(OP_CALL, 6),  # 0x00 -> 0x06
            instruction(OP_PUSH_INT, 1),  # 0x02
            instruction(OP_JUMP_IF_ZERO, 6),  # 0x04 -> 0x0A
            instruction(OP_ENTER, 1),  # 0x06 (the call lands here)
            instruction(OP_JUMP, -2),  # 0x08 -> 0x06
            instruction(OP_RETURN_VOID),  # 0x0A (the conditional jump lands here)
        ]
    )
    script = parse_script(build_script(code))
    by_offset = {item.offset: item for item in script.instructions}
    assert sorted(by_offset) == [0x00, 0x02, 0x04, 0x06, 0x08, 0x0A]
    assert by_offset[0x00].branch_target == 0x06
    assert by_offset[0x04].branch_target == 0x0A
    assert by_offset[0x08].branch_target == 0x06
    assert by_offset[0x02].branch_target is None
    assert script.call_targets == {0x06}
    assert script.jump_targets == {0x0A, 0x06}
    assert script.problems() == []


def test_entry_points_and_jumps_that_miss_an_instruction_are_reported():
    code = b"".join(
        [
            instruction(OP_JUMP, 3),  # 0x00 -> 0x03, inside the next instruction
            instruction(OP_PUSH_INT, 300),  # 0x02, three bytes
            instruction(OP_RETURN_VOID),  # 0x05
        ]
    )
    script = parse_script(build_script(code, main=4))
    problems = script.problems()
    assert any("main entry 0x4" in problem for problem in problems)
    assert any("JUMP at 0x0 lands on 0x3" in problem for problem in problems)
    assert len(problems) == 2


def test_builtin_calls_expose_their_index_and_known_ones_get_a_name():
    code = (
        instruction(OP_PUSH_INT, 230)
        + instruction(OP_CALL_BUILTIN, 56)
        + instruction(OP_CALL_BUILTIN, 66)
        + instruction(OP_RETURN_VOID)
    )
    script = parse_script(build_script(code))
    assert [item.builtin_index for item in script.instructions] == [None, 56, 66, None]
    calls = [line for line in disassemble(script) if "CALL_BUILTIN" in line]
    assert calls[0].endswith("CALL_BUILTIN 56  ; SET_FLAG")
    assert calls[1].endswith("CALL_BUILTIN 66")  # no evidence for a name: none given


# -------------------------------------------------------------- listing


def test_listing_shows_offsets_bytes_labels_and_operands():
    code = b"".join(
        [
            instruction(OP_CALL, 4),  # 0x00 -> 0x04
            instruction(OP_RETURN_VOID),  # 0x02
            instruction(OP_NOP),  # 0x03
            instruction(OP_ENTER, 1),  # 0x04 (the call lands here)
            instruction(OP_LOCAL_GET, -2),  # 0x06
            instruction(OP_JUMP_IF_ZERO, 7),  # 0x08 -> 0x0F
            push_float(0.1),  # 0x0A
            instruction(OP_RETURN, 1),  # 0x0F (the jump lands here)
        ]
    )
    script = parse_script(build_script(code, main=0))
    assert disassemble(script) == [
        "",
        "main:",
        "  00000  14 04           CALL sub_0004",
        "  00002  16              RETURN_VOID",
        "  00003  00              NOP",
        "",
        "sub_0004:",
        "  00004  01 01           ENTER 1",
        "  00006  06 7E           LOCAL_GET -2",
        "  00008  12 07           JUMP_IF_ZERO .L000F",
        "  0000A  03 3D CC CC CD  PUSH_FLOAT 0.1",
        ".L000F:",
        "  0000F  15 01           RETURN 1",
    ]


def test_listing_can_show_file_offsets_and_a_code_range():
    code = (
        instruction(OP_ENTER, 1) + instruction(OP_LOCAL_GET, -2) + bytes([OP_ADD]) + instruction(OP_RETURN, 1)
    )
    script = parse_script(build_script(code, main=None, global_count=1))
    listing = disassemble(script, start=2, end=5, use_file_offsets=True)  # LOCAL_GET and ADD, not the RETURN
    first_code_byte = HEADER_SIZE + GLOBAL_SLOT_SIZE
    assert listing == [
        f"  {first_code_byte + 2:05X}  06 7E           LOCAL_GET -2",
        f"  {first_code_byte + 4:05X}  1B              ADD",
    ]


# ------------------------------------------------- files and the command line


def write_script(directory: Path, name: str, data: bytes) -> Path:
    path = directory / f"{name}.lsb"
    path.write_bytes(data)
    return path


def test_script_is_found_by_name_by_name_with_extension_or_by_path(tmp_path):
    path = write_script(tmp_path, "demo", build_script(instruction(OP_RETURN_VOID)))
    assert resolve_script_path("demo", tmp_path) == path
    assert resolve_script_path("demo.lsb", tmp_path) == path
    assert resolve_script_path(str(path), tmp_path / "elsewhere") == path
    with pytest.raises(FileNotFoundError, match="missing"):
        resolve_script_path("missing", tmp_path)


def test_check_reports_every_script_that_does_not_hold_up(tmp_path, capsys):
    write_script(tmp_path, "good", build_script(instruction(OP_RETURN_VOID)))
    write_script(tmp_path, "bad_jump", build_script(instruction(OP_JUMP, 5) + instruction(OP_RETURN_VOID)))
    write_script(tmp_path, "bad_opcode", build_script(bytes([0xFF])))
    assert check_directory(tmp_path) == 1
    output = capsys.readouterr().out
    assert "bad_jump.lsb: JUMP at 0x0 lands on 0x5" in output
    assert "bad_opcode.lsb: unknown opcode 0xFF" in output
    assert "good.lsb" not in output
    assert "3 scripts checked, 2 problem(s)" in output


def test_check_passes_on_clean_scripts_and_complains_about_an_empty_folder(tmp_path, capsys):
    assert check_directory(tmp_path) == 1
    assert "no .lsb files" in capsys.readouterr().err
    write_script(tmp_path, "good", build_script(instruction(OP_RETURN_VOID)))
    assert check_directory(tmp_path) == 0
    assert "1 scripts checked, 0 problem(s)" in capsys.readouterr().out


def test_command_line_lists_a_script_by_name(tmp_path, capsys, monkeypatch):
    write_script(tmp_path, "demo", build_script(instruction(OP_ENTER, 1) + instruction(OP_RETURN, 1)))
    monkeypatch.setattr(sys, "argv", ["lsb.py", "demo", "--directory", str(tmp_path)])
    assert main() == 0
    output = capsys.readouterr().out
    assert "demo.lsb:" in output
    assert "entry points (code offsets): main 0x0" in output
    assert "ENTER 1" in output
    assert "RETURN 1" in output


def test_command_line_asks_for_a_script_when_given_nothing(capsys, monkeypatch):
    monkeypatch.setattr(sys, "argv", ["lsb.py"])
    with pytest.raises(SystemExit) as raised:
        main()
    assert raised.value.code == 2
    assert "give a script name or path" in capsys.readouterr().err


def test_builtin_listing_joins_the_native_table_symbols_and_script_usage(tmp_path):
    table = struct.pack(">3I", 0, 0x80060D14, 0x80062EC8)  # slot 0 is empty, as in the real table
    dol = Dol(
        write_dol(
            tmp_path / "main.dol",
            text={0: (0x80003100, [li(3, 1), BLR])},
            data={
                0: (BUILTIN_TABLE_ADDRESS, table),
                1: (BUILTIN_COUNT_ADDRESS, struct.pack(">I", 3)),
            },
            entry=0x80003100,
        )
    )
    symbols = SymbolTable([Symbol("fn_80060D14", ".text", 0x80060D14, "function", 0x80)])
    scripts = tmp_path / "scripts"
    scripts.mkdir()
    call_twice = (
        instruction(OP_CALL_BUILTIN, 1) + instruction(OP_CALL_BUILTIN, 1) + instruction(OP_RETURN_VOID)
    )
    write_script(scripts, "a", build_script(call_twice))
    write_script(scripts, "b", build_script(instruction(OP_CALL_BUILTIN, 1) + instruction(OP_RETURN_VOID)))

    assert count_builtin_usage(scripts) == {1: (3, 2)}
    lines = describe_builtins(dol, symbols, scripts)
    assert lines[0] == "; 3 slots at 0x80219EAC; empty slots are left out"
    assert lines[1].split() == ["1", "0x80060D14", "fn_80060D14", "3", "calls", "in", "2", "scripts"]
    assert lines[2].split()[:3] == ["2", "0x80062EC8", "?"]  # not in the symbol table
    assert len(lines) == 3


# ------------------------------------------- the real scripts and main.dol

DOL_PATH = os.environ.get("HM_MM_DOL")
REAL_SCRIPT_DIRECTORY = Path(DOL_PATH).parent.parent / "files" / "script" if DOL_PATH else None
needs_game_files = pytest.mark.skipif(
    REAL_SCRIPT_DIRECTORY is None or not REAL_SCRIPT_DIRECTORY.is_dir(),
    reason="set HM_MM_DOL to your extracted main.dol (the scripts are in ../files/script next to it)",
)

# Known-good facts for G4AEE9, the ones docs/SCRIPTS.md builds on. If a
# change to the parser alters one, update it here on purpose and say why.
EXPECTED_SCRIPT_COUNT = 291
EXPECTED_SCRIPTS_WITH_ARGUMENT_ENTRY = 67
EXPECTED_GLOBAL_COUNTS = {0: 204, 1: 85, 4: 2}
EXPECTED_DISTINCT_BUILTINS_CALLED = 160
EXPECTED_HIGHEST_BUILTIN_CALLED = 221
EXPECTED_BUILTIN_SLOTS = 230
EXPECTED_FILLED_BUILTIN_SLOTS = 188
# Opcodes that no shipped script contains (the compiler never emits them).
EXPECTED_UNUSED_OPCODES = {
    "NOP",
    "PUSH_STRING",
    "UNKNOWN_0D",
    "UNKNOWN_0E",
    "UNKNOWN_0F",
    "UNKNOWN_10",
    "CALL_VALUE",
    "NEW_ARRAY",
    "ARRAY_GET",
    "ARRAY_SET",
    "PUSH_RESULT",
    "MODULO",
    "SHIFT_LEFT",
    "SHIFT_RIGHT",
    "BIT_AND",
    "BIT_XOR",
    "LOGICAL_OR",
    "NEGATE",
    "BIT_NOT",
    "PRINT",
    "PRINT_LINE",
    "NOP_32",
    "LINE",
}
# Native functions behind the built-ins that BUILTIN_NAMES names, read from
# the table at 0x80219EAC (docs/SCRIPTS.md explains what each one does).
EXPECTED_NAMED_BUILTIN_FUNCTIONS = {
    6: 0x800631A0,
    48: 0x80062EC8,
    51: 0x800629D0,
    55: 0x80060D94,
    56: 0x80060D14,
    57: 0x80060C94,
    58: 0x80060BE8,
    62: 0x800627C0,
}
ENDING_GATE_FLAG = 230  # main flag 0xE6, tested by the native field check before the ending sequence


@pytest.fixture(scope="module")
def real_scripts():
    return {path.stem: load_script(path) for path in sorted(REAL_SCRIPT_DIRECTORY.glob("*.lsb"))}


def find_sequence(script, steps):
    """Code offsets where the (opcode, operand) steps appear one right after another."""
    instructions = script.instructions
    return [
        instructions[start].offset
        for start in range(len(instructions) - len(steps) + 1)
        if all(
            instructions[start + index].opcode is opcode
            and (operand is None or instructions[start + index].operand == operand)
            for index, (opcode, operand) in enumerate(steps)
        )
    ]


@needs_game_files
def test_every_real_script_parses_and_every_target_is_an_instruction(real_scripts):
    assert len(real_scripts) == EXPECTED_SCRIPT_COUNT
    assert {name: script.problems() for name, script in real_scripts.items() if script.problems()} == {}


@needs_game_files
def test_real_headers_follow_the_documented_layout(real_scripts):
    scripts = list(real_scripts.values())
    assert all(script.header.init_entry is None for script in scripts)
    assert all(script.header.main_entry is not None for script in scripts)
    assert all(script.string_table == b"" for script in scripts)
    assert (
        sum(script.header.argument_entry is not None for script in scripts)
        == EXPECTED_SCRIPTS_WITH_ARGUMENT_ENTRY
    )
    assert dict(Counter(script.header.global_count for script in scripts)) == EXPECTED_GLOBAL_COUNTS
    assert all(slot == ZERO_INT_SLOT for script in scripts for slot in script.global_slots)


@needs_game_files
def test_real_scripts_use_the_documented_subset_of_opcodes(real_scripts):
    used = {item.opcode.name for script in real_scripts.values() for item in script.instructions}
    assert {opcode.name for opcode in Opcode} - used == EXPECTED_UNUSED_OPCODES


@needs_game_files
def test_real_scripts_call_the_documented_number_of_builtins():
    usage = count_builtin_usage(REAL_SCRIPT_DIRECTORY)
    assert len(usage) == EXPECTED_DISTINCT_BUILTINS_CALLED
    assert max(usage) == EXPECTED_HIGHEST_BUILTIN_CALLED


@needs_game_files
def test_subtraction_pushes_the_right_operand_first(real_scripts):
    # `variable - constant` is written PUSH_INT, LOCAL_GET, SUBTRACT because the
    # interpreter computes TOP op SECOND; the reverse order is the rarer `constant - variable`.
    steps_x_minus_constant = [(Opcode.PUSH_INT, None), (Opcode.LOCAL_GET, None), (Opcode.SUBTRACT, None)]
    steps_constant_minus_x = [(Opcode.LOCAL_GET, None), (Opcode.PUSH_INT, None), (Opcode.SUBTRACT, None)]
    common = sum(len(find_sequence(script, steps_x_minus_constant)) for script in real_scripts.values())
    rare = sum(len(find_sequence(script, steps_constant_minus_x)) for script in real_scripts.values())
    assert common > 2 * rare


@needs_game_files
def test_flag_230_is_set_and_cleared_only_by_talk(real_scripts):
    set_flag = [(Opcode.PUSH_INT, ENDING_GATE_FLAG), (Opcode.CALL_BUILTIN, 56)]
    clear_flag = [(Opcode.PUSH_INT, ENDING_GATE_FLAG), (Opcode.CALL_BUILTIN, 57)]
    test_flag = [(Opcode.PUSH_INT, ENDING_GATE_FLAG), (Opcode.CALL_BUILTIN_VALUE, 58)]
    sites = {
        kind: {
            name: found for name, script in real_scripts.items() if (found := find_sequence(script, steps))
        }
        for kind, steps in (("set", set_flag), ("clear", clear_flag), ("test", test_flag))
    }
    assert sites == {"set": {"talk": [0xC0B]}, "clear": {"talk": [0xCA6, 0xCDE]}, "test": {}}


@needs_game_files
def test_wedding_scripts_end_with_the_card_then_the_closing_calls(real_scripts):
    # PUSH_INT 18 (message number), SHOW_MESSAGE, WAIT_MESSAGE: the "Several Seasons Later..." card.
    card = [(Opcode.PUSH_INT, 18), (Opcode.CALL_BUILTIN, 51), (Opcode.CALL_BUILTIN, 62)]
    assert find_sequence(real_scripts["Eve_rm01"], card) == [0x1C40]
    assert find_sequence(real_scripts["Eve_rm02"], card) == [0x1C1C]
    for name, size in (("Eve_rm01", 7307), ("Eve_rm02", 7271)):
        script = real_scripts[name]
        assert script.file_size == size
        assert script.header.main_entry == 0xC9F
        last = script.instructions[-1]
        assert (last.opcode, last.operand) == (Opcode.RETURN, 9)  # main releases its 9 locals


@needs_game_files
def test_builtin_names_match_the_native_table_in_main_dol():
    dol = Dol(Path(DOL_PATH))
    assert dol.u32(BUILTIN_COUNT_ADDRESS) == EXPECTED_BUILTIN_SLOTS
    functions = [dol.u32(BUILTIN_TABLE_ADDRESS + 4 * index) for index in range(EXPECTED_BUILTIN_SLOTS)]
    assert sum(address != 0 for address in functions) == EXPECTED_FILLED_BUILTIN_SLOTS
    assert set(BUILTIN_NAMES) == set(EXPECTED_NAMED_BUILTIN_FUNCTIONS)
    for index, address in EXPECTED_NAMED_BUILTIN_FUNCTIONS.items():
        assert functions[index] == address, BUILTIN_NAMES[index]
