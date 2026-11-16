#!/usr/bin/env python3
"""Make the Android app of the game on the player's computer, from the player's own disc.

Hace la app de Android del juego en la computadora del jugador, con su propio disco.

It is tools/prepare_game.py for a phone. The game is translated from the disc as for the computer,
then compiled for Android (ARM64) with the package's own compiler (llvm-mingw's clang, which also
compiles for Android) and linked with the Android runtime kit into libmain.so, which goes into the
app that the Android package brings without the game (the shell), and the app is signed with the
player's key (tools/android_apk.py). What it needs comes from the Android package, unpacked into
the computer package's builder/android:

    builder/android/kit/        the runtime compiled for Android, with the NDK's sysroot in
                                kit/sysroot (tools/package_native_android.py)
    builder/android/shell.apk   the app without the game, not signed

The steps, printed with prepare_game.py's protocol (@stage, @progress, @error, @done):

  1. disc       as prepare_game.py: a compressed image is converted to ISO, and checked
  2. translate  the same translation
  3. adapt      the same crop grid, mesh of tilled soil and mod hooks
  4. compile    the translated game, for Android, with the kit's commands
  5. link       libmain.so
  6. app        the shell with libmain.so and the player's settings.json, signed

The disc does not go into the app (it is 1.4 GB): the app asks for it the first time it opens, and
the player copies to the phone the ISO that the last line, "@disc <image>", names (the player's
own, or the one converted from an .rvz).

Es tools/prepare_game.py para un celular: el juego se traduce igual, se compila para Android con el
compilador del propio paquete, se enlaza con el kit de Android en libmain.so, que va en la app sin
el juego que trae el paquete de Android, y la app se firma con la llave del jugador.

Usage / Uso:
    python3 tools/make_android_app.py --disc <image> --android <builder/android> \\
        --translator <Translator.Cli> --out <folder> --key <key.json> [--toolchain <compiler>] \\
        [--nodtool <nodtool>] [--settings <settings.json>] [--jobs <n>]
"""

import argparse
import json
import os
import shutil
import subprocess
import sys
from collections.abc import Sequence
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

import android_apk  # noqa: E402
import prepare_game  # noqa: E402
from prepare_game import PreparationError, progress, say  # noqa: E402

STAGES = ("disc", "translate", "adapt", "compile", "link", "app")
APP_NAME = "HarvestMoonMM.apk"
LIBRARY_IN_THE_APP = "lib/arm64-v8a/libmain.so"
SETTINGS_IN_THE_APP = "assets/settings.json"
# The package's clang reads a configuration of its own for the computer (on Linux, its sysroot):
# none of it is for Android.
NO_CONFIGURATION = "--no-default-config"
ELF_READ_ONLY = '.section .rodata,"a",@progbits'
ELF_STACK_NOTE = '.section .note.GNU-stack,"",@progbits'


def stage(name: str) -> None:
    say(f"@stage {name} {STAGES.index(name) + 1} {len(STAGES)}")


def elf_assembly(text: str) -> str:
    """The translated data for an Android (ELF) object. The translator writes them for the system
    it runs on: on Windows as COFF, which is turned back into ELF here."""
    text = text.replace('.section .rdata,"dr"', ELF_READ_ONLY)
    if ELF_STACK_NOTE not in text:
        text += "\n" + ELF_STACK_NOTE + "\n"
    return text


def for_android(recipe: dict) -> dict:
    """The kit's recipe as the package's clang runs it: without the computer's configuration."""
    if recipe.get("system") != "Android":
        raise PreparationError("setup", f"the Android kit's recipe is for {recipe.get('system')}")
    return {
        **recipe,
        "compile": [NO_CONFIGURATION, *recipe["compile"]],
        "assemble": [NO_CONFIGURATION, *recipe["assemble"]],
        "link": [NO_CONFIGURATION, *recipe["link"]],
    }


def make(arguments: argparse.Namespace) -> Path:
    android = arguments.android.resolve()
    kit = android / "kit"
    shell = android / "shell.apk"
    out = arguments.out.resolve()
    work = (arguments.work or out / "android-work").resolve()
    if not (kit / "recipe.json").is_file() or not shell.is_file():
        raise PreparationError("setup", f"{android} does not have the Android package (kit/ and shell.apk)")
    if not (kit / "sysroot").is_dir():
        raise PreparationError("setup", f"the Android kit in {kit} has no sysroot")
    recipe = for_android(json.loads((kit / "recipe.json").read_text(encoding="utf-8")))
    cxx = prepare_game.find_compiler(recipe["cxx_compiler"], arguments.toolchain)
    cc = prepare_game.find_compiler(recipe["c_compiler"], arguments.toolchain)
    if work.exists():
        shutil.rmtree(work)
    work.mkdir(parents=True)
    out.mkdir(parents=True, exist_ok=True)

    stage("disc")
    image, dol = prepare_game.disc_image(arguments.disc.resolve(), out, arguments.nodtool)
    (work / "main.dol").write_bytes(dol)

    stage("translate")
    translation = work / "translation"
    prepare_game.translate(work / "main.dol", translation, arguments.translator.resolve())
    game = translation / "game"
    blobs = game / "data_sections_init_blobs.S"
    blobs.write_text(elf_assembly(blobs.read_text(encoding="utf-8")), encoding="utf-8")

    stage("adapt")
    prepare_game.adapt(game, recipe, kit, cc)

    stage("compile")
    objects = prepare_game.compile_game(game, work / "objects", recipe, kit, game, cxx, cc, arguments.jobs)

    stage("link")
    library = work / "libmain.so"
    prepare_game.link_game(objects, library, recipe, kit, cxx, work)
    progress(1, 1)

    stage("app")
    additions = {LIBRARY_IN_THE_APP: library}
    if arguments.settings is not None and arguments.settings.is_file():
        additions[SETTINGS_IN_THE_APP] = arguments.settings
    app = android_apk.make_app(shell, additions, arguments.key, out / APP_NAME)
    if not arguments.keep_work:
        shutil.rmtree(work, ignore_errors=True)
    progress(1, 1)
    say(f"@disc {image}")
    return app


def parse_arguments(argv: Sequence[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument(
        "--disc", type=Path, required=True, help="the player's disc image (.iso, .gcm, .rvz, ...)"
    )
    parser.add_argument(
        "--android", type=Path, required=True, help="the Android package (kit/ and shell.apk)"
    )
    parser.add_argument("--translator", type=Path, required=True, help="Translator.Cli(.exe), or its .dll")
    parser.add_argument("--out", type=Path, required=True, help="where the app (and a converted disc) go")
    parser.add_argument(
        "--key", type=Path, required=True, help="the player's signing key (made the first time)"
    )
    parser.add_argument("--work", type=Path, help="the folder for the translation and the objects")
    parser.add_argument("--toolchain", type=Path, help="the package's compiler; default: the PATH's")
    parser.add_argument("--nodtool", type=Path, help="nodtool, to convert compressed images to ISO")
    parser.add_argument("--settings", type=Path, help="the player's settings.json, which the app starts with")
    parser.add_argument("--jobs", type=int, default=os.cpu_count() or 1, help="compilers at once")
    parser.add_argument("--keep-work", action="store_true", help="keep the work folder")
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    try:
        app = make(arguments)
    except PreparationError as problem:
        say(f"@error {problem.reason} {problem}")
        return 1
    except android_apk.ApkError as problem:
        say(f"@error tool_failed {problem}")
        return 1
    except (OSError, subprocess.CalledProcessError, json.JSONDecodeError, KeyError) as problem:
        say(f"@error setup {problem}")
        return 1
    say(f"@done {app}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
