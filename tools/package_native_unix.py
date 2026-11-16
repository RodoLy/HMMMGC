#!/usr/bin/env python3
"""Assemble the Linux package of the native version: the launcher and everything that prepares the
game on the player's computer, without anything of the game.

Arma el paquete de Linux de la versión nativa: el launcher y todo lo que prepara el juego en la
computadora del jugador, sin nada del juego.

It is the Linux counterpart of tools/package_native_windows.py, with the same builder/ (the
preparation's tools, laid out like this repository, the kit, the translator, Python and nodtool)
and these differences:

    HarvestMoonMM       the launcher, a Linux program
    builder/clang/      the compiler: clang and lld of llvm-mingw for Linux (whose clang also
                        compiles for Linux itself), trimmed to what compiling and linking use, with
                        a sysroot (tools/make_linux_sysroot.py) in builder/clang/sysroot and a
                        configuration beside clang that points it there and at lld
                        (bin/clang.cfg, bin/clang++.cfg), so the preparation runs it as it is
    builder/translator/ the translator published for Linux with its own .NET
    README.txt, THIRD_PARTY_NOTICES.txt
                        the Linux ones (packaging/native/README-linux.txt, ...)

The package is a .tar.gz, which keeps what a zip loses on Linux: which files are programs, and the
links (python3 to python3.13, clang++ to clang, ...).

It refuses to finish, as the Windows one does, when a file looks like game data, when the kit's
recipe could hold the translated game, or when one of its programs would not start on a player's
computer: when it is for the other processor, or needs a library that is not beside it and that a
Linux desktop does not have (tools/elf_binaries.py). VERSIONS.txt says the newest glibc that any of
them needs, which is the oldest Linux the package runs on.

Es la versión de Linux de tools/package_native_windows.py: lleva el launcher para Linux, el
compilador (clang y lld de llvm-mingw para Linux) con un sysroot y su configuración, y el
traductor publicado para Linux. Es un .tar.gz, que guarda qué archivos son programas y los
enlaces. Se niega a terminar si algún archivo parece datos del juego o si algún programa no
arrancaría en la computadora de un jugador.

Usage / Uso:
    python3 tools/package_native_unix.py --architecture x64 --launcher L --kit K --translator T \\
        --python P --llvm-mingw M --sysroot S --nodtool N --output-directory O [--archive A] \\
        [--package-version V] [--version NAME=VALUE ...] [--licenses-directory D]
"""

import argparse
import fnmatch
import json
import os
import shutil
import sys
import tarfile
from collections.abc import Iterable, Sequence
from dataclasses import dataclass, field
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from elf_binaries import ElfError, check_programs, read_elf_binary  # noqa: E402
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

LAUNCHER_NAME = "HarvestMoonMM"
ARCHITECTURES = {"arm64": "aarch64", "x64": "x86_64"}  # the package's names, and CMake's
# The compiler's files that the preparation uses, relative to llvm-mingw's folder: clang (and the
# names it is run by), lld, the libraries they are linked with, and clang's own headers. A pattern
# matches the relative path; `*` also crosses folders.
COMPILER_KEPT = (
    "bin/clang",
    "bin/clang++",
    "bin/clang-[0-9]*",
    "bin/lld",
    "bin/ld.lld",
    "lib/libLLVM.so.*",
    "lib/libclang-cpp.so.*",
    "lib/clang/*/include/*",
    "LICENSE.TXT",
)
# What every run of the package's clang adds, written beside it as its configuration (clang reads
# <driver>.cfg from its own folder): the sysroot beside bin/, and lld, the linker that comes with it.
COMPILER_CONFIGURATION = "--sysroot=<CFGDIR>/../sysroot\n-fuse-ld=lld\n"
COMPILER_CONFIGURATION_FILES = ("clang.cfg", "clang++.cfg")
# What of Python running one script does not need, relative to its folder.
PYTHON_EXCLUDED = (
    "include/*",
    "share/*",
    "bin/idle3*",
    "bin/pip*",
    "bin/pydoc3*",
    "bin/2to3*",
    "bin/python3*-config",
    "lib/pkgconfig/*",
    "lib/libtcl*",
    "lib/libtk*",
    "lib/tcl*",
    "lib/tk*",
    "lib/itcl*",
    "lib/thread*",
    "lib/python3.*/test/*",
    "lib/python3.*/idlelib/*",
    "lib/python3.*/tkinter/*",
    "lib/python3.*/turtledemo/*",
    "lib/python3.*/ensurepip/*",
    "lib/python3.*/venv/*",
    "lib/python3.*/pydoc_data/*",
    "lib/python3.*/site-packages/*",
    "lib/python3.*/config-*/*",
    "lib/python3.*/lib-dynload/_tkinter*",
    "*/__pycache__/*",
)


@dataclass(frozen=True)
class Inputs:
    architecture: str
    launcher: Path
    kit: Path
    translator: Path  # the folder of the translator published for Linux
    python: Path
    llvm_mingw: Path  # llvm-mingw for Linux, unpacked
    sysroot: Path  # tools/make_linux_sysroot.py's
    nodtool: Path
    output_directory: Path
    package_version: str = "dev"
    versions: dict[str, str] = field(default_factory=dict)
    licenses_directory: Path | None = None


def matches(relative: str, patterns: Iterable[str]) -> bool:
    return any(fnmatch.fnmatchcase(relative, pattern) for pattern in patterns)


def copy_keeping_links(
    source: Path, destination: Path, excluded: Iterable[str] = (), kept: Iterable[str] | None = None
) -> None:
    """Copies the files and links under `source`, links as links: those that `excluded` matches stay
    out, and when `kept` is given only those that it matches go in."""
    if not source.is_dir():
        raise PackageError(f"not found: {source}")
    excluded, kept = tuple(excluded), None if kept is None else tuple(kept)
    for folder, folders, files in os.walk(source):
        for name in sorted(folders + files):
            path = Path(folder) / name
            relative = path.relative_to(source).as_posix()
            if path.is_dir() and not path.is_symlink():
                continue
            if matches(relative, excluded) or (kept is not None and not matches(relative, kept)):
                continue
            target = destination / relative
            target.parent.mkdir(parents=True, exist_ok=True)
            if path.is_symlink():
                target.symlink_to(os.readlink(path))
            else:
                shutil.copy2(path, target)


def add_compiler(llvm_mingw: Path, sysroot: Path, compiler: Path, recipe: dict) -> None:
    copy_keeping_links(llvm_mingw, compiler, kept=COMPILER_KEPT)
    for name in COMPILER_CONFIGURATION_FILES:
        (compiler / "bin" / name).write_text(COMPILER_CONFIGURATION, encoding="utf-8")
    copy_keeping_links(sysroot, compiler / "sysroot")
    for name in (recipe["cxx_compiler"], recipe["c_compiler"], "ld.lld"):
        if not (compiler / "bin" / name).exists():
            raise PackageError(f"the compiler has no bin/{name}, which the kit's recipe or clang uses")
    if not (compiler / "sysroot" / "usr" / "include" / "stdio.h").is_file():
        raise PackageError(f"{sysroot} is not a sysroot (tools/make_linux_sysroot.py makes it)")


def check_that_the_programs_start(package: Path, architecture: str) -> tuple[int, ...]:
    """Stops the package when one of its programs would not start on a player's computer; returns
    the newest glibc that they need. The translator's own libraries are left out: .NET publishes
    it with everything it needs (its program is checked for the processor)."""
    builder = package / "builder"
    compiler = builder / "clang" / "bin"
    programs = [
        package / LAUNCHER_NAME,
        builder / "nodtool" / "nodtool",
        builder / "python" / "bin" / "python3",
        *sorted(compiler.glob("clang-[0-9]*")),
        compiler / "lld",
    ]
    try:
        newest = check_programs(programs, architecture)
        translator = read_elf_binary(builder / "translator" / "Translator.Cli")
        if translator.machine != architecture:
            raise PackageError(
                f"the translator is for {translator.machine}, and the package for {architecture}"
            )
    except ElfError as problem:
        raise PackageError(f"a program of the package would not start: {problem}") from None
    return newest


def assemble(inputs: Inputs) -> Path:
    if inputs.architecture not in ARCHITECTURES:
        raise PackageError(f"the architecture must be one of {', '.join(ARCHITECTURES)}")
    recipe = check_kit(inputs.kit, "Linux")
    if recipe.get("processor") != ARCHITECTURES[inputs.architecture]:
        raise PackageError(
            f"the kit is for {recipe.get('processor')}, and the package for {inputs.architecture}"
        )
    package = inputs.output_directory / f"HarvestMoonMM-linux-{inputs.architecture}"
    if package.exists():
        shutil.rmtree(package)
    builder = package / "builder"

    copy_file(inputs.launcher, package / LAUNCHER_NAME)
    write_recomplauncher_profile(package, inputs.package_version)
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
        raise PackageError(f"{inputs.translator} has no Translator.Cli: publish it for Linux")
    copy_keeping_links(inputs.python, builder / "python", PYTHON_EXCLUDED)
    if not (builder / "python" / "bin" / "python3").exists():
        raise PackageError(f"{inputs.python} has no bin/python3")
    add_compiler(inputs.llvm_mingw, inputs.sysroot, builder / "clang", recipe)
    copy_file(inputs.nodtool, builder / "nodtool" / "nodtool")

    for name, source in (
        ("README.txt", "README-linux.txt"),
        ("THIRD_PARTY_NOTICES.txt", "THIRD_PARTY_NOTICES-linux.txt"),
    ):
        copy_file(PACKAGE_FILES / source, package / name)
    copy_file(REPOSITORY / "LICENSE", package / "LICENSE.txt")
    if inputs.licenses_directory is not None:
        copy_keeping_links(inputs.licenses_directory, package / "licenses")
    for program in (package / LAUNCHER_NAME, builder / "nodtool" / "nodtool"):
        program.chmod(0o755)

    glibc = check_that_the_programs_start(package, inputs.architecture)
    check_no_game_data(package)
    versions = [f"{name}: {value}" for name, value in sorted(inputs.versions.items())]
    versions.append(f"architecture: {inputs.architecture}")
    versions.append(f"glibc needed: {'.'.join(str(part) for part in glibc)} or newer")
    versions.append(f"mods: {', '.join(recipe.get('mods', [])) or 'none'}")
    versions.append("")
    versions.append((builder / "clang" / "sysroot" / "PACKAGES.txt").read_text(encoding="utf-8"))
    (package / "VERSIONS.txt").write_text("\n".join(versions), encoding="utf-8")
    return package


def write_archive(package: Path, archive_path: Path) -> None:
    """A .tar.gz of the package, in one folder of its name, with its links and which files are
    programs, and without this computer's owners."""

    def as_anyone(member: tarfile.TarInfo) -> tarfile.TarInfo:
        member.uid = member.gid = 0
        member.uname = member.gname = ""
        return member

    archive_path.parent.mkdir(parents=True, exist_ok=True)
    with tarfile.open(archive_path, "w:gz", compresslevel=9) as archive:
        archive.add(package, arcname=package.name, filter=as_anyone)


def make_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--architecture", required=True, choices=sorted(ARCHITECTURES))
    parser.add_argument("--launcher", type=Path, required=True, help="the launcher built for Linux")
    parser.add_argument("--kit", type=Path, required=True, help="the runtime kit for Linux")
    parser.add_argument("--translator", type=Path, required=True, help="the translator published for Linux")
    parser.add_argument("--python", type=Path, required=True, help="an unpacked Python for Linux")
    parser.add_argument("--llvm-mingw", type=Path, required=True, help="an unpacked llvm-mingw for Linux")
    parser.add_argument("--sysroot", type=Path, required=True, help="tools/make_linux_sysroot.py's sysroot")
    parser.add_argument("--nodtool", type=Path, required=True, help="nodtool for Linux")
    parser.add_argument("--output-directory", type=Path, required=True)
    parser.add_argument("--archive", type=Path, help="also write the package as this .tar.gz")
    parser.add_argument("--package-version", default="dev", help="the launcher's version, as v0.4.0")
    parser.add_argument(
        "--version", type=key_value, action="append", default=[], help="NAME=VALUE for VERSIONS.txt"
    )
    parser.add_argument("--licenses-directory", type=Path, help="a folder of license texts for licenses/")
    return parser


def parse_arguments(argv: Sequence[str] | None) -> argparse.Namespace:
    return make_parser().parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    inputs = Inputs(
        architecture=arguments.architecture,
        launcher=arguments.launcher,
        kit=arguments.kit,
        translator=arguments.translator,
        python=arguments.python,
        llvm_mingw=arguments.llvm_mingw,
        sysroot=arguments.sysroot,
        nodtool=arguments.nodtool,
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
