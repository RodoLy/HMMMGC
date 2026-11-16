"""Tests of tools/macho_binaries.py with made-up macOS programs (tests/made_up_macho_binary.py).

Pruebas de tools/macho_binaries.py con programas de macOS inventados.
"""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from macho_binaries import MachOError, check_programs, read_macho_binary  # noqa: E402
from made_up_macho_binary import macho, write_macho_binary, write_universal_binary  # noqa: E402


def test_it_reads_the_processor_the_oldest_macos_and_the_libraries(tmp_path):
    program = write_macho_binary(
        tmp_path / "nodtool", "arm64", ["/usr/lib/libSystem.B.dylib", "@rpath/libcoreclr.dylib"], (13, 3)
    )
    binary = read_macho_binary(program, "arm64")
    assert binary.processor == "arm64"
    assert binary.minimum_macos == (13, 3, 0)
    assert binary.libraries == ["/usr/lib/libSystem.B.dylib", "@rpath/libcoreclr.dylib"]


def test_a_universal_file_gives_the_slice_of_the_processor(tmp_path):
    program = write_universal_binary(
        tmp_path / "python3",
        [macho("x64", minimum_macos=(10, 13)), macho("arm64", minimum_macos=(11, 0))],
        ["x64", "arm64"],
    )
    assert read_macho_binary(program, "arm64").minimum_macos == (11, 0, 0)
    assert read_macho_binary(program, "x64").minimum_macos == (10, 13, 0)


def test_a_program_for_the_other_processor_is_refused(tmp_path):
    with pytest.raises(MachOError, match="is for x64, and the package for arm64"):
        read_macho_binary(write_macho_binary(tmp_path / "HarvestMoonMM", "x64"), "arm64")
    universal = write_universal_binary(tmp_path / "tool", [macho("x64")], ["x64"])
    with pytest.raises(MachOError, match="is for x64, and the package for arm64"):
        read_macho_binary(universal, "arm64")


def test_the_check_says_the_newest_oldest_macos_and_finds_the_package_s_own_libraries(tmp_path):
    write_macho_binary(tmp_path / "python" / "lib" / "libpython3.13.dylib", "arm64")
    python = write_macho_binary(
        tmp_path / "python" / "bin" / "python3",
        "arm64",
        ["@executable_path/../lib/libpython3.13.dylib", "/usr/lib/libSystem.B.dylib"],
        (11, 0),
    )
    launcher = write_macho_binary(
        tmp_path / "MacOS" / "HarvestMoonMM",
        "arm64",
        ["/System/Library/Frameworks/Metal.framework/Versions/A/Metal"],
        (12, 0),
    )
    assert check_programs([python, launcher], "arm64") == (12, 0, 0)


def test_a_library_that_is_neither_of_macos_nor_in_the_package_stops_the_check(tmp_path):
    program = write_macho_binary(
        tmp_path / "MacOS" / "HarvestMoonMM", "arm64", ["/opt/homebrew/lib/libSDL3.0.dylib"]
    )
    with pytest.raises(MachOError, match="not part of macOS"):
        check_programs([program], "arm64")
    program = write_macho_binary(
        tmp_path / "bin" / "python3", "arm64", ["@executable_path/../lib/libpython3.13.dylib"]
    )
    with pytest.raises(MachOError, match="the package does not have"):
        check_programs([program], "arm64")


def test_a_file_that_is_not_a_macos_program_is_refused(tmp_path):
    text = tmp_path / "python3"
    text.write_text("#!/bin/sh\n")
    with pytest.raises(MachOError, match="not a 64-bit macOS program"):
        read_macho_binary(text, "arm64")
