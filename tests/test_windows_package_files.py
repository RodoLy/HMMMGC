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


def body_of_function(script: str, name: str) -> str:
    """What is between the braces of `function <name> { ... }`, whose closing brace is at the margin."""
    return re.search(rf"function {name} \{{(.*?)^\}}", script, re.MULTILINE | re.DOTALL).group(1)


def messages_by_language() -> dict[str, set[str]]:
    """The keys of the messages of play.ps1, for each language.

    A message is `Key = 'text'`, or `Key = @'` and the lines of the text up to `'@` when it has several.
    """
    script = read_packaging_file("play.ps1")
    blocks = re.findall(r"^    (\w\w) = @\{\n(.*?)^    \}", script, re.MULTILINE | re.DOTALL)
    return {
        language: set(re.findall(r"^\s+(\w+)\s+= @?'", block, re.MULTILINE)) for language, block in blocks
    }


# -- The files of the package folder -----------------------------------------------------------------


@pytest.mark.parametrize("name", package_windows.PACKAGE_FILES)
def test_every_file_the_assembler_expects_is_in_the_packaging_folder(name):
    assert (PACKAGING_DIRECTORY / name).is_file()


def test_no_file_of_the_packaging_folder_is_forgotten_by_the_assembler():
    build_time_only = {"branding.cmake"}  # the programs are built with it; the player never needs it
    found = {path.name for path in PACKAGING_DIRECTORY.iterdir() if path.is_file()}
    assert found == set(package_windows.PACKAGE_FILES) | build_time_only


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


def test_the_project_has_a_settings_file_for_the_game_the_package_plays():
    assert package_windows.GAME_SETTINGS_FILE.is_file()
    assert package_windows.GAME_SETTINGS_FILE.parent.name == game_definition()["GameId"]


def project_settings() -> dict[str, str]:
    """The `Name = value` lines of the project's settings file, with the section each is in."""
    settings = {}
    section = ""
    for line in package_windows.GAME_SETTINGS_FILE.read_text(encoding="utf-8").splitlines():
        line = line.strip()
        if line.startswith("[") and line.endswith("]"):
            section = line[1:-1]
        elif line and not line.startswith("#"):
            name, _, value = line.partition("=")
            settings[f"{section}.{name.strip()}"] = value.strip()
    return settings


def test_the_projects_settings_are_in_the_core_section_where_dolphin_reads_them():
    assert set(project_settings()) == {"Core.StaticRecompIdlePC", "Core.FastDiscSpeed"}


def test_the_idle_loop_is_an_address_of_the_games_code():
    address = int(project_settings()["Core.StaticRecompIdlePC"], 16)
    assert 0x80003100 <= address < 0x81800000  # main memory, from where the code starts
    assert address % 4 == 0  # an instruction


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


def test_what_a_program_writes_on_its_error_output_is_shown_as_its_lines_and_not_as_class_names():
    # Windows PowerShell 5.1 turns an empty line of the error output into the name of its class
    # ("System.Management.Automation.RemoteException") when the record is made a string.
    script = read_packaging_file("play.ps1")
    conversion = body_of_function(script, "ConvertTo-ProgramText")
    assert "[System.Management.Automation.ErrorRecord]" in conversion
    assert "$Output.Exception.Message" in conversion
    invoking = body_of_function(script, "Invoke-Program")
    assert "ConvertTo-ProgramText $_" in invoking
    assert "[string] $_" not in invoking


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


# -- The speed meter: Measure-Speed.bat and Measure-Speed.ps1 ---------------------------------------


@pytest.mark.parametrize("name", package_windows.MEASURING_FILES)
def test_a_speed_meter_file_has_no_tabs_and_no_trailing_spaces(name):
    for number, line in enumerate(read_packaging_file(name).splitlines(), start=1):
        assert "\t" not in line, f"{name} line {number} has a tab"
        assert line == line.rstrip(), f"{name} line {number} has trailing spaces"


def test_the_speed_meter_batch_file_is_plain_ascii_because_cmd_reads_it_in_the_oem_code_page():
    assert all(byte < 128 for byte in (PACKAGING_DIRECTORY / "Measure-Speed.bat").read_bytes())


def test_the_speed_meter_batch_file_runs_the_script_that_sits_next_to_it():
    commands = [
        line
        for line in read_packaging_file("Measure-Speed.bat").splitlines()
        if "Measure-Speed.ps1" in line and not line.lstrip().startswith("rem")
    ]
    assert len(commands) == 1
    assert '"%~dp0Measure-Speed.ps1"' in commands[0]  # the folder of the batch file, wherever it is run from
    assert "-ExecutionPolicy Bypass" in commands[0]


def test_the_speed_meter_asks_for_the_powershell_that_windows_has_and_stops_on_errors():
    script = read_packaging_file("Measure-Speed.ps1")
    assert "#Requires -Version 5.1" in script
    assert "Set-StrictMode -Version Latest" in script
    assert "$ErrorActionPreference = 'Stop'" in script


def test_the_speed_meter_only_reads_values_that_the_game_definition_has():
    keys_read = set(re.findall(r"\$Game\.(\w+)", read_packaging_file("Measure-Speed.ps1")))
    assert keys_read
    assert keys_read <= set(game_definition())


def test_the_speed_meter_starts_the_runner_with_the_graphics_the_keyboard_batch_file_uses():
    # Measuring with other graphics than the ones that are played with would measure something else.
    assert "'--graphics', $Game.GraphicsBackend" in read_packaging_file("Measure-Speed.ps1")
    keyboard_start = body_of_function(read_packaging_file("play.ps1"), "Start-GameWithKeyboard")
    assert "$Game.GraphicsBackend" in keyboard_start


# -- The batch files: Play.bat and Play-Keyboard.bat -------------------------------------------------


def script_command_of(batch_file: str) -> str:
    """The line of a batch file that runs play.ps1 (the lines that start with `rem` are comments)."""
    command = [
        line
        for line in read_packaging_file(batch_file).splitlines()
        if "play.ps1" in line and not line.lstrip().startswith("rem")
    ]
    assert len(command) == 1
    return command[0]


@pytest.mark.parametrize("batch_file", package_windows.BATCH_FILES)
def test_a_batch_file_is_plain_ascii_because_cmd_reads_it_in_the_oem_code_page(batch_file):
    assert all(byte < 128 for byte in (PACKAGING_DIRECTORY / batch_file).read_bytes())


@pytest.mark.parametrize("batch_file", package_windows.BATCH_FILES)
def test_a_batch_file_runs_the_script_that_sits_next_to_it_with_what_it_was_given(batch_file):
    command = script_command_of(batch_file)
    assert '"%~dp0play.ps1"' in command  # the folder of the batch file, whatever the current folder
    assert command.rstrip().endswith("%*")  # a disc image dropped on the batch file
    assert "-ExecutionPolicy Bypass" in command
    assert "-STA" in command  # the file dialog needs a single-threaded apartment


@pytest.mark.parametrize("batch_file", package_windows.BATCH_FILES)
def test_a_batch_file_waits_for_a_key_only_when_the_script_failed(batch_file):
    lines = [line.strip() for line in read_packaging_file(batch_file).splitlines()]
    assert "if errorlevel 1 (" in lines
    assert lines.index("pause") > lines.index("if errorlevel 1 (")


def test_only_the_keyboard_batch_file_asks_for_the_keyboard():
    assert "-Keyboard" in script_command_of("Play-Keyboard.bat")
    assert "-Keyboard" not in script_command_of("Play.bat")


def test_the_keyboard_switch_is_declared_and_opens_the_runner_with_the_game_and_the_players_folder():
    script = read_packaging_file("play.ps1")
    assert re.search(r"\[switch\] \$Keyboard\b", script)
    start = body_of_function(script, "Start-GameWithKeyboard")
    assert "$RunnerPath" in start
    assert "'--game', $ExtractedGameDirectory" in start  # what the launcher gives the runner, too
    assert "'--user-dir', $UserDirectory" in start


# The names that Dolphin's video backends have on Windows, as the runner's --graphics option takes them.
DOLPHIN_VIDEO_BACKENDS_ON_WINDOWS = {"D3D12", "D3D", "Vulkan"}


def test_the_keyboard_start_gives_the_runner_the_graphics_api_that_game_psd1_names():
    # What the runner picks when it is not told depends on the patches it was built with (Vulkan in
    # an unpatched one, which draws nothing on the Snapdragon laptop of the first real test), so the
    # script says which API to use.
    start = body_of_function(read_packaging_file("play.ps1"), "Start-GameWithKeyboard")
    assert "'--graphics', $Game.GraphicsBackend" in start


def test_the_graphics_api_in_game_psd1_is_a_name_that_dolphin_knows():
    # The runner gives the name to Dolphin without checking it, so a mistyped one would show only
    # when someone starts the game.
    assert game_definition()["GraphicsBackend"] in DOLPHIN_VIDEO_BACKENDS_ON_WINDOWS
