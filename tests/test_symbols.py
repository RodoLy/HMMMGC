"""symbols.txt reading, writing and name preservation. No game data needed.

Lectura, escritura y conservación de nombres de symbols.txt. Sin datos del juego."""

import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from export_dolrecomp_map import dolrecomp_map_lines  # noqa: E402
from symbols import Symbol, SymbolTable  # noqa: E402


def make_table() -> SymbolTable:
    return SymbolTable(
        [
            Symbol("fn_80003100", ".init", 0x80003100, "function", 0x40),
            Symbol("OSInit", ".text", 0x8016EF2C, "function", 0x4E0),
            Symbol("jumptable_80202AB4", ".data", 0x80202AB4, "object", 0x30, scope="local"),
        ]
    )


def test_lines_use_decomp_toolkit_syntax():
    lines = [symbol.to_line() for symbol in make_table()]
    assert lines == [
        "fn_80003100 = .init:0x80003100; // type:function size:0x40",
        "OSInit = .text:0x8016EF2C; // type:function size:0x4E0",
        "jumptable_80202AB4 = .data:0x80202AB4; // type:object size:0x30 scope:local",
    ]


def test_write_then_read_gives_the_same_symbols(tmp_path):
    path = tmp_path / "symbols.txt"
    make_table().write(path)
    reread = SymbolTable.read(path)
    assert [symbol.to_line() for symbol in reread] == [symbol.to_line() for symbol in make_table()]


def test_describe_names_addresses_inside_symbols():
    table = make_table()
    assert table.describe(0x8016EF2C) == "OSInit"
    assert table.describe(0x8016EF3C) == "OSInit+0x10"
    assert table.describe(0x8016F500) == "0x8016F500"  # past the end of OSInit


def test_hand_given_names_survive_regeneration():
    previous = SymbolTable([Symbol("__check_pad3", ".init", 0x80003100, "function", 0x40)])
    regenerated = make_table()
    warnings = regenerated.keep_names_from(previous)
    assert warnings == []
    assert regenerated.by_address[0x80003100].name == "__check_pad3"


def test_generated_names_do_not_overwrite_better_ones():
    previous = SymbolTable([Symbol("fn_8016EF2C", ".text", 0x8016EF2C, "function", 0x4E0)])
    regenerated = make_table()
    regenerated.keep_names_from(previous)
    assert regenerated.by_address[0x8016EF2C].name == "OSInit"


def test_conflicting_names_keep_the_human_one_and_warn():
    previous = SymbolTable([Symbol("MyOSInit", ".text", 0x8016EF2C, "function", 0x4E0)])
    regenerated = make_table()
    warnings = regenerated.keep_names_from(previous)
    assert regenerated.by_address[0x8016EF2C].name == "MyOSInit"
    assert len(warnings) == 1 and "OSInit" in warnings[0]


def test_dolrecomp_map_lists_functions_only():
    table = make_table()
    table.add(Symbol("_rom_copy_info", ".init", 0x80005544, "object", 0x84))
    assert dolrecomp_map_lines(table) == [
        "80003100 00000040 fn_80003100",
        "8016EF2C 000004E0 OSInit",
    ]
