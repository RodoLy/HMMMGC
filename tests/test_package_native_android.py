"""Tests of tools/package_native_android.py with a made-up kit, NDK sysroot and shell app.

Pruebas del armado del paquete de Android con un kit, un sysroot del NDK y una app base inventados.
"""

import json
import sys
import zipfile
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from package_native_android import Inputs, PackageError, assemble, write_archive  # noqa: E402


def touch(path: Path, content: bytes = b"x") -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    return path


def made_up_inputs(tmp_path: Path) -> Inputs:
    kit = tmp_path / "kit"
    touch(kit / "lib" / "000_main.cpp.o")
    recipe = {
        "format": 1,
        "system": "Android",
        "processor": "aarch64",
        "executable": "libmain.so",
        "cxx_compiler": "clang++",
        "c_compiler": "clang",
        "compile": ["--target=aarch64-none-linux-android26", "--sysroot={kit}/sysroot", "-O2"],
        "link": ["--sysroot={kit}/sysroot", "-shared", "{game_objects}", "-o", "{output}"],
        "mods": ["player_speed"],
    }
    touch(kit / "recipe.json", json.dumps(recipe).encode())
    sysroot = tmp_path / "ndk-sysroot"
    touch(sysroot / "usr" / "include" / "stdio.h")
    touch(sysroot / "usr" / "include" / "aarch64-linux-android" / "asm" / "types.h")
    touch(sysroot / "usr" / "include" / "x86_64-linux-android" / "asm" / "types.h")
    touch(sysroot / "usr" / "lib" / "aarch64-linux-android" / "libc++_static.a")
    for level in ("21", "26", "35"):
        touch(sysroot / "usr" / "lib" / "aarch64-linux-android" / level / "crtbegin_so.o")
    touch(sysroot / "usr" / "lib" / "x86_64-linux-android" / "libc++_static.a")
    shell = tmp_path / "shell.apk"
    with zipfile.ZipFile(shell, "w") as app:
        for name in ("AndroidManifest.xml", "classes.dex", "resources.arsc"):
            app.writestr(name, b"x")
    return Inputs(kit=kit, ndk_sysroot=sysroot, shell=shell, output_directory=tmp_path / "out")


def test_the_package_puts_the_kit_with_a_trimmed_sysroot_and_the_shell_in_builder_android(tmp_path):
    package = assemble(made_up_inputs(tmp_path))
    android = package / "builder" / "android"
    assert (android / "shell.apk").is_file()
    sysroot = android / "kit" / "sysroot"
    assert (sysroot / "usr" / "include" / "stdio.h").is_file()
    assert (sysroot / "usr" / "lib" / "aarch64-linux-android" / "libc++_static.a").is_file()
    assert (sysroot / "usr" / "lib" / "aarch64-linux-android" / "26" / "crtbegin_so.o").is_file()
    for left_out in (
        "usr/lib/aarch64-linux-android/21",
        "usr/lib/aarch64-linux-android/35",
        "usr/lib/x86_64-linux-android",
    ):
        assert not (sysroot / left_out).exists(), left_out
    assert not (sysroot / "usr" / "include" / "x86_64-linux-android").exists()
    assert "Android needed: API 26" in (android / "VERSIONS.txt").read_text()
    assert "Make the Android app" in (package / "README-android.txt").read_text(encoding="utf-8")


def test_the_zip_unpacks_into_the_computer_package_s_folder(tmp_path):
    package = assemble(made_up_inputs(tmp_path))
    archive = tmp_path / "android.zip"
    write_archive(package, archive)
    with zipfile.ZipFile(archive) as opened:
        names = opened.namelist()
    assert "builder/android/shell.apk" in names
    assert "README-android.txt" in names


def test_a_signed_shell_or_one_with_a_game_is_refused(tmp_path):
    inputs = made_up_inputs(tmp_path)
    with zipfile.ZipFile(inputs.shell, "a") as app:
        app.writestr("lib/arm64-v8a/libmain.so", b"game")
    with pytest.raises(PackageError, match="already has native libraries"):
        assemble(inputs)


def test_a_kit_without_the_sysroot_or_for_the_computer_is_refused(tmp_path):
    inputs = made_up_inputs(tmp_path)
    recipe = json.loads((inputs.kit / "recipe.json").read_text())
    (inputs.kit / "recipe.json").write_text(json.dumps({**recipe, "compile": ["-O2"]}))
    with pytest.raises(PackageError, match="sysroot"):
        assemble(inputs)
    (inputs.kit / "recipe.json").write_text(json.dumps({**recipe, "system": "Linux"}))
    with pytest.raises(PackageError, match="is for Linux, not Android"):
        assemble(inputs)


WORKFLOW = Path(__file__).resolve().parent.parent / ".github" / "workflows" / "package-native-android.yml"


def test_the_workflow_gives_the_tools_only_options_they_have():
    import re

    import yaml

    import package_native_android

    workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
    assert set(workflow[True]) == {"release", "workflow_dispatch"}
    assert workflow["permissions"] == {"contents": "read"}
    assert workflow["jobs"]["release"]["permissions"] == {"contents": "write"}
    text = WORKFLOW.read_text(encoding="utf-8")
    start = text.index("tools/package_native_android.py")
    given = set(re.findall(r"(--[a-z-]+)", text[start : text.index("cat ", start)]))
    assert given <= set(package_native_android.make_parser()._option_string_actions), given
    # The name has none of the words of a computer's system, which RecompLauncher picks its file by.
    assert "android.zip" in text and "linux-" not in text.split("PACKAGE_ZIP=")[1].split("\n")[0]
