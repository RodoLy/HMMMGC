"""DOL reader tests on a tiny hand-built DOL, so no game data is needed.

Pruebas del lector de DOL con un DOL mínimo hecho a mano, sin datos del juego."""

import struct
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from dol import Dol  # noqa: E402

TEXT_ADDRESS = 0x80003100
DATA_ADDRESS = 0x80004000


def build_dol(path: Path) -> None:
    """One text section with two instructions, one data section with a string."""
    text = struct.pack(">II", 0x38600001, 0x4E800020)  # li r3, 1 ; blr
    data = b"Harvest Moon\0\0\0\0"

    offsets = [0] * 18
    addresses = [0] * 18
    sizes = [0] * 18
    offsets[0], addresses[0], sizes[0] = 0x100, TEXT_ADDRESS, len(text)
    offsets[7], addresses[7], sizes[7] = 0x100 + len(text), DATA_ADDRESS, len(data)

    header = struct.pack(">18I18I18I3I", *offsets, *addresses, *sizes, 0x80005000, 0x100, TEXT_ADDRESS)
    header = header.ljust(0x100, b"\0")
    path.write_bytes(header + text + data)


def test_sections_and_address_translation(tmp_path):
    build_dol(tmp_path / "test.dol")
    dol = Dol(tmp_path / "test.dol")

    assert [section.name for section in dol.sections] == ["text0", "data0"]
    assert dol.entry == TEXT_ADDRESS
    assert dol.offset_of(TEXT_ADDRESS + 4) == 0x104
    assert dol.address_of(0x104) == TEXT_ADDRESS + 4
    assert dol.u32(TEXT_ADDRESS + 4) == 0x4E800020


def test_code_data_and_bss_are_told_apart(tmp_path):
    build_dol(tmp_path / "test.dol")
    dol = Dol(tmp_path / "test.dol")

    assert dol.is_code(TEXT_ADDRESS)
    assert not dol.is_code(DATA_ADDRESS)
    assert dol.is_mapped(0x80005010)  # inside .bss
    assert not dol.is_mapped(0x80006000)


def test_strings(tmp_path):
    build_dol(tmp_path / "test.dol")
    dol = Dol(tmp_path / "test.dol")

    assert dol.c_string(DATA_ADDRESS) == "Harvest Moon"
    assert dol.find_bytes(b"Moon") == [DATA_ADDRESS + 8]
