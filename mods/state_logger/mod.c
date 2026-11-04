/*
 * state_logger: prints the states the game's state machine goes through.
 * state_logger: imprime los estados por los que pasa la máquina de estados del juego.
 *
 * The game's top-level flow (title screen, a new day, story events, free
 * play) is driven by a state machine, fn_80034868. Its current state is the
 * word at 0x80321D18. Other code asks for the next state with
 * fn_8003468C(state), and one state, 0x28, calls the field check
 * (fn_80166888), the routine that decides whether a story event starts.
 * Playing with this mod installed shows when each of those happens:
 *
 *   [state_logger] state 0x00000026 -> 0x00000025
 *   [state_logger] field check entered, current state 0x00000028
 *
 * docs/VALIDATION.md and docs/SCRIPTS.md use it to show that the field check
 * runs when a new day starts, and not while the character walks.
 *
 * A state change is printed one frame after the state machine enters the new
 * state, because the mod looks at the state before the machine runs. So the
 * field check's line can come before the line that announces state 0x28.
 *
 * It changes nothing in the game: a hook runs before the function it watches,
 * and the runtime discards any change the hook makes to the CPU registers.
 *
 * El flujo principal del juego (pantalla de título, un día nuevo, eventos de
 * la historia, juego libre) lo lleva una máquina de estados, fn_80034868. Su
 * estado actual es la palabra en 0x80321D18. Otro código pide el siguiente
 * estado con fn_8003468C(estado), y un estado, el 0x28, llama a la revisión
 * del campo (fn_80166888), la rutina que decide si arranca un evento de la
 * historia. Jugar con este mod instalado muestra cuándo ocurre cada cosa.
 *
 * Un cambio de estado se imprime un cuadro después de que la máquina entra al
 * nuevo estado, porque el mod mira el estado antes de que la máquina corra. Por
 * eso la línea de la revisión del campo puede salir antes que la que anuncia el
 * estado 0x28.
 *
 * Los hooks pueden repetirse en una sola llamada (ver mods/README.md), así que
 * una línea puede salir más de una vez. Lo que sí es fiable es el orden.
 */

#include "hm_mm_mod.h"

#define MOD_ID "state_logger"

/* The state machine's current state, a 32-bit word in the game's memory. */
#define CURRENT_STATE_ADDRESS 0x80321D18u
#define STATE_WORD_BYTES 4

/* Value of previous_state before the first state has been seen. */
#define NO_STATE_SEEN_YET 0xFFFFFFFFu

static uint32_t previous_state = NO_STATE_SEEN_YET;

static uint32_t read_current_state(CPUState* cpu)
{
    return (uint32_t)moderngekko_mod_read(cpu, CURRENT_STATE_ADDRESS, STATE_WORD_BYTES);
}

/*
 * Runs once per frame, just before the state machine. The state machine
 * (fn_80034868) has a single caller, fn_80036F40, and that caller sits in the
 * same 16 KB block of code. ModernGekko only notices a hook when control
 * arrives at the hooked address from outside the block, so the hook goes on
 * the caller. At this point the state is the one the previous frame ended in.
 */
static void log_state_change(CPUState* cpu)
{
    const uint32_t state = read_current_state(cpu);
    if (state == previous_state) {
        return;
    }
    HM_MM_LOG(MOD_ID, "state 0x%08X -> 0x%08X", (unsigned int)previous_state, (unsigned int)state);
    previous_state = state;
}

/* fn_8003468C(state): some code of the game asks for a state to come next. */
static void log_state_request(CPUState* cpu)
{
    HM_MM_LOG(MOD_ID, "state 0x%02X requested from 0x%08X",
              (unsigned int)moderngekko_mod_arg_u32(cpu, 0), (unsigned int)cpu->lr);
}

/* fn_80166888(field): the field check, called by state 0x28. */
static void log_field_check(CPUState* cpu)
{
    HM_MM_LOG(MOD_ID, "field check entered, current state 0x%08X", (unsigned int)read_current_state(cpu));
}

static const ModernGekkoModHook hooks[] = {
    RECOMP_HOOK(DOLRECOMP_SYMBOL_fn_80036F40, log_state_change),
    RECOMP_HOOK(DOLRECOMP_SYMBOL_fn_8003468C, log_state_request),
    RECOMP_HOOK(DOLRECOMP_SYMBOL_fn_80166888, log_field_check),
};

static const ModernGekkoModDesc descriptor = {
    .abi_version = MODERNGEKKO_MOD_ABI_VERSION,
    .cpu_abi_version = MODERNGEKKO_CPU_ABI_VERSION,
    .cpu_state_size = sizeof(CPUState),
    .game_id = HM_MM_GAME_ID,
    .id = MOD_ID,
    .version = "1.0.0",
    .display_name = "State logger",
    .hooks = hooks,
    .num_hooks = HM_MM_ARRAY_COUNT(hooks),
};

MODERNGEKKO_MOD_EXPORT const ModernGekkoModDesc* moderngekko_get_mod(void)
{
    return &descriptor;
}
