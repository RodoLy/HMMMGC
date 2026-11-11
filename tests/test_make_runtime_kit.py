"""Tests of tools/make_runtime_kit.py with made-up builds. None needs the game or a real build.

Pruebas de tools/make_runtime_kit.py con compilaciones inventadas. Ninguna necesita el juego
ni una compilación de verdad.
"""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from made_up_windows_binary import write_windows_binary  # noqa: E402
from make_runtime_kit import (  # noqa: E402
    STUB_GAME_DIRECTORY,
    Kit,
    KitError,
    PathRewriter,
    compile_flags,
    link_arguments,
    runtime_dlls,
    target_system,
)


@pytest.fixture
def kit(tmp_path):
    build = tmp_path / "build"
    out = tmp_path / "kit"
    (out / "lib").mkdir(parents=True)
    build.mkdir()
    return Kit(out, build)


@pytest.fixture
def wiicompiled_include(tmp_path):
    include = tmp_path / "wiicompiled" / "runtime" / "include"
    (include / "guest_abi").mkdir(parents=True)
    return include


def test_the_game_is_compiled_with_its_includes_as_placeholders_and_without_debugging_information(
    kit, wiicompiled_include, tmp_path
):
    shims = tmp_path / "case-shims"
    rewriter = PathRewriter(kit, wiicompiled_include, [shims])
    stub = STUB_GAME_DIRECTORY
    command = [
        "/opt/llvm-mingw/bin/aarch64-w64-mingw32-clang++",
        f"-I{wiicompiled_include}",
        f"-I{stub}",
        "-I",
        str(stub / "build_shards" / "base_common"),
        "-idirafter",
        str(shims),
        "-O2",
        "-g",
        "-DNDEBUG",
        "-std=gnu++20",
        "-MD",
        "-MT",
        "x.o",
        "-MF",
        "x.o.d",
        "-o",
        "CMakeFiles/translated_game.dir/data_sections_init.cpp.obj",
        "-c",
        str(stub / "data_sections_init.cpp"),
    ]
    assert compile_flags(command, rewriter) == [
        "-I{kit}/include",
        "-I{translation}",
        "-I",
        "{translation}/build_shards/base_common",
        "-O2",
        "-DNDEBUG",
        "-std=gnu++20",
    ]


def test_an_include_directory_of_this_computer_that_the_kit_does_not_carry_stops_it(kit, wiicompiled_include):
    rewriter = PathRewriter(kit, wiicompiled_include, [])
    with pytest.raises(KitError, match="cannot carry"):
        compile_flags(["clang++", "-I/home/someone/headers", "-c", "a.cpp"], rewriter)


def make_files(build: Path, *names: str) -> None:
    for name in names:
        path = build / name
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(name.encode())


def test_the_link_names_the_kit_s_copies_and_leaves_the_game_and_the_program_to_be_filled_in(kit, tmp_path):
    make_files(
        kit.build_directory,
        "CMakeFiles/hmmm-native.dir/src/main.cpp.obj",
        "aurora/libaurora_core.a",
        "_deps/sdl-build/libSDL3.a",
    )
    dawn = tmp_path / "dawn" / "lib" / "webgpu_dawn.lib"
    dawn.parent.mkdir(parents=True)
    dawn.write_bytes(b"import library")
    command = [
        "/opt/llvm-mingw/bin/aarch64-w64-mingw32-clang++",
        "-O3",
        "-static",
        "CMakeFiles/hmmm-native.dir/src/main.cpp.obj",
        "CMakeFiles/translated_game.dir/build_shards/base_common/a.cpp.obj",
        "CMakeFiles/translated_game.dir/data_sections_init.cpp.obj",
        "-o",
        "hmmm-native.exe",
        "-Wl,--out-implib,libhmmm-native.dll.a",
        "aurora/libaurora_core.a",
        "_deps/sdl-build/libSDL3.a",
        "aurora/libaurora_core.a",
        str(dawn),
        "-lonecore",
        "-ldbghelp",
    ]
    arguments = link_arguments(command, kit)
    assert arguments == [
        "-O3",
        "-static",
        "{kit}/lib/000_main.cpp.obj",
        "{game_objects}",
        "-o",
        "{output}",
        "{kit}/lib/001_libaurora_core.a",
        "{kit}/lib/002_libSDL3.a",
        "{kit}/lib/001_libaurora_core.a",  # named twice, copied once
        "{kit}/lib/003_webgpu_dawn.lib",
        "-lonecore",
        "-ldbghelp",
    ]
    assert (kit.out / "lib" / "003_webgpu_dawn.lib").read_bytes() == b"import library"
    assert len(list((kit.out / "lib").iterdir())) == 4


def test_a_link_without_the_stand_in_game_is_refused(kit):
    make_files(kit.build_directory, "a.o")
    with pytest.raises(KitError, match="stand-in game"):
        link_arguments(["clang++", "a.o", "-o", "hmmm-native"], kit)


def test_a_library_path_of_this_computer_is_refused(kit):
    with pytest.raises(KitError, match="cannot carry"):
        link_arguments(["clang++", "-L/home/someone/lib", "-o", "hmmm-native"], kit)


def test_the_target_comes_from_cmake_s_system_file(tmp_path):
    system_file = tmp_path / "CMakeFiles" / "3.28.3" / "CMakeSystem.cmake"
    system_file.parent.mkdir(parents=True)
    system_file.write_text('set(CMAKE_SYSTEM_NAME "Windows")\nset(CMAKE_SYSTEM_PROCESSOR "ARM64")\n')
    assert target_system(tmp_path) == ("Windows", "ARM64")


def test_the_compile_flags_that_cmake_also_gives_the_link_are_left_out(kit, tmp_path):
    make_files(kit.build_directory, "a.o")
    command = [
        "clang++",
        "-idirafter",
        str(tmp_path / "case-shims"),
        f"-I{tmp_path / 'headers'}",
        "a.o",
        "CMakeFiles/translated_game.dir/data_sections_init.cpp.obj",
        "-o",
        "hmmm-native.exe",
    ]
    assert link_arguments(command, kit) == ["{kit}/lib/000_a.o", "{game_objects}", "-o", "{output}"]


def test_the_game_gets_beside_it_every_dll_it_needs_and_only_those(tmp_path):
    build = tmp_path / "build"
    program = write_windows_binary(
        build / "hmmm-native.exe", "arm64", ["KERNEL32.dll", "webgpu_dawn.dll", "libpng16.dll"]
    )
    write_windows_binary(build / "_deps" / "png-build" / "libpng16.dll", "arm64", ["libzlib1.dll"])
    write_windows_binary(
        build / "_deps" / "zlib-build" / "libzlib1.dll", "arm64", ["api-ms-win-crt-heap-l1-1-0.dll"]
    )
    write_windows_binary(build / "_deps" / "other-build" / "unused.dll", "arm64")
    dawn = tmp_path / "dawn" / "bin"
    write_windows_binary(dawn / "webgpu_dawn.dll", "arm64", ["MSVCP140.dll", "USER32.dll"])
    write_windows_binary(dawn / "dxcompiler.dll", "arm64", ["MSVCP140.dll", "VCRUNTIME140.dll"])
    write_windows_binary(dawn / "dxil.dll", "arm64")  # loaded by name: imported by nothing
    visual_cpp = tmp_path / "visual-cpp-runtime"
    write_windows_binary(visual_cpp / "msvcp140.dll", "arm64", ["VCRUNTIME140.dll"])
    write_windows_binary(visual_cpp / "vcruntime140.dll", "arm64")
    write_windows_binary(visual_cpp / "concrt140.dll", "arm64")
    write_windows_binary(visual_cpp / "vcruntime140_1.dll", "x64")  # ARM64EC, for x64 programs only
    dlls = runtime_dlls(program, build, [dawn], [visual_cpp])
    assert sorted(dlls) == [
        "dxcompiler.dll",
        "dxil.dll",
        "libpng16.dll",
        "libzlib1.dll",
        "msvcp140.dll",
        "vcruntime140.dll",
        "webgpu_dawn.dll",
    ]


def test_a_dll_the_game_needs_and_nobody_gives_stops_the_kit(tmp_path):
    build = tmp_path / "build"
    program = write_windows_binary(build / "hmmm-native.exe", "x64", ["webgpu_dawn.dll"])
    dawn = tmp_path / "dawn"
    write_windows_binary(dawn / "webgpu_dawn.dll", "x64", ["VCRUNTIME140_1.dll"])
    with pytest.raises(KitError, match="would not start.*VCRUNTIME140_1.dll"):
        runtime_dlls(program, build, [dawn], [])


def test_two_different_dlls_of_the_same_name_in_the_build_stop_the_kit_when_the_game_uses_it(tmp_path):
    build = tmp_path / "build"
    program = write_windows_binary(build / "hmmm-native.exe", "x64", ["libpng16.dll"])
    write_windows_binary(build / "a" / "libpng16.dll", "x64")
    write_windows_binary(build / "b" / "libpng16.dll", "x64", ["KERNEL32.dll"])
    # Two of a DLL that nothing imports do not matter.
    write_windows_binary(build / "a" / "unused.dll", "x64")
    write_windows_binary(build / "b" / "unused.dll", "x64", ["KERNEL32.dll"])
    with pytest.raises(KitError, match="different libpng16.dll"):
        runtime_dlls(program, build, [], [])
    (build / "b" / "libpng16.dll").unlink()
    assert sorted(runtime_dlls(program, build, [], [])) == ["libpng16.dll"]
