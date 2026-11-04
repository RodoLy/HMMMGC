#!/usr/bin/env python3
"""Assemble the Windows ARM64 package: the programs, a compiler, and the scripts that tie them.

Arma el paquete para Windows ARM64: los programas, un compilador y los scripts que los unen.

The game is recompiled on the player's own laptop, from the player's own disc, so what is
shipped is not the game but everything needed to do that without installing anything:

    bin/        the launcher, the runner, the recompiler (DolRecomp) and the tool that drives
                it (moderngekko-port), with the files they read at run time; `bin/source/`
                holds the few source folders that building a game module needs
    toolchain/  the portable C compiler (llvm-mingw), CMake, Ninja and Python that build it
    licenses/   the license texts of the programs in bin/
    Play.bat, play.ps1, game.psd1
                the double-click entry point, the script that does the work, and the data
                that says which game this package plays

The inputs are folders that the workflow has already built or unpacked; this script only
decides what goes where and what stays out, which is why it can be tested on any machine. The
package must never hold game data, so the script refuses to finish if any file looks like a
disc image or a game executable.

El juego se recompila en la laptop del jugador, con el disco del jugador, así que lo que se
entrega no es el juego sino todo lo necesario para hacerlo sin instalar nada:

    bin/        el launcher, el runner, el recompilador (DolRecomp) y la herramienta que lo
                maneja (moderngekko-port), con los archivos que leen al ejecutarse; `bin/source/`
                tiene las pocas carpetas de código que hacen falta para construir el módulo
                de un juego
    toolchain/  el compilador de C portátil (llvm-mingw), CMake, Ninja y Python que lo construyen
    licenses/   los textos de las licencias de los programas de bin/
    Play.bat, play.ps1, game.psd1
                el punto de entrada con doble clic, el script que hace el trabajo y los datos
                que dicen qué juego juega este paquete

Las entradas son carpetas que el workflow ya construyó o descomprimió; este script solo decide
qué va dónde y qué se queda fuera, por eso se puede probar en cualquier máquina. El paquete no
debe llevar datos del juego nunca, así que el script se niega a terminar si algún archivo
parece una imagen de disco o un ejecutable del juego.

Usage / Uso:
    python3 tools/package_windows.py --build-directory B --source-directory S \\
        --llvm-mingw-directory L --cmake-directory C --ninja-directory N --python-directory P \\
        --runtime-directory R --output-directory O --zip O/HarvestMoonMM-windows-arm64.zip \\
        --version ModernGekko=edd460a --version llvm-mingw=20261020
"""

import argparse
import codecs
import fnmatch
import shutil
import sys
import zipfile
from collections.abc import Iterable
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from textfiles import write_text  # noqa: E402

PACKAGE_FOLDER_NAME = "HarvestMoonMM-windows-arm64"
DEFAULT_LAUNCHER_NAME = "HarvestMoonMM"
PACKAGE_FILES_DIRECTORY = Path(__file__).resolve().parent.parent / "packaging" / "windows"
LICENSE_FILE = Path(__file__).resolve().parent.parent / "LICENSE"

# The programs that sit side by side in bin/. moderngekko-port looks for DolRecomp beside itself,
# and the launcher for the runner.
PROGRAMS = ("moderngekko-port", "moderngekko-run", "dolrecomp")
RUN_TIME_DATA_FOLDER = "Sys"  # Dolphin's data folder, which the runner reads from beside itself
GAME_ID_OF_THE_PACKAGE_SETTINGS = "G4AEE9"

# What building a game module reads from ModernGekko's source tree, relative to it. They go to
# bin/source/ with the same layout, where moderngekko-port looks for them (see
# patches/moderngekko/relocatable-port.patch).
SOURCE_FOLDERS = (
    "vendor/dolphin/module-template",
    "vendor/dolphin/GXRuntime/include",
    "vendor/dolphin/GXRuntime/src/core",
    "vendor/dolphin/Source/Core/Core/PowerPC/StaticRecomp",
)
SOURCE_FILES = (f"vendor/dolphin/Data/Sys/GameSettings/{GAME_ID_OF_THE_PACKAGE_SETTINGS}.ini",)
# The license texts of the programs in bin/, copied into licenses/: name there -> path in the source
# tree. The licenses of the tools in toolchain/ travel inside their own folders.
LICENSE_FILES = {
    "ModernGekko-GPL-3.0.txt": "LICENSE",
    "Dolphin-COPYING.txt": "vendor/dolphin/COPYING",
    "Dolphin-externals-licenses.md": "vendor/dolphin/Externals/licenses.md",
}
# The ABI folder is shared with Dolphin's own build, whose C++ sources a module never compiles.
SOURCE_EXCLUDED = ("*.cpp",)

# Files of the package folder itself, from packaging/windows/. The player double-clicks Play.bat.
BATCH_FILES = ("Play.bat",)
SCRIPT_FILE = "play.ps1"
GAME_DEFINITION_FILE = "game.psd1"
PACKAGE_DOCUMENTS = ("README.md", "THIRD_PARTY_NOTICES.md")
PACKAGE_FILES = (*BATCH_FILES, SCRIPT_FILE, GAME_DEFINITION_FILE, *PACKAGE_DOCUMENTS)

# Parts of each tool that a player's build never uses, left out to keep the download small. A
# pattern matches the path relative to the tool's folder, in lower case; `*` also crosses folders.
LLVM_MINGW_EXCLUDED = (
    "i686-w64-mingw32/*",  # the system libraries of the other processors: modules are built for ARM64
    "x86_64-w64-mingw32/*",
    "armv7-w64-mingw32/*",
    "lib/clang/*/lib/linux/*",  # the compiler's runtime libraries for Linux programs
    "lib/clang/*/lib/windows/*-x86_64.*",  # and for the other processors
    "lib/clang/*/lib/windows/*-i386.*",
    "lib/clang/*/lib/windows/*-arm.*",
    "bin/lldb*",  # the debugger, the language server, the linter and the interface compiler
    "bin/liblldb.dll",
    "bin/libpython*.dll",
    "bin/clangd.exe",
    "bin/clang-tidy.exe",
    "bin/*widl.exe",
    "python/*",  # the debugger's Python
    "busybox/*",  # Unix tools that a build with CMake and Ninja does not use
)
CMAKE_EXCLUDED = (
    "doc/cmake/html/*",  # the manual; the license texts next to it stay
    "man/*",
    "bin/cmake-gui.exe",  # the graphical program, and the test and packaging programs
    "bin/ctest.exe",
    "bin/cpack.exe",
    "share/vim/*",
    "share/emacs/*",
    "share/bash-completion/*",
    "share/aclocal/*",
)
PYTHON_EXCLUDED = (
    "include/*",  # only the interpreter and the standard library are needed, to run one script
    "libs/*",
    "scripts/*",
    "tcl/*",
    "tools/*",
    "doc/*",
    "pythonw.exe",
    "dlls/_tkinter.pyd",
    "dlls/tcl*.dll",
    "dlls/tk*.dll",
    "lib/test/*",
    "lib/site-packages/*",
    "lib/idlelib/*",
    "lib/tkinter/*",
    "lib/turtledemo/*",
    "lib/lib2to3/*",
    "lib/ensurepip/*",
    "lib/pydoc_data/*",
    "lib/venv/*",
    "*/__pycache__/*",
    "__pycache__/*",
)

# A file with one of these extensions is a disc image or an executable of the game, and one with one
# of these names is a system file of a disc. The package is built from tools and source only;
# finding one means something went very wrong, such as an extracted game given as a tool folder.
GAME_DATA_EXTENSIONS = frozenset(
    {".iso", ".gcm", ".rvz", ".wia", ".wbfs", ".ciso", ".gcz", ".tgc", ".dol", ".rel"}
)
GAME_DATA_FILE_NAMES = frozenset({"boot.bin", "bi2.bin", "apploader.img", "fst.bin"})

UTF8_BYTE_ORDER_MARK = codecs.BOM_UTF8  # Windows PowerShell 5.1 reads a script without it as ANSI
WINDOWS_LINE_END = "\r\n"
BATCH_FILE_SUFFIXES = frozenset({".bat"})
POWERSHELL_SUFFIXES = frozenset({".ps1", ".psd1"})


class PackageError(Exception):
    """An input is missing or the package would hold something it must not."""


@dataclass(frozen=True)
class PackageInputs:
    """Where everything that goes into the package is, as folders that already exist."""

    build_directory: Path  # ModernGekko's build folder: the programs and Sys/
    source_directory: Path  # ModernGekko's source tree (lib/ModernGekko)
    llvm_mingw_directory: Path
    cmake_directory: Path
    ninja_directory: Path
    python_directory: Path
    launcher_name: str = DEFAULT_LAUNCHER_NAME
    runtime_directory: Path | None = None  # the Visual C++ runtime DLLs the programs need
    package_files_directory: Path = PACKAGE_FILES_DIRECTORY
    license_file: Path = LICENSE_FILE
    versions: dict[str, str] = field(default_factory=dict)  # what was packaged, for VERSIONS.txt


def program_file_name(program: str) -> str:
    return f"{program}.exe"


def require_file(path: Path, what: str) -> None:
    if not path.is_file():
        raise PackageError(f"{what} not found: {path}")


def require_directory(path: Path, what: str) -> None:
    if not path.is_dir():
        raise PackageError(f"{what} not found: {path}")


def is_excluded(relative_path: str, patterns: Iterable[str]) -> bool:
    """Whether a path (relative to its tool's folder, with `/`) matches one of the patterns."""
    lowered = relative_path.lower()
    return any(fnmatch.fnmatchcase(lowered, pattern) for pattern in patterns)


def copy_tree(source: Path, destination: Path, excluded: Iterable[str] = ()) -> int:
    """Copy the files under `source` to `destination`, except those that match `excluded`.

    Returns how many files were copied.
    """
    excluded = tuple(excluded)
    copied = 0
    for path in sorted(source.rglob("*")):
        if not path.is_file() or is_excluded(path.relative_to(source).as_posix(), excluded):
            continue
        target = destination / path.relative_to(source)
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(path, target)
        copied += 1
    return copied


def as_windows_text(text: str, suffix: str) -> bytes:
    """A script or batch file as Windows wants it: CRLF line ends, and a byte-order mark for PowerShell."""
    encoded = text.replace("\r\n", "\n").replace("\n", WINDOWS_LINE_END).encode("utf-8")
    return UTF8_BYTE_ORDER_MARK + encoded if suffix in POWERSHELL_SUFFIXES else encoded


def copy_package_file(source: Path, destination: Path) -> None:
    """Copy a file of the package folder; scripts and batch files are converted for Windows."""
    require_file(source, "package file")
    destination.parent.mkdir(parents=True, exist_ok=True)
    if source.suffix in BATCH_FILE_SUFFIXES | POWERSHELL_SUFFIXES:
        destination.write_bytes(as_windows_text(source.read_text(encoding="utf-8"), source.suffix))
    else:
        shutil.copy2(source, destination)


def add_programs(inputs: PackageInputs, bin_directory: Path) -> None:
    """The programs, the DLLs beside them, Dolphin's Sys folder and the Visual C++ runtime."""
    launcher = inputs.launcher_name
    for program in (launcher, *PROGRAMS):
        source = inputs.build_directory / program_file_name(program)
        require_file(source, f"the program {program_file_name(program)}")
        bin_directory.mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, bin_directory / source.name)
    for library in sorted(inputs.build_directory.glob("*.dll")):
        shutil.copy2(library, bin_directory / library.name)
    require_directory(inputs.build_directory / RUN_TIME_DATA_FOLDER, "Dolphin's data folder")
    copy_tree(inputs.build_directory / RUN_TIME_DATA_FOLDER, bin_directory / RUN_TIME_DATA_FOLDER)
    if inputs.runtime_directory is not None:
        require_directory(inputs.runtime_directory, "the Visual C++ runtime folder")
        for library in sorted(inputs.runtime_directory.glob("*.dll")):
            shutil.copy2(library, bin_directory / library.name)


def add_module_sources(inputs: PackageInputs, bin_directory: Path) -> None:
    """The source folders moderngekko-port reads to build a game module, laid out as in the source tree."""
    destination = bin_directory / "source"
    for folder in SOURCE_FOLDERS:
        require_directory(inputs.source_directory / folder, f"source folder {folder}")
        copy_tree(inputs.source_directory / folder, destination / folder, SOURCE_EXCLUDED)
    for file in SOURCE_FILES:
        require_file(inputs.source_directory / file, f"source file {file}")
        (destination / file).parent.mkdir(parents=True, exist_ok=True)
        shutil.copy2(inputs.source_directory / file, destination / file)


def add_licenses(inputs: PackageInputs, root: Path) -> None:
    """The license texts of ModernGekko and of Dolphin, which the programs in bin/ are made of."""
    for name, relative_path in LICENSE_FILES.items():
        source = inputs.source_directory / relative_path
        require_file(source, f"license file {relative_path}")
        (root / "licenses").mkdir(parents=True, exist_ok=True)
        shutil.copy2(source, root / "licenses" / name)


def add_toolchain(inputs: PackageInputs, root: Path) -> None:
    """The compiler and the build tools, each in its own folder under toolchain/."""
    toolchain = root / "toolchain"
    for folder, source, excluded, executable in (
        ("llvm-mingw", inputs.llvm_mingw_directory, LLVM_MINGW_EXCLUDED, "bin/clang.exe"),
        ("cmake", inputs.cmake_directory, CMAKE_EXCLUDED, "bin/cmake.exe"),
        ("ninja", inputs.ninja_directory, (), "ninja.exe"),
        ("python", inputs.python_directory, PYTHON_EXCLUDED, "python.exe"),
    ):
        require_file(source / executable, f"{folder}'s {executable}")
        copy_tree(source, toolchain / folder, excluded)


def add_package_files(inputs: PackageInputs, root: Path) -> None:
    for name in PACKAGE_FILES:
        copy_package_file(inputs.package_files_directory / name, root / name)
    require_file(inputs.license_file, "the project's license")
    shutil.copy2(inputs.license_file, root / "LICENSE")


def write_versions(inputs: PackageInputs, root: Path) -> None:
    """VERSIONS.txt: what is in the package, one `name: version` line each, for whoever asks for sources."""
    lines = [f"{name}: {version}" for name, version in sorted(inputs.versions.items())]
    write_text(root / "VERSIONS.txt", "\n".join(lines) + "\n")


def looks_like_game_data(path: Path) -> bool:
    return path.suffix.lower() in GAME_DATA_EXTENSIONS or path.name.lower() in GAME_DATA_FILE_NAMES


def find_game_data(root: Path) -> list[Path]:
    """The files under `root` that look like a disc image, an executable or a system file of the game."""
    return sorted(path for path in root.rglob("*") if path.is_file() and looks_like_game_data(path))


def assemble_package(inputs: PackageInputs, output_directory: Path) -> Path:
    """Build the package folder under `output_directory` and return it."""
    root = output_directory / PACKAGE_FOLDER_NAME
    if root.exists():
        shutil.rmtree(root)
    bin_directory = root / "bin"
    add_programs(inputs, bin_directory)
    add_module_sources(inputs, bin_directory)
    add_licenses(inputs, root)
    add_toolchain(inputs, root)
    add_package_files(inputs, root)
    write_versions(inputs, root)
    game_data = find_game_data(root)
    if game_data:
        listing = "\n  ".join(str(path.relative_to(root)) for path in game_data)
        raise PackageError(f"the package would hold files that look like game data:\n  {listing}")
    return root


def write_zip(package_directory: Path, zip_path: Path) -> None:
    """Zip the package folder, with the folder's own name as the first part of every path."""
    zip_path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(package_directory.rglob("*")):
            if path.is_file():
                archive.write(
                    path, (Path(package_directory.name) / path.relative_to(package_directory)).as_posix()
                )


# -- Command line ------------------------------------------------------------------------------


def parse_version(text: str) -> tuple[str, str]:
    name, separator, version = text.partition("=")
    if not separator or not name or not version:
        raise argparse.ArgumentTypeError(f"expected NAME=VERSION, not {text!r}")
    return name, version


def parse_arguments(argv: list[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    for option, help_text in (
        ("--build-directory", "ModernGekko's build folder, with the programs and Sys/"),
        ("--source-directory", "ModernGekko's source tree (lib/ModernGekko of ModernGekko-Template)"),
        ("--llvm-mingw-directory", "the unpacked llvm-mingw for Windows ARM64"),
        ("--cmake-directory", "the unpacked CMake for Windows ARM64"),
        ("--ninja-directory", "the folder with ninja.exe"),
        ("--python-directory", "a Python for Windows ARM64: the folder with python.exe"),
        ("--output-directory", "where the package folder is made"),
    ):
        parser.add_argument(option, type=Path, required=True, help=help_text)
    parser.add_argument("--runtime-directory", type=Path, help="the folder with the Visual C++ runtime DLLs")
    parser.add_argument(
        "--package-files-directory",
        type=Path,
        default=PACKAGE_FILES_DIRECTORY,
        help="the folder with the scripts and documents of the package (default: packaging/windows)",
    )
    parser.add_argument(
        "--license-file", type=Path, default=LICENSE_FILE, help="the project's license (default: LICENSE)"
    )
    parser.add_argument(
        "--launcher-name", default=DEFAULT_LAUNCHER_NAME, help="the launcher's file name, without .exe"
    )
    parser.add_argument("--zip", type=Path, help="also write the package as this zip file")
    parser.add_argument(
        "--version",
        type=parse_version,
        action="append",
        default=[],
        metavar="NAME=VERSION",
        help="something that is in the package and its version, for VERSIONS.txt; repeat it",
    )
    return parser.parse_args(argv)


def main(argv: list[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    inputs = PackageInputs(
        build_directory=arguments.build_directory,
        source_directory=arguments.source_directory,
        llvm_mingw_directory=arguments.llvm_mingw_directory,
        cmake_directory=arguments.cmake_directory,
        ninja_directory=arguments.ninja_directory,
        python_directory=arguments.python_directory,
        launcher_name=arguments.launcher_name,
        runtime_directory=arguments.runtime_directory,
        package_files_directory=arguments.package_files_directory,
        license_file=arguments.license_file,
        versions=dict(arguments.version),
    )
    try:
        package_directory = assemble_package(inputs, arguments.output_directory)
        if arguments.zip:
            write_zip(package_directory, arguments.zip)
    except PackageError as problem:
        print(f"error: {problem}", file=sys.stderr)
        return 2
    print(f"package: {package_directory}")
    if arguments.zip:
        print(f"zip: {arguments.zip} ({arguments.zip.stat().st_size:,} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
