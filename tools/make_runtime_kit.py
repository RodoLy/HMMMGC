#!/usr/bin/env python3
"""Make the runtime kit: the native runtime compiled without the game, and the commands that
compile the game and link it with the runtime on a player's computer.

Hace el kit del runtime: el runtime nativo compilado sin el juego, y los comandos que compilan
el juego y lo enlazan con el runtime en la computadora de un jugador.

The game's code is translated from each player's own disc, on their own computer
(tools/prepare_game.py), so a package cannot carry it. What it can carry is everything around
it, already compiled: the runtime, aurora and the libraries they use. To get them, the runtime
is built as usual but with a stand-in of the translated game (runtime/kit/stub_game), and this
script reads that build (built with Ninja and CMAKE_EXPORT_COMPILE_COMMANDS=ON):

- from compile_commands.json, how the stand-in was compiled, which is how the translated game
  is compiled (they are the same CMake target);
- from Ninja, the command that linked the program, with every object and library in order.

It copies the objects and libraries into the kit and writes recipe.json: the same commands
with the paths of this computer replaced by placeholders that prepare_game.py fills in:
{kit} (the kit's folder), {translation} (the translated game), {game_objects} (the compiled
game) and {output} (the program). A path that it does not know how to replace stops it, so
that no recipe ever depends on the computer that made it.

El código del juego se traduce del disco de cada jugador en su propia computadora, así que un
paquete no puede llevarlo. Lo que sí puede llevar es todo lo de alrededor ya compilado: el
runtime, aurora y sus bibliotecas. Para obtenerlos, el runtime se compila como siempre pero con
un sustituto del juego traducido, y este script lee esa compilación: cómo se compiló el
sustituto (igual que el juego traducido) y el comando que enlazó el programa. Copia los objetos
y las bibliotecas al kit y escribe recipe.json con esos comandos, cambiando las rutas de esta
computadora por marcadores que llena prepare_game.py.

The kit's layout:

    recipe.json   the commands (see RECIPE_FORMAT)
    lib/          the runtime's objects and libraries, as the link command names them
    include/      the headers the translated game is compiled with (WiiCompiled's guest ABI)
                  and the header of the names of the game's functions for the mods
    bin/          the DLLs the program needs beside it (on Windows): those of
                  --runtime-files-directory (Dawn's), and those that the program and they
                  import, from the build (libpng's and zlib's) or from --dll-directory (the
                  Visual C++ runtime that Dawn uses)

Usage / Uso:
    python3 tools/make_runtime_kit.py --build-directory build/native/runtime-kit \\
        --wiicompiled-directory build/native/wiicompiled --out build/native/kit
"""

import argparse
import json
import re
import shlex
import shutil
import subprocess
import sys
from collections.abc import Sequence
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from windows_binaries import WindowsBinaryError, dlls_in, needed_dlls  # noqa: E402

REPOSITORY = Path(__file__).resolve().parent.parent
STUB_GAME_DIRECTORY = REPOSITORY / "runtime" / "kit" / "stub_game"
SYMBOL_HEADER = REPOSITORY / "build" / "G4AEE9_symbols.h"
PROGRAM_TARGET = "hmmm-native"
# Changes when the recipe's meaning changes; prepare_game.py refuses a format it does not know.
RECIPE_FORMAT = 1
# Libraries of the system that a Linux build links by path: they are on every Linux computer
# that can build the runtime, so they stay as they are (only for trying the kit on Linux).
SYSTEM_LIBRARY_DIRECTORIES = (Path("/usr/lib"), Path("/lib"), Path("/usr/lib64"), Path("/lib64"))
# Compiler options that only give debugging information, which nobody uses on a player's
# computer and which makes the game's objects several times bigger and slower to write.
DEBUG_INFORMATION = re.compile(r"-g\d?|-gdwarf(-\d)?|-gcodeview")
# The files a link command takes: objects, static libraries (and Windows import libraries),
# compiled Windows resources and, on Linux, shared libraries of the system.
LINKED_FILE = re.compile(r"\.(o|obj|a|lib|res|so(\.\d+)*)$")


class KitError(Exception):
    """The build is not one the kit can be made from."""


@dataclass
class Kit:
    """What goes into the kit's folder, filled while the commands are rewritten."""

    out: Path
    build_directory: Path
    copied: dict[Path, str] = field(default_factory=dict)  # source -> name in lib/

    def library(self, source: Path) -> str:
        """`source`, copied into lib/ once; returns its placeholder path."""
        source = source.resolve()
        if source not in self.copied:
            if not source.is_file():
                raise KitError(f"the link command names {source}, which is not there")
            name = f"{len(self.copied):03d}_{source.name}"
            shutil.copy2(source, self.out / "lib" / name)
            self.copied[source] = name
        return "{kit}/lib/" + self.copied[source]


def cache_value(build_directory: Path, name: str) -> str:
    """A value of the build's CMakeCache.txt ("" when it is not there)."""
    for line in (build_directory / "CMakeCache.txt").read_text(encoding="utf-8").splitlines():
        if line.startswith(name + ":"):
            return line.split("=", 1)[1]
    return ""


def target_system(build_directory: Path) -> tuple[str, str]:
    """The system and the processor the build is for, as CMake named them."""
    for system_file in build_directory.glob("CMakeFiles/*/CMakeSystem.cmake"):
        text = system_file.read_text(encoding="utf-8")
        system = re.search(r'set\(CMAKE_SYSTEM_NAME "([^"]+)"\)', text)
        processor = re.search(r'set\(CMAKE_SYSTEM_PROCESSOR "([^"]*)"\)', text)
        if system and processor:
            return system[1], processor[1]
    raise KitError(f"{build_directory} has no CMakeFiles/<version>/CMakeSystem.cmake: is it a CMake build?")


def compile_command(build_directory: Path, source: Path) -> list[str]:
    """The command that compiled `source`, from compile_commands.json, as a list."""
    commands_file = build_directory / "compile_commands.json"
    if not commands_file.is_file():
        raise KitError(
            f"{commands_file} is missing: configure the build with -DCMAKE_EXPORT_COMPILE_COMMANDS=ON"
        )
    for entry in json.loads(commands_file.read_text(encoding="utf-8")):
        if Path(entry["file"]).resolve() == source.resolve():
            return entry["arguments"] if "arguments" in entry else shlex.split(entry["command"])
    raise KitError(f"{source} is not in {commands_file}: was the build configured with the stand-in game?")


def link_command(build_directory: Path) -> list[str]:
    """The command that linked the program, as Ninja runs it, as a list."""
    commands = subprocess.run(
        ["ninja", "-C", str(build_directory), "-t", "commands", PROGRAM_TARGET],
        check=True,
        capture_output=True,
        text=True,
    ).stdout.splitlines()
    if not commands:
        raise KitError("Ninja has no commands for the program")
    # CMake wraps a link in ": && ... && :" and may add steps after it with "&&".
    for part in commands[-1].split(" && "):
        words = shlex.split(part)
        if "-o" in words:
            if any(word.startswith("@") for word in words):
                raise KitError("the link command uses a response file; build the kit on Linux or macOS")
            return words
    raise KitError(f"no link in the last command of the program: {commands[-1][:200]}")


class PathRewriter:
    """Replaces the paths of this computer in a command by the recipe's placeholders."""

    def __init__(self, kit: Kit, wiicompiled_include: Path, dropped_include_directories: Sequence[Path]):
        self.kit = kit
        self.prefixes = [
            (STUB_GAME_DIRECTORY.resolve(), "{translation}"),
            (wiicompiled_include.resolve(), "{kit}/include"),
        ]
        self.dropped = [directory.resolve() for directory in dropped_include_directories]

    def directory(self, path: Path) -> str | None:
        """The placeholder form of an include directory; None when it is to be dropped."""
        resolved = path.resolve()
        if resolved in self.dropped:
            return None
        for prefix, placeholder in self.prefixes:
            if resolved == prefix or prefix in resolved.parents:
                return placeholder + resolved.as_posix()[len(prefix.as_posix()) :]
        raise KitError(
            f"a command includes {path}, which the kit cannot carry (add it to the kit or drop it)"
        )


def compile_flags(command: list[str], rewriter: PathRewriter) -> list[str]:
    """The options of a compile command, without the compiler, its input, its output and the
    debugging information, with the include directories as placeholders."""
    flags: list[str] = []
    words = iter(command[1:])
    for word in words:
        if word in ("-o", "-c", "-MF", "-MT", "-MQ"):
            next(words)  # the output, the source or a dependency file: prepare_game.py's own
        elif word in ("-MD", "-MMD") or DEBUG_INFORMATION.fullmatch(word):
            continue
        elif word in ("-I", "-isystem", "-idirafter", "-iquote"):
            directory = rewriter.directory(Path(next(words)))
            if directory is not None:
                flags += [word, directory]
        elif match := re.fullmatch(r"(-I|-isystem|-idirafter|-iquote)(.+)", word):
            directory = rewriter.directory(Path(match[2]))
            if directory is not None:
                flags.append(match[1] + directory)
        else:
            flags.append(word)
    return flags


def link_arguments(command: list[str], kit: Kit) -> list[str]:
    """The options and inputs of the link command, with the game's objects, the program and
    every file as placeholders. The files of the build are copied into the kit."""
    build = kit.build_directory.resolve()
    game_objects = "CMakeFiles/translated_game.dir/"
    arguments: list[str] = []
    words = iter(command[1:])
    for word in words:
        if word == "-o":
            next(words)
            arguments += ["-o", "{output}"]
        elif word.startswith(game_objects):
            if "{game_objects}" not in arguments:
                arguments.append("{game_objects}")
        elif word.startswith("-Wl,--out-implib,"):
            continue  # an import library of the program, which nothing uses
        elif word in ("-I", "-isystem", "-idirafter", "-iquote"):
            next(words)  # the compile flags that CMake also passes to the link, where they do nothing
        elif re.fullmatch(r"(-I|-isystem|-idirafter|-iquote).+", word):
            continue
        elif word.startswith("-") or not LINKED_FILE.search(word):
            if Path(word).is_absolute() or word.startswith("-L"):
                raise KitError(f"the link command has {word}, which the kit cannot carry")
            arguments.append(word)
        else:
            path = Path(word) if Path(word).is_absolute() else build / word
            if any(path.resolve().is_relative_to(directory) for directory in SYSTEM_LIBRARY_DIRECTORIES):
                arguments.append(str(path))
            else:
                arguments.append(kit.library(path))
    if "{game_objects}" not in arguments:
        raise KitError("the link command has none of the stand-in game's objects")
    return arguments


def runtime_dlls(
    program: Path,
    build_directory: Path,
    runtime_file_directories: Sequence[Path],
    dll_directories: Sequence[Path],
) -> dict[str, Path]:
    """The DLLs to put beside the Windows program, by file name: every DLL of
    `runtime_file_directories` (Dawn's: some of them are loaded by name, and nothing imports them),
    and the DLLs that the program and those import, and that those import in turn, taken from the
    build or from `dll_directories`. Stops when one of them is for another processor or needs a DLL
    that is in none of those places and is not a part of Windows: the game would not start."""
    runtime_files: dict[str, Path] = {}
    available: dict[str, Path] = {}
    ambiguous: dict[str, list[Path]] = {}  # names of which the build has different DLLs
    for dll in sorted(build_directory.rglob("*")):
        if dll.suffix.lower() == ".dll" and dll.is_file():
            name = dll.name.lower()
            if name in available and available[name].read_bytes() != dll.read_bytes():
                ambiguous.setdefault(name, [available[name]]).append(dll)
            available.setdefault(name, dll)
    for folders, carried_whole in ((dll_directories, False), (runtime_file_directories, True)):
        for folder in folders:
            if not Path(folder).is_dir():
                raise KitError(f"{folder} is not a folder")
            for name, dll in dlls_in(Path(folder)).items():
                available[name] = dll
                ambiguous.pop(name, None)  # a DLL given wins over those of the build
                if carried_whole:
                    runtime_files[name] = dll
    try:
        runtime_files.update(needed_dlls([program, *runtime_files.values()], available))
    except WindowsBinaryError as problem:
        raise KitError(f"the game would not start on a player's computer: {problem}") from None
    if used_but_ambiguous := sorted(ambiguous.keys() & runtime_files.keys()):
        name = used_but_ambiguous[0]
        raise KitError(
            f"the build has different {name}, and it is not known which the game uses: {ambiguous[name]}"
        )
    return {path.name: path for path in runtime_files.values()}


def make_kit(
    build_directory: Path,
    wiicompiled_directory: Path,
    out: Path,
    dropped_include_directories: Sequence[Path],
    runtime_file_directories: Sequence[Path] = (),
    dll_directories: Sequence[Path] = (),
) -> dict:
    system, processor = target_system(build_directory)
    executable = PROGRAM_TARGET + (".exe" if system == "Windows" else "")
    if not (build_directory / executable).is_file():
        raise KitError(
            f"{build_directory / executable} is missing: build the runtime with the stand-in game first"
        )
    wiicompiled_include = wiicompiled_directory / "runtime" / "include"
    if not (wiicompiled_include / "guest_abi").is_dir():
        raise KitError(f"{wiicompiled_directory} is not a WiiCompiled checkout")
    if not SYMBOL_HEADER.is_file():
        raise KitError(f"{SYMBOL_HEADER} is missing: run tools/export_mod_symbols.py")

    if out.exists():
        shutil.rmtree(out)
    for folder in ("lib", "include", "bin"):
        (out / folder).mkdir(parents=True)
    kit = Kit(out, build_directory)
    rewriter = PathRewriter(kit, wiicompiled_include, dropped_include_directories)

    compile_game = compile_command(build_directory, STUB_GAME_DIRECTORY / "data_sections_init.cpp")
    # On Windows the build assembles a copy of the blobs converted for COFF (runtime/CMakeLists.txt).
    blobs = STUB_GAME_DIRECTORY / "data_sections_init_blobs.S"
    if system == "Windows":
        blobs = build_directory / "data_sections_init_blobs_coff.S"
    assemble_game = compile_command(build_directory, blobs)
    link = link_command(build_directory)

    shutil.copytree(wiicompiled_include, out / "include", dirs_exist_ok=True)
    shutil.copy2(SYMBOL_HEADER, out / "include" / SYMBOL_HEADER.name)
    dlls: dict[str, Path] = {}
    if system == "Windows":
        dlls = runtime_dlls(
            build_directory / executable, build_directory, runtime_file_directories, dll_directories
        )
    elif runtime_file_directories or dll_directories:
        raise KitError("--runtime-files-directory and --dll-directory are for builds for Windows")
    runtime_files = sorted(dlls)
    for name in runtime_files:
        shutil.copy2(dlls[name], out / "bin" / name)

    recipe = {
        "format": RECIPE_FORMAT,
        "system": system,
        "processor": processor,
        "executable": executable,
        "cxx_compiler": Path(compile_game[0]).name,
        "c_compiler": Path(assemble_game[0]).name,
        "compile": compile_flags(compile_game, rewriter),
        "assemble": compile_flags(assemble_game, rewriter),
        # COFF objects (Windows) name the read-only section differently and have no stack note;
        # runtime/CMakeLists.txt converts the translated data blobs the same way.
        "coff_sections": system == "Windows",
        "link": link_arguments(link, kit),
        "runtime_files": runtime_files,
        "mods": [mod for mod in cache_value(build_directory, "NATIVE_MODS").split(";") if mod],
        "symbol_header": "{kit}/include/" + SYMBOL_HEADER.name,
    }
    (out / "recipe.json").write_text(json.dumps(recipe, indent=2) + "\n", encoding="utf-8")
    return recipe


def parse_arguments(argv: Sequence[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument(
        "--build-directory",
        type=Path,
        required=True,
        help="a build of runtime/ with -DTRANSLATED_GAME_DIR=runtime/kit/stub_game",
    )
    parser.add_argument(
        "--wiicompiled-directory", type=Path, required=True, help="the WiiCompiled checkout it used"
    )
    parser.add_argument("--out", type=Path, required=True, help="the kit's folder (replaced)")
    parser.add_argument(
        "--drop-include-directory",
        type=Path,
        action="append",
        default=[],
        help="an include directory that only the computer that built the kit needs "
        "(the case shims of a cross build); repeat for several",
    )
    parser.add_argument(
        "--runtime-files-directory",
        type=Path,
        action="append",
        default=[],
        help="a folder whose DLLs all go beside the program (Dawn's bin/); repeat for several",
    )
    parser.add_argument(
        "--dll-directory",
        type=Path,
        action="append",
        default=[],
        help="a folder from which only the DLLs that the program and its runtime files import go beside "
        "it (the Visual C++ runtime); repeat for several",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    try:
        recipe = make_kit(
            arguments.build_directory,
            arguments.wiicompiled_directory,
            arguments.out,
            arguments.drop_include_directory,
            arguments.runtime_files_directory,
            arguments.dll_directory,
        )
    except (KitError, subprocess.CalledProcessError, OSError) as problem:
        print(f"error: {problem}", file=sys.stderr)
        return 2
    libraries = len(list((arguments.out / "lib").iterdir()))
    mods = ", ".join(recipe["mods"]) or "none"
    target = f"{recipe['system']} {recipe['processor']}"
    print(f"kit for {target} in {arguments.out}: {libraries} files, mods {mods}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
