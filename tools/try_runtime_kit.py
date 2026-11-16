#!/usr/bin/env python3
"""Try a runtime kit as a player's computer uses it: compile the stand-in of the translated game with
the kit's recipe and link it with the kit's runtime, into a program.

Prueba un kit del runtime como lo usa la computadora de un jugador: compila el sustituto del juego
traducido con la receta del kit y lo enlaza con el runtime del kit, en un programa.

The package workflows run this after making a kit, on the computer that made it and with the
compiler the player will use (the package's own, or Apple's on a Mac; for the Android kit, the
computer package's clang, as tools/make_android_app.py runs it), so that a recipe that
would not compile or link on a player's computer stops the workflow instead of the player. It goes
through tools/prepare_game.py's own compile and link, with runtime/kit/stub_game in place of the
game translated from a disc: no game is needed. The stand-in's data are written for the kit's
system first, as the translator writes them on that system (ELF, COFF or Mach-O).

Los workflows de los paquetes corren esto después de hacer un kit, con el compilador que va a usar
el jugador, para que una receta que no compilaría o no enlazaría lo detenga a él y no al jugador.

Usage / Uso:
    python3 tools/try_runtime_kit.py --kit <kit> --out <folder> [--toolchain <compiler folder>]
"""

import argparse
import json
import shutil
import sys
from collections.abc import Sequence
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import make_android_app  # noqa: E402
import prepare_game  # noqa: E402

STUB_GAME = Path(__file__).resolve().parent.parent / "runtime" / "kit" / "stub_game"
ELF_READ_ONLY = '.section .rodata,"a",@progbits'
ELF_STACK_NOTE = '.section .note.GNU-stack,"",@progbits'


def stand_in_for(system: str, out: Path) -> Path:
    """The stand-in game in `out`, with its data in the syntax that the translator writes on `system`."""
    game = out / "game"
    if game.exists():
        shutil.rmtree(game)
    shutil.copytree(STUB_GAME, game)
    blobs = game / "data_sections_init_blobs.S"
    if system == "Darwin":
        text = blobs.read_text(encoding="utf-8")
        blobs.write_text(text.replace(ELF_READ_ONLY, ".section __TEXT,__const").replace(ELF_STACK_NOTE, ""))
    return game


def try_kit(kit: Path, out: Path, toolchain: Path | None) -> Path:
    recipe = json.loads((kit / "recipe.json").read_text(encoding="utf-8"))
    if recipe["system"] == "Android":
        recipe = make_android_app.for_android(recipe)  # as the computer's clang compiles for a phone
    out.mkdir(parents=True, exist_ok=True)
    game = stand_in_for(recipe["system"], out)
    cxx = prepare_game.find_compiler(recipe["cxx_compiler"], toolchain)
    cc = prepare_game.find_compiler(recipe["c_compiler"], toolchain)
    objects = prepare_game.compile_game(game, out / "objects", recipe, kit.resolve(), game, cxx, cc, jobs=2)
    program = out / recipe["executable"]
    prepare_game.link_game(objects, program, recipe, kit.resolve(), cxx, out)
    if not program.is_file():
        raise prepare_game.PreparationError("tool_failed", f"the link wrote no {program}")
    return program


def main(argv: Sequence[str] | None = None) -> int:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--kit", type=Path, required=True)
    parser.add_argument("--out", type=Path, required=True, help="a folder for the objects and the program")
    parser.add_argument("--toolchain", type=Path, help="the package's compiler folder; default: the PATH's")
    arguments = parser.parse_args(argv)
    try:
        program = try_kit(arguments.kit, arguments.out, arguments.toolchain)
    except prepare_game.PreparationError as problem:
        print(f"error: {problem}", file=sys.stderr)
        return 1
    print(f"the kit compiles and links: {program} ({program.stat().st_size / 2**20:.0f} MB)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
