"""A small C program that runs mods/jamie_marriage's hooks on a fake memory, for the tests of the scenes.

Un pequeño programa de C que ejecuta los hooks de mods/jamie_marriage sobre una memoria falsa, para las
pruebas de las escenas.

The program includes mod.c, so it can call the hooks and read the mod's tables and macros. DRIVER_PRELUDE
gives it the fake memory and what every test of a scene needs: a married player's save data, and the game
loading a scene's script or messages into fake files that the hooks then change. Each test adds a main() that
plays its moments and prints one line "case fact value" for each thing it checks; run_driver builds and runs
the program and gives the facts by case. tests/test_jamie_pregnancy_and_birth.py and
tests/test_jamie_special_days.py use it. It needs a C compiler and ModernGekko's headers
(MODERNGEKKO_TEMPLATE, as for scripts/build_mods.sh); without them the tests that use it are skipped.
"""

import os
import shutil
import subprocess
import sys
from pathlib import Path

import pytest
from jamie_mod_source import MOD_SOURCE, Scene, SceneLine, encode_for_the_game

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

from project import REPOSITORY_ROOT  # noqa: E402

TEMPLATE_DIRECTORY = Path(os.environ.get("MODERNGEKKO_TEMPLATE", Path.home() / "ModernGekko-Template"))
MODERNGEKKO_INCLUDE_DIRECTORY = TEMPLATE_DIRECTORY / "lib" / "ModernGekko" / "include"
C_COMPILER = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")

# The fake files. The loaded event message file holds messages 0 to 63 (as many as the files of the special
# days, more than the other scenes'), message m at text offset m * 0x40, and every byte of its text is 0xEE
# until the mod writes. Every byte of the loaded script is 0x55, except a PUSH_INT of the partner at each
# place of the scenes that the test puts there.
FAKE_HIGHEST_MESSAGE = 63
FAKE_MESSAGE_ROOM = 0x40
FAKE_FILE_TABLE = 0x10
FAKE_FILE_TEXT = FAKE_FILE_TABLE + 4 * (FAKE_HIGHEST_MESSAGE + 1)
FAKE_TEXT_FILLER = 0xEE
FAKE_SCRIPT_FILLER = 0x55
FAKE_SCRIPT_BYTES = 0x1000
PLAYER_CHARACTER_THE_GIRL = 1
ANOTHER_SPOUSE = 5

FAKE_MEMORY_NUMBERS = {
    "FAKE_HIGHEST_MESSAGE": FAKE_HIGHEST_MESSAGE,
    "FAKE_MESSAGE_ROOM": FAKE_MESSAGE_ROOM,
    "FAKE_FILE_TABLE": FAKE_FILE_TABLE,
    "FAKE_FILE_TEXT": FAKE_FILE_TEXT,
    "FAKE_TEXT_FILLER": FAKE_TEXT_FILLER,
    "FAKE_SCRIPT_FILLER": FAKE_SCRIPT_FILLER,
    "FAKE_SCRIPT_BYTES": FAKE_SCRIPT_BYTES,
    "PLAYER_CHARACTER_THE_GIRL": PLAYER_CHARACTER_THE_GIRL,
    "ANOTHER_SPOUSE": ANOTHER_SPOUSE,
}

DRIVER_PRELUDE = r"""
/* The fake memory that the hooks of mod.c run on, and the moments that every test of a scene plays. */
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
#define FAKE_TEXT_BYTES ((FAKE_HIGHEST_MESSAGE + 1) * FAKE_MESSAGE_ROOM)

static uint8_t memory[MEMORY_BYTES];
static CPUState cpu;
static uint8_t script_before_the_load[FAKE_SCRIPT_BYTES];

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

/* An empty memory with the save data of a player married to `spouse`, and the mod as the game starts it. */
static void set_up_the_memory(uint32_t spouse, uint32_t player_character)
{
    memset(memory, 0, sizeof memory);
    memset(&cpu, 0, sizeof cpu);
    cpu.external_read = fake_read;
    cpu.external_write = fake_write;
    stand_in.scene = NULL;
    set_word_at(SAVE_DATA_POINTER_ADDRESS, SAVE_DATA);
    set_word_at(SAVE_DATA + SPOUSE_OFFSET, spouse);
    set_word_at(SAVE_DATA + PLAYER_CHARACTER_OFFSET, player_character);
}

static void report(const char* name, const char* fact, uint32_t value)
{
    printf("%s %s %u\n", name, fact, (unsigned int)value);
}

/* fn_80029E74(path) loads a file: the path on entry, the loaded file on return. A hook may run twice. */
static void load_file(const char* path, uint32_t file)
{
    memcpy(bytes_at(PATH), path, strlen(path) + 1);
    cpu.gpr[3] = PATH;
    before_a_file_is_loaded(&cpu);
    before_a_file_is_loaded(&cpu);
    cpu.gpr[3] = file;
    after_a_file_is_loaded(&cpu);
}

/* The event's message file as the game has just loaded it (see FAKE_HIGHEST_MESSAGE in the Python side). */
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

static void clear_the_script(void)
{
    memset(bytes_at(SCRIPT_FILE), FAKE_SCRIPT_FILLER, FAKE_SCRIPT_BYTES);
}

/* A PUSH_INT of the scene's partner at each of the scene's places. */
static void put_the_partner_in_the_script(const JamieScene* scene)
{
    for (uint32_t index = 0; index < scene->partner_byte_count; ++index) {
        bytes_at(SCRIPT_FILE)[scene->partner_bytes[index] - 1u] = PUSH_INT_OPCODE;
        bytes_at(SCRIPT_FILE)[scene->partner_bytes[index]] = (uint8_t)scene->partner;
    }
}

/* fn_80029E74 loads the script at `path`, with what the driver has put in the fake one. */
static void load_the_script(const char* path)
{
    memcpy(script_before_the_load, bytes_at(SCRIPT_FILE), FAKE_SCRIPT_BYTES);
    load_file(path, SCRIPT_FILE);
}

static bool is_one_of_the_places(const JamieScene* scene, uint32_t offset)
{
    for (uint32_t index = 0; index < scene->partner_byte_count; ++index) {
        if (offset == scene->partner_bytes[index]) {
            return true;
        }
    }
    return false;
}

/* How many of the scene's places hold `value` after the load, and how many other bytes the load changed. */
static void report_the_script(const char* name, const JamieScene* scene, uint32_t value)
{
    uint32_t places_with_the_value = 0;
    uint32_t other_bytes_changed = 0;
    for (uint32_t offset = 0; offset < FAKE_SCRIPT_BYTES; ++offset) {
        const uint8_t byte = bytes_at(SCRIPT_FILE)[offset];
        if (is_one_of_the_places(scene, offset)) {
            places_with_the_value += byte == value;
        } else {
            other_bytes_changed += byte != script_before_the_load[offset];
        }
    }
    report(name, "places_with_the_value", places_with_the_value);
    report(name, "other_bytes_changed", other_bytes_changed);
}
"""


def run_driver(work: Path, main_code: str, numbers: dict[str, int]) -> dict[str, dict[str, str]]:
    """Builds DRIVER_PRELUDE and `main_code` with mod.c in `work`, runs it and gives its facts by case.

    `numbers` become macros of the program, next to those of the fake memory.
    """
    if C_COMPILER is None or not MODERNGEKKO_INCLUDE_DIRECTORY.is_dir():
        pytest.skip("needs a C compiler and ModernGekko's headers (set MODERNGEKKO_TEMPLATE)")
    subprocess.run(
        [sys.executable, str(REPOSITORY_ROOT / "tools" / "export_mod_symbols.py"), "--out"]
        + [str(work / "G4AEE9_symbols.h")],
        check=True,
        capture_output=True,
    )
    macros = "".join(
        f"#define {name} 0x{value:X}u\n" for name, value in {**FAKE_MEMORY_NUMBERS, **numbers}.items()
    )
    (work / "driver.c").write_text(macros + DRIVER_PRELUDE + main_code, encoding="utf-8")
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


def fact(results: dict[str, dict[str, str]], case: str, name: str) -> int:
    return int(results[case][name])


def check_jamies_lines_are_over_the_unused_messages(
    results: dict[str, dict[str, str]], case: str, scene: Scene, lines: list[SceneLine]
) -> None:
    """In the case's message file, each line is under its number over the scene's unused messages, and
    nothing else changed."""
    assert lines, f"{case}: there are no lines to look for"
    entries = [int(value) for value in results[case]["entries"].split()]
    text = bytes.fromhex(results[case]["text"])
    room_start = scene.first_unused_message * FAKE_MESSAGE_ROOM
    room_end = scene.first_message_after_the_unused_ones * FAKE_MESSAGE_ROOM
    for line in lines:
        start = entries[line.number]
        encoded = encode_for_the_game(line.text)
        where = f"{case}, message {line.number}"
        assert room_start <= start and start + len(encoded) <= room_end, f"{where} is elsewhere"
        assert text[start : start + len(encoded)] == encoded, f"{where} is not Jamie's line"
    jamies_numbers = {line.number for line in lines}
    for message, entry in enumerate(entries):
        if message not in jamies_numbers:
            assert entry == message * FAKE_MESSAGE_ROOM, f"{case}: the entry of message {message} changed"
    assert set(text[:room_start]) == {FAKE_TEXT_FILLER}, f"{case}: the text before the room changed"
    assert set(text[room_end:]) == {FAKE_TEXT_FILLER}, f"{case}: the text after the room changed"


def check_the_messages_are_as_loaded(results: dict[str, dict[str, str]], case: str) -> None:
    """The case's message file is as make_the_event_messages made it."""
    assert results[case]["entries"].split() == [
        str(message * FAKE_MESSAGE_ROOM) for message in range(FAKE_HIGHEST_MESSAGE + 1)
    ]
    assert set(bytes.fromhex(results[case]["text"])) == {FAKE_TEXT_FILLER}
