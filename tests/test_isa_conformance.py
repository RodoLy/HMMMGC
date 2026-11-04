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
    ADD_MINUS_ONE_OPERATIONS,
    ALIGNMENT_CHECKED_OPERATIONS,
    CODE_ADDRESS,
    CONTROL_SECTION_SIZE,
    DISPLACEMENT_OPERATIONS,
    DONE_ADDRESS,
    DONE_VALUE,
    FPSCR_EXCEPTION_BITS,
    INDEXED_OPERATIONS,
    INDEXED_OPERATIONS_WITH_UPDATE,
    NO_GAME_ADDRESS,
    OPERAND_FIELD_SHIFTS,
    PROGRESS_ADDRESS,
    R0_OPERAND,
    READABLE_SPRS,
    REQUIRED_ALIGNMENT,
    SCRATCH_ADDRESS,
    SCRATCH_CENTER,
    SCRATCH_SIZE,
    SKIPPED_OPERATIONS,
    STATE_FPRS,
    STATE_SIZE,
    STATE_TABLE_ADDRESS,
    SYNTHETIC_REGISTERS,
    WRITABLE_SPRS,
    XER_MASK,
    Candidate,
    Case,
    FloatState,
    FormResult,
    Treatment,
    build_program,
    capstone_view,
    classify,
    clear_carry,
    collect_candidates,
    deal_by_register_pattern,
    encode_spr,
    existing_file,
    field_ra,
    field_rb,
    field_rd,
    float_state_of,
    format_cases,
    format_report,
    is_instruction,
    load_constant,
    main,
    named_encodings,
    parse_cases,
    progress_code,
    random_state,
    register_combinations,
    register_pattern,
    reserved_bits,
    setup_before,
    spr_number,
    state_setup_code,
    synthetic_candidates,
    synthetic_candidates_for_seed,
    synthetic_word,
    treatment_of,
)
from ppc import Instruction, format_instruction, sign_extend  # noqa: E402
from ppc_encode import BLR, b, lfd, lis, lmw, lwz, lwzx, mtfsf, mtspr, psq_l, stw  # noqa: E402
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
    operation_of,
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


def test_an_indexed_update_form_without_a_base_register_is_not_run_but_the_other_forms_are():
    # With rA = r0 the update forms are invalid, and their address would be r0 + rB, which is
    # anywhere in memory: the first run of the made-up words hung on one of them.
    update_forms_found = set()
    for encoding, _ in named_encodings():
        operation, _ = operation_of(encoding)
        if treatment_of(operation) is not Treatment.INDEXED:
            continue
        without_base = encoding | (3 << 21) | (0 << 16) | (5 << 11)
        setup = setup_before(without_base, random.Random(0))
        if operation in INDEXED_OPERATIONS_WITH_UPDATE:
            assert setup is None, operation
            update_forms_found.add(operation)
        else:
            assert setup == load_constant(5, SCRATCH_CENTER), operation  # the address is rB alone
    assert update_forms_found == INDEXED_OPERATIONS_WITH_UPDATE


def test_an_update_form_is_not_run_when_the_destination_is_the_base():
    lwzux_r4_r4_r5 = (31 << 26) | (4 << 21) | (4 << 16) | (5 << 11) | (55 << 1)
    assert form_name(lwzux_r4_r4_r5) == "lwzux"
    assert setup_before(lwzux_r4_r4_r5, random.Random(0)) is None


@pytest.mark.parametrize(
    ("opcode", "form"), [(33, "lwzu"), (35, "lbzu"), (41, "lhzu"), (43, "lhau")], ids=lambda value: str(value)
)
def test_an_update_load_with_a_displacement_is_not_run_when_the_destination_is_the_base(opcode, form):
    destination_is_base = (opcode << 26) | (4 << 21) | (4 << 16) | 8
    other_destination = (opcode << 26) | (5 << 21) | (4 << 16) | 8
    assert form_name(destination_is_base) == form
    assert setup_before(destination_is_base, random.Random(0)) is None
    assert setup_before(other_destination, random.Random(0)) == load_constant(4, SCRATCH_CENTER)


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


# The instructions of opcode 31 that add or subtract with the carry. The ones that take a second
# register (adde, subfe) get rB; the others have that field set to zero.
EXTENDED_OPCODES_OF_THE_CARRY_FORMS = {
    "adde": 138,
    "subfe": 136,
    "addze": 202,
    "subfze": 200,
    "addme": 234,
    "subfme": 232,
}
FORMS_WITH_A_SECOND_REGISTER = ("adde", "subfe")
OVERFLOW_ENABLE_BIT_POSITION = 10


def carry_form_word(name: str, *, overflow_enable: bool, record: bool) -> int:
    """`name` rD=r3, rA=r4 (and rB=r5 for the two that take one), with the `o` and `.` bits as asked."""
    second_register = 5 if name in FORMS_WITH_A_SECOND_REGISTER else 0
    return (
        (31 << 26)
        | (3 << 21)
        | (4 << 16)
        | (second_register << 11)
        | (int(overflow_enable) << OVERFLOW_ENABLE_BIT_POSITION)
        | (EXTENDED_OPCODES_OF_THE_CARRY_FORMS[name] << 1)
        | int(record)
    )


@pytest.mark.parametrize("record", [False, True], ids=["no dot", "dot"])
@pytest.mark.parametrize("overflow_enable", [False, True], ids=["no o", "o"])
@pytest.mark.parametrize("name", ["addme", "subfme"])
def test_add_to_minus_one_and_subtract_from_minus_one_run_with_the_carry_clear(name, overflow_enable, record):
    word = carry_form_word(name, overflow_enable=overflow_enable, record=record)
    assert operation_of(word)[0] == name
    assert [text_of(setup_word) for setup_word in setup_before(word, random.Random(0))] == ["addic r0, r0, 0"]


@pytest.mark.parametrize("record", [False, True], ids=["no dot", "dot"])
@pytest.mark.parametrize("overflow_enable", [False, True], ids=["no o", "o"])
@pytest.mark.parametrize("name", ["adde", "subfe", "addze", "subfze"])
def test_the_other_instructions_that_use_the_carry_run_with_the_carry_they_were_given(
    name, overflow_enable, record
):
    # The interpreter gets the carry out of these right for either carry in, so a random one
    # (which the machine state sets) is as good as any.
    word = carry_form_word(name, overflow_enable=overflow_enable, record=record)
    assert operation_of(word)[0] == name
    assert setup_before(word, random.Random(0)) == []


def test_the_instruction_that_clears_the_carry_changes_no_register_and_cannot_carry():
    (clearing_word,) = clear_carry()
    assert form_name(clearing_word) == "addic"
    # addic rD, rA, SIMM puts rA + SIMM in rD and the carry out of the sum in XER[CA]. Here
    # that is r0 + 0 into r0, which gives r0 back, with nothing carried out of it.
    assert (field_rd(clearing_word), field_ra(clearing_word), clearing_word & 0xFFFF) == (0, 0, 0)


def test_the_operations_whose_carry_out_the_interpreter_gets_wrong_are_the_two_minus_one_ones():
    assert sorted(ADD_MINUS_ONE_OPERATIONS) == ["addme", "subfme"]
    assert known_operations() >= ADD_MINUS_ONE_OPERATIONS


# The primary opcodes of the accesses with a displacement (D form), to build words with any displacement.
D_FORM_OPCODES = {
    "lmw": 46, "stmw": 47, "lwz": 32, "lhz": 40, "stw": 36, "sth": 44,
    "lfs": 48, "lfsu": 49, "lfd": 50, "lfdu": 51, "stfs": 52, "stfsu": 53, "stfd": 54, "stfdu": 55,
}  # fmt: skip


def displacement_word(
    operation: str, displacement: int, data_register: int = 28, base_register: int = 3
) -> int:
    """`operation` with the given displacement; the data register is clear of the base for lmw."""
    opcode = D_FORM_OPCODES[operation]
    return (opcode << 26) | (data_register << 21) | (base_register << 16) | (displacement & 0xFFFF)


def value_loaded_by(setup: list[int]) -> int:
    """The constant that `load_constant` puts in a register: its upper half, then the lower half."""
    high_half_load, low_half_or = setup
    return ((high_half_load & 0xFFFF) << 16) | (low_half_or & 0xFFFF)


MISALIGNED_AND_ALIGNED_DISPLACEMENTS = (-0x8000, -0x7FFF, -5, -4, -3, -1, 0, 1, 2, 3, 4, 8, 0x2803, 0x7FFF)


@pytest.mark.parametrize("operation", sorted(ALIGNMENT_CHECKED_OPERATIONS))
def test_accesses_that_trap_on_misalignment_get_an_aligned_address_inside_the_scratch_memory(operation):
    # Dolphin's interpreter raises an alignment exception for these, and the recompiled code does
    # not, so the address must never be misaligned, whatever displacement the word has.
    for displacement in MISALIGNED_AND_ALIGNED_DISPLACEMENTS:
        word = displacement_word(operation, displacement)
        assert operation_of(word)[0] == operation
        address = value_loaded_by(setup_before(word, random.Random(0))) + displacement
        assert address % REQUIRED_ALIGNMENT == 0, displacement
        assert SCRATCH_ADDRESS <= address < SCRATCH_ADDRESS + SCRATCH_SIZE, displacement


@pytest.mark.parametrize("operation", sorted(ALIGNMENT_CHECKED_OPERATIONS))
def test_an_aligned_displacement_leaves_the_base_where_it_always_was(operation):
    # The words of the game have aligned displacements, so its runs are the same as before.
    for displacement in (-8, 0, 4, 8, 0x2800):
        setup = setup_before(displacement_word(operation, displacement), random.Random(0))
        assert setup == load_constant(3, SCRATCH_CENTER)


@pytest.mark.parametrize("operation", ["lwz", "lhz", "stw", "sth"])
def test_integer_accesses_keep_their_misaligned_addresses_because_the_processor_handles_them(operation):
    setup = setup_before(displacement_word(operation, 3), random.Random(0))
    assert setup == load_constant(3, SCRATCH_CENTER)


def test_paired_single_accesses_keep_their_misaligned_addresses_as_dolphin_raises_nothing_for_them():
    assert setup_before(psq_l(1, 3, 4), random.Random(0)) == load_constant(4, SCRATCH_CENTER)


def test_the_operations_that_need_an_aligned_address_are_accesses_with_a_displacement():
    assert ALIGNMENT_CHECKED_OPERATIONS <= DISPLACEMENT_OPERATIONS
    assert set(D_FORM_OPCODES) >= ALIGNMENT_CHECKED_OPERATIONS


FPSCR_BIT_COUNT = 32
EXTENDED_OPCODES_OF_MTFSB = {"mtfsb0": 70, "mtfsb1": 38}


def mtfsb_word(operation: str, bit: int, *, record: bool = False) -> int:
    """mtfsb0 or mtfsb1 on FPSCR bit `bit`, which counts from the most significant bit."""
    return (63 << 26) | (bit << 21) | (EXTENDED_OPCODES_OF_MTFSB[operation] << 1) | int(record)


@pytest.mark.parametrize("record", [False, True], ids=["no dot", "dot"])
@pytest.mark.parametrize("bit", sorted(FPSCR_EXCEPTION_BITS))
def test_mtfsb1_is_not_run_on_a_bit_that_records_an_exception(bit, record):
    # The interpreter sets FX along with such a bit, and the recompiled code does not.
    word = mtfsb_word("mtfsb1", bit, record=record)
    assert operation_of(word)[0] == "mtfsb1"
    assert setup_before(word, random.Random(0)) is None


@pytest.mark.parametrize("bit", [bit for bit in range(FPSCR_BIT_COUNT) if bit not in FPSCR_EXCEPTION_BITS])
def test_mtfsb1_is_run_on_every_other_bit(bit):
    # FX, FEX and VX themselves, the exception enables, NI and the rounding mode, among others.
    assert setup_before(mtfsb_word("mtfsb1", bit), random.Random(0)) == []


@pytest.mark.parametrize("bit", range(FPSCR_BIT_COUNT))
def test_mtfsb0_is_run_on_every_bit_because_clearing_a_bit_sets_nothing_else(bit):
    assert operation_of(mtfsb_word("mtfsb0", bit))[0] == "mtfsb0"
    assert setup_before(mtfsb_word("mtfsb0", bit), random.Random(0)) == []


def test_the_exception_bits_are_the_status_bits_and_not_the_summaries_enables_or_modes():
    summaries_enables_and_modes = {0, 1, 2, 13, 14, *range(15, 20), 20, *range(24, 29), 29, 30, 31}
    assert not FPSCR_EXCEPTION_BITS & summaries_enables_and_modes


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


# -- Words that need no game -------------------------------------------------------------------

ADDO = 0x7C642E14  # addo r3,r4,r5: Capstone does not know the overflow-enabled forms
ADD_RECORD = 0x7C642A15  # add. r3,r4,r5
MTFSB0_RECORD = 0xFC60008D  # mtfsb0. 3
FCMPO = 0xFC011040  # fcmpo cr0,f1,f2: Capstone does not know this one either
MCRXR = 0x7C000400  # mcrxr cr0
LWZX_WITH_BIT_0_SET = 0x7C64282F  # lwzx r3,r4,r5 with the bit that must be zero set
STWCX_RECORD = 0x7C00212D  # stwcx. r0,0,r4: its bit 0 is always set, and is part of the instruction
MFSR = 0x7C0004A6  # mfsr r0,0
# Forms the tables name that Capstone cannot read and that this test program does not run anyway
# (the string instructions, and the external-control pair): the generator never reaches them.
FORMS_THE_GENERATOR_CANNOT_REACH = {"eciwx", "ecowx", "lswx", "stswx"}


def every_form_the_tables_name() -> set[str]:
    """The form names of all the encodings, with the record, link and absolute bits clear and set."""
    return {form_name(encoding | low_bits) for encoding, _ in named_encodings() for low_bits in range(4)}


@pytest.fixture(scope="module")
def made_up_words() -> dict[str, list[Candidate]]:
    """The generator's output, made once: building it takes a couple of seconds."""
    return synthetic_candidates(random.Random(1), random_fills_per_encoding=4)


def test_the_encodings_to_fill_in_name_every_operation_of_the_tables_and_nothing_else():
    operations = {operation_of(encoding)[0] for encoding, _ in named_encodings()}
    assert operations == known_operations()


def test_an_encoding_leaves_free_only_the_bits_below_its_opcode_fields():
    for encoding, free_bits in named_encodings():
        assert encoding & free_bits == 0
        if encoding >> 26 in (4, 19, 31, 59, 63):
            assert free_bits & (0x3FF << 1) == 0  # the extended opcode is part of the encoding


def test_every_mix_of_the_chosen_registers_is_made_once_with_and_without_the_record_bit():
    add_encoding, free_bits = 0x7C000214, 0x03FF_F801
    words = list(register_combinations(add_encoding, free_bits))
    assert len(words) == len(set(words)) == len(SYNTHETIC_REGISTERS) ** 3 * 2
    assert {form_name(word) for word in words} == {"add", "add."}
    mixes = {tuple((word >> shift) & 0x1F for shift in OPERAND_FIELD_SHIFTS) for word in words}
    assert len(mixes) == len(SYNTHETIC_REGISTERS) ** 3
    assert all(register in SYNTHETIC_REGISTERS for mix in mixes for register in mix)
    # Nothing outside the three register fields and the record bit differs from the encoding.
    assert {word & ~free_bits for word in words} == {add_encoding}


def test_a_branch_gets_all_four_values_of_its_link_and_absolute_address_bits():
    branch_encoding, free_bits = 18 << 26, 0x03FF_FFFF
    forms = {form_name(word) for word in register_combinations(branch_encoding, free_bits)}
    assert forms == {"b", "bl", "ba", "bla"}


def test_a_made_up_word_keeps_its_opcode_and_varies_everything_else():
    encoding, free_bits = 0x7C000214, 0x03FF_F801
    words = {synthetic_word(encoding, free_bits, random.Random(seed)) for seed in range(200)}
    assert all(word & ~free_bits == encoding for word in words)
    assert len(words) > 100


def test_a_made_up_word_often_names_the_same_register_twice():
    rng = random.Random(7)
    words = [synthetic_word(0x7C000214, 0x03FF_F801, rng) for _ in range(300)]
    aliasing = [
        word for word in words if len({register_pattern(word)}) and len(set(register_pattern(word))) < 3
    ]
    assert len(aliasing) > 60


@pytest.mark.parametrize("spr", sorted(READABLE_SPRS | WRITABLE_SPRS))
def test_the_number_of_a_special_register_survives_being_encoded(spr):
    mfspr_encoding = 31 << 26 | 339 << 1
    assert spr_number(encode_spr(mfspr_encoding, spr)) == spr
    assert operation_of(encode_spr(mfspr_encoding, spr))[0] == "mfspr"


def test_made_up_special_register_words_name_a_register_the_program_can_use():
    rng = random.Random(3)
    for operation, allowed in (("mfspr", READABLE_SPRS), ("mtspr", WRITABLE_SPRS)):
        encoding = 31 << 26 | (339 if operation == "mfspr" else 467) << 1
        free_bits = 0x03FF_F801
        # A word draws its register number from the allowed ones, apart from the registers
        # combinations (which are not made by synthetic_word at all).
        assert all(spr_number(synthetic_word(encoding, free_bits, rng)) in allowed for _ in range(100)), (
            operation
        )


@pytest.mark.parametrize(
    ("word", "viewed_as"),
    [
        (ADD, ADD),
        (ADD_RECORD, ADD_RECORD),  # the record bit belongs to add and Capstone knows it
        (ADDO, ADD),  # the overflow-enable bit is cleared
        (ADDO | 1, ADD_RECORD),
        (MTFSB0_RECORD, MTFSB0_RECORD & ~1),  # Capstone only knows mtfsb0 without its record bit
    ],
    ids=["add", "add.", "addo", "addo.", "mtfsb0."],
)
def test_capstone_is_shown_the_word_without_the_bits_it_does_not_model(word, viewed_as):
    assert capstone_view(word) == viewed_as


def test_the_operands_of_an_overflow_enabled_word_are_read_through_its_plain_twin():
    assert register_pattern(ADDO) == register_pattern(ADD) == (0, 1, 2)


@pytest.mark.parametrize(
    ("word", "is_defined"),
    [
        (ADD, True),
        (ADDO, True),
        (lwz(3, 8, 4), True),
        (FCMPO, True),
        (MCRXR, True),
        (FCMPO | 1 << 22, False),  # a reserved bit of fcmpo
        (MCRXR | 1 << 12, False),  # a reserved bit of mcrxr
        (LWZX_WITH_BIT_0_SET, False),
        (0x00000000, False),
        (0x7C0007FE, False),  # opcode 31, an extended opcode that no instruction has
    ],
    ids=[
        "add",
        "addo",
        "lwz",
        "fcmpo",
        "mcrxr",
        "fcmpo reserved",
        "mcrxr reserved",
        "lwzx bit 0",
        "zero",
        "unnamed",
    ],
)
def test_a_word_is_an_instruction_when_its_form_exists_and_its_reserved_bits_are_clear(word, is_defined):
    assert is_instruction(word) is is_defined


# A word that Capstone reads as an instruction of a later architecture, because a bit that the
# 750CL manual keeps reserved means something there, and the same word with that bit clear.
WORDS_WITH_A_RESERVED_BIT_SET = [
    (0x7CA02000, 0x7C802000),  # cmpd cr1,r0,r4: the L bit makes a compare 64 bits wide (cmpw)
    (0x2CA42003, 0x2C842003),  # cmpdi
    (0x7CA00040, 0x7C800040),  # cmpld
    (0x28A30FF5, 0x28830FF5),  # cmpldi
    (0x7C60022C, 0x7C00022C),  # dcbt 0,r0,3: the hint
    (0x7C8401EC, 0x7C0401EC),  # dcbtst r4,r0,4
    (0x7CA000AC, 0x7C0000AC),  # dcbf 0,r0,5: the flush level
    (0x7CA006AC, 0x7C0006AC),  # mbar 5: the barrier to wait for (eieio is the one with none)
    (0x7C2004AC, 0x7C0004AC),  # lwsync: sync with its L bit
    (0xFC8D058E, 0xFC8C058E),  # mtfsf 0x46,f0,0,1: the W bit
    (0x7C002CA6, 0x7C0004A6),  # mfsr r0,0 with a bit set in the field after the segment number
    (0x7C0004A7, 0x7C0004A6),  # mfsr r0,0 with bit 0 set
    (0x7C002029, 0x7C002028),  # lwarx r0,0,r4 with the hint in bit 0
    (0x10A41830, 0x10A01830),  # ps_res f5,f3 with a bit set in the field of rA: maddhd
    (0x10605A6A, 0x1060582A),  # ps_add f3,f0,f11 with a bit set in the field of frC: vsel
    (0x10002B8D, 0x10002B8C),  # psq_lx f0,r0,r5,0,7 with bit 0 set: vinsertw
]  # fmt: skip


@pytest.mark.parametrize(("reserved_bit_set", "plain"), WORDS_WITH_A_RESERVED_BIT_SET)
def test_a_bit_that_the_manual_keeps_reserved_makes_a_word_no_instruction_even_if_capstone_reads_it(
    reserved_bit_set, plain
):
    assert operation_of(reserved_bit_set) == operation_of(plain)  # the tables name both the same
    assert not is_instruction(reserved_bit_set)
    assert is_instruction(plain)


@pytest.mark.parametrize(
    ("word", "form"),
    [(ADD_RECORD, "add."), (STWCX_RECORD, "stwcx."), (lwz(3, 1, 4), "lwz"), (b(0x80003100, 0x80003101), "b")],
    ids=["a record bit", "a bit that is part of the name", "a displacement", "an address"],
)
def test_bit_0_is_not_reserved_where_the_form_uses_it(word, form):
    assert form_name(word) == form
    assert reserved_bits(word, operation_of(word)[0]) & 1 == 0


def test_bit_0_is_reserved_in_a_form_that_has_an_extended_opcode_and_no_use_for_the_bit():
    assert reserved_bits(MFSR, "mfsr") & 1
    assert reserved_bits(ADD, "add") & 1 == 0  # add has a record twin: the bit is its Rc


def test_the_made_up_words_reach_every_form_the_tables_name_that_capstone_can_read(made_up_words):
    assert (
        set(made_up_words) == every_form_the_tables_name() - {ILLEGAL_FORM} - FORMS_THE_GENERATOR_CANNOT_REACH
    )


@pytest.mark.parametrize(
    "form", ["add", "addo.", "lwz", "fmadds", "ps_sum0", "rlwinm", "mtfsf", "psq_l", "sync", "fcmpo"]
)
def test_the_made_up_words_include_forms_of_every_kind(made_up_words, form):
    assert made_up_words[form]


def test_every_made_up_word_has_the_form_it_is_listed_under_and_is_an_instruction(made_up_words):
    for form, candidates in made_up_words.items():
        for candidate in candidates:
            assert form_name(candidate.word) == form
            assert is_instruction(candidate.word)


def test_a_made_up_word_has_no_place_in_the_game(made_up_words):
    assert all(
        candidate.game_address == NO_GAME_ADDRESS for words in made_up_words.values() for candidate in words
    )


def test_the_words_of_a_form_are_distinct_and_in_order(made_up_words):
    for candidates in made_up_words.values():
        words = [candidate.word for candidate in candidates]
        assert words == sorted(set(words))


def test_a_form_gets_words_that_name_one_register_twice_and_words_that_name_r0(made_up_words):
    patterns = {register_pattern(candidate.word) for candidate in made_up_words["add"]}
    assert (0, 0, 2) in patterns  # rD = rA
    assert (0, 1, 0) in patterns  # rD = rB
    assert (0, 1, 1) in patterns  # rA = rB
    assert (0, 0, 0) in patterns  # all three
    assert any(R0_OPERAND in pattern for pattern in patterns)


@pytest.fixture
def a_few_encodings(monkeypatch):
    """Only a handful of encodings to fill in, so that a test that makes words twice is quick."""
    some = [(0x7C000214, 0x03FF_F801), (32 << 26, 0x03FF_FFFF), (63 << 26 | 29 << 1, 0x03FF_F801)]
    monkeypatch.setattr("isa_conformance.named_encodings", lambda: iter(some))


def test_the_same_seed_makes_the_same_words_and_another_seed_other_words(a_few_encodings):
    first = synthetic_candidates(random.Random(1), random_fills_per_encoding=20)
    again = synthetic_candidates(random.Random(1), random_fills_per_encoding=20)
    other = synthetic_candidates(random.Random(2), random_fills_per_encoding=20)
    assert first == again
    assert first != other


def test_the_words_of_a_run_depend_on_its_seed_only(a_few_encodings):
    assert synthetic_candidates_for_seed(5) == synthetic_candidates_for_seed(5)
    assert synthetic_candidates_for_seed(5) != synthetic_candidates_for_seed(6)


def test_a_program_of_made_up_words_has_cases_for_the_special_register_forms_too(made_up_words):
    program = build_program(made_up_words, seed=1, words_per_form=3, group_size=4)
    forms = {case.form for case in program.cases}
    assert {"mfspr", "mtspr", "add", "addo", "fcmpo"} <= forms
    assert all(spr_number(case.word) in READABLE_SPRS for case in program.cases if case.form == "mfspr")
    assert all(spr_number(case.word) in WRITABLE_SPRS for case in program.cases if case.form == "mtspr")


def test_a_program_of_made_up_words_says_which_forms_it_leaves_out(made_up_words):
    program = build_program(made_up_words, seed=1, words_per_form=3, group_size=4)
    assert {"b", "bl", "bclr", "rfi", "sc", "tw", "mtmsr"} <= set(program.skipped_forms)
    assert not {case.form for case in program.cases} & set(program.skipped_forms)


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


def positions_of(sequence: list[int], words: list[int]) -> list[int]:
    """Every place in `words` where `sequence` appears, as an index into the list."""
    return [
        start
        for start in range(len(words) - len(sequence) + 1)
        if words[start : start + len(sequence)] == sequence
    ]


def test_the_progress_code_writes_the_group_number_next_to_the_marker():
    assert [text_of(word) for word in progress_code(300)] == [
        "lis r0, 0",
        "ori r0, r0, 0x12c",  # 300
        "lis r4, -0x7fa0",  # 0x8060
        "stw r0, 4(r4)",  # the word after DONE_ADDRESS
    ]
    assert PROGRESS_ADDRESS == DONE_ADDRESS + 4
    assert PROGRESS_ADDRESS & 0xFFFF == 4
    assert CONTROL_SECTION_SIZE == 8


def test_main_notes_the_number_of_each_group_just_before_it_loads_the_group_state():
    program = build_program(CANDIDATES, seed=1, words_per_form=12, group_size=2)
    group_count = len(program.group_first_cases)
    assert group_count > 1
    previous_position = -1
    for group_number in range(group_count):
        (position,) = positions_of(progress_code(group_number), program.text_words)
        assert position > previous_position  # in order
        previous_position = position
        # The state set-up starts with the load of the address of the group's row of the table.
        row_address = STATE_TABLE_ADDRESS + STATE_SIZE * group_number
        first_setup_word = program.text_words[position + len(progress_code(group_number))]
        assert first_setup_word == lis(2, row_address >> 16)


def test_the_cases_of_each_group_are_the_ones_main_calls_after_its_note():
    program = build_program(CANDIDATES, seed=1, words_per_form=12, group_size=2)
    assert program.group_first_cases[0] == 0
    assert program.group_first_cases == sorted(set(program.group_first_cases))
    groups = [program.cases_of_group(number) for number in range(len(program.group_first_cases))]
    assert all(1 <= len(group) <= 2 for group in groups)
    assert [case for group in groups for case in group] == program.cases  # every case once, in order


def test_a_group_number_that_main_never_wrote_has_no_cases():
    program = build_program(CANDIDATES, seed=1, words_per_form=12, group_size=2)
    assert program.cases_of_group(-1) == []
    assert program.cases_of_group(len(program.group_first_cases)) == []
    assert program.cases_of_group(0xFFFFFFFF) == []


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
    assert dol.u32(PROGRESS_ADDRESS) == 0  # and the number of the group it is running here
    assert dol.is_mapped(DONE_ADDRESS + CONTROL_SECTION_SIZE - 1)
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


def test_generate_with_synthetic_needs_no_game_and_uses_made_up_words(tmp_path, monkeypatch, capsys):
    monkeypatch.setattr(
        "isa_conformance.synthetic_candidates_for_seed",
        lambda seed: {
            "add": [Candidate(ADD, NO_GAME_ADDRESS)],
            "lwz": [Candidate(lwz(3, 8, 4), NO_GAME_ADDRESS)],
        },
    )
    output, cases = tmp_path / "main.dol", tmp_path / "cases.tsv"
    # No --dol and no --symbols: the defaults point at a game that is not here.
    assert main(["generate", "--synthetic", "--output", str(output), "--cases", str(cases)]) == 0
    assert {case.form for case in parse_cases(cases.read_text())} == {"add", "lwz"}
    assert "2 cases of 2 forms" in capsys.readouterr().out


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
