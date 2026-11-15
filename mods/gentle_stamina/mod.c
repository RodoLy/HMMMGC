/*
 * gentle_stamina: tools, fishing and the rest of the work spend less stamina.
 * gentle_stamina: las herramientas, la pesca y el resto del trabajo gastan menos estamina.
 *
 * How the game spends stamina (found in its code):
 *
 *   - The stamina of each player is a {value, minimum, maximum} triple in the save data: the value at
 *     save + 0x4004 (12 bytes for each player) and the most it can be at save + 0x3FC8. The bar under the
 *     character's face shows one over the other (fn_800803A8 reads both).
 *   - Every change of stamina goes through fn_800A4D8C(save, player, change). It multiplies the change by
 *     1, 2 or 4 by how tired the player is (save + 0x4040: under 50, under 80, or more), adds it and keeps
 *     the result between the triple's limits. The game calls it with -1 to -5 when a tool is used, when
 *     the player fishes, runs a long way, and so on (fn_8001E44C, fn_80049D28, fn_800CF950...), and with
 *     a positive change when something gives stamina back.
 *
 * This mod hooks the entry of fn_800A4D8C and makes a negative change smaller: the chosen share of it
 * (HM_MM_STAMINA_COST_PERCENT, from MINIMUM_COST_PERCENT to MAXIMUM_COST_PERCENT; 50 is half). Stamina is
 * spent in whole points, so what is left of a point is kept for the next time, for each player: at 50 %
 * two hoe strokes of 1 point cost 1 point in all. Positive changes, the tiredness multiplier and the
 * limits stay as they are. At 100 % the mod does nothing.
 *
 * The share that is kept between calls assumes that the hook runs once per call, as it does in the native
 * runtime, where it is written into the translated code (tools/insert_mod_hooks.py).
 *
 * Cómo gasta estamina el juego: todo cambio pasa por fn_800A4D8C(partida, jugador, cambio), que lo
 * multiplica por 1, 2 o 4 según el cansancio, lo suma a la estamina (partida + 0x4004) y la deja entre sus
 * límites. Las herramientas, la pesca y correr llaman con -1 a -5. Este mod hace más chico un cambio
 * negativo: la parte elegida (HM_MM_STAMINA_COST_PERCENT; 50 es la mitad), y guarda lo que sobra de un
 * punto para la siguiente vez. Lo que devuelve estamina no cambia.
 */

#include <stdlib.h>

#include "hm_mm_mod.h"

#define MOD_ID "gentle_stamina"

#define COST_PERCENT_VARIABLE "HM_MM_STAMINA_COST_PERCENT"
#define DEFAULT_COST_PERCENT 50
#define MINIMUM_COST_PERCENT 10
#define MAXIMUM_COST_PERCENT 100
#define PERCENT_OF_THE_ORIGINAL_COST 100

/* fn_800A4D8C(save, player, change): every change of a player's stamina. */
#define STAMINA_CHANGE_FUNCTION DOLRECOMP_SYMBOL_fn_800A4D8C
#define PLAYER_REGISTER 4
#define CHANGE_REGISTER 5
/* The game keeps the stamina of four players; fn_800A4D8C does nothing for a higher number. */
#define PLAYERS 4

static int32_t cost_percent = PERCENT_OF_THE_ORIGINAL_COST;
/* For each player, the hundredths of a point that have been spared but not yet taken off. */
static int32_t hundredths_owed[PLAYERS];

static void spend_less_stamina(CPUState* cpu)
{
    const uint32_t player = cpu->gpr[PLAYER_REGISTER];
    const int32_t change = (int32_t)cpu->gpr[CHANGE_REGISTER];
    if (cost_percent >= PERCENT_OF_THE_ORIGINAL_COST || change >= 0 || player >= PLAYERS) {
        return;
    }
    hundredths_owed[player] += -change * cost_percent;
    const int32_t points = hundredths_owed[player] / PERCENT_OF_THE_ORIGINAL_COST;
    hundredths_owed[player] -= points * PERCENT_OF_THE_ORIGINAL_COST;
    cpu->gpr[CHANGE_REGISTER] = (uint32_t)-points;
}

/*
 * The share that the player asked for in the environment variable, or DEFAULT_COST_PERCENT when the variable
 * is not set, is not a whole number, or is outside the limits. It is read as a whole number so that the
 * language and region settings of the computer cannot change how it is understood.
 */
static long read_requested_cost_percent(void)
{
    const char* text = getenv(COST_PERCENT_VARIABLE);
    if (text == NULL || text[0] == '\0') {
        return DEFAULT_COST_PERCENT;
    }
    char* end_of_number = NULL;
    const long percent = strtol(text, &end_of_number, 10);
    if (*end_of_number != '\0' || percent < MINIMUM_COST_PERCENT || percent > MAXIMUM_COST_PERCENT) {
        HM_MM_LOG(MOD_ID, "%s=\"%s\" is not a whole number from %d to %d: using %d", COST_PERCENT_VARIABLE, text,
                  MINIMUM_COST_PERCENT, MAXIMUM_COST_PERCENT, DEFAULT_COST_PERCENT);
        return DEFAULT_COST_PERCENT;
    }
    return percent;
}

/* Called when the mod is loaded, before the game runs. */
static void choose_the_cost(const ModernGekkoModHostApi* host_api)
{
    (void)host_api; /* The mod does not call back into the runner. */
    cost_percent = (int32_t)read_requested_cost_percent();
    for (int player = 0; player < PLAYERS; player++) {
        hundredths_owed[player] = 0;
    }
    HM_MM_LOG(MOD_ID, "work costs %d%% of the original stamina", (int)cost_percent);
}

static const ModernGekkoModHook hooks[] = {
    RECOMP_HOOK(STAMINA_CHANGE_FUNCTION, spend_less_stamina),
};

static const ModernGekkoModDesc descriptor = {
    .abi_version = MODERNGEKKO_MOD_ABI_VERSION,
    .cpu_abi_version = MODERNGEKKO_CPU_ABI_VERSION,
    .cpu_state_size = sizeof(CPUState),
    .game_id = HM_MM_GAME_ID,
    .id = MOD_ID,
    .version = "0.1.0",
    .display_name = "Gentler stamina",
    .hooks = hooks,
    .num_hooks = HM_MM_ARRAY_COUNT(hooks),
    .on_load = choose_the_cost,
};

MODERNGEKKO_MOD_EXPORT const ModernGekkoModDesc* moderngekko_get_mod(void)
{
    return &descriptor;
}
