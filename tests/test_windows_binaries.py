"""Tests of tools/windows_binaries.py with made-up Windows programs and DLLs.

Pruebas de tools/windows_binaries.py con programas y DLL de Windows inventados.
"""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from made_up_windows_binary import write_windows_binary  # noqa: E402
from windows_binaries import (  # noqa: E402
    WindowsBinaryError,
    dlls_in,
    is_part_of_windows,
    needed_dlls,
    read_windows_binary,
)


@pytest.mark.parametrize("machine", ["x64", "arm64"])
def test_the_headers_say_the_processor_and_the_dlls_imported(tmp_path, machine):
    program = write_windows_binary(tmp_path / "game.exe", machine, ["KERNEL32.dll", "webgpu_dawn.dll"])
    binary = read_windows_binary(program)
    assert binary.machine == machine
    assert binary.imports == ("KERNEL32.dll", "webgpu_dawn.dll")


def test_a_file_that_is_not_a_windows_program_is_refused(tmp_path):
    text = tmp_path / "notes.dll"
    text.write_bytes(b"not a program, just some text that is long enough for the headers" * 2)
    with pytest.raises(WindowsBinaryError, match="not a Windows program"):
        read_windows_binary(text)
    cut = tmp_path / "cut.dll"
    cut.write_bytes(write_windows_binary(tmp_path / "whole.dll", "x64", ["a.dll"]).read_bytes()[:100])
    with pytest.raises(WindowsBinaryError, match="not a whole"):
        read_windows_binary(cut)


def test_the_parts_of_windows_are_told_apart_from_what_has_to_go_beside_the_program():
    for dll in (
        "KERNEL32.dll",
        "user32.dll",
        "dxgi.dll",
        "api-ms-win-crt-runtime-l1-1-0.dll",
        "winspool.drv",
    ):
        assert is_part_of_windows(dll), dll
    for dll in ("MSVCP140.dll", "VCRUNTIME140.dll", "libpng16.dll", "webgpu_dawn.dll", "python313.dll"):
        assert not is_part_of_windows(dll), dll


def test_the_dlls_needed_are_those_imported_and_those_they_import(tmp_path):
    program = write_windows_binary(tmp_path / "game.exe", "arm64", ["KERNEL32.dll", "libpng16.dll"])
    folder = tmp_path / "dlls"
    write_windows_binary(folder / "libpng16.dll", "arm64", ["libzlib1.dll", "api-ms-win-crt-heap-l1-1-0.dll"])
    write_windows_binary(folder / "libzlib1.dll", "arm64", ["KERNEL32.dll"])
    write_windows_binary(folder / "unused.dll", "arm64")
    # The x64 one of the Visual C++ runtime for ARM64: nothing of the ARM64 program uses it.
    write_windows_binary(folder / "vcruntime140_1.dll", "x64")
    assert set(needed_dlls([program], dlls_in(folder))) == {"libpng16.dll", "libzlib1.dll"}


def test_a_dll_that_is_nowhere_is_named_with_the_one_that_needs_it(tmp_path):
    program = write_windows_binary(tmp_path / "game.exe", "x64", ["webgpu_dawn.dll"])
    write_windows_binary(tmp_path / "webgpu_dawn.dll", "x64", ["MSVCP140.dll"])
    with pytest.raises(WindowsBinaryError, match="webgpu_dawn.dll needs MSVCP140.dll"):
        needed_dlls([program], dlls_in(tmp_path))


def test_a_dll_for_another_processor_is_refused(tmp_path):
    program = write_windows_binary(tmp_path / "game.exe", "arm64", ["webgpu_dawn.dll"])
    write_windows_binary(tmp_path / "webgpu_dawn.dll", "x64")
    with pytest.raises(WindowsBinaryError, match="webgpu_dawn.dll is for x64"):
        needed_dlls([program], dlls_in(tmp_path))
