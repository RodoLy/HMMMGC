"""Jamie's lines in the scenes that the mod lends Jamie, checked the way the game will get them.

Las frases de Jamie en las escenas que el mod le presta a Jamie, revisadas como las recibirá el juego.

mods/jamie_marriage writes lines of its own for Jamie into the message files of the scenes of the pregnancy,
the birth and the special days (jamie_scenes[] and the SceneLine tables of mod.c). These tests read the lines
as the C compiler reads them and check them in every language of the disc: every scene has lines, the same
messages in all the languages; Jamie speaks each line, with letters of the console's font; the window codes
are where they belong; and every row, page and line fits. They need neither the game nor a compiler.
"""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent))

from jamie_mod_source import (  # noqa: E402
    CODES_AFTER_WINDOW_CODE,
    DISC_LANGUAGES,
    END_OF_MESSAGE,
    JAMIE_SPEAKS,
    LONGEST_MESSAGE_BYTES,
    LONGEST_ROW,
    MOD_SOURCE,
    ROWS_PER_PAGE,
    WINDOW_CODE,
    SceneLine,
    encode_for_the_game,
    letters_in,
    pages_of,
    read_scene_languages,
    read_scenes,
    readable,
)

NEW_ROW_OR_PAGE_CODES = (0x04, 0x05)  # each followed by 0x1D


@pytest.fixture(scope="module")
def source() -> str:
    return MOD_SOURCE.read_text(encoding="utf-8")


@pytest.fixture(scope="module")
def scene_names(source) -> set[str]:
    return set(read_scenes(source))


@pytest.fixture(scope="module")
def scene_lines(source) -> dict[str, list[SceneLine]]:
    return read_scene_languages(source)


def every_line(scene_lines):
    """Each line of each language, with where it is (for the messages) and its bytes in the game."""
    for letter, lines in scene_lines.items():
        for line in lines:
            yield f"language {letter}, {line.scene}, message {line.number}", encode_for_the_game(line.text)


def test_every_language_of_the_disc_has_the_same_lines_for_every_scene(scene_lines, scene_names):
    assert set(scene_lines) == DISC_LANGUAGES
    english = sorted((line.scene, line.number) for line in scene_lines["e"])
    assert len(english) == len(set(english)), "a message of a scene appears twice in English"
    assert {scene for scene, _ in english} == scene_names, "a scene has no lines"
    for letter, lines in scene_lines.items():
        assert sorted((line.scene, line.number) for line in lines) == english, f"language {letter} differs"


def test_every_line_is_spoken_by_jamie_with_letters_of_the_font(scene_lines):
    for where, encoded in every_line(scene_lines):  # encode_for_the_game raises on a letter the font lacks
        assert encoded.startswith(JAMIE_SPEAKS), f"{where} does not start with JAMIE_SPEAKS"
        assert JAMIE_SPEAKS not in encoded[len(JAMIE_SPEAKS) :], f"{where} names a speaker twice"
        # The game passes the message through vsprintf (fn_801A1C88), where % would start a format.
        assert b"%" not in encoded, f"{where}: % is a format in the game"
        assert b"?" not in encoded, f"{where}: ? must become 0x9D"


def test_every_line_has_the_window_codes_where_they_belong(scene_lines):
    for where, encoded in every_line(scene_lines):
        assert encoded.endswith(END_OF_MESSAGE), f"{where} does not end with END_OF_MESSAGE"
        body = encoded[len(JAMIE_SPEAKS) :].removesuffix(END_OF_MESSAGE)
        assert b"\x01\x08" not in body, f"{where} ends twice"
        for position, byte in enumerate(body):
            if byte != WINDOW_CODE or (position > 0 and body[position - 1] == WINDOW_CODE):
                continue
            code = body[position + 1] if position + 1 < len(body) else None
            assert code in CODES_AFTER_WINDOW_CODE, f"{where}: unknown window code {code!r}"
            if code in NEW_ROW_OR_PAGE_CODES:
                assert body[position + 2] == 0x1D, f"{where}: a new row or page without 0x1D"


def test_every_row_fits_the_window_and_every_line_the_mods_buffer(scene_lines):
    for where, encoded in every_line(scene_lines):
        assert len(encoded) <= LONGEST_MESSAGE_BYTES, f"{where} is too long"
        for page in pages_of(encoded[len(JAMIE_SPEAKS) :]):
            assert len(page) <= ROWS_PER_PAGE, f"{where}: {readable(encoded)}"
            for row in page:
                assert letters_in(row) <= LONGEST_ROW, f"{where}: {row!r} has {letters_in(row)} letters"
