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


def game_mod_ids() -> list[str]:
    """The names in `ModIds = @('a', 'b')` of game.psd1: the mods that play.ps1 builds."""
    listed = re.search(r"ModIds\s*=\s*@\((.*?)\)", read_packaging_file("game.psd1"))
    return re.findall(r"'([^']*)'", listed.group(1))


def mods_cmake_file() -> str:
    return (package_windows.MODS_DIRECTORY / "CMakeLists.txt").read_text(encoding="utf-8")


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


# -- The mods the package carries ---------------------------------------------------------------------


def test_the_project_has_the_mods_and_the_symbols_that_the_assembler_packs():
    assert (package_windows.MODS_DIRECTORY / "CMakeLists.txt").is_file()
    assert package_windows.SYMBOLS_PATH.is_file()


def test_every_function_the_mods_name_is_in_the_header_the_package_gets():
    # A mod that names a function the header lacks would stop building on the player's computer.
    header = package_windows.mod_symbols_header(package_windows.SYMBOLS_PATH)
    defined = set(re.findall(r"^#define (DOLRECOMP_SYMBOL_\w+) ", header, re.MULTILINE))
    used = {
        name
        for source in package_windows.MODS_DIRECTORY.glob("*/mod.c")
        for name in re.findall(r"\bDOLRECOMP_SYMBOL_\w+", source.read_text(encoding="utf-8"))
    }
    assert used
    assert used <= defined


def test_the_mods_the_package_builds_are_listed_and_have_their_source_and_their_cmake_target():
    mod_ids = game_mod_ids()
    assert mod_ids
    assert len(set(mod_ids)) == len(mod_ids)
    for mod_id in mod_ids:
        assert re.fullmatch(r"[a-z][a-z0-9_]*", mod_id), f"{mod_id} is not a name for a folder and a target"
        source = package_windows.MODS_DIRECTORY / mod_id / "mod.c"
        assert source.is_file(), f"{source} is missing"
        target_line = rf"^add_game_mod\({mod_id} "
        assert re.search(target_line, mods_cmake_file(), re.MULTILINE), f"{mod_id} is not a CMake target"


def test_every_mod_the_package_builds_is_for_the_game_the_package_plays():
    for mod_id in game_mod_ids():
        source = (package_windows.MODS_DIRECTORY / mod_id / "mod.c").read_text(encoding="utf-8")
        assert "HM_MM_GAME_ID" in source, f"{mod_id} does not say which game it is for"
    header = (package_windows.MODS_DIRECTORY / "include" / "hm_mm_mod.h").read_text(encoding="utf-8")
    assert f'#define HM_MM_GAME_ID "{game_definition()["GameId"]}"' in header


def test_the_script_looks_for_the_mods_where_the_assembler_puts_them():
    script = read_packaging_file("play.ps1")
    assert f"Join-Path $PackageDirectory '{package_windows.MODS_FOLDER_IN_PACKAGE}'" in script
    # The script builds the name of the header from the game's ID, so the ID is not written in it.
    generated = package_windows.MOD_SYMBOLS_HEADER.replace("/", "\\")
    generated = generated.replace(game_definition()["GameId"], "' + $Game.GameId + '")
    assert f"Join-Path $ModsSourceDirectory ('{generated}')" in script
    # The headers of ModernGekko are in bin\source, laid out as in its source tree (include/moderngekko).
    assert "Join-Path $ProgramsDirectory 'source'" in script
    assert {header.rsplit("/", 1)[0] for header in package_windows.MOD_HEADERS} == {"include/moderngekko"}


def test_the_script_gives_cmake_the_options_that_the_mods_cmake_file_asks_for():
    asked_for = set(re.findall(r"^set\((\w+) \"\" CACHE", mods_cmake_file(), re.MULTILINE))
    given = set(re.findall(r'"-D(\w+)=', body_of_function(read_packaging_file("play.ps1"), "Install-Mods")))
    assert asked_for == {"MODERNGEKKO_SOURCE_DIR", "MOD_SYMBOL_HEADER"}
    assert asked_for <= given
    assert given - asked_for <= {"CMAKE_C_COMPILER"}  # what CMake itself reads


def test_the_mods_are_built_in_a_new_folder_each_time_and_only_the_listed_ones():
    body = body_of_function(read_packaging_file("play.ps1"), "Install-Mods")
    assert body.index("Remove-Item -LiteralPath $BuildDirectory") < body.index("$configureArguments")
    assert "'--target') + @($Game.ModIds)" in body


def test_what_the_tools_say_while_the_mods_are_built_goes_to_the_log_and_not_to_the_console():
    body = body_of_function(read_packaging_file("play.ps1"), "Install-Mods")
    calls = re.findall(r"Invoke-Program [^\n]*", body)
    assert len(calls) == 2  # setting up the build, and building
    assert all(call.endswith("-Quiet") for call in calls)


def test_a_failure_while_the_mods_are_built_is_a_warning_and_the_game_starts_anyway():
    body = body_of_function(read_packaging_file("play.ps1"), "Install-Mods")
    before_catch, catch, after_catch = body.partition("catch {")
    assert catch
    assert "Get-Message 'ModsFailed' $LogPath" in after_catch
    assert "throw" not in after_catch  # the failure is not passed on to the script, which would end it
    assert "exit" not in after_catch


def test_playing_without_mods_takes_out_only_the_mods_of_the_package():
    script = read_packaging_file("play.ps1")
    body = body_of_function(script, "Remove-InstalledMods")
    assert "foreach ($modId in $Game.ModIds)" in body
    assert "Join-Path $InstalledModsDirectory ($modId + '.mgm')" in body
    assert re.search(r"if \(\$NoMods\) \{\s+Remove-InstalledMods\s+\}\s+else \{\s+Install-Mods", script)
    assert "[switch] $NoMods" in script
    assert ".PARAMETER NoMods" in script


def test_the_mods_go_where_the_runner_looks_for_them():
    # moderngekko-run reads <the player's folder>/Mods, and in it <mod id>.mgm/mod.dll.
    script = read_packaging_file("play.ps1")
    assert "$InstalledModsDirectory = Join-Path $UserDirectory 'Mods'" in script
    assert "($modId + '.mgm')" in script


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
    lists = {"DiscExtensions", "ModIds"}  # they are not among the text values
    assert used <= set(game_definition()) | lists


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
