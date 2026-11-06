"""Jamie in the pregnancy and birth scenes: the lines, where they go, and what the mod borrows from the game.

Jamie en las escenas del embarazo y del parto: las frases, dónde van y lo que el mod toma prestado del juego.

mods/jamie_marriage lends Jamie the pregnancy and birth scenes of partner 4 when the player is the boy and of
partner 13 when the player is the girl ("Babies" in mod.c). The first tests check Jamie's lines the way the
game will get them, in every language of the disc. The next ones build the mod's hooks into a small program
with a fake memory and play the moments of the field check that the mod hooks: when Jamie stands in for the
partner, when Jamie is the spouse again, and what lands in the files of the scenes. They need a C compiler
and ModernGekko's headers (MODERNGEKKO_TEMPLATE, as for scripts/build_mods.sh). The last ones check, on the
game's files, what the mod relies on: where the scripts name the partner (tools/script_actors.py), that
Jamie's lines replace messages the scenes show and fit over messages they never show, and the field check's
code at the addresses the mod reads and hooks. They are skipped without the game.
"""

import os
import re
import shutil
import struct
import subprocess
import sys
from dataclasses import dataclass
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))

from jamie_mod_source import (  # noqa: E402
    CODES_AFTER_WINDOW_CODE,
    DISC_LANGUAGES,
    END_OF_MESSAGE,
    JAMIE_CHARACTER_NUMBER,
    LONGEST_MESSAGE_BYTES,
    LONGEST_ROW,
    MOD_SOURCE,
    ROWS_PER_PAGE,
    WINDOW_CODE,
    SceneLine,
    encode_for_the_game,
    letters_in,
    pages_of,
    read_macros,
    read_scene_languages,
    readable,
)

from dol import Dol  # noqa: E402
from lsb import load_script  # noqa: E402
from project import (  # noqa: E402
    DISC_DIRECTORY,
    MAIN_DOL_PATH,
    REPOSITORY_ROOT,
    SCRIPT_DIRECTORY,
    SYMBOLS_PATH,
)
from symbols import SymbolTable  # noqa: E402

JAMIE_SPEAKS = b"\x1e\x02"  # the speaker code of an event message: 0x1E and the character's number
NEW_ROW_OR_PAGE_CODES = (0x04, 0x05)  # each followed by 0x1D
SHOW_MESSAGE_BUILTIN = 51  # PUSH_INT <message>; CALL_BUILTIN 51 in the event scripts
MESSAGE_FILE_HEADER = struct.Struct(">HH4xII")  # speaker, highest message, 4 other bytes, table, text

# ---------------------------------------------------------------------------------------------------------
# What mod.c says about the scenes.
# ---------------------------------------------------------------------------------------------------------


@dataclass(frozen=True)
class Scene:
    """A row of jamie_scenes[] in mod.c."""

    name: str  # its name in JamieSceneId, such as SCENE_PREGNANCY_AS_THE_WIFE
    jamie_is_the_wife: bool
    is_the_birth: bool
    partner: int
    event: int
    script_name: str
    message_file: str
    actor_bytes: tuple[int, ...]
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


SCENE_ROW = re.compile(
    r'\[(SCENE_\w+)\] = \{"[^"]*", (true|false), (true|false), (\w+),\s*(\w+), "(\w+)", "(\w+)",\s*(\w+),'
    r"\s*HM_MM_ARRAY_COUNT\(\8\),\s*(\d+)u, (\d+)u\}"
)


def read_scenes(source: str) -> dict[str, Scene]:
    start = source.index("static const JamieScene jamie_scenes[JAMIE_SCENE_COUNT] = {")
    table = source[start : source.index("\n};", start)]
    numbers = {**read_macros(source)[1], **hex_macros(source)}
    arrays = uint16_arrays(source)
    scenes = {}
    for match in SCENE_ROW.finditer(table):
        name, wife, birth, partner, event, script, messages, array, first, after = match.groups()
        scenes[name] = Scene(
            name=name,
            jamie_is_the_wife=wife == "true",
            is_the_birth=birth == "true",
            partner=numbers[partner],
            event=numbers[event],
            script_name=script,
            message_file=messages,
            actor_bytes=arrays[array],
            first_unused_message=int(first),
            first_message_after_the_unused_ones=int(after),
        )
    return scenes


@pytest.fixture(scope="module")
def source() -> str:
    return MOD_SOURCE.read_text(encoding="utf-8")


@pytest.fixture(scope="module")
def scenes(source) -> dict[str, Scene]:
    return read_scenes(source)


@pytest.fixture(scope="module")
def scene_lines(source) -> dict[str, list[SceneLine]]:
    return read_scene_languages(source)


def lines_of(scene_lines, letter: str, scene: Scene) -> list[SceneLine]:
    return [line for line in scene_lines[letter] if line.scene == scene.name]


def every_line(scene_lines):
    """Each line of each language, with where it is (for the messages) and its bytes in the game."""
    for letter, lines in scene_lines.items():
        for line in lines:
            yield f"language {letter}, {line.scene}, message {line.number}", encode_for_the_game(line.text)


# ---------------------------------------------------------------------------------------------------------
# Jamie's lines.
# ---------------------------------------------------------------------------------------------------------


def test_there_is_a_scene_for_each_kind_and_each_version(scenes):
    assert len(scenes) == 4
    assert {(scene.is_the_birth, scene.jamie_is_the_wife) for scene in scenes.values()} == {
        (False, True),
        (False, False),
        (True, True),
        (True, False),
    }


def test_every_language_of_the_disc_has_the_same_lines_for_every_scene(scene_lines, scenes):
    assert set(scene_lines) == DISC_LANGUAGES
    english = sorted((line.scene, line.number) for line in scene_lines["e"])
    assert len(english) == len(set(english)), "a message of a scene appears twice in English"
    assert {scene for scene, _ in english} == set(scenes), "a scene has no lines"
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


# ---------------------------------------------------------------------------------------------------------
# The hooks, run on a fake memory.
# ---------------------------------------------------------------------------------------------------------

TEMPLATE_DIRECTORY = Path(os.environ.get("MODERNGEKKO_TEMPLATE", Path.home() / "ModernGekko-Template"))
MODERNGEKKO_INCLUDE_DIRECTORY = TEMPLATE_DIRECTORY / "lib" / "ModernGekko" / "include"
C_COMPILER = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")

# The fake memory of the program below. The loaded event message file holds messages 0 to 46 (more than the
# scenes'), message m at text offset m * 0x40, and every byte of its text is 0xEE until the mod writes. Every
# byte of the loaded script is 0x55, except a PUSH_INT of the partner at each place the mod knows.
FAKE_HIGHEST_MESSAGE = 46
FAKE_MESSAGE_ROOM = 0x40
FAKE_FILE_TABLE = 0x10
FAKE_FILE_TEXT = FAKE_FILE_TABLE + 4 * (FAKE_HIGHEST_MESSAGE + 1)
FAKE_TEXT_FILLER = 0xEE
FAKE_SCRIPT_FILLER = 0x55
FAKE_SCRIPT_BYTES = 0x1000
HOUSE_AREA = 27  # the first house of the player
NOT_A_HOUSE_AREA = 0  # the farm in spring
PARTNERS_OWN_FRIENDSHIP = 100
JAMIES_FRIENDSHIP = 950
ANOTHER_SPOUSE = 5
ANOTHER_EVENT = 0x100
PLAYER_CHARACTER_THE_GIRL = 1

SCENES_DRIVER = r"""
/* Plays, on a fake memory, the moments of the field check that the mod hooks. Prints "case fact value". */
#include MOD_SOURCE_PATH

#include <stdio.h>
#include <string.h>

#define MEMORY_START 0x80000000u
#define MEMORY_BYTES 0x01800000u
#define SAVE_DATA 0x80900000u
#define PATH 0x80800000u
#define MESSAGES_FILE 0x80A00000u
#define SCRIPT_FILE 0x80B00000u
#define EVENT_MANAGER 0x80C00000u
#define WIFE PARTNER_JAMIE_STANDS_IN_FOR_AS_THE_WIFE
#define HUSBAND PARTNER_JAMIE_STANDS_IN_FOR_AS_THE_HUSBAND
#define FAKE_TEXT_BYTES ((FAKE_HIGHEST_MESSAGE + 1) * FAKE_MESSAGE_ROOM)
#define TALKS_ABOUT_THE_BIRTH CHARACTER_BIT_TALKS_ABOUT_THE_BIRTH

static uint8_t memory[MEMORY_BYTES];
static CPUState cpu;

static uint64_t fake_read(CPUState* state, uint32_t address, uint8_t size)
{
    (void)state;
    uint64_t value = 0;
    for (uint8_t index = 0; index < size; ++index) {
        value = (value << 8) | memory[address - MEMORY_START + index];
    }
    return value;
}

static void fake_write(CPUState* state, uint32_t address, uint64_t value, uint8_t size)
{
    (void)state;
    for (uint8_t index = 0; index < size; ++index) {
        memory[address - MEMORY_START + index] = (uint8_t)(value >> (8 * (size - 1 - index)));
    }
}

static uint8_t* bytes_at(uint32_t address) { return &memory[address - MEMORY_START]; }
static uint32_t word_at(uint32_t address) { return (uint32_t)fake_read(NULL, address, WORD_BYTES); }
static void set_word_at(uint32_t address, uint32_t value) { fake_write(NULL, address, value, WORD_BYTES); }

/* A married player who has just come into the first house, with every condition of the pregnancy met. */
static void set_up(uint32_t spouse, uint32_t player_character)
{
    memset(memory, 0, sizeof memory);
    memset(&cpu, 0, sizeof cpu);
    cpu.external_read = fake_read;
    cpu.external_write = fake_write;
    stand_in.scene = NULL;
    set_word_at(SAVE_DATA_POINTER_ADDRESS, SAVE_DATA);
    set_word_at(SAVE_DATA + SPOUSE_OFFSET, spouse);
    set_word_at(SAVE_DATA + PLAYER_CHARACTER_OFFSET, player_character);
    set_word_at(SAVE_DATA + DAYS_MARRIED_OFFSET, DAYS_OF_MARRIAGE_BEFORE_A_PREGNANCY);
    set_word_at(SAVE_DATA + DAILY_MARRIAGE_COUNTER_OFFSET, DAILY_MARRIAGE_COUNT_BEFORE_A_PREGNANCY);
    set_word_at(friendship_of(SAVE_DATA, JAMIE_CHARACTER_NUMBER), JAMIES_FRIENDSHIP);
    set_word_at(friendship_of(SAVE_DATA, WIFE), PARTNERS_OWN_FRIENDSHIP);
    set_word_at(friendship_of(SAVE_DATA, HUSBAND), PARTNERS_OWN_FRIENDSHIP);
    set_word_at(EVENTS_ALLOWED_ADDRESS, EVENTS_ALLOWED_BIT);
    set_word_at(CURRENT_AREA_ADDRESS, HOUSE_AREA);
    set_word_at(FIELD_CHECK_CASES_ADDRESS + HOUSE_AREA * WORD_BYTES, HOUSE_CASE_ADDRESS);
    set_word_at(FIELD_CHECK_CASES_ADDRESS + NOT_A_HOUSE_AREA * WORD_BYTES, HOUSE_CASE_ADDRESS + WORD_BYTES);
}

/* The morning of the birth: the morning checks on, 60 days of pregnancy, the wedding on another day. */
static void make_it_the_morning_of_the_birth(void)
{
    set_bit(&cpu, SAVE_DATA + MAIN_FLAGS_OFFSET, FLAG_MORNING_CHECKS);
    set_word_at(SAVE_DATA + PREGNANCY_DAYS_OFFSET, DAYS_OF_PREGNANCY_BEFORE_THE_BIRTH);
    set_word_at(SAVE_DATA + DAY_OF_THE_SEASON_OFFSET, 5);
    set_word_at(SAVE_DATA + WEDDING_DAY_OFFSET, 6);
}

static void report(const char* name, const char* fact, uint32_t value)
{
    printf("%s %s %u\n", name, fact, (unsigned int)value);
}

/* The spouse and the friendship of both partners, at a moment of the case. */
static void report_the_spouse(const char* name, const char* moment)
{
    char fact[64];
    snprintf(fact, sizeof fact, "spouse_%s", moment);
    report(name, fact, word_at(SAVE_DATA + SPOUSE_OFFSET));
    snprintf(fact, sizeof fact, "friendship_of_%u_%s", (unsigned int)WIFE, moment);
    report(name, fact, word_at(friendship_of(SAVE_DATA, WIFE)));
    snprintf(fact, sizeof fact, "friendship_of_%u_%s", (unsigned int)HUSBAND, moment);
    report(name, fact, word_at(friendship_of(SAVE_DATA, HUSBAND)));
}

/* fn_800FC2B4(event manager, event) is called. A hook may run twice for one call. */
static void start_event(uint32_t event)
{
    cpu.gpr[3] = EVENT_MANAGER;
    cpu.gpr[4] = event;
    before_an_event_starts(&cpu);
    before_an_event_starts(&cpu);
}

/* fn_80029E74(path) loads a file: the path on its entry, the loaded file on its return. */
static void load_file(const char* path, uint32_t file)
{
    memcpy(bytes_at(PATH), path, strlen(path) + 1);
    cpu.gpr[3] = PATH;
    before_a_file_is_loaded(&cpu);
    before_a_file_is_loaded(&cpu);
    cpu.gpr[3] = file;
    after_a_file_is_loaded(&cpu);
}

static void make_the_event_messages(void)
{
    const uint32_t highest_message = MESSAGES_FILE + MESSAGE_FILE_HEADER_HIGHEST_MESSAGE;
    fake_write(NULL, highest_message, FAKE_HIGHEST_MESSAGE, HALF_WORD_BYTES);
    set_word_at(MESSAGES_FILE + MESSAGE_FILE_HEADER_TABLE_START, FAKE_FILE_TABLE);
    set_word_at(MESSAGES_FILE + MESSAGE_FILE_HEADER_TEXT_START, FAKE_FILE_TEXT);
    for (uint32_t message = 0; message <= FAKE_HIGHEST_MESSAGE; ++message) {
        set_word_at(MESSAGES_FILE + FAKE_FILE_TABLE + message * WORD_BYTES, message * FAKE_MESSAGE_ROOM);
    }
    memset(bytes_at(MESSAGES_FILE + FAKE_FILE_TEXT), FAKE_TEXT_FILLER, FAKE_TEXT_BYTES);
}

/* Two lines: every entry of the table, and the whole text in hexadecimal. */
static void report_the_event_messages(const char* name)
{
    printf("%s entries", name);
    for (uint32_t message = 0; message <= FAKE_HIGHEST_MESSAGE; ++message) {
        printf(" %u", (unsigned int)word_at(MESSAGES_FILE + FAKE_FILE_TABLE + message * WORD_BYTES));
    }
    printf("\n%s text ", name);
    const uint8_t* text = bytes_at(MESSAGES_FILE + FAKE_FILE_TEXT);
    for (uint32_t index = 0; index < FAKE_TEXT_BYTES; ++index) {
        printf("%02x", text[index]);
    }
    printf("\n");
}

static void make_the_script(const JamieScene* scene)
{
    memset(bytes_at(SCRIPT_FILE), FAKE_SCRIPT_FILLER, FAKE_SCRIPT_BYTES);
    for (uint32_t index = 0; index < scene->actor_byte_count; ++index) {
        bytes_at(SCRIPT_FILE)[scene->actor_bytes[index] - 1u] = PUSH_INT_OPCODE;
        bytes_at(SCRIPT_FILE)[scene->actor_bytes[index]] = (uint8_t)scene->partner;
    }
}

static bool is_one_of_the_places(const JamieScene* scene, uint32_t offset)
{
    for (uint32_t index = 0; index < scene->actor_byte_count; ++index) {
        if (offset == scene->actor_bytes[index]) {
            return true;
        }
    }
    return false;
}

/* How many of the scene's places hold `value`, and how many other bytes are no longer make_the_script's. */
static void report_the_script(const char* name, const JamieScene* scene, uint32_t value)
{
    uint32_t places_with_the_value = 0;
    uint32_t other_bytes_changed = 0;
    for (uint32_t offset = 0; offset < FAKE_SCRIPT_BYTES; ++offset) {
        const uint8_t byte = bytes_at(SCRIPT_FILE)[offset];
        if (is_one_of_the_places(scene, offset)) {
            places_with_the_value += byte == value;
        } else if (is_one_of_the_places(scene, offset + 1u)) {
            other_bytes_changed += byte != PUSH_INT_OPCODE;
        } else {
            other_bytes_changed += byte != FAKE_SCRIPT_FILLER;
        }
    }
    report(name, "places_with_the_value", places_with_the_value);
    report(name, "other_bytes_changed", other_bytes_changed);
}

static void report_the_birth_conversation(const char* name, uint32_t partner)
{
    const uint32_t jamies_bits = character_bits_of(SAVE_DATA, JAMIE_CHARACTER_NUMBER);
    const uint32_t partners_bits = character_bits_of(SAVE_DATA, partner);
    report(name, "jamie_talks_about_the_birth", is_bit_set(&cpu, jamies_bits, TALKS_ABOUT_THE_BIRTH));
    report(name, "partner_talks_about_the_birth", is_bit_set(&cpu, partners_bits, TALKS_ABOUT_THE_BIRTH));
}

/* The birth's block sets the bit of the birth's conversation on the partner. */
static void the_birth_block_runs_for(uint32_t partner)
{
    set_bit(&cpu, character_bits_of(SAVE_DATA, partner), TALKS_ABOUT_THE_BIRTH);
}

/* The pregnancy, from the field check's entry to its return, with the files the event loads. */
static void play_the_pregnancy(const char* name, uint32_t player_character, const char* messages_path,
                               const char* script_path)
{
    set_up(JAMIE_CHARACTER_NUMBER, player_character);
    const JamieScene* scene = scene_for_the_player(&cpu, SAVE_DATA, false);
    settle_the_stand_in(&cpu);
    clear_the_ending_flag_after_the_wedding(&cpu);
    before_the_field_check_stand_in_for_the_birth(&cpu); /* not the morning: nothing to do */
    report_the_spouse(name, "at_the_entry");
    at_the_start_of_the_pregnancy_check(&cpu);
    at_the_start_of_the_pregnancy_check(&cpu);
    report_the_spouse(name, "in_the_check");
    start_event(scene->event);
    report_the_spouse(name, "as_the_event_starts");
    make_the_event_messages();
    load_file(messages_path, MESSAGES_FILE); /* fn_800FC2B4 loads the messages before it returns */
    report_the_event_messages(name);
    settle_the_stand_in(&cpu); /* the field check returns */
    report_the_spouse(name, "after");
    make_the_script(scene);
    load_file(script_path, SCRIPT_FILE); /* the event's script, when the event begins */
    report_the_script(name, scene, JAMIE_CHARACTER_NUMBER);
}

/* The pregnancy check with Jamie as the spouse, after `change` spoilt one of its conditions. */
static void check_the_pregnancy_after(const char* name, void (*change)(void))
{
    set_up(JAMIE_CHARACTER_NUMBER, PLAYER_CHARACTER_THE_BOY);
    change();
    at_the_start_of_the_pregnancy_check(&cpu);
    report_the_spouse(name, "in_the_check");
}

static void too_few_days_of_marriage(void)
{
    set_word_at(SAVE_DATA + DAYS_MARRIED_OFFSET, DAYS_OF_MARRIAGE_BEFORE_A_PREGNANCY - 1u);
}

static void too_few_counted_days(void)
{
    set_word_at(SAVE_DATA + DAILY_MARRIAGE_COUNTER_OFFSET, DAILY_MARRIAGE_COUNT_BEFORE_A_PREGNANCY - 1u);
}

static void too_little_friendship(void)
{
    set_word_at(friendship_of(SAVE_DATA, JAMIE_CHARACTER_NUMBER), FRIENDSHIP_BEFORE_A_PREGNANCY - 1u);
}

static void the_scene_has_been_seen(void)
{
    set_bit(&cpu, SAVE_DATA + STARTED_EVENTS_OFFSET, EVENT_PREGNANCY_OF_PARTNER_4);
}

static void events_are_not_allowed(void)
{
    set_word_at(EVENTS_ALLOWED_ADDRESS, 0);
}

/* The field check's entry on the morning of the birth, after `change`. A hook may run twice for one call. */
static void check_the_birth_after(const char* name, uint32_t player_character, void (*change)(void))
{
    set_up(JAMIE_CHARACTER_NUMBER, player_character);
    make_it_the_morning_of_the_birth();
    change();
    before_the_field_check_stand_in_for_the_birth(&cpu);
    before_the_field_check_stand_in_for_the_birth(&cpu);
    report_the_spouse(name, "at_the_entry");
}

static void nothing(void) {}

static void the_wedding_anniversary(void)
{
    set_word_at(SAVE_DATA + WEDDING_DAY_OFFSET, word_at(SAVE_DATA + DAY_OF_THE_SEASON_OFFSET));
}

static void outside_the_house(void)
{
    set_word_at(CURRENT_AREA_ADDRESS, NOT_A_HOUSE_AREA);
}

static void the_morning_checks_are_off(void)
{
    clear_bit(&cpu, SAVE_DATA + MAIN_FLAGS_OFFSET, FLAG_MORNING_CHECKS);
}

static void too_few_days_of_pregnancy(void)
{
    set_word_at(SAVE_DATA + PREGNANCY_DAYS_OFFSET, DAYS_OF_PREGNANCY_BEFORE_THE_BIRTH - 1u);
}

int main(void)
{
    const JamieScene* pregnancy_as_the_wife = &jamie_scenes[SCENE_PREGNANCY_AS_THE_WIFE];
    const JamieScene* birth_as_the_wife = &jamie_scenes[SCENE_BIRTH_AS_THE_WIFE];

    play_the_pregnancy("pregnancy_as_the_wife", PLAYER_CHARACTER_THE_BOY,
                       "/message_e/event/Eve_conce00_e.bin", "/Script/Eve_conce01.lsb");
    /* Spanish, and the file names in lower case as the disc has them: the mod ignores case. */
    play_the_pregnancy("pregnancy_as_the_husband", PLAYER_CHARACTER_THE_GIRL,
                       "/message_s/event/eve_conce10_s.bin", "/script/eve_conce10.lsb");

    /* Another event starts while Jamie stands in, or the scene's event will not start after all. */
    set_up(JAMIE_CHARACTER_NUMBER, PLAYER_CHARACTER_THE_BOY);
    at_the_start_of_the_pregnancy_check(&cpu);
    start_event(ANOTHER_EVENT);
    report_the_spouse("another_event", "after");
    set_up(JAMIE_CHARACTER_NUMBER, PLAYER_CHARACTER_THE_BOY);
    at_the_start_of_the_pregnancy_check(&cpu);
    events_are_not_allowed();
    start_event(pregnancy_as_the_wife->event);
    report_the_spouse("events_not_allowed_any_more", "after");

    /* Another event that will not start (seen before, or events not allowed): nothing changes. */
    set_up(JAMIE_CHARACTER_NUMBER, PLAYER_CHARACTER_THE_BOY);
    at_the_start_of_the_pregnancy_check(&cpu);
    set_bit(&cpu, SAVE_DATA + STARTED_EVENTS_OFFSET, ANOTHER_EVENT);
    start_event(ANOTHER_EVENT);
    report_the_spouse("another_event_seen_before", "after");
    set_up(JAMIE_CHARACTER_NUMBER, PLAYER_CHARACTER_THE_BOY);
    at_the_start_of_the_pregnancy_check(&cpu);
    events_are_not_allowed();
    start_event(ANOTHER_EVENT);
    report_the_spouse("another_event_not_allowed", "after");

    check_the_pregnancy_after("too_few_days_of_marriage", too_few_days_of_marriage);
    check_the_pregnancy_after("too_few_counted_days", too_few_counted_days);
    check_the_pregnancy_after("too_little_friendship", too_little_friendship);
    check_the_pregnancy_after("the_scene_has_been_seen", the_scene_has_been_seen);
    check_the_pregnancy_after("events_are_not_allowed", events_are_not_allowed);

    /* Another spouse: nothing changes, not even the scene's files. */
    set_up(ANOTHER_SPOUSE, PLAYER_CHARACTER_THE_BOY);
    at_the_start_of_the_pregnancy_check(&cpu);
    report_the_spouse("another_spouse", "in_the_check");
    make_the_event_messages();
    load_file("/message_e/event/Eve_conce00_e.bin", MESSAGES_FILE);
    report_the_event_messages("another_spouse");
    make_the_script(pregnancy_as_the_wife);
    load_file("/Script/Eve_conce01.lsb", SCRIPT_FILE);
    report_the_script("another_spouse", pregnancy_as_the_wife, pregnancy_as_the_wife->partner);

    /* A script that is not the one the mod knows (one of the places holds another number): left as it is. */
    set_up(JAMIE_CHARACTER_NUMBER, PLAYER_CHARACTER_THE_BOY);
    make_the_script(pregnancy_as_the_wife);
    bytes_at(SCRIPT_FILE)[pregnancy_as_the_wife->actor_bytes[3]] = ANOTHER_SPOUSE;
    load_file("/Script/Eve_conce01.lsb", SCRIPT_FILE);
    report_the_script("unknown_script", pregnancy_as_the_wife, JAMIE_CHARACTER_NUMBER);

    /*
     * The birth: Jamie stands in from the field check's entry, while the morning checks ask for events seen
     * long ago (as in the game: 0x100, 0x109 and 0x1A), and gets the birth's conversation after.
     */
    check_the_birth_after("birth_as_the_wife", PLAYER_CHARACTER_THE_BOY, nothing);
    set_bit(&cpu, SAVE_DATA + STARTED_EVENTS_OFFSET, ANOTHER_EVENT);
    start_event(ANOTHER_EVENT);
    start_event(birth_as_the_wife->event);
    report_the_spouse("birth_as_the_wife", "as_the_event_starts");
    the_birth_block_runs_for(birth_as_the_wife->partner);
    settle_the_stand_in(&cpu);
    report_the_spouse("birth_as_the_wife", "after");
    report_the_birth_conversation("birth_as_the_wife", birth_as_the_wife->partner);
    check_the_birth_after("birth_as_the_husband", PLAYER_CHARACTER_THE_GIRL, nothing);

    const uint32_t boy = PLAYER_CHARACTER_THE_BOY;
    check_the_birth_after("birth_on_the_wedding_anniversary", boy, the_wedding_anniversary);
    check_the_birth_after("birth_outside_the_house", boy, outside_the_house);
    check_the_birth_after("birth_without_the_morning_checks", boy, the_morning_checks_are_off);
    check_the_birth_after("birth_too_early", boy, too_few_days_of_pregnancy);

    /* The birth's conversation stays with the partner when the partner is the spouse. */
    set_up(birth_as_the_wife->partner, PLAYER_CHARACTER_THE_BOY);
    the_birth_block_runs_for(birth_as_the_wife->partner);
    settle_the_stand_in(&cpu);
    report_the_birth_conversation("birth_of_the_partner", birth_as_the_wife->partner);
    return 0;
}
"""


@pytest.fixture(scope="module")
def results(tmp_path_factory) -> dict[str, dict[str, str]]:
    """What the program printed: for each case, its facts and their values."""
    if C_COMPILER is None or not MODERNGEKKO_INCLUDE_DIRECTORY.is_dir():
        pytest.skip("needs a C compiler and ModernGekko's headers (set MODERNGEKKO_TEMPLATE)")
    work = tmp_path_factory.mktemp("scenes")
    subprocess.run(
        [sys.executable, str(REPOSITORY_ROOT / "tools" / "export_mod_symbols.py"), "--out"]
        + [str(work / "G4AEE9_symbols.h")],
        check=True,
        capture_output=True,
    )
    driver = SCENES_DRIVER
    for name, value in {
        "FAKE_HIGHEST_MESSAGE": FAKE_HIGHEST_MESSAGE,
        "FAKE_MESSAGE_ROOM": FAKE_MESSAGE_ROOM,
        "FAKE_FILE_TABLE": FAKE_FILE_TABLE,
        "FAKE_FILE_TEXT": FAKE_FILE_TEXT,
        "FAKE_TEXT_FILLER": FAKE_TEXT_FILLER,
        "FAKE_SCRIPT_FILLER": FAKE_SCRIPT_FILLER,
        "FAKE_SCRIPT_BYTES": FAKE_SCRIPT_BYTES,
        "HOUSE_AREA": HOUSE_AREA,
        "NOT_A_HOUSE_AREA": NOT_A_HOUSE_AREA,
        "PARTNERS_OWN_FRIENDSHIP": PARTNERS_OWN_FRIENDSHIP,
        "JAMIES_FRIENDSHIP": JAMIES_FRIENDSHIP,
        "ANOTHER_SPOUSE": ANOTHER_SPOUSE,
        "ANOTHER_EVENT": ANOTHER_EVENT,
        "PLAYER_CHARACTER_THE_GIRL": PLAYER_CHARACTER_THE_GIRL,
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
    facts: dict[str, dict[str, str]] = {}
    for line in output.splitlines():
        case, name, value = line.split(" ", 2)
        facts.setdefault(case, {})[name] = value
    return facts


def fact(results, case: str, name: str) -> int:
    return int(results[case][name])


@pytest.mark.parametrize(
    ("case", "scene_name", "partners_friendship"),
    [
        ("pregnancy_as_the_wife", "SCENE_PREGNANCY_AS_THE_WIFE", "friendship_of_4"),
        ("pregnancy_as_the_husband", "SCENE_PREGNANCY_AS_THE_HUSBAND", "friendship_of_13"),
    ],
)
def test_jamie_stands_in_for_the_pregnancy_and_is_the_spouse_again_after(
    results, scenes, case, scene_name, partners_friendship
):
    partner = scenes[scene_name].partner
    assert fact(results, case, "spouse_at_the_entry") == JAMIE_CHARACTER_NUMBER
    assert fact(results, case, "spouse_in_the_check") == partner
    assert (
        fact(results, case, f"{partners_friendship}_in_the_check") == JAMIES_FRIENDSHIP
    )  # what the check reads
    assert fact(results, case, "spouse_as_the_event_starts") == partner
    assert fact(results, case, "spouse_after") == JAMIE_CHARACTER_NUMBER
    assert fact(results, case, f"{partners_friendship}_after") == PARTNERS_OWN_FRIENDSHIP


@pytest.mark.parametrize("case", ["another_event", "events_not_allowed_any_more"])
def test_jamie_is_the_spouse_again_at_once_when_another_event_starts_or_the_scene_will_not(results, case):
    assert fact(results, case, "spouse_after") == JAMIE_CHARACTER_NUMBER
    assert fact(results, case, "friendship_of_4_after") == PARTNERS_OWN_FRIENDSHIP


@pytest.mark.parametrize("case", ["another_event_seen_before", "another_event_not_allowed"])
def test_jamie_goes_on_standing_in_when_another_event_will_not_start(results, scenes, case):
    assert fact(results, case, "spouse_after") == scenes["SCENE_PREGNANCY_AS_THE_WIFE"].partner
    assert fact(results, case, "friendship_of_4_after") == JAMIES_FRIENDSHIP


@pytest.mark.parametrize(
    "case",
    [
        "too_few_days_of_marriage",
        "too_few_counted_days",
        "too_little_friendship",
        "the_scene_has_been_seen",
        "events_are_not_allowed",
    ],
)
def test_jamie_stands_in_for_nobody_when_a_condition_of_the_pregnancy_fails(results, case):
    assert fact(results, case, "spouse_in_the_check") == JAMIE_CHARACTER_NUMBER
    assert fact(results, case, "friendship_of_4_in_the_check") == PARTNERS_OWN_FRIENDSHIP


def test_nothing_changes_when_jamie_is_not_the_spouse(results, scenes):
    scene = scenes["SCENE_PREGNANCY_AS_THE_WIFE"]
    assert fact(results, "another_spouse", "spouse_in_the_check") == ANOTHER_SPOUSE
    assert results["another_spouse"]["entries"].split() == [
        str(message * FAKE_MESSAGE_ROOM) for message in range(FAKE_HIGHEST_MESSAGE + 1)
    ]
    assert set(bytes.fromhex(results["another_spouse"]["text"])) == {FAKE_TEXT_FILLER}
    assert fact(results, "another_spouse", "places_with_the_value") == len(scene.actor_bytes)
    assert fact(results, "another_spouse", "other_bytes_changed") == 0


PREGNANCY_CASES = [
    ("pregnancy_as_the_wife", "SCENE_PREGNANCY_AS_THE_WIFE", "e"),
    ("pregnancy_as_the_husband", "SCENE_PREGNANCY_AS_THE_HUSBAND", "s"),
]


@pytest.mark.parametrize(("case", "scene_name", "letter"), PREGNANCY_CASES)
def test_the_scenes_script_gets_jamie_at_every_place_and_nowhere_else(
    results, scenes, case, scene_name, letter
):
    assert fact(results, case, "places_with_the_value") == len(scenes[scene_name].actor_bytes)
    assert fact(results, case, "other_bytes_changed") == 0


def test_a_script_the_mod_does_not_know_is_left_as_it_is(results):
    assert fact(results, "unknown_script", "places_with_the_value") == 0
    assert fact(results, "unknown_script", "other_bytes_changed") == 0


@pytest.mark.parametrize(("case", "scene_name", "letter"), PREGNANCY_CASES)
def test_the_scenes_messages_get_jamies_lines_over_the_unused_ones(
    results, scenes, scene_lines, case, scene_name, letter
):
    scene = scenes[scene_name]
    lines = lines_of(scene_lines, letter, scene)
    entries = [int(value) for value in results[case]["entries"].split()]
    text = bytes.fromhex(results[case]["text"])
    room_start = scene.first_unused_message * FAKE_MESSAGE_ROOM
    room_end = scene.first_message_after_the_unused_ones * FAKE_MESSAGE_ROOM
    for line in lines:
        start = entries[line.number]
        encoded = encode_for_the_game(line.text)
        assert room_start <= start and start + len(encoded) <= room_end, f"message {line.number} is elsewhere"
        assert text[start : start + len(encoded)] == encoded, f"message {line.number} is not Jamie's line"
    jamies_numbers = {line.number for line in lines}
    for message, entry in enumerate(entries):
        if message not in jamies_numbers:
            assert entry == message * FAKE_MESSAGE_ROOM, f"the entry of message {message} changed"
    assert set(text[:room_start]) == {FAKE_TEXT_FILLER}, "the text before the room changed"
    assert set(text[room_end:]) == {FAKE_TEXT_FILLER}, "the text after the room changed"


@pytest.mark.parametrize(
    ("case", "scene_name"),
    [
        ("birth_as_the_wife", "SCENE_BIRTH_AS_THE_WIFE"),
        ("birth_as_the_husband", "SCENE_BIRTH_AS_THE_HUSBAND"),
    ],
)
def test_jamie_stands_in_for_the_birth_from_the_field_checks_entry(results, scenes, case, scene_name):
    assert fact(results, case, "spouse_at_the_entry") == scenes[scene_name].partner
    assert (
        fact(results, case, "friendship_of_4_at_the_entry") == PARTNERS_OWN_FRIENDSHIP
    )  # the birth reads none
    assert fact(results, case, "friendship_of_13_at_the_entry") == PARTNERS_OWN_FRIENDSHIP


def test_after_the_birth_jamie_is_the_spouse_and_talks_about_it(results, scenes):
    partner = scenes["SCENE_BIRTH_AS_THE_WIFE"].partner
    assert fact(results, "birth_as_the_wife", "spouse_as_the_event_starts") == partner
    assert fact(results, "birth_as_the_wife", "spouse_after") == JAMIE_CHARACTER_NUMBER
    assert fact(results, "birth_as_the_wife", "jamie_talks_about_the_birth") == 1
    assert fact(results, "birth_as_the_wife", "partner_talks_about_the_birth") == 0


def test_the_partner_keeps_the_birth_conversation_as_the_spouse(results):
    assert fact(results, "birth_of_the_partner", "jamie_talks_about_the_birth") == 0
    assert fact(results, "birth_of_the_partner", "partner_talks_about_the_birth") == 1


@pytest.mark.parametrize(
    "case",
    [
        "birth_on_the_wedding_anniversary",
        "birth_outside_the_house",
        "birth_without_the_morning_checks",
        "birth_too_early",
    ],
)
def test_jamie_stands_in_for_nobody_when_the_field_check_will_not_reach_the_birth(results, case):
    assert fact(results, case, "spouse_at_the_entry") == JAMIE_CHARACTER_NUMBER


# ---------------------------------------------------------------------------------------------------------
# What the mod relies on, checked on the game's files.
# ---------------------------------------------------------------------------------------------------------

needs_the_scripts = pytest.mark.skipif(
    not SCRIPT_DIRECTORY.is_dir(), reason="needs the extracted disc (tools/extract_disc.py)"
)


def messages_the_script_shows(script_name: str) -> set[int]:
    instructions = load_script(SCRIPT_DIRECTORY / f"{script_name}.lsb").instructions
    shown = set()
    for index, instruction in enumerate(instructions):
        if instruction.builtin_index == SHOW_MESSAGE_BUILTIN:
            pushed = instructions[index - 1]
            assert pushed.opcode.name == "PUSH_INT", f"{script_name}: a message whose number is not pushed"
            shown.add(int(pushed.operand))
    return shown


def event_message_file(scene: Scene, letter: str) -> bytes:
    """The scene's message file in a language, found without regard to case, as the game asks for it."""
    folder = DISC_DIRECTORY / "files" / f"message_{letter}" / "event"
    wanted = f"{scene.message_file}_{letter}.bin".lower()
    matches = [path for path in folder.iterdir() if path.name.lower() == wanted]
    assert matches, f"the disc has no {wanted} in {folder}"
    return matches[0].read_bytes()


@needs_the_scripts
def test_the_places_of_the_partner_are_the_ones_the_tool_finds_in_the_scripts(scenes):
    import script_actors

    scripts = script_actors.disc_scripts(SCRIPT_DIRECTORY)
    arities = script_actors.value_builtin_arities(scripts)
    slots = script_actors.actor_slots(scripts, arities)
    for scene in scenes.values():
        script = load_script(SCRIPT_DIRECTORY / f"{scene.script_name}.lsb")
        found = script_actors.character_references(script, scene.partner, arities, slots)
        assert scene.actor_bytes == tuple(reference.file_offset for reference in found), scene.name


@needs_the_scripts
def test_jamies_lines_are_messages_the_scene_shows_and_the_unused_ones_are_not(scenes, scene_lines):
    for scene in scenes.values():
        shown = messages_the_script_shows(scene.script_name)
        jamies = {line.number for line in lines_of(scene_lines, "e", scene)}
        assert jamies <= shown, f"{scene.name} never shows {sorted(jamies - shown)}"
        assert not shown & set(scene.unused_messages), (
            f"{scene.name} shows a message that Jamie's lines cover"
        )


@needs_the_scripts
def test_jamies_lines_fit_over_the_unused_messages_in_every_language(scenes, scene_lines):
    for scene in scenes.values():
        for letter in scene_lines:
            where = f"{scene.name}, language {letter}"
            data = event_message_file(scene, letter)
            _speaker, highest_message, table, _text = MESSAGE_FILE_HEADER.unpack_from(data)
            assert scene.first_message_after_the_unused_ones <= highest_message, where
            offsets = struct.unpack_from(f">{highest_message + 1}I", data, table)
            room = offsets[scene.first_unused_message : scene.first_message_after_the_unused_ones + 1]
            assert list(room) == sorted(room), f"{where}: the unused messages are not in a row"
            needed = sum(len(encode_for_the_game(line.text)) for line in lines_of(scene_lines, letter, scene))
            assert needed <= room[-1] - room[0], f"{where}: {needed} bytes for {room[-1] - room[0]}"


# The field check's code in main.dol, as tools/disasm.py shows it.

FIELD_CHECK = "fn_80166888"
BIRTH_CHECK_ADDRESS = 0x8016809C  # the first instruction of the birth check, among the morning checks
EVENT_SCRIPT_TABLE_ADDRESS = 0x8026BBA0  # for each event, a pointer to the name of its script
EVENT_MESSAGE_TABLE_ADDRESS = 0x8026B740  # for each event, a pointer to the path of its messages, or 0
LOAD_THE_SAVE_DATA_POINTER = 0x806D8EC8  # lwz r3, -0x7138(r13): r3 = *(0x80321F48)
ADD_THE_SPOUSE_OFFSET = 0x386322B8  # addi r3, r3, 0x22B8
COMPARE_R3_WITH = 0x2C030000  # cmpwi r3, <the low 16 bits>
LOAD_R4_WITH = 0x38800000  # li r4, <the low 16 bits>
LOAD_R3_WITH_THE_FARM = 0x38600081  # li r3, 0x81: fn_800A232C(0x81) is the farm's area in this season
FIRST_HOUSE_AREA = 27

# DolRecomp cuts each code section of main.dol into chunks of 4096 instructions from the section's start
# (DOLC_DEFAULT_CHUNK_INSTRUCTIONS); a branch from one chunk to another goes through ModernGekko's dispatcher,
# which runs the hooks of its target.
CHUNK_BYTES = 4096 * 4


@pytest.fixture(scope="module")
def dol() -> Dol:
    if not MAIN_DOL_PATH.is_file():
        pytest.skip("needs the extracted disc (tools/extract_disc.py)")
    return Dol(MAIN_DOL_PATH)


@pytest.fixture(scope="module")
def field_check() -> range:
    """The addresses of the field check's instructions."""
    symbol = SymbolTable.read(SYMBOLS_PATH).by_name(FIELD_CHECK)
    assert symbol is not None, f"{FIELD_CHECK} is not in {SYMBOLS_PATH.name}"
    return range(symbol.address, symbol.address + symbol.size, 4)


def chunk_of(dol: Dol, address: int) -> tuple[int, int]:
    section = dol.section_at(address)
    assert section is not None and section.is_text, f"{address:#x} is not code"
    return section.address, (address - section.address) // CHUNK_BYTES


def low_16_bits_signed(word: int) -> int:
    value = word & 0xFFFF
    return value - 0x10000 if value & 0x8000 else value


def branch_target(dol: Dol, address: int) -> int | None:
    """Where the relative branch at `address` (b or bc, without link) lands, or None for anything else."""
    word = dol.u32(address)
    if word & 0b11:  # with link, or to an absolute address
        return None
    primary_opcode = word >> 26
    if primary_opcode == 18:  # b
        offset = word & 0x03FFFFFC
        return address + (offset - 0x04000000 if offset & 0x02000000 else offset)
    if primary_opcode == 16:  # bc
        return address + low_16_bits_signed(word)
    return None


def test_the_hooked_points_of_the_field_check_are_reached_from_another_chunk(dol, source, field_check):
    addresses = hex_macros(source)
    for name in ("PREGNANCY_CHECK_ADDRESS", "END_OF_THE_FIELD_CHECKS_ADDRESS"):
        target = addresses[name]
        sources = [address for address in field_check if branch_target(dol, address) == target]
        assert sources, f"nothing in {FIELD_CHECK} branches to {name}"
        assert any(chunk_of(dol, branch) != chunk_of(dol, target) for branch in sources), (
            f"{name}: no hook runs"
        )
    assert dol.u32(addresses["PREGNANCY_CHECK_ADDRESS"]) == LOAD_R3_WITH_THE_FARM


def test_the_houses_go_to_the_case_that_tests_the_morning_flag(dol, source):
    addresses = hex_macros(source)
    cases = [
        dol.u32(addresses["FIELD_CHECK_CASES_ADDRESS"] + 4 * area)
        for area in range(addresses["LAST_AREA_WITH_A_CASE"] + 1)
    ]
    house_case = addresses["HOUSE_CASE_ADDRESS"]
    assert cases.index(house_case) == FIRST_HOUSE_AREA
    assert dol.u32(house_case + 4) == LOAD_R4_WITH | addresses["FLAG_MORNING_CHECKS"]
    # bne: with the flag clear, the case goes on to the pregnancy check.
    assert branch_target(dol, house_case + 0x18) == addresses["PREGNANCY_CHECK_ADDRESS"]


def chain_block(dol: Dol, field_check: range, partner: int, event: int) -> int | None:
    """The block of the field check that compares the spouse with `partner` and then starts `event`."""
    for address in field_check:
        if (
            dol.u32(address) == LOAD_THE_SAVE_DATA_POINTER
            and dol.u32(address + 4) == ADD_THE_SPOUSE_OFFSET
            and dol.u32(address + 12) == COMPARE_R3_WITH | partner
            and dol.u32(address + 24) == LOAD_R4_WITH | event
        ):
            return address
    return None


def test_each_scene_is_its_partners_block_in_the_right_chain(dol, source, field_check, scenes):
    pregnancy_check = hex_macros(source)["PREGNANCY_CHECK_ADDRESS"]
    for scene in scenes.values():
        block = chain_block(dol, field_check, scene.partner, scene.event)
        assert block is not None, (
            f"{scene.name}: no block for partner {scene.partner} and event {scene.event:#x}"
        )
        if scene.is_the_birth:
            assert BIRTH_CHECK_ADDRESS < block < pregnancy_check, scene.name
        else:
            assert block > pregnancy_check, scene.name
        script = dol.c_string(dol.u32(EVENT_SCRIPT_TABLE_ADDRESS + 4 * scene.event))
        messages = dol.c_string(dol.u32(EVENT_MESSAGE_TABLE_ADDRESS + 4 * scene.event))
        assert script == scene.script_name, scene.name
        assert messages == f"/message_e/event/{scene.message_file}_e.bin", scene.name


def compared_values(dol: Dol, start: int, count: int) -> list[int]:
    """The values of the first `count` instructions `cmpwi r3, n` from `start`."""
    values = []
    address = start
    while len(values) < count:
        word = dol.u32(address)
        if word >> 16 == COMPARE_R3_WITH >> 16:
            values.append(low_16_bits_signed(word))
        address += 4
    return values


def test_the_mod_checks_the_conditions_with_the_games_numbers(dol, source):
    numbers = read_macros(source)[1]
    pregnancy_check = hex_macros(source)["PREGNANCY_CHECK_ADDRESS"]
    assert compared_values(dol, pregnancy_check, 3) == [
        numbers["DAYS_OF_MARRIAGE_BEFORE_A_PREGNANCY"],
        numbers["DAILY_MARRIAGE_COUNT_BEFORE_A_PREGNANCY"],
        numbers["FRIENDSHIP_BEFORE_A_PREGNANCY"],
    ]
    assert compared_values(dol, BIRTH_CHECK_ADDRESS, 1) == [numbers["DAYS_OF_PREGNANCY_BEFORE_THE_BIRTH"]]
