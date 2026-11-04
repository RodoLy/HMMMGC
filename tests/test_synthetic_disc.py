"""Tests of the make-believe boot files. No game and no ModernGekko needed.

Pruebas de los archivos de arranque inventados. Sin el juego ni ModernGekko.

The apploader is PowerPC code that Dolphin runs while it boots, so these tests run it too: a
few lines below interpret the nine instructions it uses, and call its functions the way
Dolphin's boot code does (`RunApploader` in Boot_BS2Emu.cpp).
"""

import struct
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import synthetic_disc  # noqa: E402
from dol import Dol, write_dol  # noqa: E402
from ppc_encode import BLR, lis  # noqa: E402
from synthetic_disc import (  # noqa: E402
    APPLOADER_DISC_OFFSET,
    APPLOADER_HEADER_SIZE,
    APPLOADER_LOAD_ADDRESS,
    BI2_BIN_SIZE,
    BOOT_BIN_SIZE,
    DISC_MAGIC,
    Copy,
    align_up,
    apploader_code,
    apploader_img,
    bi2_bin,
    boot_bin,
    dol_address_on_disc,
    write_system_files,
)

# -- A program to load ---------------------------------------------------------------------------

CODE_ADDRESS = 0x80003100
STATE_ADDRESS = 0x80400000
SCRATCH_ADDRESS = 0x80500000
BYTES_OF_STATE = bytes(range(64))
BYTES_OF_SCRATCH = bytes(reversed(range(200)))


@pytest.fixture
def program_path(tmp_path) -> Path:
    """A DOL of one text section and two data sections, like the programs of isa_conformance.py."""
    return write_dol(
        tmp_path / "program.dol",
        text={0: (CODE_ADDRESS, [lis(3, 1), BLR, BLR])},
        data={0: (STATE_ADDRESS, BYTES_OF_STATE), 1: (SCRATCH_ADDRESS, BYTES_OF_SCRATCH)},
        entry=CODE_ADDRESS,
    )


# -- A processor that knows the instructions of the apploader --------------------------------------

OPCODE_RLWINM, OPCODE_ADDI, OPCODE_ADDIS, OPCODE_ORI, OPCODE_LWZ, OPCODE_STW, OPCODE_EXTENDED = (
    21,
    14,
    15,
    24,
    32,
    36,
    31,
)
EXTENDED_NEG, EXTENDED_OR, BLR_WORD = 104, 444, BLR
MASK_32 = 0xFFFFFFFF
RETURN_ADDRESS = 0  # what the boot code puts in LR before it runs a function


class Machine:
    """Registers, memory and a loop that interprets the instructions the apploader is made of."""

    def __init__(self):
        self.registers = [0] * 32
        self.memory: dict[int, int] = {}  # word address -> word; reading an address nobody wrote fails

    def load_words(self, address: int, words: list[int]) -> None:
        for index, word in enumerate(words):
            self.memory[address + 4 * index] = word

    def read(self, address: int) -> int:
        assert address % 4 == 0, f"unaligned access to 0x{address:08X}"
        assert address in self.memory, f"read of 0x{address:08X}, which nothing wrote"
        return self.memory[address]

    def base(self, register: int) -> int:
        return 0 if register == 0 else self.registers[register]  # r0 as a base means zero

    def run_function(self, address: int, arguments: tuple[int, ...] = ()) -> int:
        """Run the function at `address` until it returns, and return r3, as Dolphin's RunFunction does."""
        for number, value in enumerate(arguments, start=3):
            self.registers[number] = value
        program_counter = address
        for _ in range(1000):  # a function that does not return is a bug, not something to wait for
            word = self.read(program_counter)
            if word == BLR_WORD:
                return self.registers[3]
            self.step(word)
            program_counter += 4
        raise AssertionError(f"the function at 0x{address:08X} did not return")

    def step(self, word: int) -> None:
        opcode = word >> 26
        rd = rs = (word >> 21) & 0x1F
        ra = (word >> 16) & 0x1F
        rb = (word >> 11) & 0x1F
        immediate = word & 0xFFFF
        signed = immediate - 0x10000 if immediate & 0x8000 else immediate
        registers = self.registers
        if opcode == OPCODE_ADDIS:
            registers[rd] = (self.base(ra) + (signed << 16)) & MASK_32
        elif opcode == OPCODE_ADDI:
            registers[rd] = (self.base(ra) + signed) & MASK_32
        elif opcode == OPCODE_ORI:
            registers[ra] = registers[rs] | immediate
        elif opcode == OPCODE_LWZ:
            registers[rd] = self.read((self.base(ra) + signed) & MASK_32)
        elif opcode == OPCODE_STW:
            address = (self.base(ra) + signed) & MASK_32
            assert address % 4 == 0, f"unaligned store to 0x{address:08X}"
            self.memory[address] = registers[rs]
        elif opcode == OPCODE_RLWINM:
            shift, mask_begin, mask_end = rb, (word >> 6) & 0x1F, (word >> 1) & 0x1F
            rotated = ((registers[rs] << shift) | (registers[rs] >> (32 - shift))) & MASK_32
            registers[ra] = rotated & self.mask(mask_begin, mask_end)
        elif opcode == OPCODE_EXTENDED and (word >> 1) & 0x3FF == EXTENDED_NEG:
            registers[rd] = -registers[ra] & MASK_32
        elif opcode == OPCODE_EXTENDED and (word >> 1) & 0x3FF == EXTENDED_OR:
            registers[ra] = registers[rs] | registers[rb]
        else:
            raise AssertionError(f"the apploader should not use 0x{word:08X}")

    @staticmethod
    def mask(begin: int, end: int) -> int:
        """The bits from MB to ME, numbered from the most significant one; wraps when MB > ME."""
        first_to_last = (MASK_32 >> begin) & ~(MASK_32 >> (end + 1)) if end < 31 else MASK_32 >> begin
        if begin <= end:
            return first_to_last & MASK_32
        return (MASK_32 >> begin) | ~(MASK_32 >> (end + 1)) & MASK_32


def booted_machine(image: bytes) -> tuple[Machine, dict[str, int]]:
    """A machine that has done what Dolphin does before it calls the apploader's functions.

    The boot code copies the apploader's code to its load address, calls `entry` with the
    addresses of three slots and reads the functions it finds in them.
    """
    machine = Machine()
    code = struct.unpack_from(f">{(len(image) - APPLOADER_HEADER_SIZE) // 4}I", image, APPLOADER_HEADER_SIZE)
    machine.load_words(APPLOADER_LOAD_ADDRESS, list(code))
    slots = (0x80003100, 0x80003104, 0x80003108)
    for slot in slots:
        machine.memory[slot] = 0
    entry = struct.unpack_from(">I", image, synthetic_disc.APPLOADER_ENTRY_OFFSET)[0]
    machine.run_function(entry, slots)
    functions = {
        name: machine.memory[slot] for name, slot in zip(("init", "main", "close"), slots, strict=True)
    }
    return machine, functions


# -- The files ---------------------------------------------------------------------------------------


def test_the_disc_header_has_the_id_the_magic_number_and_a_name():
    header = boot_bin()
    assert len(header) == BOOT_BIN_SIZE
    assert header[:6] == b"GISE00"
    assert struct.unpack_from(">I", header, 0x1C)[0] == DISC_MAGIC
    assert header[0x20:].split(b"\0")[0] == b"Instruction set conformance test"


def test_the_game_id_is_six_letters_or_digits_and_says_gamecube_and_usa():
    game_id = synthetic_disc.GAME_ID
    assert len(game_id) == 6 and game_id.isalnum()
    assert game_id[0] == "G"  # a GameCube game
    assert game_id[3] == "E"  # the USA, which is what the video mode and the region follow


def test_the_second_header_gives_the_memory_size_and_the_region():
    header = bi2_bin()
    assert len(header) == BI2_BIN_SIZE
    assert struct.unpack_from(">I", header, 0x04)[0] == 24 * 1024 * 1024
    assert struct.unpack_from(">I", header, 0x18)[0] == 1  # the USA
    assert not any(header[0x1C:])  # the rest is empty


def test_the_three_files_are_written_for_the_program(tmp_path, program_path):
    sys_directory = tmp_path / "sys"
    sys_directory.mkdir()
    write_system_files(sys_directory, program_path)
    assert (sys_directory / "boot.bin").read_bytes() == boot_bin()
    assert (sys_directory / "bi2.bin").read_bytes() == bi2_bin()
    assert (sys_directory / "apploader.img").read_bytes() == apploader_img(Dol(program_path))


# -- The apploader's header -------------------------------------------------------------------------


def test_the_header_of_the_apploader_says_where_it_starts_and_how_long_it_is(program_path):
    image = apploader_img(Dol(program_path))
    entry, size, trailer = struct.unpack_from(">III", image, 0x10)
    assert entry == APPLOADER_LOAD_ADDRESS  # the first function is `entry`
    assert trailer == 0
    # Dolphin's folder disc takes a file that is not 0x20 + size + trailer bytes for no apploader.
    assert len(image) == APPLOADER_HEADER_SIZE + size + trailer
    assert size % 4 == 0
    assert image.startswith(b"2001/01/01")


def test_the_program_is_placed_after_the_apploader_padded_and_aligned():
    # Dolphin: AlignUp(0x2440 + file size + 0x20, 0x20).
    assert dol_address_on_disc(0x40) == 0x24A0
    assert dol_address_on_disc(0x41) == 0x24C0
    assert dol_address_on_disc(0x1000) == 0x3460
    assert align_up(0x2440 + 0x1000 + 0x20, 0x20) == dol_address_on_disc(0x1000)


# -- What the apploader does ----------------------------------------------------------------------------


def test_entry_hands_over_three_functions_inside_the_apploader(program_path):
    image = apploader_img(Dol(program_path))
    _, functions = booted_machine(image)
    end_of_code = APPLOADER_LOAD_ADDRESS + len(image) - APPLOADER_HEADER_SIZE
    assert all(APPLOADER_LOAD_ADDRESS <= address < end_of_code for address in functions.values())
    assert len(set(functions.values())) == 3


def test_init_returns_without_needing_anything(program_path):
    machine, functions = booted_machine(apploader_img(Dol(program_path)))
    machine.run_function(functions["init"], (0x81300000,))


def copies_requested(machine: Machine, main: int) -> list[Copy]:
    """What the boot code collects by calling `main` until it returns zero."""
    outputs = (0x81300004, 0x81300008, 0x8130000C)
    for address in outputs:
        machine.memory[address] = 0
    copies = []
    for _ in range(50):
        if machine.run_function(main, outputs) == 0:
            return copies
        copies.append(Copy(*(machine.memory[address] for address in outputs)))
    raise AssertionError("main never said it was finished")


def test_main_asks_for_each_section_of_the_program_once_and_then_says_it_is_finished(program_path):
    dol = Dol(program_path)
    image = apploader_img(dol)
    machine, functions = booted_machine(image)
    dol_address = dol_address_on_disc(len(image))
    expected = [
        Copy(section.address, section.size, dol_address + section.file_offset) for section in dol.sections
    ]
    assert len(expected) == 3
    assert copies_requested(machine, functions["main"]) == expected


def test_the_disc_offsets_of_the_copies_hold_the_bytes_of_each_section(program_path):
    """Lay the files out the way Dolphin does and read each block where `main` says it is."""
    dol = Dol(program_path)
    image = apploader_img(dol)
    machine, functions = booted_machine(image)
    disc = bytearray(dol_address_on_disc(len(image)) + len(dol.data))
    disc[APPLOADER_DISC_OFFSET : APPLOADER_DISC_OFFSET + len(image)] = image
    disc[dol_address_on_disc(len(image)) :] = dol.data
    for copy in copies_requested(machine, functions["main"]):
        assert bytes(disc[copy.disc_offset : copy.disc_offset + copy.length]) == dol.read(
            copy.memory_address, copy.length
        )


def test_close_returns_where_the_program_starts(program_path):
    machine, functions = booted_machine(apploader_img(Dol(program_path)))
    assert machine.run_function(functions["close"]) == CODE_ADDRESS


@pytest.mark.parametrize("length", [1, 4, 0x7FFFFFFF, 0x80000000, 0xFFFFFFFF])
def test_any_length_but_zero_means_there_is_more_to_copy(length):
    machine = Machine()
    machine.load_words(
        APPLOADER_LOAD_ADDRESS, apploader_code(CODE_ADDRESS, [Copy(0x80003100, length, 0x3000)])
    )
    functions = apploader_code_functions(machine)
    outputs = (0x81300004, 0x81300008, 0x8130000C)
    for address in outputs:
        machine.memory[address] = 0
    assert machine.run_function(functions["main"], outputs) == 1


def apploader_code_functions(machine: Machine) -> dict[str, int]:
    """Call `entry` of the code that is already loaded at the load address."""
    slots = (0x80003100, 0x80003104, 0x80003108)
    for slot in slots:
        machine.memory[slot] = 0
    machine.run_function(APPLOADER_LOAD_ADDRESS, slots)
    return {name: machine.memory[slot] for name, slot in zip(("init", "main", "close"), slots, strict=True)}


def test_a_program_with_nothing_to_copy_has_a_table_that_ends_at_once():
    machine = Machine()
    machine.load_words(APPLOADER_LOAD_ADDRESS, apploader_code(CODE_ADDRESS, []))
    assert copies_requested(machine, apploader_code_functions(machine)["main"]) == []
