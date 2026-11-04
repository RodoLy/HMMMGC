"""Tests of the program that checks every instruction form the game uses. No game data needed.

Pruebas del programa que comprueba cada forma de instrucción que usa el juego.
Sin datos del juego.

The "game" is a handful of hand-written functions in a DOL built for the test; the
verifier's log is written by hand in the format the real one prints.
"""

import argparse
import random
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from dol import Dol, write_dol  # noqa: E402
from isa_conformance import (  # noqa: E402
    CODE_ADDRESS,
    DISPLACEMENT_OPERATIONS,
    DONE_ADDRESS,
    DONE_VALUE,
    INDEXED_OPERATIONS,
    R0_OPERAND,
    SCRATCH_ADDRESS,
    SCRATCH_CENTER,
    SCRATCH_SIZE,
    SKIPPED_OPERATIONS,
    STATE_FPRS,
    STATE_SIZE,
    STATE_TABLE_ADDRESS,
    XER_MASK,
    Candidate,
    Case,
    FloatState,
    FormResult,
    Treatment,
    build_program,
    classify,
    collect_candidates,
    deal_by_register_pattern,
    existing_file,
    field_ra,
    field_rb,
    field_rd,
    float_state_of,
    format_cases,
    format_report,
    load_constant,
    main,
    parse_cases,
    random_state,
    register_pattern,
    setup_before,
    spr_number,
    state_setup_code,
    treatment_of,
)
from ppc import Instruction, format_instruction, sign_extend  # noqa: E402
from ppc_encode import BLR, b, lfd, lis, lmw, lwz, lwzx, mtfsf, mtspr, stw  # noqa: E402
from ppc_mnemonics import (  # noqa: E402
    ILLEGAL_FORM,
    OPCODE_4_FIVE_BIT_NAMES,
    OPCODE_4_SIX_BIT_NAMES,
    OPCODE_4_TEN_BIT_NAMES,
    OPCODE_19_NAMES,
    OPCODE_31_NAMES,
    OPCODE_59_NAMES,
    OPCODE_63_FIVE_BIT_NAMES,
    OPCODE_63_TEN_BIT_NAMES,
    PRIMARY_OPCODE_NAMES,
    form_name,
)
from symbols import Symbol, SymbolTable  # noqa: E402

SPR_XER = 1
SPR_LR = 8
SPR_CTR = 9
SPR_DEC = 22  # the decrementer: reading or writing it would change the program's timing

ADD = 0x7C642A14  # add r3,r4,r5
FADD = 0xFC22182A  # fadd f1,f2,f3
FADDS = 0xEC22182A  # fadds f1,f2,f3
PS_ADD = 0x1022182A  # ps_add f1,f2,f3
LFD = 0xC8240008  # lfd f1,8(r4)
STFS = 0xD0240008  # stfs f1,8(r4)


def mfspr(rd: int, spr: int) -> int:
    """mfspr rD, SPR; like mtspr, the instruction stores the two halves of the number swapped."""
    swapped_spr = ((spr & 0x1F) << 5) | (spr >> 5)
    return (31 << 26) | (rd << 21) | (swapped_spr << 11) | (339 << 1)


def text_of(word: int) -> str:
    return " ".join(format_instruction(Instruction(CODE_ADDRESS, word)).split())


def known_operations() -> set[str]:
    names = set(PRIMARY_OPCODE_NAMES.values())
    for table in (
        OPCODE_4_FIVE_BIT_NAMES,
        OPCODE_4_SIX_BIT_NAMES,
        OPCODE_4_TEN_BIT_NAMES,
        OPCODE_19_NAMES,
        OPCODE_31_NAMES,
        OPCODE_59_NAMES,
        OPCODE_63_FIVE_BIT_NAMES,
        OPCODE_63_TEN_BIT_NAMES,
    ):
        names |= set(table.values())
    return names


# -- What each instruction needs ---------------------------------------------------------------


@pytest.mark.parametrize(
    ("operation", "treatment"),
    [
        ("add", Treatment.REGISTERS),
        ("fmadd", Treatment.REGISTERS),
        ("lwz", Treatment.DISPLACEMENT),
        ("psq_stu", Treatment.DISPLACEMENT),
        ("lwzx", Treatment.INDEXED),
        ("dcbz", Treatment.INDEXED),
        ("mfspr", Treatment.SPECIAL_REGISTER),
        ("mtspr", Treatment.SPECIAL_REGISTER),
        ("bclr", Treatment.SKIPPED),
        ("sc", Treatment.SKIPPED),
        ("mtmsr", Treatment.SKIPPED),
        (ILLEGAL_FORM, Treatment.SKIPPED),
    ],
)
def test_each_operation_is_prepared_in_the_way_it_needs(operation, treatment):
    assert treatment_of(operation) is treatment


def test_the_operation_sets_name_only_instructions_the_tables_know():
    # A typo in one of these sets would silently send an instruction down the wrong path.
    for names in (DISPLACEMENT_OPERATIONS, INDEXED_OPERATIONS, SKIPPED_OPERATIONS):
        assert names <= known_operations()


def test_no_operation_is_in_two_sets():
    assert not DISPLACEMENT_OPERATIONS & INDEXED_OPERATIONS
    assert not DISPLACEMENT_OPERATIONS & SKIPPED_OPERATIONS
    assert not INDEXED_OPERATIONS & SKIPPED_OPERATIONS


@pytest.mark.parametrize(
    ("operation", "float_state"),
    [
        ("add", FloatState.NONE),
        ("lwz", FloatState.NONE),
        ("rlwinm", FloatState.NONE),
        ("mfspr", FloatState.NONE),
        ("fadd", FloatState.DOUBLES),
        ("lfd", FloatState.DOUBLES),
        ("stfs", FloatState.DOUBLES),
        ("frsp", FloatState.DOUBLES),
        ("mffs", FloatState.DOUBLES),
        ("fadds", FloatState.SINGLES),
        ("fres", FloatState.SINGLES),
        ("ps_add", FloatState.SINGLES),
        ("psq_l", FloatState.SINGLES),
    ],
)
def test_floating_point_registers_hold_what_the_instruction_is_defined_for(operation, float_state):
    assert float_state_of(operation) is float_state


# -- Reading fields ----------------------------------------------------------------------------


def test_register_fields_are_read_where_the_manual_puts_them():
    word = lwzx(3, 4, 5)
    assert (field_rd(word), field_ra(word), field_rb(word)) == (3, 4, 5)


@pytest.mark.parametrize("spr", [SPR_XER, SPR_LR, SPR_CTR, 272, 275, 912, 919, 920])
def test_the_number_of_a_special_register_is_unswapped(spr):
    assert spr_number(mtspr(spr, 7)) == spr
    assert spr_number(mfspr(7, spr)) == spr
    assert field_rd(mtspr(spr, 7)) == 7


def test_a_constant_is_loaded_with_a_lis_and_an_ori():
    assert [text_of(word) for word in load_constant(5, 0x12345678)] == [
        "lis r5, 0x1234",
        "ori r5, r5, 0x5678",
    ]


# -- Making a word safe to run -----------------------------------------------------------------


def test_instructions_that_touch_only_registers_need_no_set_up():
    assert setup_before(ADD, random.Random(0)) == []
    assert setup_before(FADD, random.Random(0)) == []


def test_a_load_gets_its_base_register_pointed_at_the_scratch_memory():
    assert setup_before(lwz(3, 8, 4), random.Random(0)) == load_constant(4, SCRATCH_CENTER)


def test_a_load_with_no_base_register_would_address_the_bottom_of_memory_so_it_is_not_run():
    assert setup_before(lwz(3, 8, 0), random.Random(0)) is None


def test_lmw_is_not_run_when_it_would_overwrite_its_own_base_register():
    assert setup_before(lmw(5, 0, 30), random.Random(0)) is None  # r5-r31 includes r30
    assert setup_before(lmw(28, 0, 3), random.Random(0)) == load_constant(3, SCRATCH_CENTER)


def test_an_indexed_access_gets_a_base_and_a_small_aligned_index():
    for seed in range(20):
        setup = setup_before(lwzx(3, 4, 5), random.Random(seed))
        assert setup[:2] == load_constant(4, SCRATCH_CENTER)
        assert text_of(setup[2]).startswith("li r5, ")
        index = sign_extend(setup[2] & 0xFFFF, 16)
        assert index % 8 == 0
        assert abs(index) <= 0x100


def test_an_indexed_access_without_a_base_register_uses_the_index_register_alone():
    assert setup_before(lwzx(3, 0, 5), random.Random(0)) == load_constant(5, SCRATCH_CENTER)


def test_an_indexed_access_is_not_run_when_the_two_registers_are_the_same():
    assert setup_before(lwzx(3, 4, 4), random.Random(0)) is None


def test_an_update_form_is_not_run_when_the_destination_is_the_base():
    lwzux_r4_r4_r5 = (31 << 26) | (4 << 21) | (4 << 16) | (5 << 11) | (55 << 1)
    assert form_name(lwzux_r4_r4_r5) == "lwzux"
    assert setup_before(lwzux_r4_r4_r5, random.Random(0)) is None


@pytest.mark.parametrize(
    ("form", "extended_opcode"),
    [("stwux", 183), ("lfsux", 567)],
    ids=["a store: the old value is stored", "a float load: f4 is not r4"],
)
def test_an_update_form_runs_when_its_data_register_has_the_number_of_the_base_but_is_not_the_base(
    form, extended_opcode
):
    word = (31 << 26) | (4 << 21) | (4 << 16) | (5 << 11) | (extended_opcode << 1)
    assert form_name(word) == form
    setup = setup_before(word, random.Random(0))
    assert setup is not None
    assert setup[:2] == load_constant(4, SCRATCH_CENTER)


@pytest.mark.parametrize("spr", [SPR_XER, SPR_LR, SPR_CTR, 912])
def test_the_special_registers_a_program_can_read_are_read(spr):
    assert setup_before(mfspr(3, spr), random.Random(0)) == []


def test_a_special_register_that_would_change_the_program_is_not_read():
    assert setup_before(mfspr(3, SPR_DEC), random.Random(0)) is None


def test_the_link_register_is_not_written_because_every_case_returns_through_it():
    assert setup_before(mtspr(SPR_LR, 3), random.Random(0)) is None
    assert setup_before(mtspr(SPR_CTR, 3), random.Random(0)) == []


def test_xer_is_only_given_values_whose_bits_are_all_defined():
    # The interpreter clears the reserved bits of a write; the recompiled code keeps them.
    for seed in range(200):
        high_half_load, low_half_or = setup_before(mtspr(SPR_XER, 6), random.Random(seed))
        assert (form_name(high_half_load), form_name(low_half_or)) == ("addis", "ori")
        value = ((high_half_load & 0xFFFF) << 16) | (low_half_or & 0xFFFF)
        assert value & ~XER_MASK == 0


MTFSF_F0 = 0xFDFE058E  # mtfsf 0xff, f0: the words of the game that write the FPSCR from a register
MTFSF_F31 = 0xFDFEFD8E
MFFS_F0 = 0xFC00048E
MTFSB1_29 = 0xFFA0004C  # sets the "non-IEEE" bit; the bit to set is in the word, not in a register
FPSCR_RESERVED_BIT_MASK = 0x00000800  # bit 20 of the manual, which counts from the most significant


def result_of_rlwinm(word: int, value: int) -> int:
    """What an rlwinm word does to a value, from the manual's definition of its mask."""
    shift, mask_begin, mask_end = (word >> 11) & 0x1F, (word >> 6) & 0x1F, (word >> 1) & 0x1F
    rotated = ((value << shift) | (value >> (32 - shift))) & 0xFFFFFFFF
    ones_from_begin = 0xFFFFFFFF >> mask_begin
    ones_up_to_end = (0xFFFFFFFF << (31 - mask_end)) & 0xFFFFFFFF
    wraps_around = mask_begin > mask_end
    mask = ones_from_begin | ones_up_to_end if wraps_around else ones_from_begin & ones_up_to_end
    return rotated & mask


@pytest.mark.parametrize(("word", "register"), [(MTFSF_F0, 0), (MTFSF_F31, 31), (mtfsf(0xFF, 7), 7)])
def test_the_register_an_mtfsf_copies_from_is_stored_changed_and_loaded_again(word, register):
    setup = setup_before(word, random.Random(0))
    assert [text_of(word) for word in setup[2:]] == [
        f"stfd f{register}, 0(r3)",
        "lwz r0, 4(r3)",  # the low word, which is the half mtfsf reads
        "rlwinm r0, r0, 0, 0x15, 0x13",
        "stw r0, 4(r3)",
        f"lfd f{register}, 0(r3)",
    ]
    assert setup[:2] == load_constant(3, SCRATCH_CENTER)


def test_the_mask_of_that_rlwinm_clears_the_reserved_fpscr_bit_and_nothing_else():
    rlwinm_word = setup_before(MTFSF_F0, random.Random(0))[4]
    assert form_name(rlwinm_word) == "rlwinm"
    rng = random.Random(1)
    for value in [0, 0xFFFFFFFF, FPSCR_RESERVED_BIT_MASK, *(rng.getrandbits(32) for _ in range(200))]:
        assert result_of_rlwinm(rlwinm_word, value) == value & ~FPSCR_RESERVED_BIT_MASK


@pytest.mark.parametrize("word", [MFFS_F0, MTFSB1_29], ids=["mffs", "mtfsb1"])
def test_the_other_instructions_that_touch_the_fpscr_need_no_set_up(word):
    assert setup_before(word, random.Random(0)) == []


def test_jumps_and_traps_are_never_made_safe():
    assert setup_before(BLR, random.Random(0)) is None
    assert setup_before(b(CODE_ADDRESS, CODE_ADDRESS + 8), random.Random(0)) is None


# -- The game's words --------------------------------------------------------------------------

FIRST_FUNCTION = 0x80003100
SECOND_FUNCTION = 0x80003110
GAME_FUNCTIONS = {
    FIRST_FUNCTION: [ADD, lwz(3, 8, 4), ADD, BLR],  # the same add twice: it must be listed once
    SECOND_FUNCTION: [FADD, 0x00000000, BLR],  # a padding word is not an instruction
}


@pytest.fixture
def game(tmp_path):
    words = [0] * ((SECOND_FUNCTION + 12 - FIRST_FUNCTION) // 4)
    for start, function_words in GAME_FUNCTIONS.items():
        first = (start - FIRST_FUNCTION) // 4
        words[first : first + len(function_words)] = function_words
    dol = Dol(
        write_dol(tmp_path / "game.dol", text={0: (FIRST_FUNCTION, words)}, data={}, entry=FIRST_FUNCTION)
    )
    symbols = SymbolTable(
        [
            Symbol("first", ".text", FIRST_FUNCTION, "function", 16),
            Symbol("second", ".text", SECOND_FUNCTION, "function", 12),
            Symbol("a_table", ".data", 0x80004000, "object", 8),  # not a function: ignored
        ]
    )
    return dol, symbols


def test_every_distinct_word_of_the_game_is_a_candidate_listed_under_its_form(game):
    candidates = collect_candidates(*game)
    assert set(candidates) == {"add", "lwz", "bclr", "fadd"}
    assert candidates["add"] == [Candidate(ADD, FIRST_FUNCTION)]  # once, at its first place
    assert candidates["lwz"] == [Candidate(lwz(3, 8, 4), FIRST_FUNCTION + 4)]


def test_words_that_are_not_instructions_are_not_candidates(game):
    candidates = collect_candidates(*game)
    assert ILLEGAL_FORM not in candidates
    assert all(candidate.word != 0 for words in candidates.values() for candidate in words)


# -- Random machine states ---------------------------------------------------------------------


def test_a_machine_state_has_the_size_the_program_expects():
    assert len(random_state(random.Random(1))) == STATE_SIZE


def test_the_same_seed_gives_the_same_state_and_another_seed_a_different_one():
    assert random_state(random.Random(1)) == random_state(random.Random(1))
    assert random_state(random.Random(1)) != random_state(random.Random(2))


def test_a_machine_state_only_sets_defined_bits_of_xer_and_the_rounding_mode():
    for seed in range(50):
        row = random_state(random.Random(seed))
        xer = int.from_bytes(row[0x84:0x88], "big")
        fpscr = int.from_bytes(row[0x88:0x90], "big")
        assert xer & ~XER_MASK == 0
        assert fpscr <= 3  # the rounding mode only: no exception can be enabled


def test_the_integer_set_up_does_not_touch_the_floating_point_registers():
    texts = [text_of(word) for word in state_setup_code(STATE_TABLE_ADDRESS, FloatState.NONE)]
    assert not any(text.startswith(("lfd", "psq_l", "mtfsf")) for text in texts)
    assert texts[-1].startswith("lmw")  # r3-r31 come last: r0 and r1 were scratch before


@pytest.mark.parametrize(
    ("float_state", "doubles_loaded"), [(FloatState.SINGLES, 1), (FloatState.DOUBLES, 1 + 32)]
)
def test_the_floating_point_set_up_loads_pairs_and_for_doubles_also_a_double(float_state, doubles_loaded):
    texts = [text_of(word) for word in state_setup_code(STATE_TABLE_ADDRESS, float_state)]
    assert sum(text.startswith("psq_l") for text in texts) == 32
    assert sum(text.startswith("lfd") for text in texts) == doubles_loaded  # one is the FPSCR
    assert sum(text.startswith("mtfsf") for text in texts) == 1
    assert STATE_FPRS == 0x90  # the layout random_state writes and the code reads


# -- The program -------------------------------------------------------------------------------


def candidate(word: int, address: int = 0x80100000) -> Candidate:
    return Candidate(word, address)


CANDIDATES = {
    "add": [candidate(ADD)],
    "lwz": [candidate(lwz(3, 8, 4)), candidate(lwz(5, -16, 6))],
    "lwzx": [candidate(lwzx(3, 4, 5))],
    "fadd": [candidate(FADD)],
    "fadds": [candidate(FADDS)],
    "ps_add": [candidate(PS_ADD)],
    "bclr": [candidate(BLR)],  # control flow: not run
}


def branch_target(address: int, word: int) -> int:
    target = Instruction(address, word).branch_target()
    assert target is not None
    return target


def test_every_runnable_form_gets_cases_and_the_others_say_why_not():
    program = build_program(CANDIDATES, seed=1, words_per_form=12, group_size=4)
    assert {case.form for case in program.cases} == {"add", "lwz", "lwzx", "fadd", "fadds", "ps_add"}
    assert set(program.skipped_forms) == {"bclr"}
    assert len([case for case in program.cases if case.form == "lwz"]) == 2


def test_no_more_words_per_form_than_asked_for_are_run():
    program = build_program(CANDIDATES, seed=1, words_per_form=1, group_size=4)
    assert len([case for case in program.cases if case.form == "lwz"]) == 1


# -- Choosing the words ------------------------------------------------------------------------

ADD_R3_R3_R5 = 0x7C632A14  # add r3,r3,r5: the destination is also a source
ADD_R3_R0_R5 = 0x7C602A14  # add r3,r0,r5
FADD_F1_F1_F1 = (63 << 26) | (1 << 21) | (1 << 16) | (1 << 11) | (21 << 1)


@pytest.mark.parametrize(
    ("word", "pattern"),
    [
        (ADD, (0, 1, 2)),
        (ADD_R3_R3_R5, (0, 0, 2)),
        (ADD_R3_R0_R5, (0, R0_OPERAND, 2)),
        (lwz(3, 8, 4), (0, 1)),
        (lwz(3, 8, 3), (0, 0)),
        (lwz(3, 8, 0), (0,)),  # no base register: the disassembly shows a plain 0, not r0
        (FADD_F1_F1_F1, (0, 0, 0)),
        (lfd(3, 8, 3), (0, 1)),  # f3 and r3 are different registers
    ],
    ids=["distinct", "destination is a source", "r0 as a source", "load", "load through itself",
         "load without base", "same float register", "float and integer register"],
)  # fmt: skip
def test_a_register_pattern_says_which_operands_name_the_same_register(word, pattern):
    assert register_pattern(word) == pattern


def test_dealing_by_pattern_shows_every_pattern_before_repeating_one():
    ordinary = [candidate(lwz(3, 8, base)) for base in range(4, 9)]  # five words of one pattern
    through_itself = candidate(lwz(3, 8, 3))
    without_base = candidate(lwz(3, 8, 0))
    for seed in range(20):
        dealt = deal_by_register_pattern([*ordinary, through_itself, without_base], random.Random(seed))
        assert len(dealt) == 7  # all of them, in some order
        assert {register_pattern(entry.word) for entry in dealt[:3]} == {(0, 1), (0, 0), (0,)}


def test_dealing_by_pattern_gives_the_same_order_for_the_same_seed():
    words = [candidate(lwz(3, 8, base)) for base in range(4, 12)] + [candidate(lwz(3, 8, 3))]
    first = deal_by_register_pattern(words, random.Random(7))
    assert first == deal_by_register_pattern(words, random.Random(7))


def test_a_word_that_aliases_its_registers_is_run_even_among_many_ordinary_ones():
    aliasing = lwz(3, 8, 3)
    candidates = {"lwz": [*(candidate(lwz(3, 8, base)) for base in range(4, 24)), candidate(aliasing)]}
    for seed in range(20):
        program = build_program(candidates, seed=seed, words_per_form=2, group_size=4)
        assert aliasing in {case.word for case in program.cases}


def test_a_form_whose_words_cannot_be_made_safe_is_reported_not_dropped_silently():
    program = build_program({"lwz": [candidate(lwz(3, 8, 0))]}, seed=1, words_per_form=12, group_size=4)
    assert program.cases == []
    assert "safely" in program.skipped_forms["lwz"]


def test_each_case_is_its_setup_then_the_instruction_then_a_return():
    program = build_program(CANDIDATES, seed=1, words_per_form=12, group_size=4)
    words = {CODE_ADDRESS + 4 * index: word for index, word in enumerate(program.text_words)}
    for case in program.cases:
        assert words[case.word_address] == case.word
        assert words[case.word_address + 4] == BLR
        assert case.word_address >= case.address


def test_the_cases_touch_each_other_because_a_gap_would_hide_them_from_the_verifier():
    program = build_program(CANDIDATES, seed=1, words_per_form=12, group_size=4)
    for previous, following in zip(program.cases, program.cases[1:], strict=False):
        assert following.address == previous.word_address + 8  # the instruction and the blr


def test_main_calls_every_case_once_in_order():
    program = build_program(CANDIDATES, seed=1, words_per_form=12, group_size=4)
    calls = [
        branch_target(CODE_ADDRESS + 4 * index, word)
        for index, word in enumerate(program.text_words)
        if form_name(word) == "bl"
    ]
    assert calls == [case.address for case in program.cases]


def test_main_ends_by_writing_the_marker_and_then_loops_on_one_branch():
    program = build_program(CANDIDATES, seed=1, words_per_form=12, group_size=4)
    end_of_main = (program.cases[0].address - CODE_ADDRESS) // 4  # the first case starts where main ends
    last_address = CODE_ADDRESS + 4 * (end_of_main - 1)
    assert branch_target(last_address, program.text_words[end_of_main - 1]) == last_address
    assert program.text_words[end_of_main - 5 : end_of_main - 1] == [
        *load_constant(3, DONE_VALUE),
        lis(4, DONE_ADDRESS >> 16),
        stw(3, 0, 4),
    ]
    assert DONE_ADDRESS & 0xFFFF == 0  # one lis is enough to address it


def test_cases_that_want_the_same_kind_of_floating_point_registers_share_a_state():
    program = build_program(CANDIDATES, seed=1, words_per_form=12, group_size=100)
    forms_in_order = [case.form for case in program.cases]
    assert forms_in_order.index("add") < forms_in_order.index("fadds") < forms_in_order.index("fadd")


def test_the_same_seed_builds_the_same_program_and_another_seed_a_different_one():
    first = build_program(CANDIDATES, seed=1, words_per_form=12, group_size=4)
    again = build_program(CANDIDATES, seed=1, words_per_form=12, group_size=4)
    other = build_program(CANDIDATES, seed=2, words_per_form=12, group_size=4)
    assert first.text_words == again.text_words
    assert first.data_sections == again.data_sections
    assert first.data_sections != other.data_sections


def test_the_program_is_a_dol_with_the_code_the_state_table_the_scratch_memory_and_the_marker(tmp_path):
    program = build_program(CANDIDATES, seed=1, words_per_form=12, group_size=4)
    dol = Dol(program.write(tmp_path / "main.dol"))
    assert dol.u32(CODE_ADDRESS) == program.text_words[0]
    assert dol.u32(CODE_ADDRESS + 4 * (len(program.text_words) - 1)) == program.text_words[-1]
    assert dol.is_mapped(STATE_TABLE_ADDRESS) and dol.is_mapped(STATE_TABLE_ADDRESS + STATE_SIZE - 1)
    assert dol.is_mapped(SCRATCH_ADDRESS) and dol.is_mapped(SCRATCH_ADDRESS + SCRATCH_SIZE - 1)
    assert dol.u32(DONE_ADDRESS) == 0  # the program writes DONE_VALUE here when it is finished
    assert dol.entry == CODE_ADDRESS


# -- The list of cases -------------------------------------------------------------------------


def test_the_list_of_cases_is_written_and_read_back_unchanged():
    program = build_program(CANDIDATES, seed=1, words_per_form=12, group_size=4)
    text = format_cases(program.cases)
    assert parse_cases(text) == program.cases
    assert "add r3, r4, r5" in text  # a person can read what each case is


# -- Report ------------------------------------------------------------------------------------

EQUAL_CASE = Case(address=0x80010000, word_address=0x80010000, form="add", word=ADD, game_address=0)
DIVERGED_CASE = Case(address=0x80010008, word_address=0x80010008, form="fadd", word=FADD, game_address=0)
SKIPPED_CASE = Case(address=0x80010010, word_address=0x80010010, form="dcbf", word=0x7C0018AC, game_address=0)
CASES = [EQUAL_CASE, DIVERGED_CASE, SKIPPED_CASE]


def test_a_case_is_equal_diverged_or_not_compared_and_divergence_wins():
    results = classify(CASES, {EQUAL_CASE.word_address, DIVERGED_CASE.word_address}, {DIVERGED_CASE.address})
    assert results["add"] == FormResult(cases=1, compared=1)
    assert results["fadd"] == FormResult(cases=1, diverged=1)  # also verified, but it diverged
    assert results["dcbf"] == FormResult(cases=1, not_compared=1)


def test_the_report_counts_cases_and_names_the_forms_with_a_problem():
    results = classify(CASES, {EQUAL_CASE.word_address}, {DIVERGED_CASE.address})
    report = format_report(results, ["entry=0x80010008  fadd: fadd f1, f2, f3  [f1:N=0x1,I=0x2]"], None)
    assert "Cases: 3 of 3 forms" in report
    assert "compared and equal:  1" in report
    assert "diverged:            1" in report
    assert "not compared:        1" in report
    assert "entry=0x80010008  fadd: fadd f1, f2, f3  [f1:N=0x1,I=0x2]" in report
    problem_lines = report.split("Forms with cases that were not compared and equal:")[1].splitlines()
    assert any(line.split()[:1] == ["dcbf"] for line in problem_lines)
    assert any(line.split()[:1] == ["fadd"] for line in problem_lines)
    assert not any(line.split()[:1] == ["add"] for line in problem_lines)


def test_the_report_of_a_clean_run_says_so():
    results = classify([EQUAL_CASE], {EQUAL_CASE.word_address}, set())
    assert "Every case of every form was compared and equal." in format_report(results, [], None)


def test_the_verifier_summary_is_repeated_in_the_report():
    results = classify([EQUAL_CASE], {EQUAL_CASE.word_address}, set())
    report = format_report(results, [], {"checks": 12, "reports": 0})
    assert "Verifier summary: checks=12 reports=0" in report


# -- Command line ------------------------------------------------------------------------------


LOG_WITHOUT_DIVERGENCES = "[lockstep] summary: checks=2 reports=0 skipped_fallback=0 skipped_zero=0\n"
LOG_WITH_A_DIVERGENCE = (
    "[lockstep] DIVERGE #1 entry=0x80010008 end=0x80010010: f1:N=0x1,I=0x2\n"
    "[lockstep] summary: checks=2 reports=1 skipped_fallback=0 skipped_zero=0\n"
)


@pytest.fixture
def run_files(tmp_path):
    cases = tmp_path / "cases.tsv"
    cases.write_text(format_cases(CASES))
    coverage = tmp_path / "coverage.txt"
    coverage.write_text("0x80010000\n0x80010008\n")
    return tmp_path, cases, coverage


def test_report_exits_with_zero_when_nothing_diverged(run_files, capsys):
    tmp_path, cases, coverage = run_files
    log = tmp_path / "clean.log"
    log.write_text(LOG_WITHOUT_DIVERGENCES)
    assert main(["report", str(cases), str(log), "--coverage", str(coverage)]) == 0
    assert "diverged:            0" in capsys.readouterr().out


def test_report_exits_with_one_and_names_the_case_when_something_diverged(run_files, capsys):
    tmp_path, cases, coverage = run_files
    log = tmp_path / "diverged.log"
    log.write_text(LOG_WITH_A_DIVERGENCE)
    assert main(["report", str(cases), str(log), "--coverage", str(coverage)]) == 1
    output = capsys.readouterr().out
    assert "fadd: fadd f1, f2, f3" in output
    assert "f1:N=0x1,I=0x2" in output


def test_report_works_without_a_coverage_file_but_then_nothing_counts_as_compared(run_files, capsys):
    tmp_path, cases, _ = run_files
    log = tmp_path / "clean.log"
    log.write_text(LOG_WITHOUT_DIVERGENCES)
    assert main(["report", str(cases), str(log)]) == 0
    assert "compared and equal:  0" in capsys.readouterr().out


def test_generate_writes_the_program_and_the_list_of_cases(game, tmp_path, capsys):
    dol, symbols = game
    symbols_path = tmp_path / "symbols.txt"
    symbols.write(symbols_path)
    output, cases = tmp_path / "out" / "main.dol", tmp_path / "out" / "cases.tsv"
    output.parent.mkdir()
    assert (
        main(
            [
                "generate",
                "--dol",
                str(dol.path),
                "--symbols",
                str(symbols_path),
                "--output",
                str(output),
                "--cases",
                str(cases),
            ]
        )
        == 0
    )
    assert Dol(output).u32(CODE_ADDRESS) != 0
    assert {case.form for case in parse_cases(cases.read_text())} == {"add", "lwz", "fadd"}
    assert "not tested: bclr" in capsys.readouterr().out


def test_a_path_that_is_not_a_file_is_refused_before_any_work(tmp_path):
    with pytest.raises(argparse.ArgumentTypeError, match="not found"):
        existing_file(str(tmp_path / "missing"))
    with pytest.raises(argparse.ArgumentTypeError, match="not found"):
        existing_file(str(tmp_path))  # a folder is not a file


def test_a_path_that_is_a_file_is_accepted(tmp_path):
    file = tmp_path / "present"
    file.write_text("")
    assert existing_file(str(file)) == file


def test_a_missing_input_stops_the_command_line_with_a_message(tmp_path):
    with pytest.raises(SystemExit):
        main(["report", str(tmp_path / "cases.tsv"), str(tmp_path / "run.log")])
