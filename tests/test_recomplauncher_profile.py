"""packaging/native/recomplauncher.json agrees with the launcher and the game it describes.

packaging/native/recomplauncher.json concuerda con el launcher y el juego que describe.

RecompLauncher reads the file to set the game up (HarvestMoonMM --prepare), start it (--play) and show
the player's options on the game's page. The options it writes are those of settings.json
(shared/settings.h), with the values the launcher and the game accept; the mods are those the runtime
has; the exit codes are the launcher's (launcher/src/headless.h). Its schema is RecompLauncher's
(https://recomplauncher.com/schema/recomplauncher.json), checked by hand when the file changes.
"""

import json
import re
from pathlib import Path

import pytest

REPOSITORY = Path(__file__).resolve().parent.parent
PROFILE_FILE = REPOSITORY / "packaging" / "native" / "recomplauncher.json"
PROFILE = json.loads(PROFILE_FILE.read_text(encoding="utf-8"))
SETTINGS_HEADER = (REPOSITORY / "shared" / "settings.h").read_text(encoding="utf-8")
HEADLESS_HEADER = (REPOSITORY / "launcher" / "src" / "headless.h").read_text(encoding="utf-8")
RUNTIME_CMAKE = (REPOSITORY / "runtime" / "CMakeLists.txt").read_text(encoding="utf-8")


def setting_name(constant: str) -> str:
    return re.search(rf'{constant} = "([^"]+)"', SETTINGS_HEADER).group(1)


def numbers(constant: str) -> list[str]:
    values = re.search(rf"{constant}\[\] = \{{([^}}]*)\}}", SETTINGS_HEADER).group(1)
    return [value.strip() for value in values.split(",")]


def choice_values(setting_id: str) -> tuple[str, list[str]]:
    setting = next(setting for setting in PROFILE["settings"] if setting["id"] == setting_id)
    keys = {key for choice in setting["choices"] for key in choice["set"]}
    assert len(keys) == 1
    key = keys.pop()
    return key, [choice["set"][key] for choice in setting["choices"]]


def test_the_launcher_prepares_and_starts_the_game():
    assert PROFILE["prepare"]["args"] == ["--prepare", "{game}"]
    assert PROFILE["start"]["args"] == ["--play"]
    assert PROFILE["prepare"]["done"] == ["{local}/HarvestMoonMM/Native/Game/prepared.json"]


def test_the_exit_codes_are_the_launcher_s():
    codes = {name: value for name, value in re.findall(r"constexpr int (\w+) = (\d+);", HEADLESS_HEADER)}
    assert set(PROFILE["exit_codes"]) == {codes["NOT_PREPARED"], codes["NOT_STARTED"]}


def test_the_options_are_those_of_settings_json_with_the_values_the_game_accepts():
    key, values = choice_values("language")
    assert key == setting_name("LANGUAGE")
    assert [json.loads(value) for value in values] == re.findall(
        r'"(\w\w)"', re.search(r"GAME_LANGUAGES\[\] = \{([^}]*)\}", SETTINGS_HEADER).group(1)
    )
    key, values = choice_values("resolution")
    assert key == setting_name("RESOLUTION") and values == numbers("RESOLUTIONS")
    key, values = choice_values("frame_rate")
    assert key == setting_name("FRAME_RATE") and values == numbers("FRAME_RATES")
    for setting_id, constant in (("fullscreen", "FULLSCREEN"), ("widescreen", "WIDESCREEN")):
        key, values = choice_values(setting_id)
        assert key == setting_name(constant) and values == ["false", "true"]
    sliders = {
        setting["id"]: setting["slider"]["key"] for setting in PROFILE["settings"] if "slider" in setting
    }
    assert sliders == {
        "volume": setting_name("VOLUME"),
        "player_speed_percent": setting_name("PLAYER_SPEED"),
        "day_length_percent": mod_setting("longer_days")["key"],
        "stamina_cost_percent": mod_setting("gentle_stamina")["key"],
        "tool_experience_percent": mod_setting("faster_tools")["key"],
    }


def mod_setting(mod_id: str) -> dict:
    manifest = json.loads((REPOSITORY / "mods" / mod_id / "mod.json").read_text(encoding="utf-8"))
    (setting,) = manifest["settings"]
    return setting


@pytest.mark.parametrize(
    ("mod_id", "slider_id"),
    [
        ("longer_days", "day_length_percent"),
        ("gentle_stamina", "stamina_cost_percent"),
        ("faster_tools", "tool_experience_percent"),
    ],
)
def test_a_mod_s_slider_has_the_limits_of_the_mod(mod_id: str, slider_id: str):
    setting = mod_setting(mod_id)
    slider = next(entry for entry in PROFILE["settings"] if entry["id"] == slider_id)
    assert (slider["slider"]["min"], slider["slider"]["max"], slider["slider"]["step"]) == (
        setting["minimum"],
        setting["maximum"],
        setting["step"],
    )
    assert slider["defaults"] == {setting["key"]: str(setting["default"])}
    assert slider["when"] == {f"mods/{mod_id}": "true"}


def test_every_mod_of_the_runtime_can_be_turned_on():
    runtime_mods = re.search(r'set\(NATIVE_MODS "([^"]*)"', RUNTIME_CMAKE).group(1).split(";")
    mods_key = setting_name("MODS")
    turned_on = {
        key.split("/", 1)[1]
        for setting in PROFILE["settings"]
        for choice in setting.get("choices", [])
        for key in choice["set"]
        if key.startswith(mods_key + "/")
    }
    # The cheats mod has no switch of its own: it goes with the cheats (the next test).
    assert turned_on == set(runtime_mods) - {"cheats"}


def test_every_cheat_can_be_turned_on():
    names = re.findall(r'"(\w+)"', re.search(r"CHEAT_NAMES\[\] = \{([^}]*)\}", SETTINGS_HEADER).group(1))
    cheats_key = re.search(r'CHEATS = "([^"]+)"', SETTINGS_HEADER).group(1)
    turned_on = {
        key.split("/", 1)[1]
        for setting in PROFILE["settings"]
        for choice in setting.get("choices", [])
        for key in choice["set"]
        if key.startswith(cheats_key + "/")
    }
    assert turned_on == set(names)


def test_the_saves_and_the_logs_are_in_the_player_s_folder():
    assert PROFILE["saves"] == ["{local}/HarvestMoonMM/Native/USA"]
    assert PROFILE["folders"]["logs"] == [{"path": "{local}/HarvestMoonMM/Native"}]
