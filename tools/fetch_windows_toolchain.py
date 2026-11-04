#!/usr/bin/env python3
"""Download the build tools that the Windows ARM64 package ships, and check what was downloaded.

Descarga las herramientas de compilación que lleva el paquete para Windows ARM64 y comprueba
lo que se descargó.

The package carries a C compiler (llvm-mingw), CMake and Ninja so that the player does not have
to install anything. They are not ours: each one is the official download of its project for
Windows on ARM. This file is the one place that says which version it is, where it comes from
and what its SHA-256 is. A download whose hash differs from the one written here is refused, so a
file that was changed on the way, or a release that was replaced afterwards, can never end up in
a package unnoticed. To move to a newer version, change the version, the address and the hash of
the tool together, and run the package workflow to see that it still works.

El paquete lleva un compilador de C (llvm-mingw), CMake y Ninja para que el jugador no tenga que
instalar nada. No son nuestros: cada uno es la descarga oficial de su proyecto para Windows en
ARM. Este archivo es el único lugar que dice qué versión es, de dónde viene y cuál es su SHA-256.
Una descarga cuyo hash difiere del que está escrito aquí se rechaza, así que un archivo cambiado
en el camino, o una versión publicada que se reemplazó después, nunca llega a un paquete sin que
nadie lo note. Para pasar a una versión más nueva, se cambian juntos la versión, la dirección y el
hash de la herramienta, y se ejecuta el workflow del paquete para ver que todo sigue funcionando.

Each tool is unpacked into `DESTINATION/<folder>` (llvm-mingw, cmake, ninja), without the folder
that its archive wraps everything in, which is where tools/package_windows.py expects it.

Cada herramienta se descomprime en `DESTINO/<carpeta>` (llvm-mingw, cmake, ninja), sin la carpeta
en la que su archivo lo envuelve todo, que es donde la espera tools/package_windows.py.

Usage / Uso:
    python3 tools/fetch_windows_toolchain.py --destination C:/toolchain
"""

import argparse
import hashlib
import http.client
import shutil
import sys
import time
import urllib.request
import zipfile
from collections.abc import Sequence
from dataclasses import dataclass
from pathlib import Path

DOWNLOAD_ATTEMPTS = 3
RETRY_DELAY_SECONDS = 5
READ_BLOCK_BYTES = 1024 * 1024
# GitHub answers a request with no User-Agent with an error. A plain name, with no personal data.
USER_AGENT = "hm-mm-recomp-toolchain-download"


@dataclass(frozen=True)
class BuildTool:
    """One download of the package's toolchain."""

    folder: str  # where it goes under the destination, and under toolchain/ in the package
    name: str  # what people call it, for messages and for VERSIONS.txt
    version: str
    url: str
    sha256: str  # of the downloaded archive
    archive_root: str  # the one folder the archive wraps everything in; "" if it has none
    executable: str  # a file that must be there once unpacked, relative to the folder


TOOLS = (
    BuildTool(
        folder="llvm-mingw",
        name="llvm-mingw",
        version="20261020",
        url="https://github.com/mstorsjo/llvm-mingw/releases/download/20261020/"
        "llvm-mingw-20261020-ucrt-aarch64.zip",
        sha256="8f24fedd0b75c674c6c83dec2d76a7ac31a690120212843a17e465ef49854f69",
        archive_root="llvm-mingw-20261020-ucrt-aarch64",
        executable="bin/clang.exe",
    ),
    BuildTool(
        folder="cmake",
        name="CMake",
        version="4.4.4",
        url="https://github.com/Kitware/CMake/releases/download/v4.4.4/cmake-4.4.4-windows-arm64.zip",
        # The same hash as in cmake-4.4.4-SHA-256.txt, which CMake publishes with the release.
        sha256="ed673bbc4eb7c1e59b0407fd52f1680fc0ae3229a46b9b7b3ea9133764f07f67",
        archive_root="cmake-4.4.4-windows-arm64",
        executable="bin/cmake.exe",
    ),
    BuildTool(
        folder="ninja",
        name="Ninja",
        version="1.13.2",
        url="https://github.com/ninja-build/ninja/releases/download/v1.13.2/ninja-winarm64.zip",
        sha256="e52f0bdef9dfb1003229dbd6508a508c4073fd017247002adc66e5e806cb0391",
        archive_root="",
        executable="ninja.exe",
    ),
)


class ToolchainError(Exception):
    """A download or an archive is not what the package expects."""


def sha256_of(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as file:
        while block := file.read(READ_BLOCK_BYTES):
            digest.update(block)
    return digest.hexdigest()


def download(url: str, destination: Path) -> None:
    """Write what `url` serves into `destination`, trying again if the connection breaks."""
    request = urllib.request.Request(url, headers={"User-Agent": USER_AGENT})
    for attempt in range(1, DOWNLOAD_ATTEMPTS + 1):
        try:
            with urllib.request.urlopen(request) as response, destination.open("wb") as file:
                shutil.copyfileobj(response, file, READ_BLOCK_BYTES)
            return
        except (OSError, http.client.HTTPException) as problem:
            if attempt == DOWNLOAD_ATTEMPTS:
                raise ToolchainError(f"could not download {url}: {problem}") from problem
            print(f"  download attempt {attempt} failed ({problem}); trying again", file=sys.stderr)
            time.sleep(RETRY_DELAY_SECONDS)


def fetch_archive(tool: BuildTool, cache_directory: Path) -> Path:
    """The verified archive of `tool`, downloaded into `cache_directory` unless it is already there."""
    cache_directory.mkdir(parents=True, exist_ok=True)
    archive = cache_directory / tool.url.rsplit("/", 1)[-1]
    if archive.is_file() and sha256_of(archive) == tool.sha256:
        print(f"  {archive.name}: already downloaded, hash checked")
        return archive
    partial = archive.with_name(archive.name + ".part")
    print(f"  downloading {tool.url}")
    download(tool.url, partial)
    actual = sha256_of(partial)
    if actual != tool.sha256:
        partial.unlink()
        raise ToolchainError(
            f"{tool.name} {tool.version}: the download has the SHA-256 {actual}, "
            f"and the package expects {tool.sha256}. It was not used."
        )
    partial.replace(archive)
    print(f"  {archive.name}: SHA-256 {actual} matches")
    return archive


def check_archive_layout(tool: BuildTool, archive: zipfile.ZipFile, staging: Path) -> None:
    """Refuse an archive that would write outside `staging`, or that is not laid out as expected."""
    staging_root = staging.resolve()
    for member in archive.namelist():
        target = (staging / member).resolve()
        if target != staging_root and staging_root not in target.parents:
            raise ToolchainError(f"{tool.name}: the archive has a path that leaves its folder: {member}")
        if tool.archive_root and not member.startswith(tool.archive_root + "/"):
            raise ToolchainError(
                f"{tool.name} {tool.version}: expected everything in the archive to be inside "
                f"{tool.archive_root}/, but it has {member}"
            )


def unpack(tool: BuildTool, archive_path: Path, destination: Path) -> Path:
    """Unpack the archive into `destination/<folder>` and return that folder."""
    target = destination / tool.folder
    staging = destination / f"{tool.folder}.unpacking"
    for leftover in (target, staging):
        if leftover.exists():
            shutil.rmtree(leftover)
    staging.mkdir(parents=True)
    try:
        with zipfile.ZipFile(archive_path) as archive:
            check_archive_layout(tool, archive, staging)
            archive.extractall(staging)
        unpacked = staging / tool.archive_root if tool.archive_root else staging
        if not (unpacked / tool.executable).is_file():
            raise ToolchainError(f"{tool.name} {tool.version}: {tool.executable} is not in the archive")
        unpacked.rename(target)
    finally:
        shutil.rmtree(staging, ignore_errors=True)
    return target


def fetch_tool(tool: BuildTool, destination: Path, cache_directory: Path) -> Path:
    print(f"{tool.name} {tool.version}")
    archive = fetch_archive(tool, cache_directory)
    target = unpack(tool, archive, destination)
    print(f"  unpacked into {target}")
    return target


def parse_arguments(argv: Sequence[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--destination", type=Path, required=True, help="where each tool gets its folder")
    parser.add_argument(
        "--cache-directory",
        type=Path,
        help="where the downloaded archives are kept (default: DESTINATION/downloads)",
    )
    parser.add_argument(
        "--tool",
        action="append",
        choices=[tool.folder for tool in TOOLS],
        help="fetch only this tool; repeat it for several (default: all of them)",
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    destination = arguments.destination
    cache_directory = arguments.cache_directory or destination / "downloads"
    wanted = set(arguments.tool or [tool.folder for tool in TOOLS])
    try:
        for tool in TOOLS:
            if tool.folder in wanted:
                fetch_tool(tool, destination, cache_directory)
    except (ToolchainError, zipfile.BadZipFile, OSError) as problem:
        print(f"error: {problem}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
