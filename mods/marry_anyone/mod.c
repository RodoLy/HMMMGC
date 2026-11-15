/*
 * marry_anyone: any of the twenty partners can be courted and married, whichever character the player chose.
 * marry_anyone: cualquiera de las veinte parejas se puede cortejar y casar, sea cual sea el personaje del jugador.
 *
 * How the game ties marriage to the player (save data + 0x227C: 0 the boy, 1 the girl), found in its code:
 *
 *   - fn_800A2B60(character) says whether a character is one the player can marry: 3 to 12 for the boy, 13 to
 *     22 for the girl. talk.lsb asks it (built-in 189) when the player gives the blue feather: only then does
 *     the character accept (message 38) and become the fiance (save data + 0x2330).
 *   - fn_800A311C(least) gives the partner the player is closest to, from those of the player's character (the
 *     spouse once married); the field check (fn_80166888) makes that partner the player's date at four
 *     festivals (from 0x80169D20). fn_800A3334() says whether one of those partners, or Jamie, is at 800 of
 *     friendship or more, which lets the field check start Eve_marry00, the village's advice before marrying.
 *   - The field check starts the second love scene of each partner (at 500 of friendship, events 0x9E to
 *     0xB1) only for the player's character: it reads the character at 0x8016B438 and goes to the chain of the
 *     boy's partners or, from 0x8016B650, to the girl's.
 *   - The wedding: the field check reads the character again at 0x8016727C and looks for the fiance among the
 *     boy's partners (3 to 12: Eve_marry02 to 11, events 0xB4 to 0xBD) or, from 0x80167994, among the girl's
 *     (13 to 22: Eve_marry12 to 21, events 0xBE to 0xC7). Each wedding script dresses the player for the
 *     wedding as its own character: actor 72 is the boy in his suit, 73 the girl in her dress.
 *
 * What this mod does:
 *
 *   - Built-in 189 (fn_80057AC4) asks fn_800A2B60 about a partner made for the player's character instead
 *     of one of the other's, so that it says yes to all twenty and the blue feather works with any of them.
 *     (The translator copied fn_800A2B60 into the built-in, so the question is changed, not the answer.)
 *   - fn_800A311C and fn_800A3334 look at all twenty.
 *   - At the branch of the love scenes (0x8016B444) it sends the field check to the chain that has a scene
 *     waiting, the player's own first; at the branch of the wedding (0x80167288), to the chain of the fiance.
 *   - When a wedding script is loaded for a player of the other character, it dresses the player as the
 *     player is: the wedding of a boy and one of the girl's partners uses that partner's script, with the boy
 *     in his suit. The two roles of the script make the same moves, so nothing else changes.
 *   - The rival scenes (Eve_npc00 to 09, each about two partners who are drawn to each other) do not start
 *     when the player is engaged or married to one of the two.
 *   - A couple of the same sex cannot have the pregnancy (Eve_conce): when the game would start it, the mod
 *     keeps it back and counts its days as over instead. The next morning the game starts the birth's event
 *     (Eve_child), and the mod writes over its script and its messages the adoption scene that
 *     tools/make_adoption_scene.py makes (adoption_scene.h): at the Harvest Goddess's spring, the Goddess, or
 *     one of her sprites while she is still stone, gives the couple a baby with no family, the player names
 *     it, places its crib at home and gets the birth's musical note. The game's own code for the birth does
 *     the rest, so the child grows as any other.
 *   - The first thing a spouse says after the wedding (message 9, and 100 when the player has chosen what the
 *     spouse will call them) has words that go by the player's sex in many partners' lines: "husband" or
 *     "wife", "Mrs.", "married" in the languages where it agrees with the couple. For a couple of the same sex
 *     the mod changes those words in the copy of the message file the game has just loaded, in the five
 *     languages of the disc. Nothing of the game's text is in this file: only the words that are swapped.
 *
 * Cómo ata el juego el matrimonio al jugador (datos de la partida + 0x227C: 0 el chico, 1 la chica):
 * fn_800A2B60 dice quién puede casarse con el jugador (3 a 12 para el chico, 13 a 22 para la chica) y
 * talk.lsb la consulta al dar la pluma azul; fn_800A311C y fn_800A3334 eligen entre las parejas del personaje
 * del jugador; la revisión del campo da la segunda escena de amor y la boda solo con las parejas de su
 * personaje, y cada guion de boda viste al jugador como su personaje (72 el traje del chico, 73 el vestido de
 * la chica). El mod hace que las veinte parejas cuenten, lleva la revisión a la cadena que corresponde, viste
 * al jugador como es en la boda, no deja empezar las escenas de rivales de su pareja, cambia «esposo» o
 * «esposa», «casados» o «casadas» y palabras así en las primeras palabras del cónyuge y, en lugar del embarazo
 * y el parto de una pareja del mismo sexo, la Diosa de la Cosecha (o un duende, si sigue de piedra) les da un
 * bebé en adopción en su manantial.
 */

#include <stdbool.h>
#include <stddef.h>
#include <string.h>

#include "hm_mm_mod.h"

#include "adoption_scene.h"

#define MOD_ID "marry_anyone"

#define WORD_BYTES 4
#define HALF_WORD_BYTES 2
#define BYTE_BYTES 1
#define BITS_PER_BYTE 8u

/* The save data: a pointer to it, 0 until a game is loaded. */
#define SAVE_DATA_POINTER_ADDRESS 0x80321F48u
#define NO_SAVE_DATA 0u

#define MAIN_FLAGS_OFFSET 0x04u
#define FLAG_MARRIED 0u
#define STARTED_EVENTS_OFFSET 0x6Cu
#define PLAYER_CHARACTER_OFFSET 0x227Cu /* 0 the boy, 1 the girl */
#define PLAYER_IS_THE_BOY 0u
#define PLAYER_IS_THE_GIRL 1u
#define SPOUSE_OFFSET 0x22B8u
#define FIANCE_OFFSET 0x2330u
#define NOBODY 126u /* the spouse and the fiance while there is none */
/* The friendship with each character: {value, minimum, maximum}, 12 bytes for each character number. */
#define FRIENDSHIP_OFFSET 0x6628u
#define FRIENDSHIP_BYTES_EACH 12u

/* Characters: Jamie is 2, the boy's partners 3 to 12, the girl's 13 to 22. */
#define JAMIE 2u
#define FIRST_PARTNER_OF_THE_BOY 3u
#define FIRST_PARTNER_OF_THE_GIRL 13u
#define PARTNERS_OF_EACH 10u
#define LAST_PARTNER 22u

/*
 * Built-in 189 (fn_80057AC4) pops a character from the script's stack and, at 0x80057B3C, loads it from the
 * popped slot (r4 + 4) into r3 for fn_800A2B60.
 */
#define ASK_IF_A_PARTNER_ADDRESS 0x80057B3Cu
#define POPPED_SLOT_REGISTER 4
#define POPPED_SLOT_VALUE_OFFSET 4u

/* The functions that choose among the partners. */
#define CLOSEST_PARTNER_FUNCTION DOLRECOMP_SYMBOL_fn_800A311C
#define SOMEONE_LOVES_THE_PLAYER_FUNCTION DOLRECOMP_SYMBOL_fn_800A3334
#define LOVE_FOR_THE_VILLAGE_ADVICE 800u

/*
 * The branches of the field check (fn_80166888) that go by the player's character. At each, r3 holds the
 * character just read (the translated code compares r3; the console's code compared it into cr0 just
 * before): 0 goes on to the boy's chain, anything else to the girl's.
 */
#define LOVE_SCENE_BRANCH_ADDRESS 0x8016B444u
#define WEDDING_BRANCH_ADDRESS 0x80167288u
#define CR0_LESS_THAN 0x80000000u
#define CR0_GREATER_THAN 0x40000000u
#define CR0_EQUAL 0x20000000u
#define CR0_BITS (CR0_LESS_THAN | CR0_GREATER_THAN | CR0_EQUAL)

/* The second love scene of each partner: event 0x9E + (partner - 3), at 500 of friendship. */
#define FIRST_SECOND_LOVE_SCENE 0x9Eu
#define SECOND_LOVE_SCENE_FRIENDSHIP 500u

/*
 * fn_800FC2B4(field, event) starts an event unless it started before: at 0x800FC2F8 r3 says whether it did
 * (the result of fn_800A77DC on the bits of the started events), r30 is the event, and cr0 holds r3's test.
 */
#define EVENT_START_BRANCH_ADDRESS 0x800FC2F8u
#define EVENT_REGISTER 30

/* Eve_npc00 to 09: the rival scenes, each about one of the boy's partners and one of the girl's. */
#define FIRST_RIVAL_SCENE 0xF6u
static const uint8_t rival_couples[][2] = {
    {4, 13}, {10, 14}, {9, 15}, {8, 16}, {5, 17}, {7, 18}, {6, 19}, {3, 20}, {11, 21}, {12, 22},
};
#define RIVAL_SCENES (sizeof rival_couples / sizeof rival_couples[0])

/* Eve_conce00 to 19 (0xC8 to 0xDB) and Eve_child00 to 19 (0xDC to 0xEF): the pregnancy and the birth. */
#define FIRST_PREGNANCY_SCENE 0xC8u
#define FIRST_BIRTH_SCENE 0xDCu
#define LAST_BIRTH_SCENE 0xEFu
/*
 * Value 43 of the scripts: 0, then the days of the pregnancy, 255 once the child is born. The morning checks
 * start the birth when it is 60 or more (see jamie_marriage, "The pregnancy and the birth").
 */
#define PREGNANCY_DAYS_OFFSET 0xCB60u
#define NOT_EXPECTING 0u
#define DAYS_UNTIL_THE_BIRTH 60u
/* Eve_mega02, where the Harvest Goddess wakes up from stone. */
#define EVENT_THE_GODDESS_WAKES_UP 0x105u
/* Each character's bits (3 bytes each); bit 5 of the spouse's is the first conversation after the birth. */
#define CHARACTER_BITS_OFFSET 0x5564u
#define CHARACTER_BITS_BYTES_EACH 3u
#define BIT_TALK_ABOUT_THE_BIRTH 5u

/* The wedding scripts, the actor that is the player in them, and where the player is dressed. */
#define WEDDING_SCRIPT_PREFIX "/Script/Eve_marry"
#define FIRST_WEDDING_SCRIPT 2u
#define FIRST_WEDDING_SCRIPT_OF_THE_GIRL 12u
#define LAST_WEDDING_SCRIPT 21u
#define LSB_MAIN_ENTRY_OFFSET 0x04u
#define LSB_CODE_OFFSET 0x14u
#define LSB_PUSH_INT 0x02u
#define LSB_LONG_INT 0x80u /* a PUSH_INT of 64 to 8191 takes two bytes: 0x80 + the high bits, then the low */
#define ACTOR_BOY_IN_HIS_SUIT 72u
#define ACTOR_GIRL_IN_HER_DRESS 73u
/* How far into main the dressing is (0x22 bytes in every wedding script); a little room is searched. */
#define DRESSING_SEARCH_BYTES 0x40u

/* fn_80029E74(path) loads a whole file and returns its address. */
#define LOAD_FILE_FUNCTION DOLRECOMP_SYMBOL_fn_80029E74
#define LONGEST_PATH 96u

/* fn_8008534C(window, character, message, ...) shows a message from a character's file, which it loads. */
#define SHOW_CHARACTER_MESSAGE_FUNCTION DOLRECOMP_SYMBOL_fn_8008534C
#define MESSAGE_WHAT_TO_CALL_YOU 9u
#define MESSAGE_ANSWER_TO_THE_NAME 100u
#define MESSAGE_FILE_HEADER_HIGHEST_MESSAGE 0x02u
#define MESSAGE_FILE_HEADER_TABLE_START 0x08u
#define MESSAGE_FILE_HEADER_TEXT_START 0x0Cu
#define LONGEST_MESSAGE_BYTES 400u
/* An event's message starts with this code and the number of the character who speaks it. */
#define SPEAKER_CODE 0x1Eu
/* A message ends with these four bytes. */
static const uint8_t end_of_a_message[] = {0x01, 0x08, 0x1F, 0x1D};

static uint32_t read_word(CPUState* cpu, uint32_t address)
{
    return (uint32_t)moderngekko_mod_read(cpu, address, WORD_BYTES);
}

static void write_word(CPUState* cpu, uint32_t address, uint32_t value)
{
    moderngekko_mod_write(cpu, address, value, WORD_BYTES);
}

static uint32_t read_half_word(CPUState* cpu, uint32_t address)
{
    return (uint32_t)moderngekko_mod_read(cpu, address, HALF_WORD_BYTES);
}

static uint8_t read_byte(CPUState* cpu, uint32_t address)
{
    return (uint8_t)moderngekko_mod_read(cpu, address, BYTE_BYTES);
}

static void write_byte(CPUState* cpu, uint32_t address, uint8_t value)
{
    moderngekko_mod_write(cpu, address, value, BYTE_BYTES);
}

static bool is_bit_set(CPUState* cpu, uint32_t array_address, uint32_t bit_number)
{
    const uint8_t byte = read_byte(cpu, array_address + bit_number / BITS_PER_BYTE);
    return (byte & (1u << (bit_number % BITS_PER_BYTE))) != 0;
}

static uint32_t save_data_of(CPUState* cpu)
{
    return read_word(cpu, SAVE_DATA_POINTER_ADDRESS);
}

static bool is_a_partner(uint32_t character)
{
    return character >= FIRST_PARTNER_OF_THE_BOY && character <= LAST_PARTNER;
}

/* The character a partner was made for: the boy's partners are women, the girl's are men. */
static uint32_t player_character_of_the_partner(uint32_t partner)
{
    return partner < FIRST_PARTNER_OF_THE_GIRL ? PLAYER_IS_THE_BOY : PLAYER_IS_THE_GIRL;
}

static uint32_t friendship_with(CPUState* cpu, uint32_t save_data, uint32_t character)
{
    return read_word(cpu, save_data + FRIENDSHIP_OFFSET + character * FRIENDSHIP_BYTES_EACH);
}

/* Whether the player and this partner are of the same sex (Jamie never is: Jamie is always the other one). */
static bool is_of_the_players_sex(CPUState* cpu, uint32_t save_data, uint32_t partner)
{
    if (!is_a_partner(partner)) {
        return false;
    }
    const uint32_t player = read_word(cpu, save_data + PLAYER_CHARACTER_OFFSET) == PLAYER_IS_THE_BOY
                                ? PLAYER_IS_THE_BOY
                                : PLAYER_IS_THE_GIRL;
    return player_character_of_the_partner(partner) != player;
}

/* Sets r3 and cr0 as the console's code left them after comparing r3 with 0 (cmpwi r3, 0). */
static void set_r3_and_its_test(CPUState* cpu, uint32_t value)
{
    cpu->gpr[3] = value;
    const uint32_t test = value == 0 ? CR0_EQUAL : ((int32_t)value < 0 ? CR0_LESS_THAN : CR0_GREATER_THAN);
    cpu->cr = (cpu->cr & ~CR0_BITS) | test;
}

/* ---- Who counts as a partner ---- */

/*
 * Before built-in 189 asks fn_800A2B60 about a character: a partner made for the player's other character is
 * asked about as the partner in the same place among the player's own, which fn_800A2B60 says yes to.
 */
static void ask_about_a_partner_of_the_player(CPUState* cpu)
{
    const uint32_t save_data = save_data_of(cpu);
    if (save_data == NO_SAVE_DATA) {
        return;
    }
    const uint32_t slot = cpu->gpr[POPPED_SLOT_REGISTER] + POPPED_SLOT_VALUE_OFFSET;
    const uint32_t character = read_word(cpu, slot);
    if (!is_of_the_players_sex(cpu, save_data, character)) {
        return;
    }
    const bool player_is_the_boy = read_word(cpu, save_data + PLAYER_CHARACTER_OFFSET) == PLAYER_IS_THE_BOY;
    const uint32_t place = (character - FIRST_PARTNER_OF_THE_BOY) % PARTNERS_OF_EACH;
    write_word(cpu, slot, (player_is_the_boy ? FIRST_PARTNER_OF_THE_BOY : FIRST_PARTNER_OF_THE_GIRL) + place);
}

/* The least friendship fn_800A311C was asked for (it gives a partner with more than that minus one). */
static int32_t asked_least_friendship;

static void before_closest_partner(CPUState* cpu)
{
    asked_least_friendship = (int32_t)moderngekko_mod_arg_u32(cpu, 0);
}

/*
 * The same choice as the game's, among all twenty: the spouse once married; otherwise the partner with the
 * most friendship, the first one on a tie, above the least asked for; nobody (126) when none is.
 */
static void after_closest_partner(CPUState* cpu)
{
    const uint32_t save_data = save_data_of(cpu);
    if (save_data == NO_SAVE_DATA || is_bit_set(cpu, save_data + MAIN_FLAGS_OFFSET, FLAG_MARRIED)) {
        return;
    }
    int32_t best_friendship = asked_least_friendship - 1;
    uint32_t best = NOBODY;
    for (uint32_t partner = FIRST_PARTNER_OF_THE_BOY; partner <= LAST_PARTNER; ++partner) {
        const int32_t friendship = (int32_t)friendship_with(cpu, save_data, partner);
        if (friendship > best_friendship) {
            best_friendship = friendship;
            best = partner;
        }
    }
    cpu->gpr[3] = best;
}

static void after_someone_loves_the_player(CPUState* cpu)
{
    const uint32_t save_data = save_data_of(cpu);
    if (save_data == NO_SAVE_DATA) {
        return;
    }
    for (uint32_t character = JAMIE; character <= LAST_PARTNER; ++character) {
        if (friendship_with(cpu, save_data, character) >= LOVE_FOR_THE_VILLAGE_ADVICE) {
            cpu->gpr[3] = 1;
            return;
        }
    }
    cpu->gpr[3] = 0;
}

/* ---- The field check's two branches ---- */

/* Whether one of the ten partners made for `player` has its second love scene waiting. */
static bool a_second_love_scene_waits(CPUState* cpu, uint32_t save_data, uint32_t player)
{
    const uint32_t first = player == PLAYER_IS_THE_BOY ? FIRST_PARTNER_OF_THE_BOY : FIRST_PARTNER_OF_THE_GIRL;
    for (uint32_t partner = first; partner < first + PARTNERS_OF_EACH; ++partner) {
        const uint32_t scene = FIRST_SECOND_LOVE_SCENE + (partner - FIRST_PARTNER_OF_THE_BOY);
        if (friendship_with(cpu, save_data, partner) >= SECOND_LOVE_SCENE_FRIENDSHIP &&
            !is_bit_set(cpu, save_data + STARTED_EVENTS_OFFSET, scene)) {
            return true;
        }
    }
    return false;
}

/* At the branch to the chains of the second love scenes: the player's own chain first, else the other one. */
static void choose_the_chain_of_love_scenes(CPUState* cpu)
{
    const uint32_t save_data = save_data_of(cpu);
    if (save_data == NO_SAVE_DATA) {
        return;
    }
    const uint32_t player = cpu->gpr[3] == PLAYER_IS_THE_BOY ? PLAYER_IS_THE_BOY : PLAYER_IS_THE_GIRL;
    const uint32_t other = player == PLAYER_IS_THE_BOY ? PLAYER_IS_THE_GIRL : PLAYER_IS_THE_BOY;
    if (!a_second_love_scene_waits(cpu, save_data, player) && a_second_love_scene_waits(cpu, save_data, other)) {
        set_r3_and_its_test(cpu, other);
    }
}

/* At the branch to the chains of the weddings: the chain where the fiance is. */
static void choose_the_chain_of_the_fiance(CPUState* cpu)
{
    const uint32_t save_data = save_data_of(cpu);
    if (save_data == NO_SAVE_DATA) {
        return;
    }
    const uint32_t fiance = read_word(cpu, save_data + FIANCE_OFFSET);
    if (is_a_partner(fiance)) {
        set_r3_and_its_test(cpu, player_character_of_the_partner(fiance));
    }
}

/* ---- Scenes that do not start ---- */

static bool is_engaged_or_married_to(CPUState* cpu, uint32_t save_data, uint32_t character)
{
    return read_word(cpu, save_data + SPOUSE_OFFSET) == character ||
           read_word(cpu, save_data + FIANCE_OFFSET) == character;
}

/* Whether the mod keeps this event from starting now (see the comment at the top). */
static bool event_does_not_start(CPUState* cpu, uint32_t save_data, uint32_t event)
{
    if (event >= FIRST_RIVAL_SCENE && event < FIRST_RIVAL_SCENE + RIVAL_SCENES) {
        const uint8_t* couple = rival_couples[event - FIRST_RIVAL_SCENE];
        return is_engaged_or_married_to(cpu, save_data, couple[0]) ||
               is_engaged_or_married_to(cpu, save_data, couple[1]);
    }
    if (event >= FIRST_PREGNANCY_SCENE && event < FIRST_BIRTH_SCENE) {
        return is_of_the_players_sex(cpu, save_data, read_word(cpu, save_data + SPOUSE_OFFSET));
    }
    return false;
}

/* Whether this is the birth of a couple of the same sex: the adoption scene takes its place. */
static bool is_an_adoption(CPUState* cpu, uint32_t save_data, uint32_t event)
{
    return event >= FIRST_BIRTH_SCENE && event <= LAST_BIRTH_SCENE &&
           is_of_the_players_sex(cpu, save_data, read_word(cpu, save_data + SPOUSE_OFFSET));
}

/* The adoption scene is to be written over the birth's files as the game loads them. */
static bool adoption_pending;

/* At the test of whether an event started before: an event the mod keeps back counts as started. */
static void keep_back_an_event(CPUState* cpu)
{
    const uint32_t save_data = save_data_of(cpu);
    if (save_data == NO_SAVE_DATA || cpu->gpr[3] != 0) {
        return;
    }
    const uint32_t event = cpu->gpr[EVENT_REGISTER];
    if (event_does_not_start(cpu, save_data, event)) {
        set_r3_and_its_test(cpu, 1);
        HM_MM_LOG(MOD_ID, "event 0x%X does not start for this couple", (unsigned int)event);
        /*
         * Instead of the pregnancy, its days are over: the next morning the game starts the birth's event,
         * which becomes the Harvest Goddess's visit.
         */
        const bool is_the_pregnancy = event >= FIRST_PREGNANCY_SCENE && event < FIRST_BIRTH_SCENE;
        if (is_the_pregnancy && read_word(cpu, save_data + PREGNANCY_DAYS_OFFSET) == NOT_EXPECTING) {
            write_word(cpu, save_data + PREGNANCY_DAYS_OFFSET, DAYS_UNTIL_THE_BIRTH);
            HM_MM_LOG(MOD_ID, "the Harvest Goddess will come the next morning");
        }
        return;
    }
    if (is_an_adoption(cpu, save_data, event)) {
        adoption_pending = true;
        HM_MM_LOG(MOD_ID, "event 0x%X becomes the Harvest Goddess's visit", (unsigned int)event);
    }
}

/* ---- Files: the wedding scripts and the spouse's first words ---- */

typedef enum PendingFile {
    NO_PENDING_FILE,
    WEDDING_SCRIPT,
    SPOUSE_MESSAGES,
    ADOPTION_SCRIPT,
    ADOPTION_MESSAGES,
} PendingFile;

static PendingFile pending_file = NO_PENDING_FILE;
/* For SPOUSE_MESSAGES: the message to change and the letter of the language's folder (message_e, ...). */
static uint32_t pending_message_number;
static char pending_language_letter;
static bool pending_speaker_is_a_woman;

/* The message the game is about to show, when it is one whose word the mod changes (0 when not). */
static uint32_t message_to_change;
static bool message_speaker_is_a_woman;

static bool read_path(CPUState* cpu, uint32_t address, char* path, size_t size)
{
    if (address == 0) {
        return false;
    }
    for (size_t index = 0; index < size; ++index) {
        path[index] = (char)read_byte(cpu, address + (uint32_t)index);
        if (path[index] == '\0') {
            return true;
        }
    }
    return false;
}

/* The number in "/Script/Eve_marryNN.lsb", or 0 when the path is not a wedding script. */
static uint32_t wedding_script_number(const char* path)
{
    const size_t prefix_length = strlen(WEDDING_SCRIPT_PREFIX);
    if (strncmp(path, WEDDING_SCRIPT_PREFIX, prefix_length) != 0) {
        return 0;
    }
    const char* digits = path + prefix_length;
    if (digits[0] < '0' || digits[0] > '9' || digits[1] < '0' || digits[1] > '9' || strcmp(digits + 2, ".lsb") != 0) {
        return 0;
    }
    const uint32_t number = (uint32_t)(digits[0] - '0') * 10u + (uint32_t)(digits[1] - '0');
    return number >= FIRST_WEDDING_SCRIPT && number <= LAST_WEDDING_SCRIPT ? number : 0;
}

/* The letter of "/message_X/chara/...", the folder of a language, or '\0'. */
static char language_letter_of_a_character_file(const char* path)
{
    static const char prefix[] = "/message_";
    if (strncmp(path, prefix, sizeof prefix - 1) != 0) {
        return '\0';
    }
    const char letter = path[sizeof prefix - 1];
    return strncmp(path + sizeof prefix, "/chara/", 7) == 0 ? letter : '\0';
}

/* Whether `text` starts with `prefix`, whatever the case of their letters (the disc's names mix cases). */
static bool starts_with_any_case(const char* text, const char* prefix)
{
    for (; *prefix != '\0'; ++text, ++prefix) {
        const char a = (char)(*text >= 'A' && *text <= 'Z' ? *text - 'A' + 'a' : *text);
        const char b = (char)(*prefix >= 'A' && *prefix <= 'Z' ? *prefix - 'A' + 'a' : *prefix);
        if (a != b) {
            return false;
        }
    }
    return true;
}

/* The birth's files that the adoption scene replaces: its script, and its messages ("/message_X/event/...). */
static PendingFile adoption_file(const char* path, char* language_letter)
{
    if (starts_with_any_case(path, "/Script/Eve_child")) {
        return ADOPTION_SCRIPT;
    }
    static const char prefix[] = "/message_";
    if (starts_with_any_case(path, prefix) && path[sizeof prefix - 1] != '\0' &&
        starts_with_any_case(path + sizeof prefix, "/event/eve_child")) {
        *language_letter = path[sizeof prefix - 1];
        return ADOPTION_MESSAGES;
    }
    return NO_PENDING_FILE;
}

static void before_a_file_is_loaded(CPUState* cpu)
{
    pending_file = NO_PENDING_FILE;
    char path[LONGEST_PATH];
    if (!read_path(cpu, moderngekko_mod_arg_u32(cpu, 0), path, sizeof path)) {
        return;
    }
    const uint32_t save_data = save_data_of(cpu);
    if (save_data == NO_SAVE_DATA) {
        return;
    }
    if (adoption_pending) {
        char letter = '\0';
        pending_file = adoption_file(path, &letter);
        if (pending_file != NO_PENDING_FILE) {
            pending_language_letter = letter;
            return;
        }
    }
    const uint32_t wedding = wedding_script_number(path);
    if (wedding != 0) {
        const uint32_t script_player =
            wedding < FIRST_WEDDING_SCRIPT_OF_THE_GIRL ? PLAYER_IS_THE_BOY : PLAYER_IS_THE_GIRL;
        const uint32_t player = read_word(cpu, save_data + PLAYER_CHARACTER_OFFSET);
        if ((player == PLAYER_IS_THE_BOY) != (script_player == PLAYER_IS_THE_BOY)) {
            pending_file = WEDDING_SCRIPT;
        }
        return;
    }
    const char letter = language_letter_of_a_character_file(path);
    if (letter != '\0' && message_to_change != 0) {
        pending_file = SPOUSE_MESSAGES;
        pending_message_number = message_to_change;
        pending_language_letter = letter;
        pending_speaker_is_a_woman = message_speaker_is_a_woman;
    }
}

/*
 * Dresses the player as the player is in a wedding script of the other character: the first PUSH_INT of the
 * other character's wedding clothes in main becomes the player's own. Only that one, and only when the bytes
 * are where they are expected; otherwise the script stays as it is.
 */
static void dress_the_player_for_the_wedding(CPUState* cpu, uint32_t script)
{
    const uint32_t save_data = save_data_of(cpu);
    const bool player_is_the_boy = read_word(cpu, save_data + PLAYER_CHARACTER_OFFSET) == PLAYER_IS_THE_BOY;
    const uint32_t wrong = player_is_the_boy ? ACTOR_GIRL_IN_HER_DRESS : ACTOR_BOY_IN_HIS_SUIT;
    const uint32_t right = player_is_the_boy ? ACTOR_BOY_IN_HIS_SUIT : ACTOR_GIRL_IN_HER_DRESS;
    const uint32_t main = script + LSB_CODE_OFFSET + read_word(cpu, script + LSB_MAIN_ENTRY_OFFSET);
    for (uint32_t at = main; at < main + DRESSING_SEARCH_BYTES; ++at) {
        if (read_byte(cpu, at) == LSB_PUSH_INT && read_byte(cpu, at + 1) == LSB_LONG_INT &&
            read_byte(cpu, at + 2) == wrong) {
            write_byte(cpu, at + 2, (uint8_t)right);
            HM_MM_LOG(MOD_ID, "the wedding dresses the player as the %s", player_is_the_boy ? "boy" : "girl");
            return;
        }
    }
    HM_MM_LOG(MOD_ID, "the wedding script is not as expected; it stays as it is");
}

/*
 * The words of messages 9 and 100 that go by the player's sex, in each language of the disc, as said to a man
 * and as said to a woman: "husband" or "wife", "Mrs." (Azuma, who thinks he is greeting his bride), and the
 * words that agree with the couple ("we're married" is "casados", "mariés" or "sposati" for a man and a woman,
 * "casadas", "mariées" or "sposate" for two women). A woman speaking to a woman says the second word of each
 * pair where the game says the first; a man speaking to a man, the first where the game says the second. A
 * pair may end with the game's code of a line break (01 04 1D) or of "..." (85), as the line has it; a string
 * is cut after such a code, so that C does not read the next letter as part of it.
 */
typedef struct WordPair {
    char language;
    const char* for_a_man;
    const char* for_a_woman;
} WordPair;

static const WordPair spouse_words[] = {
    {'e', "husband", "wife"},
    {'e', "Mr. \x85", "Mrs. \x85"},
    {'s', "mi marido", "mi mujer"},
    {'s', "esposo", "esposa"},
    {'s', "casados", "casadas"},
    {'s', "Sr..", "Sra.."},
    {'f', "mon mari", "ma femme"},
    {'f', "mari\xE9s", "mari\xE9" "es"},
    {'f', "M.\x85", "Mme\x85"},
    {'f', "Sois patient.", "Sois patiente."},
    {'f', "heureux.", "heureuses."},
    {'f', "poulet,", "poulette,"},
    {'g', "meinen Ehemann", "meine Ehefrau"},
    {'g', "als Ehemann", "als Ehefrau"},
    {'g', "meinen\x01\x04\x1D" "Mann", "meine\x01\x04\x1D" "Frau"},
    {'g', "Herr \x85", "Frau \x85"},
    {'i', "mio marito", "mia moglie"},
    {'i', "sposati", "sposate"},
    {'i', "sposi!", "spose!"},
    {'i', "signore\x85", "signora\x85"},
    {'i', "micino", "micina"},
};
#define SPOUSE_WORD_PAIRS (sizeof spouse_words / sizeof spouse_words[0])

/* Replaces every `from` in `text` with `to`; false when the result would not fit. */
static bool replace_words(uint8_t* text, size_t* length, size_t capacity, const char* from, const char* to)
{
    const size_t from_length = strlen(from);
    const size_t to_length = strlen(to);
    size_t at = 0;
    while (at + from_length <= *length) {
        if (memcmp(text + at, from, from_length) != 0) {
            ++at;
            continue;
        }
        if (*length - from_length + to_length > capacity) {
            return false;
        }
        memmove(text + at + to_length, text + at + from_length, *length - at - from_length);
        memcpy(text + at, to, to_length);
        *length = *length - from_length + to_length;
        at += to_length;
    }
    return true;
}

/*
 * Changes the spouse's word for the player in a message of the copy of a character's file that the game has
 * just loaded. The changed message goes at the start of the file's text, over messages that are not shown
 * this time (the game frees the copy as soon as the message is shown), and the table points to it.
 */
static void change_the_spouse_word(CPUState* cpu, uint32_t file)
{
    const uint32_t highest_message = read_half_word(cpu, file + MESSAGE_FILE_HEADER_HIGHEST_MESSAGE);
    if (pending_message_number > highest_message) {
        return;
    }
    const uint32_t table = file + read_word(cpu, file + MESSAGE_FILE_HEADER_TABLE_START);
    const uint32_t text = file + read_word(cpu, file + MESSAGE_FILE_HEADER_TEXT_START);
    const uint32_t table_entry = table + pending_message_number * WORD_BYTES;
    const uint32_t message = text + read_word(cpu, table_entry);

    uint8_t bytes[LONGEST_MESSAGE_BYTES];
    size_t length = 0;
    while (true) {
        if (length + sizeof end_of_a_message > sizeof bytes) {
            return;
        }
        bytes[length] = read_byte(cpu, message + (uint32_t)length);
        ++length;
        if (length >= sizeof end_of_a_message &&
            memcmp(bytes + length - sizeof end_of_a_message, end_of_a_message, sizeof end_of_a_message) == 0) {
            break;
        }
    }
    const size_t original_length = length;
    uint8_t original[LONGEST_MESSAGE_BYTES];
    memcpy(original, bytes, length);
    for (size_t index = 0; index < SPOUSE_WORD_PAIRS; ++index) {
        const WordPair* pair = &spouse_words[index];
        if (pair->language != pending_language_letter) {
            continue;
        }
        const char* from = pending_speaker_is_a_woman ? pair->for_a_man : pair->for_a_woman;
        const char* to = pending_speaker_is_a_woman ? pair->for_a_woman : pair->for_a_man;
        if (!replace_words(bytes, &length, sizeof bytes, from, to)) {
            return;
        }
    }
    if (length == original_length && memcmp(bytes, original, length) == 0) {
        return; /* no word of the pairs is in this message */
    }
    for (size_t index = 0; index < length; ++index) {
        write_byte(cpu, text + (uint32_t)index, bytes[index]);
    }
    write_word(cpu, table_entry, 0);
    HM_MM_LOG(MOD_ID, "the spouse's message %u speaks to the player as %s", (unsigned int)pending_message_number,
              pending_speaker_is_a_woman ? "a woman" : "a man");
}

/* ---- The adoption ---- */

/* Who brings the baby: the Harvest Goddess once she is awake (the event Eve_mega02 has started), else a sprite. */
static AdoptionVisitor visitor_of_the_adoption(CPUState* cpu, uint32_t save_data)
{
    return is_bit_set(cpu, save_data + STARTED_EVENTS_OFFSET, EVENT_THE_GODDESS_WAKES_UP) ? ADOPTION_BY_THE_GODDESS
                                                                                         : ADOPTION_BY_THE_SPRITE;
}

/*
 * Writes the adoption scene over the birth's script that the game has just loaded (every birth script is
 * longer than it), with the spouse's number where the scene names the spouse.
 */
static void write_the_adoption_script(CPUState* cpu, uint32_t file)
{
    const uint32_t save_data = save_data_of(cpu);
    const uint32_t spouse = read_word(cpu, save_data + SPOUSE_OFFSET);
    const AdoptionVisitor visitor = visitor_of_the_adoption(cpu, save_data);
    const AdoptionScript* script = &adoption_scripts[visitor];
    for (uint32_t index = 0; index < script->size; ++index) {
        write_byte(cpu, file + index, script->bytes[index]);
    }
    for (uint32_t index = 0; index < script->spouse_place_count; ++index) {
        write_byte(cpu, file + script->spouse_places[index], (uint8_t)spouse);
    }
    /* The spouse's first conversation after a birth is about having given birth: not this time. */
    const uint32_t spouse_bits = save_data + CHARACTER_BITS_OFFSET + CHARACTER_BITS_BYTES_EACH * spouse;
    write_byte(cpu, spouse_bits, (uint8_t)(read_byte(cpu, spouse_bits) & ~(1u << BIT_TALK_ABOUT_THE_BIRTH)));
    adoption_pending = false;
    HM_MM_LOG(MOD_ID, "%s comes to the spring with a baby",
              visitor == ADOPTION_BY_THE_GODDESS ? "the Harvest Goddess" : "a Harvest Sprite");
}

/*
 * Writes the adoption's lines over the birth's messages that the game has just loaded: one after another from
 * the start of the text, the table pointing to each, and the spouse's number as the speaker of the spouse's.
 */
static void write_the_adoption_lines(CPUState* cpu, uint32_t file)
{
    const uint32_t save_data = save_data_of(cpu);
    const uint32_t spouse = read_word(cpu, save_data + SPOUSE_OFFSET);
    const bool two_women = read_word(cpu, save_data + PLAYER_CHARACTER_OFFSET) == PLAYER_IS_THE_GIRL;
    const AdoptionVisitor visitor = visitor_of_the_adoption(cpu, save_data);
    const AdoptionLines* lines = NULL;
    for (size_t index = 0; index < HM_MM_ARRAY_COUNT(adoption_lines); ++index) {
        const AdoptionLines* candidate = &adoption_lines[index];
        if (candidate->visitor == visitor && candidate->language == pending_language_letter &&
            candidate->two_women == two_women) {
            lines = candidate;
        }
    }
    const uint32_t highest_message = read_half_word(cpu, file + MESSAGE_FILE_HEADER_HIGHEST_MESSAGE);
    if (lines == NULL || highest_message + 1 < ADOPTION_MESSAGE_COUNT) {
        HM_MM_LOG(MOD_ID, "the birth's messages are not as expected; the adoption has no lines");
        return;
    }
    const uint32_t table = file + read_word(cpu, file + MESSAGE_FILE_HEADER_TABLE_START);
    const uint32_t text = file + read_word(cpu, file + MESSAGE_FILE_HEADER_TEXT_START);
    uint32_t message = 0;
    bool starts_a_message = true;
    for (uint32_t index = 0; index < lines->size; ++index) {
        uint8_t byte = lines->bytes[index];
        if (starts_a_message) {
            write_word(cpu, table + message * WORD_BYTES, index);
            ++message;
        }
        const bool is_the_spouse_speaking =
            index > 0 && lines->bytes[index - 1] == SPEAKER_CODE && byte == ADOPTION_SPOUSE_PLACEHOLDER;
        if (is_the_spouse_speaking) {
            byte = (uint8_t)spouse;
        }
        write_byte(cpu, text + index, byte);
        starts_a_message = index + 1 >= sizeof end_of_a_message &&
                           memcmp(lines->bytes + index + 1 - sizeof end_of_a_message, end_of_a_message,
                                  sizeof end_of_a_message) == 0;
    }
    HM_MM_LOG(MOD_ID, "gave the adoption %u lines (%c)", (unsigned int)message, pending_language_letter);
}

static void after_a_file_is_loaded(CPUState* cpu)
{
    const PendingFile kind = pending_file;
    pending_file = NO_PENDING_FILE;
    const uint32_t file = cpu->gpr[3];
    if (file == 0) {
        return;
    }
    if (kind == WEDDING_SCRIPT) {
        dress_the_player_for_the_wedding(cpu, file);
    } else if (kind == SPOUSE_MESSAGES) {
        message_to_change = 0;
        change_the_spouse_word(cpu, file);
    } else if (kind == ADOPTION_SCRIPT) {
        write_the_adoption_script(cpu, file);
    } else if (kind == ADOPTION_MESSAGES) {
        write_the_adoption_lines(cpu, file);
    }
}

/* Before a character's message is shown: whether it is the spouse's 9 or 100 for a couple of the same sex. */
static void before_a_character_message_is_shown(CPUState* cpu)
{
    message_to_change = 0;
    const uint32_t save_data = save_data_of(cpu);
    if (save_data == NO_SAVE_DATA) {
        return;
    }
    const uint32_t character = moderngekko_mod_arg_u32(cpu, 1);
    const uint32_t message = moderngekko_mod_arg_u32(cpu, 2);
    const bool first_words = message == MESSAGE_WHAT_TO_CALL_YOU || message == MESSAGE_ANSWER_TO_THE_NAME;
    if (!first_words || read_word(cpu, save_data + SPOUSE_OFFSET) != character ||
        !is_of_the_players_sex(cpu, save_data, character)) {
        return;
    }
    message_to_change = message;
    message_speaker_is_a_woman = player_character_of_the_partner(character) == PLAYER_IS_THE_BOY;
}

static void after_a_character_message_is_shown(CPUState* cpu)
{
    (void)cpu;
    message_to_change = 0;
}

static const ModernGekkoModHook hooks[] = {
    RECOMP_HOOK(ASK_IF_A_PARTNER_ADDRESS, ask_about_a_partner_of_the_player),
    RECOMP_HOOK(CLOSEST_PARTNER_FUNCTION, before_closest_partner),
    RECOMP_HOOK_RETURN(CLOSEST_PARTNER_FUNCTION, after_closest_partner),
    RECOMP_HOOK_RETURN(SOMEONE_LOVES_THE_PLAYER_FUNCTION, after_someone_loves_the_player),
    RECOMP_HOOK(LOVE_SCENE_BRANCH_ADDRESS, choose_the_chain_of_love_scenes),
    RECOMP_HOOK(WEDDING_BRANCH_ADDRESS, choose_the_chain_of_the_fiance),
    RECOMP_HOOK(EVENT_START_BRANCH_ADDRESS, keep_back_an_event),
    RECOMP_HOOK(LOAD_FILE_FUNCTION, before_a_file_is_loaded),
    RECOMP_HOOK_RETURN(LOAD_FILE_FUNCTION, after_a_file_is_loaded),
    RECOMP_HOOK(SHOW_CHARACTER_MESSAGE_FUNCTION, before_a_character_message_is_shown),
    RECOMP_HOOK_RETURN(SHOW_CHARACTER_MESSAGE_FUNCTION, after_a_character_message_is_shown),
};

static const ModernGekkoModDesc descriptor = {
    .abi_version = MODERNGEKKO_MOD_ABI_VERSION,
    .cpu_abi_version = MODERNGEKKO_CPU_ABI_VERSION,
    .cpu_state_size = sizeof(CPUState),
    .game_id = HM_MM_GAME_ID,
    .id = MOD_ID,
    .version = "0.2.1",
    .display_name = "Marry anyone",
    .hooks = hooks,
    .num_hooks = HM_MM_ARRAY_COUNT(hooks),
};

MODERNGEKKO_MOD_EXPORT const ModernGekkoModDesc* moderngekko_get_mod(void)
{
    return &descriptor;
}
