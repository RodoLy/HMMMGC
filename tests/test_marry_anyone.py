"""mods/marry_anyone on a fake memory, and on the disc's messages of the spouses.

mods/marry_anyone sobre una memoria falsa, y sobre los mensajes de los cónyuges del disco.

A small C program includes mod.c, gives its hooks a fake memory with a player's save data and plays the
moments the game calls them at; each line it prints is "case fact value". The first tests check what each hook
does: which partner built-in 189 asks about, the closest partner among all twenty, the chain of love scenes
and of the wedding the field check goes to, the events that do not start, and how the player is dressed in a
wedding script of the other character. The last ones run the swap of words of the spouse's first messages (9
and 100) on the disc's own message files, for every partner and language: the words for the other sex are
gone, every pair of words of mod.c is found in some message, and the rows still fit the window. They are
skipped without the game. The program needs a C compiler; it is built with the native runtime's stand-in for
ModernGekko's headers (runtime/mod_api).
"""

import shutil
import struct
import subprocess
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))

import make_adoption_scene  # noqa: E402
from lsb import parse_script  # noqa: E402
from project import DISC_DIRECTORY, REPOSITORY_ROOT, SCRIPT_DIRECTORY  # noqa: E402

MOD_SOURCE = REPOSITORY_ROOT / "mods" / "marry_anyone" / "mod.c"
C_COMPILER = shutil.which("cc") or shutil.which("gcc") or shutil.which("clang")

THE_BOY = 0
THE_GIRL = 1
NOBODY = 126
PARTNER_FILES = {
    3: "nina", 4: "anne", 5: "ellen", 6: "mary", 7: "eve", 8: "jina", 9: "dia", 10: "ketty", 11: "sara",
    12: "raira", 13: "blue", 14: "shin", 15: "hayato", 16: "azuma", 17: "curl", 18: "Dann", 19: "Laif",
    20: "Basil", 21: "Kazan", 22: "Runn",
}  # fmt: skip
LANGUAGES = "esfgi"
FIRST_WORDS = (9, 100)

END_OF_MESSAGE = b"\x01\x08\x1f\x1d"
NEW_ROW = b"\x01\x04\x1d"
NEW_PAGE = b"\x01\x05\x1d"
LONGEST_ROW = 28  # letters, as tests/jamie_mod_source.py measured the window

Facts = dict[str, dict[str, int | str]]

DRIVER = r"""
#include MOD_SOURCE_PATH

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MEMORY_START 0x80000000u
#define MEMORY_BYTES 0x01000000u
#define SAVE_DATA 0x80900000u
#define PATH 0x80800000u
#define SLOT 0x80810000u
#define LOADED_FILE 0x80A00000u

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

static void set_friendship(uint32_t character, uint32_t value)
{
    set_word_at(SAVE_DATA + FRIENDSHIP_OFFSET + character * FRIENDSHIP_BYTES_EACH, value);
}

static void set_bit(uint32_t array, uint32_t bit) { bytes_at(array)[bit / 8] |= (uint8_t)(1u << (bit % 8)); }

/* An empty memory with the save data of a player of `player` with that spouse and fiance. */
static void set_up(uint32_t player, uint32_t spouse, uint32_t fiance)
{
    memset(memory, 0, sizeof memory);
    memset(&cpu, 0, sizeof cpu);
    cpu.external_read = fake_read;
    cpu.external_write = fake_write;
    set_word_at(SAVE_DATA_POINTER_ADDRESS, SAVE_DATA);
    set_word_at(SAVE_DATA + PLAYER_CHARACTER_OFFSET, player);
    set_word_at(SAVE_DATA + SPOUSE_OFFSET, spouse);
    set_word_at(SAVE_DATA + FIANCE_OFFSET, fiance);
    if (spouse != NOBODY) {
        set_bit(SAVE_DATA + MAIN_FLAGS_OFFSET, FLAG_MARRIED);
    }
}

static void report(const char* name, const char* fact, uint32_t value)
{
    printf("%s %s %u\n", name, fact, (unsigned int)value);
}

/* Built-in 189 about `character`: the character it asks fn_800A2B60 about. */
static uint32_t asked_about(uint32_t character)
{
    set_word_at(SLOT + POPPED_SLOT_VALUE_OFFSET, character);
    cpu.gpr[POPPED_SLOT_REGISTER] = SLOT;
    ask_about_a_partner_of_the_player(&cpu);
    return word_at(SLOT + POPPED_SLOT_VALUE_OFFSET);
}

static uint32_t closest_partner(uint32_t least, uint32_t game_answer)
{
    cpu.gpr[3] = least;
    before_closest_partner(&cpu);
    cpu.gpr[3] = game_answer;
    after_closest_partner(&cpu);
    return cpu.gpr[3];
}

/* A branch of the field check with r3 = the player's character: the chain it goes to, 0 the boy's. */
static uint32_t chain(void (*hook)(CPUState*), uint32_t player)
{
    cpu.gpr[3] = player;
    cpu.cr = player == 0 ? CR0_EQUAL : CR0_GREATER_THAN;
    hook(&cpu);
    const uint32_t by_cr = (cpu.cr & CR0_EQUAL) != 0 ? 0u : 1u;
    return cpu.gpr[3] == by_cr ? by_cr : 99u;
}

/* Whether StartEvent goes on to start `event`, which has not started before. */
static uint32_t starts(uint32_t event)
{
    cpu.gpr[3] = 0;
    cpu.cr = CR0_EQUAL;
    cpu.gpr[EVENT_REGISTER] = event;
    keep_back_an_event(&cpu);
    return cpu.gpr[3] == 0 && (cpu.cr & CR0_EQUAL) != 0;
}

static void load_file(const char* path)
{
    memcpy(bytes_at(PATH), path, strlen(path) + 1);
    cpu.gpr[3] = PATH;
    before_a_file_is_loaded(&cpu);
    cpu.gpr[3] = LOADED_FILE;
    after_a_file_is_loaded(&cpu);
}

/* A wedding script: main at 0x40 and, 0x22 bytes into it, a PUSH_INT of `actor`. */
static void make_a_wedding_script(uint32_t actor)
{
    memset(bytes_at(LOADED_FILE), 0x55, 0x400);
    set_word_at(LOADED_FILE + LSB_MAIN_ENTRY_OFFSET, 0x40 - LSB_CODE_OFFSET);
    uint8_t* dressing = bytes_at(LOADED_FILE + 0x40 + 0x22);
    dressing[0] = LSB_PUSH_INT;
    dressing[1] = LSB_LONG_INT;
    dressing[2] = (uint8_t)actor;
}

static uint32_t actor_after_the_load(void) { return bytes_at(LOADED_FILE + 0x40 + 0x22)[2]; }

static void make_a_birth_message_file(void);
static void report_the_birth_message_file(const char* name);
static void report_the_loaded_bytes(const char* name, size_t size);

static void play_the_hooks(void)
{
    /* Built-in 189 asks about a partner of the player's character in the same place. */
    set_up(THE_BOY_PLAYER, NOBODY, NOBODY);
    report("boy_asks", "about_basil", asked_about(20));
    report("boy_asks", "about_mary", asked_about(6));
    report("boy_asks", "about_jamie", asked_about(JAMIE));
    set_up(THE_GIRL_PLAYER, NOBODY, NOBODY);
    report("girl_asks", "about_mary", asked_about(6));
    report("girl_asks", "about_basil", asked_about(20));

    /* The closest partner among all twenty, and the spouse once married. */
    set_up(THE_BOY_PLAYER, NOBODY, NOBODY);
    set_friendship(5, 300);
    set_friendship(20, 420);
    set_friendship(21, 420);
    report("closest", "of_all", closest_partner(100, 5));
    report("closest", "none_above", closest_partner(500, 5));
    set_up(THE_BOY_PLAYER, 20, NOBODY);
    set_friendship(5, 900);
    report("closest", "married", closest_partner(100, 20));

    /* The village's advice: anyone at 800. */
    set_up(THE_GIRL_PLAYER, NOBODY, NOBODY);
    set_friendship(7, 800);
    cpu.gpr[3] = 0;
    after_someone_loves_the_player(&cpu);
    report("advice", "a_woman_at_800", cpu.gpr[3]);
    set_friendship(7, 799);
    after_someone_loves_the_player(&cpu);
    report("advice", "nobody_at_800", cpu.gpr[3]);

    /* The second love scenes: the player's own chain first, else the other with a scene waiting. */
    set_up(THE_BOY_PLAYER, NOBODY, NOBODY);
    report("love", "none_waits", chain(choose_the_chain_of_love_scenes, THE_BOY_PLAYER));
    set_friendship(20, 500);
    report("love", "a_man_waits", chain(choose_the_chain_of_love_scenes, THE_BOY_PLAYER));
    set_friendship(4, 500);
    report("love", "both_wait", chain(choose_the_chain_of_love_scenes, THE_BOY_PLAYER));
    set_bit(SAVE_DATA + STARTED_EVENTS_OFFSET, FIRST_SECOND_LOVE_SCENE + 20 - 3);
    set_bit(SAVE_DATA + STARTED_EVENTS_OFFSET, FIRST_SECOND_LOVE_SCENE + 4 - 3);
    report("love", "both_seen", chain(choose_the_chain_of_love_scenes, THE_BOY_PLAYER));

    /* The wedding: the fiance's chain. */
    set_up(THE_BOY_PLAYER, NOBODY, 20);
    report("wedding", "boy_and_basil", chain(choose_the_chain_of_the_fiance, THE_BOY_PLAYER));
    set_up(THE_GIRL_PLAYER, NOBODY, 6);
    report("wedding", "girl_and_mary", chain(choose_the_chain_of_the_fiance, THE_GIRL_PLAYER));
    set_up(THE_GIRL_PLAYER, NOBODY, 20);
    report("wedding", "girl_and_basil", chain(choose_the_chain_of_the_fiance, THE_GIRL_PLAYER));

    /* The events: the rival scene of the spouse or the fiance, and a same-sex couple's pregnancy. */
    set_up(THE_BOY_PLAYER, 20, NOBODY);
    report("events", "basils_rival_scene", starts(FIRST_RIVAL_SCENE + 7));
    report("events", "anothers_rival_scene", starts(FIRST_RIVAL_SCENE));
    report("events", "pregnancy_of_two_men", starts(FIRST_PREGNANCY_SCENE + 17));
    report("events", "birth_of_two_men", starts(LAST_BIRTH_SCENE));
    report("events", "village_scene", starts(0x10));
    set_up(THE_BOY_PLAYER, NOBODY, 3);
    report("events", "fiances_rival_scene", starts(FIRST_RIVAL_SCENE + 7));
    set_up(THE_BOY_PLAYER, 5, NOBODY);
    report("events", "pregnancy_of_a_man_and_a_woman", starts(FIRST_PREGNANCY_SCENE + 2));

    /* The wedding scripts dress the player as the player is. */
    set_up(THE_BOY_PLAYER, NOBODY, 20);
    make_a_wedding_script(ACTOR_GIRL_IN_HER_DRESS);
    load_file("/Script/Eve_marry19.lsb");
    report("dress", "boy_in_a_girls_wedding", actor_after_the_load());
    make_a_wedding_script(ACTOR_BOY_IN_HIS_SUIT);
    load_file("/Script/Eve_marry05.lsb");
    report("dress", "boy_in_his_own_wedding", actor_after_the_load());
    set_up(THE_GIRL_PLAYER, NOBODY, 6);
    make_a_wedding_script(ACTOR_BOY_IN_HIS_SUIT);
    load_file("/Script/Eve_marry05.lsb");
    report("dress", "girl_in_a_boys_wedding", actor_after_the_load());
    make_a_wedding_script(ACTOR_BOY_IN_HIS_SUIT);
    load_file("/Script/Eve_marry50.lsb");
    report("dress", "not_a_wedding", actor_after_the_load());

    /* The adoption: two men's pregnancy is over at once, and their birth becomes the Goddess's visit. */
    set_up(THE_BOY_PLAYER, 20, NOBODY);
    report("adoption", "pregnancy_starts", starts(FIRST_PREGNANCY_SCENE + 17));
    report("adoption", "days_after_the_pregnancy", word_at(SAVE_DATA + PREGNANCY_DAYS_OFFSET));
    set_word_at(SAVE_DATA + PREGNANCY_DAYS_OFFSET, 30);
    starts(FIRST_PREGNANCY_SCENE + 17);
    report("adoption", "days_already_counted", word_at(SAVE_DATA + PREGNANCY_DAYS_OFFSET));
    bytes_at(SAVE_DATA + CHARACTER_BITS_OFFSET + CHARACTER_BITS_BYTES_EACH * 20)[0] = 0xFF;
    report("adoption", "birth_starts", starts(FIRST_BIRTH_SCENE + 17));
    make_a_birth_message_file();
    load_file("/message_s/event/Eve_child10_s.bin");
    report_the_birth_message_file("adoption_messages");
    memset(bytes_at(LOADED_FILE), 0x55, 0x1000);
    load_file("/Script/Eve_child17.lsb");
    report_the_loaded_bytes("adoption_script_sprite", adoption_scripts[ADOPTION_BY_THE_SPRITE].size);
    const uint32_t basils_bits = SAVE_DATA + CHARACTER_BITS_OFFSET + CHARACTER_BITS_BYTES_EACH * 20;
    report("adoption", "spouse_bits", bytes_at(basils_bits)[0]);
    memset(bytes_at(LOADED_FILE), 0x55, 0x1000);
    load_file("/Script/Eve_child17.lsb");
    report("adoption", "second_load_untouched", bytes_at(LOADED_FILE)[0] == 0x55);

    /* Once the Goddess is awake, she comes herself. */
    set_up(THE_GIRL_PLAYER, 6, NOBODY);
    set_bit(SAVE_DATA + STARTED_EVENTS_OFFSET, EVENT_THE_GODDESS_WAKES_UP);
    starts(FIRST_BIRTH_SCENE + 3);
    make_a_birth_message_file();
    load_file("/message_e/event/eve_child00_e.bin");
    report_the_birth_message_file("adoption_messages_goddess");
    memset(bytes_at(LOADED_FILE), 0x55, 0x1000);
    load_file("/Script/Eve_child03.lsb");
    report_the_loaded_bytes("adoption_script_goddess", adoption_scripts[ADOPTION_BY_THE_GODDESS].size);

    set_up(THE_BOY_PLAYER, 5, NOBODY);
    report("adoption", "birth_of_a_man_and_a_woman_starts", starts(FIRST_BIRTH_SCENE + 2));
    memset(bytes_at(LOADED_FILE), 0x55, 0x1000);
    load_file("/Script/Eve_child02.lsb");
    report("adoption", "a_man_and_a_woman_keep_the_birth", bytes_at(LOADED_FILE)[0] == 0x55);
}

/* A birth's message file as the game loads it: 48 messages, all of 0x20 bytes of 0xEE. */
static void make_a_birth_message_file(void)
{
    memset(bytes_at(LOADED_FILE), 0, 0x1000);
    fake_write(NULL, LOADED_FILE + MESSAGE_FILE_HEADER_HIGHEST_MESSAGE, 47, HALF_WORD_BYTES);
    set_word_at(LOADED_FILE + MESSAGE_FILE_HEADER_TABLE_START, 0x10);
    set_word_at(LOADED_FILE + MESSAGE_FILE_HEADER_TEXT_START, 0xD0);
    for (uint32_t message = 0; message < 48; ++message) {
        set_word_at(LOADED_FILE + 0x10 + message * WORD_BYTES, message * 0x20);
    }
    memset(bytes_at(LOADED_FILE + 0xD0), 0xEE, 48 * 0x20);
}

static void report_the_birth_message_file(const char* name)
{
    printf("%s entries", name);
    for (uint32_t message = 0; message < 48; ++message) {
        printf(" %u", (unsigned int)word_at(LOADED_FILE + 0x10 + message * WORD_BYTES));
    }
    printf("\n%s text ", name);
    for (uint32_t index = 0; index < 48 * 0x20; ++index) {
        printf("%02x", bytes_at(LOADED_FILE + 0xD0)[index]);
    }
    printf("\n");
}

static void report_the_loaded_bytes(const char* name, size_t size)
{
    printf("%s bytes ", name);
    for (size_t index = 0; index < size + 4; ++index) {
        printf("%02x", bytes_at(LOADED_FILE)[index]);
    }
    printf("\n");
}

/*
 * Reads lines "player partner message language path" from the standard input: loads the file at path into the
 * fake memory as the game loads the spouse's file to show that message, and prints the message as it is then,
 * in hexadecimal ("swap <line number> <hex>").
 */
static void swaps(void)
{
    char line[512];
    unsigned int number = 0;
    while (fgets(line, sizeof line, stdin) != NULL) {
        unsigned int player, partner, message;
        char language;
        char path[400];
        if (sscanf(line, "%u %u %u %c %399s", &player, &partner, &message, &language, path) != 5) {
            continue;
        }
        set_up(player, partner, NOBODY);
        FILE* file = fopen(path, "rb");
        if (file == NULL) {
            exit(2);
        }
        const size_t size = fread(bytes_at(LOADED_FILE), 1, 0x100000, file);
        fclose(file);
        (void)size;
        cpu.gpr[3] = 0;
        cpu.gpr[4] = partner;
        cpu.gpr[5] = message;
        before_a_character_message_is_shown(&cpu);
        char game_path[64];
        snprintf(game_path, sizeof game_path, "/message_%c/chara/x_%c.bin", language, language);
        load_file(game_path);
        after_a_character_message_is_shown(&cpu);
        const uint32_t table = LOADED_FILE + word_at(LOADED_FILE + MESSAGE_FILE_HEADER_TABLE_START);
        const uint32_t text = LOADED_FILE + word_at(LOADED_FILE + MESSAGE_FILE_HEADER_TEXT_START);
        const uint8_t* at = bytes_at(text + word_at(table + message * WORD_BYTES));
        printf("swap %u ", number++);
        for (size_t index = 0; index < LONGEST_MESSAGE_BYTES; ++index) {
            printf("%02x", at[index]);
            if (index >= 3 && memcmp(at + index - 3, end_of_a_message, sizeof end_of_a_message) == 0) {
                break;
            }
        }
        printf("\n");
    }
}

int main(int count, char** arguments)
{
    if (count > 1 && strcmp(arguments[1], "swaps") == 0) {
        swaps();
    } else {
        play_the_hooks();
    }
    return 0;
}
"""


@pytest.fixture(scope="module")
def driver(tmp_path_factory: pytest.TempPathFactory) -> Path:
    """The program, built once."""
    if C_COMPILER is None:
        pytest.skip("needs a C compiler")
    work = tmp_path_factory.mktemp("marry_anyone")
    subprocess.run(
        [sys.executable, str(REPOSITORY_ROOT / "tools" / "export_mod_symbols.py"), "--out"]
        + [str(work / "G4AEE9_symbols.h")],
        check=True,
        capture_output=True,
    )
    macros = f"#define THE_BOY_PLAYER {THE_BOY}u\n#define THE_GIRL_PLAYER {THE_GIRL}u\n"
    (work / "driver.c").write_text(macros + DRIVER, encoding="utf-8")
    program = work / "driver"
    subprocess.run(
        [C_COMPILER, "-std=c11", "-Wall", "-Wextra", "-Werror", f'-DMOD_SOURCE_PATH="{MOD_SOURCE}"']
        + [f"-I{REPOSITORY_ROOT / 'runtime' / 'mod_api'}", f"-I{REPOSITORY_ROOT / 'mods' / 'include'}"]
        + [f"-I{work}", str(work / "driver.c"), "-o", str(program)],
        check=True,
    )
    return program


@pytest.fixture(scope="module")
def facts(driver: Path) -> Facts:
    """Each fact by case: a number, or the text of a longer fact (a table's entries, bytes in hexadecimal)."""
    output = subprocess.run([str(driver)], check=True, capture_output=True, text=True).stdout
    results: Facts = {}
    for line in output.splitlines():
        case, name, value = line.split(" ", 2)
        results.setdefault(case, {})[name] = int(value) if value.isdigit() else value
    return results


def test_built_in_189_asks_about_a_partner_of_the_players_character(facts: Facts) -> None:
    # Basil (20) is the eighth of the girl's partners, so the boy's eighth is asked about: Ketty (10).
    # Mary (6) is the boy's fourth, so the girl's fourth: Azuma (16).
    assert facts["boy_asks"] == {"about_basil": 10, "about_mary": 6, "about_jamie": 2}
    assert facts["girl_asks"] == {"about_mary": 16, "about_basil": 20}


def test_the_closest_partner_is_among_all_twenty(facts: Facts) -> None:
    # Basil and Kazan tie at 420: the first one wins, as in the game's own loop.
    assert facts["closest"] == {"of_all": 20, "none_above": NOBODY, "married": 20}
    assert facts["advice"] == {"a_woman_at_800": 1, "nobody_at_800": 0}


def test_the_field_check_goes_to_the_chain_with_a_scene_waiting(facts: Facts) -> None:
    assert facts["love"] == {
        "none_waits": THE_BOY,
        "a_man_waits": THE_GIRL,
        "both_wait": THE_BOY,
        "both_seen": THE_BOY,
    }
    assert facts["wedding"] == {
        "boy_and_basil": THE_GIRL,
        "girl_and_mary": THE_BOY,
        "girl_and_basil": THE_GIRL,
    }


def test_the_rival_scenes_and_a_same_sex_pregnancy_do_not_start(facts: Facts) -> None:
    assert facts["events"] == {
        "basils_rival_scene": 0,
        "anothers_rival_scene": 1,
        "pregnancy_of_two_men": 0,
        "birth_of_two_men": 1,
        "village_scene": 1,
        "fiances_rival_scene": 0,
        "pregnancy_of_a_man_and_a_woman": 1,
    }


def test_the_wedding_dresses_the_player_as_the_player_is(facts: Facts) -> None:
    suit, dress = 72, 73
    assert facts["dress"] == {
        "boy_in_a_girls_wedding": suit,
        "boy_in_his_own_wedding": suit,
        "girl_in_a_boys_wedding": dress,
        "not_a_wedding": suit,
    }


# ---------------------------------------------------------------------------------------------------------
# The spouse's first words, on the disc.


def word_pairs() -> list[tuple[str, bytes, bytes]]:
    """The pairs of spouse_words in mod.c, as bytes: (language, for a man, for a woman)."""
    text = MOD_SOURCE.read_text(encoding="utf-8")
    table = text[text.index("spouse_words[] = {") : text.index("};", text.index("spouse_words[] = {"))]
    pairs = []
    for line in table.splitlines()[1:]:
        line = line.strip().rstrip(",")
        if not line.startswith("{'"):
            continue
        language = line[2]
        literals = line[line.index(",") + 1 : -1]
        # Each word is one or more C strings next to each other; the two words are split by the comma.
        words = [_c_strings(part) for part in _split_words(literals)]
        pairs.append((language, words[0], words[1]))
    return pairs


def _split_words(literals: str) -> list[str]:
    inside, parts, current = False, [], ""
    for character in literals:
        if character == '"':
            inside = not inside
        if character == "," and not inside:
            parts.append(current)
            current = ""
        else:
            current += character
    parts.append(current)
    return parts


def _c_strings(part: str) -> bytes:
    """The bytes of adjacent C string literals with \\xHH escapes."""
    result = b""
    for chunk in part.split('"')[1::2]:
        index = 0
        while index < len(chunk):
            if chunk.startswith("\\x", index):
                end = index + 2
                while end < len(chunk) and chunk[end] in "0123456789abcdefABCDEF":
                    end += 1
                result += bytes([int(chunk[index + 2 : end], 16)])
                index = end
            else:
                result += chunk[index].encode("latin-1")
                index += 1
    return result


def message_of(data: bytes, number: int) -> bytes | None:
    highest = struct.unpack_from(">H", data, 2)[0]
    if number > highest:
        return None
    table, text = struct.unpack_from(">II", data, 8)
    start = text + struct.unpack_from(">I", data, table + 4 * number)[0]
    return data[start : data.index(END_OF_MESSAGE, start) + len(END_OF_MESSAGE)]


def rows_of(message: bytes) -> list[bytes]:
    body = message.removesuffix(END_OF_MESSAGE).replace(NEW_PAGE, NEW_ROW)
    return body.split(NEW_ROW)


def partner_file(language: str, name: str) -> Path:
    folder = DISC_DIRECTORY / "files" / f"message_{language}" / "chara"
    return next(path for path in folder.iterdir() if path.name.lower() == f"{name.lower()}_{language}.bin")


def same_sex_player(partner: int) -> int:
    return THE_BOY if partner >= 13 else THE_GIRL


@pytest.fixture(scope="module")
def first_words(driver: Path) -> list[tuple[str, int, int, bytes, bytes]]:
    """(language, partner, message, as on the disc, as the mod leaves it) for every first message."""
    if not (DISC_DIRECTORY / "files" / "message_e").is_dir():
        pytest.skip("needs the game's files (orig/G4AEE9)")
    cases, lines = [], []
    for language in LANGUAGES:
        for partner, name in PARTNER_FILES.items():
            path = partner_file(language, name)
            data = path.read_bytes()
            for number in FIRST_WORDS:
                original = message_of(data, number)
                if original is None:
                    continue
                cases.append((language, partner, number, original))
                lines.append(f"{same_sex_player(partner)} {partner} {number} {language} {path}")
    output = subprocess.run(
        [str(driver), "swaps"], input="\n".join(lines) + "\n", check=True, capture_output=True, text=True
    ).stdout
    changed = [bytes.fromhex(line.split(" ")[2]) for line in output.splitlines()]
    assert len(changed) == len(cases)
    return [(*case, after) for case, after in zip(cases, changed, strict=True)]


def test_the_words_for_the_other_sex_are_gone(first_words: list[tuple[str, int, int, bytes, bytes]]) -> None:
    pairs = word_pairs()
    for language, partner, number, _, after in first_words:
        speaker_is_a_woman = partner < 13
        for pair_language, for_a_man, for_a_woman in pairs:
            if pair_language != language:
                continue
            wrong = for_a_man if speaker_is_a_woman else for_a_woman
            assert wrong not in after, f"{language} {PARTNER_FILES[partner]} {number} still has {wrong!r}"


def test_every_pair_of_words_is_in_some_message(
    first_words: list[tuple[str, int, int, bytes, bytes]],
) -> None:
    for language, for_a_man, for_a_woman in word_pairs():
        assert any(
            (for_a_man if partner < 13 else for_a_woman) in original
            for message_language, partner, _, original, _ in first_words
            if message_language == language
        ), f"no message says {for_a_man!r} or {for_a_woman!r}"


def test_some_lines_as_the_mod_leaves_them(first_words: list[tuple[str, int, int, bytes, bytes]]) -> None:
    after = {(language, partner, number): changed for language, partner, number, _, changed in first_words}
    assert b"As my husband, what" in after[("e", 20, 9)]
    assert b"as my wife," in after[("e", 4, 9)]
    assert b"best wife" in after[("e", 6, 9)]  # Mary speaks of herself
    assert b"Mr. \x85 oh" in after[("e", 16, 9)]
    assert b"Eres mi esposo" in after[("s", 21, 9)]
    assert b"Sr... oh" in after[("s", 16, 9)]
    assert b"vraiment mari\xe9es" in after[("f", 7, 9)]
    assert b"mon mari" not in after[("f", 11, 9)] and b"ma femme" in after[("f", 11, 9)]
    assert b"als meinen\x01\x04\x1dMann nennen" in after[("g", 21, 9)]
    assert b"Herr \x85 oh" in after[("g", 16, 9)]
    assert b"siamo sposate" in after[("i", 11, 9)]


def test_the_rows_still_fit_the_window(first_words: list[tuple[str, int, int, bytes, bytes]]) -> None:
    for language, partner, number, original, after in first_words:
        assert after.endswith(END_OF_MESSAGE)
        for row_before, row_after in zip(rows_of(original), rows_of(after), strict=True):
            if row_after != row_before:
                assert len(row_after) <= max(LONGEST_ROW, len(row_before)), (
                    f"{language} {PARTNER_FILES[partner]} {number}: the row {row_after!r} is too long"
                )


# ---------------------------------------------------------------------------------------------------------
# The adoption.


def test_the_adoption_scene_is_up_to_date() -> None:
    header = REPOSITORY_ROOT / "mods" / "marry_anyone" / "adoption_scene.h"
    assert header.read_text(encoding="utf-8") == make_adoption_scene.header_text(), (
        "run python3 tools/make_adoption_scene.py"
    )


def test_the_adoption_scripts_read_as_scripts() -> None:
    for visitor in make_adoption_scene.VISITORS:
        script, spouse_places = make_adoption_scene.scene_script(visitor)
        parsed = parse_script(script)
        offsets = parsed.instruction_offsets
        assert parsed.header.main_entry in offsets
        assert all(
            instruction.branch_target in offsets
            for instruction in parsed.instructions
            if instruction.branch_target
        ), "every jump and call lands on an instruction"
        assert spouse_places
        assert all(script[place] == make_adoption_scene.SPOUSE_PLACEHOLDER for place in spouse_places)
        assert all(script[place - 1] == 0x02 for place in spouse_places), "each place is a PUSH_INT's"


def every_adoption_text() -> list[tuple[str, str, str, list[str]]]:
    return [
        (visitor, language, couple, make_adoption_scene.lines_of(visitor, language, couple))
        for visitor in make_adoption_scene.VISITORS
        for language in make_adoption_scene.LINES[visitor]
        for couple in ("men", "women")
    ]


def test_the_adoption_lines_fit_the_window() -> None:
    for visitor, language, _, lines in every_adoption_text():
        for text in lines:
            message = make_adoption_scene.message_bytes(visitor, make_adoption_scene.VISITOR_LINE, text)
            body = message[2:].removesuffix(END_OF_MESSAGE)
            for page in body.split(NEW_PAGE):
                rows = page.split(NEW_ROW)
                assert len(rows) <= make_adoption_scene.ROWS_PER_PAGE, f"{visitor} {language}: {text!r}"
                for row in rows:
                    letters = len(row.replace(make_adoption_scene.BABYS_NAME, b"N" * 8))
                    assert letters <= LONGEST_ROW, f"{visitor} {language}: the row {row!r} is too long"


def test_the_pregnancy_of_a_same_sex_couple_becomes_an_adoption(facts: Facts) -> None:
    assert facts["adoption"]["pregnancy_starts"] == 0
    assert facts["adoption"]["days_after_the_pregnancy"] == 60
    assert facts["adoption"]["days_already_counted"] == 30
    assert facts["adoption"]["birth_starts"] == 1
    assert facts["adoption"]["birth_of_a_man_and_a_woman_starts"] == 1


def expected_script(visitor: str, spouse: int) -> bytes:
    script, spouse_places = make_adoption_scene.scene_script(visitor)
    expected = bytearray(script)
    for place in spouse_places:
        expected[place] = spouse
    return bytes(expected)


def test_the_birth_script_becomes_the_adoption_scene(facts: Facts) -> None:
    for visitor, spouse in (("sprite", 20), ("goddess", 6)):
        loaded = bytes.fromhex(str(facts[f"adoption_script_{visitor}"]["bytes"]))
        script = expected_script(visitor, spouse)
        assert loaded[: len(script)] == script, f"the {visitor}'s scene"
        assert loaded[len(script) :] == b"\x55" * 4, "nothing after the scene changed"
    assert facts["adoption"]["spouse_bits"] == 0xFF & ~(1 << 5), "the talk about the birth is cleared"
    assert facts["adoption"]["second_load_untouched"] == 1
    assert facts["adoption"]["a_man_and_a_woman_keep_the_birth"] == 1


def test_the_birth_messages_become_the_adoption_lines(facts: Facts) -> None:
    for case, visitor, language, couple, spouse in (
        ("adoption_messages", "sprite", "s", "men", 20),
        ("adoption_messages_goddess", "goddess", "e", "women", 6),
    ):
        entries = [int(value) for value in str(facts[case]["entries"]).split()]
        text = bytes.fromhex(str(facts[case]["text"]))
        lines = make_adoption_scene.lines_of(visitor, language, couple)
        position = 0
        for number, (speaker, line) in enumerate(zip(make_adoption_scene.SPEAKERS, lines, strict=True)):
            message = bytearray(make_adoption_scene.message_bytes(visitor, speaker, line))
            if speaker == make_adoption_scene.SPOUSE_LINE:
                message[1] = spouse
            assert entries[number] == position, f"{case}: message {number}"
            assert text[position : position + len(message)] == bytes(message), f"{case}: message {number}"
            position += len(message)
        assert entries[len(lines) :] == [number * 0x20 for number in range(len(lines), 48)]


def test_the_adoption_fits_every_birth_on_the_disc() -> None:
    if not SCRIPT_DIRECTORY.is_dir():
        pytest.skip("needs the game's files (orig/G4AEE9)")
    for visitor in make_adoption_scene.VISITORS:
        script, _ = make_adoption_scene.scene_script(visitor)
        for number in range(20):
            path = SCRIPT_DIRECTORY / f"Eve_child{number:02}.lsb"
            assert len(script) <= path.stat().st_size, f"the {visitor}'s scene is longer than {path.name}"
    for visitor, language, couple, _ in every_adoption_text():
        size = len(make_adoption_scene.messages_of(visitor, language, couple))
        for file_number in ("00", "10"):
            folder = DISC_DIRECTORY / "files" / f"message_{language}" / "event"
            data = (folder / f"eve_child{file_number}_{language}.bin").read_bytes()
            highest = struct.unpack_from(">H", data, 2)[0]
            text_start = struct.unpack_from(">I", data, 12)[0]
            assert highest + 1 >= len(make_adoption_scene.SPEAKERS)
            assert size <= len(data) - text_start, f"the {visitor}'s lines ({language}) do not fit"
