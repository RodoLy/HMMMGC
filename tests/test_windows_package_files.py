"""Tests of the files that make up the Windows package (packaging/windows). No Windows needed.

Pruebas de los archivos que forman el paquete para Windows (packaging/windows). Sin Windows.

PowerShell cannot be run here, so these tests check what can be checked from outside: that the
files agree with each other (the game's data in game.psd1 and in branding.cmake, the messages in
the two languages, the settings the script reads), and a few rules that Windows is strict about
(a batch file in plain ASCII). That the script itself works is tried on Windows ARM64, by the
workflow that builds the package.
"""

import json
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


def messages_by_language(script_name: str = "play.ps1") -> dict[str, set[str]]:
    """The keys of the messages of a script (play.ps1 or Settings.ps1), for each language.

    A message is `Key = 'text'`, or `Key = @'` and the lines of the text up to `'@` when it has several.
    """
    script = read_packaging_file(script_name)
    table = re.search(r"^\$Messages = @\{\n(.*?)^\}", script, re.MULTILINE | re.DOTALL).group(1)
    blocks = re.findall(r"^    (\w\w) = @\{\n(.*?)^    \}", table, re.MULTILINE | re.DOTALL)
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
    given = set(re.findall(r'"-D(\w+)=', body_of_function(read_packaging_file("play.ps1"), "Install-Mod")))
    assert asked_for == {"MODERNGEKKO_SOURCE_DIR", "MOD_SYMBOL_HEADER"}
    assert asked_for <= given
    assert given - asked_for <= {"CMAKE_C_COMPILER"}  # what CMake itself reads


def test_the_mods_are_built_in_a_new_folder_each_time_and_only_the_ones_that_are_asked_for():
    body = body_of_function(read_packaging_file("play.ps1"), "Install-Mod")
    assert body.index("Remove-Item -LiteralPath $BuildDirectory") < body.index("$configureArguments")
    assert "'--target') + @($ModIds)" in body


def test_what_the_tools_say_while_the_mods_are_built_goes_to_the_log_and_not_to_the_console():
    body = body_of_function(read_packaging_file("play.ps1"), "Install-Mod")
    calls = re.findall(r"Invoke-Program [^\n]*", body)
    assert len(calls) == 2  # setting up the build, and building
    assert all(call.endswith("-Quiet") for call in calls)


def test_a_failure_while_the_mods_are_built_is_a_warning_and_the_game_starts_anyway():
    body = body_of_function(read_packaging_file("play.ps1"), "Install-Mod")
    before_catch, catch, after_catch = body.partition("catch {")
    assert catch
    assert "Get-Message 'ModsFailed' $LogPath" in after_catch
    assert "throw" not in after_catch  # the failure is not passed on to the script, which would end it
    assert "exit" not in after_catch


def test_a_mod_is_taken_out_by_the_name_of_its_folder_and_nothing_else_goes():
    body = body_of_function(read_packaging_file("play.ps1"), "Remove-InstalledMod")
    assert "foreach ($modId in $ModIds)" in body
    assert "Join-Path $InstalledModsDirectory ($modId + '.mgm')" in body


def test_playing_without_mods_takes_out_all_the_mods_of_the_package_and_does_not_read_the_settings():
    script = read_packaging_file("play.ps1")
    body = body_of_function(script, "Initialize-ModSetup")
    without_mods, reading, _ = body.partition("$settings = Get-SettingsFromFile")
    assert reading  # the settings are read, but only after the case of -NoMods
    assert re.search(
        r"if \(\$NoMods\) \{\s+Remove-InstalledMod -ModIds \$AllModIds\s+.*?\s+return\s+\}",
        without_mods,
        re.DOTALL,
    )
    assert "[switch] $NoMods" in script
    assert ".PARAMETER NoMods" in script


def test_the_mods_that_the_settings_turn_off_are_taken_out_before_the_others_are_built():
    body = body_of_function(read_packaging_file("play.ps1"), "Initialize-ModSetup")
    assert "$enabledModIds = @(Get-EnabledModId -Settings $settings)" in body
    assert "$disabledModIds = @($AllModIds | Where-Object { $enabledModIds -notcontains $_ })" in body
    removing = body.index("Remove-InstalledMod -ModIds $disabledModIds")
    building = body.index("Install-Mod -BuildDirectory $BuildDirectory -ModIds $enabledModIds")
    assert removing < building
    # With every mod turned off there is nothing to build (the build is not even asked for).
    assert "if ($enabledModIds.Count -gt 0) { Install-Mod" in body


def test_the_mods_are_set_up_once_the_package_and_the_game_are_ready():
    script = read_packaging_file("play.ps1")
    assert len(re.findall(r"^\s+Initialize-ModSetup ", script, re.MULTILINE)) == 1
    assert script.index("Install-GameModule -ModulePath") < script.index("    Initialize-ModSetup ")


def test_the_mods_go_where_the_runner_looks_for_them():
    # moderngekko-run reads <the player's folder>/Mods, and in it <mod id>.mgm/mod.dll.
    script = read_packaging_file("play.ps1")
    assert "$InstalledModsDirectory = Join-Path $UserDirectory 'Mods'" in script
    assert "($modId + '.mgm')" in script


def mod_definition(mod_id: str, name: str) -> str:
    """The value in `#define NAME value` of the source of a mod, as written."""
    source = (package_windows.MODS_DIRECTORY / mod_id / "mod.c").read_text(encoding="utf-8")
    match = re.search(rf"^#define {name}\s+(.+?)\s*$", source, re.MULTILINE)
    assert match, f"{mod_id} does not define {name}"
    return match.group(1)


def test_the_speed_the_script_gives_is_what_the_player_speed_mod_reads():
    script = read_packaging_file("play.ps1")
    variable = mod_definition("player_speed", "SPEED_PERCENT_VARIABLE").strip('"')
    assert f"$PlayerSpeedVariable = '{variable}'" in script
    # The limits that the script checks are the ones the mod accepts, so the mod never has to ignore a value.
    minimum = mod_definition("player_speed", "MINIMUM_SPEED_PERCENT")
    maximum = mod_definition("player_speed", "MAXIMUM_SPEED_PERCENT")
    assert f"[ValidateRange({minimum}, {maximum})]" in script
    assert int(minimum) <= int(mod_definition("player_speed", "DEFAULT_SPEED_PERCENT")) <= int(maximum)
    assert "[Nullable[int]] $PlayerSpeed" in script
    assert ".PARAMETER PlayerSpeed" in script


def test_the_speed_is_given_to_the_mod_only_while_it_is_on_and_a_speed_was_chosen():
    body = body_of_function(read_packaging_file("play.ps1"), "Set-PlayerSpeed")
    assert "if (-not $ModIsOn) {" in body
    assert "if ($null -eq $Percent) { return }" in body
    assert (
        body.index("-not $ModIsOn")
        < body.index("if ($null -eq $Percent)")
        < body.index("SetEnvironmentVariable")
    )
    assert "SetEnvironmentVariable($PlayerSpeedVariable, [string] $Percent, 'Process')" in body


def test_the_speed_asked_for_on_the_command_line_wins_over_the_saved_one():
    body = body_of_function(read_packaging_file("play.ps1"), "Initialize-ModSetup")
    saved = body.index("$percent = $settings.PlayerSpeedPercent")
    asked_for = body.index("if ($null -ne $PlayerSpeed) { $percent = $PlayerSpeed }")
    assert saved < asked_for < body.index("Set-PlayerSpeed -Percent $percent")
    # The mod is told only if the settings leave it on.
    assert "-ModIsOn ($enabledModIds -contains $PlayerSpeedModId)" in body


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


@pytest.mark.parametrize("batch_file", [*package_windows.BATCH_FILES, "Settings.bat"])
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


# -- The settings: Settings.bat, Settings.ps1 and settings-file.ps1 ----------------------------------------


def library_value(name: str) -> str:
    """The value of `$Name = value` in settings-file.ps1, as written."""
    match = re.search(rf"^\${name} = (.+?)\s*$", read_packaging_file("settings-file.ps1"), re.MULTILINE)
    assert match, f"settings-file.ps1 does not set ${name}"
    return match.group(1)


def settings_window_mod_texts() -> dict[str, dict[str, set[str]]]:
    """What Settings.ps1 says about each mod, per language: mod id -> its fields (Name, Description)."""
    script = read_packaging_file("Settings.ps1")
    table = re.search(r"^\$ModTexts = @\{\n(.*?)^\}", script, re.MULTILINE | re.DOTALL).group(1)
    texts = {}
    for language, block in re.findall(r"^    (\w\w) = @\{\n(.*?)^    \}", table, re.MULTILINE | re.DOTALL):
        texts[language] = {
            mod_id: set(re.findall(r"^\s+(\w+)\s+= '", fields, re.MULTILINE))
            for mod_id, fields in re.findall(
                r"^        (\w+)\s+= @\{\n(.*?)^        \}", block, re.MULTILINE | re.DOTALL
            )
        }
    return texts


@pytest.mark.parametrize("name", package_windows.SETTINGS_FILES)
def test_a_settings_file_has_no_tabs_and_no_trailing_spaces(name):
    for number, line in enumerate(read_packaging_file(name).splitlines(), start=1):
        assert "\t" not in line, f"{name} line {number} has a tab"
        assert line == line.rstrip(), f"{name} line {number} has trailing spaces"


def test_the_limits_of_the_settings_are_the_ones_the_player_speed_mod_accepts():
    for setting, definition in (
        ("PlayerSpeedPercentMinimum", "MINIMUM_SPEED_PERCENT"),
        ("PlayerSpeedPercentMaximum", "MAXIMUM_SPEED_PERCENT"),
        ("PlayerSpeedPercentDefault", "DEFAULT_SPEED_PERCENT"),
    ):
        assert library_value(setting) == mod_definition("player_speed", definition)


def test_the_mod_whose_speed_is_chosen_is_one_that_the_package_builds():
    assert library_value("PlayerSpeedModId").strip("'") in game_mod_ids()


def test_the_example_in_the_settings_code_is_a_file_that_the_code_reads_and_writes():
    library = read_packaging_file("settings-file.ps1")
    assert library_value("SettingsFileName") == "'settings.json'"
    example = json.loads(re.search(r"^ +\{\n.*?^ +\}$", library, re.MULTILINE | re.DOTALL).group(0))
    assert set(example) == {"mods", "playerSpeedPercent", "gameSpeedPercent"}
    assert set(example["mods"]) == set(game_mod_ids())
    minimum = int(library_value("PlayerSpeedPercentMinimum"))
    maximum = int(library_value("PlayerSpeedPercentMaximum"))
    assert minimum <= example["playerSpeedPercent"] <= maximum
    game_minimum = int(library_value("GameSpeedPercentMinimum"))
    game_maximum = int(library_value("GameSpeedPercentMaximum"))
    assert game_minimum <= example["gameSpeedPercent"] <= game_maximum
    reading = body_of_function(library, "Read-SettingsFile")
    writing = body_of_function(library, "Save-SettingsFile")
    for name in example:
        assert f"Properties['{name}']" in reading
    assert "[ordered] @{ mods = $Settings.Mods }" in writing
    assert "$content['playerSpeedPercent']" in writing
    assert "$content['gameSpeedPercent']" in writing


def test_the_speed_of_the_game_starts_at_the_normal_speed_and_only_goes_faster():
    minimum = int(library_value("GameSpeedPercentMinimum"))
    maximum = int(library_value("GameSpeedPercentMaximum"))
    default = int(library_value("GameSpeedPercentDefault"))
    assert minimum == default == 100  # a player who chose nothing plays at the normal speed
    assert maximum > minimum


def test_the_speed_of_the_game_goes_where_the_emulation_core_reads_it():
    # Dolphin reads EmulationSpeed from the [Core] section of Config/Dolphin.ini in the user folder
    # (Config::MAIN_EMULATION_SPEED), and its throttle follows it: 1 is the normal speed.
    assert library_value("CoreSettingsRelativePath") == "'Config\\Dolphin.ini'"
    assert library_value("CoreSettingsSection") == "'Core'"
    assert library_value("EmulationSpeedKey") == "'EmulationSpeed'"


def test_the_speed_of_the_game_is_written_with_a_point_whatever_the_language_of_windows_is():
    library = read_packaging_file("settings-file.ps1")
    converting = body_of_function(library, "ConvertTo-EmulationSpeedText")
    assert "[System.Globalization.CultureInfo]::InvariantCulture" in converting
    assert "$Percent / 100" in converting


def test_the_core_settings_are_changed_one_line_at_a_time_and_kept_otherwise():
    library = read_packaging_file("settings-file.ps1")
    writing = body_of_function(library, "Set-CoreEmulationSpeed")
    assert "Set-IniValue -Lines $lines" in writing
    # Without a byte order mark, as Dolphin writes the file.
    assert "New-Object System.Text.UTF8Encoding($false)" in writing
    editing = body_of_function(library, "Set-IniValue")
    assert "$result.Add($line)" in editing  # every other line is copied as it was


def test_the_script_sets_the_speed_of_the_game_with_and_without_mods():
    script = read_packaging_file("play.ps1")
    body = body_of_function(script, "Initialize-ModSetup")
    without_mods, _, with_mods = body.partition("$settings = Get-SettingsFromFile")
    assert "Set-GameSpeed -Percent $GameSpeedPercentDefault" in without_mods
    assert "Set-GameSpeed -Percent $settings.GameSpeedPercent" in with_mods
    setting = body_of_function(script, "Set-GameSpeed")
    assert "Set-CoreEmulationSpeed -UserDirectory $UserDirectory" in setting
    assert "catch {" in setting  # a failure is a warning, and the game starts anyway


def test_the_settings_window_shows_and_saves_the_speed_of_the_game():
    window = read_packaging_file("Settings.ps1")
    shown = body_of_function(window, "Get-SettingsFromWindow")
    assert "GameSpeedPercent   = [int] $GameSpeedBox.Value" in shown
    resetting = body_of_function(window, "Reset-SettingsWindow")
    assert "$GameSpeedBox.Value = $GameSpeedPercentDefault" in resetting
    assert "-Minimum $GameSpeedPercentMinimum -Maximum $GameSpeedPercentMaximum" in window


def test_the_settings_code_never_stops_the_game_for_a_value_it_cannot_use():
    reading = body_of_function(read_packaging_file("settings-file.ps1"), "Read-SettingsFile")
    assert "catch {" in reading  # a file that is not JSON at all is a problem to report
    assert "$problems.Add(" in reading
    assert "throw" not in reading


def test_the_settings_batch_file_is_plain_ascii_because_cmd_reads_it_in_the_oem_code_page():
    assert all(byte < 128 for byte in (PACKAGING_DIRECTORY / "Settings.bat").read_bytes())


def test_the_settings_batch_file_runs_the_window_that_sits_next_to_it():
    commands = [
        line
        for line in read_packaging_file("Settings.bat").splitlines()
        if "Settings.ps1" in line and not line.lstrip().startswith("rem")
    ]
    assert len(commands) == 1
    assert '"%~dp0Settings.ps1"' in commands[0]  # the folder of the batch file, wherever it is run from
    assert "-ExecutionPolicy Bypass" in commands[0]
    assert "-STA" in commands[0]  # a window needs a single-threaded apartment


def test_the_settings_window_asks_for_the_powershell_that_windows_has_and_stops_on_errors():
    window = read_packaging_file("Settings.ps1")
    assert "#Requires -Version 5.1" in window
    assert "#Requires -Version 5.1" in read_packaging_file("settings-file.ps1")
    assert "Set-StrictMode -Version Latest" in window
    assert "$ErrorActionPreference = 'Stop'" in window


def test_the_settings_window_only_reads_values_that_the_game_definition_has():
    keys_read = set(re.findall(r"\$Game\.(\w+)", read_packaging_file("Settings.ps1")))
    assert keys_read
    assert keys_read <= set(game_definition()) | {"ModIds"}


def test_the_settings_window_has_the_same_messages_in_both_languages():
    languages = messages_by_language("Settings.ps1")
    assert set(languages) == {"en", "es"}
    assert languages["en"] == languages["es"]


def test_every_message_the_settings_window_asks_for_exists_and_none_is_left_unused():
    asked_for = set(re.findall(r"Get-Message '(\w+)'", read_packaging_file("Settings.ps1")))
    defined = messages_by_language("Settings.ps1")["en"]
    assert asked_for <= defined
    assert defined <= asked_for


def test_the_settings_window_can_make_a_label_with_no_text():
    # The line that says what the last click did starts empty, and PowerShell refuses an empty string
    # for a mandatory parameter ("Cannot bind argument to parameter 'Text' because it is an empty
    # string") unless the parameter allows it. The first run of the window on Windows failed this way.
    window = read_packaging_file("Settings.ps1")
    assert "[Parameter(Mandatory)] [AllowEmptyString()] [string] $Text," in body_of_function(
        window, "New-TextLabel"
    )
    assert "$StatusLabel = New-TextLabel -Text $StatusText" in window
    assert "$StatusText = ''" in window


def test_the_settings_window_describes_every_mod_of_the_package_in_both_languages():
    texts = settings_window_mod_texts()
    assert set(texts) == {"en", "es"}
    for language, mods in texts.items():
        assert set(mods) == set(game_mod_ids()), language
        assert all(fields == {"Name", "Description"} for fields in mods.values()), language


def test_the_settings_window_starts_the_games_with_the_batch_files_of_the_package():
    window = read_packaging_file("Settings.ps1")
    for batch_file in ("Play-Keyboard.bat", "Play.bat"):
        assert f"Join-Path $PackageDirectory '{batch_file}'" in window
        assert batch_file in package_windows.BATCH_FILES


def test_the_window_and_the_script_share_the_code_that_reads_the_settings():
    assert ". (Join-Path $PackageDirectory 'settings-file.ps1')" in read_packaging_file("Settings.ps1")
    script = read_packaging_file("play.ps1")
    assert "$SettingsLibraryPath = Join-Path $PackageDirectory 'settings-file.ps1'" in script
    required = re.search(r"\$RequiredFiles = @\((.*?)^\)", script, re.MULTILINE | re.DOTALL).group(1)
    assert "$SettingsLibraryPath" in required
    # It is loaded after the check that says whether it is there (a package without it gets the message
    # about an incomplete package, not a PowerShell error) and before anything uses it.
    checked = script.index("$missingFile = Find-MissingFile")
    loaded = script.index(". $SettingsLibraryPath")
    used = script.index("Initialize-ModSetup -BuildDirectory")
    assert checked < loaded < used


def test_the_readme_explains_the_settings_in_both_languages_with_examples_that_are_valid():
    readme = read_packaging_file("README.md")
    spanish_part, english_part = readme.split("\n## English\n", 1)
    for part in (spanish_part, english_part):
        for name in package_windows.SETTINGS_FILES:
            assert f"`{name}`" in part, f"{name} is not described in the README"
        assert "`%LOCALAPPDATA%\\HarvestMoonMM\\settings.json`" in part
        (example,) = re.findall(r"```json\n(.*?)```", part, re.DOTALL)
        settings = json.loads(example)
        assert set(settings["mods"]) == set(game_mod_ids())
        minimum = int(library_value("PlayerSpeedPercentMinimum"))
        maximum = int(library_value("PlayerSpeedPercentMaximum"))
        assert minimum <= settings["playerSpeedPercent"] <= maximum
        assert f"de {minimum} a {maximum}" in part or f"from {minimum} to {maximum}" in part
