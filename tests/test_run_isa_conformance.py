"""The one-command runner of the instruction-level check. No game and no ModernGekko needed.

El ejecutor de un solo comando de la prueba por instrucción. Sin el juego ni ModernGekko.

`moderngekko-port` and `moderngekko-run` are replaced by small scripts that make the files
and answer the commands the real ones do.
"""

import stat
import sys
import textwrap
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import run_isa_conformance  # noqa: E402
import synthetic_disc  # noqa: E402
from dol import write_dol  # noqa: E402
from isa_conformance import (  # noqa: E402
    DONE_VALUE,
    SCRATCH_ADDRESS,
    STATE_SIZE,
    STATE_TABLE_ADDRESS,
    Candidate,
    Case,
    Evaluation,
    FormResult,
    Program,
)
from ppc_encode import BLR  # noqa: E402
from run_isa_conformance import (  # noqa: E402
    ModernGekkoTools,
    ProgramNotFinishedError,
    SeedOutcome,
    SetupError,
    format_summary,
    main,
    make_game_root,
    recompile,
    report_self_check,
    run_under_the_verifier,
    where_the_program_stopped,
)


def write_script(path: Path, body: str) -> Path:
    """An executable Python script, to stand in for one of ModernGekko's programs."""
    path.write_text(f"#!{sys.executable}\n{textwrap.dedent(body)}")
    path.chmod(path.stat().st_mode | stat.S_IEXEC)
    return path


FAKE_PORT = """
    import sys
    from pathlib import Path

    command, game_root, *options = sys.argv[1:]
    output = Path(options[options.index("--output") + 1])
    if b"FAIL_THE_BUILD" in Path(game_root, "sys", "main.dol").read_bytes():
        print("compiler: error: something broke")
        sys.exit(1)
    game_id = Path(game_root, "sys", "boot.bin").read_bytes()[:6].decode()
    module = output / game_id / "0123abcd"
    (module / "module-build").mkdir(parents=True)
    (module / f"g{game_id}_recomp.so").write_text("the module")
    (module / "module-build" / f"g{game_id}_recomp.so").write_text("an intermediate copy")
"""

# What the real runner does that the orchestrator relies on: it writes `state=running` into
# status.txt, answers `read_memory` with the guest's memory (here, the two control words: the
# marker and the number of the group the program is running, which advances at every read when
# FAKE_GROUP_ADVANCES is set), and on `stop` writes the files the verifier writes on exit and quits.
FAKE_RUNNER = """
    import os
    import sys
    import time
    from pathlib import Path

    arguments = sys.argv[1:]
    automation = Path(arguments[arguments.index("--automation-dir") + 1])
    print("arguments:", " ".join(arguments), flush=True)
    for name in ("LOCKSTEP", "LOCKSTEP_ANY_ARRIVAL", "LOCKSTEP_INJECT_FAULT"):
        print(f"STATICRECOMP_{name}={os.environ.get('STATICRECOMP_' + name)}", flush=True)
    (automation / "status.txt").write_text("state=running\\nframe_count=1\\n")
    control_words = bytes.fromhex("%(control_words)s")
    reads = 0
    while True:
        for path in sorted((automation / "commands").glob("*.txt")):
            fields = dict(line.split("=", 1) for line in path.read_text().splitlines())
            (automation / "processed").mkdir(exist_ok=True)
            if fields["command"] == "read_memory":
                reads += 1
                answer = control_words
                if os.environ.get("FAKE_GROUP_ADVANCES"):
                    answer = control_words[:4] + reads.to_bytes(4, "big")
                Path(fields["path"]).write_bytes(answer)
            path.rename(automation / "processed" / path.name)
            if fields["command"] == "stop":
                Path(os.environ["STATICRECOMP_LOCKSTEP_COVERAGE_FILE"]).write_text("0x80000000\\n")
                print("[lockstep] summary: checks=1 reports=0", flush=True)
                sys.exit(0)
        time.sleep(0.005)
"""

# The marker and then the number of the group the program is running.
FINISHED = DONE_VALUE.to_bytes(4, "big").hex() + "00000009"
NOT_FINISHED_IN_GROUP_7 = "00000000" + "00000007"

# Two made-up forms with one word each, to keep the tests of a synthetic run quick.
ADD_R3_R4_R5 = 0x7C642A14
LWZ_R3_8_R4 = 0x80640008  # lwz r3, 8(r4)
MADE_UP_WORDS = {"add": [Candidate(ADD_R3_R4_R5, 0)], "lwz": [Candidate(LWZ_R3_8_R4, 0)]}


@pytest.fixture(autouse=True)
def quick_polling_and_a_screen(monkeypatch):
    monkeypatch.setattr(run_isa_conformance, "POLL_SECONDS", 0.01)
    monkeypatch.setenv("DISPLAY", ":fake")  # so that no virtual display is started


@pytest.fixture
def disc(tmp_path) -> Path:
    sys_directory = tmp_path / "disc" / "sys"
    sys_directory.mkdir(parents=True)
    for name in ("apploader.img", "bi2.bin", "main.dol"):
        (sys_directory / name).write_text(f"the disc's {name}")
    # A real disc header starts with the game's ID, and the fake `moderngekko-port` reads it.
    (sys_directory / "boot.bin").write_bytes(b"G4AEE9 the disc's boot.bin")
    return tmp_path / "disc"


@pytest.fixture
def tools(tmp_path) -> ModernGekkoTools:
    build_directory = tmp_path / "template" / "lib" / "ModernGekko" / "build"
    build_directory.mkdir(parents=True)
    port = write_script(build_directory / "moderngekko-port", FAKE_PORT)
    runner = write_script(build_directory / "moderngekko-run", FAKE_RUNNER % {"control_words": FINISHED})
    return ModernGekkoTools(port, runner)


# -- Finding the tools -------------------------------------------------------------------------


def test_the_tools_are_found_in_the_template_build_folder(tools, tmp_path):
    assert ModernGekkoTools.in_template(tmp_path / "template") == tools


def test_a_missing_tool_is_named_and_the_guide_to_build_it_is_pointed_at(tmp_path):
    with pytest.raises(SetupError, match=r"moderngekko-port not found.*docs/BUILDING.md"):
        ModernGekkoTools.in_template(tmp_path / "no-template")


# -- The folder the runner takes for a game ----------------------------------------------------


def test_the_game_folder_holds_the_discs_boot_files_and_the_test_program_as_main_dol(disc, tmp_path):
    program = tmp_path / "program.dol"
    program.write_text("the test program")
    game_root = tmp_path / "game"
    make_game_root(program, game_root, disc)
    assert (game_root / "sys" / "boot.bin").read_bytes() == (disc / "sys" / "boot.bin").read_bytes()
    assert (game_root / "sys" / "main.dol").read_text() == "the test program"  # not the game's
    assert (game_root / "files").is_dir()


def test_a_disc_without_its_boot_files_says_how_to_get_them(tmp_path):
    (tmp_path / "empty-disc" / "sys").mkdir(parents=True)
    with pytest.raises(SetupError, match=r"apploader.img not found.*extract_disc"):
        make_game_root(tmp_path / "program.dol", tmp_path / "game", tmp_path / "empty-disc")


def test_without_a_disc_the_game_folder_holds_made_up_boot_files_and_the_test_program(tmp_path):
    program = write_dol(tmp_path / "program.dol", text={0: (0x80003100, [BLR])}, data={}, entry=0x80003100)
    game_root = tmp_path / "game"
    make_game_root(program, game_root)
    sys_directory = game_root / "sys"
    assert sorted(path.name for path in sys_directory.iterdir()) == [
        "apploader.img",
        "bi2.bin",
        "boot.bin",
        "main.dol",
    ]
    assert (sys_directory / "boot.bin").read_bytes().startswith(synthetic_disc.GAME_ID.encode())
    assert (sys_directory / "main.dol").read_bytes() == program.read_bytes()
    assert (game_root / "files").is_dir()


# -- Recompiling -------------------------------------------------------------------------------


def test_the_recompiled_module_is_the_library_not_a_file_inside_the_build_folder(tools, disc, tmp_path):
    program = tmp_path / "program.dol"
    program.write_text("the test program")
    make_game_root(program, tmp_path / "game", disc)
    module = recompile(tools, tmp_path / "game", tmp_path / "module", "gcc")
    assert module == tmp_path / "module" / "G4AEE9" / "0123abcd" / "gG4AEE9_recomp.so"


def test_the_module_of_the_made_up_game_is_found_under_its_own_id(tools, tmp_path):
    program = write_dol(tmp_path / "program.dol", text={0: (0x80003100, [BLR])}, data={}, entry=0x80003100)
    make_game_root(program, tmp_path / "game")
    game_id = synthetic_disc.GAME_ID
    module = recompile(tools, tmp_path / "game", tmp_path / "module", "gcc", game_id)
    assert module == tmp_path / "module" / game_id / "0123abcd" / f"g{game_id}_recomp.so"


def test_a_failed_build_shows_the_end_of_its_log(tools, disc, tmp_path):
    program = tmp_path / "program.dol"
    program.write_text("FAIL_THE_BUILD")
    make_game_root(program, tmp_path / "game", disc)
    with pytest.raises(SetupError, match=r"compiler: error: something broke"):
        recompile(tools, tmp_path / "game", tmp_path / "module", "gcc")


# -- Running -----------------------------------------------------------------------------------


def run(tools, tmp_path, extra_environment=None, timeout_seconds=10.0, stall_seconds=60.0):
    return run_under_the_verifier(
        tools,
        tmp_path / "game",
        tmp_path / "module.so",
        tmp_path / "run",
        timeout_seconds,
        stall_seconds,
        extra_environment or {},
    )


def test_the_program_is_run_to_its_marker_and_the_game_is_closed_normally(tools, tmp_path):
    files = run(tools, tmp_path)
    assert files.coverage.read_text() == "0x80000000\n"  # written by the runner on `stop`
    log = files.log.read_text()
    assert "[lockstep] summary: checks=1 reports=0" in log
    assert "--graphics Null" in log
    assert f"--module {tmp_path / 'module.so'}" in log


def test_the_verifier_options_reach_the_runner_through_the_environment(tools, tmp_path):
    log = run(tools, tmp_path).log.read_text()
    assert "STATICRECOMP_LOCKSTEP=1" in log
    assert "STATICRECOMP_LOCKSTEP_ANY_ARRIVAL=1" in log
    assert "STATICRECOMP_LOCKSTEP_INJECT_FAULT=None" in log  # not set unless asked for


def test_an_injected_fault_reaches_the_runner(tools, tmp_path):
    log = run(tools, tmp_path, {"STATICRECOMP_LOCKSTEP_INJECT_FAULT": "0x80010000"}).log.read_text()
    assert "STATICRECOMP_LOCKSTEP_INJECT_FAULT=0x80010000" in log


def never_finishing_tools(tmp_path) -> ModernGekkoTools:
    never_finishes = write_script(
        tmp_path / "moderngekko-run", FAKE_RUNNER % {"control_words": NOT_FINISHED_IN_GROUP_7}
    )
    return ModernGekkoTools(tmp_path / "moderngekko-port", never_finishes)


def test_a_program_that_moves_on_but_never_writes_its_marker_runs_out_of_time(tmp_path):
    moving_on = {"FAKE_GROUP_ADVANCES": "1"}
    with pytest.raises(ProgramNotFinishedError, match=r"\(it ran out of time\); see .*game.log"):
        run(never_finishing_tools(tmp_path), tmp_path, moving_on, timeout_seconds=1.5, stall_seconds=0.3)


def test_a_program_that_stays_in_one_group_is_given_up_on_long_before_the_time_runs_out(tmp_path):
    with pytest.raises(
        ProgramNotFinishedError, match=r"stayed in one group of cases for 0.3 seconds"
    ) as hang:
        run(never_finishing_tools(tmp_path), tmp_path, timeout_seconds=60.0, stall_seconds=0.3)
    assert hang.value.group_number == 7
    assert hang.value.log == tmp_path / "run" / "game.log"


def test_a_hung_program_is_still_a_setup_error_with_the_log_to_read(tmp_path):
    with pytest.raises(SetupError, match=r"did not finish .*; see .*game.log"):
        run(never_finishing_tools(tmp_path), tmp_path, timeout_seconds=60.0, stall_seconds=0.3)


def test_a_runner_that_exits_at_once_is_reported(tmp_path):
    crashes = write_script(tmp_path / "moderngekko-run", "import sys\nsys.exit(3)\n")
    tools = ModernGekkoTools(tmp_path / "moderngekko-port", crashes)
    with pytest.raises(SetupError, match=r"exited with code 3"):
        run(tools, tmp_path, timeout_seconds=5)


def test_without_a_screen_or_xvfb_the_message_says_what_to_install(tools, tmp_path, monkeypatch):
    monkeypatch.delenv("DISPLAY")
    monkeypatch.delenv("WAYLAND_DISPLAY", raising=False)
    monkeypatch.setenv("PATH", str(tmp_path))  # no Xvfb here
    with pytest.raises(SetupError, match="apt install xvfb"):
        run(tools, tmp_path)


# -- A program that hangs ----------------------------------------------------------------------


def program_with_groups(group_first_cases: list[int]) -> Program:
    """Five cases that sit 8 bytes apart, split into groups that start at `group_first_cases`."""
    cases = [
        Case(0x80010000 + 8 * number, 0x80010000 + 8 * number, "add", ADD_R3_R4_R5, 0) for number in range(5)
    ]
    return Program([], {}, cases, {}, group_first_cases)


def test_a_hung_program_is_traced_to_the_cases_of_its_last_group():
    message = where_the_program_stopped(program_with_groups([0, 2, 4]), 1)
    assert "It stopped in group 1 of 3" in message
    assert message.count("add r3, r4, r5") == 2  # the cases #2 and #3, which sit at 0x80010010 and 0x80010018
    assert "0x80010010" in message
    assert "0x80010018" in message
    assert "0x80010008" not in message  # the group before
    assert "0x80010020" not in message  # the group after


def test_the_last_group_ends_with_the_last_case():
    message = where_the_program_stopped(program_with_groups([0, 2, 4]), 2)
    assert message.count("add r3, r4, r5") == 1
    assert "0x80010020" in message


def test_a_group_number_that_names_no_group_is_said_so_and_no_case_is_blamed():
    message = where_the_program_stopped(program_with_groups([0, 2, 4]), 0xDEADBEEF)
    assert "not one of its groups" in message
    assert "add r3" not in message


def test_a_synthetic_run_whose_program_hangs_ends_with_the_cases_to_look_at(tmp_path, monkeypatch, capsys):
    build_directory = tmp_path / "template" / "lib" / "ModernGekko" / "build"
    build_directory.mkdir(parents=True)
    write_script(build_directory / "moderngekko-port", FAKE_PORT)
    write_script(build_directory / "moderngekko-run", FAKE_RUNNER % {"control_words": "00000000" * 2})
    monkeypatch.setattr(run_isa_conformance, "synthetic_candidates_for_seed", lambda seed: MADE_UP_WORDS)
    arguments = synthetic_arguments(tmp_path, "--timeout", "30", "--stall-timeout", "0.3")
    assert main(arguments) == 2
    error = capsys.readouterr().err
    assert "seed 1: the test program did not finish (it stayed in one group of cases" in error
    assert "It stopped in group 0 of 1" in error
    assert "add r3, r4, r5" in error
    assert "lwz r3, 8(r4)" in error


# -- Summary -----------------------------------------------------------------------------------


def case(address: int, form: str) -> Case:
    return Case(address, address, form, 0, 0)


def outcome(seed: int, results: dict[str, FormResult], divergences=(), entries=frozenset()) -> SeedOutcome:
    evaluation = Evaluation(results, list(divergences), None, set(entries))
    return SeedOutcome(seed, [case(0x80010000, "lwz")], evaluation)


def test_the_summary_adds_up_the_seeds():
    first = outcome(1, {"add": FormResult(cases=4, compared=4), "dcbf": FormResult(cases=1, not_compared=1)})
    second = outcome(2, {"add": FormResult(cases=3, compared=2, diverged=1)})
    summary = format_summary([first, second])
    assert "2 seed(s): 8 cases of 2 forms" in summary
    assert "compared and equal:  6" in summary
    assert "diverged:            1" in summary
    assert "not compared:        1" in summary


def test_the_summary_names_the_forms_that_no_seed_compared_and_found_equal():
    summary = format_summary(
        [outcome(1, {"add": FormResult(cases=2, compared=2), "dcbf": FormResult(cases=1)})]
    )
    assert "no case compared and equal in any seed: dcbf" in summary


def test_the_summary_lists_the_divergences_with_their_seed():
    summary = format_summary(
        [
            outcome(
                7,
                {"add": FormResult(cases=1, diverged=1)},
                ["entry=0x80010000  add: add r3, r4, r5  [r3:N=1,I=2]"],
            )
        ]
    )
    assert "seed 7: entry=0x80010000  add: add r3, r4, r5  [r3:N=1,I=2]" in summary


def test_the_self_check_passes_when_exactly_the_injected_case_is_reported(capsys):
    injected = outcome(1, {"lwz": FormResult(cases=1, diverged=1)}, entries={0x80010000})
    assert report_self_check("lwz", [injected]) == 0
    assert "self-check passed" in capsys.readouterr().out


@pytest.mark.parametrize(
    "reported",
    [set(), {0x80010000, 0x80010008}, {0x80010008}],
    ids=["nothing reported", "an extra divergence", "the wrong case"],
)
def test_the_self_check_fails_otherwise(reported, capsys):
    assert report_self_check("lwz", [outcome(1, {}, entries=reported)]) == 1
    assert "self-check FAILED" in capsys.readouterr().out


# -- Command line ------------------------------------------------------------------------------


def synthetic_arguments(tmp_path, *more: str) -> list[str]:
    """The command line of a synthetic run that uses the fake tools, which the `tools` fixture made."""
    return [
        "--synthetic",
        *("--template", str(tmp_path / "template")),
        *("--work-directory", str(tmp_path / "work")),
        *("--toolchain", "gcc"),
        *more,
    ]


def test_a_missing_template_is_a_clear_error_and_exit_code_two(tmp_path, capsys):
    assert main(["--template", str(tmp_path / "missing")]) == 2
    assert "moderngekko-port not found" in capsys.readouterr().err


def test_a_synthetic_run_needs_neither_the_disc_nor_the_symbols(tools, tmp_path, monkeypatch, capsys):
    monkeypatch.setattr(run_isa_conformance, "synthetic_candidates_for_seed", lambda seed: MADE_UP_WORDS)
    no_such_disc, no_such_symbols = tmp_path / "no-disc", tmp_path / "no-symbols.txt"
    arguments = synthetic_arguments(tmp_path, "--disc", str(no_such_disc), "--symbols", str(no_such_symbols))
    assert main(arguments) == 0
    assert "seed 1: 2 cases of 2 forms" in capsys.readouterr().out


def test_a_synthetic_run_makes_up_its_words_again_for_each_seed(tools, tmp_path, monkeypatch, capsys):
    seeds_asked_for = []

    def words_of(seed):
        seeds_asked_for.append(seed)
        return MADE_UP_WORDS

    monkeypatch.setattr(run_isa_conformance, "synthetic_candidates_for_seed", words_of)
    assert main(synthetic_arguments(tmp_path, "--seeds", "4", "9")) == 0
    assert seeds_asked_for == [4, 9]


def test_a_program_whose_sections_would_overlap_is_a_setup_error_that_says_what_to_change(
    tools, tmp_path, monkeypatch, capsys
):
    group_count = (SCRATCH_ADDRESS - STATE_TABLE_ADDRESS) // STATE_SIZE + 1  # one row too many
    words = {"add": [Candidate(ADD_R3_R4_R5, 0)] * group_count}
    monkeypatch.setattr(run_isa_conformance, "synthetic_candidates_for_seed", lambda seed: words)
    arguments = synthetic_arguments(tmp_path, "--words-per-form", str(group_count), "--group-size", "1")
    assert main(arguments) == 2
    error = capsys.readouterr().err
    assert "seed 1: the state table" in error
    assert "the scratch memory" in error
    assert "--group-size" in error


def test_a_fault_in_a_form_that_the_program_does_not_have_is_a_setup_error(
    tools, tmp_path, monkeypatch, capsys
):
    monkeypatch.setattr(run_isa_conformance, "synthetic_candidates_for_seed", lambda seed: MADE_UP_WORDS)
    assert main(synthetic_arguments(tmp_path, "--inject-fault", "fmadds")) == 2
    assert "the test program has no case of form 'fmadds'" in capsys.readouterr().err
