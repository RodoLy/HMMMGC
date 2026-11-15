/*
 * faster_tools: the tools gain experience faster, so they reach their next level sooner.
 * faster_tools: las herramientas ganan experiencia más rápido, así que llegan antes a su siguiente nivel.
 *
 * How the game keeps the tools' experience (found in its code):
 *
 *   - Each tool has a counter in the save data, a {value, minimum, maximum} triple of 12 bytes with a
 *     maximum of 10000: the first at save + 0xCD1C (FIRST_TOOL_COUNTER), one after the other. The game sets
 *     them all to 0 when a new game starts (fn_800A4EC8).
 *   - Using a tool adds 1 to its counter with fn_800A78D0(counter, amount), which adds and keeps the result
 *     within the triple's limits; r3 is the counter and r4 the amount. Other counters of the game are raised
 *     with the same function, so the mod looks at r3 and only acts on the tools' counters.
 *   - The translator copies fn_800A78D0, a small function, into its callers. tools/insert_mod_hooks.py writes
 *     the hook at the entry of each of those copies too, so it runs for every call.
 *   - One tool's counter (the sixth, at save + 0xCD58) is raised another way, in fn_8004E244: the game reads
 *     it and sets it one higher with fn_800A7950. The mod does not change that one yet.
 *
 * At those calls the mod makes the amount the chosen share of it (HM_MM_TOOL_EXPERIENCE_PERCENT, from
 * MINIMUM_EXPERIENCE_PERCENT to MAXIMUM_EXPERIENCE_PERCENT; 200 is twice as fast). Experience is counted in
 * whole points, so what is left of a point is kept for the next use of the same tool. At 100 % the mod does
 * nothing.
 *
 * What is kept between calls assumes that a hook runs once per call, as it does in the native runtime,
 * where it is written into the translated code (tools/insert_mod_hooks.py).
 *
 * Cómo guarda el juego la experiencia de las herramientas: cada una tiene un contador en la partida (de
 * partida + 0xCD1C en adelante, 12 bytes cada uno, hasta 10000), y usar la herramienta le suma con
 * fn_800A78D0(contador, cantidad); el mod engancha esa función (también sus copias dentro de otras) y, si r3
 * es el contador de una herramienta, multiplica la cantidad (r4) por lo que eligió el jugador
 * (HM_MM_TOOL_EXPERIENCE_PERCENT; 200 es el doble), guardando lo que sobra de un punto para la siguiente vez.
 */

#include <stdlib.h>

#include "hm_mm_mod.h"

#define MOD_ID "faster_tools"

#define EXPERIENCE_PERCENT_VARIABLE "HM_MM_TOOL_EXPERIENCE_PERCENT"
#define DEFAULT_EXPERIENCE_PERCENT 200
#define MINIMUM_EXPERIENCE_PERCENT 100
#define MAXIMUM_EXPERIENCE_PERCENT 400
#define PERCENT_OF_THE_ORIGINAL_EXPERIENCE 100

#define WORD_BYTES 4
#define FIRST_TOOL_COUNTER 0xCD1Cu
#define COUNTER_BYTES 12u
#define TOOL_COUNTERS 9u

#define SAVE_DATA_POINTER_ADDRESS 0x80321F48u /* the game's pointer to the save data */
#define TOOL_COUNTERS 9u

/* fn_800A78D0(counter, amount): adds to one of the game's counters. */
#define COUNTER_ADD_FUNCTION DOLRECOMP_SYMBOL_fn_800A78D0
#define COUNTER_REGISTER 3
#define AMOUNT_REGISTER 4

static int32_t experience_percent = PERCENT_OF_THE_ORIGINAL_EXPERIENCE;
/* For each tool, the hundredths of a point that have been earned but not yet added. */
static int32_t hundredths_earned[TOOL_COUNTERS];

static uint32_t read_word(CPUState* cpu, uint32_t address)
{
    return (uint32_t)moderngekko_mod_read(cpu, address, WORD_BYTES);
}

/* The experience a tool gets for `amount` points of the original game, counting what was left before. */
static int32_t more_experience(uint32_t tool, int32_t amount)
{
    hundredths_earned[tool] += amount * experience_percent;
    const int32_t points = hundredths_earned[tool] / PERCENT_OF_THE_ORIGINAL_EXPERIENCE;
    hundredths_earned[tool] -= points * PERCENT_OF_THE_ORIGINAL_EXPERIENCE;
    return points;
}

/* Which tool's counter `address` is, or TOOL_COUNTERS when it is not one of them. */
static uint32_t tool_of_the_counter(CPUState* cpu, uint32_t address)
{
    const uint32_t first = read_word(cpu, SAVE_DATA_POINTER_ADDRESS) + FIRST_TOOL_COUNTER;
    if (address < first || (address - first) % COUNTER_BYTES != 0) {
        return TOOL_COUNTERS;
    }
    const uint32_t tool = (address - first) / COUNTER_BYTES;
    return tool < TOOL_COUNTERS ? tool : TOOL_COUNTERS;
}

/* At the entry of fn_800A78D0(counter, amount), or of a copy of it. */
static void give_more_experience(CPUState* cpu)
{
    const int32_t amount = (int32_t)cpu->gpr[AMOUNT_REGISTER];
    if (experience_percent <= PERCENT_OF_THE_ORIGINAL_EXPERIENCE || amount <= 0) {
        return;
    }
    const uint32_t tool = tool_of_the_counter(cpu, cpu->gpr[COUNTER_REGISTER]);
    if (tool == TOOL_COUNTERS) {
        return;
    }
    cpu->gpr[AMOUNT_REGISTER] = (uint32_t)more_experience(tool, amount);
}

/*
 * The share that the player asked for in the environment variable, or DEFAULT_EXPERIENCE_PERCENT when the
 * variable is not set, is not a whole number, or is outside the limits. It is read as a whole number so that
 * the language and region settings of the computer cannot change how it is understood.
 */
static long read_requested_experience_percent(void)
{
    const char* text = getenv(EXPERIENCE_PERCENT_VARIABLE);
    if (text == NULL || text[0] == '\0') {
        return DEFAULT_EXPERIENCE_PERCENT;
    }
    char* end_of_number = NULL;
    const long percent = strtol(text, &end_of_number, 10);
    if (*end_of_number != '\0' || percent < MINIMUM_EXPERIENCE_PERCENT || percent > MAXIMUM_EXPERIENCE_PERCENT) {
        HM_MM_LOG(MOD_ID, "%s=\"%s\" is not a whole number from %d to %d: using %d", EXPERIENCE_PERCENT_VARIABLE,
                  text, MINIMUM_EXPERIENCE_PERCENT, MAXIMUM_EXPERIENCE_PERCENT, DEFAULT_EXPERIENCE_PERCENT);
        return DEFAULT_EXPERIENCE_PERCENT;
    }
    return percent;
}

/* Called when the mod is loaded, before the game runs. */
static void choose_the_experience(const ModernGekkoModHostApi* host_api)
{
    (void)host_api; /* The mod does not call back into the runner. */
    experience_percent = (int32_t)read_requested_experience_percent();
    for (uint32_t tool = 0; tool < TOOL_COUNTERS; tool++) {
        hundredths_earned[tool] = 0;
    }
    HM_MM_LOG(MOD_ID, "the tools gain %d%% of the original experience", (int)experience_percent);
}

static const ModernGekkoModHook hooks[] = {
    RECOMP_HOOK(COUNTER_ADD_FUNCTION, give_more_experience),
};

static const ModernGekkoModDesc descriptor = {
    .abi_version = MODERNGEKKO_MOD_ABI_VERSION,
    .cpu_abi_version = MODERNGEKKO_CPU_ABI_VERSION,
    .cpu_state_size = sizeof(CPUState),
    .game_id = HM_MM_GAME_ID,
    .id = MOD_ID,
    .version = "0.1.0",
    .display_name = "Faster tools",
    .hooks = hooks,
    .num_hooks = HM_MM_ARRAY_COUNT(hooks),
    .on_load = choose_the_experience,
};

MODERNGEKKO_MOD_EXPORT const ModernGekkoModDesc* moderngekko_get_mod(void)
{
    return &descriptor;
}
