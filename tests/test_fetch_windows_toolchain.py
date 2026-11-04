"""Tests of tools/fetch_windows_toolchain.py. No network: the "downloads" are files on disk.

Pruebas de tools/fetch_windows_toolchain.py. Sin red: las "descargas" son archivos en disco.
"""

import dataclasses
import hashlib
import re
import sys
import zipfile
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import fetch_windows_toolchain as fetch  # noqa: E402
import package_windows  # noqa: E402


def make_archive(path: Path, members: dict[str, bytes]) -> str:
    """Write a zip with `members` (name -> content) and return its SHA-256."""
    with zipfile.ZipFile(path, "w") as archive:
        for name, content in members.items():
            archive.writestr(name, content)
    return hashlib.sha256(path.read_bytes()).hexdigest()


def tool_for(archive: Path, sha256: str, *, root: str = "tool-1.0", executable: str = "bin/tool.exe"):
    return fetch.BuildTool(
        folder="tool",
        name="Tool",
        version="1.0",
        url=archive.as_uri(),
        sha256=sha256,
        archive_root=root,
        executable=executable,
    )


@pytest.fixture
def served_tool(tmp_path):
    """A tool whose archive wraps its files in a folder, and whose "download" is a file on disk."""
    archive = tmp_path / "server" / "tool-1.0.zip"
    archive.parent.mkdir()
    sha256 = make_archive(archive, {"tool-1.0/bin/tool.exe": b"program", "tool-1.0/README.txt": b"read me"})
    return tool_for(archive, sha256)


@pytest.fixture(autouse=True)
def no_waiting(monkeypatch):
    monkeypatch.setattr(fetch.time, "sleep", lambda seconds: None)


# -- The versions the package is pinned to ---------------------------------------------------------------


@pytest.mark.parametrize("tool", fetch.TOOLS, ids=lambda tool: tool.folder)
def test_every_pinned_tool_is_an_https_download_with_a_sha256_and_a_version_in_its_address(tool):
    assert tool.url.startswith("https://github.com/")
    assert re.fullmatch(r"[0-9a-f]{64}", tool.sha256)
    assert tool.version in tool.url


@pytest.mark.parametrize("tool", fetch.TOOLS, ids=lambda tool: tool.folder)
def test_the_wrapping_folder_of_each_archive_is_the_archive_name(tool):
    archive_name = tool.url.rsplit("/", 1)[-1]
    assert tool.archive_root in {"", archive_name.removesuffix(".zip")}


def test_the_tools_have_different_folders_and_hashes():
    assert len({tool.folder for tool in fetch.TOOLS}) == len(fetch.TOOLS)
    assert len({tool.sha256 for tool in fetch.TOOLS}) == len(fetch.TOOLS)


def test_the_folders_are_the_ones_the_package_assembler_takes_its_toolchain_from(tmp_path):
    inputs_by_folder = {}
    for tool in fetch.TOOLS:
        directory = tmp_path / tool.folder
        executable = directory / tool.executable
        executable.parent.mkdir(parents=True)
        executable.write_bytes(b"x")
        inputs_by_folder[tool.folder] = directory
    python_directory = tmp_path / "python"
    python_directory.mkdir()
    (python_directory / "python.exe").write_bytes(b"x")
    inputs = package_windows.PackageInputs(
        build_directory=tmp_path,
        source_directory=tmp_path,
        llvm_mingw_directory=inputs_by_folder["llvm-mingw"],
        cmake_directory=inputs_by_folder["cmake"],
        ninja_directory=inputs_by_folder["ninja"],
        python_directory=python_directory,
    )
    package_windows.add_toolchain(inputs, tmp_path / "package")  # raises if a file is not where it looks
    assert (tmp_path / "package" / "toolchain" / "llvm-mingw" / "bin" / "clang.exe").is_file()


# -- Downloading ----------------------------------------------------------------------------------------


def test_the_archive_is_unpacked_without_the_folder_it_wraps_everything_in(served_tool, tmp_path):
    destination = tmp_path / "toolchain"
    folder = fetch.fetch_tool(served_tool, destination, tmp_path / "downloads")
    assert folder == destination / "tool"
    assert (folder / "bin" / "tool.exe").read_bytes() == b"program"
    assert (folder / "README.txt").read_bytes() == b"read me"


def test_an_archive_that_wraps_nothing_is_unpacked_as_it_is(tmp_path):
    archive = tmp_path / "ninja.zip"
    tool = tool_for(archive, make_archive(archive, {"ninja.exe": b"ninja"}), root="", executable="ninja.exe")
    folder = fetch.fetch_tool(tool, tmp_path / "toolchain", tmp_path / "downloads")
    assert (folder / "ninja.exe").read_bytes() == b"ninja"


def test_nothing_is_left_beside_the_unpacked_folder(served_tool, tmp_path):
    destination = tmp_path / "toolchain"
    fetch.fetch_tool(served_tool, destination, tmp_path / "downloads")
    assert [path.name for path in destination.iterdir()] == ["tool"]


def test_a_download_with_the_wrong_hash_is_refused_and_not_kept(served_tool, tmp_path):
    wrong = dataclasses.replace(served_tool, sha256="0" * 64)
    cache = tmp_path / "downloads"
    with pytest.raises(fetch.ToolchainError, match="expects " + "0" * 64):
        fetch.fetch_tool(wrong, tmp_path / "toolchain", cache)
    assert list(cache.iterdir()) == []
    assert not (tmp_path / "toolchain" / "tool").exists()


def test_the_message_of_a_refused_download_says_both_hashes(served_tool, tmp_path):
    wrong = dataclasses.replace(served_tool, sha256="f" * 64)
    with pytest.raises(fetch.ToolchainError) as problem:
        fetch.fetch_tool(wrong, tmp_path / "toolchain", tmp_path / "downloads")
    assert served_tool.sha256 in str(problem.value)
    assert "f" * 64 in str(problem.value)


def test_an_archive_that_is_already_downloaded_is_not_downloaded_again(served_tool, tmp_path):
    cache = tmp_path / "downloads"
    fetch.fetch_tool(served_tool, tmp_path / "first", cache)
    gone = dataclasses.replace(served_tool, url="file:///nowhere/tool-1.0.zip")  # nowhere to get it from
    folder = fetch.fetch_tool(gone, tmp_path / "second", cache)
    assert (folder / "bin" / "tool.exe").is_file()


def test_a_damaged_archive_in_the_cache_is_downloaded_again(served_tool, tmp_path):
    cache = tmp_path / "downloads"
    cache.mkdir()
    (cache / "tool-1.0.zip").write_bytes(b"cut short")
    folder = fetch.fetch_tool(served_tool, tmp_path / "toolchain", cache)
    assert (folder / "bin" / "tool.exe").is_file()
    assert fetch.sha256_of(cache / "tool-1.0.zip") == served_tool.sha256


class FlakyServer:
    """Stands in for urllib.request.urlopen: fails some times, then serves the real thing."""

    def __init__(self, failures: int, real_urlopen):
        self.failures_left = failures
        self.calls = 0
        self.real_urlopen = real_urlopen

    def __call__(self, request):
        self.calls += 1
        if self.failures_left:
            self.failures_left -= 1
            raise ConnectionResetError("the connection was closed")
        return self.real_urlopen(request)


def test_a_broken_connection_is_tried_again(served_tool, tmp_path, monkeypatch):
    server = FlakyServer(failures=2, real_urlopen=fetch.urllib.request.urlopen)
    monkeypatch.setattr(fetch.urllib.request, "urlopen", server)
    folder = fetch.fetch_tool(served_tool, tmp_path / "toolchain", tmp_path / "downloads")
    assert (folder / "bin" / "tool.exe").is_file()
    assert server.calls == 3


def test_it_gives_up_after_the_attempts_and_says_which_address_failed(served_tool, tmp_path, monkeypatch):
    server = FlakyServer(failures=99, real_urlopen=fetch.urllib.request.urlopen)
    monkeypatch.setattr(fetch.urllib.request, "urlopen", server)
    with pytest.raises(fetch.ToolchainError, match="could not download .*tool-1.0.zip"):
        fetch.fetch_tool(served_tool, tmp_path / "toolchain", tmp_path / "downloads")
    assert server.calls == fetch.DOWNLOAD_ATTEMPTS


def test_the_request_carries_a_plain_user_agent(served_tool, tmp_path, monkeypatch):
    requests = []
    real_urlopen = fetch.urllib.request.urlopen

    def recording_urlopen(request):
        requests.append(request)
        return real_urlopen(request)

    monkeypatch.setattr(fetch.urllib.request, "urlopen", recording_urlopen)
    fetch.fetch_tool(served_tool, tmp_path / "toolchain", tmp_path / "downloads")
    assert requests[0].get_header("User-agent") == fetch.USER_AGENT
    assert "@" not in fetch.USER_AGENT  # no e-mail address or any other personal data


# -- Unpacking ------------------------------------------------------------------------------------------


def test_an_archive_with_a_path_that_leaves_its_folder_is_refused(tmp_path):
    archive = tmp_path / "evil.zip"
    sha256 = make_archive(archive, {"tool-1.0/bin/tool.exe": b"x", "tool-1.0/../../outside.txt": b"x"})
    with pytest.raises(fetch.ToolchainError, match="leaves its folder"):
        fetch.fetch_tool(tool_for(archive, sha256), tmp_path / "toolchain", tmp_path / "downloads")
    assert not (tmp_path / "toolchain" / "outside.txt").exists()
    assert not (tmp_path / "toolchain" / "tool").exists()


def test_an_archive_with_something_outside_its_wrapping_folder_is_refused(tmp_path):
    archive = tmp_path / "odd.zip"
    sha256 = make_archive(archive, {"tool-1.0/bin/tool.exe": b"x", "other/file.txt": b"x"})
    with pytest.raises(fetch.ToolchainError, match=r"inside tool-1\.0/"):
        fetch.fetch_tool(tool_for(archive, sha256), tmp_path / "toolchain", tmp_path / "downloads")


def test_an_archive_without_the_program_is_refused_and_leaves_nothing_behind(tmp_path):
    archive = tmp_path / "empty.zip"
    sha256 = make_archive(archive, {"tool-1.0/README.txt": b"x"})
    destination = tmp_path / "toolchain"
    with pytest.raises(fetch.ToolchainError, match=r"bin/tool\.exe is not in the archive"):
        fetch.fetch_tool(tool_for(archive, sha256), destination, tmp_path / "downloads")
    assert list(destination.iterdir()) == []


def test_unpacking_again_replaces_what_an_earlier_run_left(served_tool, tmp_path):
    destination = tmp_path / "toolchain"
    old_file = destination / "tool" / "old.txt"
    old_file.parent.mkdir(parents=True)
    old_file.write_text("from an earlier version")
    (destination / "tool.unpacking").mkdir()  # a run that was cut short
    folder = fetch.fetch_tool(served_tool, destination, tmp_path / "downloads")
    assert not (folder / "old.txt").exists()
    assert [path.name for path in destination.iterdir()] == ["tool"]


# -- Command line ---------------------------------------------------------------------------------------


def test_the_command_line_fetches_what_was_asked_for(served_tool, tmp_path, monkeypatch, capsys):
    monkeypatch.setattr(fetch, "TOOLS", (served_tool,))
    destination = tmp_path / "toolchain"
    assert fetch.main(["--destination", str(destination), "--tool", "tool"]) == 0
    assert (destination / "tool" / "bin" / "tool.exe").is_file()
    assert (destination / "downloads" / "tool-1.0.zip").is_file()  # the default cache folder
    assert "Tool 1.0" in capsys.readouterr().out


def test_the_command_line_keeps_the_archives_where_it_is_told(served_tool, tmp_path, monkeypatch):
    monkeypatch.setattr(fetch, "TOOLS", (served_tool,))
    cache = tmp_path / "kept"
    assert fetch.main(["--destination", str(tmp_path / "toolchain"), "--cache-directory", str(cache)]) == 0
    assert (cache / "tool-1.0.zip").is_file()


def test_the_command_line_fails_with_code_2_and_a_message_for_a_refused_download(
    served_tool, tmp_path, monkeypatch, capsys
):
    wrong = dataclasses.replace(served_tool, sha256="0" * 64)
    monkeypatch.setattr(fetch, "TOOLS", (wrong,))
    assert fetch.main(["--destination", str(tmp_path / "toolchain")]) == 2
    assert "error:" in capsys.readouterr().err


def test_the_command_line_fails_with_code_2_for_a_file_that_is_not_a_zip(tmp_path, monkeypatch, capsys):
    not_a_zip = tmp_path / "tool-1.0.zip"
    not_a_zip.write_bytes(b"this is not a zip")
    broken = tool_for(not_a_zip, hashlib.sha256(b"this is not a zip").hexdigest())
    monkeypatch.setattr(fetch, "TOOLS", (broken,))
    assert fetch.main(["--destination", str(tmp_path / "toolchain")]) == 2
    assert "error:" in capsys.readouterr().err


def test_an_unknown_tool_is_an_error_of_the_command_line(tmp_path):
    with pytest.raises(SystemExit) as stop:
        fetch.main(["--destination", str(tmp_path), "--tool", "nonexistent"])
    assert stop.value.code == 2
