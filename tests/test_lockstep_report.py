"""Sorting lockstep-verifier divergences by cause. No game data needed.

Clasificación por causa de las divergencias del verificador lockstep. Sin
datos del juego.

The code the tests classify is a tiny DOL built by hand, with one function
for each shape the loop check has to tell apart.
"""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))

from dol import Dol  # noqa: E402
from lockstep_report import (  # noqa: E402
    EXIT_ALL_EXPLAINED,
    EXIT_NEEDS_A_PERSON,
    EXIT_NOT_A_VERIFIER_LOG,
    Cause,
    classify,
    classify_all,
    ends_inside_loop,
    exit_status,
    format_report,
    main,
    only_result_flags_of_fpscr_differ,
    parse_fields,
    parse_log,
)
from symbols import Symbol, SymbolTable  # noqa: E402
from synthetic import BLR, addi, b, bgt, bl, build_dol, cmplwi, li  # noqa: E402

# Four functions, one per shape of code the loop check has to tell apart.
COUNTING_LOOP = 0x80003100  # a loop whose back-edge is the bgt at 0x8000310C
STRAIGHT_LINE = 0x80003114  # no branches at all
TAIL_CALLER = 0x80003120  # ends with a jump into another function
RECURSIVE = 0x80003130  # calls itself with bl

# Addresses inside those functions.
LOOP_HEADER = COUNTING_LOOP + 4
LOOP_BODY_END = COUNTING_LOOP + 8  # the cmplwi, just before the back-edge
AFTER_THE_LOOP = COUNTING_LOOP + 0x10  # the blr that follows the back-edge

COUNTING_LOOP_WORDS = [
    li(3, 0),  # 80003100
    addi(3, 3, 1),  # 80003104  <- loop header
    cmplwi(3, 10),  # 80003108
    bgt(COUNTING_LOOP + 0xC, LOOP_HEADER),  # 8000310C  back-edge
    BLR,  # 80003110
]
STRAIGHT_LINE_WORDS = [li(3, 0), addi(3, 3, 1), BLR]
TAIL_CALLER_WORDS = [li(3, 1), addi(3, 3, 1), b(TAIL_CALLER + 8, COUNTING_LOOP)]
RECURSIVE_WORDS = [li(3, 0), li(4, 0), bl(RECURSIVE + 8, RECURSIVE), BLR]

ALL_WORDS = {
    COUNTING_LOOP: COUNTING_LOOP_WORDS,
    STRAIGHT_LINE: STRAIGHT_LINE_WORDS,
    TAIL_CALLER: TAIL_CALLER_WORDS,
    RECURSIVE: RECURSIVE_WORDS,
}


@pytest.fixture
def dol(tmp_path) -> Dol:
    # One text section from 0x80003100 to the end of the last function.
    words = [0] * ((RECURSIVE + 4 * len(RECURSIVE_WORDS) - COUNTING_LOOP) // 4)
    for start, function_words in ALL_WORDS.items():
        first = (start - COUNTING_LOOP) // 4
        words[first : first + len(function_words)] = function_words
    return Dol(
        build_dol(tmp_path / "test.dol", text={0: (COUNTING_LOOP, words)}, data={}, entry=COUNTING_LOOP)
    )


@pytest.fixture
def symbols() -> SymbolTable:
    return SymbolTable(
        Symbol(name, ".text", start, "function", 4 * len(ALL_WORDS[start]))
        for name, start in [
            ("counting_loop", COUNTING_LOOP),
            ("straight_line", STRAIGHT_LINE),
            ("tail_caller", TAIL_CALLER),
            ("recursive", RECURSIVE),
        ]
    )


# A log with one divergence of each kind, as the verifier prints them.
SAMPLE_LOG = f"""\
[lockstep] ENABLED: start=0 limit=0 maxreport=0 stepcap=20000 whitelist=0
[staticrecomp] a line that is not the verifier's
[lockstep] DIVERGE #1 entry=0x801C32C0 end=0x801C44C0: fpscr:N=0xa7b88104,I=0xa7b8c104
[lockstep] DIVERGE #2 entry=0x{COUNTING_LOOP:08X} end=0x{LOOP_BODY_END:08X}: r3:N=0xa,I=0x9 ctr:N=0x1,I=0x2
[lockstep] DIVERGE #3 entry=0x{STRAIGHT_LINE:08X} end=0x{STRAIGHT_LINE + 8:08X}: r4:N=0x0,I=0x1 \
mem[0x80300000]:N=0x01,I=0x02 mmio#:N=1,I=0 N@0x0c008000/4
[lockstep] DIVERGE #4 entry=0x{TAIL_CALLER:08X} end=0x{TAIL_CALLER + 4:08X}: \
CTRLFLOW:N_end=0x{TAIL_CALLER + 4:08X},I_pc=0x80000500,steps=7,N_cyc=10,I_cyc=20
[lockstep] summary: checks=100 reports=4 skipped_fallback=1 skipped_zero=2 cap_hits=0 filtered=0 \
undercharges=0 max_deficit=0 distinct_pcs=100
"""


def divergence_from(fields_text: str, end_address: int = STRAIGHT_LINE + 8):
    """A parsed divergence with the given fields, ending at a straight-line address."""
    line = f"[lockstep] DIVERGE #1 entry=0x{STRAIGHT_LINE:08X} end=0x{end_address:08X}: {fields_text}"
    return parse_log(line).divergences[0]


# -- Reading the log -----------------------------------------------------------


def test_every_kind_of_field_is_named():
    fields = parse_fields(
        "r3:N=0x1,I=0x2 f1:N=0x0,I=0x1 ps1_2:N=0x0,I=0x1 mem[0x80300000]:N=0x01,I=0x02 "
        "lc[0xe0000010]:N=0x00,I=0x01 vmem[0x000010]:N=0x00,I=0x01 mmio#:N=1,I=0 N@0x0c008000/4 "
        "mmio[0]:N=0xcc008000=0x1/4,I=0xcc008000=0x2/4 mmio-read-seq-divergence"
    )
    assert list(fields) == [
        "r3",
        "f1",
        "ps1_2",
        "mem[0x80300000]",
        "lc[0xe0000010]",
        "vmem[0x000010]",
        "mmio#",
        "mmio[0]",
        "mmio-read-seq-divergence",
    ]
    assert fields["r3"] == "N=0x1,I=0x2"  # the comparison text, as printed


def test_the_log_is_read_into_divergences_and_a_summary():
    run = parse_log(SAMPLE_LOG)
    assert [divergence.number for divergence in run.divergences] == [1, 2, 3, 4]
    second = run.divergences[1]
    assert (second.entry_address, second.end_address) == (COUNTING_LOOP, LOOP_BODY_END)
    assert set(second.fields) == {"r3", "ctr"}
    assert set(run.divergences[2].fields) == {"r4", "mem[0x80300000]", "mmio#"}
    assert set(run.divergences[3].fields) == {"CTRLFLOW"}
    assert run.summary["checks"] == 100
    assert run.summary["skipped_zero"] == 2
    assert run.verifier_was_enabled
    assert run.log_is_complete


def test_a_log_without_the_summary_or_with_missing_lines_is_not_complete():
    lines = SAMPLE_LOG.splitlines()
    without_summary = parse_log("\n".join(line for line in lines if "summary" not in line))
    assert without_summary.summary is None
    assert not without_summary.log_is_complete

    without_one_divergence = parse_log("\n".join(line for line in lines if "DIVERGE #2" not in line))
    assert without_one_divergence.summary["reports"] == 4  # the verifier counted four...
    assert len(without_one_divergence.divergences) == 3  # ...but only three are in the log
    assert not without_one_divergence.log_is_complete


def test_a_log_with_no_verifier_lines_is_recognised():
    assert not parse_log("[staticrecomp] shutdown: native=1\n").has_verifier_output
    assert parse_log("[lockstep] ENABLED: start=0\n").has_verifier_output


# -- The FPSCR rule ------------------------------------------------------------


@pytest.mark.parametrize(
    "comparison",
    [
        "N=0xa7b88104,I=0xa7b8c104",  # FPCC: FL against FL+FG
        "N=0x00000000,I=0x00020000",  # FI, fraction inexact
        "N=0x00000000,I=0x00040000",  # FR, fraction rounded
        "N=0x00000000,I=0x0001f000",  # every FPRF bit
        "N=0x00000000,I=0x0007f000",  # the whole mask
    ],
)
def test_result_flag_differences_in_the_fpscr_are_recognised(comparison):
    assert only_result_flags_of_fpscr_differ(divergence_from(f"fpscr:{comparison}"))


@pytest.mark.parametrize(
    "fields_text",
    [
        "fpscr:N=0x00000000,I=0x00000003",  # rounding mode: changes results, not a flag
        "fpscr:N=0x00000000,I=0x80000000",  # FX, the exception summary bit
        "fpscr:N=0x00000000,I=0x00080000",  # one bit above FR
        "fpscr:N=0x00000000,I=0x00000800",  # one bit below FPRF
        "fpscr:N=0x00000000,I=0x00004000 r3:N=0x1,I=0x2",  # other fields differ too
        "r3:N=0x1,I=0x2",  # no FPSCR at all
    ],
)
def test_other_fpscr_differences_are_not_result_flags(fields_text):
    assert not only_result_flags_of_fpscr_differ(divergence_from(fields_text))


# -- The loop rule -------------------------------------------------------------


@pytest.mark.parametrize(
    ("address", "expected", "why"),
    [
        (LOOP_HEADER, True, "the back-edge jumps exactly here"),
        (LOOP_BODY_END, True, "the back-edge is after it and jumps before it"),
        (COUNTING_LOOP, False, "before the loop: nothing at or after it jumps back to it"),
        (AFTER_THE_LOOP, False, "after the back-edge: the loop is over"),
        (STRAIGHT_LINE + 4, False, "no branches"),
        (TAIL_CALLER + 4, False, "the jump goes to another function: a tail call, not a loop"),
        (RECURSIVE + 4, False, "bl is a call, even to the function's own start"),
        (0x80009000, False, "no function holds the address"),
    ],
)
def test_loop_check(dol, symbols, address, expected, why):
    assert ends_inside_loop(dol, symbols, address) is expected, why


# -- Classification ------------------------------------------------------------


def test_each_divergence_gets_the_cause_it_deserves(dol, symbols):
    run = parse_log(SAMPLE_LOG)
    causes = [cause for _, cause in classify_all(run, dol, symbols)]
    assert causes == [
        Cause.FPSCR_RESULT_FLAGS,  # #1: only FPSCR flag bits
        Cause.END_INSIDE_LOOP,  # #2: r3 and ctr differ and the block ends inside the loop
        Cause.UNEXPLAINED,  # #3: straight-line code with real differences
        Cause.CONTROL_FLOW,  # #4: the interpreter got lost
    ]


def test_a_loop_does_not_explain_an_fpscr_difference_outside_the_result_flags(dol, symbols):
    divergence = divergence_from("fpscr:N=0x0,I=0x3", end_address=LOOP_BODY_END)
    assert classify(divergence, dol, symbols) is Cause.END_INSIDE_LOOP  # last resort, as for any field
    divergence = divergence_from("fpscr:N=0x0,I=0x3")
    assert classify(divergence, dol, symbols) is Cause.UNEXPLAINED


def test_control_flow_wins_over_a_loop(dol, symbols):
    divergence = divergence_from(
        "CTRLFLOW:N_end=0x1,I_pc=0x2,steps=1,N_cyc=1,I_cyc=1", end_address=LOOP_BODY_END
    )
    assert classify(divergence, dol, symbols) is Cause.CONTROL_FLOW


# -- Report and exit status ----------------------------------------------------


def test_the_report_names_functions_and_says_what_needs_a_person(dol, symbols):
    run = parse_log(SAMPLE_LOG)
    classified = classify_all(run, dol, symbols)
    report = format_report("game.log", run, classified, symbols)
    assert "Lockstep report for game.log" in report
    assert "100  blocks checked against the interpreter" in report
    assert "fpscr_result_flags             1" in report
    assert "end_inside_loop                1" in report
    # The loop end address is listed once, with the function and offset it is in.
    assert f"1 x end=0x{LOOP_BODY_END:08X} (counting_loop+0x8)" in report
    # The two causes that need a person are printed in full.
    assert "unexplained (1):" in report
    assert f"entry=0x{STRAIGHT_LINE:08X} (straight_line)" in report
    assert "control_flow (1):" in report
    assert "Verdict: some divergences need a person to read them" in report
    assert exit_status(run, classified) == EXIT_NEEDS_A_PERSON


def test_show_prints_the_lines_of_a_known_cause_and_limit_cuts_them(dol, symbols):
    many_flags = "\n".join(
        f"[lockstep] DIVERGE #{n} entry=0x801C32C0 end=0x801C44C0: fpscr:N=0x0,I=0x4000" for n in range(1, 6)
    )
    log = f"[lockstep] ENABLED\n{many_flags}\n[lockstep] summary: checks=9 reports=5\n"
    run = parse_log(log)
    classified = classify_all(run, dol, symbols)
    hidden = format_report("g.log", run, classified, symbols)
    assert "DIVERGE" not in hidden and "#1 entry" not in hidden
    shown = format_report("g.log", run, classified, symbols, (Cause.FPSCR_RESULT_FLAGS,), lines_per_cause=2)
    assert "fpscr_result_flags (5):" in shown
    assert "#2 entry" in shown
    assert "#3 entry" not in shown
    assert "... and 3 more (raise --limit to see them)" in shown
    assert "Verdict: every divergence has a known cause." in shown


def test_a_clean_run_says_so(dol, symbols):
    run = parse_log("[lockstep] ENABLED\n[lockstep] summary: checks=50 reports=0\n")
    classified = classify_all(run, dol, symbols)
    assert "Verdict: no divergences" in format_report("g.log", run, classified, symbols)
    assert exit_status(run, classified) == EXIT_ALL_EXPLAINED


def test_a_log_that_may_be_cut_short_never_counts_as_explained(dol, symbols):
    run = parse_log(
        "[lockstep] ENABLED\n[lockstep] DIVERGE #1 entry=0x801C32C0 end=0x801C44C0: fpscr:N=0x0,I=0x4000\n"
    )
    classified = classify_all(run, dol, symbols)
    assert "WARNING: no summary line" in format_report("g.log", run, classified, symbols)
    assert exit_status(run, classified) == EXIT_NEEDS_A_PERSON


def test_a_mismatch_between_summary_and_lines_is_called_out(dol, symbols):
    log = (
        "[lockstep] ENABLED\n"
        "[lockstep] DIVERGE #1 entry=0x801C32C0 end=0x801C44C0: fpscr:N=0x0,I=0x4000\n"
        "[lockstep] summary: checks=9 reports=3\n"
    )
    run = parse_log(log)
    report = format_report("g.log", run, classify_all(run, dol, symbols), symbols)
    assert "WARNING: the verifier counted 3 divergences but the log holds 1 lines" in report


# -- Command line --------------------------------------------------------------


@pytest.fixture
def command_line(tmp_path, dol, symbols):
    """The arguments that point the tool at the synthetic DOL and symbols."""
    symbols_path = tmp_path / "symbols.txt"
    symbols.write(symbols_path)
    return ["--dol", str(dol.path), "--symbols", str(symbols_path)]


def run_tool(tmp_path, command_line, log_text: str) -> int:
    log_path = tmp_path / "game.log"
    log_path.write_text(log_text)
    return main([str(log_path), *command_line])


def test_exit_status_is_zero_when_every_divergence_is_explained(tmp_path, command_line, capsys):
    log = (
        "[lockstep] ENABLED\n"
        "[lockstep] DIVERGE #1 entry=0x801C32C0 end=0x801C44C0: fpscr:N=0xa7b88104,I=0xa7b8c104\n"
        f"[lockstep] DIVERGE #2 entry=0x{COUNTING_LOOP:08X} end=0x{LOOP_BODY_END:08X}: r3:N=0xa,I=0x9\n"
        "[lockstep] summary: checks=9 reports=2\n"
    )
    assert run_tool(tmp_path, command_line, log) == EXIT_ALL_EXPLAINED
    assert "Verdict: every divergence has a known cause." in capsys.readouterr().out


def test_exit_status_is_one_when_a_person_has_to_look(tmp_path, command_line):
    assert run_tool(tmp_path, command_line, SAMPLE_LOG) == EXIT_NEEDS_A_PERSON


def test_a_log_from_a_normal_run_is_refused(tmp_path, command_line, capsys):
    assert run_tool(tmp_path, command_line, "[staticrecomp] shutdown: native=1\n") == EXIT_NOT_A_VERIFIER_LOG
    assert "STATICRECOMP_LOCKSTEP=1" in capsys.readouterr().err


def test_missing_files_are_reported_by_name(tmp_path, command_line):
    with pytest.raises(SystemExit):
        main([str(tmp_path / "missing.log"), *command_line])
