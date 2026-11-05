"""The SDK libraries: which one a name belongs to, and where each one sits in the program.

Las bibliotecas del SDK: a cuál pertenece un nombre y dónde está cada una en el programa.

The runs are tested on small hand-made lists, and, because symbols.txt is part of the repository, on the real
symbol table too: it must give the library ranges that docs/FUNCTION_MAP.md quotes.
"""

import sys
from dataclasses import dataclass
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from project import SYMBOLS_PATH  # noqa: E402
from sdk_libraries import (  # noqa: E402
    LibraryMember,
    LibraryRun,
    library_members,
    library_of,
    library_runs,
)
from symbols import Symbol, SymbolTable  # noqa: E402

# ------------------------------------------------------------------------------- library prefixes


@pytest.mark.parametrize(
    ("name", "library"),
    [
        ("OSInit", "OS"),
        ("__OSDispatchInterrupt", "OS"),  # the library's private functions start with underscores
        ("GXSetViewport", "GX"),
        ("ARQInit", "ARQ"),  # not AR
        ("ARStartDMA", "AR"),
        ("AXFXGetHooks", "AXFX"),  # not AX
        ("AXInit", "AX"),
        ("C_MTXLookAt", "MTX"),
        ("PSMTXMultVec", "MTX"),
        ("DCFlushRange", "OS"),  # the cache functions are part of the OS library
        ("TRKNubMainLoop", "TRK"),
        ("SIGetType", "SI"),
        ("Airplane", None),  # a word that merely starts with the letters AI
        ("OnReset", None),
        ("WriteCallback", None),
        ("memcpy", None),
        ("OS", "OS"),
    ],
)
def test_the_library_comes_from_the_prefix_of_the_name(name, library):
    assert library_of(name) == library


# ------------------------------------------------------------------------------------ library runs


@dataclass(frozen=True)
class Placed:
    """A function at an address range, saying which library its name belongs to (or none)."""

    address: int
    end: int
    library: str | None


def test_neighbouring_functions_of_one_library_make_one_run_that_ends_where_the_last_one_ends():
    functions = [Placed(0x1000, 0x1010, "GX"), Placed(0x1010, 0x1040, "GX"), Placed(0x1040, 0x1044, "GX")]
    assert library_runs(functions) == [LibraryRun("GX", 0x1000, 0x1044, 3)]


def test_the_functions_may_come_in_any_order():
    functions = [Placed(0x1040, 0x1050, "DVD"), Placed(0x1000, 0x1020, "GX"), Placed(0x1020, 0x1040, "GX")]
    assert library_runs(functions) == [
        LibraryRun("GX", 0x1000, 0x1040, 2),
        LibraryRun("DVD", 0x1040, 0x1050, 1),
    ]


def test_a_library_that_comes_back_after_another_one_starts_a_new_run():
    functions = [Placed(0x1000, 0x1010, "GX"), Placed(0x1010, 0x1020, "OS"), Placed(0x1020, 0x1030, "GX")]
    assert [run.library for run in library_runs(functions)] == ["GX", "OS", "GX"]


def test_a_function_whose_name_says_no_library_does_not_split_the_run_it_falls_in():
    functions = [Placed(0x1000, 0x1010, "GX"), Placed(0x1010, 0x1020, None), Placed(0x1020, 0x1030, "GX")]
    assert library_runs(functions) == [LibraryRun("GX", 0x1000, 0x1030, 2)]  # the helper is inside, uncounted


def test_without_functions_there_are_no_runs():
    assert library_runs([]) == []
    assert library_runs([Placed(0x1000, 0x1010, None)]) == []


def test_a_run_holds_its_first_address_and_stops_before_its_end_address():
    run = LibraryRun("GX", 0x1000, 0x1040, 3)
    assert not run.contains(0x0FFC)
    assert run.contains(0x1000)
    assert run.contains(0x103C)
    assert not run.contains(0x1040)  # the end is the address right after the last byte


# ----------------------------------------------------------------- the members in a symbol table


def function(name: str, address: int, size: int = 0x20, kind: str = "function") -> Symbol:
    return Symbol(name, ".text", address, kind, size)


def test_only_functions_with_a_real_name_that_says_a_library_are_members():
    symbols = SymbolTable(
        [
            function("GXInit", 0x1000, 0x600),
            function("fn_80001600", 0x1600),  # a placeholder: its name says nothing yet
            function("memcpy", 0x1620),  # a real name, but no library
            function("DVDOpen", 0x1640, 0x80),
            function("OSReport", 0x16C0, 0x10, kind="object"),  # not a function
        ]
    )
    assert library_members(symbols) == [
        LibraryMember(0x1000, 0x1600, "GX"),
        LibraryMember(0x1640, 0x16C0, "DVD"),
    ]


def test_the_members_are_in_address_order_and_make_runs():
    symbols = SymbolTable(
        [function("GXSetCullMode", 0x1020), function("GXInit", 0x1000), function("VIInit", 0x1040)]
    )
    assert library_runs(library_members(symbols)) == [
        LibraryRun("GX", 0x1000, 0x1040, 2),
        LibraryRun("VI", 0x1040, 0x1060, 1),
    ]


# ------------------------------------------------------------------------------ the real symbols

# The ranges in docs/FUNCTION_MAP.md, now from the names written in symbols.txt alone, without Dolphin's
# database. The one difference is the AX run. The documented table shows AX as the whole stretch up to DSP,
# but only two functions named by the version rule (AXInit and AXInitEx) say AX in their names, because the
# database knows almost nothing of that library.
RUNS_IN_SYMBOLS_TXT = [
    ("OS", 0x8016EDA0, 0x80175778),
    ("MTX", 0x8017580C, 0x801759C8),
    ("DVD", 0x801759C8, 0x80179BB0),
    ("VI", 0x80179DF0, 0x8017BBB0),
    ("PAD", 0x8017BF98, 0x8017D73C),
    ("AI", 0x8017D780, 0x8017E008),
    ("AR", 0x8017E04C, 0x8017FB40),
    ("ARQ", 0x8017FB40, 0x8017FEDC),
    ("AX", 0x8017FEDC, 0x8017FF4C),
    ("DSP", 0x801877FC, 0x80188300),
    ("CARD", 0x80188338, 0x8018ED98),
    ("GX", 0x8018EDE0, 0x80195F98),
    ("TRK", 0x80195F98, 0x8019B5F0),
    ("DB", 0x801A7A14, 0x801A848C),
    ("EXI", 0x801A8580, 0x801A9E68),
    ("OS", 0x801A9FF4, 0x801AA1B0),
    ("SI", 0x801AA420, 0x801ABC18),
]


def test_the_names_in_symbols_txt_give_the_library_ranges_the_function_map_quotes():
    runs = library_runs(library_members(SymbolTable.read(SYMBOLS_PATH)))
    assert [(run.library, run.first_address, run.end_address) for run in runs] == RUNS_IN_SYMBOLS_TXT
