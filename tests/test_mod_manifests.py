"""Tests of the mods' manifests (mods/<id>/mod.json), which the launcher shows. No game data needed.

Pruebas de los manifiestos de los mods, que muestra el launcher. No necesitan datos del juego.

Each mod that the native runtime compiles in needs one, and it must agree with the mod's own C
descriptor: the same id and version. Every text is in Spanish and in English.
"""

import json
import re
from pathlib import Path

import pytest

REPOSITORY = Path(__file__).resolve().parent.parent
MODS = REPOSITORY / "mods"
RUNTIME_CMAKE = REPOSITORY / "runtime" / "CMakeLists.txt"
LANGUAGES = ("es", "en")
TEXTS = ("name", "summary", "description")


def native_mods() -> list[str]:
    """The mods the runtime compiles in by default (NATIVE_MODS in runtime/CMakeLists.txt)."""
    match = re.search(r'set\(NATIVE_MODS "([^"]*)" CACHE', RUNTIME_CMAKE.read_text(encoding="utf-8"))
    assert match, "NATIVE_MODS is not set in runtime/CMakeLists.txt"
    return [mod for mod in match[1].split(";") if mod]


def manifest(mod: str) -> dict:
    return json.loads((MODS / mod / "mod.json").read_text(encoding="utf-8"))


def test_every_native_mod_has_a_manifest():
    assert native_mods()
    for mod in native_mods():
        assert (MODS / mod / "mod.json").is_file(), f"{mod} has no mod.json"


@pytest.mark.parametrize("mod", native_mods())
def test_the_manifest_has_the_id_and_version_of_the_mod_s_descriptor(mod):
    source = (MODS / mod / "mod.c").read_text(encoding="utf-8")
    version = re.search(r'\.version = "([^"]+)"', source)
    assert version, f"no version in {mod}/mod.c"
    data = manifest(mod)
    assert data["id"] == mod
    assert data["version"] == version[1]
    assert data["authors"] and all(isinstance(author, str) and author for author in data["authors"])


@pytest.mark.parametrize("mod", native_mods())
def test_every_text_of_the_manifest_is_in_both_languages(mod):
    data = manifest(mod)
    for name in (*TEXTS, *(["warning"] if "warning" in data else [])):
        for language in LANGUAGES:
            assert data[name][language].strip(), f"{mod}: {name} has no {language} text"
    for setting in data.get("settings", []):
        for language in LANGUAGES:
            assert setting["label"][language].strip()


@pytest.mark.parametrize("mod", native_mods())
def test_settings_are_percentages_the_launcher_can_show(mod):
    for setting in manifest(mod).get("settings", []):
        assert setting["type"] == "percent"
        assert setting["key"] and setting["environment"]
        assert setting["minimum"] < setting["maximum"]
        assert setting["minimum"] <= setting["default"] <= setting["maximum"]
        assert setting["step"] > 0
        assert (setting["default"] - setting["minimum"]) % setting["step"] == 0


def test_the_player_speed_setting_has_the_mod_s_variable_and_limits():
    source = (MODS / "player_speed" / "mod.c").read_text(encoding="utf-8")
    (setting,) = manifest("player_speed")["settings"]
    assert f'"{setting["environment"]}"' in source
    minimum = re.search(r"#define MINIMUM_SPEED_PERCENT (\d+)", source)
    maximum = re.search(r"#define MAXIMUM_SPEED_PERCENT (\d+)", source)
    default = re.search(r"#define DEFAULT_SPEED_PERCENT (\d+)", source)
    assert minimum and maximum and default
    assert (setting["minimum"], setting["maximum"], setting["default"]) == (
        int(minimum[1]),
        int(maximum[1]),
        int(default[1]),
    )
