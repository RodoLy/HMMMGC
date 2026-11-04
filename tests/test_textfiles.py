"""The helpers that read and write text the same way on every platform.

Los ayudantes que leen y escriben texto igual en todas las plataformas.

Python's own defaults differ on Windows (ANSI code page, "\\r\\n"), and a Linux machine cannot
see that difference in the result, so one test checks what the helper asks of Python.
"""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from textfiles import read_program_output, read_text, write_text  # noqa: E402

TEXT_WITH_ACCENTS_AND_AN_ARROW = "instrucción\tañadir → r3\nsegunda línea\n"


def test_text_is_written_as_utf8(tmp_path):
    path = tmp_path / "report.txt"
    write_text(path, TEXT_WITH_ACCENTS_AND_AN_ARROW)
    assert path.read_bytes() == TEXT_WITH_ACCENTS_AND_AN_ARROW.encode("utf-8")


def test_what_was_written_is_read_back_unchanged(tmp_path):
    path = tmp_path / "report.txt"
    write_text(path, TEXT_WITH_ACCENTS_AND_AN_ARROW)
    assert read_text(path) == TEXT_WITH_ACCENTS_AND_AN_ARROW


def test_the_encoding_and_the_end_of_line_are_asked_for_and_not_left_to_the_platform(tmp_path, monkeypatch):
    requests = []
    original_write_text = Path.write_text

    def write_text_and_remember(self, data, encoding=None, errors=None, newline=None):
        requests.append({"encoding": encoding, "newline": newline})
        return original_write_text(self, data, encoding=encoding, errors=errors, newline=newline)

    monkeypatch.setattr(Path, "write_text", write_text_and_remember)
    write_text(tmp_path / "command.txt", "command=stop\n")
    assert requests == [{"encoding": "utf-8", "newline": "\n"}]


def test_the_strict_reader_refuses_bytes_that_are_not_utf8(tmp_path):
    path = tmp_path / "table.tsv"
    path.write_bytes(b"caf\xe9\n")  # Latin-1, as a Windows editor might have saved it
    with pytest.raises(UnicodeDecodeError):
        read_text(path)


def test_the_output_of_another_program_is_read_even_if_some_of_its_bytes_are_not_utf8(tmp_path):
    path = tmp_path / "game.log"
    path.write_bytes(b"[lockstep] summary: checks=3 \xff\xfe reports=0\n")
    assert read_program_output(path) == "[lockstep] summary: checks=3 �� reports=0\n"
