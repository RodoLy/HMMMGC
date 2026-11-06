"""Jamie on the special days of a married couple: what the mod changes in their scripts and messages.

Jamie en los días especiales de un matrimonio: lo que el mod cambia en sus scripts y sus mensajes.

The morning when the spouse asks the player to come home early (Eve_memo03), the evening of the wedding
anniversary (Eve_memo00) and of the child's birthday (Eve_memo01), and the child's checkup (Eve_memo04) each
have one script for every spouse, which picks the spouse's lines by the spouse's number. mods/jamie_marriage
makes Jamie take partner 4's lines when the player is the boy and partner 13's when the player is the girl
("Special days" in mod.c); tests/test_jamie_scene_lines.py checks Jamie's lines. The first tests play, on a
fake memory (tests/jamie_mod_driver.py), the game loading those files: Jamie's number goes over the partner's
only in the version for the player's gender, and Jamie's lines over messages that the scene does not show. The
last ones check, on the game's files, what the mod relies on: where the scripts compare the spouse with the
partner (tools/spouse_switches.py), that Jamie's lines are the messages of the partner's cases and fit over
messages that only the cases of other partners show, and that the field check starts each of these events.
They are skipped without the game.
"""

import sys
from collections import Counter
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent / "tools"))
sys.path.insert(0, str(Path(__file__).resolve().parent))

from jamie_mod_driver import (  # noqa: E402
    check_jamies_lines_are_over_the_unused_messages,
    check_the_messages_are_as_loaded,
    fact,
    run_driver,
)
from jamie_mod_source import (  # noqa: E402
    EVENT_MESSAGE_TABLE_ADDRESS,
    EVENT_SCRIPT_TABLE_ADDRESS,
    MOD_SOURCE,
    Scene,
    SceneLine,
    encode_for_the_game,
    read_scene_languages,
    read_scenes,
    room_of_the_unused_messages,
)
from spouse_switches import partner_bytes, resolve_any_case, show_message_sites, spouse_cases  # noqa: E402

from dol import Dol  # noqa: E402
from lsb import load_script  # noqa: E402
from project import MAIN_DOL_PATH, SCRIPT_DIRECTORY, SYMBOLS_PATH  # noqa: E402
from symbols import SymbolTable  # noqa: E402

# The special days, by the name of their cases in the program, with the scenes of their two versions.
SPECIAL_DAYS = {
    "come_home_early": ("SCENE_COME_HOME_EARLY_AS_THE_WIFE", "SCENE_COME_HOME_EARLY_AS_THE_HUSBAND"),
    "anniversary": ("SCENE_ANNIVERSARY_AS_THE_WIFE", "SCENE_ANNIVERSARY_AS_THE_HUSBAND"),
    "childs_birthday": ("SCENE_CHILDS_BIRTHDAY_AS_THE_WIFE", "SCENE_CHILDS_BIRTHDAY_AS_THE_HUSBAND"),
    "childs_checkup": ("SCENE_CHILDS_CHECKUP_AS_THE_WIFE", "SCENE_CHILDS_CHECKUP_AS_THE_HUSBAND"),
}

# ---------------------------------------------------------------------------------------------------------
# What mod.c says about the special days.
# ---------------------------------------------------------------------------------------------------------


@pytest.fixture(scope="module")
def source() -> str:
    return MOD_SOURCE.read_text(encoding="utf-8")


@pytest.fixture(scope="module")
def scenes(source) -> dict[str, Scene]:
    """The scenes for every spouse: the special days, each in the version for the boy and the girl."""
    return {
        name: scene for name, scene in read_scenes(source).items() if scene.kind == "SCENE_FOR_EVERY_SPOUSE"
    }


@pytest.fixture(scope="module")
def scene_lines(source) -> dict[str, list[SceneLine]]:
    return read_scene_languages(source)


def lines_of(scene_lines, letter: str, scene: Scene) -> list[SceneLine]:
    return [line for line in scene_lines[letter] if line.scene == scene.name]


def test_each_special_day_has_a_version_for_the_boy_and_one_for_the_girl(scenes):
    assert set(scenes) == {name for versions in SPECIAL_DAYS.values() for name in versions}
    for wife_name, husband_name in SPECIAL_DAYS.values():
        wife, husband = scenes[wife_name], scenes[husband_name]
        assert wife.jamie_is_the_wife and not husband.jamie_is_the_wife
        assert wife.partner != husband.partner
        # The two versions are the same event, with the same files: what differs is the partner.
        assert (wife.event, wife.script_name, wife.message_file) == (
            husband.event,
            husband.script_name,
            husband.message_file,
        )
        assert wife.unused_messages == husband.unused_messages


# ---------------------------------------------------------------------------------------------------------
# The hooks, run on a fake memory.
# ---------------------------------------------------------------------------------------------------------

SPECIAL_DAYS_DRIVER = r"""
/*
 * The special days, as the game loads their files: the script, with the places of both versions in it, and
 * the messages, in English when the player is the boy and in Spanish when the player is the girl. Prints the
 * places of the version for the player's gender, which hold Jamie's number or, with another spouse, still the
 * partner's, and the messages.
 */
static void load_a_special_day(const char* name, uint32_t spouse, uint32_t player_character,
                               JamieSceneId as_the_wife, JamieSceneId as_the_husband)
{
    set_up_the_memory(spouse, player_character);
    const JamieScene* wife = &jamie_scenes[as_the_wife];
    const JamieScene* husband = &jamie_scenes[as_the_husband];
    const JamieScene* players_version = player_character == PLAYER_CHARACTER_THE_BOY ? wife : husband;
    const char language = player_character == PLAYER_CHARACTER_THE_BOY ? 'e' : 's';
    char path[LONGEST_PATH];

    clear_the_script();
    put_the_partner_in_the_script(wife);
    put_the_partner_in_the_script(husband);
    snprintf(path, sizeof path, "/Script/%s.lsb", wife->script_name);
    load_the_script(path);
    const bool jamie_is_the_spouse = spouse == JAMIE_CHARACTER_NUMBER;
    report_the_script(name, players_version, jamie_is_the_spouse ? spouse : players_version->partner);

    make_the_event_messages();
    snprintf(path, sizeof path, "/message_%c/event/%s_%c.bin", language, wife->message_file, language);
    load_file(path, MESSAGES_FILE);
    report_the_event_messages(name);
}

int main(void)
{
    static const struct {
        const char* name;
        JamieSceneId as_the_wife;
        JamieSceneId as_the_husband;
    } special_days[] = {
        {"come_home_early", SCENE_COME_HOME_EARLY_AS_THE_WIFE, SCENE_COME_HOME_EARLY_AS_THE_HUSBAND},
        {"anniversary", SCENE_ANNIVERSARY_AS_THE_WIFE, SCENE_ANNIVERSARY_AS_THE_HUSBAND},
        {"childs_birthday", SCENE_CHILDS_BIRTHDAY_AS_THE_WIFE, SCENE_CHILDS_BIRTHDAY_AS_THE_HUSBAND},
        {"childs_checkup", SCENE_CHILDS_CHECKUP_AS_THE_WIFE, SCENE_CHILDS_CHECKUP_AS_THE_HUSBAND},
    };
    char name[64];
    for (uint32_t index = 0; index < HM_MM_ARRAY_COUNT(special_days); ++index) {
        const JamieSceneId wife = special_days[index].as_the_wife;
        const JamieSceneId husband = special_days[index].as_the_husband;
        snprintf(name, sizeof name, "%s_as_the_wife", special_days[index].name);
        load_a_special_day(name, JAMIE_CHARACTER_NUMBER, PLAYER_CHARACTER_THE_BOY, wife, husband);
        snprintf(name, sizeof name, "%s_as_the_husband", special_days[index].name);
        load_a_special_day(name, JAMIE_CHARACTER_NUMBER, PLAYER_CHARACTER_THE_GIRL, wife, husband);
        snprintf(name, sizeof name, "%s_with_another_spouse", special_days[index].name);
        load_a_special_day(name, ANOTHER_SPOUSE, PLAYER_CHARACTER_THE_BOY, wife, husband);
    }
    return 0;
}
"""


@pytest.fixture(scope="module")
def results(tmp_path_factory) -> dict[str, dict[str, str]]:
    """What the program printed: for each case, its facts and their values."""
    return run_driver(tmp_path_factory.mktemp("special_days"), SPECIAL_DAYS_DRIVER, {})


# (the end of the case's name, which version of the day, the language of the messages that the program loads)
VERSIONS = [("as_the_wife", 0, "e"), ("as_the_husband", 1, "s")]


@pytest.mark.parametrize("day", SPECIAL_DAYS)
@pytest.mark.parametrize(("version", "which", "letter"), VERSIONS)
def test_jamie_takes_the_partners_place_only_in_the_version_for_the_players_gender(
    results, scenes, day, version, which, letter
):
    case = f"{day}_{version}"
    assert fact(results, case, "places_with_the_value") == len(scenes[SPECIAL_DAYS[day][which]].partner_bytes)
    assert fact(results, case, "other_bytes_changed") == 0  # the other version's places keep their partner


@pytest.mark.parametrize("day", SPECIAL_DAYS)
@pytest.mark.parametrize(("version", "which", "letter"), VERSIONS)
def test_jamies_lines_go_over_the_unused_messages_in_the_players_language(
    results, scenes, scene_lines, day, version, which, letter
):
    scene = scenes[SPECIAL_DAYS[day][which]]
    case = f"{day}_{version}"
    check_jamies_lines_are_over_the_unused_messages(
        results, case, scene, lines_of(scene_lines, letter, scene)
    )


@pytest.mark.parametrize("day", SPECIAL_DAYS)
def test_nothing_changes_when_jamie_is_not_the_spouse(results, scenes, day):
    case = f"{day}_with_another_spouse"
    assert fact(results, case, "places_with_the_value") == len(scenes[SPECIAL_DAYS[day][0]].partner_bytes)
    assert fact(results, case, "other_bytes_changed") == 0
    check_the_messages_are_as_loaded(results, case)


# ---------------------------------------------------------------------------------------------------------
# What the mod relies on, checked on the game's files.
# ---------------------------------------------------------------------------------------------------------

needs_the_scripts = pytest.mark.skipif(
    not SCRIPT_DIRECTORY.is_dir(), reason="needs the extracted disc (tools/extract_disc.py)"
)


def script_of(scene: Scene):
    """The scene's script from the disc, whose file names differ in case from the game's (eve_memo00.lsb)."""
    return load_script(resolve_any_case(scene.script_name))


@needs_the_scripts
def test_the_places_are_the_partners_comparisons_and_copies_that_the_tool_finds(scenes):
    for scene in scenes.values():
        found = partner_bytes(spouse_cases(script_of(scene)), scene.partner)
        assert scene.partner_bytes == tuple(found), scene.name


@needs_the_scripts
def test_jamies_lines_are_the_messages_of_the_partners_cases(scenes, scene_lines):
    for scene in scenes.values():
        cases = spouse_cases(script_of(scene))
        partners_cases = [case for case in cases if case.partner == scene.partner]
        partners_messages = {message for case in partners_cases for message in case.messages}
        assert {line.number for line in lines_of(scene_lines, "e", scene)} == partners_messages, scene.name


@needs_the_scripts
def test_only_the_cases_of_other_partners_show_the_messages_under_jamies_lines(scenes):
    for scene in scenes.values():
        script = script_of(scene)
        other_partners_cases = [case for case in spouse_cases(script) if case.partner != scene.partner]
        shown = Counter()
        for offset, message in show_message_sites(script):
            if message in scene.unused_messages:
                shown[message] += 1
                assert any(offset in case.case_code for case in other_partners_cases), (
                    f"{scene.name}: message {message} is shown at {offset:#x}, outside other partners' cases"
                )
        assert shown, f"{scene.name}: the script never shows the messages under Jamie's lines"


@needs_the_scripts
def test_jamies_lines_fit_over_the_unused_messages_in_every_language(scenes, scene_lines):
    for scene in scenes.values():
        for letter in scene_lines:
            room = room_of_the_unused_messages(scene, letter)
            needed = sum(len(encode_for_the_game(line.text)) for line in lines_of(scene_lines, letter, scene))
            assert needed <= room, f"{scene.name}, language {letter}: {needed} bytes for {room}"


# The field check's code in main.dol, as tools/disasm.py shows it.

FIELD_CHECK = "fn_80166888"
START_AN_EVENT = "fn_800FC2B4"  # fn_800FC2B4(event manager, event)
LOAD_R4_WITH = 0x38800000  # li r4, <the low 16 bits>
BRANCH_AND_LINK = 0x48000001  # bl <target>, with the distance in the bits between
BRANCH_DISTANCE_BITS = 0x03FFFFFC


@pytest.fixture(scope="module")
def dol() -> Dol:
    if not MAIN_DOL_PATH.is_file():
        pytest.skip("needs the extracted disc (tools/extract_disc.py)")
    return Dol(MAIN_DOL_PATH)


def call_target(dol: Dol, address: int) -> int | None:
    """Where the `bl` at `address` calls, or None for another instruction."""
    word = dol.u32(address)
    if word & ~BRANCH_DISTANCE_BITS != BRANCH_AND_LINK:
        return None
    distance = word & BRANCH_DISTANCE_BITS
    return address + (distance - 0x04000000 if distance & 0x02000000 else distance)


def test_the_event_tables_name_the_files_of_each_special_day(dol, scenes):
    for scene in scenes.values():
        script = dol.c_string(dol.u32(EVENT_SCRIPT_TABLE_ADDRESS + 4 * scene.event))
        messages = dol.c_string(dol.u32(EVENT_MESSAGE_TABLE_ADDRESS + 4 * scene.event))
        assert script == scene.script_name, scene.name
        assert messages == f"/message_e/event/{scene.message_file}_e.bin", scene.name


def test_the_field_check_starts_each_special_day(dol, scenes):
    symbols = SymbolTable.read(SYMBOLS_PATH)
    field_check = symbols.by_name(FIELD_CHECK)
    start_an_event = symbols.by_name(START_AN_EVENT)
    started = {
        dol.u32(address) & 0xFFFF
        for address in range(field_check.address, field_check.address + field_check.size, 4)
        if dol.u32(address) >> 16 == LOAD_R4_WITH >> 16
        and call_target(dol, address + 4) == start_an_event.address
    }
    for scene in scenes.values():
        assert scene.event in started, f"{scene.name}: the field check never starts event {scene.event:#x}"
