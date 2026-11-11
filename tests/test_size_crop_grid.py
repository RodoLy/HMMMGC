"""tools/size_crop_grid.py on translated functions made up for the test. No game data needed.

tools/size_crop_grid.py sobre funciones traducidas inventadas para la prueba. Sin datos del
juego."""

import re
import sys
from pathlib import Path

import pytest

TOOLS_DIRECTORY = Path(__file__).resolve().parent.parent / "tools"
sys.path.insert(0, str(TOOLS_DIRECTORY))

import size_crop_grid  # noqa: E402
from size_crop_grid import CHANGES, FUNCTION_HEADER, MARKER, change_function  # noqa: E402

from project import REPOSITORY_ROOT  # noqa: E402

CROP_GRID_HEADER = REPOSITORY_ROOT / "runtime" / "src" / "crop_grid.h"
CROP_GRID_SOURCE = REPOSITORY_ROOT / "runtime" / "src" / "crop_grid.cpp"


def translated_function(address: int, *, loop_form: int = 0, leave_out: str | None = None) -> list[str]:
    """A function as the translator would write it: each statement the tool looks for, as
    many times as it expects, between other statements. `loop_form` picks which of the two
    forms of each loop comparison the function uses."""
    lines = [FUNCTION_HEADER.format(address), "{", "    ctx->gpr[3] = 0;"]
    for old, _, times in CHANGES[address]:
        is_other_form = size_crop_grid.is_loop_comparison(old) and not old.startswith(
            size_crop_grid.OPTIONAL_PREFIXES[loop_form]
        )
        if is_other_form or old == leave_out:
            continue
        # A comparison written as the branch opens a block.
        statement = [f"    {old}", "    }"] if old.endswith("{") else [f"    {old}"]
        lines += [*statement, "    r9 = (r9 + 1);"] * times
    return [*lines, "    return;", "}"]


def shard(*functions: list[str]) -> str:
    lines = ['#include "ppc_context.h"', '#include "ppc_natives.h"', ""]
    for function in functions:
        lines += [*function, ""]
    return "\n".join(lines)


@pytest.mark.parametrize("loop_form", [0, 1])
@pytest.mark.parametrize("address", sorted(CHANGES))
def test_every_statement_of_the_game_is_replaced(address, loop_form):
    lines = translated_function(address, loop_form=loop_form)
    assert change_function(lines, address)
    body = "\n".join(lines)
    for old, new, times in CHANGES[address]:
        if old not in new:
            assert old not in body
        if not size_crop_grid.is_loop_comparison(old) or old.startswith(
            size_crop_grid.OPTIONAL_PREFIXES[loop_form]
        ):
            assert body.count(f"    {new}\n") >= times
    assert "static_cast<uint32_t>(25)" not in body
    assert "* 800)" not in body and "* 100)" not in body and "* 50)" not in body
    assert lines[2] == f"    {MARKER}"


def test_a_changed_function_is_left_alone():
    address = 0x80072B0C
    lines = translated_function(address)
    assert change_function(lines, address)
    again = list(lines)
    assert not change_function(again, address)
    assert again == lines


def test_a_missing_statement_stops_the_tool():
    address = 0x80073628
    missing = CHANGES[address][0][0]
    with pytest.raises(SystemExit, match="0 times, not 1"):
        change_function(translated_function(address, leave_out=missing), address)


def test_a_statement_found_more_times_stops_the_tool():
    address = 0x8007351C
    lines = translated_function(address)
    extra = CHANGES[address][0][0]
    lines.insert(3, f"    {extra}")
    with pytest.raises(SystemExit, match="2 times, not 1"):
        change_function(lines, address)


def test_a_loop_left_comparing_with_25_stops_the_tool():
    address = 0x800738CC
    lines = translated_function(address)
    lines.insert(3, "    if ((static_cast<uint32_t>(r7) < static_cast<uint32_t>(25))) {")
    lines.insert(4, "    }")
    with pytest.raises(SystemExit, match="still compares with 25"):
        change_function(lines, address)


def test_the_whole_translation_is_changed_once(tmp_path, monkeypatch, capsys):
    shards = tmp_path / "build_shards" / "base_common"
    shards.mkdir(parents=True)
    addresses = sorted(CHANGES)
    (shards / "shard_0.cpp").write_text(shard(*(translated_function(a) for a in addresses[:4])))
    (shards / "shard_1.cpp").write_text(shard(*(translated_function(a) for a in addresses[4:])))
    monkeypatch.setattr(sys, "argv", ["size_crop_grid.py", "--translation", str(tmp_path)])
    assert size_crop_grid.main() == 0
    assert capsys.readouterr().out.strip() == f"crop grid: {len(CHANGES)} of {len(CHANGES)} functions changed"
    texts = [path.read_text() for path in sorted(shards.glob("*.cpp"))]
    for text in texts:
        # The variables are declared once in each shard, after its includes.
        assert text.count(size_crop_grid.DECLARATIONS[1]) == 1
        assert text.index(size_crop_grid.DECLARATIONS[0]) > text.index('#include "ppc_natives.h"')
    assert size_crop_grid.main() == 0
    assert capsys.readouterr().out.strip() == "crop grid: already changed"
    assert [path.read_text() for path in sorted(shards.glob("*.cpp"))] == texts


def test_a_copy_of_a_function_inside_another_stops_the_tool(tmp_path, monkeypatch):
    shards = tmp_path / "build_shards" / "base_common"
    shards.mkdir(parents=True)
    copy = ["// inline leaf 0x8006D1F0", "static void copied() {}"]
    (shards / "shard_0.cpp").write_text(shard(*(translated_function(a) for a in sorted(CHANGES)), copy))
    monkeypatch.setattr(sys, "argv", ["size_crop_grid.py", "--translation", str(tmp_path)])
    with pytest.raises(SystemExit, match="copied 0x8006D1F0"):
        size_crop_grid.main()


def test_the_runtime_defines_the_variables_the_tool_declares():
    declared = {re.search(r"(hmmm_crop_grid_\w+)", line)[1] for line in size_crop_grid.DECLARATIONS[1:]}
    header = set(re.findall(r"extern uint32_t (hmmm_crop_grid_\w+);", CROP_GRID_HEADER.read_text()))
    defined = set(
        re.findall(r"^uint32_t (hmmm_crop_grid_\w+) = ", CROP_GRID_SOURCE.read_text(), re.MULTILINE)
    )
    used = set(
        re.findall(
            r"hmmm_crop_grid_\w+", "\n".join(new for changes in CHANGES.values() for _, new, _ in changes)
        )
    )
    assert declared == header == defined
    assert used <= declared
