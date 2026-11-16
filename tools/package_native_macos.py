#!/usr/bin/env python3
"""Assemble the macOS package of the native version: the launcher as a Mac app, with everything that
prepares the game on the player's Mac inside it, without anything of the game.

Arma el paquete de macOS de la versión nativa: el launcher como app de Mac, con todo lo que prepara
el juego en la Mac del jugador adentro, sin nada del juego.

The package is a folder with the app and the texts:

    HarvestMoonMM.app/Contents/
        Info.plist          what macOS reads of the app: its name, its program, its version, the
                            oldest macOS it runs on
        MacOS/HarvestMoonMM the launcher
        Resources/builder/  the same builder/ as the Linux package (tools/package_native_unix.py):
                            the preparation's tools, the kit, the translator published for macOS,
                            Python and nodtool. SDL tells the launcher that Resources/ is its
                            folder, so it finds builder/ there.
        Resources/HarvestMoonMM.icns   the app's icon, when one is given
    recomplauncher.json, README.txt, LICENSE.txt, THIRD_PARTY_NOTICES.txt, VERSIONS.txt, licenses/

It carries no compiler: on a Mac the game is compiled with Apple's clang, from the Command Line
Tools, which the preparation asks the player to install when they are not there
(tools/prepare_game.py). The package is a .zip that keeps the links and which files are programs,
as the Finder's own zips do.

It refuses to finish when a file looks like game data, when the kit's recipe could hold the
translated game, or when one of its programs would not start on a player's Mac: when it has no
code for the package's processor, or loads a library that is neither part of macOS nor in the
package (tools/macho_binaries.py). Info.plist and VERSIONS.txt say the oldest macOS that they all
run on.

El paquete es una carpeta con la app y los textos. No lleva compilador: en una Mac el juego se
compila con el clang de Apple, de las Command Line Tools. Se niega a terminar si algún archivo
parece datos del juego o si algún programa no arrancaría en una Mac.

Usage / Uso:
    python3 tools/package_native_macos.py --architecture arm64 --launcher L --kit K --translator T \\
        --python P --nodtool N --output-directory O [--icon I] [--archive A] [--package-version V] \\
        [--version NAME=VALUE ...] [--licenses-directory D]
"""

import argparse
import json
import os
import plistlib
import re
import shutil
import stat
import sys
import zipfile
from collections.abc import Sequence
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from macho_binaries import MachOError, check_programs  # noqa: E402
from package_native_unix import PYTHON_EXCLUDED, copy_keeping_links  # noqa: E402
from package_native_windows import (  # noqa: E402
    PACKAGE_FILES,
    PREPARATION_TOOLS,
    REPOSITORY,
    REPOSITORY_FILES,
    REPOSITORY_FOLDERS,
    REPOSITORY_FOLDERS_EXCLUDED,
    TOOLS,
    check_kit,
    check_no_game_data,
    copy_file,
    key_value,
    local_imports,
    mod_files,
    write_recomplauncher_profile,
)
from package_windows import PackageError  # noqa: E402

APP_NAME = "HarvestMoonMM.app"
LAUNCHER_NAME = "HarvestMoonMM"
BUNDLE_IDENTIFIER = "io.github.rodoly.hmmmgc.launcher"  # the launcher's SDL_SetAppMetadata
ARCHITECTURES = {"arm64": "arm64", "x64": "x86_64"}  # the package's names, and CMake's
NUMERIC_VERSION = re.compile(r"v?(\d+)\.(\d+)\.(\d+)")


@dataclass(frozen=True)
class Inputs:
    architecture: str
    launcher: Path
    kit: Path
    translator: Path  # the folder of the translator published for macOS
    python: Path
    nodtool: Path
    output_directory: Path
    icon: Path | None = None  # an .icns
    package_version: str = "dev"
    versions: dict[str, str] = field(default_factory=dict)
    licenses_directory: Path | None = None


def bundle_version(package_version: str) -> str:
    """The version as macOS wants it in Info.plist (numbers only): v0.4.0 is 0.4.0, and the version of
    a run that is not a release, 0.0.0."""
    found = NUMERIC_VERSION.fullmatch(package_version)
    return ".".join(found.groups()) if found else "0.0.0"


def write_info(contents: Path, package_version: str, minimum_macos: tuple[int, int, int], icon: bool) -> None:
    info = {
        "CFBundleDevelopmentRegion": "en",
        "CFBundleDisplayName": "Harvest Moon: Magical Melody",
        "CFBundleExecutable": LAUNCHER_NAME,
        "CFBundleIdentifier": BUNDLE_IDENTIFIER,
        "CFBundleInfoDictionaryVersion": "6.0",
        "CFBundleName": "Harvest Moon MM",
        "CFBundlePackageType": "APPL",
        "CFBundleShortVersionString": bundle_version(package_version),
        "CFBundleVersion": bundle_version(package_version),
        "LSApplicationCategoryType": "public.app-category.games",
        "LSMinimumSystemVersion": ".".join(str(part) for part in minimum_macos[:2]),
        "NSHighResolutionCapable": True,
        "NSSupportsAutomaticGraphicsSwitching": True,
    }
    if icon:
        info["CFBundleIconFile"] = LAUNCHER_NAME
    with (contents / "Info.plist").open("wb") as file:
        plistlib.dump(info, file)


def add_builder(inputs: Inputs, builder: Path, recipe: dict) -> None:
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
        copy_keeping_links(REPOSITORY / relative, builder / relative, REPOSITORY_FOLDERS_EXCLUDED)
    for mod in recipe.get("mods", []):
        for source in mod_files(REPOSITORY / "mods" / mod):
            copy_file(source, builder / "mods" / mod / source.name)
    copy_keeping_links(inputs.kit, builder / "kit")
    copy_keeping_links(inputs.translator, builder / "translator")
    if not (builder / "translator" / "Translator.Cli").is_file():
        raise PackageError(f"{inputs.translator} has no Translator.Cli: publish it for macOS")
    copy_keeping_links(inputs.python, builder / "python", PYTHON_EXCLUDED)
    if not (builder / "python" / "bin" / "python3").exists():
        raise PackageError(f"{inputs.python} has no bin/python3")
    copy_file(inputs.nodtool, builder / "nodtool" / "nodtool")
    (builder / "nodtool" / "nodtool").chmod(0o755)


def assemble(inputs: Inputs) -> Path:
    if inputs.architecture not in ARCHITECTURES:
        raise PackageError(f"the architecture must be one of {', '.join(ARCHITECTURES)}")
    recipe = check_kit(inputs.kit, "Darwin")
    if recipe.get("processor") != ARCHITECTURES[inputs.architecture]:
        raise PackageError(
            f"the kit is for {recipe.get('processor')}, and the package for {inputs.architecture}"
        )
    package = inputs.output_directory / f"HarvestMoonMM-macos-{inputs.architecture}"
    if package.exists():
        shutil.rmtree(package)
    contents = package / APP_NAME / "Contents"
    builder = contents / "Resources" / "builder"

    copy_file(inputs.launcher, contents / "MacOS" / LAUNCHER_NAME)
    (contents / "MacOS" / LAUNCHER_NAME).chmod(0o755)
    add_builder(inputs, builder, recipe)
    if inputs.icon is not None:
        copy_file(inputs.icon, contents / "Resources" / f"{LAUNCHER_NAME}.icns")

    builder_programs = [
        builder / "nodtool" / "nodtool",
        (builder / "python" / "bin" / "python3").resolve(),
        builder / "translator" / "Translator.Cli",
    ]
    try:
        minimum_macos = check_programs(
            [contents / "MacOS" / LAUNCHER_NAME, *builder_programs], inputs.architecture
        )
    except MachOError as problem:
        raise PackageError(f"a program of the package would not start: {problem}") from None
    write_info(contents, inputs.package_version, minimum_macos, inputs.icon is not None)

    write_recomplauncher_profile(package, inputs.package_version)
    for name, source in (
        ("README.txt", "README-macos.txt"),
        ("THIRD_PARTY_NOTICES.txt", "THIRD_PARTY_NOTICES-macos.txt"),
    ):
        copy_file(PACKAGE_FILES / source, package / name)
    copy_file(REPOSITORY / "LICENSE", package / "LICENSE.txt")
    if inputs.licenses_directory is not None:
        copy_keeping_links(inputs.licenses_directory, package / "licenses")
    check_no_game_data(package)
    versions = [f"{name}: {value}" for name, value in sorted(inputs.versions.items())]
    versions.append(f"architecture: {inputs.architecture}")
    versions.append(f"macOS needed: {'.'.join(str(part) for part in minimum_macos[:2])} or newer")
    versions.append(f"mods: {', '.join(recipe.get('mods', [])) or 'none'}")
    (package / "VERSIONS.txt").write_text("\n".join(versions) + "\n", encoding="utf-8")
    return package


def write_archive(package: Path, archive_path: Path) -> None:
    """A .zip of the package in one folder of its name, which keeps the links and which files are
    programs (in the Unix part of each entry, as the Finder's Archive Utility reads it)."""
    archive_path.parent.mkdir(parents=True, exist_ok=True)
    with zipfile.ZipFile(archive_path, "w", zipfile.ZIP_DEFLATED, compresslevel=9) as archive:
        for folder, folders, files in os.walk(package):
            folders.sort()
            for name in sorted(folders + files):
                path = Path(folder) / name
                if path.is_dir() and not path.is_symlink():
                    continue
                entry = zipfile.ZipInfo((Path(package.name) / path.relative_to(package)).as_posix())
                entry.create_system = 3  # Unix: the high half of external_attr is the file's mode
                if path.is_symlink():
                    entry.external_attr = (stat.S_IFLNK | 0o755) << 16
                    archive.writestr(entry, os.readlink(path), zipfile.ZIP_STORED)
                else:
                    mode = 0o755 if path.stat().st_mode & 0o111 else 0o644
                    entry.external_attr = (stat.S_IFREG | mode) << 16
                    entry.compress_type = zipfile.ZIP_DEFLATED
                    archive.writestr(entry, path.read_bytes())


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--architecture", required=True, choices=sorted(ARCHITECTURES))
    parser.add_argument("--launcher", type=Path, required=True, help="the launcher built for macOS")
    parser.add_argument("--kit", type=Path, required=True, help="the runtime kit for macOS")
    parser.add_argument("--translator", type=Path, required=True, help="the translator published for macOS")
    parser.add_argument("--python", type=Path, required=True, help="an unpacked Python for macOS")
    parser.add_argument("--nodtool", type=Path, required=True, help="nodtool for macOS")
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument("--icon", type=Path, help="the app's icon, an .icns")
    parser.add_argument("--archive", type=Path, help="also write the package as this .zip")
    parser.add_argument("--package-version", default="dev", help="the launcher's version, as v0.4.0")
    parser.add_argument(
        "--version", type=key_value, action="append", default=[], help="NAME=VALUE for VERSIONS.txt"
    )
    parser.add_argument("--licenses-directory", type=Path, help="a folder of license texts for licenses/")
    return parser


def main(argv: Sequence[str] | None = None) -> int:
    arguments = make_parser().parse_args(argv)
    inputs = Inputs(
        architecture=arguments.architecture,
        launcher=arguments.launcher,
        kit=arguments.kit,
        translator=arguments.translator,
        python=arguments.python,
        nodtool=arguments.nodtool,
        output_directory=arguments.output_directory,
        icon=arguments.icon,
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
