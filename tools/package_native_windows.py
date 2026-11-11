#!/usr/bin/env python3
"""Assemble the Windows package of the native version: the launcher and everything that prepares the
game on the player's computer, without anything of the game.

Arma el paquete de Windows de la versión nativa: el launcher y todo lo que prepara el juego en la
computadora del jugador, sin nada del juego.

The game's program is made on the player's computer from the player's own disc
(docs/NATIVE_PACKAGE.md), so the package carries:

    HarvestMoonMM.exe   the launcher (launcher/)
    README.txt, LICENSE.txt, THIRD_PARTY_NOTICES.txt, VERSIONS.txt, licenses/
    builder/            what prepares the game, laid out like this repository so that its tools
                        find each other:
        tools/          tools/prepare_game.py and the tools it runs, with what they import
        config/G4AEE9/symbols.txt, runtime/src/ (read by the translator), runtime/mod_api/,
        mods/           the mods of the kit (their mod.c, read for their hooks, and mod.json)
        kit/            the runtime kit (tools/make_runtime_kit.py)
        translator/     WiiCompiled's translator, published as a Windows program with its .NET
        python/         a Python for Windows, trimmed to the interpreter and the standard library
        llvm-mingw/     the compiler, trimmed to the processor of the package
        nodtool/        nodtool, which converts compressed disc images to ISO, with the part of
                        the Visual C++ runtime it uses

The inputs are folders and files that the workflow has already built or downloaded; this script
only decides what goes where and what stays out, which is why it can be tested on any machine. It
refuses to finish when a file looks like game data (a disc image, a DOL, a disc's system file),
when the kit's recipe could hold the translated game, or when one of the package's programs would
not start on a player's computer: when it is for the other processor, or needs a DLL that is not
beside it and is not a part of Windows (tools/windows_binaries.py).

El programa del juego se hace en la computadora del jugador con su propio disco, así que el paquete
lleva el launcher y lo que prepara el juego. Las entradas son carpetas y archivos que el workflow ya
construyó o descargó; este script solo decide qué va dónde y qué se queda fuera. Se niega a terminar
si algún archivo parece datos del juego.

Usage / Uso:
    python3 tools/package_native_windows.py --architecture arm64 --launcher L --kit K --translator T \\
        --python P --llvm-mingw M --nodtool N --visual-cpp-runtime V --output-directory O [--zip Z] \\
        [--version NAME=VALUE ...] [--license NAME=FILE ...] [--licenses-directory D]
"""

import argparse
import ast
import json
import shutil
import sys
import zipfile
from collections.abc import Iterable, Sequence
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from package_windows import (  # noqa: E402
    GAME_DATA_EXTENSIONS,
    GAME_DATA_FILE_NAMES,
    PYTHON_EXCLUDED,
    PackageError,
    copy_tree,
)
from windows_binaries import WindowsBinaryError, dlls_in, needed_dlls, read_windows_binary  # noqa: E402

REPOSITORY = Path(__file__).resolve().parent.parent
TOOLS = REPOSITORY / "tools"
PACKAGE_FILES = REPOSITORY / "packaging" / "native"
LAUNCHER_NAME = "HarvestMoonMM.exe"
ARCHITECTURES = {"arm64": "aarch64-w64-mingw32", "x64": "x86_64-w64-mingw32"}
# The tools the preparation runs; what they import comes with them.
PREPARATION_TOOLS = (
    "prepare_game.py",
    "write_translator_project.py",
    "size_crop_grid.py",
    "insert_mod_hooks.py",
)
# What of the repository the tools read, relative to its root.
REPOSITORY_FILES = ("config/G4AEE9/symbols.txt",)
REPOSITORY_FOLDERS = ("runtime/src", "runtime/mod_api", "mods/include")
REPOSITORY_FOLDERS_EXCLUDED = ("*/__pycache__/*",)

# The parts of llvm-mingw that preparing the game never uses: the other processors, the debugger,
# the language server and the linter, and the Unix tools. A pattern matches the path relative to its
# folder, in lower case; `*` also crosses folders.
LLVM_MINGW_EXCLUDED_ALWAYS = (
    "armv7-w64-mingw32/*",
    "i686-w64-mingw32/*",
    "arm64ec-w64-mingw32/*",
    "bin/armv7-*",
    "bin/i686-*",
    "bin/arm64ec-*",
    "bin/*uwp-*",
    "lib/clang/*/lib/linux/*",
    "lib/clang/*/lib/windows/*-i386.*",
    "lib/clang/*/lib/windows/*-arm.*",
    "lib/clang/*/lib/windows/*-armhf.*",
    "bin/lldb*",
    "bin/liblldb.dll",
    "bin/libpython*.dll",
    "bin/clangd.exe",
    "bin/clang-tidy.exe",
    "bin/clang-format.exe",
    "bin/*widl.exe",
    "bin/libffi-*.dll",
    "python/*",
    "busybox/*",
    "share/*",
)
LLVM_MINGW_EXCLUDED_BY_ARCHITECTURE = {
    "arm64": ("x86_64-w64-mingw32/*", "bin/x86_64-*", "lib/clang/*/lib/windows/*-x86_64.*"),
    "x64": ("aarch64-w64-mingw32/*", "bin/aarch64-*", "lib/clang/*/lib/windows/*-aarch64.*"),
}


@dataclass(frozen=True)
class Inputs:
    architecture: str
    launcher: Path
    kit: Path
    translator: Path  # the folder of the published translator
    python: Path
    llvm_mingw: Path
    nodtool: Path
    visual_cpp_runtime: Path  # the DLLs of Microsoft.VC*.CRT, for the package's processor
    output_directory: Path
    versions: dict[str, str] = field(default_factory=dict)
    licenses: dict[str, Path] = field(default_factory=dict)
    licenses_directory: Path | None = None  # every file in it goes to licenses/


def local_imports(script: Path) -> set[str]:
    """The tools of tools/ that `script` imports, and what those import, as file names."""
    found: set[str] = set()
    pending = [script]
    while pending:
        tree = ast.parse(pending.pop().read_text(encoding="utf-8"))
        for node in ast.walk(tree):
            names = []
            if isinstance(node, ast.Import):
                names = [alias.name.split(".")[0] for alias in node.names]
            elif isinstance(node, ast.ImportFrom) and node.module:
                names = [node.module.split(".")[0]]
            for name in names:
                module = TOOLS / f"{name}.py"
                if module.is_file() and module.name not in found:
                    found.add(module.name)
                    pending.append(module)
    return found


def check_kit(kit: Path) -> dict:
    recipe_file = kit / "recipe.json"
    if not recipe_file.is_file():
        raise PackageError(f"no runtime kit in {kit} (tools/make_runtime_kit.py makes it)")
    recipe = json.loads(recipe_file.read_text(encoding="utf-8"))
    if recipe.get("system") != "Windows":
        raise PackageError(f"the kit in {kit} is for {recipe.get('system')}, not Windows")
    # The translated game is filled in on the player's computer; a kit that names it would carry it.
    link = recipe.get("link", [])
    if "{game_objects}" not in link or any("translated_game" in word for word in link):
        raise PackageError("the kit's link command does not leave the translated game out")
    return recipe


def check_no_game_data(folder: Path) -> None:
    for path in folder.rglob("*"):
        if path.is_file() and (
            path.suffix.lower() in GAME_DATA_EXTENSIONS or path.name.lower() in GAME_DATA_FILE_NAMES
        ):
            raise PackageError(f"the package would hold a file that looks like game data: {path}")


def copy_file(source: Path, destination: Path) -> None:
    if not source.is_file():
        raise PackageError(f"not found: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    shutil.copy2(source, destination)


def copy_folder(source: Path, destination: Path, excluded: Iterable[str] = ()) -> None:
    if not source.is_dir():
        raise PackageError(f"not found: {source}")
    copy_tree(source, destination, excluded)


def check_that_the_programs_start(package: Path, architecture: str, recipe: dict) -> None:
    """Stops the package when one of its programs would not start on a player's computer: when it
    is for another processor, or needs a DLL that is not beside it and is not a part of Windows.
    The translator is left out: .NET publishes it with everything it needs."""
    builder = package / "builder"
    compilers = builder / "llvm-mingw" / "bin"
    kit_dlls = builder / "kit" / "bin"
    groups = (
        (package, [LAUNCHER_NAME]),
        (builder / "nodtool", ["nodtool.exe"]),
        (builder / "python", ["python.exe"]),
        # Of the compiler, what the preparation runs: the recipe's compilers, which are small
        # wrappers of clang-<version>.exe, that clang, and the linker it runs.
        (
            compilers,
            [
                f"{recipe['cxx_compiler']}.exe",
                f"{recipe['c_compiler']}.exe",
                "clang.exe",
                *sorted(path.name for path in compilers.glob("clang-[0-9]*.exe")),
                "ld.lld.exe",
            ],
        ),
        # The DLLs that go beside the game, which is made on the player's computer.
        (kit_dlls, sorted(dll.name for dll in dlls_in(kit_dlls).values()) if kit_dlls.is_dir() else []),
    )
    for folder, names in groups:
        if not names:
            continue
        programs = [folder / name for name in names]
        try:
            machine = read_windows_binary(programs[0]).machine
            if machine != architecture:
                raise PackageError(f"{programs[0]} is for {machine}, and the package for {architecture}")
            needed_dlls(programs, dlls_in(folder))
        except WindowsBinaryError as problem:
            raise PackageError(f"a program of the package would not start: {problem}") from None


def windows_text(text: str) -> str:
    """A text file for Notepad: CRLF line ends."""
    return text.replace("\r\n", "\n").replace("\n", "\r\n")


def assemble(inputs: Inputs) -> Path:
    if inputs.architecture not in ARCHITECTURES:
        raise PackageError(f"the architecture must be one of {', '.join(ARCHITECTURES)}")
    recipe = check_kit(inputs.kit)
    package = inputs.output_directory / f"HarvestMoonMM-windows-{inputs.architecture}"
    if package.exists():
        shutil.rmtree(package)
    builder = package / "builder"

    copy_file(inputs.launcher, package / LAUNCHER_NAME)
    for tool in sorted(
        {
            *PREPARATION_TOOLS,
            *(name for script in PREPARATION_TOOLS for name in local_imports(TOOLS / script)),
        }
    ):
        copy_file(TOOLS / tool, builder / "tools" / tool)
    for relative in REPOSITORY_FILES:
        copy_file(REPOSITORY / relative, builder / relative)
    for relative in REPOSITORY_FOLDERS:
        copy_folder(REPOSITORY / relative, builder / relative, REPOSITORY_FOLDERS_EXCLUDED)
    for mod in recipe.get("mods", []):
        for name in ("mod.c", "mod.json"):
            copy_file(REPOSITORY / "mods" / mod / name, builder / "mods" / mod / name)
    copy_folder(inputs.kit, builder / "kit")
    copy_folder(inputs.translator, builder / "translator")
    if not (builder / "translator" / "Translator.Cli.exe").is_file():
        raise PackageError(f"{inputs.translator} has no Translator.Cli.exe: publish it for Windows")
    copy_folder(inputs.python, builder / "python", PYTHON_EXCLUDED)
    if not (builder / "python" / "python.exe").is_file():
        raise PackageError(f"{inputs.python} has no python.exe")
    llvm_mingw_excluded = (
        *LLVM_MINGW_EXCLUDED_ALWAYS,
        *LLVM_MINGW_EXCLUDED_BY_ARCHITECTURE[inputs.architecture],
    )
    copy_folder(inputs.llvm_mingw, builder / "llvm-mingw", llvm_mingw_excluded)
    for compiler in (recipe["cxx_compiler"], recipe["c_compiler"]):
        if not (builder / "llvm-mingw" / "bin" / f"{compiler}.exe").is_file():
            raise PackageError(f"llvm-mingw has no bin/{compiler}.exe, which the kit's recipe uses")
    copy_file(inputs.nodtool, builder / "nodtool" / "nodtool.exe")
    # nodtool is built with Visual C++, and its runtime is not on every computer: it goes beside it.
    if not inputs.visual_cpp_runtime.is_dir():
        raise PackageError(f"not found: {inputs.visual_cpp_runtime}")
    try:
        nodtool_dlls = needed_dlls([builder / "nodtool" / "nodtool.exe"], dlls_in(inputs.visual_cpp_runtime))
    except WindowsBinaryError as problem:
        raise PackageError(f"nodtool would not start: {problem}") from None
    for dll in nodtool_dlls.values():
        copy_file(dll, builder / "nodtool" / dll.name)

    for name in ("README.txt", "THIRD_PARTY_NOTICES.txt"):
        text = (PACKAGE_FILES / name).read_text(encoding="utf-8")
        (package / name).write_text(windows_text(text), encoding="utf-8", newline="")
    (package / "LICENSE.txt").write_text(
        windows_text((REPOSITORY / "LICENSE").read_text(encoding="utf-8")), encoding="utf-8", newline=""
    )
    if inputs.licenses_directory is not None:
        copy_folder(inputs.licenses_directory, package / "licenses")
    for name, source in sorted(inputs.licenses.items()):
        copy_file(source, package / "licenses" / name)
    versions = [f"{name}: {value}" for name, value in sorted(inputs.versions.items())]
    versions.append(f"architecture: {inputs.architecture}")
    versions.append(f"mods: {', '.join(recipe.get('mods', [])) or 'none'}")
    (package / "VERSIONS.txt").write_text(
        windows_text("\n".join(versions) + "\n"), encoding="utf-8", newline=""
    )

    check_that_the_programs_start(package, inputs.architecture, recipe)
    check_no_game_data(package)
    return package


def write_zip(package: Path, zip_path: Path) -> None:
    zip_path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(package.rglob("*")):
            if path.is_file():
                archive.write(path, (Path(package.name) / path.relative_to(package)).as_posix())


def key_value(text: str) -> tuple[str, str]:
    name, separator, value = text.partition("=")
    if not separator or not name:
        raise argparse.ArgumentTypeError(f"expected NAME=VALUE, got {text!r}")
    return name, value


def parse_arguments(argv: Sequence[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--architecture", required=True, choices=sorted(ARCHITECTURES))
    parser.add_argument("--launcher", type=Path, required=True, help="the launcher built for Windows")
    parser.add_argument("--kit", type=Path, required=True, help="the runtime kit for Windows")
    parser.add_argument("--translator", type=Path, required=True, help="the translator published for Windows")
    parser.add_argument("--python", type=Path, required=True, help="an unpacked Python for Windows")
    parser.add_argument("--llvm-mingw", type=Path, required=True, help="an unpacked llvm-mingw for Windows")
    parser.add_argument("--nodtool", type=Path, required=True, help="nodtool.exe for Windows")
    parser.add_argument(
        "--visual-cpp-runtime",
        type=Path,
        required=True,
        help="a folder with the Visual C++ runtime's DLLs for the package's processor (Microsoft.VC*.CRT)",
    )
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument("--zip", type=Path, help="also write the package as this zip")
    parser.add_argument(
        "--version", type=key_value, action="append", default=[], help="NAME=VALUE for VERSIONS.txt"
    )
    parser.add_argument(
        "--license", type=key_value, action="append", default=[], help="NAME=FILE for licenses/"
    )
    parser.add_argument("--licenses-directory", type=Path, help="a folder of license texts for licenses/")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    inputs = Inputs(
        architecture=arguments.architecture,
        launcher=arguments.launcher,
        kit=arguments.kit,
        translator=arguments.translator,
        python=arguments.python,
        llvm_mingw=arguments.llvm_mingw,
        nodtool=arguments.nodtool,
        visual_cpp_runtime=arguments.visual_cpp_runtime,
        output_directory=arguments.output_directory,
        versions=dict(arguments.version),
        licenses={name: Path(path) for name, path in arguments.license},
        licenses_directory=arguments.licenses_directory,
    )
    try:
        package = assemble(inputs)
        if arguments.zip:
            write_zip(package, arguments.zip)
    except (PackageError, OSError, KeyError, json.JSONDecodeError) as problem:
        print(f"error: {problem}", file=sys.stderr)
        return 2
    size = sum(path.stat().st_size for path in package.rglob("*") if path.is_file())
    print(
        f"package in {package}: {size / 2**20:.0f} MB" + (f", zip {arguments.zip}" if arguments.zip else "")
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
