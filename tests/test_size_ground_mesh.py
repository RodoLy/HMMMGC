"""tools/size_ground_mesh.py on translated functions made up for the test. No game data needed.

tools/size_ground_mesh.py sobre funciones traducidas inventadas para la prueba. Sin datos del
juego."""

import re
import sys
from pathlib import Path

import pytest

TOOLS_DIRECTORY = Path(__file__).resolve().parent.parent / "tools"
sys.path.insert(0, str(TOOLS_DIRECTORY))

import size_ground_mesh  # noqa: E402
from size_ground_mesh import CHANGES, FUNCTION_HEADER, MARKER, change_function  # noqa: E402

from project import REPOSITORY_ROOT  # noqa: E402

GROUND_MESH_HEADER = REPOSITORY_ROOT / "runtime" / "src" / "ground_mesh.h"
GROUND_MESH_SOURCE = REPOSITORY_ROOT / "runtime" / "src" / "ground_mesh.cpp"


def translated_function(address: int, *, leave_out: str | None = None) -> list[str]:
    """A function as the translator would write it: each statement the tool looks for, as
    many times as it expects, between other statements."""
    lines = [FUNCTION_HEADER.format(address), "{", "    ctx->gpr[3] = 0;"]
    for old, _, times in CHANGES[address]:
        if old == leave_out:
            continue
        lines += [f"    {old}", "    r9 = (r9 + 1);"] * times
    return [*lines, "    return;", "}"]


def shard(*functions: list[str]) -> str:
    lines = ['#include "ppc_context.h"', '#include "ppc_natives.h"', ""]
    for function in functions:
        lines += [*function, ""]
    return "\n".join(lines)


@pytest.mark.parametrize("address", sorted(CHANGES))
def test_every_statement_of_the_game_is_replaced(address):
    lines = translated_function(address)
    assert change_function(lines, address)
    body = "\n".join(lines)
    for old, new, times in CHANGES[address]:
        if old not in new:
            assert old not in body
        assert body.count(f"    {new}\n") >= times
    assert lines[2] == f"    {MARKER}"


def test_the_drawing_lends_a_room_around_each_display_list():
    address = 0x80063554
    lines = translated_function(address)
    change_function(lines, address)
    begins = [line for line in lines if "func_801C8958" in line]
    ends = [line for line in lines if "func_801C8924" in line]
    assert len(begins) == len(ends) == 2
    assert all(line.strip().startswith("hmmm_ground_mesh_list_begins();") for line in begins)
    assert all(line.strip().endswith("hmmm_ground_mesh_list_ends();") for line in ends)


def test_a_changed_function_is_left_alone():
    address = 0x80064108
    lines = translated_function(address)
    assert change_function(lines, address)
    again = list(lines)
    assert not change_function(again, address)
    assert again == lines


def test_a_missing_statement_stops_the_tool():
    address = 0x80064108
    missing = CHANGES[address][0][0]
    with pytest.raises(SystemExit, match="0 times, not 1"):
        change_function(translated_function(address, leave_out=missing), address)


def test_a_statement_found_more_times_stops_the_tool():
    address = 0x80064B88
    lines = translated_function(address)
    lines.insert(3, f"    {CHANGES[address][0][0]}")
    with pytest.raises(SystemExit, match="2 times, not 1"):
        change_function(lines, address)


def test_a_loop_left_comparing_with_the_game_size_stops_the_tool():
    address = 0x80063554
    lines = translated_function(address)
    lines.insert(3, "    SetCRResident(cr, xer, 0, static_cast<uint32_t>(r7), static_cast<uint32_t>(19));")
    with pytest.raises(SystemExit, match="still compares with the game's size"):
        change_function(lines, address)


def test_the_loops_that_start_the_game_tables_are_kept():
    address = 0x8006490C
    lines = translated_function(address)
    lines.insert(3, "    SetCRResident(cr, xer, 0, static_cast<uint32_t>(r4), static_cast<uint32_t>(20));")
    assert change_function(lines, address)


def test_the_whole_translation_is_changed_once(tmp_path, monkeypatch, capsys):
    shards = tmp_path / "build_shards" / "base_common"
    shards.mkdir(parents=True)
    addresses = sorted(CHANGES)
    (shards / "shard_0.cpp").write_text(shard(*(translated_function(a) for a in addresses[:3])))
    (shards / "shard_1.cpp").write_text(shard(*(translated_function(a) for a in addresses[3:])))
    monkeypatch.setattr(sys, "argv", ["size_ground_mesh.py", "--translation", str(tmp_path)])
    assert size_ground_mesh.main() == 0
    assert (
        capsys.readouterr().out.strip() == f"ground mesh: {len(CHANGES)} of {len(CHANGES)} functions changed"
    )
    texts = [path.read_text() for path in sorted(shards.glob("*.cpp"))]
    for text in texts:
        # The variables are declared once in each shard, after its includes.
        assert text.count(size_ground_mesh.DECLARATIONS[1]) == 1
        assert text.index(size_ground_mesh.DECLARATIONS[0]) > text.index('#include "ppc_natives.h"')
    assert size_ground_mesh.main() == 0
    assert capsys.readouterr().out.strip() == "ground mesh: already changed"
    assert [path.read_text() for path in sorted(shards.glob("*.cpp"))] == texts


def test_a_copy_of_a_function_inside_another_stops_the_tool(tmp_path, monkeypatch):
    shards = tmp_path / "build_shards" / "base_common"
    shards.mkdir(parents=True)
    copy = ["// inline leaf 0x8006D094", "static void copied() {}"]
    (shards / "shard_0.cpp").write_text(shard(*(translated_function(a) for a in sorted(CHANGES)), copy))
    monkeypatch.setattr(sys, "argv", ["size_ground_mesh.py", "--translation", str(tmp_path)])
    with pytest.raises(SystemExit, match="copied 0x8006D094"):
        size_ground_mesh.main()


def test_the_runtime_defines_what_the_tool_declares():
    declared = {re.search(r"(hmmm_ground_mesh_\w+)", line)[1] for line in size_ground_mesh.DECLARATIONS[1:]}
    header = set(
        re.findall(r"extern (?:uint32_t|float) (hmmm_ground_mesh_\w+);", GROUND_MESH_HEADER.read_text())
    )
    header |= set(
        re.findall(r"^void (hmmm_ground_mesh_\w+)\(\);", GROUND_MESH_HEADER.read_text(), re.MULTILINE)
    )
    source = GROUND_MESH_SOURCE.read_text()
    defined = set(re.findall(r"^(?:uint32_t|float) (hmmm_ground_mesh_\w+) = ", source, re.MULTILINE))
    defined |= set(re.findall(r'^extern "C" void (hmmm_ground_mesh_\w+)\(\) \{', source, re.MULTILINE))
    used = set(
        re.findall(
            r"hmmm_ground_mesh_\w+", "\n".join(new for changes in CHANGES.values() for _, new, _ in changes)
        )
    )
    assert declared == header == defined
    assert used <= declared
