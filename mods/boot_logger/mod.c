/*
 * boot_logger: smoke test of the mod system.
 * boot_logger: prueba de humo del sistema de mods.
 *
 * Hooks the SDK initialisation functions that the game calls while it starts,
 * and main, and prints a line each time one is entered, with the address it
 * was called from (the link register) and the stack pointer. When a run's log
 * shows these lines, mods load, the function names in G4AEE9_symbols.h point
 * at the right addresses, and hooks fire.
 *
 * It changes nothing in the game: a hook runs before the function it watches,
 * and the runtime discards any change the hook makes to the CPU registers.
 * A hook can fire several times for one call (see mods/README.md), so the
 * same line may appear more than once.
 *
 * No cambia nada del juego: un hook corre antes de la función que observa, y
 * el runtime descarta cualquier cambio que haga a los registros de la CPU.
 * Un hook puede dispararse varias veces en una sola llamada (ver
 * mods/README.md), así que una misma línea puede repetirse.
 */

#include "hm_mm_mod.h"

#define MOD_ID "boot_logger"

/*
 * A hook receives no context of its own, so each watched function gets its
 * own small logging function, generated here to avoid six copies of the same
 * code.
 */
#define DEFINE_ENTRY_LOGGER(game_function)                                             \
    static void log_entry_of_##game_function(CPUState* cpu)                            \
    {                                                                                  \
        HM_MM_LOG(MOD_ID, #game_function " entered, called from 0x%08X, stack 0x%08X", \
                  (unsigned int)cpu->lr, (unsigned int)cpu->gpr[1]);                   \
    }

DEFINE_ENTRY_LOGGER(OSInit)
DEFINE_ENTRY_LOGGER(DVDInit)
DEFINE_ENTRY_LOGGER(VIInit)
DEFINE_ENTRY_LOGGER(PADInit)
DEFINE_ENTRY_LOGGER(GXInit)
DEFINE_ENTRY_LOGGER(main)

/*
 * In the order the game first reaches them: __start calls OSInit (which calls
 * DVDInit) and then main; a start-up routine of the game then calls OSInit and
 * DVDInit again, followed by VIInit, PADInit and GXInit.
 */
static const ModernGekkoModHook hooks[] = {
    RECOMP_HOOK(DOLRECOMP_SYMBOL_OSInit, log_entry_of_OSInit),
    RECOMP_HOOK(DOLRECOMP_SYMBOL_DVDInit, log_entry_of_DVDInit),
    RECOMP_HOOK(DOLRECOMP_SYMBOL_main, log_entry_of_main),
    RECOMP_HOOK(DOLRECOMP_SYMBOL_VIInit, log_entry_of_VIInit),
    RECOMP_HOOK(DOLRECOMP_SYMBOL_PADInit, log_entry_of_PADInit),
    RECOMP_HOOK(DOLRECOMP_SYMBOL_GXInit, log_entry_of_GXInit),
};

static const ModernGekkoModDesc descriptor = {
    .abi_version = MODERNGEKKO_MOD_ABI_VERSION,
    .cpu_abi_version = MODERNGEKKO_CPU_ABI_VERSION,
    .cpu_state_size = sizeof(CPUState),
    .game_id = HM_MM_GAME_ID,
    .id = MOD_ID,
    .version = "1.0.0",
    .display_name = "Boot logger (smoke test)",
    .hooks = hooks,
    .num_hooks = HM_MM_ARRAY_COUNT(hooks),
};

MODERNGEKKO_MOD_EXPORT const ModernGekkoModDesc* moderngekko_get_mod(void)
{
    return &descriptor;
}
