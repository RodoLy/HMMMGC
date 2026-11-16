"""Tests of tools/try_runtime_kit.py: the stand-in game is written as the translator writes it on each
system. (Compiling and linking a kit is what the package workflows do with a real one.)

Pruebas de tools/try_runtime_kit.py: el sustituto del juego se escribe como el traductor lo escribe en
cada sistema.
"""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from try_runtime_kit import stand_in_for  # noqa: E402


def test_on_a_mac_the_stand_in_s_data_are_in_a_mach_o_section_without_a_stack_note(tmp_path):
    blobs = (stand_in_for("Darwin", tmp_path) / "data_sections_init_blobs.S").read_text(encoding="utf-8")
    assert ".section __TEXT,__const" in blobs
    assert "@progbits" not in blobs


def test_on_linux_the_stand_in_is_as_it_is(tmp_path):
    game = stand_in_for("Linux", tmp_path)
    assert '.section .rodata,"a",@progbits' in (game / "data_sections_init_blobs.S").read_text(
        encoding="utf-8"
    )
    assert (game / "data_sections_init.cpp").is_file()
