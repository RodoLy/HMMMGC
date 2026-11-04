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
from isa_conformance import DONE_VALUE, Case, Evaluation, FormResult  # noqa: E402
from run_isa_conformance import (  # noqa: E402
    ModernGekkoTools,
    SeedOutcome,
    SetupError,
    format_summary,
    main,
    make_game_root,
    recompile,
    report_self_check,
    run_under_the_verifier,
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
    if "FAIL_THE_BUILD" in Path(game_root, "sys", "main.dol").read_text():
        print("compiler: error: something broke")
        sys.exit(1)
    module = output / "G4AEE9" / "0123abcd"
    (module / "module-build").mkdir(parents=True)
    (module / "gG4AEE9_recomp.so").write_text("the module")
    (module / "module-build" / "gG4AEE9_recomp.so").write_text("an intermediate copy")
"""

# What the real runner does that the orchestrator relies on: it writes `state=running` into
# status.txt, answers `read_memory` with the guest's memory, and on `stop` writes the files the
# verifier writes on exit and quits.
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
    marker = bytes.fromhex("%(marker)s")
    while True:
        for path in sorted((automation / "commands").glob("*.txt")):
            fields = dict(line.split("=", 1) for line in path.read_text().splitlines())
            (automation / "processed").mkdir(exist_ok=True)
            if fields["command"] == "read_memory":
                Path(fields["path"]).write_bytes(marker)
            path.rename(automation / "processed" / path.name)
            if fields["command"] == "stop":
                Path(os.environ["STATICRECOMP_LOCKSTEP_COVERAGE_FILE"]).write_text("0x80000000\\n")
                print("[lockstep] summary: checks=1 reports=0", flush=True)
                sys.exit(0)
        time.sleep(0.005)
"""

FINISHED = DONE_VALUE.to_bytes(4, "big").hex()
NOT_FINISHED = "00000000"


@pytest.fixture(autouse=True)
def quick_polling_and_a_screen(monkeypatch):
    monkeypatch.setattr(run_isa_conformance, "POLL_SECONDS", 0.01)
    monkeypatch.setenv("DISPLAY", ":fake")  # so that no virtual display is started


@pytest.fixture
def disc(tmp_path) -> Path:
    sys_directory = tmp_path / "disc" / "sys"
    sys_directory.mkdir(parents=True)
    for name in ("apploader.img", "bi2.bin", "boot.bin", "main.dol"):
        (sys_directory / name).write_text(f"the disc's {name}")
    return tmp_path / "disc"


@pytest.fixture
def tools(tmp_path) -> ModernGekkoTools:
    build_directory = tmp_path / "template" / "lib" / "ModernGekko" / "build"
    build_directory.mkdir(parents=True)
    port = write_script(build_directory / "moderngekko-port", FAKE_PORT)
    runner = write_script(build_directory / "moderngekko-run", FAKE_RUNNER % {"marker": FINISHED})
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
    make_game_root(disc, program, game_root)
    assert (game_root / "sys" / "boot.bin").read_text() == "the disc's boot.bin"
    assert (game_root / "sys" / "main.dol").read_text() == "the test program"  # not the game's
    assert (game_root / "files").is_dir()


def test_a_disc_without_its_boot_files_says_how_to_get_them(tmp_path):
    (tmp_path / "empty-disc" / "sys").mkdir(parents=True)
    with pytest.raises(SetupError, match=r"apploader.img not found.*extract_disc"):
        make_game_root(tmp_path / "empty-disc", tmp_path / "program.dol", tmp_path / "game")


# -- Recompiling -------------------------------------------------------------------------------


def test_the_recompiled_module_is_the_library_not_a_file_inside_the_build_folder(tools, disc, tmp_path):
    program = tmp_path / "program.dol"
    program.write_text("the test program")
    make_game_root(disc, program, tmp_path / "game")
    module = recompile(tools, tmp_path / "game", tmp_path / "module", "gcc")
    assert module == tmp_path / "module" / "G4AEE9" / "0123abcd" / "gG4AEE9_recomp.so"


def test_a_failed_build_shows_the_end_of_its_log(tools, disc, tmp_path):
    program = tmp_path / "program.dol"
    program.write_text("FAIL_THE_BUILD")
    make_game_root(disc, program, tmp_path / "game")
    with pytest.raises(SetupError, match=r"compiler: error: something broke"):
        recompile(tools, tmp_path / "game", tmp_path / "module", "gcc")


# -- Running -----------------------------------------------------------------------------------


def run(tools, tmp_path, extra_environment=None, timeout_seconds=10.0):
    return run_under_the_verifier(
        tools,
        tmp_path / "game",
        tmp_path / "module.so",
        tmp_path / "run",
        timeout_seconds,
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


def test_a_program_that_never_writes_its_marker_is_reported_with_the_log_to_read(tmp_path):
    never_finishes = write_script(tmp_path / "moderngekko-run", FAKE_RUNNER % {"marker": NOT_FINISHED})
    tools = ModernGekkoTools(tmp_path / "moderngekko-port", never_finishes)
    with pytest.raises(SetupError, match=r"did not finish; see .*game.log"):
        run(tools, tmp_path, timeout_seconds=2.0)


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


def test_a_missing_template_is_a_clear_error_and_exit_code_two(tmp_path, capsys):
    assert main(["--template", str(tmp_path / "missing")]) == 2
    assert "moderngekko-port not found" in capsys.readouterr().err
