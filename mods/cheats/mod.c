/*
 * cheats: the cheats that the player turns on in the launcher's options (settings.json, "cheats").
 * cheats: los trucos que el jugador enciende en las opciones del launcher (settings.json, "cheats").
 *
 * The runtime puts the names of the cheats that are on in the environment variable HM_MM_CHEATS, separated by
 * commas (shared/settings.h, CHEAT_NAMES), and loads this mod whenever one is on. Each cheat uses what the
 * game's code does (found in it, as the other mods say):
 *
 *   - infiniteStamina: every change of stamina goes through fn_800A4D8C(save, player, change) (see
 *     gentle_stamina). A change that takes stamina away becomes 0.
 *   - infiniteMoney: the money is a {value, minimum, maximum} triple at save + 0x245C (the load screen shows
 *     the same number). In free play, every frame, the mod sets it to MONEY_KEPT when it is lower.
 *   - stopTime: the clock's tick, fn_80037EAC, adds the whole part of f1 every frame of free play (see
 *     longer_days). The mod gives it 0 (with the same mark as longer_days, so that the two agree).
 *   - maxFriendship: the friendship with each character is a triple at save + 0x6628 + 12 * character. In free
 *     play, every frame, the mod sets each one to its maximum. The game starts the scenes that come with
 *     friendship (love scenes, rivals) as it would with the friendship earned by playing.
 *
 * The mod sets the money and the friendships in the player's update, fn_80049D28, and only in free play (the
 * game's main state 0, at 0x80321D18), as player_speed does: never in the middle of an event.
 *
 * El runtime pone en HM_MM_CHEATS los trucos encendidos y carga este mod si hay alguno. Estamina infinita:
 * un cambio que quita estamina se vuelve 0. Dinero: en juego libre se mantiene en MONEY_KEPT. Detener el
 * tiempo: el reloj suma 0. Amistad al máximo: en juego libre, cada amistad se pone en su máximo.
 */

#include <stdbool.h>
#include <stdlib.h>
#include <string.h>

#include "hm_mm_mod.h"

#define MOD_ID "cheats"
#define CHEATS_VARIABLE "HM_MM_CHEATS"

#define WORD_BYTES 4
#define SAVE_DATA_POINTER_ADDRESS 0x80321F48u
#define MAIN_STATE_ADDRESS 0x80321D18u
#define MAIN_STATE_FREE_PLAY 0u

/* Where the game keeps what the cheats change, in the save data: {value, minimum, maximum} triples. */
#define MONEY 0x245Cu
#define MONEY_KEPT 999999u
#define FRIENDSHIP 0x6628u
#define TRIPLE_BYTES 12u
#define TRIPLE_MAXIMUM 8u
/* The characters with a friendship: the table of the characters' states (save + 0x45EC to 0x4B14) has 110. */
#define CHARACTERS 110u

#define STAMINA_CHANGE_FUNCTION DOLRECOMP_SYMBOL_fn_800A4D8C
#define STAMINA_CHANGE_REGISTER 5
#define CLOCK_TICK_FUNCTION DOLRECOMP_SYMBOL_fn_80037EAC
#define TIME_TO_ADD_REGISTER 1
#define NO_TIME_MARK 0.25 /* a whole 0 with longer_days's mark: the game cuts the quarter off */
#define PLAYER_UPDATE_FUNCTION DOLRECOMP_SYMBOL_fn_80049D28

typedef struct Cheats {
    bool infinite_stamina;
    bool infinite_money;
    bool stop_time;
    bool max_friendship;
} Cheats;

static Cheats cheats;

static uint32_t read_word(CPUState* cpu, uint32_t address)
{
    return (uint32_t)moderngekko_mod_read(cpu, address, WORD_BYTES);
}

static void write_word(CPUState* cpu, uint32_t address, uint32_t value)
{
    moderngekko_mod_write(cpu, address, value, WORD_BYTES);
}

static void keep_the_stamina(CPUState* cpu)
{
    if (cheats.infinite_stamina && (int32_t)cpu->gpr[STAMINA_CHANGE_REGISTER] < 0) {
        cpu->gpr[STAMINA_CHANGE_REGISTER] = 0;
    }
}

static void stop_the_clock(CPUState* cpu)
{
    if (cheats.stop_time) {
        cpu->fpr[TIME_TO_ADD_REGISTER] = NO_TIME_MARK;
    }
}

/* Every frame of free play: the money and the friendships. */
static void keep_the_money_and_the_friendships(CPUState* cpu)
{
    if (!cheats.infinite_money && !cheats.max_friendship) {
        return;
    }
    if (read_word(cpu, MAIN_STATE_ADDRESS) != MAIN_STATE_FREE_PLAY) {
        return;
    }
    const uint32_t save = read_word(cpu, SAVE_DATA_POINTER_ADDRESS);
    if (save == 0) {
        return;
    }
    if (cheats.infinite_money && read_word(cpu, save + MONEY) < MONEY_KEPT) {
        write_word(cpu, save + MONEY, MONEY_KEPT);
    }
    if (cheats.max_friendship) {
        for (uint32_t character = 0; character < CHARACTERS; character++) {
            const uint32_t triple = save + FRIENDSHIP + character * TRIPLE_BYTES;
            const uint32_t maximum = read_word(cpu, triple + TRIPLE_MAXIMUM);
            if (read_word(cpu, triple) != maximum) {
                write_word(cpu, triple, maximum);
            }
        }
    }
}

/* Whether `name` is one of the comma-separated names in `list`. */
static bool is_in_the_list(const char* list, const char* name)
{
    const size_t length = strlen(name);
    for (const char* start = list; *start != '\0';) {
        const char* end = strchr(start, ',');
        const size_t size = end != NULL ? (size_t)(end - start) : strlen(start);
        if (size == length && strncmp(start, name, length) == 0) {
            return true;
        }
        if (end == NULL) {
            break;
        }
        start = end + 1;
    }
    return false;
}

/* Called when the mod is loaded, before the game runs. */
static void read_the_cheats(const ModernGekkoModHostApi* host_api)
{
    (void)host_api; /* The mod does not call back into the runner. */
    const char* list = getenv(CHEATS_VARIABLE);
    if (list == NULL) {
        list = "";
    }
    cheats.infinite_stamina = is_in_the_list(list, "infiniteStamina");
    cheats.infinite_money = is_in_the_list(list, "infiniteMoney");
    cheats.stop_time = is_in_the_list(list, "stopTime");
    cheats.max_friendship = is_in_the_list(list, "maxFriendship");
    HM_MM_LOG(MOD_ID, "cheats on: %s", list[0] != '\0' ? list : "none");
}

static const ModernGekkoModHook hooks[] = {
    RECOMP_HOOK(STAMINA_CHANGE_FUNCTION, keep_the_stamina),
    RECOMP_HOOK(CLOCK_TICK_FUNCTION, stop_the_clock),
    RECOMP_HOOK(PLAYER_UPDATE_FUNCTION, keep_the_money_and_the_friendships),
};

static const ModernGekkoModDesc descriptor = {
    .abi_version = MODERNGEKKO_MOD_ABI_VERSION,
    .cpu_abi_version = MODERNGEKKO_CPU_ABI_VERSION,
    .cpu_state_size = sizeof(CPUState),
    .game_id = HM_MM_GAME_ID,
    .id = MOD_ID,
    .version = "0.1.0",
    .display_name = "Cheats",
    .hooks = hooks,
    .num_hooks = HM_MM_ARRAY_COUNT(hooks),
    .on_load = read_the_cheats,
};

MODERNGEKKO_MOD_EXPORT const ModernGekkoModDesc* moderngekko_get_mod(void)
{
    return &descriptor;
}
