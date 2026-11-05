/*
 * jamie_marriage: marrying Jamie no longer ends the game.
 * jamie_marriage: casarse con Jamie ya no termina el juego.
 *
 * In the original game the wedding with Jamie is the ending. The proposal sets a flag in the save data
 * ("main flag 230"), and while that flag is set the field check (fn_80166888), which the game runs
 * whenever a new day starts and whenever an event ends, plays the wedding and then eight more epilogue
 * events, the last of which sends the game back to the title screen. The field check is the only place
 * in the game's code that tests the flag (docs/SCRIPTS.md, "Qué termina el juego").
 *
 * This mod hooks the field check. Just before the check looks at the flag, the mod looks at the save
 * data: if the wedding ceremony has already started (the game marks an event as started at the moment it
 * begins, so by the time the next field check runs, the ceremony is over), the flag is cleared and the
 * epilogue never starts. The wedding itself still plays, and the game goes on from there.
 *
 * A side effect, seen when the mod was tried in the game: with the flag clear, the same field check falls
 * into the branch of its area-26 handler for "no ending flag", whose default is the musical notes scene
 * (event 0x10B, Eve_music00, where the player is told that five notes turn into an instrument; two events
 * tied to a counter of the save data, 0x105 and 0x106, come first when they are due). So that scene plays
 * once right after the ceremony and then the game returns to free play. It does not loop, because when an
 * event ends the game puts the player back using that event's placement entry, whose field index is not
 * 26 (docs/SCRIPTS.md, "Prueba en ejecución del mod").
 *
 * It is safe to run more than once for the same check, which can happen (see mods/README.md): clearing
 * a flag that is already clear changes nothing. It also covers a second proposal to Jamie, because the
 * proposal sets the flag again and the next field check clears it again.
 *
 * En el juego original la boda con Jamie es el final. La propuesta enciende una bandera en la partida
 * («bandera principal 230») y, mientras está encendida, la revisión del campo (fn_80166888), que el
 * juego corre cada vez que empieza un día nuevo y cada vez que termina un evento, reproduce la boda y
 * otros ocho eventos de epílogo, el último de los cuales manda el juego a la pantalla de título. La
 * revisión del campo es el único lugar del código del juego que prueba la bandera
 * (docs/SCRIPTS.md, «Qué termina el juego»).
 *
 * Este mod engancha la revisión del campo. Justo antes de que la revisión mire la bandera, el mod mira la
 * partida: si la ceremonia de la boda ya empezó (el juego marca un evento como iniciado en el momento en
 * que arranca, así que cuando corre la siguiente revisión del campo la ceremonia ya terminó), apaga la
 * bandera y el epílogo nunca arranca. La boda se juega igual y el juego sigue desde ahí.
 *
 * Un efecto secundario, visto al probar el mod en el juego: con la bandera apagada, la misma revisión del
 * campo cae en la rama de su manejador del área 26 para «sin bandera de final», cuyo valor por defecto es
 * la escena de las notas musicales (evento 0x10B, Eve_music00, donde se le dice al jugador que cinco notas
 * se vuelven un instrumento; antes van dos eventos ligados a un contador de la partida, 0x105 y 0x106,
 * cuando les toca). Así que esa escena sale una vez justo después de la ceremonia y luego el juego vuelve
 * a juego libre. No se repite en bucle, porque al terminar un evento el juego devuelve al jugador con la
 * entrada de colocación de ese evento, cuyo índice de campo no es 26 (docs/SCRIPTS.md, «Prueba en
 * ejecución del mod»).
 *
 * Es seguro que corra más de una vez por la misma revisión, cosa que puede pasar (ver mods/README.md):
 * apagar una bandera que ya está apagada no cambia nada. También cubre una segunda propuesta a Jamie,
 * porque la propuesta vuelve a encender la bandera y la siguiente revisión del campo la vuelve a apagar.
 */

#include <stdbool.h>

#include "hm_mm_mod.h"

#define MOD_ID "jamie_marriage"

/* The game keeps the address of its save data in this word of its memory (0 until a game is loaded). */
#define SAVE_DATA_POINTER_ADDRESS 0x80321F48u
#define NO_SAVE_DATA 0u
#define WORD_BYTES 4
#define BYTE_BYTES 1

/*
 * The save data holds arrays of bits, which the game reads and writes with its own functions
 * (fn_800A77DC tests a bit, fn_800A7804 clears one, fn_800A7824 sets one). Bit number n is bit n % 8 of
 * byte n / 8, counting the bits of a byte from the least significant.
 */
#define BITS_PER_BYTE 8u

/* The "main flags", one bit for each thing the story remembers, start at this offset of the save data. */
#define MAIN_FLAGS_OFFSET 0x04u

/*
 * Main flag 230: set when the player proposes to Jamie. While it is set, the field check starts the
 * wedding and then the epilogue that ends the game.
 */
#define FLAG_STARTS_THE_ENDING 230u

/*
 * The events that the game has already started, so it will not start them again, are another array of
 * bits at this offset of the save data. Event number n is bit n.
 */
#define STARTED_EVENTS_OFFSET 0x6Cu

/*
 * The wedding ceremony with Jamie. The game starts one script or the other, depending on a value of the
 * save data (docs/SCRIPTS.md): Eve_rm01 is event 0x10D and Eve_rm02 is event 0x10E.
 */
#define EVENT_WEDDING_CEREMONY_RM01 0x10Du
#define EVENT_WEDDING_CEREMONY_RM02 0x10Eu

static uint32_t read_word(CPUState* cpu, uint32_t address)
{
    return (uint32_t)moderngekko_mod_read(cpu, address, WORD_BYTES);
}

/* Whether bit `bit_number` of the array of bits that starts at `array_address` is set. */
static bool is_bit_set(CPUState* cpu, uint32_t array_address, uint32_t bit_number)
{
    const uint32_t byte_address = array_address + bit_number / BITS_PER_BYTE;
    const uint32_t bit_mask = 1u << (bit_number % BITS_PER_BYTE);
    return (moderngekko_mod_read(cpu, byte_address, BYTE_BYTES) & bit_mask) != 0;
}

static void clear_bit(CPUState* cpu, uint32_t array_address, uint32_t bit_number)
{
    const uint32_t byte_address = array_address + bit_number / BITS_PER_BYTE;
    const uint32_t bit_mask = 1u << (bit_number % BITS_PER_BYTE);
    const uint64_t current_byte = moderngekko_mod_read(cpu, byte_address, BYTE_BYTES);
    moderngekko_mod_write(cpu, byte_address, current_byte & ~(uint64_t)bit_mask, BYTE_BYTES);
}

/*
 * fn_80166888(field): the field check. This runs when the game enters it, before the flag is tested.
 */
static void clear_the_ending_flag_after_the_wedding(CPUState* cpu)
{
    const uint32_t save_data = read_word(cpu, SAVE_DATA_POINTER_ADDRESS);
    if (save_data == NO_SAVE_DATA) {
        return;
    }
    const uint32_t main_flags = save_data + MAIN_FLAGS_OFFSET;
    const uint32_t started_events = save_data + STARTED_EVENTS_OFFSET;

    const bool wedding_has_started = is_bit_set(cpu, started_events, EVENT_WEDDING_CEREMONY_RM01) ||
                                     is_bit_set(cpu, started_events, EVENT_WEDDING_CEREMONY_RM02);
    if (!wedding_has_started || !is_bit_set(cpu, main_flags, FLAG_STARTS_THE_ENDING)) {
        return;
    }

    clear_bit(cpu, main_flags, FLAG_STARTS_THE_ENDING);
    HM_MM_LOG(MOD_ID,
              "the wedding has started and flag %u was set: cleared it, so the epilogue does not follow",
              (unsigned int)FLAG_STARTS_THE_ENDING);
}

static const ModernGekkoModHook hooks[] = {
    RECOMP_HOOK(DOLRECOMP_SYMBOL_fn_80166888, clear_the_ending_flag_after_the_wedding),
};

static const ModernGekkoModDesc descriptor = {
    .abi_version = MODERNGEKKO_MOD_ABI_VERSION,
    .cpu_abi_version = MODERNGEKKO_CPU_ABI_VERSION,
    .cpu_state_size = sizeof(CPUState),
    .game_id = HM_MM_GAME_ID,
    .id = MOD_ID,
    .version = "0.1.0",
    .display_name = "Jamie marriage does not end the game",
    .hooks = hooks,
    .num_hooks = HM_MM_ARRAY_COUNT(hooks),
};

MODERNGEKKO_MOD_EXPORT const ModernGekkoModDesc* moderngekko_get_mod(void)
{
    return &descriptor;
}
