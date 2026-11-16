#!/usr/bin/env python3
"""Assemble the Android package: what a computer package needs to make the game's Android app,
without anything of the game.

Arma el paquete de Android: lo que un paquete de computadora necesita para hacer la app de Android
del juego, sin nada del juego.

The app is made on the player's computer (tools/make_android_app.py), by the Windows or Linux
package, which already has the translator, Python and a clang that also compiles for Android. This
package adds what is only for Android, laid out so that unpacking it in the computer package's
folder puts it in place:

    builder/android/kit/        the runtime compiled for Android with the NDK (the kit,
                                tools/make_runtime_kit.py), and in kit/sysroot the NDK's sysroot
                                trimmed to ARM64 and the kit's Android version: the C library's and
                                the C++ library's headers and libraries the game is compiled and
                                linked with
    builder/android/shell.apk   the app without the game, built with Gradle (android/), unsigned
    README-android.txt, licenses/android/

It refuses to finish when a file looks like game data, when the kit could hold the translated game
or is not for Android on ARM64, or when the shell is signed or already has a game.

Usage / Uso:
    python3 tools/package_native_android.py --kit K --ndk-sysroot S --shell A --output-directory O \\
        [--archive Z] [--package-version V] [--version NAME=VALUE ...] [--licenses-directory D]
"""

import argparse
import json
import re
import shutil
import sys
import zipfile
from collections.abc import Sequence
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from package_native_unix import copy_keeping_links  # noqa: E402
from package_native_windows import (  # noqa: E402
    PACKAGE_FILES,
    check_kit,
    check_no_game_data,
    copy_file,
    key_value,
)
from package_windows import PackageError  # noqa: E402

TRIPLE = "aarch64-linux-android"
# The other processors' folders of the NDK's sysroot, which an ARM64 app does not use.
OTHER_TRIPLES = (
    "arm-linux-androideabi",
    "i686-linux-android",
    "x86_64-linux-android",
    "riscv64-linux-android",
)
API_LEVEL = re.compile(r"--target=aarch64-[a-z]*-?linux-android(\d+)")


@dataclass(frozen=True)
class Inputs:
    kit: Path
    ndk_sysroot: Path
    shell: Path
    output_directory: Path
    package_version: str = "dev"
    versions: dict[str, str] = field(default_factory=dict)
    licenses_directory: Path | None = None


def api_level(recipe: dict) -> str:
    """The Android version the kit was compiled for, as its compile command says (--target=...26)."""
    for word in recipe["compile"]:
        if found := API_LEVEL.fullmatch(word):
            return found[1]
    raise PackageError("the kit's recipe does not say its Android version (--target=aarch64-...-android<N>)")


# What of the NDK's sysroot is left out: the other processors' headers and libraries, and the
# libraries of every Android version (the kit's own is copied after).
SYSROOT_EXCLUDED = (
    *(f"usr/include/{triple}/*" for triple in OTHER_TRIPLES),
    *(f"usr/lib/{triple}/*" for triple in OTHER_TRIPLES),
    f"usr/lib/{TRIPLE}/[0-9]*",
)


def add_sysroot(ndk_sysroot: Path, destination: Path, level: str) -> None:
    copy_keeping_links(ndk_sysroot, destination, SYSROOT_EXCLUDED)
    copy_keeping_links(
        ndk_sysroot / "usr" / "lib" / TRIPLE / level, destination / "usr" / "lib" / TRIPLE / level
    )
    for needed in (
        f"usr/lib/{TRIPLE}/{level}/crtbegin_so.o",
        f"usr/lib/{TRIPLE}/libc++_static.a",
        "usr/include/stdio.h",
    ):
        if not (destination / needed).exists():
            raise PackageError(f"the NDK's sysroot has no {needed}: is {ndk_sysroot} an NDK's sysroot?")


def check_shell(shell: Path) -> None:
    try:
        with zipfile.ZipFile(shell) as app:
            names = set(app.namelist())
    except (OSError, zipfile.BadZipFile) as problem:
        raise PackageError(f"{shell} is not an app: {problem}") from None
    for needed in ("AndroidManifest.xml", "classes.dex", "resources.arsc"):
        if needed not in names:
            raise PackageError(f"{shell} has no {needed}: is it the shell app built with Gradle?")
    if any(name.startswith("lib/") for name in names):
        raise PackageError(f"{shell} already has native libraries: the game goes in on the player's computer")
    if any(name.startswith("META-INF/") and name.upper().endswith((".SF", ".RSA", ".EC")) for name in names):
        raise PackageError(f"{shell} is signed: the player's computer signs the app")


def assemble(inputs: Inputs) -> Path:
    recipe = check_kit(inputs.kit, "Android")
    if recipe.get("processor") != "aarch64":
        raise PackageError(f"the kit is for {recipe.get('processor')}, and the app for ARM64")
    if not any(word.startswith("--sysroot={kit}/sysroot") for word in recipe["compile"]):
        raise PackageError(
            "the kit's recipe does not compile with the kit's sysroot (make_runtime_kit --sysroot)"
        )
    level = api_level(recipe)
    check_shell(inputs.shell)
    package = inputs.output_directory / "HarvestMoonMM-android"
    if package.exists():
        shutil.rmtree(package)
    android = package / "builder" / "android"
    copy_keeping_links(inputs.kit, android / "kit")
    add_sysroot(inputs.ndk_sysroot, android / "kit" / "sysroot", level)
    copy_file(inputs.shell, android / "shell.apk")
    copy_file(PACKAGE_FILES / "README-android.txt", package / "README-android.txt")
    if inputs.licenses_directory is not None:
        copy_keeping_links(inputs.licenses_directory, package / "licenses" / "android")
    versions = [f"{name}: {value}" for name, value in sorted(inputs.versions.items())]
    versions += ["processor: arm64", f"Android needed: API {level} or newer"]
    versions.append(f"mods: {', '.join(recipe.get('mods', [])) or 'none'}")
    (android / "VERSIONS.txt").write_text("\n".join(versions) + "\n", encoding="utf-8")
    (android / "package.json").write_text(
        json.dumps({"package_version": inputs.package_version, "api_level": int(level)}, indent=2) + "\n",
        encoding="utf-8",
    )
    check_no_game_data(package)
    return package


def write_archive(package: Path, archive_path: Path) -> None:
    """A zip whose top is the computer package's folder: unpacked there, it puts builder/android in
    place. Links are stored as the files they point at (the NDK's sysroot has few, and Windows
    cannot make them)."""
    archive_path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for path in sorted(package.rglob("*")):
            if path.is_file():
                archive.write(path, path.relative_to(package).as_posix())


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--kit", type=Path, required=True, help="the runtime kit for Android")
    parser.add_argument(
        "--ndk-sysroot", type=Path, required=True, help="the NDK's sysroot the kit was built with"
    )
    parser.add_argument("--shell", type=Path, required=True, help="the app without the game, unsigned")
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument("--archive", type=Path, help="also write the package as this zip")
    parser.add_argument("--package-version", default="dev")
    parser.add_argument(
        "--version", type=key_value, action="append", default=[], help="NAME=VALUE for VERSIONS.txt"
    )
    parser.add_argument("--licenses-directory", type=Path, help="a folder of license texts")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = make_parser().parse_args(argv)
    inputs = Inputs(
        kit=arguments.kit,
        ndk_sysroot=arguments.ndk_sysroot,
        shell=arguments.shell,
        output_directory=arguments.output_directory,
        package_version=arguments.package_version,
        versions=dict(arguments.version),
        licenses_directory=arguments.licenses_directory,
    )
    try:
        package = assemble(inputs)
        if arguments.archive:
            write_archive(package, arguments.archive)
    except (PackageError, OSError, KeyError, json.JSONDecodeError) as problem:
        print(f"error: {problem}", file=sys.stderr)
        return 2
    size = sum(path.stat().st_size for path in package.rglob("*") if path.is_file() and not path.is_symlink())
    print(
        f"package in {package}: {size / 2**20:.0f} MB"
        + (f", {arguments.archive}" if arguments.archive else "")
    )
    return 0


if __name__ == "__main__":
    sys.exit(main())
