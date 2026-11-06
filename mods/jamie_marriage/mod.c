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
 * Living in the house. Where a character is at each hour comes from their daily routines: a table in the
 * game's data (0x8029F844) has one record of 60 bytes for each character, starting with Jamie, and each
 * record points to nine routines. Every one of the twenty partners has, in routines 4 and 5, the routine
 * of a spouse (on a sunny day and on a rainy day): rows of "where to be" for the parts of the day, many
 * of them at place 0x81, the player's house. Jamie's routines 4 and 5 are empty, because the game never
 * meant Jamie to marry, so the field (fn_80042D10, which puts the characters of an area in it) never finds
 * Jamie at home. This mod writes those two routines for Jamie, built from the game's own data while the
 * game runs (nothing of the game is copied into this file): the spouse routine of the first partner gives
 * the shape and the rows at home, and Jamie's own everyday routine gives the places she goes to. On a
 * sunny day Jamie starts the day at home, goes out to the first two places of her usual morning (she is
 * the outdoors kind, always with the animals and the fields), and comes back home for the rest of the
 * day; on a rainy day she stays at home. The routines are only read for a spouse, so writing them changes
 * nothing while the player is not married to Jamie.
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
 * Vivir en la casa. Dónde está un personaje a cada hora sale de sus rutinas diarias: una tabla de los
 * datos del juego (0x8029F844) tiene un registro de 60 bytes por personaje, empezando por Jamie, y cada
 * registro apunta a nueve rutinas. Cada una de las veinte parejas tiene, en las rutinas 4 y 5, la rutina
 * de cónyuge (día soleado y día de lluvia): filas de «dónde estar» para las partes del día, muchas en el
 * lugar 0x81, la casa del jugador. Las rutinas 4 y 5 de Jamie están vacías, porque el juego nunca pensó en
 * casar a Jamie, así que el campo (fn_80042D10, que pone a los personajes de un área en ella) nunca
 * encuentra a Jamie en casa. Este mod escribe esas dos rutinas para Jamie, armadas con los datos del propio
 * juego mientras corre (nada del juego se copia en este archivo): la rutina de cónyuge de la primera
 * pareja da la forma y las filas en casa, y la rutina diaria de Jamie da los lugares a los que va. En un
 * día soleado Jamie empieza el día en casa, sale a los dos primeros lugares de su mañana de siempre (es de
 * las que viven afuera, con los animales y el campo) y vuelve a casa el resto del día; en un día de lluvia
 * se queda en casa. Las rutinas solo se leen para un cónyuge, así que escribirlas no cambia nada mientras
 * el jugador no está casado con Jamie.
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
 * The daily routines (see the comment at the top). Each record of the table is 60 bytes; the pointers to
 * the nine routines start at byte 16 of it. A routine is 200 bytes and holds rows of 18 bytes whose first
 * byte is the place: in a spouse routine, four rows for the morning from byte 18 (the last one is the walk
 * home), an empty row, and five rows at home from byte 108.
 */
#define ROUTINE_TABLE_ADDRESS 0x8029F844u
#define ROUTINE_RECORD_BYTES 60u
#define ROUTINE_POINTERS_OFFSET 16u
#define FIRST_CHARACTER_OF_THE_TABLE 2u /* Jamie */
#define ROUTINE_EVERYDAY_SUNNY 0u
#define ROUTINE_SPOUSE_SUNNY 4u
#define ROUTINE_SPOUSE_RAINY 5u
#define ROUTINE_BYTES 200u
#define ROUTINE_MORNING_ROWS_OFFSET 18u
#define ROUTINE_HOME_ROWS_OFFSET 108u
#define ROUTINE_ROW_BYTES 18u
#define PLACE_PLAYERS_HOUSE 0x81u

/* The partner whose spouse routine gives the shape: the first one of a male player. */
#define MODEL_PARTNER_CHARACTER_NUMBER 3u

/* The morning rows of a spouse routine: rows 0 to 2 are places to be, row 3 is the walk home. */
#define SPOUSE_ROW_FIRST_MORNING 0u
#define SPOUSE_ROW_LAST_MORNING 2u
#define SPOUSE_ROW_WALK_HOME 3u

static uint32_t read_word(CPUState* cpu, uint32_t address)
{
    return (uint32_t)moderngekko_mod_read(cpu, address, WORD_BYTES);
}

static void write_word(CPUState* cpu, uint32_t address, uint32_t value)
{
    moderngekko_mod_write(cpu, address, value, WORD_BYTES);
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

static void set_bit(CPUState* cpu, uint32_t array_address, uint32_t bit_number)
{
    const uint32_t byte_address = array_address + bit_number / BITS_PER_BYTE;
    const uint32_t bit_mask = 1u << (bit_number % BITS_PER_BYTE);
    const uint64_t current_byte = moderngekko_mod_read(cpu, byte_address, BYTE_BYTES);
    moderngekko_mod_write(cpu, byte_address, current_byte | bit_mask, BYTE_BYTES);
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

static uint32_t routine_address(CPUState* cpu, uint32_t character_number, uint32_t routine_number)
{
    const uint32_t record =
        ROUTINE_TABLE_ADDRESS + (character_number - FIRST_CHARACTER_OF_THE_TABLE) * ROUTINE_RECORD_BYTES;
    return read_word(cpu, record + ROUTINE_POINTERS_OFFSET + routine_number * WORD_BYTES);
}

static uint32_t morning_row_address(uint32_t routine, uint32_t row)
{
    return routine + ROUTINE_MORNING_ROWS_OFFSET + row * ROUTINE_ROW_BYTES;
}

static uint32_t first_home_row_address(uint32_t routine)
{
    return routine + ROUTINE_HOME_ROWS_OFFSET;
}

static void copy_bytes(CPUState* cpu, uint32_t destination, uint32_t source, uint32_t count)
{
    for (uint32_t index = 0; index < count; ++index) {
        const uint64_t value = moderngekko_mod_read(cpu, source + index, BYTE_BYTES);
        moderngekko_mod_write(cpu, destination + index, value, BYTE_BYTES);
    }
}

/*
 * Writes Jamie's two spouse routines, unless they are already there. The game's data is loaded once, when
 * the game starts, so the work is done once; looking first keeps it cheap for every area that is entered.
 *
 * Sunny day: the model partner's routine, with Jamie at home in the first row and, in the other two rows
 * of the morning, the first two places of Jamie's everyday morning. Rainy day: at home all day.
 */
static void give_jamie_the_routines_of_a_spouse(CPUState* cpu)
{
    const uint32_t jamie_sunny = routine_address(cpu, JAMIE_CHARACTER_NUMBER, ROUTINE_SPOUSE_SUNNY);
    const uint32_t jamie_rainy = routine_address(cpu, JAMIE_CHARACTER_NUMBER, ROUTINE_SPOUSE_RAINY);
    const uint32_t jamie_everyday = routine_address(cpu, JAMIE_CHARACTER_NUMBER, ROUTINE_EVERYDAY_SUNNY);
    const uint32_t model_sunny = routine_address(cpu, MODEL_PARTNER_CHARACTER_NUMBER, ROUTINE_SPOUSE_SUNNY);
    const uint32_t model_rainy = routine_address(cpu, MODEL_PARTNER_CHARACTER_NUMBER, ROUTINE_SPOUSE_RAINY);
    const bool all_routines_exist =
        jamie_sunny != 0 && jamie_rainy != 0 && jamie_everyday != 0 && model_sunny != 0 && model_rainy != 0;
    if (!all_routines_exist) {
        return;
    }
    const uint32_t first_row_of_jamie = morning_row_address(jamie_sunny, SPOUSE_ROW_FIRST_MORNING);
    if (moderngekko_mod_read(cpu, first_row_of_jamie, BYTE_BYTES) == PLACE_PLAYERS_HOUSE) {
        return; /* already written */
    }
    const uint32_t model_row_at_home = first_home_row_address(model_sunny);

    copy_bytes(cpu, jamie_sunny, model_sunny, ROUTINE_BYTES);
    copy_bytes(cpu, first_row_of_jamie, model_row_at_home, ROUTINE_ROW_BYTES);
    for (uint32_t row = SPOUSE_ROW_FIRST_MORNING + 1; row <= SPOUSE_ROW_LAST_MORNING; ++row) {
        const uint32_t everyday_row = row - 1;
        const uint32_t jamie_place = morning_row_address(jamie_everyday, everyday_row);
        copy_bytes(cpu, morning_row_address(jamie_sunny, row), jamie_place, ROUTINE_ROW_BYTES);
    }

    copy_bytes(cpu, jamie_rainy, model_rainy, ROUTINE_BYTES);
    for (uint32_t row = SPOUSE_ROW_FIRST_MORNING; row <= SPOUSE_ROW_WALK_HOME; ++row) {
        copy_bytes(cpu, morning_row_address(jamie_rainy, row), model_row_at_home, ROUTINE_ROW_BYTES);
    }
    HM_MM_LOG(MOD_ID, "wrote Jamie's routines of a spouse, so that Jamie can live in the house");
}

/* fn_80042D10(): puts the characters of the area that is being entered in it. This runs on its entry. */
static void before_the_characters_are_placed(CPUState* cpu)
{
    give_jamie_the_routines_of_a_spouse(cpu);
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
    .version = "0.3.0",
    .display_name = "Jamie marriage does not end the game",
    .hooks = hooks,
    .num_hooks = HM_MM_ARRAY_COUNT(hooks),
};

MODERNGEKKO_MOD_EXPORT const ModernGekkoModDesc* moderngekko_get_mod(void)
{
    return &descriptor;
}
