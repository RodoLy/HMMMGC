"""Tests of the workflow files in .github/workflows. They need no GitHub.

Pruebas de los archivos de workflows de .github/workflows. No necesitan GitHub.

The workflows run on GitHub, where a mistake costs a long wait and a run of the build machines, so
what can be checked from outside is checked here: that the files are well formed, that the
PowerShell they run is valid, and that the workflow of the Windows package agrees with the
assembler and with the files of the package. The PowerShell check needs `pwsh` and is skipped
where it is not installed; the build servers of GitHub have it.
"""

import re
import shutil
import subprocess
import sys
from collections.abc import Iterator
from pathlib import Path

import pytest
import yaml

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import package_windows  # noqa: E402

REPOSITORY = Path(__file__).resolve().parent.parent
WORKFLOWS_DIRECTORY = REPOSITORY / ".github" / "workflows"
PACKAGE_WORKFLOW = WORKFLOWS_DIRECTORY / "package-windows-arm64.yml"
PACKAGING_DIRECTORY = REPOSITORY / "packaging" / "windows"
PATCHES_DIRECTORY = REPOSITORY / "patches" / "moderngekko"
POWERSHELL = shutil.which("pwsh")

# Windows PowerShell 5.1 is the one every Windows has. These came with PowerShell 7.
SYNTAX_OF_NEWER_POWERSHELL = {
    "the ?? operator": r"\?\?",
    "the ?. operator": r"\?\.",
    "the && operator": r"&&",
    "the || operator": r"\|\|",
}

PARSER_SCRIPT = """\
param([string] $Path)
$errors = $null
$null = [System.Management.Automation.Language.Parser]::ParseFile($Path, [ref] $null, [ref] $errors)
foreach ($problem in $errors) { '{0}: {1}' -f $problem.Extent.StartLineNumber, $problem.Message }
exit [int] ($errors.Count -gt 0)
"""


def load_workflow(path: Path) -> dict:
    return yaml.safe_load(path.read_text(encoding="utf-8"))


def workflow_steps(path: Path) -> Iterator[tuple[str, dict, str]]:
    """Every step of every job: the job's id, the step, and the shell its `run:` uses."""
    for job_id, job in load_workflow(path)["jobs"].items():
        runs_on_windows = "windows" in str(job["runs-on"])
        job_shell = job.get("defaults", {}).get("run", {}).get("shell")
        for step in job["steps"]:
            yield job_id, step, step.get("shell", job_shell or ("pwsh" if runs_on_windows else "bash"))


def powershell_steps(shells: set[str]) -> list[tuple[str, str]]:
    """(an identifier, the script) of every step of every workflow that runs one of these shells."""
    found = []
    for path in sorted(WORKFLOWS_DIRECTORY.glob("*.yml")):
        for job_id, step, shell in workflow_steps(path):
            if "run" in step and shell in shells:
                found.append((f"{path.name}:{job_id}:{step.get('name', 'unnamed step')}", step["run"]))
    return found


def without_expressions(script: str) -> str:
    """GitHub replaces ${{ ... }} before the shell sees the script; the parser must not see it."""
    return re.sub(r"\$\{\{.*?\}\}", "EXPRESSION", script)


WORKFLOW_FILES = sorted(WORKFLOWS_DIRECTORY.glob("*.yml"))


# -- Every workflow ---------------------------------------------------------------------------------


@pytest.mark.parametrize("path", WORKFLOW_FILES, ids=lambda path: path.name)
def test_a_workflow_has_a_name_triggers_jobs_and_limited_permissions(path):
    workflow = load_workflow(path)
    assert workflow["name"]
    assert workflow.get("on") or workflow.get(True)  # YAML 1.1 reads the key `on` as the boolean true
    assert workflow["jobs"]
    assert workflow["permissions"] == {"contents": "read"}


@pytest.mark.parametrize("path", WORKFLOW_FILES, ids=lambda path: path.name)
def test_every_action_is_used_at_a_version_and_not_at_a_branch(path):
    for _, step, _ in workflow_steps(path):
        if "uses" in step:
            _, _, version = step["uses"].partition("@")
            assert re.fullmatch(r"v\d+(\.\d+)*|[0-9a-f]{40}", version), step["uses"]


@pytest.mark.skipif(POWERSHELL is None, reason="PowerShell (pwsh) is not installed")
@pytest.mark.parametrize(
    ("identifier", "script"), powershell_steps({"pwsh", "powershell"}), ids=lambda x: x[:60]
)
def test_the_powershell_of_every_step_has_no_syntax_errors(identifier, script, tmp_path):
    parser = tmp_path / "parse.ps1"
    parser.write_text(PARSER_SCRIPT, encoding="utf-8")
    step_script = tmp_path / "step.ps1"
    step_script.write_text(without_expressions(script), encoding="utf-8")
    result = subprocess.run(
        [POWERSHELL, "-NoProfile", "-NonInteractive", "-File", str(parser), str(step_script)],
        capture_output=True,
        text=True,
        timeout=120,
        check=False,
    )
    assert result.returncode == 0, f"{identifier}:\n{result.stdout}{result.stderr}"


@pytest.mark.parametrize(("identifier", "script"), powershell_steps({"powershell"}), ids=lambda x: x[:60])
def test_a_step_for_windows_powershell_uses_no_syntax_that_only_newer_versions_have(identifier, script):
    for description, pattern in SYNTAX_OF_NEWER_POWERSHELL.items():
        assert not re.search(pattern, script), f"{identifier} uses {description}"


# -- The workflow of the Windows package ------------------------------------------------------------


@pytest.fixture(scope="module")
def package_workflow_text() -> str:
    return PACKAGE_WORKFLOW.read_text(encoding="utf-8")


@pytest.fixture(scope="module")
def game_id() -> str:
    return re.search(r"GameId\s*=\s*'(\w+)'", (PACKAGING_DIRECTORY / "game.psd1").read_text("utf-8")).group(1)


def test_the_workflow_only_runs_when_it_is_asked_for():
    assert set(load_workflow(PACKAGE_WORKFLOW)[True]) == {"workflow_dispatch"}


def test_the_programs_the_workflow_keeps_and_tests_are_the_programs_the_assembler_takes(
    package_workflow_text,
):
    all_programs = {package_windows.DEFAULT_LAUNCHER_NAME, *package_windows.PROGRAMS}
    lists = [
        set(re.findall(r"'([\w-]+)'", names))
        for names in re.findall(r"foreach \(\$name in ((?:'[\w-]+',? ?)+)\)", package_workflow_text)
    ]
    assert all_programs in lists  # the programs are kept, and checked for the processor, by name
    assert all(
        names in (all_programs, set(package_windows.PROGRAMS)) for names in lists
    )  # the launcher opens a window


def test_the_workflow_uses_the_folder_and_zip_names_of_the_assembler(package_workflow_text):
    names = set(re.findall(r"HarvestMoonMM-windows-\w+", package_workflow_text))
    assert names == {package_windows.PACKAGE_FOLDER_NAME}
    assert f"{package_windows.PACKAGE_FOLDER_NAME}.zip" in package_workflow_text


def test_the_workflow_tests_the_game_that_the_package_plays(package_workflow_text, game_id):
    assert re.findall(r"--synthetic-game-id (\w+)", package_workflow_text) == [game_id]
    assert re.findall(r"games\\(\w{6})\\sys", package_workflow_text) == [game_id]
    assert re.findall(r"\bg(\w{6})_recomp\.dll", package_workflow_text) == [game_id]
    assert re.findall(r"C:\\disc\\(\w{6})\.gcm", package_workflow_text) == [game_id] * 2


def test_the_workflow_builds_with_the_branding_of_the_package(package_workflow_text):
    assert '-C "$GITHUB_WORKSPACE/packaging/windows/branding.cmake"' in package_workflow_text
    assert (PACKAGING_DIRECTORY / "branding.cmake").is_file()


def test_the_patches_the_workflow_applies_exist_and_include_the_one_the_package_needs(package_workflow_text):
    applied = set(re.findall(r"patches/moderngekko/([\w.-]+\.patch)", package_workflow_text))
    existing = {path.name for path in PATCHES_DIRECTORY.glob("*.patch")}
    assert applied <= existing
    assert "relocatable-port.patch" in applied  # without it moderngekko-port cannot be moved


def test_the_cache_of_the_programs_is_keyed_by_everything_that_changes_what_is_built(package_workflow_text):
    key = re.search(r"key: windows-arm64-programs-.*hashFiles\((.*?)\)", package_workflow_text).group(1)
    for what in (
        "scripts/build_and_run_linux.sh",
        "patches/moderngekko/*.patch",
        "packaging/windows/branding.cmake",
    ):
        assert what in key
    assert "BUILD_CACHE_VERSION" in package_workflow_text


def test_the_cache_is_saved_with_the_key_it_was_looked_up_with(package_workflow_text):
    assert "key: ${{ steps.cache.outputs.cache-primary-key }}" in package_workflow_text


def play_test_step() -> dict:
    return next(step for _, step, _ in workflow_steps(PACKAGE_WORKFLOW) if step.get("id") == "play")


def test_every_run_of_the_play_script_has_a_time_limit_that_ends_before_the_step_does():
    # play.ps1 opens a window to ask for a disc image when it has none, and nobody can answer it
    # on a build machine: without a limit a run waits until GitHub stops the whole step.
    step = play_test_step()
    script = step["run"]
    assert "WaitForExit(" in script
    assert "taskkill.exe /F /T" in script  # stops the script and the programs it started
    minutes_per_run = [int(minutes) for minutes in re.findall(r"\$minutesToWait = (\d+)", script)]
    assert len(minutes_per_run) == 2  # one for each run
    assert sum(minutes_per_run) < step["timeout-minutes"]


def test_the_second_run_of_the_play_script_only_happens_if_the_first_left_the_extracted_game():
    # With no disc given and no extracted game, the second run would ask for a disc in a window.
    script = play_test_step()["run"]
    guard = re.search(
        r"-not \(Test-Path -LiteralPath \(Join-Path \$userFolder \$extractedProgram\)\)\) \{(.*?)\}",
        script,
        re.DOTALL,
    )
    assert guard
    assert "break" in guard.group(1)
    assert script.index("run 2 skipped") < script.index("Invoke-PlayScript -ArgumentLine")


def test_a_failed_test_does_not_stop_the_other_tests_but_does_fail_the_run():
    steps = {step.get("id"): step for _, step, _ in workflow_steps(PACKAGE_WORKFLOW) if step.get("id")}
    tests = [steps["programs"], steps["conformance"], steps["play"]]
    assert all(step.get("continue-on-error") is True for step in tests)
    assert all(step.get("timeout-minutes") for step in tests)  # a hung test must not use up the whole run
    last_step = list(workflow_steps(PACKAGE_WORKFLOW))[-1][1]
    assert last_step["if"] == "always()"
    for test_id in ("programs", "conformance", "play"):
        assert f"steps.{test_id}.outcome" in str(last_step["env"])
