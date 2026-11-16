"""Tests of tools/elf_binaries.py with made-up Linux programs (tests/made_up_elf_binary.py).

Pruebas de tools/elf_binaries.py con programas de Linux inventados.
"""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from elf_binaries import ElfError, check_programs, read_elf_binary  # noqa: E402
from made_up_elf_binary import write_elf_binary  # noqa: E402


def test_it_reads_the_processor_the_libraries_the_runpath_and_the_newest_glibc(tmp_path):
    program = write_elf_binary(
        tmp_path / "game",
        "arm64",
        ["libLLVM.so.23.1", "libc.so.6"],
        "$ORIGIN/../lib",
        {"libc.so.6": ["GLIBC_2.17", "GLIBC_2.38", "GLIBC_2.2.5"], "libstdc++.so.6": ["GLIBCXX_3.4.32"]},
    )
    binary = read_elf_binary(program)
    assert binary.machine == "arm64"
    assert binary.needed == ["libLLVM.so.23.1", "libc.so.6"]
    assert binary.runpath == ["$ORIGIN/../lib"]
    assert binary.glibc == (2, 38)


def test_a_program_finds_its_libraries_beside_it_and_the_check_says_the_newest_glibc(tmp_path):
    write_elf_binary(
        tmp_path / "lib" / "libLLVM.so.23.1", "x64", ["libz.so.1"], "", {"libc.so.6": ["GLIBC_2.34"]}
    )
    program = write_elf_binary(
        tmp_path / "bin" / "clang",
        "x64",
        ["libLLVM.so.23.1", "libc.so.6"],
        "$ORIGIN/../lib",
        {"libc.so.6": ["GLIBC_2.17"]},
    )
    assert check_programs([program], "x64") == (2, 34)


def test_a_library_that_is_neither_beside_it_nor_part_of_linux_stops_the_check(tmp_path):
    program = write_elf_binary(tmp_path / "HarvestMoonMM", "x64", ["libSDL3.so.0", "libc.so.6"])
    with pytest.raises(ElfError, match="needs libSDL3.so.0"):
        check_programs([program], "x64")


def test_a_program_for_the_other_processor_stops_the_check(tmp_path):
    program = write_elf_binary(tmp_path / "nodtool", "arm64")
    with pytest.raises(ElfError, match="is for arm64, and the package for x64"):
        check_programs([program], "x64")


def test_a_library_beside_it_is_checked_too(tmp_path):
    write_elf_binary(tmp_path / "lib" / "libLLVM.so.23.1", "arm64")
    program = write_elf_binary(tmp_path / "bin" / "lld", "x64", ["libLLVM.so.23.1"], "$ORIGIN/../lib")
    with pytest.raises(ElfError, match="libLLVM.so.23.1 is for arm64"):
        check_programs([program], "x64")


def test_a_file_that_is_not_a_linux_program_is_refused(tmp_path):
    text = tmp_path / "python3"
    text.write_text("#!/bin/sh\n")
    with pytest.raises(ElfError, match="not a Linux program"):
        read_elf_binary(text)
