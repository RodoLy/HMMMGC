"""Tests of the files that make up the Windows package (packaging/windows). No Windows needed.

Pruebas de los archivos que forman el paquete para Windows (packaging/windows). Sin Windows.

PowerShell cannot be run here, so these tests check what can be checked from outside: that the
files agree with each other (the game's data in game.psd1 and in branding.cmake, the messages in
the two languages, the settings the script reads), and a few rules that Windows is strict about
(a batch file in plain ASCII). That the script itself works is tried on Windows ARM64, by the
workflow that builds the package.
"""

import re
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import package_windows  # noqa: E402

PACKAGING_DIRECTORY = Path(__file__).resolve().parent.parent / "packaging" / "windows"


def read_packaging_file(name: str) -> str:
    return (PACKAGING_DIRECTORY / name).read_text(encoding="utf-8")


def game_definition() -> dict[str, str]:
    """The text values of game.psd1: `Name = 'value'` lines."""
    return dict(re.findall(r"^\s*(\w+)\s*=\s*'([^']*)'", read_packaging_file("game.psd1"), re.MULTILINE))


def branding() -> dict[str, str]:
    """The values that branding.cmake gives to the cache: `set(NAME "value" CACHE ...)` lines."""
    return dict(
        re.findall(r'^set\((\w+) "?([^"\s]+)"? CACHE', read_packaging_file("branding.cmake"), re.MULTILINE)
    )


def messages_by_language() -> dict[str, set[str]]:
    """The keys of the messages of play.ps1, for each language."""
    script = read_packaging_file("play.ps1")
    blocks = re.findall(r"^    (\w\w) = @\{\n(.*?)^    \}", script, re.MULTILINE | re.DOTALL)
    return {language: set(re.findall(r"^\s+(\w+)\s+= '", block, re.MULTILINE)) for language, block in blocks}


# -- The files of the package folder -----------------------------------------------------------------


@pytest.mark.parametrize("name", package_windows.PACKAGE_FILES)
def test_every_file_the_assembler_expects_is_in_the_packaging_folder(name):
    assert (PACKAGING_DIRECTORY / name).is_file()


def test_the_licence_the_assembler_adds_is_in_the_repository():
    assert package_windows.LICENSE_FILE.is_file()


# -- game.psd1, branding.cmake and the assembler agree --------------------------------------------------


def test_the_disc_the_script_expects_is_the_disc_the_programs_are_built_for():
    assert game_definition()["GameId"] == branding()["MODERNGEKKO_REQUIRED_DISC_ID"]


def test_the_launcher_the_script_starts_is_the_launcher_that_is_built():
    assert game_definition()["LauncherName"] == branding()["MODERNGEKKO_LAUNCHER_OUTPUT_NAME"]


def test_the_folder_the_script_uses_is_the_folder_the_programs_use():
    assert game_definition()["UserDirectoryName"] == branding()["MODERNGEKKO_USER_DIRECTORY_NAME"]


def test_the_assembler_packs_the_settings_of_the_game_the_package_plays():
    assert game_definition()["GameId"] == package_windows.GAME_ID_OF_THE_PACKAGE_SETTINGS
    assert game_definition()["LauncherName"] == package_windows.DEFAULT_LAUNCHER_NAME


def test_the_game_id_has_the_six_characters_of_a_disc_id():
    assert re.fullmatch(r"[A-Z0-9]{6}", game_definition()["GameId"])


def test_the_disc_extensions_start_with_a_dot_and_are_lower_case():
    extensions = re.search(r"DiscExtensions\s*=\s*@\((.*?)\)", read_packaging_file("game.psd1"))
    listed = re.findall(r"'([^']*)'", extensions.group(1))
    assert listed
    assert all(re.fullmatch(r"\.[a-z0-9]+", extension) for extension in listed)


def test_the_branding_sets_only_options_of_moderngekko():
    assert branding()
    assert all(name.startswith("MODERNGEKKO_") for name in branding())


# -- play.ps1 -----------------------------------------------------------------------------------------


def test_the_script_reads_only_settings_that_game_psd1_has():
    used = set(re.findall(r"\$Game\.(\w+)", read_packaging_file("play.ps1")))
    assert used
    assert used <= set(game_definition()) | {"DiscExtensions"}  # a list, so not among the text values


def test_both_languages_have_the_same_messages():
    languages = messages_by_language()
    assert set(languages) == {"en", "es"}
    assert languages["en"] == languages["es"]


def test_every_message_the_script_asks_for_exists_and_none_is_left_unused():
    asked_for = set(re.findall(r"Get-Message '(\w+)'", read_packaging_file("play.ps1")))
    defined = messages_by_language()["en"]
    assert asked_for <= defined
    assert defined <= asked_for


def test_the_script_names_the_files_it_expects_to_find_in_the_package():
    script = read_packaging_file("play.ps1")
    for name in ("moderngekko-port.exe", "moderngekko-run.exe", "dolrecomp.exe", "clang.exe", "ninja.exe"):
        assert name in script


def test_the_script_has_no_tabs_and_no_trailing_spaces():
    for number, line in enumerate(read_packaging_file("play.ps1").splitlines(), start=1):
        assert "\t" not in line, f"play.ps1 line {number} has a tab"
        assert line == line.rstrip(), f"play.ps1 line {number} has trailing spaces"


def test_the_script_asks_for_the_powershell_that_windows_has_and_stops_on_errors():
    script = read_packaging_file("play.ps1")
    assert "#Requires -Version 5.1" in script
    assert "Set-StrictMode -Version Latest" in script
    assert "$ErrorActionPreference = 'Stop'" in script


def test_the_modules_directory_limit_matches_the_length_the_build_needs():
    # The build of a module adds about 200 characters to the folder it is made in, and Windows
    # stops at 260 (see the note in play.ps1 and docs/BUILDING.md).
    limit = int(re.search(r"\$MaximumModulesPathLength = (\d+)", read_packaging_file("play.ps1")).group(1))
    assert limit + 201 <= 260


# -- Play.bat -----------------------------------------------------------------------------------------


def test_the_batch_file_is_plain_ascii_because_cmd_reads_it_in_the_oem_code_page():
    assert all(byte < 128 for byte in (PACKAGING_DIRECTORY / "Play.bat").read_bytes())


def test_the_batch_file_runs_the_script_that_sits_next_to_it_with_what_it_was_given():
    command = [
        line
        for line in read_packaging_file("Play.bat").splitlines()
        if "play.ps1" in line and "rem" not in line
    ]
    assert len(command) == 1
    assert '"%~dp0play.ps1"' in command[0]  # the folder of the batch file, whatever the current folder
    assert command[0].rstrip().endswith("%*")  # a disc image dropped on the batch file
    assert "-ExecutionPolicy Bypass" in command[0]
    assert "-STA" in command[0]  # the file dialog needs a single-threaded apartment


def test_the_batch_file_waits_for_a_key_only_when_the_script_failed():
    lines = [line.strip() for line in read_packaging_file("Play.bat").splitlines()]
    assert "if errorlevel 1 (" in lines
    assert lines.index("pause") > lines.index("if errorlevel 1 (")
