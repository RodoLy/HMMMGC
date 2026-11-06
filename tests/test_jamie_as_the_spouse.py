"""Jamie as the spouse: lines that fit the game's font and message window in every language of the disc,
and a birthday.

Jamie como cónyuge: frases que caben en la fuente y en la ventana de mensajes del juego, en cada idioma del
disco, y un cumpleaños.

mods/jamie_marriage/mod.c writes its texts in UTF-8 and turns them into the game's bytes when it uses them
(encode_for_the_game). These tests read the tables of that file the way the C compiler does, turn each text
into bytes the way the mod does, and check what the game will get: letters that the console's font has, the
window's codes where they belong, rows and pages that fit the window, and the same messages in every
language. Two more compare the message numbers with talk.lsb and the languages with the folders of the
disc; they are skipped without the game.

The last tests build the mod's hooks into a small program with a fake memory and play what the game does
when talk.lsb shows a message of Jamie's (the start of fn_8008534C, the loading of the file, the return):
the line has to land in the loaded file in the right language and words, and nothing may change when it
should not. The same program enters an area (fn_80042D10) to see Jamie's record of the game's birthday
table change while Jamie is the spouse, and only then. They need a C compiler and ModernGekko's headers
(MODERNGEKKO_TEMPLATE, as for scripts/build_mods.sh), and are skipped without them; two more check, on the
disc, that the new birthday falls on a free day.
"""

import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))

from jamie_mod_source import (  # noqa: E402
    CODES_AFTER_WINDOW_CODE,
    DISC_LANGUAGES,
    END_OF_MESSAGE,
    JAMIE_CHARACTER_NUMBER,
    JAMIE_FILE_NAME,
    LONGEST_MESSAGE_BYTES,
    LONGEST_ROW,
    MOD_SOURCE,
    ROWS_PER_PAGE,
    SHOW_CHARACTER_MESSAGE_BUILTIN,
    TALK_SCRIPT,
    WINDOW_CODE,
    SpouseMessage,
    encode_for_the_game,
    letters_in,
    pages_of,
    read_languages,
    readable,
)

from lsb import load_script  # noqa: E402
from project import DISC_DIRECTORY, REPOSITORY_ROOT  # noqa: E402

# ---------------------------------------------------------------------------------------------------------
# The tests.
# ---------------------------------------------------------------------------------------------------------


@pytest.fixture(scope="module")
def languages() -> dict[str, list[SpouseMessage]]:
    return read_languages(MOD_SOURCE.read_text(encoding="utf-8"))


def every_version(languages):
    for letter, messages in languages.items():
        for message in messages:
            for version in message.versions():
                yield letter, message.number, version


def test_there_are_lines_for_every_european_language_of_the_disc(languages):
    assert set(languages) == DISC_LANGUAGES


def test_every_language_answers_the_same_messages_once(languages):
    english_numbers = [message.number for message in languages["e"]]
    assert len(english_numbers) == len(set(english_numbers)), "a message number appears twice in English"
    for letter, messages in languages.items():
        numbers = [message.number for message in messages]
        assert sorted(numbers) == sorted(english_numbers), f"language {letter} answers other messages"


def test_every_letter_is_in_the_font_and_no_text_is_a_format(languages):
    for letter, number, version in every_version(languages):
        encoded = encode_for_the_game(version)  # raises on a letter the font lacks
        # The game passes the message through vsprintf (fn_801A1C88), where % would start a format.
        assert b"%" not in encoded, f"language {letter}, message {number}: % is a format in the game"
        assert b"?" not in encoded, f"language {letter}, message {number}: ? must become 0x9D"


def test_every_message_has_the_window_codes_where_they_belong(languages):
    for letter, number, version in every_version(languages):
        encoded = encode_for_the_game(version)
        where = f"language {letter}, message {number}"
        assert encoded.endswith(END_OF_MESSAGE), f"{where} does not end with END_OF_MESSAGE"
        body = encoded.removesuffix(END_OF_MESSAGE)
        assert b"\x01\x08" not in body, f"{where} ends twice"
        for position, byte in enumerate(body):
            if byte != WINDOW_CODE or (position > 0 and body[position - 1] == WINDOW_CODE):
                continue
            code = body[position + 1] if position + 1 < len(body) else None
            assert code in CODES_AFTER_WINDOW_CODE, f"{where}: unknown window code {code!r}"
            if code in (0x04, 0x05):
                assert body[position + 2] == 0x1D, f"{where}: a new row or page without 0x1D"


def test_every_row_fits_the_window(languages):
    for letter, number, version in every_version(languages):
        encoded = encode_for_the_game(version)
        for page in pages_of(encoded):
            assert len(page) <= ROWS_PER_PAGE, f"language {letter}, message {number}: {readable(encoded)}"
            for row in page:
                assert letters_in(row) <= LONGEST_ROW, (
                    f"language {letter}, message {number}, row {row.decode('cp1252', errors='replace')!r} "
                    f"has {letters_in(row)} letters"
                )


def test_every_message_fits_the_mods_buffer(languages):
    for letter, number, version in every_version(languages):
        assert len(encode_for_the_game(version)) <= LONGEST_MESSAGE_BYTES, f"language {letter}, {number}"


@pytest.mark.skipif(not TALK_SCRIPT.is_file(), reason="needs the game's talk.lsb (tools/extract_disc.py)")
def test_every_message_number_is_one_that_talk_lsb_shows(languages):
    instructions = load_script(TALK_SCRIPT).instructions
    shown = {
        int(instructions[index - 2].operand)
        for index, instruction in enumerate(instructions)
        if instruction.builtin_index == SHOW_CHARACTER_MESSAGE_BUILTIN
    }
    assert {message.number for message in languages["e"]} <= shown


@pytest.mark.skipif(not DISC_DIRECTORY.is_dir(), reason="needs the extracted disc (tools/extract_disc.py)")
def test_the_disc_has_jamies_messages_in_every_language_of_the_mod(languages):
    for letter in languages:
        path = DISC_DIRECTORY / "files" / f"message_{letter}" / "chara" / f"{JAMIE_FILE_NAME}_{letter}.bin"
        assert path.is_file(), f"the disc has no {path.relative_to(DISC_DIRECTORY)}"
        data = path.read_bytes()
        assert int.from_bytes(data[0:2], "big") == JAMIE_CHARACTER_NUMBER  # the speaker of the file


# ---------------------------------------------------------------------------------------------------------
# The hooks, run on a fake memory.
# ---------------------------------------------------------------------------------------------------------

TEMPLATE_DIRECTORY = Path(os.environ.get("MODERNGEKKO_TEMPLATE", Path.home() / "ModernGekko-Template"))
MODERNGEKKO_INCLUDE_DIRECTORY = TEMPLATE_DIRECTORY / "lib" / "ModernGekko" / "include"
C_COMPILER = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")

# The fake memory of the program below: Jamie's loaded file has its table at 0x10 and its text at 0x180,
# holds messages 0 to 91 like the disc's, and the offset of message m in its table is 0x1000 + m, so that a
# table entry the mod did not touch is easy to tell.
FILE_TABLE_START = 0x10
FILE_TEXT_START = 0x180
FILE_HIGHEST_MESSAGE = 91
UNTOUCHED_ENTRY_BASE = 0x1000

# Jamie's record of the birthday table of system/Itemdata.bin, as the disc has it: season -1 and day -1
# (each stored plus 2: 0x1 in the high bits of the first byte, 0x01 in the second), and two other bytes.
JAMIE_BIRTHDAY_RECORD_ON_THE_DISC = bytes.fromhex("12012222")
BIRTHDAY_TABLE_OFFSET = 0x329E0
BIRTHDAY_RECORD_BYTES = 4
BIRTHDAY_FIELD_BIAS = 2
CHARACTERS_WITH_A_BIRTHDAY_RECORD = 110  # fn_80032EE8 walks character numbers 0 to 109 every morning
WINTER = 3
JAMIES_NEW_BIRTHDAY = (WINTER, 22)
ITEM_DATA = DISC_DIRECTORY / "files" / "system" / "Itemdata.bin"

HOOKS_DRIVER = r"""
/* Plays, on a fake memory, what the game does when talk.lsb shows a message of Jamie's. */
#include MOD_SOURCE_PATH

#include <stdio.h>
#include <string.h>

#define MEMORY_START 0x80000000u
#define MEMORY_BYTES 0x01800000u
#define SAVE_DATA 0x80900000u
#define PATH 0x80800000u
#define FILE_ADDRESS 0x80A00000u
#define WINDOW 0x802DBA28u
#define GAME_DATA 0x80B00000u

static uint8_t memory[MEMORY_BYTES];

static uint64_t fake_read(CPUState* cpu, uint32_t address, uint8_t size)
{
    (void)cpu;
    uint64_t value = 0;
    for (uint8_t index = 0; index < size; ++index) {
        value = (value << 8) | memory[address - MEMORY_START + index];
    }
    return value;
}

static void fake_write(CPUState* cpu, uint32_t address, uint64_t value, uint8_t size)
{
    (void)cpu;
    for (uint8_t index = 0; index < size; ++index) {
        memory[address - MEMORY_START + index] = (uint8_t)(value >> (8 * (size - 1 - index)));
    }
}

static void set_up(uint32_t spouse, uint32_t player_character, const char* path)
{
    memset(memory, 0, sizeof memory);
    fake_write(NULL, SAVE_DATA_POINTER_ADDRESS, SAVE_DATA, WORD_BYTES);
    fake_write(NULL, SAVE_DATA + SPOUSE_OFFSET, spouse, WORD_BYTES);
    fake_write(NULL, SAVE_DATA + PLAYER_CHARACTER_OFFSET, player_character, WORD_BYTES);
    fake_write(NULL, MESSAGE_FILE_PATHS_ADDRESS + JAMIE_CHARACTER_NUMBER * WORD_BYTES, PATH, WORD_BYTES);
    memcpy(&memory[PATH - MEMORY_START], path, strlen(path) + 1);
    fake_write(NULL, FILE_ADDRESS, JAMIE_CHARACTER_NUMBER, HALF_WORD_BYTES);
    fake_write(NULL, FILE_ADDRESS + MESSAGE_FILE_HEADER_HIGHEST_MESSAGE, FILE_HIGHEST_MESSAGE,
               HALF_WORD_BYTES);
    fake_write(NULL, FILE_ADDRESS + MESSAGE_FILE_HEADER_TABLE_START, FILE_TABLE_START, WORD_BYTES);
    fake_write(NULL, FILE_ADDRESS + MESSAGE_FILE_HEADER_TEXT_START, FILE_TEXT_START, WORD_BYTES);
    for (uint32_t message = 0; message <= FILE_HIGHEST_MESSAGE; ++message) {
        fake_write(NULL, FILE_ADDRESS + FILE_TABLE_START + message * WORD_BYTES,
                   UNTOUCHED_ENTRY_BASE + message, WORD_BYTES);
    }
}

/* fn_8008534C starts, loads Jamie's file with fn_80029E74 (unless it gives up first) and returns. */
static void show_message(uint32_t message_number, bool gives_up_before_loading)
{
    static CPUState cpu;
    cpu.external_read = fake_read;
    cpu.external_write = fake_write;
    cpu.gpr[3] = WINDOW;
    cpu.gpr[4] = JAMIE_CHARACTER_NUMBER;
    cpu.gpr[5] = message_number;
    before_a_character_message_is_shown(&cpu);
    before_a_character_message_is_shown(&cpu); /* a hook may run twice for one call */
    if (gives_up_before_loading) {
        after_a_character_message_is_shown(&cpu);
    }
    cpu.gpr[3] = FILE_ADDRESS;
    after_a_file_is_loaded(&cpu);
    after_a_character_message_is_shown(&cpu);
}

/* One line: the case, the file's highest message, the table entry of the message, the bytes it points at. */
static void report(const char* name, uint32_t message_number)
{
    const uint32_t highest = (uint32_t)fake_read(NULL, FILE_ADDRESS + MESSAGE_FILE_HEADER_HIGHEST_MESSAGE,
                                                 HALF_WORD_BYTES);
    const uint32_t entry = (uint32_t)fake_read(NULL, FILE_ADDRESS + FILE_TABLE_START +
                                                         message_number * WORD_BYTES, WORD_BYTES);
    printf("%s %u %u ", name, highest, entry);
    if (entry < UNTOUCHED_ENTRY_BASE) {
        const uint8_t* text = &memory[FILE_ADDRESS + FILE_TEXT_START + entry - MEMORY_START];
        for (uint32_t index = 0; index < LONGEST_SPOUSE_MESSAGE_BYTES; ++index) {
            printf("%02x", text[index]);
            if (index >= 3 && memcmp(&text[index - 3], END_OF_MESSAGE, 4) == 0) {
                break;
            }
        }
    }
    printf("\n");
}

/*
 * Enters an area (fn_80042D10) with Jamie's record of the game's birthday table as the disc has it, and
 * prints the record afterwards: the case, then its four bytes.
 */
static void report_birthday(const char* name, uint32_t spouse)
{
    set_up(spouse, PLAYER_CHARACTER_THE_BOY, "/message_e/chara/Rion_e.bin");
    fake_write(NULL, GAME_DATA_POINTER_ADDRESS, GAME_DATA, WORD_BYTES);
    const uint32_t record = GAME_DATA + BIRTHDAY_TABLE_OFFSET +
                            JAMIE_CHARACTER_NUMBER * BIRTHDAY_RECORD_BYTES;
    fake_write(NULL, record, JAMIE_BIRTHDAY_RECORD_ON_THE_DISC, WORD_BYTES);
    static CPUState cpu;
    cpu.external_read = fake_read;
    cpu.external_write = fake_write;
    before_the_characters_are_placed(&cpu);
    before_the_characters_are_placed(&cpu); /* a hook may run twice for one call */
    printf("%s %08x\n", name, (unsigned int)fake_read(NULL, record, WORD_BYTES));
}

int main(void)
{
    set_up(JAMIE_CHARACTER_NUMBER, PLAYER_CHARACTER_THE_BOY, "/message_s/chara/Rion_s.bin");
    show_message(MESSAGE_RAIN_WITH_LITTLE_FRIENDSHIP, false);
    report("spanish_rain", MESSAGE_RAIN_WITH_LITTLE_FRIENDSHIP);

    set_up(JAMIE_CHARACTER_NUMBER, PLAYER_CHARACTER_THE_BOY, "/message_e/chara/Rion_e.bin");
    show_message(MESSAGE_AFTER_THE_PLAYER_CHOSE_A_NAME, false);
    report("english_message_beyond_the_file", MESSAGE_AFTER_THE_PLAYER_CHOSE_A_NAME);

    set_up(JAMIE_CHARACTER_NUMBER, 1, "/message_f/chara/Rion_f.bin");
    show_message(MESSAGE_THE_PLAYER_SHOWS_THE_BABY, false);
    report("french_for_a_girl", MESSAGE_THE_PLAYER_SHOWS_THE_BABY);

    set_up(MODEL_PARTNER_CHARACTER_NUMBER, PLAYER_CHARACTER_THE_BOY, "/message_e/chara/Rion_e.bin");
    show_message(MESSAGE_RAIN_WITH_LITTLE_FRIENDSHIP, false);
    report("jamie_is_not_the_spouse", MESSAGE_RAIN_WITH_LITTLE_FRIENDSHIP);

    set_up(JAMIE_CHARACTER_NUMBER, PLAYER_CHARACTER_THE_BOY, "/message/chara/Rion.bin");
    show_message(MESSAGE_RAIN_WITH_LITTLE_FRIENDSHIP, false);
    report("japanese_folder", MESSAGE_RAIN_WITH_LITTLE_FRIENDSHIP);

    set_up(JAMIE_CHARACTER_NUMBER, PLAYER_CHARACTER_THE_BOY, "/message_e/chara/Rion_e.bin");
    show_message(MESSAGE_RAIN_WITH_LITTLE_FRIENDSHIP, true);
    report("gave_up_before_loading", MESSAGE_RAIN_WITH_LITTLE_FRIENDSHIP);

    set_up(JAMIE_CHARACTER_NUMBER, PLAYER_CHARACTER_THE_BOY, "/message_e/chara/Rion_e.bin");
    show_message(36, false);
    report("message_without_a_line", 36);

    report_birthday("birthday_as_the_spouse", JAMIE_CHARACTER_NUMBER);
    report_birthday("birthday_as_the_rival", MODEL_PARTNER_CHARACTER_NUMBER);
    return 0;
}
"""


@pytest.fixture(scope="module")
def hook_results(tmp_path_factory) -> dict[str, tuple[int, int, bytes]]:
    if C_COMPILER is None or not MODERNGEKKO_INCLUDE_DIRECTORY.is_dir():
        pytest.skip("needs a C compiler and ModernGekko's headers (set MODERNGEKKO_TEMPLATE)")
    work = tmp_path_factory.mktemp("hooks")
    subprocess.run(
        [sys.executable, str(REPOSITORY_ROOT / "tools" / "export_mod_symbols.py"), "--out"]
        + [str(work / "G4AEE9_symbols.h")],
        check=True,
        capture_output=True,
    )
    driver = HOOKS_DRIVER
    for name, value in {
        "FILE_TABLE_START": FILE_TABLE_START,
        "FILE_TEXT_START": FILE_TEXT_START,
        "FILE_HIGHEST_MESSAGE": FILE_HIGHEST_MESSAGE,
        "UNTOUCHED_ENTRY_BASE": UNTOUCHED_ENTRY_BASE,
        "JAMIE_BIRTHDAY_RECORD_ON_THE_DISC": int.from_bytes(JAMIE_BIRTHDAY_RECORD_ON_THE_DISC, "big"),
    }.items():
        driver = f"#define {name} 0x{value:X}u\n" + driver
    (work / "driver.c").write_text(driver, encoding="utf-8")
    program = work / "driver"
    subprocess.run(
        [C_COMPILER, "-std=c11", "-Wall", "-Wextra", "-Werror", f'-DMOD_SOURCE_PATH="{MOD_SOURCE}"']
        + [f"-I{MODERNGEKKO_INCLUDE_DIRECTORY}", f"-I{REPOSITORY_ROOT / 'mods' / 'include'}", f"-I{work}"]
        + [str(work / "driver.c"), "-o", str(program)],
        check=True,
    )
    output = subprocess.run([str(program)], check=True, capture_output=True, text=True).stdout
    results = {}
    for line in output.splitlines():
        name, *values = line.split()
        if name.startswith("birthday_"):
            results[name] = bytes.fromhex(values[0])
            continue
        highest, entry, *text = values
        results[name] = (int(highest), int(entry), bytes.fromhex(text[0]) if text else b"")
    return results


def spouse_message(languages, letter: str, number: int) -> SpouseMessage:
    return next(message for message in languages[letter] if message.number == number)


def test_the_hooks_put_the_line_where_the_game_reads_it(languages, hook_results):
    rain = 12  # MESSAGE_RAIN_WITH_LITTLE_FRIENDSHIP
    highest, entry, text = hook_results["spanish_rain"]
    assert (highest, entry) == (FILE_HIGHEST_MESSAGE, 0)  # at the start of the text, the file as it was
    assert text == encode_for_the_game(spouse_message(languages, "s", rain).text)


def test_a_message_beyond_the_file_goes_after_its_table_entry_and_raises_the_highest(languages, hook_results):
    after_the_name = 100  # MESSAGE_AFTER_THE_PLAYER_CHOSE_A_NAME
    highest, entry, text = hook_results["english_message_beyond_the_file"]
    entry_end = FILE_TABLE_START + 4 * after_the_name + 4
    assert highest == after_the_name
    assert entry == entry_end - FILE_TEXT_START
    assert text == encode_for_the_game(spouse_message(languages, "e", after_the_name).text)


def test_jamie_speaks_as_a_man_to_a_girl(languages, hook_results):
    baby = 50  # MESSAGE_THE_PLAYER_SHOWS_THE_BABY
    message = spouse_message(languages, "f", baby)
    assert message.text_if_the_player_is_a_girl is not None
    assert hook_results["french_for_a_girl"][2] == encode_for_the_game(message.text_if_the_player_is_a_girl)


@pytest.mark.parametrize(
    "case, message_number",
    [
        ("jamie_is_not_the_spouse", 12),
        ("japanese_folder", 12),  # a folder without Jamie's lines
        ("gave_up_before_loading", 12),
        ("message_without_a_line", 36),
    ],
)
def test_nothing_changes_when_it_should_not(hook_results, case, message_number):
    assert hook_results[case] == (FILE_HIGHEST_MESSAGE, UNTOUCHED_ENTRY_BASE + message_number, b"")


def birthday_of(record: bytes) -> tuple[int, int]:
    """The (season, day) of a record of the birthday table, as fn_800A7D3C and fn_800A7D14 read it."""
    season = (record[0] >> 4) - BIRTHDAY_FIELD_BIAS
    day = int.from_bytes(record[1:2], "big", signed=True) - BIRTHDAY_FIELD_BIAS
    return season, day


def test_jamie_has_a_birthday_only_while_married(hook_results):
    as_the_spouse = hook_results["birthday_as_the_spouse"]
    assert birthday_of(as_the_spouse) == JAMIES_NEW_BIRTHDAY
    assert as_the_spouse[0] & 0x0F == JAMIE_BIRTHDAY_RECORD_ON_THE_DISC[0] & 0x0F  # the other bits stay
    assert as_the_spouse[2:] == JAMIE_BIRTHDAY_RECORD_ON_THE_DISC[2:]
    assert hook_results["birthday_as_the_rival"] == JAMIE_BIRTHDAY_RECORD_ON_THE_DISC


@pytest.mark.skipif(not ITEM_DATA.is_file(), reason="needs the extracted disc (tools/extract_disc.py)")
def test_jamies_new_birthday_is_free_on_the_disc():
    data = ITEM_DATA.read_bytes()
    records = [
        data[BIRTHDAY_TABLE_OFFSET + BIRTHDAY_RECORD_BYTES * character :][:BIRTHDAY_RECORD_BYTES]
        for character in range(CHARACTERS_WITH_A_BIRTHDAY_RECORD)
    ]
    assert records[JAMIE_CHARACTER_NUMBER] == JAMIE_BIRTHDAY_RECORD_ON_THE_DISC
    assert birthday_of(records[JAMIE_CHARACTER_NUMBER]) == (-1, -1)
    assert JAMIES_NEW_BIRTHDAY not in [birthday_of(record) for record in records]


@pytest.mark.skipif(not TALK_SCRIPT.is_file(), reason="needs the game's talk.lsb (tools/extract_disc.py)")
def test_no_festival_line_falls_on_jamies_new_birthday():
    # Sub 0x001A compares the day of the season (local 4) with each festival's: PUSH_INT day; LOCAL_GET 4.
    instructions = load_script(TALK_SCRIPT).instructions
    festival_days = {
        int(instruction.operand)
        for instruction, following in zip(instructions, instructions[1:], strict=False)
        if instruction.opcode.name == "PUSH_INT"
        and following.opcode.name == "LOCAL_GET"
        and following.operand == 4
    }
    assert festival_days, "talk.lsb compares no day"
    assert JAMIES_NEW_BIRTHDAY[1] not in festival_days
