"""Tests of tools/package_native_windows.py and tools/fetch_native_package_tools.py, with made-up
inputs. They need no game, no build and no network.

Pruebas del armado del paquete nativo para Windows y de las descargas que lleva, con entradas
inventadas. No necesitan el juego, ni una compilación, ni la red.
"""

import json
import re
import sys
from pathlib import Path

import pytest
import yaml

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import fetch_native_package_tools  # noqa: E402
import package_native_windows  # noqa: E402
from made_up_windows_binary import write_windows_binary  # noqa: E402
from package_native_windows import Inputs, PackageError, assemble, local_imports  # noqa: E402

REPOSITORY = Path(__file__).resolve().parent.parent
WORKFLOW = REPOSITORY / ".github" / "workflows" / "package-native-windows.yml"


def touch(path: Path, content: bytes = b"x") -> Path:
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_bytes(content)
    return path


def made_up_inputs(tmp_path: Path, architecture: str = "arm64") -> Inputs:
    triple = package_native_windows.ARCHITECTURES[architecture]
    other = "x86_64" if architecture == "arm64" else "aarch64"
    processor = "aarch64" if architecture == "arm64" else "x86_64"
    kit = tmp_path / "kit"
    touch(kit / "lib" / "000_main.cpp.obj")
    recipe = {
        "format": 1,
        "system": "Windows",
        "executable": "hmmm-native.exe",
        "cxx_compiler": f"{triple}-clang++",
        "c_compiler": f"{triple}-clang",
        "link": ["{kit}/lib/000_main.cpp.obj", "{game_objects}", "-o", "{output}"],
        "mods": ["player_speed"],
    }
    touch(kit / "recipe.json", json.dumps(recipe).encode())
    translator = tmp_path / "translator"
    touch(translator / "Translator.Cli.exe")
    touch(translator / "Translator.Core.dll")
    python = tmp_path / "python"
    write_windows_binary(python / "python.exe", architecture, ["python313.dll", "VCRUNTIME140.dll"])
    write_windows_binary(python / "python313.dll", architecture, ["KERNEL32.dll", "VCRUNTIME140.dll"])
    write_windows_binary(python / "vcruntime140.dll", architecture)
    touch(python / "Lib" / "os.py")
    touch(python / "Lib" / "test" / "test_os.py")
    touch(python / "tcl" / "tk.tcl")
    llvm_mingw = tmp_path / "llvm-mingw"
    # As in llvm-mingw: the triple's compilers and clang.exe are small wrappers of clang-<version>.
    for name in (f"{triple}-clang++.exe", f"{triple}-clang.exe", "clang.exe"):
        write_windows_binary(llvm_mingw / "bin" / name, architecture, ["KERNEL32.dll"])
    write_windows_binary(
        llvm_mingw / "bin" / "clang-23.exe", architecture, ["libclang-cpp.dll", "libc++.dll"]
    )
    write_windows_binary(llvm_mingw / "bin" / "ld.lld.exe", architecture, ["libc++.dll"])
    write_windows_binary(llvm_mingw / "bin" / "libclang-cpp.dll", architecture, ["libc++.dll"])
    write_windows_binary(llvm_mingw / "bin" / "libc++.dll", architecture, ["libunwind.dll"])
    write_windows_binary(llvm_mingw / "bin" / "libunwind.dll", architecture)
    for name in ("lldb.exe", "clangd.exe"):
        touch(llvm_mingw / "bin" / name)
    touch(llvm_mingw / "bin" / f"{other}-w64-mingw32-clang.exe")
    touch(llvm_mingw / triple / "lib" / "libc++.a")
    touch(llvm_mingw / f"{other}-w64-mingw32" / "lib" / "libc++.a")
    touch(llvm_mingw / "i686-w64-mingw32" / "lib" / "libc++.a")
    touch(llvm_mingw / "include" / "windows.h")
    touch(llvm_mingw / "lib" / "clang" / "23" / "lib" / "windows" / f"libclang_rt.builtins-{processor}.a")
    touch(llvm_mingw / "lib" / "clang" / "23" / "lib" / "windows" / f"libclang_rt.builtins-{other}.a")
    touch(llvm_mingw / "lib" / "clang" / "23" / "lib" / "linux" / "libclang_rt.builtins-x86_64.a")
    touch(llvm_mingw / "python" / "lib" / "site.py")
    visual_cpp_runtime = tmp_path / "visual-cpp-runtime"
    write_windows_binary(visual_cpp_runtime / "vcruntime140.dll", architecture)
    write_windows_binary(visual_cpp_runtime / "msvcp140.dll", architecture, ["VCRUNTIME140.dll"])
    return Inputs(
        architecture=architecture,
        launcher=write_windows_binary(
            tmp_path / "HarvestMoonMM.exe", architecture, ["KERNEL32.dll", "USER32.dll"]
        ),
        kit=kit,
        translator=translator,
        python=python,
        llvm_mingw=llvm_mingw,
        nodtool=write_windows_binary(tmp_path / "nodtool-windows.exe", architecture, ["VCRUNTIME140.dll"]),
        visual_cpp_runtime=visual_cpp_runtime,
        output_directory=tmp_path / "out",
        versions={"HMMMGC": "v1.0"},
        licenses={"nod-LICENSE-MIT.txt": touch(tmp_path / "LICENSE-MIT", b"MIT")},
    )


def files_of(folder: Path) -> set[str]:
    return {path.relative_to(folder).as_posix() for path in folder.rglob("*") if path.is_file()}


@pytest.mark.parametrize("architecture", ["arm64", "x64"])
def test_the_package_has_the_launcher_and_the_builder_laid_out_like_the_repository(tmp_path, architecture):
    package = assemble(made_up_inputs(tmp_path, architecture))
    assert package.name == f"HarvestMoonMM-windows-{architecture}"
    files = files_of(package)
    for expected in (
        "HarvestMoonMM.exe",
        "README.txt",
        "LICENSE.txt",
        "THIRD_PARTY_NOTICES.txt",
        "VERSIONS.txt",
        "licenses/nod-LICENSE-MIT.txt",
        "builder/tools/prepare_game.py",
        "builder/tools/check_disc.py",
        "builder/tools/insert_mod_hooks.py",
        "builder/config/G4AEE9/symbols.txt",
        "builder/runtime/src/main.cpp",
        "builder/runtime/mod_api/moderngekko/mod_abi.h",
        "builder/mods/include/hm_mm_mod.h",
        "builder/mods/player_speed/mod.c",
        "builder/mods/player_speed/mod.json",
        "builder/kit/recipe.json",
        "builder/translator/Translator.Cli.exe",
        "builder/python/python.exe",
        "builder/python/Lib/os.py",
        "builder/nodtool/nodtool.exe",
    ):
        assert expected in files, expected
    # Only the mods of the kit, and none of the tools that the preparation does not run.
    assert not any(path.startswith("builder/mods/jamie_marriage/") for path in files)
    assert "builder/tools/package_native_windows.py" not in files


def test_the_preparation_s_tools_bring_what_they_import():
    imported = local_imports(REPOSITORY / "tools" / "prepare_game.py")
    assert {"check_disc.py", "project.py", "dol.py"} <= imported


@pytest.mark.parametrize("architecture", ["arm64", "x64"])
def test_the_compiler_and_python_keep_only_what_the_preparation_uses(tmp_path, architecture):
    package = assemble(made_up_inputs(tmp_path, architecture))
    files = files_of(package / "builder" / "llvm-mingw")
    triple = package_native_windows.ARCHITECTURES[architecture]
    other = "x86_64" if architecture == "arm64" else "aarch64"
    processor = "aarch64" if architecture == "arm64" else "x86_64"
    assert f"bin/{triple}-clang++.exe" in files
    assert f"{triple}/lib/libc++.a" in files
    assert f"lib/clang/23/lib/windows/libclang_rt.builtins-{processor}.a" in files
    assert not any(
        path.startswith((f"{other}-w64-mingw32/", "i686-w64-mingw32/", "python/")) for path in files
    )
    assert f"bin/{other}-w64-mingw32-clang.exe" not in files
    assert f"lib/clang/23/lib/windows/libclang_rt.builtins-{other}.a" not in files
    assert "lib/clang/23/lib/linux/libclang_rt.builtins-x86_64.a" not in files
    assert "bin/lldb.exe" not in files and "bin/clangd.exe" not in files
    python = files_of(package / "builder" / "python")
    assert python == {"python.exe", "python313.dll", "vcruntime140.dll", "Lib/os.py"}


def test_nodtool_gets_beside_it_the_part_of_the_visual_cpp_runtime_it_uses(tmp_path):
    package = assemble(made_up_inputs(tmp_path))
    assert files_of(package / "builder" / "nodtool") == {"nodtool.exe", "vcruntime140.dll"}


def test_a_program_for_the_other_processor_stops_the_package(tmp_path):
    inputs = made_up_inputs(tmp_path, "arm64")
    write_windows_binary(inputs.launcher, "x64", ["KERNEL32.dll"])
    with pytest.raises(PackageError, match="HarvestMoonMM.exe is for x64, and the package for arm64"):
        assemble(inputs)


def test_a_program_that_would_not_find_a_dll_stops_the_package(tmp_path):
    inputs = made_up_inputs(tmp_path)
    (inputs.python / "python313.dll").unlink()
    with pytest.raises(PackageError, match="would not start: python.exe needs python313.dll"):
        assemble(inputs)


def test_the_clang_that_the_compilers_run_is_checked_too(tmp_path):
    inputs = made_up_inputs(tmp_path)
    (inputs.llvm_mingw / "bin" / "libclang-cpp.dll").unlink()
    with pytest.raises(PackageError, match="clang-23.exe needs libclang-cpp.dll"):
        assemble(inputs)


def test_the_dlls_that_go_beside_the_game_are_checked_too(tmp_path):
    inputs = made_up_inputs(tmp_path, "x64")
    write_windows_binary(inputs.kit / "bin" / "webgpu_dawn.dll", "x64", ["MSVCP140.dll"])
    with pytest.raises(PackageError, match="webgpu_dawn.dll needs MSVCP140.dll"):
        assemble(inputs)
    write_windows_binary(inputs.kit / "bin" / "msvcp140.dll", "x64")
    assert "builder/kit/bin/msvcp140.dll" in files_of(assemble(inputs))


def test_the_package_s_texts_are_for_notepad_and_say_the_versions(tmp_path):
    package = assemble(made_up_inputs(tmp_path))
    for name in ("README.txt", "THIRD_PARTY_NOTICES.txt", "LICENSE.txt", "VERSIONS.txt"):
        text = (package / name).read_bytes()
        assert b"\r\n" in text and b"\n" not in text.replace(b"\r\n", b"")
    versions = (package / "VERSIONS.txt").read_text(encoding="utf-8")
    assert (
        "HMMMGC: v1.0" in versions and "architecture: arm64" in versions and "mods: player_speed" in versions
    )


def test_a_file_that_looks_like_game_data_stops_the_package(tmp_path):
    inputs = made_up_inputs(tmp_path)
    touch(inputs.kit / "lib" / "main.dol")
    with pytest.raises(PackageError, match="game data"):
        assemble(inputs)


def test_a_kit_whose_link_names_the_translated_game_is_refused(tmp_path):
    inputs = made_up_inputs(tmp_path)
    recipe = json.loads((inputs.kit / "recipe.json").read_text())
    recipe["link"].append("CMakeFiles/translated_game.dir/shard.obj")
    (inputs.kit / "recipe.json").write_text(json.dumps(recipe))
    with pytest.raises(PackageError, match="translated game"):
        assemble(inputs)


def test_a_compiler_the_recipe_uses_must_be_in_the_package(tmp_path):
    inputs = made_up_inputs(tmp_path)
    triple = package_native_windows.ARCHITECTURES["arm64"]
    (inputs.llvm_mingw / "bin" / f"{triple}-clang++.exe").unlink()
    with pytest.raises(PackageError, match="clang"):
        assemble(inputs)


def test_every_download_is_pinned_to_an_address_and_a_hash():
    items = [*fetch_native_package_tools.DOWNLOADS["arm64"], *fetch_native_package_tools.DOWNLOADS["x64"]]
    items.append(fetch_native_package_tools.CROSS_COMPILER)
    for item in items:
        assert item.url.startswith("https://github.com/"), item.url
        assert re.fullmatch(r"[0-9a-f]{64}", item.sha256), item.name
        assert item.version in item.url
    for architecture in ("arm64", "x64"):
        folders = {item.folder for item in fetch_native_package_tools.DOWNLOADS[architecture]}
        assert folders == {"llvm-mingw", "python", "nodtool", "dawn"}


def test_the_workflow_runs_on_releases_and_by_hand_and_only_its_last_job_can_write():
    workflow = yaml.safe_load(WORKFLOW.read_text(encoding="utf-8"))
    assert set(workflow[True]) == {"release", "workflow_dispatch"}
    assert workflow["permissions"] == {"contents": "read"}
    assert workflow["jobs"]["release"]["permissions"] == {"contents": "write"}
    assert workflow["jobs"]["release"]["if"] == "github.event_name == 'release'"
    # The Visual C++ runtime comes from a Windows runner, before the packages are assembled.
    assert workflow["jobs"]["package"]["needs"] == "visual-cpp-runtime"
    assert "permissions" not in workflow["jobs"]["visual-cpp-runtime"]
    assert set(workflow["jobs"]["package"]["strategy"]["matrix"]["architecture"]) == set(
        package_native_windows.ARCHITECTURES
    )


def test_the_workflow_reads_the_folder_the_assembler_writes():
    text = WORKFLOW.read_text(encoding="utf-8")
    assert "build/dist/HarvestMoonMM-windows-$ARCHITECTURE/VERSIONS.txt" in text
    assert "tools/fetch_native_package_tools.py" in text and "tools/package_native_windows.py" in text
