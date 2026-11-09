#!/usr/bin/env python3
"""Download what the native version's Windows package carries and what builds it, and check it.

Descarga lo que lleva el paquete de Windows de la versión nativa y lo que lo construye, y lo
comprueba.

None of these is ours: each is the official download of its project. This file is the one place
that says which version each one is, where it comes from and what its SHA-256 is; a download whose
hash differs is refused (as tools/fetch_windows_toolchain.py does for the ModernGekko package). To
move to a newer version, change the version, the address and the hash together, and run the
package workflow to see that it still works.

Ninguno es nuestro: cada uno es la descarga oficial de su proyecto. Este archivo es el único lugar
que dice qué versión es cada uno, de dónde viene y cuál es su SHA-256; una descarga con otro hash
se rechaza.

For each processor of the package (arm64, x64):

    llvm-mingw/   the compiler that prepares the game on the player's computer (llvm-mingw for
                  Windows on that processor)
    python/       Python for Windows (python-build-standalone, the stripped "install only" build)
    nodtool/      nodtool, which converts compressed disc images (RVZ, ...) to ISO
    dawn/         Dawn for Windows, which the runtime draws with: its DLLs go in the package, and
                  its import library links the runtime kit

and, with --cross-compiler, llvm-mingw for Linux (cross-compiler/), which builds the runtime kit
and the launcher for Windows on a Linux computer.

Usage / Uso:
    python3 tools/fetch_native_package_tools.py --architecture arm64 --destination D [--cross-compiler]
"""

import argparse
import shutil
import sys
import tarfile
import zipfile
from collections.abc import Sequence
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from fetch_windows_toolchain import ToolchainError, download, sha256_of  # noqa: E402

LLVM_MINGW_VERSION = "20261020"
PYTHON_RELEASE = "20261020"  # python-build-standalone's release
PYTHON_VERSION = "3.13.16"
NODTOOL_VERSION = "v2.0.0-alpha.12"
# The Dawn that WiiCompiled's aurora is pinned to (AURORA_DAWN_VERSION, cmake/AuroraDependencyVersions.cmake).
DAWN_VERSION = "v20260807.225922"

LLVM_MINGW_URL = f"https://github.com/mstorsjo/llvm-mingw/releases/download/{LLVM_MINGW_VERSION}/"
PYTHON_URL = f"https://github.com/astral-sh/python-build-standalone/releases/download/{PYTHON_RELEASE}/"
NODTOOL_URL = f"https://github.com/encounter/nod/releases/download/{NODTOOL_VERSION}/"
DAWN_URL = f"https://github.com/encounter/dawn/releases/download/{DAWN_VERSION}/"


@dataclass(frozen=True)
class Download:
    folder: str  # where it goes under the destination
    name: str  # what people call it, for messages and VERSIONS.txt
    version: str
    url: str
    sha256: str
    archive_root: str  # the folder the archive wraps everything in ("" if none)
    expected: str  # a file that must be there once unpacked, relative to the folder
    file_name: str = ""  # for a download that is a program, not an archive: its name in the folder


def python_build(processor: str, sha256: str) -> Download:
    return Download(
        folder="python",
        name="Python",
        version=PYTHON_VERSION,
        url=f"{PYTHON_URL}cpython-{PYTHON_VERSION}+{PYTHON_RELEASE}-{processor}-pc-windows-msvc-install_only_stripped.tar.gz",
        sha256=sha256,
        archive_root="python",
        expected="python.exe",
    )


def llvm_mingw(host: str, sha256: str, archive: str = "zip") -> Download:
    root = f"llvm-mingw-{LLVM_MINGW_VERSION}-ucrt-{host}"
    return Download(
        folder="llvm-mingw",
        name="llvm-mingw",
        version=LLVM_MINGW_VERSION,
        url=f"{LLVM_MINGW_URL}{root}.{archive}",
        sha256=sha256,
        archive_root=root,
        expected="bin/clang.exe" if archive == "zip" else "bin/clang",
    )


DOWNLOADS: dict[str, tuple[Download, ...]] = {
    "arm64": (
        llvm_mingw("aarch64", "8f24fedd0b75c674c6c83dec2d76a7ac31a690120212843a17e465ef49854f69"),
        python_build("aarch64", "243dd08d29bc85c519b2600fc38b2f4803d640a27cb62ac305cee3a4fcdc54be"),
        Download(
            "nodtool",
            "nodtool",
            NODTOOL_VERSION,
            f"{NODTOOL_URL}nodtool-windows-arm64.exe",
            "463d5f7fd7d753fde05c6772f8114ec6f8ab6eaf054790ed9b95a89ef8c63526",
            "",
            "nodtool.exe",
            file_name="nodtool.exe",
        ),
        Download(
            "dawn",
            "Dawn",
            DAWN_VERSION,
            f"{DAWN_URL}dawn-windows-arm64.tar.gz",
            "764e7d9a12e3c8743211087dd184cb76342cbba4946fb81e89d8136560df5d89",
            "",
            "bin/webgpu_dawn.dll",
        ),
    ),
    "x64": (
        llvm_mingw("x86_64", "fd5462b7f5d93b2ff981fcd3f9c7e427e47f3203c64ea91c27bcc444eebd74cd"),
        python_build("x86_64", "cab7dfd540382d1687fba3efb70dd176366cf25687b0d52e5b4e72499b514f8c"),
        Download(
            "nodtool",
            "nodtool",
            NODTOOL_VERSION,
            f"{NODTOOL_URL}nodtool-windows-x86_64.exe",
            "f65de62c44f0ad5d179eff85bd1e92099713ffacbf717f651fb761129a25c7bc",
            "",
            "nodtool.exe",
            file_name="nodtool.exe",
        ),
        # The same hash as aurora pins for this version (cmake/AuroraDawnProvider.cmake).
        Download(
            "dawn",
            "Dawn",
            DAWN_VERSION,
            f"{DAWN_URL}dawn-windows-amd64.tar.gz",
            "42a7a295c87b2e8a36312e2f84a5823aac63325e8e1d271bc7ced6f83bfb075f",
            "",
            "bin/webgpu_dawn.dll",
        ),
    ),
}
CROSS_COMPILER = Download(
    folder="cross-compiler",
    name="llvm-mingw for Linux",
    version=LLVM_MINGW_VERSION,
    url=f"{LLVM_MINGW_URL}llvm-mingw-{LLVM_MINGW_VERSION}-ucrt-ubuntu-22.04-x86_64.tar.xz",
    sha256="55064b60b991a1e1e8090b8c2f84e82b22cfe5903d12c159d65064cf5e8ae335",
    archive_root=f"llvm-mingw-{LLVM_MINGW_VERSION}-ucrt-ubuntu-22.04-x86_64",
    expected="bin/clang",
)


def fetch(item: Download, cache_directory: Path) -> Path:
    """The verified download of `item`, from the cache when it is already there."""
    cache_directory.mkdir(parents=True, exist_ok=True)
    archive = cache_directory / item.url.rsplit("/", 1)[-1]
    if archive.is_file() and sha256_of(archive) == item.sha256:
        print(f"  {archive.name}: already downloaded, hash checked")
        return archive
    partial = archive.with_name(archive.name + ".part")
    print(f"  downloading {item.url}")
    download(item.url, partial)
    actual = sha256_of(partial)
    if actual != item.sha256:
        partial.unlink()
        raise ToolchainError(
            f"{item.name} {item.version}: the download has the SHA-256 {actual}, not {item.sha256}"
        )
    partial.replace(archive)
    return archive


def inside(root: Path, member: str) -> bool:
    target = (root / member).resolve()
    return target == root.resolve() or root.resolve() in target.parents


def unpack(item: Download, archive: Path, destination: Path) -> Path:
    """`archive` unpacked into destination/<folder>, without the folder it wraps everything in."""
    target = destination / item.folder
    staging = destination / f"{item.folder}.unpacking"
    for leftover in (target, staging):
        if leftover.exists():
            shutil.rmtree(leftover)
    staging.mkdir(parents=True)
    try:
        if item.file_name:
            shutil.copy2(archive, staging / item.file_name)
        elif archive.name.endswith(".zip"):
            with zipfile.ZipFile(archive) as opened:
                bad = [name for name in opened.namelist() if not inside(staging, name)]
                if bad:
                    raise ToolchainError(
                        f"{item.name}: the archive has a path that leaves its folder: {bad[0]}"
                    )
                opened.extractall(staging)
        else:
            with tarfile.open(archive) as opened:
                opened.extractall(staging, filter="data")  # refuses paths that leave the folder
        unpacked = staging / item.archive_root if item.archive_root else staging
        if not (unpacked / item.expected).is_file():
            raise ToolchainError(f"{item.name} {item.version}: {item.expected} is not in the archive")
        unpacked.rename(target)
    finally:
        shutil.rmtree(staging, ignore_errors=True)
    return target


def parse_arguments(argv: Sequence[str] | None) -> argparse.Namespace:
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--architecture", required=True, choices=sorted(DOWNLOADS))
    parser.add_argument("--destination", type=Path, required=True, help="where each one gets its folder")
    parser.add_argument(
        "--cache-directory", type=Path, help="where the downloads are kept (default: DESTINATION/downloads)"
    )
    parser.add_argument(
        "--cross-compiler", action="store_true", help="also llvm-mingw for Linux, to build for Windows"
    )
    return parser.parse_args(argv)


def main(argv: Sequence[str] | None = None) -> int:
    arguments = parse_arguments(argv)
    cache = arguments.cache_directory or arguments.destination / "downloads"
    items = list(DOWNLOADS[arguments.architecture])
    if arguments.cross_compiler:
        items.append(CROSS_COMPILER)
    try:
        for item in items:
            print(f"{item.name} {item.version}")
            print(f"  unpacked into {unpack(item, fetch(item, cache), arguments.destination)}")
    except (ToolchainError, OSError, zipfile.BadZipFile, tarfile.TarError) as problem:
        print(f"error: {problem}", file=sys.stderr)
        return 2
    return 0


if __name__ == "__main__":
    sys.exit(main())
