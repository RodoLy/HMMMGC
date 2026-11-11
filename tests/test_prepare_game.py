"""Tests of tools/prepare_game.py with made-up discs and translations. None needs the game.

Pruebas de tools/prepare_game.py con discos y traducciones inventados. Ninguna necesita el juego.

The whole preparation (translator, compiler, the real disc) is tried by hand and on the build
machines; these check the parts that decide what the player is told and what the compiler gets.
"""

import hashlib
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import prepare_game  # noqa: E402
from prepare_game import (  # noqa: E402
    PreparationError,
    check_disc,
    coff_assembly,
    compressed_format,
    disc_image,
    dol_size,
    expand,
    game_sources,
    response_file_word,
)

DOL_OFFSET = 0x3000


def made_up_dol() -> bytes:
    """A DOL with one text section of 0x40 bytes at file offset 0x100."""
    header = bytearray(0x100)
    header[0:4] = (0x100).to_bytes(4, "big")  # text 0's file offset
    header[0x48:0x4C] = (0x80003100).to_bytes(4, "big")  # its address
    header[0x90:0x94] = (0x40).to_bytes(4, "big")  # its size
    return bytes(header) + bytes(range(0x40))


def made_up_disc(path: Path, game_id: bytes = b"G4AEE9", revision: int = 0, magic: bool = True) -> bytes:
    """A disc image with a header and the made-up DOL; returns the DOL."""
    image = bytearray(DOL_OFFSET)
    image[0:6] = game_id
    image[7] = revision
    image[0x20:0x40] = b"Harvest Moon Magical Melody".ljust(0x20, b"\0")
    if magic:
        image[0x1C:0x20] = bytes.fromhex("C2339F3D")
    image[0x420:0x424] = DOL_OFFSET.to_bytes(4, "big")
    dol = made_up_dol()
    path.write_bytes(bytes(image) + dol)
    return dol


@pytest.fixture
def expect_made_up_dol(monkeypatch):
    """Makes the made-up DOL the one the port expects."""
    monkeypatch.setattr(prepare_game, "EXPECTED_DOL_SHA1", hashlib.sha1(made_up_dol()).hexdigest())


def test_the_size_of_a_dol_is_the_end_of_its_last_section():
    assert dol_size(made_up_dol()) == 0x140


def test_the_disc_of_the_port_gives_its_main_dol(tmp_path, expect_made_up_dol):
    image = tmp_path / "disc.iso"
    dol = made_up_disc(image)
    assert check_disc(image) == dol


@pytest.mark.parametrize(
    ("game_id", "revision", "magic", "reason"),
    [
        (b"G4AEE9", 0, False, "not_a_disc"),
        (b"GM4E01", 0, True, "wrong_game"),
        (b"G4AP9B", 0, True, "wrong_version"),  # the same game for another region
        (b"G4AEE9", 1, True, "wrong_version"),
    ],
)
def test_another_disc_is_refused_with_its_reason(
    tmp_path, expect_made_up_dol, game_id, revision, magic, reason
):
    image = tmp_path / "disc.iso"
    made_up_disc(image, game_id, revision, magic)
    with pytest.raises(PreparationError) as refused:
        check_disc(image)
    assert refused.value.reason == reason


def test_a_changed_main_dol_is_refused(tmp_path):
    image = tmp_path / "disc.iso"
    made_up_disc(image)  # its DOL is not the game's
    with pytest.raises(PreparationError) as refused:
        check_disc(image)
    assert refused.value.reason == "modified_game"


def test_a_cut_image_is_not_a_disc(tmp_path):
    image = tmp_path / "disc.iso"
    image.write_bytes(b"\0" * 0x100)
    with pytest.raises(PreparationError) as refused:
        check_disc(image)
    assert refused.value.reason == "not_a_disc"


@pytest.mark.parametrize(
    ("start", "name"),
    [(b"RVZ\x01", "RVZ"), (b"WIA\x01", "WIA"), (b"CISO", "CISO"), (b"\x01\xc0\x0b\xb1", "GCZ")],
)
def test_compressed_images_are_recognised(tmp_path, start, name):
    image = tmp_path / "disc"
    image.write_bytes(start + b"\0" * 60)
    assert compressed_format(image) == name


def test_a_compressed_image_without_nodtool_says_so(tmp_path):
    image = tmp_path / "disc.rvz"
    image.write_bytes(b"RVZ\x01" + b"\0" * 60)
    with pytest.raises(PreparationError) as refused:
        disc_image(image, tmp_path / "out", nodtool=None)
    assert refused.value.reason == "compressed"


def test_a_missing_image_is_not_a_disc(tmp_path):
    with pytest.raises(PreparationError) as refused:
        disc_image(tmp_path / "nothing.iso", tmp_path / "out", nodtool=None)
    assert refused.value.reason == "not_a_disc"


def test_the_data_blobs_become_coff_with_forward_slashes():
    assembly = (
        '.section .rodata,"a",@progbits\n'
        ".globl kData__init\n"
        'kData__init:\n.incbin "C:\\Users\\Ana\\AppData\\Local\\HarvestMoonMM\\_init.bin"\n'
        '.section .note.GNU-stack,"",@progbits\n'
    )
    converted = coff_assembly(assembly)
    assert '.section .rdata,"dr"' in converted
    assert "GNU-stack" not in converted
    assert '.incbin "C:/Users/Ana/AppData/Local/HarvestMoonMM/_init.bin"' in converted


def test_response_file_words_are_quoted_for_clang():
    assert response_file_word("C:/Users/Ana María/obj/a.obj") == '"C:/Users/Ana María/obj/a.obj"'
    assert response_file_word('-DNAME="x"') == '"-DNAME=\\"x\\""'


def test_placeholders_become_the_folders_of_this_computer(tmp_path):
    kit = tmp_path / "kit"
    translation = tmp_path / "translation"
    assert expand(["-I{kit}/include", "-I{translation}", "-O2"], kit, translation) == [
        f"-I{kit.as_posix()}/include",
        f"-I{translation.as_posix()}",
        "-O2",
    ]


def test_the_game_s_sources_are_the_ones_cmake_lists_biggest_first(tmp_path):
    shards = tmp_path / "build_shards"
    for folder, name, size in [
        ("base_common", "small.cpp", 10),
        ("base_common", "big.cpp", 1000),
        ("base_registration", "registration.cpp", 100),
        ("base_dispatch", "dispatch.cpp", 50),
        ("retro_rewind_dispatch", "ignored.cpp", 5000),
    ]:
        (shards / folder).mkdir(parents=True, exist_ok=True)
        (shards / folder / name).write_bytes(b"x" * size)
    (tmp_path / "data_sections_init.cpp").write_bytes(b"x" * 20)
    assert [path.name for path in game_sources(tmp_path)] == [
        "big.cpp",
        "registration.cpp",
        "dispatch.cpp",
        "data_sections_init.cpp",
        "small.cpp",
    ]
