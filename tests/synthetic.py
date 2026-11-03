"""Build tiny DOLs by hand so the analysis can be tested without game data.

Arma DOLs mínimos a mano para probar el análisis sin datos del juego.

The encoders below write the handful of PowerPC instructions the tests need.
test_synthetic.py checks every one of them against capstone, so a wrong
encoding cannot silently make another test pass.
"""

import struct
from pathlib import Path

INSTRUCTION_SIZE = 4
DOL_HEADER_SIZE = 0x100
TEXT_SLOTS = 7
DATA_SLOTS = 11

# Primary and extended opcodes used below (PowerPC 750CL manual, chapter 12).
OPCODE_CMPLI = 10
OPCODE_ADDI = 14
OPCODE_ADDIS = 15
OPCODE_BC = 16
OPCODE_B = 18
OPCODE_RLWINM = 21
OPCODE_ORI = 24
OPCODE_EXTENDED = 31
OPCODE_LWZ = 32
OPCODE_LMW = 46
OPCODE_PSQ_L = 56
EXTENDED_LWZX = 23
EXTENDED_OR = 444
EXTENDED_MTSPR = 467
SPR_CTR = 9
BO_IF_TRUE = 12
CR0_GREATER_THAN = 1

BLR = 0x4E800020
BCTR = 0x4E800420


def _d_form(opcode: int, rd: int, ra: int, immediate: int) -> int:
    return (opcode << 26) | (rd << 21) | (ra << 16) | (immediate & 0xFFFF)


def lis(rd: int, value: int) -> int:
    return _d_form(OPCODE_ADDIS, rd, 0, value)


def li(rd: int, value: int) -> int:
    return _d_form(OPCODE_ADDI, rd, 0, value)


def addi(rd: int, ra: int, value: int) -> int:
    return _d_form(OPCODE_ADDI, rd, ra, value)


def ori(ra: int, rs: int, value: int) -> int:
    return _d_form(OPCODE_ORI, rs, ra, value)


def lwz(rd: int, displacement: int, ra: int) -> int:
    return _d_form(OPCODE_LWZ, rd, ra, displacement)


def lmw(rd: int, displacement: int, ra: int) -> int:
    return _d_form(OPCODE_LMW, rd, ra, displacement)


def cmplwi(ra: int, value: int) -> int:
    return _d_form(OPCODE_CMPLI, 0, ra, value)  # cr0


def psq_l(frd: int, displacement: int, ra: int) -> int:
    """psq_l frD, d(rA), W=0, I=0: only 12 bits of displacement."""
    return (OPCODE_PSQ_L << 26) | (frd << 21) | (ra << 16) | (displacement & 0xFFF)


def slwi(ra: int, rs: int, shift: int) -> int:
    """rlwinm rA, rS, shift, 0, 31 - shift."""
    return (OPCODE_RLWINM << 26) | (rs << 21) | (ra << 16) | (shift << 11) | (0 << 6) | ((31 - shift) << 1)


def lwzx(rd: int, ra: int, rb: int) -> int:
    return (OPCODE_EXTENDED << 26) | (rd << 21) | (ra << 16) | (rb << 11) | (EXTENDED_LWZX << 1)


def mr(ra: int, rs: int) -> int:
    """or rA, rS, rS."""
    return (OPCODE_EXTENDED << 26) | (rs << 21) | (ra << 16) | (rs << 11) | (EXTENDED_OR << 1)


def mtctr(rs: int) -> int:
    swapped_spr = ((SPR_CTR & 0x1F) << 5) | (SPR_CTR >> 5)
    return (OPCODE_EXTENDED << 26) | (rs << 21) | (swapped_spr << 11) | (EXTENDED_MTSPR << 1)


def b(from_address: int, to_address: int) -> int:
    return (OPCODE_B << 26) | ((to_address - from_address) & 0x03FFFFFC)


def bl(from_address: int, to_address: int) -> int:
    return b(from_address, to_address) | 1


def bgt(from_address: int, to_address: int) -> int:
    return (
        (OPCODE_BC << 26)
        | (BO_IF_TRUE << 21)
        | (CR0_GREATER_THAN << 16)
        | ((to_address - from_address) & 0xFFFC)
    )


def high_adjusted(address: int) -> int:
    """The @ha half: lis/addi pairs add a signed low half, so round up."""
    return ((address + 0x8000) >> 16) & 0xFFFF


def low(address: int) -> int:
    return address & 0xFFFF


def words_to_bytes(words: list[int]) -> bytes:
    return struct.pack(f">{len(words)}I", *words)


def build_dol(
    path: Path,
    text: dict[int, tuple[int, list[int]]],
    data: dict[int, tuple[int, bytes]],
    entry: int,
    bss: tuple[int, int] = (0, 0),
) -> Path:
    """Write a DOL. `text` and `data` map a slot number to (address, content)."""
    offsets = [0] * (TEXT_SLOTS + DATA_SLOTS)
    addresses = [0] * (TEXT_SLOTS + DATA_SLOTS)
    sizes = [0] * (TEXT_SLOTS + DATA_SLOTS)
    body = b""
    sections = [(slot, address, words_to_bytes(words)) for slot, (address, words) in text.items()]
    sections += [(TEXT_SLOTS + slot, address, content) for slot, (address, content) in data.items()]
    for slot, address, content in sections:
        offsets[slot] = DOL_HEADER_SIZE + len(body)
        addresses[slot] = address
        sizes[slot] = len(content)
        body += content
    header = struct.pack(">18I18I18I3I", *offsets, *addresses, *sizes, bss[0], bss[1], entry)
    path.write_bytes(header.ljust(DOL_HEADER_SIZE, b"\0") + body)
    return path


# The analysis needs an extab and an extabindex section to exist; one
# all-zero index entry is a valid "no exception tables" index.
EMPTY_EXCEPTION_SECTIONS = {
    0: (0x80006000, b"\0" * 8),  # extab
    1: (0x80006100, b"\0" * 12),  # extabindex
}
