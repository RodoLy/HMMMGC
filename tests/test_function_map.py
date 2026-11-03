"""Checks on the real function map. They need your own main.dol, so they only
run when HM_MM_DOL points to it:

Comprobaciones sobre el mapa de funciones real. Necesitan tu propio
main.dol, así que solo corren si HM_MM_DOL apunta a él:

    HM_MM_DOL=orig/G4AEE9/sys/main.dol python3 -m pytest
"""

import os
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from dol import Dol  # noqa: E402
from function_finder import FunctionFinder, find_small_data_bases  # noqa: E402
from sdk_names import name_sdk_functions  # noqa: E402

DOL_PATH = os.environ.get("HM_MM_DOL")
pytestmark = pytest.mark.skipif(not DOL_PATH, reason="set HM_MM_DOL to your extracted main.dol")


@pytest.fixture(scope="module")
def finder() -> FunctionFinder:
    finder = FunctionFinder(Dol(Path(DOL_PATH)))
    finder.run()
    return finder


def test_walker_agrees_with_every_extabindex_entry(finder):
    assert len(finder.exception_index_sizes) == 2597
    assert finder.compare_with_exception_index() == []


def test_no_function_overlaps_another(finder):
    assert finder.overlapping_functions() == []


def test_every_switch_table_is_accepted(finder):
    assert finder.rejected_jump_tables == []


def test_no_branch_leaves_its_function(finder):
    """Conditional branches always stay inside their function, and a plain
    `b` either stays inside or is a tail call to another function's start."""
    for start, size in finder.function_sizes.items():
        end = start + size
        for address in range(start, end, 4):
            instruction = finder.instructions[address]
            target = instruction.branch_target()
            if instruction.is_conditional_branch:
                assert start <= target < end, f"0x{address:08X} in 0x{start:08X}"
            if instruction.is_jump and not start <= target < end:
                assert target in finder.function_sizes, f"0x{address:08X} in 0x{start:08X}"


def test_all_real_code_is_covered(finder):
    covered = set()
    for start, size in finder.function_sizes.items():
        covered.update(range(start, start + size, 4))
    leftovers = [
        address
        for address in finder.instructions
        if address not in covered and not finder._is_filler(address)
    ]
    assert leftovers == []


def test_startup_tables_match_the_dol_header(finder):
    """_rom_copy_info lists the same sections the DOL header does (the DOL
    rounds sizes up to 32 bytes, the table has the exact sizes)."""
    copied = {destination: size for _, destination, size in finder.rom_copy_entries()}
    for section in finder.dol.sections:
        assert section.address in copied
        assert copied[section.address] <= section.size < copied[section.address] + 32


def test_small_data_bases_come_from_init_registers(finder):
    assert find_small_data_bases(finder.dol) == {13: 0x80329080, 2: 0x8032A780}


def test_sdk_init_chain_names(finder):
    names = name_sdk_functions(finder.dol, finder)
    by_name = {name: address for address, name in names.functions.items()}
    assert by_name["OSRegisterVersion"] == 0x8016F840
    assert by_name["OSReport"] == 0x80171504
    assert by_name["OSInit"] == 0x8016EF2C
    assert by_name["GXInit"] == 0x8018F234
    # AX splits its init: AXInit() { AXInitEx(0); }
    assert by_name["AXInit"] == 0x8017FEDC
    assert by_name["AXInitEx"] == 0x8017FF00
    assert len(names.libraries) == 13
