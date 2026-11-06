"""What tests read from mods/jamie_marriage/mod.c: its texts, tables and macros, as the C compiler reads them,
and the game's bytes and message window, as the mod and the game use them.

Lo que las pruebas leen de mods/jamie_marriage/mod.c: sus textos, tablas y macros, como los lee el
compilador de C, y los bytes y la ventana de mensajes del juego, como los usan el mod y el juego.

tests/test_jamie_as_the_spouse.py (the spouse's conversations and birthday) and the tests of the scenes that
Jamie borrows (tests/test_jamie_scene_lines.py, tests/test_jamie_pregnancy_and_birth.py and
tests/test_jamie_special_days.py) share it.
"""

import re
import struct
import sys
from dataclasses import dataclass
from functools import partial
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from project import DISC_DIRECTORY, REPOSITORY_ROOT, SCRIPT_DIRECTORY  # noqa: E402

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
JAMIE_SPEAKS = b"\x1e\x02"  # the speaker code of an event message: 0x1E and the character's number
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
# The scenes that Jamie borrows: the rows of jamie_scenes[], with the arrays and macros that they name.
# ---------------------------------------------------------------------------------------------------------


@dataclass(frozen=True)
class Scene:
    """A row of jamie_scenes[] in mod.c."""

    name: str  # its name in JamieSceneId, such as SCENE_PREGNANCY_AS_THE_WIFE
    kind: str  # SCENE_OF_A_PARTNER or SCENE_FOR_EVERY_SPOUSE
    jamie_is_the_wife: bool
    is_the_birth: bool
    partner: int
    event: int
    script_name: str
    message_file: str
    partner_bytes: tuple[int, ...]
    first_unused_message: int
    first_message_after_the_unused_ones: int

    @property
    def unused_messages(self) -> range:
        return range(self.first_unused_message, self.first_message_after_the_unused_ones)


def hex_macros(source: str) -> dict[str, int]:
    """The macros whose value is a hexadecimal number, such as `#define HOUSE_CASE_ADDRESS 0x801668D0u`."""
    return {
        name: int(value, 16)
        for name, value in re.findall(r"^#define\s+(\w+)\s+0x([0-9A-Fa-f]+)u\b", source, re.MULTILINE)
    }


def uint16_arrays(source: str) -> dict[str, tuple[int, ...]]:
    """The values of every `static const uint16_t <name>[] = {...};`, by name."""
    arrays = {}
    for name, body in re.findall(r"static const uint16_t (\w+)\[\] = \{(.*?)\};", source, re.DOTALL):
        body = re.sub(r"/\*.*?\*/", "", body, flags=re.DOTALL)
        arrays[name] = tuple(int(value, 16) for value in re.findall(r"0x([0-9A-Fa-f]+)", body))
    return arrays


# A macro that stands for designated initializers, such as `#define AS_THE_WIFE .jamie_is_the_wife = true,
# ...`, or, with a parameter, `#define PARTNER_BYTES(array) .partner_bytes = (array), ...`.
INITIALIZER_MACRO = re.compile(r"^#define\s+(\w+)(?:\((\w+)\))?\s+(\..*?)\s*$", re.MULTILINE)


def with_the_argument(match: re.Match[str], body: str, parameter: str) -> str:
    """The body of a macro with a parameter, for the call that `match` found, with its argument in place."""
    return re.sub(rf"\b{parameter}\b", match.group(1), body)


def expand_initializer_macros(text: str, source: str) -> str:
    """`text` with the initializer macros of `source` replaced by what they stand for, as in C."""
    for name, parameter, body in INITIALIZER_MACRO.findall(source):
        if parameter:
            text = re.sub(
                rf"\b{name}\((\w+)\)", partial(with_the_argument, body=body, parameter=parameter), text
            )
        else:
            text = re.sub(rf"\b{name}\b", body.replace("\\", r"\\"), text)
    return text


def single_value(fields: dict[str, list[Token]], field: str, where: str) -> str:
    """The text of a field that holds one token, taken out of `fields`."""
    tokens = fields.pop(field)
    assert len(tokens) == 1, f"{where}: .{field} is not a single value"
    return tokens[0].text


def read_scenes(source: str) -> dict[str, Scene]:
    """The rows of jamie_scenes[], `[SCENE_...] = {.field = value, ...}`, by their name in JamieSceneId."""
    start = re.search(r"static const JamieScene jamie_scenes\[JAMIE_SCENE_COUNT\] = \{", source)
    assert start, f"no jamie_scenes[] in {MOD_SOURCE.name}"
    end = source.index("\n};", start.end())
    string_macros, number_macros = read_macros(source)
    numbers = {**number_macros, **hex_macros(source)}
    arrays = uint16_arrays(source)
    scenes = {}
    rows = split_on_top_level_commas(tokens_of(expand_initializer_macros(source[start.end() : end], source)))
    for row in rows:
        texts = [token.text for token in row]
        assert texts[0] == "[" and texts[2:5] == ["]", "=", "{"] and texts[-1] == "}", (
            "use [SCENE_...] = {...}"
        )
        name = texts[1]
        fields = {}
        for field in split_on_top_level_commas(row[5:-1]):
            assert field[0].text == "." and field[2].text == "=", f"{name}: use .field = value"
            fields[field[1].text] = field[3:]
        array = [token.text for token in fields.pop("partner_bytes")]
        assert len(array) == 3 and array[0] == "(" and array[2] == ")", (
            f"{name}: .partner_bytes is not (array)"
        )
        count = [token.text for token in fields.pop("partner_byte_count")]
        assert count == ["HM_MM_ARRAY_COUNT", "(", array[1], ")"], f"{name}: the count is not its array's"
        scene = Scene(
            name=name,
            kind=single_value(fields, "kind", name),
            jamie_is_the_wife=single_value(fields, "jamie_is_the_wife", name) == "true",
            is_the_birth=fields.pop("is_the_birth", [Token("name", "false")])[0].text == "true",
            partner=numbers[single_value(fields, "partner", name)],
            event=numbers[single_value(fields, "event", name)],
            script_name=evaluate_string(fields.pop("script_name"), string_macros).decode("utf-8"),
            message_file=evaluate_string(fields.pop("message_file"), string_macros).decode("utf-8"),
            partner_bytes=arrays[array[1]],
            first_unused_message=int(single_value(fields, "first_unused_message", name).rstrip("u")),
            first_message_after_the_unused_ones=int(
                single_value(fields, "first_message_after_the_unused_ones", name).rstrip("u")
            ),
        )
        evaluate_string(fields.pop("name"), string_macros)  # only for the log, but it must be a text
        assert not fields, f"{name}: unknown fields {sorted(fields)}"
        scenes[name] = scene
    return scenes


# ---------------------------------------------------------------------------------------------------------
# The game's events and their message files.
# ---------------------------------------------------------------------------------------------------------

# The tables of events in main.dol: for each event, a pointer to the name of its script, and one to the path
# of its English messages (or 0).
EVENT_SCRIPT_TABLE_ADDRESS = 0x8026BBA0
EVENT_MESSAGE_TABLE_ADDRESS = 0x8026B740
MESSAGE_FILE_HEADER = struct.Struct(">HH4xII")  # speaker, highest message, 4 other bytes, table, text


def event_message_file(scene: Scene, letter: str) -> bytes:
    """The scene's message file in a language, found without regard to case, as the game asks for it."""
    folder = DISC_DIRECTORY / "files" / f"message_{letter}" / "event"
    wanted = f"{scene.message_file}_{letter}.bin".lower()
    matches = [path for path in folder.iterdir() if path.name.lower() == wanted]
    assert matches, f"the disc has no {wanted} in {folder}"
    return matches[0].read_bytes()


def room_of_the_unused_messages(scene: Scene, letter: str) -> int:
    """How many bytes of text the scene's unused messages take in that language's file, one after another."""
    where = f"{scene.name}, language {letter}"
    data = event_message_file(scene, letter)
    _speaker, highest_message, table, _text = MESSAGE_FILE_HEADER.unpack_from(data)
    assert scene.first_message_after_the_unused_ones <= highest_message, where
    offsets = struct.unpack_from(f">{highest_message + 1}I", data, table)
    room = offsets[scene.first_unused_message : scene.first_message_after_the_unused_ones + 1]
    assert list(room) == sorted(room), f"{where}: the unused messages are not in a row"
    return room[-1] - room[0]


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
