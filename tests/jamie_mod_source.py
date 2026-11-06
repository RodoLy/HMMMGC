"""What tests read from mods/jamie_marriage/mod.c: its texts, tables and macros, as the C compiler reads them,
and the game's bytes and message window, as the mod and the game use them.

Lo que las pruebas leen de mods/jamie_marriage/mod.c: sus textos, tablas y macros, como los lee el
compilador de C, y los bytes y la ventana de mensajes del juego, como los usan el mod y el juego.

tests/test_jamie_as_the_spouse.py (the spouse's conversations and birthday) and
tests/test_jamie_pregnancy_and_birth.py (the scenes) share it.
"""

import re
import sys
from dataclasses import dataclass
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from project import REPOSITORY_ROOT, SCRIPT_DIRECTORY  # noqa: E402

MOD_SOURCE = REPOSITORY_ROOT / "mods" / "jamie_marriage" / "mod.c"

# The window codes (see the comment above NEW_ROW in mod.c).
WINDOW_CODE = 0x01
NEW_ROW = b"\x01\x04\x1d"
NEW_PAGE = b"\x01\x05\x1d"
END_OF_MESSAGE = b"\x01\x08\x1f\x1d"
NAME_CODES = (b"\x01\x3e", b"\x01\x40")  # what the spouse calls the player, the child's name
CODES_AFTER_WINDOW_CODE = {0x04, 0x05, 0x08, 0x3E, 0x40}

# What the window holds, as measured on the disc's own messages: 98 % of their rows have up to 28 letters,
# and their pages up to 3 rows. A name takes up to 8 letters.
LONGEST_ROW = 28
ROWS_PER_PAGE = 3
NAME_LETTERS = 8
LONGEST_MESSAGE_BYTES = 255  # what LONGEST_SPOUSE_MESSAGE_BYTES leaves for the bytes of a message

# The console's font (Windows-1252), as the mod's font_byte_for reads it.
FONT_QUESTION_MARK = 0x9D
FONT_BYTES_OUTSIDE_LATIN_1 = {"Œ": 0x8C, "œ": 0x9C, "…": 0x85}

# The European languages whose text is on the disc, by the letter of their folder (/message_e/ ...).
DISC_LANGUAGES = {"e", "s", "f", "g", "i"}
JAMIE_CHARACTER_NUMBER = 2
JAMIE_FILE_NAME = "Rion"  # the disc's name for Jamie's message file

TALK_SCRIPT = SCRIPT_DIRECTORY / "talk.lsb"
SHOW_CHARACTER_MESSAGE_BUILTIN = 52  # PUSH 125; PUSH <message>; PUSH 0; CALL_BUILTIN 52 in talk.lsb


# ---------------------------------------------------------------------------------------------------------
# Reading the C source: string literals, the string and number macros, and the tables of messages.
# ---------------------------------------------------------------------------------------------------------

TOKEN = re.compile(
    r"""
    (?P<comment>/\*.*?\*/|//[^\n]*)
    |(?P<string>"(?:\\.|[^"\\\n])*")
    |(?P<character>'(?:\\.|[^'\\\n])')
    |(?P<name>[A-Za-z_][A-Za-z_0-9]*)
    |(?P<number>\d+u?)
    |(?P<space>\s+)
    |(?P<punctuation>[{}()\[\],;=.*&])
    """,
    re.VERBOSE | re.DOTALL,
)
HEX_DIGITS = "0123456789abcdefABCDEF"
SIMPLE_ESCAPES = {"n": b"\n", "t": b"\t", '"': b'"', "'": b"'", "\\": b"\\", "0": b"\0"}


@dataclass(frozen=True)
class Token:
    kind: str
    text: str


def tokens_of(source: str) -> list[Token]:
    """The tokens of a stretch of C, without comments and spaces."""
    tokens = []
    position = 0
    while position < len(source):
        match = TOKEN.match(source, position)
        if match is None:
            raise ValueError(f"cannot read the C source at {source[position : position + 40]!r}")
        if match.lastgroup not in ("comment", "space"):
            tokens.append(Token(match.lastgroup, match.group()))
        position = match.end()
    return tokens


def bytes_of_string_literal(literal: str) -> bytes:
    """The bytes the C compiler makes of a string literal of a UTF-8 source file.

    A hexadecimal escape takes every hexadecimal digit that follows it, as in C; a letter right after one,
    such as "\\x1DA", would be swallowed, so that is refused instead.
    """
    content = literal[1:-1]
    result = bytearray()
    index = 0
    while index < len(content):
        character = content[index]
        if character != "\\":
            result += character.encode("utf-8")
            index += 1
            continue
        escape = content[index + 1]
        if escape == "x":
            digits_end = index + 2
            while digits_end < len(content) and content[digits_end] in HEX_DIGITS:
                digits_end += 1
            digits = content[index + 2 : digits_end]
            if len(digits) != 2:
                raise ValueError(f"the escape \\x{digits} in {literal} does not end where it seems to")
            result.append(int(digits, 16))
            index = digits_end
        elif escape in SIMPLE_ESCAPES:
            result += SIMPLE_ESCAPES[escape]
            index += 2
        else:
            raise ValueError(f"unexpected escape \\{escape} in {literal}")
    return bytes(result)


def read_macros(source: str) -> tuple[dict[str, bytes], dict[str, int]]:
    """The macros whose value is one or more string literals, and those whose value is a number."""
    string_macros: dict[str, bytes] = {}
    number_macros: dict[str, int] = {}
    for name, value in re.findall(r"^#define\s+(\w+)\s+(.+?)\s*(?:/\*.*)?$", source, re.MULTILINE):
        if value.startswith('"'):
            value_tokens = tokens_of(value)
            assert all(token.kind == "string" for token in value_tokens), f"macro {name} mixes strings"
            string_macros[name] = b"".join(bytes_of_string_literal(token.text) for token in value_tokens)
        elif re.fullmatch(r"\d+u?", value):
            number_macros[name] = int(value.rstrip("u"))
    return string_macros, number_macros


@dataclass(frozen=True)
class SpouseMessage:
    number: int
    text: bytes  # as the C compiler makes it: UTF-8 with the window codes
    text_if_the_player_is_a_girl: bytes | None

    def versions(self) -> list[bytes]:
        girl_version = self.text_if_the_player_is_a_girl
        return [self.text] if girl_version is None else [self.text, girl_version]


def split_on_top_level_commas(tokens: list[Token]) -> list[list[Token]]:
    parts: list[list[Token]] = [[]]
    depth = 0
    for token in tokens:
        if token.text in "{(":
            depth += 1
        elif token.text in "})":
            depth -= 1
        if token.text == "," and depth == 0:
            parts.append([])
        else:
            parts[-1].append(token)
    return [part for part in parts if part]


def evaluate_string(tokens: list[Token], string_macros: dict[str, bytes]) -> bytes:
    """A run of string literals and string macros, joined as the compiler joins them."""
    result = b""
    for token in tokens:
        if token.kind == "string":
            result += bytes_of_string_literal(token.text)
        elif token.kind == "name" and token.text in string_macros:
            result += string_macros[token.text]
        else:
            raise ValueError(f"unexpected {token.text!r} in a text")
    return result


def read_table(source: str, table_name: str) -> list[SpouseMessage]:
    """The entries of `static const SpouseMessage <table_name>[] = { ... };`."""
    start = re.search(rf"static const SpouseMessage {table_name}\[\] = \{{", source)
    assert start, f"no table {table_name} in {MOD_SOURCE.name}"
    end = source.index("\n};", start.end())
    string_macros, number_macros = read_macros(source)
    messages = []
    for entry in split_on_top_level_commas(tokens_of(source[start.end() : end])):
        assert entry[0].text == "{" and entry[-1].text == "}", f"an entry of {table_name} is not {{...}}"
        fields = {}
        for field in split_on_top_level_commas(entry[1:-1]):
            assert field[0].text == "." and field[2].text == "=", f"use .name = value in {table_name}"
            fields[field[1].text] = field[3:]
        number_tokens = fields.pop("number")
        assert len(number_tokens) == 1 and number_tokens[0].text in number_macros
        text = evaluate_string(fields.pop("text"), string_macros)
        girl_tokens = fields.pop("text_if_the_player_is_a_girl", None)
        girl_text = evaluate_string(girl_tokens, string_macros) if girl_tokens else None
        assert not fields, f"unknown fields {sorted(fields)} in {table_name}"
        messages.append(SpouseMessage(number_macros[number_tokens[0].text], text, girl_text))
    return messages


LANGUAGE_ROW = re.compile(r"\{'(\w)', (\w+), HM_MM_ARRAY_COUNT\(\2\), (\w+),\s+HM_MM_ARRAY_COUNT\(\3\)\}")


def language_rows(source: str) -> list[tuple[str, str, str]]:
    """(folder letter, spouse table, scene table) for each row of spouse_languages[]."""
    start = source.index("static const SpouseLanguage spouse_languages[] = {")
    end = source.index("\n};", start)
    rows = LANGUAGE_ROW.findall(source[start:end])
    assert rows, "spouse_languages[] lists no language"
    return rows


def read_languages(source: str) -> dict[str, list[SpouseMessage]]:
    """The spouse tables of spouse_languages[], by the letter of the language's folder."""
    return {letter: read_table(source, table_name) for letter, table_name, _ in language_rows(source)}


@dataclass(frozen=True)
class SceneLine:
    scene: str  # the name of the scene in JamieSceneId, such as SCENE_PREGNANCY_AS_THE_WIFE
    number: int
    text: bytes


def read_scene_table(source: str, table_name: str) -> list[SceneLine]:
    """The entries of `static const SceneLine <table_name>[] = { ... };`."""
    start = re.search(rf"static const SceneLine {table_name}\[\] = \{{", source)
    assert start, f"no table {table_name} in {MOD_SOURCE.name}"
    end = source.index("\n};", start.end())
    string_macros, number_macros = read_macros(source)
    lines = []
    for entry in split_on_top_level_commas(tokens_of(source[start.end() : end])):
        assert entry[0].text == "{" and entry[-1].text == "}", f"an entry of {table_name} is not {{...}}"
        fields = {}
        for field in split_on_top_level_commas(entry[1:-1]):
            assert field[0].text == "." and field[2].text == "=", f"use .name = value in {table_name}"
            fields[field[1].text] = field[3:]
        scene_tokens = fields.pop("scene")
        number_tokens = fields.pop("number")
        assert len(scene_tokens) == 1 and scene_tokens[0].text.startswith("SCENE_")
        assert len(number_tokens) == 1 and number_tokens[0].text in number_macros
        text = evaluate_string(fields.pop("text"), string_macros)
        assert not fields, f"unknown fields {sorted(fields)} in {table_name}"
        lines.append(SceneLine(scene_tokens[0].text, number_macros[number_tokens[0].text], text))
    return lines


def read_scene_languages(source: str) -> dict[str, list[SceneLine]]:
    """The scene tables of spouse_languages[], by the letter of the language's folder."""
    return {letter: read_scene_table(source, table_name) for letter, _, table_name in language_rows(source)}


# ---------------------------------------------------------------------------------------------------------
# The mod's encoding, and how the window splits a message.
# ---------------------------------------------------------------------------------------------------------


def font_byte_for(letter: str) -> int:
    """The byte of the console's font for a letter, as font_byte_for in mod.c."""
    code_point = ord(letter)
    if letter == "?":
        return FONT_QUESTION_MARK
    if code_point <= 0x7F or 0xA0 <= code_point <= 0xFF:
        return code_point
    if letter in FONT_BYTES_OUTSIDE_LATIN_1:
        return FONT_BYTES_OUTSIDE_LATIN_1[letter]
    raise ValueError(f"the console's font has no {letter!r} (U+{code_point:04X})")


def encode_for_the_game(text: bytes) -> bytes:
    """The game's bytes for a text of mod.c, as encode_for_the_game does: window codes as they are."""
    result = bytearray()
    index = 0
    while index < len(text):
        if text[index] == WINDOW_CODE:
            result += text[index : index + 2]
            index += 2
            continue
        letter_size = 1
        while index + letter_size < len(text) and (text[index + letter_size] & 0xC0) == 0x80:
            letter_size += 1
        result.append(font_byte_for(text[index : index + letter_size].decode("utf-8")))
        index += letter_size
    return bytes(result)


def pages_of(message: bytes) -> list[list[bytes]]:
    """The pages of a message, each a list of rows, without the end of the message."""
    body = message.removesuffix(END_OF_MESSAGE)
    return [page.split(NEW_ROW) for page in body.split(NEW_PAGE)]


def letters_in(row: bytes) -> int:
    for name_code in NAME_CODES:
        row = row.replace(name_code, b"N" * NAME_LETTERS)
    return len(row)


def readable(message: bytes) -> str:
    return (
        message.removesuffix(END_OF_MESSAGE)
        .replace(NEW_ROW, b" / ")
        .replace(NEW_PAGE, b" // ")
        .decode("cp1252", errors="replace")
    )
