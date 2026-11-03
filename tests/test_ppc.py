"""Decoder tests. Every encoding here was read from main.dol and checked
against capstone; none of them needs game data to run.

Pruebas del decodificador. Cada codificación se leyó de main.dol y se
comparó con capstone; ninguna necesita datos del juego para correr."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from ppc import Instruction, format_instruction  # noqa: E402


def decode(word: int, address: int = 0x80003100) -> Instruction:
    return Instruction(address, word)


def test_bl_backwards_is_a_call_with_the_right_target():
    instruction = decode(0x4BFFFD81, 0x801B4AAC)  # main: bl 0x801B482C
    assert instruction.is_call
    assert not instruction.is_jump
    assert instruction.branch_target() == 0x801B482C


def test_b_is_a_jump_not_a_call():
    instruction = decode(0x4BFFFFF4, 0x801B4AC0)  # main: b 0x801B4AB4
    assert instruction.is_jump
    assert instruction.ends_straight_line_code
    assert instruction.branch_target() == 0x801B4AB4


def test_conditional_branch_target():
    instruction = decode(0x4082FFF4, 0x800034A4)  # bne 0x80003498
    assert instruction.is_conditional_branch
    assert not instruction.ends_straight_line_code
    assert instruction.branch_target() == 0x80003498


def test_blr_bctr_and_rfi_end_straight_line_code():
    assert decode(0x4E800020).is_return
    assert decode(0x4E800420).is_jump_to_count_register
    assert decode(0x4C000064).is_return_from_interrupt
    for word in (0x4E800020, 0x4E800420, 0x4C000064):
        assert decode(word).ends_straight_line_code


def test_blrl_and_bctrl_are_indirect_calls():
    assert decode(0x4E800021).is_indirect_call  # blrl
    assert decode(0x4E800421).is_indirect_call  # bctrl
    assert not decode(0x4E800021).ends_straight_line_code


def test_special_register_number_is_unswapped():
    assert decode(0x7C0802A6).special_register == 8  # mflr r0
    assert decode(0x7C0903A6).is_move_to_count_register  # mtctr r0


def test_lis_and_addi_fields():
    lis = decode(0x3C608000)  # lis r3, 0x8000
    assert lis.is_load_immediate_shifted
    assert (lis.rd, lis.unsigned_immediate) == (3, 0x8000)

    addi = decode(0x38035544)  # addi r0, r3, 0x5544
    assert addi.is_add_immediate_to_register
    assert (addi.rd, addi.ra, addi.signed_immediate) == (0, 3, 0x5544)


def test_negative_displacement_and_update_form():
    stwu = decode(0x9421FFF0)  # stwu r1, -0x10(r1)
    assert stwu.is_memory_access
    assert stwu.writes_back_base_register
    assert not stwu.writes_rd_from_memory
    assert stwu.signed_immediate == -0x10


def test_float_loads_do_not_claim_to_write_integer_registers():
    lfs = decode(0xC02280A0)  # lfs f1, -0x7F60(r2)
    assert lfs.is_memory_access
    assert not lfs.writes_rd_from_memory


def test_capstone_listing_matches_the_decoder():
    assert format_instruction(0x4BFFFD81, 0x801B4AAC) == "bl       0x801b482c"
    assert format_instruction(0x4D657472, 0x800034AC).startswith(".word")
