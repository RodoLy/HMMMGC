"""Measuring how much code the lockstep verifier compared. No game data needed.

Medición de cuánto código comparó el verificador lockstep. Sin datos del juego.

The code is a tiny DOL built by hand: two functions the "run" enters, and two
it never does.
"""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))

from dol import (
    Dol,  # noqa: E402
    write_dol,  # noqa: E402
)
from lockstep_coverage import (  # noqa: E402
    format_report,
    main,
    measure,
    read_verified_addresses,
)
from ppc_encode import BLR, bl, li, ori  # noqa: E402
from symbols import Symbol, SymbolTable  # noqa: E402

ADD_AND_RECORD = 0x7C642A15  # add. r3,r4,r5
FADDS = 0xEC22182A  # fadds f1,f2,f3
PADDING = 0x00000000  # not an instruction

# Four functions, laid end to end from the start of the text section.
ENTRY_FUNCTION = 0x80003100  # the run enters it and compares all of it
HELPER_FUNCTION = 0x80003110  # the run enters it but compares only the first instruction
NEVER_ENTERED_SMALL = 0x80003120  # has a padding word that must not count as an instruction
NEVER_ENTERED_BIG = 0x80003134  # the biggest function nobody entered
UNKNOWN_CODE = 0x80003150  # the run compared an instruction here, but no symbol holds it

FUNCTION_WORDS = {
    ENTRY_FUNCTION: [li(3, 0), ADD_AND_RECORD, bl(ENTRY_FUNCTION + 8, HELPER_FUNCTION), BLR],
    HELPER_FUNCTION: [ori(3, 3, 1), FADDS, BLR],
    NEVER_ENTERED_SMALL: [li(3, 1), li(4, 2), li(5, 3), BLR, PADDING],
    NEVER_ENTERED_BIG: [li(3, 1), li(4, 2), li(5, 3), li(6, 4), li(7, 5), BLR],
}
# The words of the whole section, so that UNKNOWN_CODE has an instruction to decode.
TEXT_WORDS = [0] * ((UNKNOWN_CODE + 4 - ENTRY_FUNCTION) // 4)
for start, words in FUNCTION_WORDS.items():
    first = (start - ENTRY_FUNCTION) // 4
    TEXT_WORDS[first : first + len(words)] = words
TEXT_WORDS[-1] = BLR


@pytest.fixture
def dol(tmp_path) -> Dol:
    return Dol(
        write_dol(
            tmp_path / "test.dol", text={0: (ENTRY_FUNCTION, TEXT_WORDS)}, data={}, entry=ENTRY_FUNCTION
        )
    )


@pytest.fixture
def symbols() -> SymbolTable:
    table = SymbolTable(
        Symbol(name, ".text", start, "function", 4 * len(FUNCTION_WORDS[start]))
        for name, start in [
            ("entry_function", ENTRY_FUNCTION),
            ("helper_function", HELPER_FUNCTION),
            ("never_entered_small", NEVER_ENTERED_SMALL),
            ("never_entered_big", NEVER_ENTERED_BIG),
        ]
    )
    table.add(Symbol("a_table", ".data", 0x80004000, "object", 0x40))  # not a function: ignored
    return table


def addresses(*starts_and_counts: tuple[int, int]) -> set[int]:
    """The addresses of `count` consecutive instructions from each start."""
    return {start + 4 * index for start, count in starts_and_counts for index in range(count)}


# The run compared all of entry_function, the first instruction of helper_function, and one
# instruction of code that no symbol describes.
RUN_ADDRESSES = addresses((ENTRY_FUNCTION, 4), (HELPER_FUNCTION, 1), (UNKNOWN_CODE, 1))


# -- Reading coverage files ---------------------------------------------------


def test_addresses_are_read_from_one_or_more_files():
    addresses_read, unreadable = read_verified_addresses(
        ["0x80003100\n0x80003104\n", "0x80003104\n\n0x80003200\n"]
    )
    assert addresses_read == {0x80003100, 0x80003104, 0x80003200}  # merged, duplicates once
    assert unreadable == 0


def test_lines_that_are_not_addresses_are_counted_not_trusted():
    addresses_read, unreadable = read_verified_addresses(
        ["0x80003100\nnot an address\n[lockstep] summary\n0x8000\n"]
    )
    assert addresses_read == {0x80003100}
    assert unreadable == 3


# -- Measuring ------------------------------------------------------------------


def test_instructions_functions_and_forms_are_counted(dol, symbols):
    coverage = measure(dol, symbols, RUN_ADDRESSES)
    # 4 + 3 + 4 + 6 instructions; the padding word of never_entered_small is not one.
    assert coverage.instructions == 17
    assert coverage.verified_instructions == 5
    assert (coverage.functions, coverage.functions_touched) == (4, 2)
    assert coverage.verified_outside_functions == 1


def test_the_biggest_untouched_functions_come_first(dol, symbols):
    coverage = measure(dol, symbols, RUN_ADDRESSES)
    assert [function.name for function in coverage.untouched_functions] == [
        "never_entered_big",
        "never_entered_small",
    ]


def test_each_form_counts_its_sites_and_the_compared_ones(dol, symbols):
    forms = measure(dol, symbols, RUN_ADDRESSES).forms
    assert (forms["addi"].sites, forms["addi"].verified_sites) == (9, 1)  # li is addi: aliases are not forms
    assert (forms["add."].sites, forms["add."].verified_sites) == (1, 1)
    assert (forms["bl"].sites, forms["bl"].verified_sites) == (1, 1)
    assert (forms["bclr"].sites, forms["bclr"].verified_sites) == (4, 1)  # blr
    assert (forms["fadds"].sites, forms["fadds"].verified_sites) == (1, 0)
    assert "illegal" not in forms  # the padding word was never compared, so it is not counted


def test_an_instruction_the_table_cannot_name_but_the_run_compared_is_not_hidden(dol, symbols):
    padding_address = NEVER_ENTERED_SMALL + 16
    coverage = measure(dol, symbols, RUN_ADDRESSES | {padding_address})
    assert coverage.forms["illegal"].verified_sites == 1  # a hole in the name table shows up
    assert coverage.instructions == 18


def test_a_symbol_outside_the_dol_is_skipped(dol, symbols):
    symbols.add(Symbol("hardware_register", ".text", 0xCC006000, "function", 4))
    assert measure(dol, symbols, RUN_ADDRESSES).functions == 4


# -- Report -----------------------------------------------------------------------


def test_the_report_says_what_was_compared_and_what_was_not(dol, symbols):
    coverage = measure(dol, symbols, RUN_ADDRESSES)
    report = format_report(["run.txt"], coverage, len(RUN_ADDRESSES))
    assert "Lockstep coverage of run.txt" in report
    assert "5 of 17  (29.4%)" in report
    assert "2 of 4  (50.0%)" in report
    assert "1 compared instructions are in no function of symbols.txt" in report
    assert "never compared:          1" in report
    # fadds is the only form the run never compared.
    never_compared = report.split("Forms the run never compared")[1].split("\n\n")[0]
    assert "fadds" in never_compared
    assert "bclr" not in never_compared
    # The untouched functions are listed biggest first, with their size in instructions.
    assert report.index("never_entered_big") < report.index("never_entered_small")
    assert "6 instructions  never_entered_big (0x80003134)" in report


def test_the_list_of_forms_is_cut_at_the_limit_unless_all_are_asked_for(dol, symbols):
    coverage = measure(dol, symbols, set())  # a run that compared nothing: every form is "never compared"
    cut = format_report(["run.txt"], coverage, 0, lines_per_list=2)
    assert "... and 4 more (--forms lists them all)" in cut
    everything = format_report(["run.txt"], coverage, 0, show_all_forms=True, lines_per_list=2)
    assert "Every form:" in everything
    assert "... and 4 more (--forms" not in everything


def test_a_run_that_compared_everything_has_no_never_compared_section(dol, symbols):
    every_instruction = addresses(
        (ENTRY_FUNCTION, 4), (HELPER_FUNCTION, 3), (NEVER_ENTERED_SMALL, 4), (NEVER_ENTERED_BIG, 6)
    )
    coverage = measure(dol, symbols, every_instruction)
    report = format_report(["run.txt"], coverage, len(every_instruction))
    assert "Forms the run never compared" not in report
    assert "17 of 17  (100.0%)" in report
    assert "4 of 4  (100.0%)" in report


# -- Command line -----------------------------------------------------------------


@pytest.fixture
def command_line(tmp_path, dol, symbols):
    symbols_path = tmp_path / "symbols.txt"
    symbols.write(symbols_path)
    return ["--dol", str(dol.path), "--symbols", str(symbols_path)]


def write_coverage(path: Path, addresses_to_write: set[int]) -> Path:
    path.write_text("".join(f"0x{address:08X}\n" for address in sorted(addresses_to_write)))
    return path


def test_the_tool_merges_several_files(tmp_path, command_line, capsys):
    first = write_coverage(tmp_path / "first.txt", addresses((ENTRY_FUNCTION, 4)))
    second = write_coverage(tmp_path / "second.txt", addresses((HELPER_FUNCTION, 3)))
    assert main([str(first), str(second), *command_line]) == 0
    output = capsys.readouterr().out
    assert "Lockstep coverage of first.txt, second.txt" in output
    assert "7 of 17" in output  # 4 from the first run, 3 from the second


def test_an_empty_coverage_file_is_an_error(tmp_path, command_line, capsys):
    empty = write_coverage(tmp_path / "empty.txt", set())
    assert main([str(empty), *command_line]) == 1
    assert "closed cleanly" in capsys.readouterr().err


def test_missing_files_are_reported_by_name(tmp_path, command_line):
    with pytest.raises(SystemExit):
        main([str(tmp_path / "missing.txt"), *command_line])
