/*
 * longer_days: the days last longer, so there is more time to farm, fish and visit before night.
 * longer_days: los días duran más, para tener más tiempo de cultivar, pescar y visitar antes de la noche.
 *
 * How the game moves its clock (found in its code):
 *
 *   - The clock is three counters in the save data, each a {value, minimum, maximum} triple: the hour at
 *     save + 0x2258, the minute at save + 0x2264 and, at save + 0x2270, the part of a minute that has gone by.
 *   - Every frame of free play, fn_80038088 (the clock's update, from the table at 0x80215E60) calls
 *     fn_80037EAC with f1 = 100 * the frame's time. fn_80037EAC turns f1 into a whole number (fctiwz, which
 *     cuts the decimals off) and adds it to the part of a minute; when that counter goes past its maximum,
 *     the minute goes on by one, and with the minutes the hour, the day and the rest of the game's time.
 *   - Sleeping, events and the start of a day set the clock directly with other functions, not through
 *     fn_80037EAC, so they are not changed.
 *
 * This mod hooks the entry of fn_80037EAC and gives it less time: what the game would add, divided by the
 * length of the day the player chose (200 % is half as much each frame, so a day lasts twice as long). The
 * game only adds whole numbers, so what is left over is kept for the next frames, and over a day the clock
 * goes exactly as much slower as asked. At 100 % the mod does nothing.
 *
 * ModernGekko can run a hook more than once for the same call (see mods/README.md). To be safe, the mod
 * gives fn_80037EAC a whole number plus a quarter (the game cuts the quarter off) and leaves alone a value
 * that already ends in exactly a quarter: 100 * the frame's time never does at the frame times of the game.
 *
 * How long the day lasts is chosen with the environment variable HM_MM_DAY_LENGTH_PERCENT, a whole number
 * from MINIMUM_DAY_LENGTH_PERCENT to MAXIMUM_DAY_LENGTH_PERCENT: 100 is the original game, 150 a day half as
 * long again. The launchers set it from the mod's setting (mod.json, "dayLengthPercent").
 *
 * Cómo avanza el reloj del juego: cada cuadro, fn_80038088 llama a fn_80037EAC con f1 = 100 por el tiempo
 * del cuadro; fn_80037EAC le quita los decimales y lo suma a la parte de minuto (save + 0x2270), y de ahí
 * avanzan el minuto (0x2264) y la hora (0x2258). Este mod le da menos tiempo: lo que el juego sumaría,
 * dividido entre la duración del día que eligió el jugador, y guarda lo que sobra para los cuadros
 * siguientes. Dormir, los eventos y el inicio del día ponen la hora con otras funciones y no cambian.
 */

#include <math.h>
#include <stdlib.h>

#include "hm_mm_mod.h"

#define MOD_ID "longer_days"

#define DAY_LENGTH_PERCENT_VARIABLE "HM_MM_DAY_LENGTH_PERCENT"
#define DEFAULT_DAY_LENGTH_PERCENT 150
#define MINIMUM_DAY_LENGTH_PERCENT 100
#define MAXIMUM_DAY_LENGTH_PERCENT 400
#define PERCENT_OF_THE_ORIGINAL_DAY 100.0

/* fn_80037EAC(clock, f1 = the time to add): the clock's tick. */
#define CLOCK_TICK_FUNCTION DOLRECOMP_SYMBOL_fn_80037EAC
#define TIME_TO_ADD_REGISTER 1

/* The mark of a value this mod has already given: a whole number plus this, which the game cuts off. */
#define ALREADY_GIVEN_MARK 0.25

/* 1 / the chosen length, and the part of the game's time that has not been given yet. */
static double share_of_the_time = 1.0;
static double time_left_over = 0.0;

static void slow_the_clock_down(CPUState* cpu)
{
    if (share_of_the_time >= 1.0) {
        return;
    }
    const double asked = cpu->fpr[TIME_TO_ADD_REGISTER];
    if (asked - floor(asked) == ALREADY_GIVEN_MARK) {
        return;
    }
    /* What the original game adds this frame: the whole part, as fctiwz takes it. */
    const double the_game_would_add = trunc(asked);
    if (the_game_would_add <= 0.0) {
        return;
    }
    const double this_frame = the_game_would_add * share_of_the_time + time_left_over;
    const double given = floor(this_frame);
    time_left_over = this_frame - given;
    cpu->fpr[TIME_TO_ADD_REGISTER] = given + ALREADY_GIVEN_MARK;
}

/*
 * The length that the player asked for in the environment variable, or DEFAULT_DAY_LENGTH_PERCENT when the
 * variable is not set, is not a whole number, or is outside the limits. It is read as a whole number so that
 * the language and region settings of the computer cannot change how it is understood.
 */
static long read_requested_day_length_percent(void)
{
    const char* text = getenv(DAY_LENGTH_PERCENT_VARIABLE);
    if (text == NULL || text[0] == '\0') {
        return DEFAULT_DAY_LENGTH_PERCENT;
    }
    char* end_of_number = NULL;
    const long percent = strtol(text, &end_of_number, 10);
    if (*end_of_number != '\0' || percent < MINIMUM_DAY_LENGTH_PERCENT || percent > MAXIMUM_DAY_LENGTH_PERCENT) {
        HM_MM_LOG(MOD_ID, "%s=\"%s\" is not a whole number from %d to %d: using %d", DAY_LENGTH_PERCENT_VARIABLE,
                  text, MINIMUM_DAY_LENGTH_PERCENT, MAXIMUM_DAY_LENGTH_PERCENT, DEFAULT_DAY_LENGTH_PERCENT);
        return DEFAULT_DAY_LENGTH_PERCENT;
    }
    return percent;
}

/* Called when the mod is loaded, before the game runs. */
static void choose_the_length_of_the_day(const ModernGekkoModHostApi* host_api)
{
    (void)host_api; /* The mod does not call back into the runner. */
    const long percent = read_requested_day_length_percent();
    share_of_the_time = PERCENT_OF_THE_ORIGINAL_DAY / (double)percent;
    time_left_over = 0.0;
    HM_MM_LOG(MOD_ID, "a day lasts %ld%% of the original day", percent);
}

static const ModernGekkoModHook hooks[] = {
    RECOMP_HOOK(CLOCK_TICK_FUNCTION, slow_the_clock_down),
};

static const ModernGekkoModDesc descriptor = {
    .abi_version = MODERNGEKKO_MOD_ABI_VERSION,
    .cpu_abi_version = MODERNGEKKO_CPU_ABI_VERSION,
    .cpu_state_size = sizeof(CPUState),
    .game_id = HM_MM_GAME_ID,
    .id = MOD_ID,
    .version = "0.1.0",
    .display_name = "Longer days",
    .hooks = hooks,
    .num_hooks = HM_MM_ARRAY_COUNT(hooks),
    .on_load = choose_the_length_of_the_day,
};

MODERNGEKKO_MOD_EXPORT const ModernGekkoModDesc* moderngekko_get_mod(void)
{
    return &descriptor;
}
