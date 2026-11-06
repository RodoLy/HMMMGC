"""Tests for tools/write_translator_project.py, which writes the translator's project file
and function map for the native runtime.

Pruebas de tools/write_translator_project.py, que escribe el archivo de proyecto y el
mapa de funciones del traductor para el runtime nativo.
"""

import sys
from pathlib import Path

import yaml

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from symbols import Symbol, SymbolTable  # noqa: E402
from write_translator_project import project_text, translator_map_lines  # noqa: E402


def test_the_map_has_one_line_per_function_in_address_order_and_no_data():
    table = SymbolTable(
        [
            Symbol("OSInit", ".text", 0x8016EF2C, "function", 0x4E0),
            Symbol("string_800034AC", ".init", 0x800034AC, "object", 0x30),
            Symbol("__start", ".init", 0x80003154, "function", 0x15C),
        ]
    )
    assert translator_map_lines(table) == ["80003154 __start", "8016EF2C OSInit"]


def test_the_project_says_where_the_game_its_small_data_and_the_natives_are(tmp_path):
    dol = tmp_path / "disc" / "sys" / "main.dol"
    natives = tmp_path / "runtime" / "src"
    project = yaml.safe_load(project_text(dol, natives, 0x80329080, 0x8032A780))
    assert project["inputs"]["dol"]["path"] == dol.resolve().as_posix()
    assert project["runtime"]["native_registration_root"] == natives.resolve().as_posix()
    # YAML reads 0x... as integers.
    assert project["memory"]["sda_base"] == 0x80329080
    assert project["memory"]["sda2_base"] == 0x8032A780
    assert project["memory"]["base"] == 0x80000000
    assert project["memory"]["size"] == 24 * 1024 * 1024
    assert project["translation"]["function_map"]["path"] == "functions.txt"
    # Relative to the project file, so that the whole translation lives in one folder.
    assert project["workspace_root"] == "."
    assert project["output"]["root"] == "game"
