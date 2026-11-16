#!/usr/bin/env python3
"""Download what the native version's packages carry and what builds them, and check it.

Descarga lo que llevan los paquetes de la versión nativa y lo que los construye, y lo comprueba.

None of these is ours: each is the official download of its project. This file is the one place
that says which version each one is, where it comes from and what its SHA-256 is; a download whose
hash differs is refused (as tools/fetch_windows_toolchain.py does for the ModernGekko package). To
move to a newer version, change the version, the address and the hash together, and run the
package workflow to see that it still works.

Ninguno es nuestro: cada uno es la descarga oficial de su proyecto. Este archivo es el único lugar
que dice qué versión es cada uno, de dónde viene y cuál es su SHA-256; una descarga con otro hash
se rechaza.

For each system and processor of a package (windows, linux or macos; arm64 or x64):

    llvm-mingw/   the compiler that prepares the game on the player's computer: llvm-mingw for
                  that system and processor (Windows and Linux; its clang also compiles for Linux
                  itself). A Mac uses Apple's clang, from the Command Line Tools.
    python/       Python for that system (python-build-standalone, the stripped "install only"
                  build)
    nodtool/      nodtool, which converts compressed disc images (RVZ, ...) to ISO
    dawn/         Dawn for Windows, which the runtime draws with: its DLLs go in the package, and
                  its import library links the runtime kit. On Linux and macOS aurora downloads
                  its own Dawn, which is linked into the program.

and, with --cross-compiler (Windows), llvm-mingw for Linux (cross-compiler/), which builds the
runtime kit and the launcher for Windows on a Linux computer.

Usage / Uso:
    python3 tools/fetch_native_package_tools.py [--system windows] --architecture arm64 --destination D \
        [--cross-compiler]
"""

import argparse
import shutil
import sys
import tarfile
import zipfile
from collections.abc import Sequence
from dataclasses import dataclass, replace
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


def python_build(processor: str, sha256: str, target: str = "pc-windows-msvc") -> Download:
    return Download(
        folder="python",
        name="Python",
        version=PYTHON_VERSION,
        url=f"{PYTHON_URL}cpython-{PYTHON_VERSION}+{PYTHON_RELEASE}-{processor}-{target}-install_only_stripped.tar.gz",
        sha256=sha256,
        archive_root="python",
        expected="python.exe" if target.endswith("windows-msvc") else "bin/python3",
    )


def nodtool(asset: str, sha256: str) -> Download:
    name = "nodtool.exe" if asset.endswith(".exe") else "nodtool"
    return Download(
        "nodtool", "nodtool", NODTOOL_VERSION, f"{NODTOOL_URL}{asset}", sha256, "", name, file_name=name
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


# The downloads of each system, by processor.
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
LINUX_DOWNLOADS: dict[str, tuple[Download, ...]] = {
    "arm64": (
        llvm_mingw(
            "ubuntu-22.04-aarch64",
            "ca16bb0f4f485876496fa46ad8d353c525eba57d0cf9e9e49ec1738fe4810151",
            "tar.xz",
        ),
        python_build(
            "aarch64", "83621f7b2b1e5da63cae0b4c67612974547b14262676608e830e246cb832d07d", "unknown-linux-gnu"
        ),
        nodtool("nodtool-linux-aarch64", "33e801c0e4aee7e714ad84e89521f50e2f79005571b9d4779b695e44ea21ff99"),
    ),
    "x64": (
        llvm_mingw(
            "ubuntu-22.04-x86_64",
            "55064b60b991a1e1e8090b8c2f84e82b22cfe5903d12c159d65064cf5e8ae335",
            "tar.xz",
        ),
        python_build(
            "x86_64", "f326b7a386bf22def82e55dc3b95d2bef8b7c938b4c98ec32dd65433e7720b50", "unknown-linux-gnu"
        ),
        nodtool("nodtool-linux-x86_64", "ee8345c004248eb82a86dd0c6068e0fcf3dc75baf68d18fe949bf4589822f5a5"),
    ),
}
MACOS_DOWNLOADS: dict[str, tuple[Download, ...]] = {
    "arm64": (
        python_build(
            "aarch64", "88b362e2e9754787c07f2847b717b127e60f77976e20847a0489f3043c925f2c", "apple-darwin"
        ),
        nodtool("nodtool-macos-arm64", "524c0b27c0c4289e435df4474d288ea7e63d6a34814cc7c650231688dd1e53b4"),
    ),
    "x64": (
        python_build(
            "x86_64", "730c2a96c2cdd05c5e00192ea6804c45c28fd6944abb4b55aeaa02b833fc79ec", "apple-darwin"
        ),
        nodtool("nodtool-macos-x86_64", "d1e7ba904c2e557fafbd3bf2d9b75a7901a2bf1568c0fb3a505fca0f30b0a833"),
    ),
}
SYSTEMS = {"windows": DOWNLOADS, "linux": LINUX_DOWNLOADS, "macos": MACOS_DOWNLOADS}
# The same llvm-mingw for Linux x64, in its own folder: it builds the Windows packages on Linux.
CROSS_COMPILER = replace(LINUX_DOWNLOADS["x64"][0], folder="cross-compiler", name="llvm-mingw for Linux")


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
    parser.add_argument("--system", default="windows", choices=sorted(SYSTEMS))
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
    items = list(SYSTEMS[arguments.system][arguments.architecture])
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
