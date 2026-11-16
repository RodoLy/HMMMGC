#!/usr/bin/env python3
"""Make the Linux sysroot that the package's compiler uses on the player's computer.

Hace el sysroot de Linux que usa el compilador del paquete en la computadora del jugador.

On Linux the game is compiled on the player's computer like on Windows (docs/NATIVE_PACKAGE.md),
but a Linux computer does not always have the C and C++ headers and libraries that compiling needs
(the Steam Deck, for example, has none). So the package carries them: a sysroot, a folder laid out
like the root of a Linux system with only what the compiler reads, which the compiler is pointed
at with --sysroot (tools/package_native_unix.py writes it into the compiler's configuration).

It is made from the packages of the Ubuntu that built the runtime kit, the same headers and
libraries the kit was compiled with, so the game is compiled exactly like the kit:

    the C library   libc6, libc6-dev and linux-libc-dev (glibc's headers, crt1.o, libc.so, ...)
    GCC's runtime   libgcc-<N>-dev and libstdc++-<N>-dev, for the GCC that clang chose
                    (crtbegin.o, libgcc.a, the C++ headers and libstdc++.a), and libgcc-s1 and
                    libstdc++6, which the -dev packages' links point at

The files come from what dpkg says each package installed: headers and libraries, without
documentation, programs or the character set converters of glibc (gconv). PACKAGES.txt says which
packages and versions they are, and where their source is, and licenses/ has each one's copyright
file. A file of /lib goes to
usr/lib, as Ubuntu itself keeps it (/lib is a link to usr/lib), and the sysroot gets the same links
(lib, lib64), because glibc's libc.so names /lib/.../libc.so.6 and the linker looks for it inside
the sysroot. A link that points at an absolute path is made relative, so that it points inside the
sysroot wherever the package is unpacked.

With --check-compiler, a small C++ program is compiled and linked against the sysroot as the game
is (static libstdc++, lld) and run, which proves that the sysroot is complete.

Se hace con los paquetes del Ubuntu que compiló el kit del runtime: las mismas cabeceras y
bibliotecas, así que el juego se compila igual que el kit. Los archivos salen de lo que dpkg dice
que instaló cada paquete: cabeceras y bibliotecas, sin documentación, programas ni los
convertidores de glibc.

Usage / Uso:
    python3 tools/make_linux_sysroot.py --compiler <clang++> --out <sysroot> [--package NAME ...]
        [--check-compiler]
"""

import argparse
import os
import re
import shutil
import subprocess
import sys
import tempfile
from collections.abc import Iterable, Sequence
from pathlib import Path, PurePosixPath

# glibc and the kernel's headers, always.
C_LIBRARY_PACKAGES = ("libc6", "libc6-dev", "linux-libc-dev")
# GCC's runtime: <N> is the version of the GCC installation that clang chose.
GCC_PACKAGES = ("libgcc-{version}-dev", "libstdc++-{version}-dev", "libgcc-s1", "libstdc++6")
# What of a package goes into the sysroot: headers and libraries.
KEPT_FOLDERS = ("usr/include", "usr/lib", "usr/lib64", "lib", "lib64")
# What does not, even inside those folders: glibc's character set converters and audit modules,
# separate debugging information, and the debugger's scripts that some libraries install. (The C++
# headers have a folder called debug too, which stays.)
DROPPED = re.compile(r"(^|/)(gconv|audit)(/|$)|^usr/lib/debug/|\.py$")
# The links of a merged-/usr system, from the sysroot's root.
ROOT_LINKS = {"lib": "usr/lib", "lib64": "usr/lib64"}

CHECK_PROGRAM = """\
#include <cmath>
#include <cstdio>
#include <string>
#include <thread>
#include <vector>

int main() {
    std::vector<std::string> words{"sysroot", "ok"};
    std::thread worker([&] { words.push_back(std::to_string(static_cast<int>(std::sqrt(16.0)))); });
    worker.join();
    std::printf("%s %s %s\\n", words[0].c_str(), words[1].c_str(), words[2].c_str());
    return words.size() == 3 ? 0 : 1;
}
"""


class SysrootError(Exception):
    """The sysroot cannot be made on this computer."""


def gcc_version(compiler: Path) -> str:
    """The version of the GCC installation that `compiler` (a clang) takes its C++ library from."""
    result = subprocess.run(
        [str(compiler), "-v", "-E", "-x", "c++", os.devnull, "-o", os.devnull],
        capture_output=True,
        text=True,
    )
    found = re.search(r"Selected GCC installation: \S*/(\d+)\s", result.stderr)
    if found is None:
        raise SysrootError(f"{compiler} uses no GCC installation: is libstdc++-dev installed?")
    return found[1]


def package_files(package: str) -> list[PurePosixPath]:
    """The paths that dpkg says `package` installed."""
    result = subprocess.run(["dpkg-query", "-L", package], capture_output=True, text=True)
    if result.returncode != 0:
        raise SysrootError(f"the package {package} is not installed: {result.stderr.strip()}")
    return [PurePosixPath(line) for line in result.stdout.splitlines() if line.startswith("/")]


def kept(path: PurePosixPath) -> bool:
    relative = path.as_posix().lstrip("/")
    return any(relative == folder or relative.startswith(folder + "/") for folder in KEPT_FOLDERS) and not (
        DROPPED.search(relative)
    )


def place(path: PurePosixPath, root: Path) -> PurePosixPath:
    """Where `path` goes in the sysroot: its folder as the system resolves it (/lib -> /usr/lib on a
    merged-/usr system), relative to the root, with its own name."""
    folder = Path(os.path.realpath(root / path.parent.as_posix().lstrip("/")))
    try:
        relative_folder = folder.relative_to(root.resolve())
    except ValueError:
        raise SysrootError(f"{path} is in a folder outside {root}") from None
    return PurePosixPath(relative_folder.as_posix()) / path.name


def copy_entry(source: Path, target: Path, sysroot: Path, system_root: Path) -> None:
    """Copies a file, or recreates a link with an absolute target as a relative one."""
    target.parent.mkdir(parents=True, exist_ok=True)
    if target.is_symlink() or target.exists():
        return
    if source.is_symlink():
        link = os.readlink(source)
        if os.path.isabs(link):
            inside = place(PurePosixPath(link), system_root)
            link = os.path.relpath(sysroot / inside, target.parent)
        target.symlink_to(link)
    elif source.is_file():
        shutil.copy2(source, target)


def make_sysroot(
    compiler: Path, out: Path, extra_packages: Iterable[str] = (), system_root: Path = Path("/")
) -> list[str]:
    """Makes the sysroot in `out` (replaced); returns the packages it holds."""
    version = gcc_version(compiler)
    packages = [
        *C_LIBRARY_PACKAGES,
        *(name.format(version=version) for name in GCC_PACKAGES),
        *extra_packages,
    ]
    if out.exists():
        shutil.rmtree(out)
    out.mkdir(parents=True)
    for package in packages:
        for path in package_files(package):
            source = system_root / path.as_posix().lstrip("/")
            if not kept(path) or (source.is_dir() and not source.is_symlink()):
                continue
            copy_entry(source, out / place(path, system_root).as_posix(), out, system_root)
    for name, target in ROOT_LINKS.items():
        if (out / target).is_dir() and not (out / name).exists():
            (out / name).symlink_to(target)
    write_sources(packages, out, system_root)
    return packages


def write_sources(packages: Sequence[str], out: Path, system_root: Path) -> None:
    """What the sysroot is made of, for whoever wants its source: PACKAGES.txt, each package with its
    version and its source package (as Ubuntu publishes it), and licenses/, each package's
    copyright file (glibc is LGPL, GCC's runtime is GPL with its runtime exception)."""
    lines = []
    for package in packages:
        fields = subprocess.run(
            ["dpkg-query", "-W", "-f", "${Package} ${Version} ${source:Package} ${source:Version}", package],
            capture_output=True,
            text=True,
            check=True,
        ).stdout.split()
        lines.append(f"{fields[0]} {fields[1]} (source: {fields[2]} {fields[3]})")
        copyright_file = system_root / "usr" / "share" / "doc" / package / "copyright"
        if copyright_file.is_file():
            (out / "licenses").mkdir(exist_ok=True)
            shutil.copy2(copyright_file, out / "licenses" / f"{package}-copyright.txt")
    (out / "PACKAGES.txt").write_text(
        "The Ubuntu packages this sysroot is made of. Their source: https://launchpad.net/ubuntu/+source/<source>\n"
        "Los paquetes de Ubuntu de los que se hizo este sysroot.\n\n" + "\n".join(lines) + "\n",
        encoding="utf-8",
    )


def check(compiler: Path, sysroot: Path) -> str:
    """Compiles, links and runs a small program with only the sysroot; returns what it printed."""
    with tempfile.TemporaryDirectory() as work:
        source = Path(work) / "check.cpp"
        program = Path(work) / "check"
        source.write_text(CHECK_PROGRAM, encoding="utf-8")
        command = [
            str(compiler),
            f"--sysroot={sysroot}",
            "-fuse-ld=lld",
            "-std=gnu++20",
            "-pthread",
            "-static-libstdc++",
            "-static-libgcc",
            str(source),
            "-o",
            str(program),
        ]
        built = subprocess.run(command, capture_output=True, text=True)
        if built.returncode != 0:
            raise SysrootError(f"a program does not build with the sysroot:\n{built.stderr}")
        ran = subprocess.run([str(program)], capture_output=True, text=True)
        if ran.returncode != 0 or ran.stdout.strip() != "sysroot ok 4":
            raise SysrootError(f"the program built with the sysroot did not run: {ran.stdout}{ran.stderr}")
        return ran.stdout.strip()


def parse_arguments(argv: Sequence[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--compiler", type=Path, required=True, help="the clang++ that built the runtime kit")
    parser.add_argument("--out", type=Path, required=True, help="the sysroot's folder (replaced)")
    parser.add_argument(
        "--package", action="append", default=[], help="another installed package to add; repeat for several"
    )
    parser.add_argument(
        "--check-compiler", action="store_true", help="build and run a program with the sysroot afterwards"
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    try:
        packages = make_sysroot(arguments.compiler, arguments.out, arguments.package)
        if arguments.check_compiler:
            check(arguments.compiler, arguments.out.resolve())
    except (SysrootError, OSError) as problem:
        print(f"error: {problem}", file=sys.stderr)
        return 2
    size = sum(
        path.stat().st_size for path in arguments.out.rglob("*") if path.is_file() and not path.is_symlink()
    )
    print(f"sysroot in {arguments.out}: {size / 2**20:.0f} MB from {', '.join(packages)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
