"""Tests of tools/package_native_unix.py with made-up inputs. They need no game, no build and no
network.

Pruebas del armado del paquete nativo para Linux con entradas inventadas. No necesitan el juego, ni
una compilación, ni la red.
"""

import dataclasses
import json
import re
import sys
import tarfile
from pathlib import Path

import pytest
import yaml

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import package_native_unix  # noqa: E402
from made_up_elf_binary import write_elf_binary  # noqa: E402
from package_native_unix import Inputs, PackageError, assemble, write_archive  # noqa: E402

REPOSITORY = Path(__file__).resolve().parent.parent


def touch(path: Path, content: bytes = b"x") -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    return path


def made_up_inputs(tmp_path: Path, architecture: str = "x64") -> Inputs:
    processor = {"x64": "x86_64", "arm64": "aarch64"}[architecture]
    glibc = {"libc.so.6": ["GLIBC_2.17", "GLIBC_2.38"]}
    kit = tmp_path / "kit"
    touch(kit / "lib" / "000_main.cpp.o")
    recipe = {
        "format": 1,
        "system": "Linux",
        "processor": processor,
        "executable": "hmmm-native",
        "cxx_compiler": "clang++",
        "c_compiler": "clang",
        "link": ["{kit}/lib/000_main.cpp.o", "{game_objects}", "-o", "{output}", "-lm"],
        "mods": ["player_speed"],
    }
    touch(kit / "recipe.json", json.dumps(recipe).encode())
    translator = tmp_path / "translator"
    write_elf_binary(translator / "Translator.Cli", architecture, ["libstdc++.so.6", "libc.so.6"])
    touch(translator / "Translator.Cli.dll")
    python = tmp_path / "python"
    write_elf_binary(python / "bin" / "python3.13", architecture, ["libc.so.6"], versions=glibc)
    (python / "bin" / "python3").symlink_to("python3.13")
    touch(python / "lib" / "python3.13" / "os.py")
    touch(python / "lib" / "python3.13" / "test" / "test_os.py")
    touch(python / "include" / "python3.13" / "Python.h")
    llvm_mingw = tmp_path / "llvm-mingw"
    write_elf_binary(
        llvm_mingw / "bin" / "clang-23", architecture, ["libLLVM.so.23.1", "libc.so.6"], "$ORIGIN/../lib"
    )
    (llvm_mingw / "bin" / "clang").symlink_to("clang-23")
    (llvm_mingw / "bin" / "clang++").symlink_to("clang")
    write_elf_binary(llvm_mingw / "bin" / "lld", architecture, ["libLLVM.so.23.1"], "$ORIGIN/../lib")
    (llvm_mingw / "bin" / "ld.lld").symlink_to("lld")
    write_elf_binary(llvm_mingw / "bin" / "lldb", architecture, ["liblldb.so.23.1"])
    touch(llvm_mingw / "bin" / "x86_64-w64-mingw32-clang", b"#!/bin/sh\n")
    write_elf_binary(llvm_mingw / "lib" / "libLLVM.so.23.1", architecture, ["libz.so.1"], versions=glibc)
    touch(llvm_mingw / "lib" / "clang" / "23" / "include" / "stddef.h")
    touch(llvm_mingw / "lib" / "clang" / "23" / "lib" / "windows" / "libclang_rt.builtins-x86_64.a")
    touch(llvm_mingw / "x86_64-w64-mingw32" / "include" / "windows.h")
    touch(llvm_mingw / "LICENSE.TXT")
    sysroot = tmp_path / "sysroot"
    touch(sysroot / "usr" / "include" / "stdio.h")
    touch(sysroot / "PACKAGES.txt", b"libc6 2.39\n")
    (sysroot / "lib").symlink_to("usr/lib")
    nodtool = write_elf_binary(tmp_path / "nodtool-linux", architecture)
    launcher = write_elf_binary(
        tmp_path / "HarvestMoonMM", architecture, ["libm.so.6", "libc.so.6"], versions=glibc
    )
    return Inputs(
        architecture=architecture,
        launcher=launcher,
        kit=kit,
        translator=translator,
        python=python,
        llvm_mingw=llvm_mingw,
        sysroot=sysroot,
        nodtool=nodtool,
        output_directory=tmp_path / "out",
        package_version="v0.4.0",
        versions={"HMMMGC": "v0.4.0"},
    )


def test_the_package_has_the_launcher_the_builder_and_the_linux_texts(tmp_path):
    package = assemble(made_up_inputs(tmp_path))
    assert package.name == "HarvestMoonMM-linux-x64"
    assert (package / "HarvestMoonMM").stat().st_mode & 0o111
    assert (package / "builder" / "nodtool" / "nodtool").stat().st_mode & 0o111
    for path in (
        "builder/tools/prepare_game.py",
        "builder/config/G4AEE9/symbols.txt",
        "builder/mods/player_speed/mod.c",
        "builder/kit/recipe.json",
        "builder/translator/Translator.Cli",
        "LICENSE.txt",
    ):
        assert (package / path).is_file(), path
    assert "Linux" in (package / "README.txt").read_text(encoding="utf-8")
    assert "sysroot" in (package / "THIRD_PARTY_NOTICES.txt").read_text(encoding="utf-8")
    profile = json.loads((package / "recomplauncher.json").read_text(encoding="utf-8"))
    assert "HarvestMoonMM" in profile["start"]["program"]
    assert profile["prepare"]["version"] == "v0.4.0"


def test_the_compiler_is_trimmed_and_points_at_the_sysroot_and_lld(tmp_path):
    clang = assemble(made_up_inputs(tmp_path)) / "builder" / "clang"
    assert (clang / "bin" / "clang++").readlink() == Path("clang")
    assert (clang / "bin" / "ld.lld").is_symlink()
    assert (clang / "lib" / "libLLVM.so.23.1").is_file()
    assert (clang / "lib" / "clang" / "23" / "include" / "stddef.h").is_file()
    for left_out in ("bin/lldb", "bin/x86_64-w64-mingw32-clang", "x86_64-w64-mingw32", "lib/clang/23/lib"):
        assert not (clang / left_out).exists(), left_out
    for name in ("clang.cfg", "clang++.cfg"):
        configuration = (clang / "bin" / name).read_text(encoding="utf-8")
        assert "--sysroot=<CFGDIR>/../sysroot" in configuration
        assert "-fuse-ld=lld" in configuration
    assert (clang / "sysroot" / "usr" / "include" / "stdio.h").is_file()
    assert (clang / "sysroot" / "lib").readlink() == Path("usr/lib")


def test_python_keeps_the_interpreter_and_the_library_without_the_tests_and_the_headers(tmp_path):
    python = assemble(made_up_inputs(tmp_path)) / "builder" / "python"
    assert (python / "bin" / "python3").readlink() == Path("python3.13")
    assert (python / "lib" / "python3.13" / "os.py").is_file()
    assert not (python / "lib" / "python3.13" / "test").exists()
    assert not (python / "include").exists()


def test_versions_says_the_newest_glibc_the_programs_need_and_the_sysroot_s_packages(tmp_path):
    versions = (assemble(made_up_inputs(tmp_path)) / "VERSIONS.txt").read_text(encoding="utf-8")
    assert "glibc needed: 2.38 or newer" in versions
    assert "libc6 2.39" in versions


def test_a_program_that_needs_a_library_it_does_not_bring_stops_the_package(tmp_path):
    inputs = made_up_inputs(tmp_path)
    write_elf_binary(inputs.launcher, "x64", ["libSDL3.so.0", "libc.so.6"])
    with pytest.raises(PackageError, match="needs libSDL3.so.0"):
        assemble(inputs)


def test_a_compiler_for_the_other_processor_stops_the_package(tmp_path):
    inputs = made_up_inputs(tmp_path)
    write_elf_binary(inputs.llvm_mingw / "bin" / "clang-23", "arm64", [], "$ORIGIN/../lib")
    with pytest.raises(PackageError, match="clang-23 is for arm64"):
        assemble(inputs)


def test_a_kit_for_windows_or_for_the_other_processor_is_refused(tmp_path):
    inputs = made_up_inputs(tmp_path)
    with pytest.raises(PackageError, match="the kit is for x86_64, and the package for arm64"):
        assemble(dataclasses.replace(inputs, architecture="arm64"))
    recipe = json.loads((inputs.kit / "recipe.json").read_text())
    (inputs.kit / "recipe.json").write_text(json.dumps({**recipe, "system": "Windows"}))
    with pytest.raises(PackageError, match="is for Windows, not Linux"):
        assemble(inputs)


def test_game_data_in_an_input_stops_the_package(tmp_path):
    inputs = made_up_inputs(tmp_path)
    touch(inputs.kit / "lib" / "main.dol")
    with pytest.raises(PackageError, match="looks like game data"):
        assemble(inputs)


def test_the_archive_keeps_the_links_and_the_programs_and_has_one_folder(tmp_path):
    package = assemble(made_up_inputs(tmp_path))
    archive = tmp_path / "package.tar.gz"
    write_archive(package, archive)
    with tarfile.open(archive) as opened:
        members = {member.name: member for member in opened.getmembers()}
    assert {name.split("/")[0] for name in members} == {"HarvestMoonMM-linux-x64"}
    assert members["HarvestMoonMM-linux-x64/builder/clang/bin/clang++"].issym()
    assert members["HarvestMoonMM-linux-x64/HarvestMoonMM"].mode & 0o111
    assert all(member.uid == 0 and member.uname == "" for member in members.values())


WORKFLOW = REPOSITORY / ".github" / "workflows" / "package-native-linux.yml"


def test_the_workflow_builds_both_processors_and_only_the_release_job_can_write():
    workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
    assert set(workflow[True]) == {"release", "workflow_dispatch"}
    assert workflow["permissions"] == {"contents": "read"}
    assert workflow["jobs"]["release"]["permissions"] == {"contents": "write"}
    matrix = workflow["jobs"]["package"]["strategy"]["matrix"]["include"]
    assert {entry["architecture"] for entry in matrix} == set(package_native_unix.ARCHITECTURES)


def test_the_workflow_gives_the_assembler_only_options_it_has_and_reads_the_folder_it_writes():
    text = WORKFLOW.read_text(encoding="utf-8")
    assembler = text[
        text.index("tools/package_native_unix.py") : text.index(
            "cat ", text.index("tools/package_native_unix.py")
        )
    ]
    given = set(re.findall(r"(--[a-z-]+)", assembler))
    known = set(package_native_unix.make_parser()._option_string_actions)
    assert given <= known, given - known
    assert "build/dist/HarvestMoonMM-linux-$ARCHITECTURE/VERSIONS.txt" in text
    assert "linux-$ARCHITECTURE.tar.gz" in text  # RecompLauncher picks the file by "linux" and the processor
