#!/usr/bin/env python3
"""Prepare the game on the player's computer: the native program, made from the player's own disc.

Prepara el juego en la computadora del jugador: el programa nativo, hecho con el disco del propio
jugador.

A package of the native version carries no game: it carries the runtime already compiled (the
kit, tools/make_runtime_kit.py), WiiCompiled's translator, a compiler, and this. The launcher
runs it the first time, and again after the package is updated. Its steps are the same as
scripts/build_native_runtime.sh, with the kit in place of compiling the runtime:

  1. disc       a compressed image (RVZ and the like) is converted to ISO with nodtool; the
                image is checked (Harvest Moon: Magical Melody, USA, G4AEE9 revision 0, and
                the SHA-1 of its main.dol) and main.dol is read out of it
  2. translate  the translator turns main.dol into C++ (tools/write_translator_project.py
                writes its project)
  3. adapt      the farm's crop grid (tools/size_crop_grid.py) and the places where the mods
                act (tools/insert_mod_hooks.py)
  4. compile    the translated game, with the kit's commands, on every processor
  5. link       the game with the kit's runtime, into the program
  6. finish     the files the program needs beside it, and prepared.json

What it prints is for the launcher, which shows it to the player: a line that starts with "@"
is a step of the protocol below, and any other line is detail for the log.

    @stage <name> <number> <count>   a step starts (the names above)
    @progress <done> <total>         how far the step is
    @error <reason> <text>           what went wrong; the program then ends with 1
    @done <program>                  the program is ready

The reasons of @error, which the launcher explains to the player in their language:
not_a_disc, compressed (no nodtool to convert it), wrong_game, wrong_version, modified_game,
tool_failed (the text says which and why) and setup (the package is not complete).

Un paquete de la versión nativa no lleva el juego: lleva el runtime ya compilado, el traductor de
WiiCompiled, un compilador y esto. El launcher lo corre la primera vez y otra vez cuando se
actualiza el paquete. Lo que imprime es para el launcher: las líneas que empiezan con "@" son
pasos del protocolo de arriba y las demás son detalle para el registro.

Usage / Uso:
    python3 tools/prepare_game.py --disc <image> --kit <kit> --translator <Translator.Cli> \\
        --out <folder> [--toolchain <llvm-mingw>] [--nodtool <nodtool>] [--jobs <n>]
"""

import argparse
import hashlib
import json
import os
import re
import shutil
import subprocess
import sys
import threading
import time
from collections.abc import Callable, Sequence
from concurrent.futures import FIRST_EXCEPTION, ThreadPoolExecutor, wait
from datetime import UTC, datetime
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from check_disc import EXPECTED_DOL_SHA1, EXPECTED_REVISION  # noqa: E402
from project import GAME_ID, REPOSITORY_ROOT  # noqa: E402

TOOLS_DIRECTORY = Path(__file__).resolve().parent
NATIVE_SOURCES = REPOSITORY_ROOT / "runtime" / "src"
SYMBOLS = REPOSITORY_ROOT / "config" / GAME_ID / "symbols.txt"
RECIPE_FORMAT = 1  # the one tools/make_runtime_kit.py writes

STAGES = ("disc", "translate", "adapt", "compile", "link", "finish")
PREPARED_FILE = "prepared.json"
CONVERTED_DISC = Path("disc") / f"{GAME_ID}.iso"

# The disc header (YAGCD 13.1) and a DOL's header: the file offsets of its 18 sections, then
# their addresses, then their sizes.
DISC_HEADER_SIZE = 0x440
GAME_ID_FIELD = slice(0x00, 0x06)
REVISION_OFFSET = 0x07
DISC_MAGIC_FIELD = slice(0x1C, 0x20)
GAMECUBE_DISC_MAGIC = bytes.fromhex("C2339F3D")
DOL_OFFSET_FIELD = slice(0x420, 0x424)
DOL_SECTION_COUNT = 18
DOL_SIZES_OFFSET = 0x90
DOL_HEADER_SIZE = 0x100
# A whole GameCube disc, which is what nodtool writes when it converts one to ISO.
GAMECUBE_DISC_SIZE = 1_459_978_240
# The first bytes of the compressed images that nodtool converts (as runtime/src/game_disc.cpp).
COMPRESSED_FORMATS = {
    b"RVZ\x01": "RVZ",
    b"WIA\x01": "WIA",
    b"CISO": "CISO",
    bytes.fromhex("01C00BB1"): "GCZ",
    b"WBFS": "WBFS",
}
# How many functions the translator writes for this game, to show how far it is (an estimate:
# a newer translator may write a few more or less).
TRANSLATED_FUNCTIONS = 4496
POLL_SECONDS = 0.5


class PreparationError(Exception):
    """A step failed; `reason` is one of the @error reasons of the module's docstring."""

    def __init__(self, reason: str, text: str):
        super().__init__(text)
        self.reason = reason


def say(line: str) -> None:
    print(line, flush=True)


def stage(name: str) -> None:
    say(f"@stage {name} {STAGES.index(name) + 1} {len(STAGES)}")


def progress(done: int, total: int) -> None:
    say(f"@progress {min(done, total)} {total}")


# -- Running the tools ----------------------------------------------------------------------------


def run(command: Sequence[str | Path], what: str, poll: Callable[[], None] | None = None) -> None:
    """Runs a tool, passing on what it prints as detail; `poll` is called every half second
    while it runs, to tell how far it is. A tool that fails stops the preparation."""
    say(f"> {' '.join(str(word) for word in command)}")
    process = subprocess.Popen(
        [str(word) for word in command],
        stdout=subprocess.PIPE,
        stderr=subprocess.STDOUT,
        text=True,
        encoding="utf-8",
        errors="replace",
    )
    last_lines: list[str] = []

    def forward() -> None:
        assert process.stdout is not None
        for line in process.stdout:
            line = line.rstrip("\n")
            # A line of a tool that starts with "@" must not look like a step of the protocol.
            say(" " + line if line.startswith("@") else line)
            last_lines.append(line)
            del last_lines[:-20]

    reader = threading.Thread(target=forward, daemon=True)
    reader.start()
    while process.poll() is None:
        if poll is not None:
            poll()
        time.sleep(POLL_SECONDS)
    reader.join()
    if process.returncode != 0:
        detail = " / ".join(line.strip() for line in last_lines[-3:] if line.strip())
        raise PreparationError("tool_failed", f"{what} failed (exit code {process.returncode}): {detail}")


def python_tool(name: str, *arguments: str | Path) -> list[str | Path]:
    return [sys.executable, TOOLS_DIRECTORY / name, *arguments]


def translator_command(translator: Path, *arguments: str | Path) -> list[str | Path]:
    """The translator as a program of its own (a package's), or as a .NET assembly (a build's)."""
    if translator.suffix == ".dll":
        return ["dotnet", translator, *arguments]
    return [translator, *arguments]


# -- The disc -------------------------------------------------------------------------------------


def read_part(path: Path, offset: int, length: int) -> bytes:
    with path.open("rb") as file:
        file.seek(offset)
        data = file.read(length)
    if len(data) != length:
        raise PreparationError(
            "not_a_disc", f"{path} ends before offset {offset + length}: it is not a whole disc"
        )
    return data


def compressed_format(path: Path) -> str | None:
    """The name of the compressed format of an image, or None for a plain one."""
    with path.open("rb") as file:
        start = file.read(4)
    return COMPRESSED_FORMATS.get(start)


def convert_to_iso(image: Path, out: Path, nodtool: Path | None) -> Path:
    """The plain ISO of a compressed image, converted with nodtool into the output folder."""
    name = compressed_format(image)
    if nodtool is None:
        raise PreparationError(
            "compressed", f"{image} is a compressed image ({name}), and there is no nodtool"
        )
    iso = out / CONVERTED_DISC
    iso.parent.mkdir(parents=True, exist_ok=True)
    # nodtool chooses the format by the extension, so the unfinished file ends in .iso too.
    partial = iso.with_name(f"{GAME_ID}.unfinished.iso")
    partial.unlink(missing_ok=True)
    say(f"converting {image.name} ({name}) to ISO in {iso}")

    def written() -> None:
        progress(partial.stat().st_size if partial.exists() else 0, GAMECUBE_DISC_SIZE)

    run([nodtool, "convert", image, partial], "nodtool (converting the disc)", poll=written)
    partial.replace(iso)
    return iso


def dol_size(header: bytes) -> int:
    end = DOL_HEADER_SIZE
    for section in range(DOL_SECTION_COUNT):
        offset = int.from_bytes(header[section * 4 : section * 4 + 4], "big")
        size = int.from_bytes(
            header[DOL_SIZES_OFFSET + section * 4 : DOL_SIZES_OFFSET + section * 4 + 4], "big"
        )
        if size:
            end = max(end, offset + size)
    return end


def check_disc(image: Path) -> bytes:
    """main.dol of the image, once the image is the disc this port is made for."""
    header = read_part(image, 0, DISC_HEADER_SIZE)
    if header[DISC_MAGIC_FIELD] != GAMECUBE_DISC_MAGIC:
        raise PreparationError("not_a_disc", f"{image} is not a GameCube disc image")
    game_id = header[GAME_ID_FIELD].decode("ascii", "replace")
    title = header[0x20:0x60].split(b"\0", 1)[0].decode("ascii", "replace").strip()
    say(f"disc: {title} ({game_id}, revision {header[REVISION_OFFSET]})")
    if game_id != GAME_ID:
        reason = "wrong_version" if game_id[:3] == GAME_ID[:3] else "wrong_game"
        raise PreparationError(reason, f"the disc is {title} ({game_id}); this port is for {GAME_ID}")
    if header[REVISION_OFFSET] != EXPECTED_REVISION:
        raise PreparationError(
            "wrong_version", f"the disc is revision {header[REVISION_OFFSET]}; this port is for revision 0"
        )
    dol_offset = int.from_bytes(header[DOL_OFFSET_FIELD], "big")
    dol = read_part(image, dol_offset, dol_size(read_part(image, dol_offset, DOL_HEADER_SIZE)))
    sha1 = hashlib.sha1(dol).hexdigest()
    say(f"main.dol: {len(dol)} bytes, SHA-1 {sha1}")
    if sha1 != EXPECTED_DOL_SHA1:
        raise PreparationError(
            "modified_game", f"main.dol has the SHA-1 {sha1}; the port expects {EXPECTED_DOL_SHA1}"
        )
    return dol


def disc_image(disc: Path, out: Path, nodtool: Path | None) -> tuple[Path, bytes]:
    """The plain image the game will read (the player's, or one converted from it) and its main.dol."""
    if not disc.is_file():
        raise PreparationError("not_a_disc", f"there is no file at {disc}")
    image = convert_to_iso(disc, out, nodtool) if compressed_format(disc) else disc
    return image, check_disc(image)


# -- Translating and adapting ---------------------------------------------------------------------


def translate(dol: Path, translation: Path, translator: Path) -> None:
    game = translation / "game"
    # It prints the entry point, where the translation starts.
    written = subprocess.run(
        [
            str(word)
            for word in python_tool(
                "write_translator_project.py",
                "--dol",
                dol,
                "--symbols",
                SYMBOLS,
                "--natives",
                NATIVE_SOURCES,
                "--out",
                translation,
            )
        ],
        capture_output=True,
        text=True,
    )
    if written.returncode != 0:
        raise PreparationError(
            "tool_failed", f"the translator's project could not be written: {written.stderr.strip()}"
        )
    entry_point = written.stdout.strip()
    say(f"entry point: {entry_point}")
    project = translation / "project.yml"
    functions = game / "functions"

    def translated() -> None:
        count = sum(1 for _ in functions.glob("*.cpp")) if functions.is_dir() else 0
        progress(count, TRANSLATED_FUNCTIONS)

    run(
        translator_command(
            translator,
            "translate-recursive",
            entry_point,
            "--project",
            project,
            "--outdir",
            functions,
            "--output-metadata",
            game / "generated" / "base_translation_output.json",
        ),
        "the translator",
        poll=translated,
    )
    run(translator_command(translator, "generate-data-init", "--project", project), "the translator (data)")
    run(
        translator_command(
            translator,
            "emit-build-shards",
            "--project",
            project,
            "--base-metadata",
            game / "generated" / "base_translation_output.json",
            "--base-functions-dir",
            functions,
            "--native-source-dir",
            NATIVE_SOURCES,
            "--out",
            game / "build_shards",
        ),
        "the translator (shards)",
    )


def adapt(game: Path, recipe: dict, kit: Path, c_compiler: Path) -> None:
    run(python_tool("size_crop_grid.py", "--translation", game), "the crop grid")
    progress(1, 2)
    mods = [argument for mod in recipe["mods"] for argument in ("--mod", mod)]
    symbol_header = recipe["symbol_header"].replace("{kit}", kit.as_posix())
    run(
        python_tool(
            "insert_mod_hooks.py",
            "--translation",
            game,
            *mods,
            "--compiler",
            c_compiler,
            "--symbol-header",
            symbol_header,
        ),
        "the mod hooks",
    )
    progress(2, 2)


# -- Compiling and linking ------------------------------------------------------------------------


def expand(words: Sequence[str], kit: Path, translation: Path) -> list[str]:
    return [
        word.replace("{kit}", kit.as_posix()).replace("{translation}", translation.as_posix())
        for word in words
    ]


def coff_assembly(text: str) -> str:
    """The translated data blobs for a Windows object, as runtime/CMakeLists.txt converts them, with
    the paths of .incbin in forward slashes (a backslash would be an escape in an assembler string)."""
    text = text.replace('.section .rodata,"a",@progbits', '.section .rdata,"dr"')
    text = text.replace('.section .note.GNU-stack,"",@progbits', "")
    return re.sub(
        r'^(\s*\.incbin\s+")([^"]*)(")', lambda m: m[1] + m[2].replace("\\", "/") + m[3], text, flags=re.M
    )


def game_sources(game: Path) -> list[Path]:
    """The translated game's C++, as runtime/CMakeLists.txt lists it, biggest first so that the
    longest compiles start early."""
    shards = game / "build_shards"
    sources = [
        *shards.glob("base_common/*.cpp"),
        *shards.glob("base_registration/*.cpp"),
        *shards.glob("base_dispatch/*.cpp"),
        game / "data_sections_init.cpp",
    ]
    return sorted(sources, key=lambda path: path.stat().st_size, reverse=True)


def compile_game(
    game: Path, objects: Path, recipe: dict, kit: Path, translation: Path, cxx: Path, cc: Path, jobs: int
) -> list[Path]:
    objects.mkdir(parents=True, exist_ok=True)
    compile_flags = expand(recipe["compile"], kit, translation)
    assemble_flags = expand(recipe["assemble"], kit, translation)
    suffix = ".obj" if recipe["system"] == "Windows" else ".o"

    blobs = game / "data_sections_init_blobs.S"
    if recipe["coff_sections"]:
        converted = objects / "data_sections_init_blobs_coff.S"
        converted.write_text(coff_assembly(blobs.read_text(encoding="utf-8")), encoding="utf-8")
        blobs = converted
    commands = [
        [cxx, *compile_flags, "-c", source, "-o", objects / (source.stem + suffix)]
        for source in game_sources(game)
    ]
    commands.append([cc, *assemble_flags, "-c", blobs, "-o", objects / ("data_sections_init_blobs" + suffix)])

    done = 0
    lock = threading.Lock()
    progress(0, len(commands))

    def compile_one(command: list) -> None:
        nonlocal done
        result = subprocess.run(
            [str(word) for word in command], capture_output=True, text=True, errors="replace"
        )
        if result.returncode != 0:
            say(result.stdout + result.stderr)
            raise PreparationError("tool_failed", f"the compiler failed on {Path(command[-3]).name}")
        with lock:
            done += 1
            progress(done, len(commands))

    with ThreadPoolExecutor(max_workers=jobs) as pool:
        futures = [pool.submit(compile_one, command) for command in commands]
        finished, _ = wait(futures, return_when=FIRST_EXCEPTION)
        for future in finished:
            if future.exception() is not None:
                for other in futures:
                    other.cancel()
                raise future.exception()
    return [Path(command[-1]) for command in commands]


def response_file_word(word: str) -> str:
    """A word of a response file as clang reads it for these targets (GNU style): quoted, with
    backslashes and quotes escaped. Paths are written with forward slashes before this."""
    return '"' + word.replace("\\", "\\\\").replace('"', '\\"') + '"'


def link_game(objects: list[Path], program: Path, recipe: dict, kit: Path, cxx: Path, work: Path) -> None:
    arguments: list[str] = []
    for word in recipe["link"]:
        if word == "{game_objects}":
            arguments += [path.as_posix() for path in objects]
        elif word == "{output}":
            arguments.append(program.as_posix())
        else:
            arguments.append(word.replace("{kit}", kit.as_posix()))
    response_file = work / "link.rsp"
    response_file.write_text(
        "\n".join(response_file_word(word) for word in arguments) + "\n", encoding="utf-8"
    )
    run([cxx, f"@{response_file}"], "the linker")


# -- The whole preparation ------------------------------------------------------------------------


def find_compiler(name: str, toolchain: Path | None) -> Path:
    """A compiler of the toolchain's bin/ (a package's), or of the PATH (a developer's)."""
    if toolchain is not None:
        for candidate in (toolchain / "bin" / name, toolchain / "bin" / f"{name}.exe"):
            if candidate.is_file():
                return candidate
        raise PreparationError("setup", f"{name} is not in {toolchain / 'bin'}")
    found = shutil.which(name)
    if found is None:
        raise PreparationError("setup", f"{name} is not installed")
    return Path(found)


def sha256_of(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def prepare(arguments: argparse.Namespace) -> Path:
    kit = arguments.kit.resolve()
    out = arguments.out.resolve()
    work = (arguments.work or out / "work").resolve()
    recipe_file = kit / "recipe.json"
    if not recipe_file.is_file():
        raise PreparationError("setup", f"there is no runtime kit in {kit}")
    recipe = json.loads(recipe_file.read_text(encoding="utf-8"))
    if recipe.get("format") != RECIPE_FORMAT:
        raise PreparationError(
            "setup", f"the kit's recipe has format {recipe.get('format')}, not {RECIPE_FORMAT}"
        )
    cxx = find_compiler(recipe["cxx_compiler"], arguments.toolchain)
    cc = find_compiler(recipe["c_compiler"], arguments.toolchain)
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    out.mkdir(parents=True, exist_ok=True)

    stage("disc")
    image, dol = disc_image(arguments.disc.resolve(), out, arguments.nodtool)
    dol_path = work / "main.dol"
    dol_path.write_bytes(dol)

    stage("translate")
    translation = work / "translation"
    translate(dol_path, translation, arguments.translator.resolve())
    game = translation / "game"

    stage("adapt")
    adapt(game, recipe, kit, cc)

    stage("compile")
    objects = compile_game(game, work / "objects", recipe, kit, translation / "game", cxx, cc, arguments.jobs)

    stage("link")
    built = work / recipe["executable"]
    link_game(objects, built, recipe, kit, cxx, work)
    progress(1, 1)

    stage("finish")
    for name in recipe["runtime_files"]:
        shutil.copy2(kit / "bin" / name, out / name)
    program = out / recipe["executable"]
    built.replace(program)
    prepared = {
        "game": GAME_ID,
        "disc": str(image),
        "source_disc": str(arguments.disc.resolve()),
        "dol_sha1": EXPECTED_DOL_SHA1,
        "program": program.name,
        "mods": recipe["mods"],
        "recipe_sha256": sha256_of(recipe_file),
        "package_version": arguments.package_version,
        "prepared_at": datetime.now(UTC).isoformat(timespec="seconds"),
    }
    (out / PREPARED_FILE).write_text(json.dumps(prepared, indent=2) + "\n", encoding="utf-8")
    if not arguments.keep_work:
        shutil.rmtree(work, ignore_errors=True)
    progress(1, 1)
    return program


def parse_arguments(argv: Sequence[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument(
        "--disc", type=Path, required=True, help="the player's disc image (.iso, .gcm, .rvz, ...)"
    )
    parser.add_argument("--kit", type=Path, required=True, help="the runtime kit (tools/make_runtime_kit.py)")
    parser.add_argument(
        "--translator", type=Path, required=True, help="Translator.Cli(.exe), or its .dll with dotnet"
    )
    parser.add_argument("--out", type=Path, required=True, help="where the program and prepared.json go")
    parser.add_argument(
        "--work", type=Path, help="the folder for the translation and the objects (default: OUT/work)"
    )
    parser.add_argument(
        "--toolchain", type=Path, help="a portable compiler (llvm-mingw); default: the PATH's"
    )
    parser.add_argument("--nodtool", type=Path, help="nodtool, to convert compressed images to ISO")
    parser.add_argument(
        "--jobs", type=int, default=os.cpu_count() or 1, help="compilers at once (default: all CPUs)"
    )
    parser.add_argument("--package-version", default="", help="written into prepared.json, for the launcher")
    parser.add_argument("--keep-work", action="store_true", help="keep the work folder (it is ~300 MB)")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    try:
        program = prepare(arguments)
    except PreparationError as problem:
        say(f"@error {problem.reason} {problem}")
        return 1
    except (OSError, subprocess.CalledProcessError, json.JSONDecodeError, KeyError) as problem:
        say(f"@error setup {problem}")
        return 1
    say(f"@done {program}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
