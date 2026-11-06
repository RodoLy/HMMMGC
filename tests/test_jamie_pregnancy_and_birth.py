"""Jamie in the pregnancy and birth scenes: where Jamie's lines go, and what the mod borrows from the game.

Jamie en las escenas del embarazo y del parto: dónde van las frases de Jamie y lo que el mod toma prestado del
juego.

mods/jamie_marriage lends Jamie the pregnancy and birth scenes of partner 4 when the player is the boy and of
partner 13 when the player is the girl ("Babies" in mod.c); tests/test_jamie_scene_lines.py checks Jamie's
lines. The first tests build the mod's hooks into a small program with a fake memory
(tests/jamie_mod_driver.py) and play the moments of the field check that the mod hooks: when Jamie stands in
for the partner, when Jamie is the spouse again, and what lands in the files of the scenes. The last ones
check, on the game's files, what the mod relies on: where the scripts name the partner
(tools/script_actors.py), that Jamie's lines replace messages the scenes show and fit over messages they never
show, and the field check's code at the addresses the mod reads and hooks. They are skipped without the game.
"""

import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))

from jamie_mod_driver import (  # noqa: E402
    ANOTHER_SPOUSE,
    check_jamies_lines_are_over_the_unused_messages,
    check_the_messages_are_as_loaded,
    fact,
    run_driver,
)
from jamie_mod_source import (  # noqa: E402
    EVENT_MESSAGE_TABLE_ADDRESS,
    EVENT_SCRIPT_TABLE_ADDRESS,
    JAMIE_CHARACTER_NUMBER,
    MOD_SOURCE,
    Scene,
    SceneLine,
    encode_for_the_game,
    hex_macros,
    read_macros,
    read_scene_languages,
    read_scenes,
    room_of_the_unused_messages,
)

from dol import Dol  # noqa: E402
from lsb import load_script  # noqa: E402
from project import MAIN_DOL_PATH, SCRIPT_DIRECTORY, SYMBOLS_PATH  # noqa: E402
from symbols import SymbolTable  # noqa: E402

SHOW_MESSAGE_BUILTIN = 51  # PUSH_INT <message>; CALL_BUILTIN 51 in the event scripts

# ---------------------------------------------------------------------------------------------------------
# What mod.c says about the scenes.
# ---------------------------------------------------------------------------------------------------------


@pytest.fixture(scope="module")
def source() -> str:
    return MOD_SOURCE.read_text(encoding="utf-8")


@pytest.fixture(scope="module")
def scenes(source) -> dict[str, Scene]:
    """The scenes of a partner: the pregnancy and the birth, each in the version for the boy and the girl."""
    return {name: scene for name, scene in read_scenes(source).items() if scene.kind == "SCENE_OF_A_PARTNER"}


@pytest.fixture(scope="module")
def scene_lines(source) -> dict[str, list[SceneLine]]:
    return read_scene_languages(source)


def lines_of(scene_lines, letter: str, scene: Scene) -> list[SceneLine]:
    return [line for line in scene_lines[letter] if line.scene == scene.name]


def test_there_is_a_scene_for_each_kind_and_each_version(scenes):
    assert len(scenes) == 4
    assert {(scene.is_the_birth, scene.jamie_is_the_wife) for scene in scenes.values()} == {
        (False, True),
        (False, False),
        (True, True),
        (True, False),
    }


# ---------------------------------------------------------------------------------------------------------
# The hooks, run on a fake memory.
# ---------------------------------------------------------------------------------------------------------

HOUSE_AREA = 27  # the first house of the player
NOT_A_HOUSE_AREA = 0  # the farm in spring
PARTNERS_OWN_FRIENDSHIP = 100
JAMIES_FRIENDSHIP = 950
ANOTHER_EVENT = 0x100

SCENES_DRIVER = r"""
/* Plays, on the fake memory, the moments of the field check that the mod hooks for the babies. */
#define WIFE PARTNER_JAMIE_STANDS_IN_FOR_AS_THE_WIFE
#define HUSBAND PARTNER_JAMIE_STANDS_IN_FOR_AS_THE_HUSBAND
#define TALKS_ABOUT_THE_BIRTH CHARACTER_BIT_TALKS_ABOUT_THE_BIRTH

/* A married player who has just come into the first house, with every condition of the pregnancy met. */
static void set_up(uint32_t spouse, uint32_t player_character)
{
    set_up_the_memory(spouse, player_character);
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
    clear_the_script();
    put_the_partner_in_the_script(scene);
    load_the_script(script_path); /* the event's script, when the event begins */
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
    clear_the_script();
    put_the_partner_in_the_script(pregnancy_as_the_wife);
    load_the_script("/Script/Eve_conce01.lsb");
    report_the_script("another_spouse", pregnancy_as_the_wife, pregnancy_as_the_wife->partner);

    /* A script that is not the one the mod knows (one of the places holds another number): left as it is. */
    set_up(JAMIE_CHARACTER_NUMBER, PLAYER_CHARACTER_THE_BOY);
    clear_the_script();
    put_the_partner_in_the_script(pregnancy_as_the_wife);
    bytes_at(SCRIPT_FILE)[pregnancy_as_the_wife->partner_bytes[3]] = ANOTHER_SPOUSE;
    load_the_script("/Script/Eve_conce01.lsb");
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
    numbers = {
        "HOUSE_AREA": HOUSE_AREA,
        "NOT_A_HOUSE_AREA": NOT_A_HOUSE_AREA,
        "PARTNERS_OWN_FRIENDSHIP": PARTNERS_OWN_FRIENDSHIP,
        "JAMIES_FRIENDSHIP": JAMIES_FRIENDSHIP,
        "ANOTHER_EVENT": ANOTHER_EVENT,
    }
    return run_driver(tmp_path_factory.mktemp("scenes"), SCENES_DRIVER, numbers)


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
    check_the_messages_are_as_loaded(results, "another_spouse")
    assert fact(results, "another_spouse", "places_with_the_value") == len(scene.partner_bytes)
    assert fact(results, "another_spouse", "other_bytes_changed") == 0


PREGNANCY_CASES = [
    ("pregnancy_as_the_wife", "SCENE_PREGNANCY_AS_THE_WIFE", "e"),
    ("pregnancy_as_the_husband", "SCENE_PREGNANCY_AS_THE_HUSBAND", "s"),
]


@pytest.mark.parametrize(("case", "scene_name", "letter"), PREGNANCY_CASES)
def test_the_scenes_script_gets_jamie_at_every_place_and_nowhere_else(
    results, scenes, case, scene_name, letter
):
    assert fact(results, case, "places_with_the_value") == len(scenes[scene_name].partner_bytes)
    assert fact(results, case, "other_bytes_changed") == 0


def test_a_script_the_mod_does_not_know_is_left_as_it_is(results):
    assert fact(results, "unknown_script", "places_with_the_value") == 0
    assert fact(results, "unknown_script", "other_bytes_changed") == 0


@pytest.mark.parametrize(("case", "scene_name", "letter"), PREGNANCY_CASES)
def test_the_scenes_messages_get_jamies_lines_over_the_unused_ones(
    results, scenes, scene_lines, case, scene_name, letter
):
    scene = scenes[scene_name]
    check_jamies_lines_are_over_the_unused_messages(
        results, case, scene, lines_of(scene_lines, letter, scene)
    )


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


@needs_the_scripts
def test_the_places_of_the_partner_are_the_ones_the_tool_finds_in_the_scripts(scenes):
    import script_actors

    scripts = script_actors.disc_scripts(SCRIPT_DIRECTORY)
    arities = script_actors.value_builtin_arities(scripts)
    slots = script_actors.actor_slots(scripts, arities)
    for scene in scenes.values():
        script = load_script(SCRIPT_DIRECTORY / f"{scene.script_name}.lsb")
        found = script_actors.character_references(script, scene.partner, arities, slots)
        assert scene.partner_bytes == tuple(reference.file_offset for reference in found), scene.name


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
            room = room_of_the_unused_messages(scene, letter)
            needed = sum(len(encode_for_the_game(line.text)) for line in lines_of(scene_lines, letter, scene))
            assert needed <= room, f"{scene.name}, language {letter}: {needed} bytes for {room}"


# The field check's code in main.dol, as tools/disasm.py shows it.

FIELD_CHECK = "fn_80166888"
BIRTH_CHECK_ADDRESS = 0x8016809C  # the first instruction of the birth check, among the morning checks
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
