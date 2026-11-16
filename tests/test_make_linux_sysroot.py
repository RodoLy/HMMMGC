"""Tests of tools/make_linux_sysroot.py with a made-up system: a folder laid out like a merged-/usr
Ubuntu, and dpkg's answers replaced by lists. They need no dpkg and no compiler.

Pruebas de tools/make_linux_sysroot.py con un sistema inventado. No necesitan dpkg ni compilador.
"""

import subprocess
import sys
from pathlib import Path, PurePosixPath

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import make_linux_sysroot  # noqa: E402
from make_linux_sysroot import SysrootError, gcc_version, kept, make_sysroot, place  # noqa: E402


@pytest.fixture
def system(tmp_path):
    """A merged-/usr system with a few files of glibc and GCC 14, as dpkg lists them."""
    root = tmp_path / "root"
    for folder in (
        "usr/lib/x86_64-linux-gnu",
        "usr/lib64",
        "usr/include/c++/14/debug",
        "usr/share/doc/libc6",
    ):
        (root / folder).mkdir(parents=True)
    (root / "lib").symlink_to("usr/lib")
    (root / "lib64").symlink_to("usr/lib64")
    (root / "usr/include/stdio.h").write_text("/* stdio */\n")
    (root / "usr/include/c++/14/debug/assertions.h").write_text("/* debug */\n")
    (root / "usr/lib/x86_64-linux-gnu/libc.so.6").write_bytes(b"\x7fELF")
    (root / "usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2").write_bytes(b"\x7fELF")
    (root / "usr/lib/x86_64-linux-gnu/libc.so").write_text("GROUP ( /lib/x86_64-linux-gnu/libc.so.6 )\n")
    (root / "usr/lib/x86_64-linux-gnu/gconv").mkdir()
    (root / "usr/lib/x86_64-linux-gnu/gconv/UTF-16.so").write_bytes(b"\x7fELF")
    (root / "usr/lib64/ld-linux-x86-64.so.2").symlink_to("/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2")
    (root / "usr/share/doc/libc6/copyright").write_text("glibc is LGPL\n")
    return root


LISTS = {
    "libc6": [
        "/lib/x86_64-linux-gnu",
        "/lib/x86_64-linux-gnu/libc.so.6",
        "/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2",
        "/usr/lib/x86_64-linux-gnu/gconv/UTF-16.so",
        "/usr/lib64/ld-linux-x86-64.so.2",
        "/usr/share/doc/libc6/copyright",
    ],
    "libc6-dev": ["/usr/include/stdio.h", "/usr/lib/x86_64-linux-gnu/libc.so"],
    "libstdc++-14-dev": ["/usr/include/c++/14/debug/assertions.h"],
}


@pytest.fixture
def made_up_dpkg(monkeypatch):
    def run(command, **_):
        if command[0] == "dpkg-query" and command[1] == "-L":
            if command[2] not in LISTS:
                return subprocess.CompletedProcess(command, 1, "", f"package '{command[2]}' is not installed")
            return subprocess.CompletedProcess(command, 0, "\n".join(LISTS[command[2]]) + "\n", "")
        if command[0] == "dpkg-query" and command[1] == "-W":
            return subprocess.CompletedProcess(command, 0, f"{command[-1]} 1.0 source{command[-1]} 1.0", "")
        if "-v" in command:
            return subprocess.CompletedProcess(
                command, 0, "", "Selected GCC installation: /usr/lib/gcc/x86_64-linux-gnu/14\n"
            )
        raise AssertionError(command)

    monkeypatch.setattr(make_linux_sysroot.subprocess, "run", run)
    monkeypatch.setattr(make_linux_sysroot, "C_LIBRARY_PACKAGES", ("libc6", "libc6-dev"))
    monkeypatch.setattr(make_linux_sysroot, "GCC_PACKAGES", ("libstdc++-{version}-dev",))


def test_the_sysroot_has_the_headers_and_libraries_in_their_merged_usr_places_and_the_root_links(
    system, tmp_path, made_up_dpkg
):
    out = tmp_path / "sysroot"
    packages = make_sysroot(Path("clang++"), out, system_root=system)
    assert packages == ["libc6", "libc6-dev", "libstdc++-14-dev"]
    assert (out / "usr/include/stdio.h").is_file()
    assert (out / "usr/include/c++/14/debug/assertions.h").is_file()  # the C++ headers' debug folder stays
    assert (out / "usr/lib/x86_64-linux-gnu/libc.so.6").is_file()  # from /lib, kept in usr/lib
    assert not (out / "usr/lib/x86_64-linux-gnu/gconv").exists()
    assert not (out / "usr/share").exists()
    assert (out / "lib").readlink() == Path("usr/lib")
    # The link to an absolute path now points inside the sysroot.
    link = out / "usr/lib64/ld-linux-x86-64.so.2"
    assert link.readlink() == Path("../lib/x86_64-linux-gnu/ld-linux-x86-64.so.2")
    assert link.resolve() == (out / "usr/lib/x86_64-linux-gnu/ld-linux-x86-64.so.2").resolve()
    assert (out / "licenses" / "libc6-copyright.txt").read_text() == "glibc is LGPL\n"
    assert "libc6 1.0 (source: sourcelibc6 1.0)" in (out / "PACKAGES.txt").read_text()


def test_a_package_that_is_not_installed_stops_it(system, tmp_path, made_up_dpkg):
    with pytest.raises(SysrootError, match="libpng-dev is not installed"):
        make_sysroot(Path("clang++"), tmp_path / "sysroot", ["libpng-dev"], system_root=system)


def test_the_gcc_version_is_the_one_clang_chose(made_up_dpkg):
    assert gcc_version(Path("clang++")) == "14"


def test_only_headers_and_libraries_are_kept():
    assert kept(PurePosixPath("/usr/include/stdio.h"))
    assert kept(PurePosixPath("/lib/x86_64-linux-gnu/libc.so.6"))
    assert not kept(PurePosixPath("/usr/share/doc/libc6/copyright"))
    assert not kept(PurePosixPath("/usr/bin/ldd"))
    assert not kept(PurePosixPath("/usr/lib/x86_64-linux-gnu/gconv/UTF-16.so"))
    assert not kept(PurePosixPath("/usr/lib/debug/.build-id/ab/cdef.debug"))


def test_a_file_of_lib_goes_where_the_system_keeps_it(system):
    assert place(PurePosixPath("/lib/x86_64-linux-gnu/libc.so.6"), system) == PurePosixPath(
        "usr/lib/x86_64-linux-gnu/libc.so.6"
    )
