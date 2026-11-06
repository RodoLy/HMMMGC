/*
 * jamie_marriage: marrying Jamie no longer ends the game, and the game counts the player as married to
 * Jamie, as with any other partner.
 * jamie_marriage: casarse con Jamie ya no termina el juego, y el juego cuenta al jugador como casado con
 * Jamie, como con cualquier otra pareja.
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
 * Being married. With the other twenty partners, the game itself writes in the save data that the player
 * is married, at the moment the field check starts their wedding (fn_80166888, from 0x801672B8 for the
 * first partner, the same seven writes for each one): who the spouse is, main flag 0 ("married"), the
 * "is married" bit of the spouse's own character bits, and three values that it copies or resets. The
 * wedding with Jamie never goes through that code, because the game ended there. So, when this mod clears
 * the ending flag, it also makes those same writes, with Jamie's character number. In the game, after a
 * night's sleep the morning brings the musical note of the wedding ("Your life together starts today"),
 * the same one as with any other partner.
 *
 * Living in the house. Where a character is at each hour comes from the game's data: a table
 * (0x8029F844) has one record of 60 bytes for each character, starting with Jamie, and each record points
 * to nine blocks. Seven are routines, rows of "where to be" whose first byte is a place (0x81 is the
 * player's house). The other two are tables of the hours of the day, one for an ordinary day and one for
 * when the character is the player's spouse: for each hour and kind of day (sunny or rainy, ordinary day
 * or day off, festival) they hold a byte that chooses a row of one of the routines, or "nowhere"
 * (fn_80131884 reads it, and the field, fn_80042D10, puts the characters of an area where it says). The
 * spouse table of every partner sends them home in the morning, in the evening and at night (rows 6, 7
 * and 10 of routine 4, all at the player's house) and to their usual places during the day. Jamie's spouse
 * table says "nowhere" at every hour and Jamie's routine 4 is empty, because the game never meant Jamie to
 * marry: once married, Jamie was nowhere at all. This mod gives Jamie both, built from the game's own data
 * while the game runs (nothing of the game is copied into this file): routine 4 is the first partner's,
 * and the spouse table takes the first partner's byte wherever it sends him home and Jamie's ordinary byte
 * everywhere else. So Jamie starts the day at home, goes to her usual places during the day (she is the
 * outdoors kind, always with the animals and the fields), and comes back home in the evening and to sleep.
 * The spouse table is only read for the spouse, so writing it changes nothing while the player is not
 * married to Jamie.
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
 * Estar casado. Con las otras veinte parejas, el propio juego escribe en la partida que el jugador está
 * casado, en el momento en que la revisión del campo empieza su boda (fn_80166888, desde 0x801672B8 para
 * la primera pareja, las mismas siete escrituras para cada una): quién es la pareja, la bandera principal
 * 0 («casado»), el bit «está casado» de los bits de personaje de la pareja, y tres valores que copia o
 * reinicia. La boda con Jamie nunca pasa por ese código, porque ahí terminaba el juego. Así que, cuando
 * este mod apaga la bandera del final, hace también esas mismas escrituras, con el número de personaje de
 * Jamie. En el juego, tras una noche de sueño la mañana trae la nota musical de la boda («Your life
 * together starts today»), la misma que con cualquier otra pareja.
 *
 * Vivir en la casa. Dónde está un personaje a cada hora sale de los datos del juego: una tabla
 * (0x8029F844) tiene un registro de 60 bytes por personaje, empezando por Jamie, y cada registro apunta a
 * nueve bloques. Siete son rutinas, filas de «dónde estar» cuyo primer byte es un lugar (0x81 es la casa
 * del jugador). Los otros dos son tablas de las horas del día, una para un día normal y otra para cuando el
 * personaje es el cónyuge del jugador: para cada hora y tipo de día (soleado o de lluvia, normal o de
 * descanso, festival) guardan un byte que elige una fila de una de las rutinas, o «en ninguna parte»
 * (fn_80131884 lo lee, y el campo, fn_80042D10, pone a los personajes de un área donde dice). La tabla de
 * cónyuge de cada pareja la manda a casa en la mañana, en la tarde y en la noche (las filas 6, 7 y 10 de la
 * rutina 4, todas en la casa del jugador) y a sus lugares de siempre durante el día. La tabla de cónyuge de
 * Jamie dice «en ninguna parte» a todas horas y su rutina 4 está vacía, porque el juego nunca pensó en
 * casar a Jamie: ya casada, Jamie no estaba en ningún lado. Este mod le da las dos, armadas con los datos
 * del propio juego mientras corre (nada del juego se copia en este archivo): la rutina 4 es la de la
 * primera pareja, y la tabla de cónyuge toma el byte de la primera pareja donde la manda a casa y el byte
 * de un día normal de Jamie en todo lo demás. Así Jamie empieza el día en casa, va a sus lugares de siempre
 * durante el día (es de las que viven afuera, con los animales y el campo) y vuelve a casa en la tarde y a
 * dormir. La tabla de cónyuge solo se lee para el cónyuge, así que escribirla no cambia nada mientras el
 * jugador no está casado con Jamie.
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

/*
 * What the game writes when the player marries (the same writes as fn_80166888 makes, from 0x801672B8, when
 * it starts the wedding of any of the other partners).
 *
 * Characters have numbers: Jamie is 2, the partners of a male player are 3 to 12 and those of a female
 * player 13 to 22 (the field check compares the fiance with these numbers to choose the wedding).
 */
#define JAMIE_CHARACTER_NUMBER 2u

/* Main flag 0: the player is married. */
#define FLAG_MARRIED 0u

/*
 * Values of the save data. The game reads and writes each one with its own functions (fn_800A7940 reads
 * the word at the start of a value, fn_800A7950 writes it, keeping it within the limits that follow it).
 */
#define SPOUSE_OFFSET 0x22B8u  /* the character number of the spouse; 126 while there is none */
#define FIANCE_OFFSET 0x2330u  /* the character number of the fiance (value 137 of the scripts) */
#define HOUSE_VALUE_OFFSET 0x2234u      /* copied into the next one when the player marries */
#define HOUSE_VALUE_COPY_OFFSET 0xD514u
#define SECOND_VALUE_OFFSET 0x224Cu     /* copied into the next one when the player marries */
#define SECOND_VALUE_COPY_OFFSET 0xD520u
#define RESET_VALUE_OFFSET 0x1BC8u      /* set to 0 when the player marries */

/*
 * Every character has three bytes of bits, at this offset of the save data plus three times its number.
 * Bit 3 of them is "married to the player".
 */
#define CHARACTER_BITS_OFFSET 0x5564u
#define CHARACTER_BITS_BYTES_EACH 3u
#define CHARACTER_BIT_MARRIED 3u

/*
 * Where the characters are (see the comment at the top). The table at 0x8029F844 has a record of 60 bytes
 * for each character, starting with Jamie; the pointers to the nine blocks of a character start at byte 16
 * of the record. Blocks 0 to 6 are routines: rows of 18 bytes whose first byte is a place. Block 7 is the
 * table of the hours of an ordinary day and block 8 the one for the player's spouse.
 */
#define ROUTINE_TABLE_ADDRESS 0x8029F844u
#define ROUTINE_RECORD_BYTES 60u
#define BLOCK_POINTERS_OFFSET 16u
#define FIRST_CHARACTER_OF_THE_TABLE 2u /* Jamie */
#define BLOCK_SPOUSE_ROUTINE 4u
#define BLOCK_HOURS_OF_AN_ORDINARY_DAY 7u
#define BLOCK_HOURS_OF_A_SPOUSE 8u
#define ROUTINE_BYTES 200u

/*
 * A table of hours has a row of 12 bytes for each hour, from 6 in the morning; the bytes of a row are the
 * kinds of day (sunny or rainy, ordinary day or day off, festival). Each byte says which row of which
 * routine the character follows at that hour (fn_80131884 reads it), or that the character is nowhere
 * (0x78, and 0x3C on festivals). The bytes from 0x28 to 0x3B choose a row of the spouse routine: the byte
 * minus 0x28 is the row. Every partner's spouse routine has its rows 6 to 10 at the player's house.
 */
#define HOURS_IN_A_DAY 24u
#define HOURS_ROW_BYTES 12u
#define HOURS_TABLE_BYTES (HOURS_IN_A_DAY * HOURS_ROW_BYTES)
#define FIRST_BYTE_OF_THE_SPOUSE_ROUTINE 0x28u
#define FIRST_BYTE_AFTER_THE_SPOUSE_ROUTINE 0x3Cu

/* The partner whose spouse routine and hours give the shape: the first one of a male player. */
#define MODEL_PARTNER_CHARACTER_NUMBER 3u

static uint32_t read_word(CPUState* cpu, uint32_t address)
{
    return (uint32_t)moderngekko_mod_read(cpu, address, WORD_BYTES);
}

static void write_word(CPUState* cpu, uint32_t address, uint32_t value)
{
    moderngekko_mod_write(cpu, address, value, WORD_BYTES);
}

static uint8_t read_byte(CPUState* cpu, uint32_t address)
{
    return (uint8_t)moderngekko_mod_read(cpu, address, BYTE_BYTES);
}

static void write_byte(CPUState* cpu, uint32_t address, uint8_t value)
{
    moderngekko_mod_write(cpu, address, value, BYTE_BYTES);
}

/* Whether bit `bit_number` of the array of bits that starts at `array_address` is set. */
static bool is_bit_set(CPUState* cpu, uint32_t array_address, uint32_t bit_number)
{
    const uint32_t byte_address = array_address + bit_number / BITS_PER_BYTE;
    const uint32_t bit_mask = 1u << (bit_number % BITS_PER_BYTE);
    return (read_byte(cpu, byte_address) & bit_mask) != 0;
}

static void clear_bit(CPUState* cpu, uint32_t array_address, uint32_t bit_number)
{
    const uint32_t byte_address = array_address + bit_number / BITS_PER_BYTE;
    const uint32_t bit_mask = 1u << (bit_number % BITS_PER_BYTE);
    write_byte(cpu, byte_address, (uint8_t)(read_byte(cpu, byte_address) & ~bit_mask));
}

static void set_bit(CPUState* cpu, uint32_t array_address, uint32_t bit_number)
{
    const uint32_t byte_address = array_address + bit_number / BITS_PER_BYTE;
    const uint32_t bit_mask = 1u << (bit_number % BITS_PER_BYTE);
    write_byte(cpu, byte_address, (uint8_t)(read_byte(cpu, byte_address) | bit_mask));
}

/*
 * Writes in the save data that the player is married to Jamie, in the same way as the game does for the
 * other partners. Writing it twice gives the same result, so it does not matter if it runs again.
 */
static void record_the_marriage_with_jamie(CPUState* cpu, uint32_t save_data)
{
    write_word(cpu, save_data + SPOUSE_OFFSET, JAMIE_CHARACTER_NUMBER);
    write_word(cpu, save_data + FIANCE_OFFSET, JAMIE_CHARACTER_NUMBER);
    set_bit(cpu, save_data + MAIN_FLAGS_OFFSET, FLAG_MARRIED);
    write_word(cpu, save_data + HOUSE_VALUE_COPY_OFFSET, read_word(cpu, save_data + HOUSE_VALUE_OFFSET));
    write_word(cpu, save_data + SECOND_VALUE_COPY_OFFSET, read_word(cpu, save_data + SECOND_VALUE_OFFSET));
    write_word(cpu, save_data + RESET_VALUE_OFFSET, 0);

    const uint32_t jamie_bits = save_data + CHARACTER_BITS_OFFSET +
                                CHARACTER_BITS_BYTES_EACH * JAMIE_CHARACTER_NUMBER;
    set_bit(cpu, jamie_bits, CHARACTER_BIT_MARRIED);
}

/* The address of one of the nine blocks of a character (0 when the character has none). */
static uint32_t block_address(CPUState* cpu, uint32_t character_number, uint32_t block_number)
{
    const uint32_t record =
        ROUTINE_TABLE_ADDRESS + (character_number - FIRST_CHARACTER_OF_THE_TABLE) * ROUTINE_RECORD_BYTES;
    return read_word(cpu, record + BLOCK_POINTERS_OFFSET + block_number * WORD_BYTES);
}

static bool is_a_row_of_the_spouse_routine(uint8_t hour_byte)
{
    return hour_byte >= FIRST_BYTE_OF_THE_SPOUSE_ROUTINE && hour_byte < FIRST_BYTE_AFTER_THE_SPOUSE_ROUTINE;
}

/*
 * Copies `count` bytes, and tells whether any of them changed, so that the caller only reports real work.
 */
static bool copy_bytes(CPUState* cpu, uint32_t destination, uint32_t source, uint32_t count)
{
    bool changed = false;
    for (uint32_t index = 0; index < count; ++index) {
        const uint8_t value = read_byte(cpu, source + index);
        if (read_byte(cpu, destination + index) != value) {
            write_byte(cpu, destination + index, value);
            changed = true;
        }
    }
    return changed;
}

/*
 * Gives Jamie the spouse routine and the hours of a spouse, unless they are already there. The game's data
 * is loaded once, when the game starts, so the writing happens once; comparing first keeps it cheap for
 * every area that is entered.
 *
 * Spouse routine: the model partner's, whose rows 6 to 10 are at the player's house.
 * Hours of a spouse: at each hour and on each kind of day, the model partner's byte where it chooses a row
 * of the spouse routine (at home), and Jamie's byte of an ordinary day everywhere else (her usual places).
 */
static void give_jamie_the_life_of_a_spouse(CPUState* cpu)
{
    const uint32_t jamie_spouse_routine = block_address(cpu, JAMIE_CHARACTER_NUMBER, BLOCK_SPOUSE_ROUTINE);
    const uint32_t jamie_ordinary_hours =
        block_address(cpu, JAMIE_CHARACTER_NUMBER, BLOCK_HOURS_OF_AN_ORDINARY_DAY);
    const uint32_t jamie_spouse_hours = block_address(cpu, JAMIE_CHARACTER_NUMBER, BLOCK_HOURS_OF_A_SPOUSE);
    const uint32_t model_spouse_routine =
        block_address(cpu, MODEL_PARTNER_CHARACTER_NUMBER, BLOCK_SPOUSE_ROUTINE);
    const uint32_t model_spouse_hours =
        block_address(cpu, MODEL_PARTNER_CHARACTER_NUMBER, BLOCK_HOURS_OF_A_SPOUSE);
    const bool all_blocks_exist = jamie_spouse_routine != 0 && jamie_ordinary_hours != 0 &&
                                  jamie_spouse_hours != 0 && model_spouse_routine != 0 &&
                                  model_spouse_hours != 0;
    if (!all_blocks_exist) {
        return;
    }

    bool changed = copy_bytes(cpu, jamie_spouse_routine, model_spouse_routine, ROUTINE_BYTES);
    for (uint32_t index = 0; index < HOURS_TABLE_BYTES; ++index) {
        const uint8_t model_byte = read_byte(cpu, model_spouse_hours + index);
        const uint8_t jamie_byte = is_a_row_of_the_spouse_routine(model_byte)
                                       ? model_byte
                                       : read_byte(cpu, jamie_ordinary_hours + index);
        if (read_byte(cpu, jamie_spouse_hours + index) != jamie_byte) {
            write_byte(cpu, jamie_spouse_hours + index, jamie_byte);
            changed = true;
        }
    }
    if (changed) {
        HM_MM_LOG(MOD_ID, "gave Jamie the routine and the hours of a spouse, so that she lives in the house");
    }
}

/* fn_80042D10(): puts the characters of the area that is being entered in it. This runs on its entry. */
static void before_the_characters_are_placed(CPUState* cpu)
{
    give_jamie_the_life_of_a_spouse(cpu);
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
    record_the_marriage_with_jamie(cpu, save_data);
    HM_MM_LOG(MOD_ID,
              "the wedding has started and flag %u was set: cleared it, so the epilogue does not follow, "
              "and recorded that the player is married to Jamie",
              (unsigned int)FLAG_STARTS_THE_ENDING);
}

static const ModernGekkoModHook hooks[] = {
    RECOMP_HOOK(DOLRECOMP_SYMBOL_fn_80166888, clear_the_ending_flag_after_the_wedding),
    RECOMP_HOOK(DOLRECOMP_SYMBOL_fn_80042D10, before_the_characters_are_placed),
};

static const ModernGekkoModDesc descriptor = {
    .abi_version = MODERNGEKKO_MOD_ABI_VERSION,
    .cpu_abi_version = MODERNGEKKO_CPU_ABI_VERSION,
    .cpu_state_size = sizeof(CPUState),
    .game_id = HM_MM_GAME_ID,
    .id = MOD_ID,
    .version = "0.4.0",
    .display_name = "Jamie marriage does not end the game",
    .hooks = hooks,
    .num_hooks = HM_MM_ARRAY_COUNT(hooks),
};

MODERNGEKKO_MOD_EXPORT const ModernGekkoModDesc* moderngekko_get_mod(void)
{
    return &descriptor;
}
