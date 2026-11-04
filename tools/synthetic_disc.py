"""Make-believe boot files for a disc that holds a test program and no game.

Archivos de arranque inventados para un disco que tiene un programa de prueba y ningún juego.

ModernGekko starts a game the way Dolphin starts a disc from a folder: it reads the disc
header (`sys/boot.bin` and `sys/bi2.bin`), loads the apploader (`sys/apploader.img`) and runs
it. The apploader is a small program of the disc that copies the game's executable
(`sys/main.dol`) into memory, section by section, and tells the loader where the executable
starts. Dolphin runs that code on the emulated processor, so the apploader has to be real
PowerPC code, even when the "game" is a test program.

tools/run_isa_conformance.py normally copies these three files from the user's extracted disc.
This module writes made-up ones, so that the test can run on a machine that has no disc:

  * `boot.bin` and `bi2.bin` hold only what Dolphin needs to take the folder for a GameCube disc
    of the USA: a game ID, the GameCube magic number, a name, the size of the console's memory
    and the region.
  * `apploader.img` is a tiny apploader assembled here. It follows the protocol that Dolphin's
    boot code (Boot_BS2Emu.cpp, `RunApploader`) expects: an entry function that hands over three
    others, `init`, `main` and `close`. `main` returns, once per call, one block to copy (where
    in memory, how many bytes, where on the disc) taken from a table, and zero when the table
    ends; `close` returns the address where the executable starts. The table lists every
    section of the test program's DOL.

Nothing here comes from a game: the files are built from constants and from the DOL they load.

tools/run_isa_conformance.py normalmente copia estos tres archivos del disco extraído del
usuario. Este módulo escribe unos inventados, para que la prueba corra en una máquina sin disco:

  * `boot.bin` y `bi2.bin` solo llevan lo que Dolphin necesita para tomar la carpeta por un
    disco de GameCube de EE. UU.: un ID de juego, el número mágico de GameCube, un nombre, el
    tamaño de la memoria de la consola y la región.
  * `apploader.img` es un apploader mínimo, armado aquí. Sigue el protocolo que espera el código
    de arranque de Dolphin (Boot_BS2Emu.cpp, `RunApploader`): una función de entrada que entrega
    otras tres, `init`, `main` y `close`. `main` devuelve, en cada llamada, un bloque por copiar
    (dónde en memoria, cuántos bytes, dónde en el disco) sacado de una tabla, y cero cuando la
    tabla termina; `close` devuelve la dirección donde empieza el ejecutable. La tabla enumera
    todas las secciones del DOL del programa de prueba.

Nada de esto viene de un juego: los archivos se arman con constantes y con el DOL que cargan.
"""

import struct
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from dol import Dol  # noqa: E402
from ppc_encode import BLR, addi, lis, lwz, neg, or_, ori, srwi, stw  # noqa: E402

# -- The disc header (boot.bin) -----------------------------------------------------------------

# The ID names the module that the recompiler leaves behind, and its fourth letter picks the
# video mode: G is for a GameCube game, IS is for the instruction set, E is for the USA, and the
# publisher is left as 00. A build of ModernGekko that is tied to one game accepts only that
# game's ID, and then the made-up disc has to claim to be that game: see `game_id` below.
GAME_ID = "GISE00"
GAME_ID_LENGTH = len(GAME_ID)
GAME_NAME = b"Instruction set conformance test"
BOOT_BIN_SIZE = 0x440
DISC_MAGIC = 0xC2339F3D  # marks the header of a GameCube disc
DISC_MAGIC_OFFSET = 0x1C
GAME_NAME_OFFSET = 0x20

# -- The second header (bi2.bin) ---------------------------------------------------------------

BI2_BIN_SIZE = 0x2000  # nearly all of it is zero
BI2_SIMULATED_MEMORY_OFFSET = 0x04
BI2_SIMULATED_MEMORY_SIZE = 24 * 1024 * 1024  # the console's main memory
BI2_COUNTRY_OFFSET = 0x18
BI2_COUNTRY_USA = 1

# -- The apploader (apploader.img) ------------------------------------------------------------------

APPLOADER_DISC_OFFSET = 0x2440  # where a disc has the apploader, and where Dolphin's folder disc puts it
APPLOADER_LOAD_ADDRESS = 0x81200000  # where the boot code copies the apploader's code to
APPLOADER_HEADER_SIZE = 0x20  # the date, the entry point, the size of the code and of its trailer
APPLOADER_DATE = b"2001/01/01"
APPLOADER_ENTRY_OFFSET = 0x10
APPLOADER_SIZE_OFFSET = 0x14
APPLOADER_TRAILER_OFFSET = 0x18
DISC_ALIGNMENT = 0x20  # Dolphin puts the executable on the disc at a multiple of this
WORD_SIZE = 4

# The apploader's registers. The boot code passes the three arguments of `entry` and of `main` in
# r3, r4 and r5; r6 to r9 are free for any function to use.
ARGUMENT_0, ARGUMENT_1, ARGUMENT_2 = 3, 4, 5
POINTER, NEXT_ENTRY, FIELD, SCRATCH = 6, 7, 8, 9
RESULT = 3
BYTES_PER_COPY_ENTRY = 3 * WORD_SIZE  # where in memory, how many bytes, where on the disc


@dataclass(frozen=True)
class Copy:
    """One block that the apploader asks the boot code to copy from the disc into memory."""

    memory_address: int
    length: int
    disc_offset: int


def align_up(value: int, alignment: int) -> int:
    return (value + alignment - 1) // alignment * alignment


def boot_bin(game_id: str = GAME_ID) -> bytes:
    """The disc header of a game that does not exist, which says it has the ID `game_id`."""
    if len(game_id) != GAME_ID_LENGTH or not game_id.isascii():
        raise ValueError(f"a game ID is {GAME_ID_LENGTH} ASCII characters, not {game_id!r}")
    header = bytearray(BOOT_BIN_SIZE)
    header[:GAME_ID_LENGTH] = game_id.encode("ascii")
    struct.pack_into(">I", header, DISC_MAGIC_OFFSET, DISC_MAGIC)
    header[GAME_NAME_OFFSET : GAME_NAME_OFFSET + len(GAME_NAME)] = GAME_NAME
    return bytes(header)


def bi2_bin() -> bytes:
    """The second header: the console's memory size and the region, the USA."""
    header = bytearray(BI2_BIN_SIZE)
    struct.pack_into(">I", header, BI2_SIMULATED_MEMORY_OFFSET, BI2_SIMULATED_MEMORY_SIZE)
    struct.pack_into(">I", header, BI2_COUNTRY_OFFSET, BI2_COUNTRY_USA)
    return bytes(header)


def load_address(address: int) -> list[int]:
    """The two instructions that put an address in r6: its upper half with `lis`, its lower with `ori`."""
    return [lis(POINTER, address >> 16), ori(POINTER, POINTER, address & 0xFFFF)]


def entry_function(init_address: int, main_address: int, close_address: int) -> list[int]:
    """entry(init_slot, main_slot, close_slot): store the address of each function in its slot."""
    code = []
    for function_address, slot in (
        (init_address, ARGUMENT_0),
        (main_address, ARGUMENT_1),
        (close_address, ARGUMENT_2),
    ):
        code += [*load_address(function_address), stw(POINTER, 0, slot)]
    return [*code, BLR]


def init_function() -> list[int]:
    """init(): there is nothing to prepare."""
    return [BLR]


def main_function(cursor_address: int) -> list[int]:
    """main(memory_address_out, length_out, disc_offset_out): hand over the next row of the table.

    The word at `cursor_address` holds the address of the row to hand over. The function writes
    the three fields of that row where the boot code looks for them and moves the cursor to the
    row after it. Its result (r3) is 1 when the row has a length and 0 for the row of zeros that
    ends the table: the sign bit of `length | -length` is set for any length but zero. The boot
    code stops calling `main` at the first zero, so the cursor is never moved past that row.
    """
    return [
        *load_address(cursor_address),
        lwz(NEXT_ENTRY, 0, POINTER),
        lwz(FIELD, 0, NEXT_ENTRY),
        stw(FIELD, 0, ARGUMENT_0),
        lwz(FIELD, WORD_SIZE, NEXT_ENTRY),
        stw(FIELD, 0, ARGUMENT_1),
        lwz(SCRATCH, 2 * WORD_SIZE, NEXT_ENTRY),
        stw(SCRATCH, 0, ARGUMENT_2),
        addi(NEXT_ENTRY, NEXT_ENTRY, BYTES_PER_COPY_ENTRY),
        stw(NEXT_ENTRY, 0, POINTER),
        neg(SCRATCH, FIELD),
        or_(SCRATCH, SCRATCH, FIELD),
        srwi(RESULT, SCRATCH, 31),
        BLR,
    ]


def close_function(entry_point: int) -> list[int]:
    """close(): the address where the executable starts."""
    return [lis(RESULT, entry_point >> 16), ori(RESULT, RESULT, entry_point & 0xFFFF), BLR]


def apploader_code(entry_point: int, copies: list[Copy]) -> list[int]:
    """The apploader's instructions and data, as words that start at APPLOADER_LOAD_ADDRESS.

    Layout: `entry`, `init`, `main`, `close`, then the cursor (the word that holds the address of
    the next row of the table), then the table: one row for each copy, and a row of zeros that
    ends it.
    """
    # A function's length does not depend on the addresses in it, so assembling each one with
    # zeros tells where the next one starts.
    entry_length = len(entry_function(0, 0, 0))
    init_length = len(init_function())
    main_length = len(main_function(0))
    close_length = len(close_function(0))

    def address_of_word(index: int) -> int:
        return APPLOADER_LOAD_ADDRESS + WORD_SIZE * index

    init_index = entry_length
    main_index = init_index + init_length
    close_index = main_index + main_length
    cursor_index = close_index + close_length
    table_index = cursor_index + 1

    table = []
    for copy in copies:
        table += [copy.memory_address, copy.length, copy.disc_offset]
    table += [0] * (BYTES_PER_COPY_ENTRY // WORD_SIZE)  # the end of the table: a length of zero

    return [
        *entry_function(
            address_of_word(init_index), address_of_word(main_index), address_of_word(close_index)
        ),
        *init_function(),
        *main_function(address_of_word(cursor_index)),
        *close_function(entry_point),
        address_of_word(table_index),
        *table,
    ]


def dol_address_on_disc(apploader_file_size: int) -> int:
    """Where Dolphin's folder disc puts main.dol: after the apploader, padded and aligned."""
    return align_up(APPLOADER_DISC_OFFSET + apploader_file_size + DISC_ALIGNMENT, DISC_ALIGNMENT)


def apploader_img(program: Dol) -> bytes:
    """An apploader that copies every section of `program`, a DOL, into memory and starts it."""
    # The length of the code does not depend on the values in the table, and the table's disc
    # offsets depend on the length of the code, so the code is assembled twice.
    sections = program.sections
    code_length = len(apploader_code(program.entry, [Copy(0, 0, 0)] * len(sections)))
    dol_address = dol_address_on_disc(APPLOADER_HEADER_SIZE + WORD_SIZE * code_length)
    copies = [Copy(section.address, section.size, dol_address + section.file_offset) for section in sections]
    code = apploader_code(program.entry, copies)
    assert len(code) == code_length

    header = bytearray(APPLOADER_HEADER_SIZE)
    header[: len(APPLOADER_DATE)] = APPLOADER_DATE
    struct.pack_into(">I", header, APPLOADER_ENTRY_OFFSET, APPLOADER_LOAD_ADDRESS)  # `entry` comes first
    struct.pack_into(">I", header, APPLOADER_SIZE_OFFSET, WORD_SIZE * len(code))
    struct.pack_into(">I", header, APPLOADER_TRAILER_OFFSET, 0)
    return bytes(header) + struct.pack(f">{len(code)}I", *code)


def write_system_files(sys_directory: Path, program_path: Path, game_id: str = GAME_ID) -> None:
    """Write boot.bin, bi2.bin and apploader.img for a disc whose main.dol is the DOL at `program_path`."""
    (sys_directory / "boot.bin").write_bytes(boot_bin(game_id))
    (sys_directory / "bi2.bin").write_bytes(bi2_bin())
    (sys_directory / "apploader.img").write_bytes(apploader_img(Dol(program_path)))
