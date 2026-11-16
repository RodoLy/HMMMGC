"""Tests of tools/package_native_macos.py with made-up inputs. They need no game, no build, no Mac and
no network.

Pruebas del armado del paquete nativo para macOS con entradas inventadas. No necesitan el juego, ni
una compilación, ni una Mac, ni la red.
"""

import dataclasses
import json
import plistlib
import re
import stat
import sys
import zipfile
from pathlib import Path

import pytest
import yaml

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import package_native_macos  # noqa: E402
from made_up_macho_binary import write_macho_binary  # noqa: E402
from package_native_macos import Inputs, PackageError, assemble, bundle_version, write_archive  # noqa: E402

SYSTEM = ["/usr/lib/libSystem.B.dylib"]


def touch(path: Path, content: bytes = b"x") -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    return path


def made_up_inputs(tmp_path: Path, architecture: str = "arm64") -> Inputs:
    processor = {"x64": "x86_64", "arm64": "arm64"}[architecture]
    kit = tmp_path / "kit"
    touch(kit / "lib" / "000_main.cpp.o")
    recipe = {
        "format": 1,
        "system": "Darwin",
        "processor": processor,
        "executable": "hmmm-native",
        "cxx_compiler": "clang++",
        "c_compiler": "clang",
        "link": ["{kit}/lib/000_main.cpp.o", "{game_objects}", "-o", "{output}", "-framework", "Metal"],
        "mods": ["player_speed"],
    }
    touch(kit / "recipe.json", json.dumps(recipe).encode())
    translator = tmp_path / "translator"
    write_macho_binary(translator / "Translator.Cli", architecture, SYSTEM)
    write_macho_binary(translator / "libcoreclr.dylib", architecture, SYSTEM)
    python = tmp_path / "python"
    write_macho_binary(
        python / "bin" / "python3.13",
        architecture,
        ["@executable_path/../lib/libpython3.13.dylib", *SYSTEM],
        (11, 0),
    )
    (python / "bin" / "python3").symlink_to("python3.13")
    write_macho_binary(python / "lib" / "libpython3.13.dylib", architecture, SYSTEM)
    touch(python / "lib" / "python3.13" / "os.py")
    touch(python / "lib" / "python3.13" / "test" / "test_os.py")
    nodtool = write_macho_binary(tmp_path / "nodtool-macos", architecture, SYSTEM, (12, 0))
    launcher = write_macho_binary(
        tmp_path / "HarvestMoonMM",
        architecture,
        ["/System/Library/Frameworks/Cocoa.framework/Cocoa", *SYSTEM],
        (12, 0),
    )
    icon = touch(tmp_path / "HarvestMoonMM.icns", b"icns")
    return Inputs(
        architecture=architecture,
        launcher=launcher,
        kit=kit,
        translator=translator,
        python=python,
        nodtool=nodtool,
        output_directory=tmp_path / "out",
        icon=icon,
        package_version="v0.4.0",
        versions={"HMMMGC": "v0.4.0"},
    )


def test_the_package_is_an_app_with_the_builder_in_its_resources_and_the_texts_beside_it(tmp_path):
    package = assemble(made_up_inputs(tmp_path))
    assert package.name == "HarvestMoonMM-macos-arm64"
    contents = package / "HarvestMoonMM.app" / "Contents"
    assert (contents / "MacOS" / "HarvestMoonMM").stat().st_mode & 0o111
    assert (contents / "Resources" / "HarvestMoonMM.icns").is_file()
    builder = contents / "Resources" / "builder"
    for path in (
        "tools/prepare_game.py",
        "config/G4AEE9/symbols.txt",
        "mods/player_speed/mod.c",
        "kit/recipe.json",
        "translator/Translator.Cli",
    ):
        assert (builder / path).is_file(), path
    assert (builder / "nodtool" / "nodtool").stat().st_mode & 0o111
    assert (builder / "python" / "bin" / "python3").readlink() == Path("python3.13")
    assert not (builder / "python" / "lib" / "python3.13" / "test").exists()
    assert not (builder / "clang").exists()  # a Mac compiles with Apple's clang
    assert "Command Line Tools" in (package / "README.txt").read_text(encoding="utf-8")
    profile = json.loads((package / "recomplauncher.json").read_text(encoding="utf-8"))
    assert "HarvestMoonMM.app/Contents/MacOS/HarvestMoonMM" in profile["start"]["program"]


def test_info_plist_names_the_program_the_version_and_the_oldest_macos(tmp_path):
    package = assemble(made_up_inputs(tmp_path))
    info = plistlib.loads((package / "HarvestMoonMM.app" / "Contents" / "Info.plist").read_bytes())
    assert info["CFBundleExecutable"] == "HarvestMoonMM"
    assert info["CFBundleShortVersionString"] == "0.4.0"
    assert info["LSMinimumSystemVersion"] == "12.0"
    assert info["CFBundleIconFile"] == "HarvestMoonMM"
    assert "macOS needed: 12.0 or newer" in (package / "VERSIONS.txt").read_text(encoding="utf-8")


def test_a_version_that_is_not_a_release_is_0_0_0_for_macos():
    assert bundle_version("v1.2.3") == "1.2.3"
    assert bundle_version("main-1a2b3c4") == "0.0.0"


def test_a_program_for_the_other_processor_stops_the_package(tmp_path):
    inputs = made_up_inputs(tmp_path)
    write_macho_binary(inputs.nodtool, "x64", SYSTEM)
    with pytest.raises(PackageError, match="nodtool is for x64"):
        assemble(inputs)


def test_a_library_from_homebrew_stops_the_package(tmp_path):
    inputs = made_up_inputs(tmp_path)
    write_macho_binary(inputs.launcher, "arm64", ["/opt/homebrew/lib/libSDL3.0.dylib"])
    with pytest.raises(PackageError, match="not part of macOS"):
        assemble(inputs)


def test_a_kit_for_another_system_or_processor_is_refused(tmp_path):
    inputs = made_up_inputs(tmp_path)
    with pytest.raises(PackageError, match="the kit is for arm64, and the package for x64"):
        assemble(dataclasses.replace(inputs, architecture="x64"))
    recipe = json.loads((inputs.kit / "recipe.json").read_text())
    (inputs.kit / "recipe.json").write_text(json.dumps({**recipe, "system": "Linux"}))
    with pytest.raises(PackageError, match="is for Linux, not Darwin"):
        assemble(inputs)


def test_the_zip_keeps_the_links_and_the_programs_in_one_folder(tmp_path):
    package = assemble(made_up_inputs(tmp_path))
    archive = tmp_path / "package.zip"
    write_archive(package, archive)
    with zipfile.ZipFile(archive) as opened:
        entries = {entry.filename: entry for entry in opened.infolist()}
        python3 = "HarvestMoonMM-macos-arm64/HarvestMoonMM.app/Contents/Resources/builder/python/bin/python3"
        assert stat.S_ISLNK(entries[python3].external_attr >> 16)
        assert opened.read(python3) == b"python3.13"
    assert {name.split("/")[0] for name in entries} == {"HarvestMoonMM-macos-arm64"}
    launcher = entries["HarvestMoonMM-macos-arm64/HarvestMoonMM.app/Contents/MacOS/HarvestMoonMM"]
    assert (launcher.external_attr >> 16) & 0o111


WORKFLOW = Path(__file__).resolve().parent.parent / ".github" / "workflows" / "package-native-macos.yml"


def test_the_workflow_builds_both_processors_and_only_the_release_job_can_write():
    workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
    assert set(workflow[True]) == {"release", "workflow_dispatch"}
    assert workflow["permissions"] == {"contents": "read"}
    assert workflow["jobs"]["release"]["permissions"] == {"contents": "write"}
    matrix = workflow["jobs"]["package"]["strategy"]["matrix"]["include"]
    assert {entry["architecture"] for entry in matrix} == set(package_native_macos.ARCHITECTURES)


def test_the_workflow_gives_the_assembler_only_options_it_has_and_reads_the_folder_it_writes():
    text = WORKFLOW.read_text(encoding="utf-8")
    start = text.index("tools/package_native_macos.py")
    given = set(re.findall(r"(--[a-z-]+)", text[start : text.index("cat ", start)]))
    known = set(package_native_macos.make_parser()._option_string_actions)
    assert given <= known, given - known
    assert "build/dist/HarvestMoonMM-macos-$ARCHITECTURE/VERSIONS.txt" in text
    assert "macos-$ARCHITECTURE.zip" in text  # RecompLauncher picks the file by "macos" and the processor
    # The icon it is given is the one tools/make_launcher_icon.py writes.
    assert (WORKFLOW.parent.parent.parent / "launcher" / "macos" / "HarvestMoonMM.icns").is_file()
