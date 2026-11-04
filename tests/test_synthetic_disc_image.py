"""Tests of the disc image made from a game folder. No game and no ModernGekko needed.

Pruebas de la imagen de disco armada con una carpeta de juego. Sin el juego ni ModernGekko.

The tests read the image back the way a disc reader does (the header tells where the file table
is, the table tells where each file is), so they check the layout and not only that the code that
writes the image agrees with itself. That Dolphin reads the image too is checked where Dolphin
is: the Windows package's workflow extracts one with the package's launcher.
"""

import struct
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from synthetic_disc_image import (  # noqa: E402
    BI2_BIN_DISC_OFFSET,
    HEADER_DOL_OFFSET,
    HEADER_FST_MAXIMUM_SIZE,
    HEADER_FST_OFFSET,
    HEADER_FST_SIZE,
    disc_image,
    file_table,
    write_disc_image,
)

from dol import write_dol  # noqa: E402
from ppc_encode import BLR, lis  # noqa: E402
from synthetic_disc import (  # noqa: E402
    APPLOADER_DISC_OFFSET,
    BI2_BIN_SIZE,
    BOOT_BIN_SIZE,
    DISC_ALIGNMENT,
    DISC_MAGIC,
    dol_address_on_disc,
    write_system_files,
)

GREETING = b"hello from a made-up disc\n"
BINARY_FILE = bytes(range(200))


@pytest.fixture
def game_root(tmp_path) -> Path:
    """A game folder like the ones of the conformance test: made-up boot files, a program, two files."""
    root = tmp_path / "game"
    (root / "sys").mkdir(parents=True)
    (root / "files").mkdir()
    program = write_dol(
        tmp_path / "program.dol",
        text={0: (0x80003100, [lis(3, 1), BLR])},
        data={0: (0x80400000, bytes(range(32)))},
        entry=0x80003100,
    )
    write_system_files(root / "sys", program, "G4AEE9")
    (root / "sys" / "main.dol").write_bytes(program.read_bytes())
    (root / "files" / "greeting.txt").write_bytes(GREETING)
    (root / "files" / "binary.bin").write_bytes(BINARY_FILE)
    return root


def read_word(image: bytes, offset: int) -> int:
    return struct.unpack_from(">I", image, offset)[0]


def read_files(image: bytes) -> dict[str, tuple[int, bytes]]:
    """The files of the image, found the way a disc reader finds them: name -> (offset, contents)."""
    table_offset, table_size = read_word(image, HEADER_FST_OFFSET), read_word(image, HEADER_FST_SIZE)
    table = image[table_offset : table_offset + table_size]
    root_flag_and_name, root_parent, row_count = struct.unpack_from(">III", table, 0)
    assert root_flag_and_name >> 24 == 1  # the first row is a folder
    assert root_parent == 0
    names_offset = row_count * 12
    files = {}
    for row in range(1, row_count):
        flag_and_name_offset, offset, length = struct.unpack_from(">III", table, row * 12)
        assert flag_and_name_offset >> 24 == 0  # a file
        name_start = names_offset + (flag_and_name_offset & 0xFFFFFF)
        name = table[name_start : table.index(b"\0", name_start)].decode("utf-8")
        files[name] = (offset, image[offset : offset + length])
    return files


# -- The header ------------------------------------------------------------------------------------


def test_the_image_starts_with_the_disc_header_of_the_game_folder(game_root):
    image = disc_image(game_root)
    boot_bin = (game_root / "sys" / "boot.bin").read_bytes()
    assert image[:6] == b"G4AEE9"
    assert read_word(image, 0x1C) == DISC_MAGIC
    # Only the four words that say where things are differ from the folder's boot.bin.
    where_it_differs = {offset for offset in range(BOOT_BIN_SIZE) if image[offset] != boot_bin[offset]}
    words_that_say_where = {HEADER_DOL_OFFSET, HEADER_FST_OFFSET, HEADER_FST_SIZE, HEADER_FST_MAXIMUM_SIZE}
    assert all(offset - offset % 4 in words_that_say_where for offset in where_it_differs)


def test_the_second_header_and_the_apploader_sit_where_a_gamecube_disc_has_them(game_root):
    image = disc_image(game_root)
    apploader = (game_root / "sys" / "apploader.img").read_bytes()
    assert (
        image[BI2_BIN_DISC_OFFSET : BI2_BIN_DISC_OFFSET + BI2_BIN_SIZE]
        == (game_root / "sys" / "bi2.bin").read_bytes()
    )
    assert image[APPLOADER_DISC_OFFSET : APPLOADER_DISC_OFFSET + len(apploader)] == apploader


def test_the_executable_is_where_the_apploader_and_the_header_say(game_root):
    image = disc_image(game_root)
    executable = (game_root / "sys" / "main.dol").read_bytes()
    apploader = (game_root / "sys" / "apploader.img").read_bytes()
    offset = read_word(image, HEADER_DOL_OFFSET)
    assert offset == dol_address_on_disc(len(apploader))  # the apploader copies it from there
    assert image[offset : offset + len(executable)] == executable


# -- The files ---------------------------------------------------------------------------------------


def test_every_file_is_found_through_the_file_table_with_its_contents(game_root):
    files = read_files(disc_image(game_root))
    assert {name: contents for name, (_, contents) in files.items()} == {
        "binary.bin": BINARY_FILE,
        "greeting.txt": GREETING,
    }


def test_the_table_the_executable_and_each_file_start_at_a_multiple_of_the_alignment(game_root):
    image = disc_image(game_root)
    offsets = [read_word(image, HEADER_DOL_OFFSET), read_word(image, HEADER_FST_OFFSET)]
    offsets += [offset for offset, _ in read_files(image).values()]
    assert all(offset % DISC_ALIGNMENT == 0 for offset in offsets)


def test_nothing_overlaps_and_the_image_ends_with_the_last_file(game_root):
    image = disc_image(game_root)
    pieces = [
        (read_word(image, HEADER_DOL_OFFSET), len((game_root / "sys" / "main.dol").read_bytes())),
        (read_word(image, HEADER_FST_OFFSET), read_word(image, HEADER_FST_SIZE)),
        *((offset, len(contents)) for offset, contents in read_files(image).values()),
    ]
    pieces.sort()
    for (start, length), (next_start, _) in zip(pieces, pieces[1:], strict=False):
        assert start + length <= next_start
    last_start, last_length = pieces[-1]
    assert len(image) == last_start + last_length + (-(last_start + last_length) % DISC_ALIGNMENT)


def test_the_header_gives_the_table_as_much_room_as_it_takes(game_root):
    image = disc_image(game_root)
    assert read_word(image, HEADER_FST_MAXIMUM_SIZE) == read_word(image, HEADER_FST_SIZE)


def test_a_game_without_files_has_a_table_of_only_the_root_folder(game_root):
    for path in (game_root / "files").iterdir():
        path.unlink()
    image = disc_image(game_root)
    assert read_files(image) == {}
    assert read_word(image, HEADER_FST_SIZE) == 12


def test_the_file_table_lists_the_root_folder_and_then_each_file_with_its_name(game_root):
    table = file_table(["a.bin", "bc.bin"], [0x1000, 0x2000], [5, 6])
    assert table == (
        struct.pack(">III", 0x01000000, 0, 3)  # the root folder: its parent and the number of rows
        + struct.pack(">III", 0, 0x1000, 5)
        + struct.pack(">III", 6, 0x2000, 6)  # "a.bin\0" is six bytes long
        + b"a.bin\0bc.bin\0"
    )


# -- What is refused ---------------------------------------------------------------------------------


def test_a_folder_inside_the_files_is_refused(game_root):
    (game_root / "files" / "a folder").mkdir()
    with pytest.raises(ValueError, match="may only hold files"):
        disc_image(game_root)


def test_a_boot_file_that_is_not_the_size_of_a_disc_header_is_refused(game_root):
    (game_root / "sys" / "boot.bin").write_bytes(b"too short")
    with pytest.raises(ValueError, match="does not have the size of a disc header"):
        disc_image(game_root)


def test_the_image_is_written_to_the_file_asked_for(game_root, tmp_path):
    image_path = tmp_path / "disc.gcm"
    write_disc_image(game_root, image_path)
    assert image_path.read_bytes() == disc_image(game_root)
