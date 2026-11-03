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
from function_finder import INSTRUCTION_SIZE, FunctionFinder, find_small_data_bases  # noqa: E402
from sdk_names import name_sdk_functions  # noqa: E402

DOL_PATH = os.environ.get("HM_MM_DOL")
pytestmark = pytest.mark.skipif(not DOL_PATH, reason="set HM_MM_DOL to your extracted main.dol")


@pytest.fixture(scope="module")
def finder() -> FunctionFinder:
    finder = FunctionFinder(Dol(Path(DOL_PATH)))
    finder.run()
    return finder


# Known-good totals for G4AEE9. If an improvement to the analysis changes
# them, update them here on purpose, in the same commit, and say why.
EXPECTED_EXCEPTION_INDEX_FUNCTIONS = 2597
EXPECTED_FUNCTIONS = 4486
EXPECTED_JUMP_TABLES = 274
EXPECTED_JUMP_TABLES_WITHOUT_BOUNDS_CHECK = 4
EXPECTED_SDK_LIBRARIES = 13
# The DOL header rounds every section size up to a multiple of 32 bytes.
DOL_SECTION_ALIGNMENT = 32


def test_walker_agrees_with_every_extabindex_entry(finder):
    assert len(finder.exception_index_sizes) == EXPECTED_EXCEPTION_INDEX_FUNCTIONS
    assert finder.compare_with_exception_index() == []


def test_known_good_totals(finder):
    assert len(finder.function_sizes) == EXPECTED_FUNCTIONS
    assert len(finder.jump_tables) == EXPECTED_JUMP_TABLES
    assert len(finder.unbounded_jump_table_bctrs) == EXPECTED_JUMP_TABLES_WITHOUT_BOUNDS_CHECK


def test_no_function_overlaps_another(finder):
    assert finder.overlapping_functions() == []


def test_every_switch_table_is_accepted(finder):
    assert finder.rejected_jump_table_bctrs == []


def test_no_branch_leaves_its_function(finder):
    """Conditional branches always stay inside their function, and a plain
    `b` either stays inside or is a tail call to another function's start."""
    for start, size in finder.function_sizes.items():
        end = start + size
        for address in range(start, end, INSTRUCTION_SIZE):
            instruction = finder.instructions[address]
            target = instruction.branch_target()
            if instruction.is_conditional_branch:
                assert start <= target < end, f"0x{address:08X} in 0x{start:08X}"
            if instruction.is_jump and not start <= target < end:
                assert target in finder.function_sizes, f"0x{address:08X} in 0x{start:08X}"


def test_all_real_code_is_covered(finder):
    covered = set()
    for start, size in finder.function_sizes.items():
        covered.update(range(start, start + size, INSTRUCTION_SIZE))
    leftovers = [
        address
        for address in finder.instructions
        if address not in covered and not finder._is_filler(address)
    ]
    assert leftovers == []


def test_startup_tables_list_exactly_the_dol_sections(finder):
    """_rom_copy_info lists exactly the sections of the DOL header, in both
    directions. The DOL rounds sizes up; the table has the exact sizes."""
    copied = {destination: size for _, destination, size in finder.rom_copy_entries()}
    assert set(copied) == {section.address for section in finder.dol.sections}
    for section in finder.dol.sections:
        exact_size = copied[section.address]
        assert exact_size <= section.size < exact_size + DOL_SECTION_ALIGNMENT


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
    assert len(names.libraries) == EXPECTED_SDK_LIBRARIES
