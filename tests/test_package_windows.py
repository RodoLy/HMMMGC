"""Tests of the Windows package assembler. No Windows, no compiler and no game needed.

Pruebas del armador del paquete para Windows. Sin Windows, sin compilador y sin el juego.

The inputs are made-up folders that have the same shape as the real ones (the build folder of
ModernGekko, its source tree, the unpacked tools), with files that only hold their own name. What
is checked is what the assembler decides: what goes where, what is left out, what is converted
for Windows, and that game data never gets in.
"""

import shutil
import sys
import zipfile
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import fetch_windows_toolchain  # noqa: E402
import package_windows as package  # noqa: E402

LAUNCHER_NAME = "HarvestMoonMM"


def write_files(root: Path, relative_paths: list[str]) -> Path:
    """Make `root` hold one small file for each path, each containing its own path."""
    for relative_path in relative_paths:
        path = root / relative_path
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_text(relative_path, encoding="utf-8")
    return root


@pytest.fixture
def inputs(tmp_path) -> package.PackageInputs:
    """Everything the assembler reads, made up, with the shape of the real thing."""
    build = write_files(
        tmp_path / "build",
        [
            f"{LAUNCHER_NAME}.exe",
            "moderngekko-port.exe",
            "moderngekko-run.exe",
            "dolrecomp.exe",
            "SDL3.dll",
            "Sys/GameSettings/G4AEE9.ini",
            "Sys/Shaders/default.glsl",
            "moderngekko-port.pdb",  # debug information, not part of a package
        ],
    )
    source = write_files(
        tmp_path / "source",
        [
            "vendor/dolphin/module-template/CMakeLists.txt",
            "vendor/dolphin/module-template/gen_module_tables.py",
            "vendor/dolphin/GXRuntime/include/gxruntime/cpu.h",
            "vendor/dolphin/GXRuntime/src/core/cpu.c",
            "vendor/dolphin/GXRuntime/src/core/cpu.h",
            "vendor/dolphin/GXRuntime/src/hle/not_needed.c",
            "vendor/dolphin/GXRuntime/tests/not_needed_either.c",
            "vendor/dolphin/Source/Core/Core/PowerPC/StaticRecomp/StaticRecompABI.h",
            "vendor/dolphin/Source/Core/Core/PowerPC/StaticRecomp/StaticRecompCore.cpp",
            "vendor/dolphin/Data/Sys/GameSettings/G4AEE9.ini",
            "vendor/dolphin/Data/Sys/GameSettings/GALE01.ini",
            "LICENSE",
            "vendor/dolphin/COPYING",
            "vendor/dolphin/Externals/licenses.md",
        ],
    )
    llvm_mingw = write_files(
        tmp_path / "llvm-mingw",
        [
            "bin/clang.exe",
            "bin/lld.exe",
            "bin/lldb.exe",
            "bin/clangd.exe",
            "aarch64-w64-mingw32/lib/libmsvcrt.a",
            "i686-w64-mingw32/lib/libmsvcrt.a",
            "x86_64-w64-mingw32/lib/libmsvcrt.a",
            "armv7-w64-mingw32/lib/libmsvcrt.a",
            "lib/clang/21/include/stddef.h",
            "lib/clang/21/lib/windows/libclang_rt.builtins-aarch64.a",
            "lib/clang/21/lib/windows/libclang_rt.builtins-x86_64.a",
            "lib/clang/21/lib/windows/libclang_rt.builtins-i386.a",
            "lib/clang/21/lib/windows/libclang_rt.builtins-arm.a",
            "lib/clang/21/lib/linux/libclang_rt.asan-aarch64.a",
            "bin/aarch64-w64-mingw32-widl.exe",
            "bin/libpython3.14.dll",
            "busybox/bin/sh.exe",
        ],
    )
    cmake = write_files(
        tmp_path / "cmake",
        [
            "bin/cmake.exe",
            "bin/cmake-gui.exe",
            "bin/ctest.exe",
            "share/cmake-4.4/Modules/Platform.cmake",
            "doc/cmake/LICENSE.rst",
            "doc/cmake/html/index.html",
        ],
    )
    ninja = write_files(tmp_path / "ninja", ["ninja.exe"])
    python = write_files(
        tmp_path / "python",
        [
            "python.exe",
            "pythonw.exe",
            "python313.dll",
            "Lib/os.py",
            "Lib/test/test_os.py",
            "Lib/site-packages/pip/__init__.py",
            "Lib/__pycache__/os.cpython-313.pyc",
            "DLLs/_tkinter.pyd",
            "DLLs/_ctypes.pyd",
            "tcl/tcl8.6/init.tcl",
            "include/Python.h",
        ],
    )
    runtime = write_files(tmp_path / "runtime", ["vcruntime140.dll", "msvcp140.dll"])
    package_files = write_files(tmp_path / "package-files", list(package.PACKAGE_FILES))
    # Scripts are text with lines that end in "\n", as they come out of the repository.
    for script in ("Play.bat", "play.ps1", "game.psd1"):
        (package_files / script).write_text("first line\nsecond line é\n", encoding="utf-8")
    license_file = tmp_path / "LICENSE"
    license_file.write_text("GPL text", encoding="utf-8")
    return package.PackageInputs(
        build_directory=build,
        source_directory=source,
        llvm_mingw_directory=llvm_mingw,
        cmake_directory=cmake,
        ninja_directory=ninja,
        python_directory=python,
        launcher_name=LAUNCHER_NAME,
        runtime_directory=runtime,
        package_files_directory=package_files,
        license_file=license_file,
        versions={"llvm-mingw": "20261020", "ModernGekko": "edd460a"},
    )


@pytest.fixture
def package_directory(inputs, tmp_path) -> Path:
    return package.assemble_package(inputs, tmp_path / "out")


def files_under(root: Path) -> set[str]:
    return {path.relative_to(root).as_posix() for path in root.rglob("*") if path.is_file()}


# -- What goes in ---------------------------------------------------------------------------------


def test_the_package_folder_is_named_after_the_game_and_the_platform(package_directory):
    assert package_directory.name == "HarvestMoonMM-windows-arm64"


def test_the_programs_sit_side_by_side_with_their_libraries_and_dolphins_data(package_directory):
    files = files_under(package_directory / "bin")
    assert {
        f"{LAUNCHER_NAME}.exe",
        "moderngekko-port.exe",
        "moderngekko-run.exe",
        "dolrecomp.exe",
        "SDL3.dll",
        "Sys/GameSettings/G4AEE9.ini",
        "Sys/Shaders/default.glsl",
    } <= files


def test_the_visual_cpp_runtime_goes_beside_the_programs(package_directory):
    files = files_under(package_directory / "bin")
    assert {"vcruntime140.dll", "msvcp140.dll"} <= files


def test_debug_information_is_not_packaged(package_directory):
    assert not any(name.endswith(".pdb") for name in files_under(package_directory))


def test_the_module_sources_keep_their_layout_where_the_port_tool_looks_for_them(package_directory):
    source = package_directory / "bin" / "source"
    assert files_under(source) >= {
        "vendor/dolphin/module-template/CMakeLists.txt",
        "vendor/dolphin/GXRuntime/include/gxruntime/cpu.h",
        "vendor/dolphin/GXRuntime/src/core/cpu.c",
        "vendor/dolphin/GXRuntime/src/core/cpu.h",
        "vendor/dolphin/Source/Core/Core/PowerPC/StaticRecomp/StaticRecompABI.h",
        "vendor/dolphin/Data/Sys/GameSettings/G4AEE9.ini",
    }


def test_only_the_sources_that_a_module_build_reads_are_packaged(package_directory):
    names = files_under(package_directory / "bin" / "source")
    assert "vendor/dolphin/GXRuntime/src/hle/not_needed.c" not in names
    assert "vendor/dolphin/GXRuntime/tests/not_needed_either.c" not in names
    assert "vendor/dolphin/Data/Sys/GameSettings/GALE01.ini" not in names  # another game's settings
    assert "vendor/dolphin/Source/Core/Core/PowerPC/StaticRecomp/StaticRecompCore.cpp" not in names


def test_each_tool_of_the_toolchain_goes_in_its_own_folder(package_directory):
    toolchain = package_directory / "toolchain"
    assert (toolchain / "llvm-mingw" / "bin" / "clang.exe").is_file()
    assert (toolchain / "cmake" / "bin" / "cmake.exe").is_file()
    assert (toolchain / "ninja" / "ninja.exe").is_file()
    assert (toolchain / "python" / "python.exe").is_file()


def test_the_compiler_keeps_the_arm64_libraries_and_drops_the_other_processors(package_directory):
    files = files_under(package_directory / "toolchain" / "llvm-mingw")
    assert "aarch64-w64-mingw32/lib/libmsvcrt.a" in files
    assert "lib/clang/21/include/stddef.h" in files
    assert "lib/clang/21/lib/windows/libclang_rt.builtins-aarch64.a" in files
    assert not {name for name in files if name.startswith(("i686-", "x86_64-", "armv7-"))}
    assert not {name for name in files if "builtins-x86_64" in name or "builtins-i386" in name}
    assert "lib/clang/21/lib/windows/libclang_rt.builtins-arm.a" not in files


def test_the_compiler_drops_the_runtimes_for_linux_and_the_tools_for_developing(package_directory):
    files = files_under(package_directory / "toolchain" / "llvm-mingw")
    assert "bin/lld.exe" in files
    assert (
        not {
            "lib/clang/21/lib/linux/libclang_rt.asan-aarch64.a",
            "bin/lldb.exe",
            "bin/clangd.exe",
            "bin/aarch64-w64-mingw32-widl.exe",
            "bin/libpython3.14.dll",
            "busybox/bin/sh.exe",
        }
        & files
    )


def test_cmake_keeps_its_modules_and_license_and_drops_the_graphical_program_and_the_manual(
    package_directory,
):
    files = files_under(package_directory / "toolchain" / "cmake")
    assert {"share/cmake-4.4/Modules/Platform.cmake", "doc/cmake/LICENSE.rst"} <= files
    assert not {"bin/cmake-gui.exe", "bin/ctest.exe", "doc/cmake/html/index.html"} & files


def test_python_keeps_the_interpreter_and_the_library_and_drops_tests_pip_and_tk(package_directory):
    files = files_under(package_directory / "toolchain" / "python")
    assert {"python.exe", "python313.dll", "Lib/os.py", "DLLs/_ctypes.pyd"} <= files
    assert (
        not {
            "pythonw.exe",
            "Lib/test/test_os.py",
            "Lib/site-packages/pip/__init__.py",
            "Lib/__pycache__/os.cpython-313.pyc",
            "DLLs/_tkinter.pyd",
            "tcl/tcl8.6/init.tcl",
            "include/Python.h",
        }
        & files
    )


def test_the_license_texts_of_the_programs_are_in_the_licenses_folder(package_directory):
    assert files_under(package_directory / "licenses") == set(package.LICENSE_FILES)
    copied = (package_directory / "licenses" / "Dolphin-COPYING.txt").read_text(encoding="utf-8")
    assert copied == "vendor/dolphin/COPYING"  # the made-up files hold their own path


def test_the_files_of_the_package_folder_the_license_and_the_versions_are_in_the_root(package_directory):
    files = files_under(package_directory)
    assert set(package.PACKAGE_FILES) | {"LICENSE", "VERSIONS.txt"} <= files
    assert (package_directory / "LICENSE").read_text(encoding="utf-8") == "GPL text"


def test_the_versions_file_lists_each_thing_with_its_version_in_alphabetical_order(package_directory):
    assert (package_directory / "VERSIONS.txt").read_text(encoding="utf-8") == (
        "ModernGekko: edd460a\nllvm-mingw: 20261020\n"
    )


# -- Converted for Windows ---------------------------------------------------------------------------


def test_batch_files_get_windows_line_ends_and_no_byte_order_mark(package_directory):
    contents = (package_directory / "Play.bat").read_bytes()
    assert contents == "first line\r\nsecond line é\r\n".encode()


def test_powershell_files_get_windows_line_ends_and_a_byte_order_mark(package_directory):
    # Windows PowerShell 5.1 reads a script without the mark as ANSI and garbles accented letters.
    for name in ("play.ps1", "game.psd1"):
        contents = (package_directory / name).read_bytes()
        assert contents == b"\xef\xbb\xbf" + "first line\r\nsecond line é\r\n".encode()


def test_a_script_that_already_has_a_byte_order_mark_does_not_get_a_second_one(inputs, tmp_path):
    (inputs.package_files_directory / "play.ps1").write_bytes(b"\xef\xbb\xbfone line\n")
    root = package.assemble_package(inputs, tmp_path / "out")
    assert (root / "play.ps1").read_bytes() == b"\xef\xbb\xbfone line\r\n"


def test_a_script_that_already_has_windows_line_ends_does_not_get_them_doubled():
    assert package.as_windows_text("a\r\nb\r\n", ".bat") == b"a\r\nb\r\n"


def test_documents_are_copied_as_they_are(package_directory, inputs):
    source = inputs.package_files_directory / "README.md"
    assert (package_directory / "README.md").read_bytes() == source.read_bytes()


# -- Game data never gets in -------------------------------------------------------------------------


@pytest.mark.parametrize(
    "name",
    ["disc.iso", "disc.GCM", "game.rvz", "game.wbfs", "main.dol", "_Main.rel", "boot.bin", "apploader.img"],
)
def test_a_file_that_looks_like_game_data_stops_the_assembly(inputs, tmp_path, name):
    (inputs.ninja_directory / name).write_text("not really", encoding="utf-8")
    with pytest.raises(package.PackageError, match="game data") as problem:
        package.assemble_package(inputs, tmp_path / "out")
    assert name in str(problem.value)


def test_the_files_that_look_like_game_data_are_all_named(inputs, tmp_path):
    (inputs.ninja_directory / "one.iso").write_text("x", encoding="utf-8")
    (inputs.ninja_directory / "two.dol").write_text("x", encoding="utf-8")
    with pytest.raises(package.PackageError) as problem:
        package.assemble_package(inputs, tmp_path / "out")
    assert "toolchain/ninja/one.iso" in str(problem.value)
    assert "toolchain/ninja/two.dol" in str(problem.value)


# -- What is refused ---------------------------------------------------------------------------------


@pytest.mark.parametrize(
    "missing",
    ["moderngekko-port.exe", "moderngekko-run.exe", "dolrecomp.exe", f"{LAUNCHER_NAME}.exe"],
)
def test_a_missing_program_is_named_in_the_error(inputs, tmp_path, missing):
    (inputs.build_directory / missing).unlink()
    with pytest.raises(package.PackageError, match=missing):
        package.assemble_package(inputs, tmp_path / "out")


def test_a_build_folder_without_dolphins_data_is_refused(inputs, tmp_path):
    shutil.rmtree(inputs.build_directory / "Sys")
    with pytest.raises(package.PackageError, match="Dolphin's data folder"):
        package.assemble_package(inputs, tmp_path / "out")


def test_a_missing_source_folder_is_named_in_the_error(inputs, tmp_path):
    shutil.rmtree(inputs.source_directory / "vendor/dolphin/module-template")
    with pytest.raises(package.PackageError, match="vendor/dolphin/module-template"):
        package.assemble_package(inputs, tmp_path / "out")


def test_a_toolchain_folder_without_its_main_program_is_refused(inputs, tmp_path):
    (inputs.cmake_directory / "bin" / "cmake.exe").unlink()
    with pytest.raises(package.PackageError, match=r"cmake.*bin/cmake\.exe"):
        package.assemble_package(inputs, tmp_path / "out")


def test_a_missing_license_text_is_named_in_the_error(inputs, tmp_path):
    (inputs.source_directory / "vendor/dolphin/COPYING").unlink()
    with pytest.raises(package.PackageError, match="vendor/dolphin/COPYING"):
        package.assemble_package(inputs, tmp_path / "out")


def test_a_missing_file_of_the_package_folder_is_named_in_the_error(inputs, tmp_path):
    (inputs.package_files_directory / "play.ps1").unlink()
    with pytest.raises(package.PackageError, match=r"play\.ps1"):
        package.assemble_package(inputs, tmp_path / "out")


def test_the_visual_cpp_runtime_is_optional(inputs, tmp_path):
    without_runtime = package.PackageInputs(**{**inputs.__dict__, "runtime_directory": None})
    root = package.assemble_package(without_runtime, tmp_path / "out")
    assert not (root / "bin" / "vcruntime140.dll").exists()


def test_assembling_again_starts_from_an_empty_folder(inputs, tmp_path):
    first = package.assemble_package(inputs, tmp_path / "out")
    (first / "left_over.txt").write_text("from an earlier run", encoding="utf-8")
    second = package.assemble_package(inputs, tmp_path / "out")
    assert not (second / "left_over.txt").exists()


# -- Which files a pattern leaves out ----------------------------------------------------------------


def test_a_pattern_is_matched_against_the_path_inside_the_tool_in_lower_case():
    assert package.is_excluded("Lib/Test/test_os.py", ["lib/test/*"])
    assert not package.is_excluded("Lib/os.py", ["lib/test/*"])


def test_a_star_in_a_pattern_also_crosses_folders():
    assert package.is_excluded("Lib/json/__pycache__/decoder.pyc", ["*/__pycache__/*"])


# -- The zip -----------------------------------------------------------------------------------------


def test_the_zip_holds_every_file_under_the_name_of_the_package_folder(package_directory, tmp_path):
    zip_path = tmp_path / "zips" / "package.zip"
    package.write_zip(package_directory, zip_path)
    with zipfile.ZipFile(zip_path) as archive:
        names = set(archive.namelist())
        assert names == {f"{package_directory.name}/{name}" for name in files_under(package_directory)}
        assert archive.read(f"{package_directory.name}/LICENSE") == b"GPL text"


# -- The command line --------------------------------------------------------------------------------


def command_line(inputs, tmp_path, *extra) -> list[str]:
    return [
        "--package-files-directory", str(inputs.package_files_directory),
        "--license-file", str(inputs.license_file),
        "--build-directory", str(inputs.build_directory),
        "--source-directory", str(inputs.source_directory),
        "--llvm-mingw-directory", str(inputs.llvm_mingw_directory),
        "--cmake-directory", str(inputs.cmake_directory),
        "--ninja-directory", str(inputs.ninja_directory),
        "--python-directory", str(inputs.python_directory),
        "--output-directory", str(tmp_path / "cli-out"),
        "--launcher-name", LAUNCHER_NAME,
        *extra,
    ]  # fmt: skip


def test_the_command_line_builds_the_folder_and_the_zip_and_says_where_they_are(inputs, tmp_path, capsys):
    zip_path = tmp_path / "cli.zip"
    status = package.main(
        command_line(inputs, tmp_path, "--zip", str(zip_path), "--version", "ModernGekko=abc")
    )
    output = capsys.readouterr().out
    assert status == 0
    assert str(tmp_path / "cli-out" / package.PACKAGE_FOLDER_NAME) in output
    assert zip_path.is_file()
    versions = (tmp_path / "cli-out" / package.PACKAGE_FOLDER_NAME / "VERSIONS.txt").read_text(
        encoding="utf-8"
    )
    pinned = {tool.name: tool.version for tool in fetch_windows_toolchain.TOOLS}
    assert versions == "".join(
        f"{name}: {version}\n" for name, version in sorted({**pinned, "ModernGekko": "abc"}.items())
    )


def test_the_command_line_records_the_versions_of_the_downloaded_build_tools_by_itself(inputs, tmp_path):
    assert package.main(command_line(inputs, tmp_path)) == 0
    versions = (tmp_path / "cli-out" / package.PACKAGE_FOLDER_NAME / "VERSIONS.txt").read_text(
        encoding="utf-8"
    )
    for tool in fetch_windows_toolchain.TOOLS:
        assert f"{tool.name}: {tool.version}\n" in versions


def test_a_version_given_on_the_command_line_wins_over_the_downloaded_tool_of_the_same_name(inputs, tmp_path):
    assert package.main(command_line(inputs, tmp_path, "--version", "CMake=9.9.9")) == 0
    versions = (tmp_path / "cli-out" / package.PACKAGE_FOLDER_NAME / "VERSIONS.txt").read_text(
        encoding="utf-8"
    )
    pinned_cmake = next(tool for tool in fetch_windows_toolchain.TOOLS if tool.name == "CMake")
    assert "CMake: 9.9.9\n" in versions
    assert f"CMake: {pinned_cmake.version}\n" not in versions


def test_the_command_line_reports_a_problem_and_fails_with_status_2(inputs, tmp_path, capsys):
    (inputs.ninja_directory / "ninja.exe").unlink()
    status = package.main(command_line(inputs, tmp_path))
    assert status == 2
    assert "ninja.exe" in capsys.readouterr().err


@pytest.mark.parametrize("text", ["no-equals-sign", "=value", "name="])
def test_a_version_without_a_name_or_a_value_is_refused(text):
    with pytest.raises(package.argparse.ArgumentTypeError):
        package.parse_version(text)


def test_a_version_is_split_at_the_first_equals_sign():
    assert package.parse_version("cmake=4.4.4=beta") == ("cmake", "4.4.4=beta")
