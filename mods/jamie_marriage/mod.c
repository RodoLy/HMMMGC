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
 * first partner, the same seven writes for each one): who the spouse is, main flag 0 ("married"), the bit
 * of the spouse's own character bits that makes the spouse ask what to call the player, the date of the
 * wedding (the season and the day, which the anniversary is compared with) and the weather of the day.
 * The wedding with Jamie never goes through that code, because the game ended there. So, when this mod
 * clears the ending flag, it also makes those same writes, with Jamie's character number. In the game,
 * after a night's sleep the morning brings the musical note of the wedding ("Your life together starts
 * today"), the same one as with any other partner.
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
 * Talking like a spouse. What the villagers say comes from talk.lsb, the script of every conversation: it
 * chooses a message number by the situation (the greeting of the hour, rain, a storm, the friendship, the
 * days of marriage, the children, birthdays) and shows that message of the speaker's own file, where each
 * number means the same situation for every character (docs/SCRIPTS.md, "Las conversaciones"). At the
 * spouse's numbers Jamie's file holds a rival's lines ("Just go home." when talked to again), and nothing
 * at 100, the answer once the player has chosen what the spouse will call them. When Jamie is the spouse,
 * this mod gives those numbers lines of its own, written for it, in the five languages whose text is on
 * the disc (the American game reads only the English one), and as a woman or as a man, since Jamie is the
 * opposite of the player. It writes the line into the copy of the file that the game has just loaded, so
 * the game shows it as Jamie's own; the copy is freed right after, and the disc is not touched.
 *
 * A birthday. Every character's birthday is in a table of the game's data (system/Itemdata.bin), and on
 * that day a present the character likes gets the birthday line. Jamie's says "none", so Jamie never had
 * one. While Jamie is the spouse, this mod gives Jamie the 22nd of winter, a day without a festival or
 * anybody else's birthday, in the copy of that table the game keeps in memory, and gives the "none" back
 * otherwise.
 *
 * Babies. When the time comes, the game starts the pregnancy (the one expecting the baby collapses and the
 * doctor gives the news) and, 60 days later, the birth, with a scene of each for every partner. Jamie has none, so
 * with Jamie nothing happened. This mod lends Jamie another partner's scenes: those of partner 4 when the
 * player is the boy (Jamie is the one expecting the baby) and of partner 13 when the player is the girl
 * (Jamie is the father). At the moment the game chooses the scene by the spouse, the mod makes Jamie that
 * partner for the game, and makes Jamie the spouse again right after. When the scene's script is loaded, the
 * mod puts Jamie in it wherever it names the partner, and in the scene's messages it writes Jamie's own lines,
 * in the five languages, over messages that the scene never shows.
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
 * 0 («casado»), el bit de los bits de personaje de la pareja que la hace preguntar cómo llamar al jugador,
 * la fecha de la boda (la estación y el día, con los que se compara el aniversario) y el clima del día. La
 * boda con Jamie nunca pasa por ese código, porque ahí terminaba el juego. Así que, cuando este mod apaga
 * la bandera del final, hace también esas mismas escrituras, con el número de personaje de Jamie. En el
 * juego, tras una noche de sueño la mañana trae la nota musical de la boda («Your life together starts
 * today»), la misma que con cualquier otra pareja.
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
 * Hablar como cónyuge. Lo que dicen los aldeanos sale de talk.lsb, el guion de todas las conversaciones:
 * elige un número de mensaje según la situación (el saludo de la hora, la lluvia, una tormenta, la
 * amistad, los días de casados, los hijos, los cumpleaños) y muestra ese mensaje del archivo del propio
 * personaje, donde cada número significa la misma situación para todos (docs/SCRIPTS.md, «Las
 * conversaciones»). En los números del cónyuge el archivo de Jamie tiene frases de rival («Just go home.»
 * al hablarle otra vez), y nada en el 100, la respuesta cuando el jugador ya eligió cómo lo llamará su
 * pareja. Cuando Jamie es el cónyuge, este mod les da a esos números frases propias, escritas para él, en
 * los cinco idiomas cuyo texto trae el disco (el juego americano solo lee el inglés), y en femenino o en
 * masculino, porque Jamie es lo contrario del jugador. Escribe la frase en la copia del archivo que el
 * juego acaba de cargar, así que el juego la muestra como propia de Jamie; la copia se libera enseguida
 * y el disco no se toca.
 *
 * Un cumpleaños. El cumpleaños de cada personaje está en una tabla de los datos del juego
 * (system/Itemdata.bin), y ese día un regalo que le gusta recibe la frase de cumpleaños. La de Jamie dice
 * «ninguno», así que Jamie nunca tuvo. Mientras Jamie es el cónyuge, este mod le da el 22 de invierno, un
 * día sin festival ni cumpleaños de nadie más, en la copia de esa tabla que el juego tiene en memoria, y le
 * devuelve el «ninguno» si no lo es.
 *
 * Bebés. Llegado el momento, el juego empieza el embarazo (quien espera al bebé se desmaya y el médico da
 * la noticia) y, 60 días después, el parto, con una escena de cada uno para cada pareja. Jamie no tiene
 * ninguna, así que con Jamie no pasaba nada. Este mod le presta a Jamie las escenas de otra pareja: las de la
 * pareja 4 cuando el jugador es el chico (Jamie es quien espera al bebé) y las de la 13 cuando es la chica
 * (Jamie es el papá). En el momento en que el juego elige la escena según el cónyuge, el mod hace que Jamie
 * sea esa pareja para el juego, y enseguida Jamie vuelve a ser el cónyuge. Cuando se carga el guion de la
 * escena, el mod pone a Jamie donde el guion nombra a la pareja, y en los mensajes de la escena escribe frases
 * propias de Jamie, en los cinco idiomas, sobre mensajes que la escena nunca muestra.
 *
 * Es seguro que corra más de una vez por la misma revisión, cosa que puede pasar (ver mods/README.md):
 * apagar una bandera que ya está apagada no cambia nada. También cubre una segunda propuesta a Jamie,
 * porque la propuesta vuelve a encender la bandera y la siguiente revisión del campo la vuelve a apagar.
 */

#include <stdbool.h>
#include <stddef.h>

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
 * The scripts read them by number with their built-in 55 (fn_80060D94, a switch over these offsets); the
 * number of each one is given where the scripts show what it means.
 *
 * The season and the day: talk.lsb (the conversations of the villagers) reads the season, 0 for spring
 * to 3 for winter, as value 124, and the day of the season as value 125, to choose the line of each
 * festival. The wedding copies both into the date of the wedding, and the field check starts the
 * anniversary on the day whose season and day are those (fn_80166888, from 0x80169A78).
 * The weather: talk.lsb reads it as value 129 and chooses the rain line when it is 2 and the storm line
 * when it is 3. The wedding sets it to 0.
 */
#define SPOUSE_OFFSET 0x22B8u  /* the character number of the spouse; 126 while there is none */
#define FIANCE_OFFSET 0x2330u  /* the character number of the fiance (value 137 of the scripts) */
#define SEASON_OFFSET 0x2234u
#define DAY_OF_THE_SEASON_OFFSET 0x224Cu
#define WEDDING_SEASON_OFFSET 0xD514u
#define WEDDING_DAY_OFFSET 0xD520u
#define WEATHER_OFFSET 0x1BC8u

/*
 * Every character has three bytes of bits, at this offset of the save data plus three times its number.
 * Bit 3: the spouse is to ask what to call the player. talk.lsb, on the first conversation after the
 * wedding, shows message 9 of the spouse ("what should I call you") and the list of names, and clears it.
 * (Being the spouse is another bit, 8, which the game sets every morning for the character whose number
 * is the spouse's: fn_80032EE8, from 0x80033670.)
 */
#define CHARACTER_BITS_OFFSET 0x5564u
#define CHARACTER_BITS_BYTES_EACH 3u
#define CHARACTER_BIT_ASKS_WHAT_TO_CALL_THE_PLAYER 3u

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
    write_word(cpu, save_data + WEDDING_SEASON_OFFSET, read_word(cpu, save_data + SEASON_OFFSET));
    write_word(cpu, save_data + WEDDING_DAY_OFFSET, read_word(cpu, save_data + DAY_OF_THE_SEASON_OFFSET));
    write_word(cpu, save_data + WEATHER_OFFSET, 0);

    const uint32_t jamie_bits = save_data + CHARACTER_BITS_OFFSET +
                                CHARACTER_BITS_BYTES_EACH * JAMIE_CHARACTER_NUMBER;
    set_bit(cpu, jamie_bits, CHARACTER_BIT_ASKS_WHAT_TO_CALL_THE_PLAYER);
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

/*
 * Birthdays (see the comment at the top). The word at 0x80321F50 holds where the game keeps its data file
 * system/Itemdata.bin in memory, read once when the game starts. From 0x329E0 of it there is a record of
 * 4 bytes for each character number: the season of the birthday plus 2 in the four high bits of the first
 * byte, and the day plus 2 in the second byte (fn_800A7D3C and fn_800A7D14 read them). Every morning the
 * game sets bit 6 of the bits of the character whose birthday it is (fn_80032EE8, from 0x80033604), and
 * talk.lsb answers a present that the character likes with message 37 on that day. Jamie's record holds
 * season -1 and day -1, the "none" of the game. The low four bits of the first byte are something else,
 * and they are kept.
 */
#define GAME_DATA_POINTER_ADDRESS 0x80321F50u
#define BIRTHDAY_TABLE_OFFSET 0x329E0u
#define BIRTHDAY_RECORD_BYTES 4u
#define BIRTHDAY_RECORD_DAY_OFFSET 1u
#define BIRTHDAY_FIELD_BIAS 2
#define BIRTHDAY_SEASON_SHIFT 4
#define BIRTHDAY_RECORD_OTHER_BITS 0x0Fu
#define NO_BIRTHDAY (-1)

/* The 22nd of winter (season 3): no festival and no other character's birthday falls on it. */
#define SEASON_WINTER 3
#define JAMIE_BIRTHDAY_SEASON SEASON_WINTER
#define JAMIE_BIRTHDAY_DAY 22

/* Writes Jamie's birthday into the game's table, unless it is there already. */
static void set_jamie_birthday(CPUState* cpu, int season, int day)
{
    const uint32_t game_data = read_word(cpu, GAME_DATA_POINTER_ADDRESS);
    if (game_data == 0) {
        return;
    }
    const uint32_t record = game_data + BIRTHDAY_TABLE_OFFSET + JAMIE_CHARACTER_NUMBER * BIRTHDAY_RECORD_BYTES;
    const uint8_t first_byte = read_byte(cpu, record);
    const uint8_t season_byte = (uint8_t)(((season + BIRTHDAY_FIELD_BIAS) << BIRTHDAY_SEASON_SHIFT) |
                                          (first_byte & BIRTHDAY_RECORD_OTHER_BITS));
    const uint8_t day_byte = (uint8_t)(day + BIRTHDAY_FIELD_BIAS);
    if (first_byte == season_byte && read_byte(cpu, record + BIRTHDAY_RECORD_DAY_OFFSET) == day_byte) {
        return;
    }
    write_byte(cpu, record, season_byte);
    write_byte(cpu, record + BIRTHDAY_RECORD_DAY_OFFSET, day_byte);
    HM_MM_LOG(MOD_ID, "set Jamie's birthday to season %d, day %d", season, day);
}

/* Jamie has a birthday while Jamie is the spouse, and the game's "none" otherwise. */
static void give_jamie_a_birthday_while_married(CPUState* cpu)
{
    const uint32_t save_data = read_word(cpu, SAVE_DATA_POINTER_ADDRESS);
    if (save_data == NO_SAVE_DATA) {
        return;
    }
    if (read_word(cpu, save_data + SPOUSE_OFFSET) == JAMIE_CHARACTER_NUMBER) {
        set_jamie_birthday(cpu, JAMIE_BIRTHDAY_SEASON, JAMIE_BIRTHDAY_DAY);
    } else {
        set_jamie_birthday(cpu, NO_BIRTHDAY, NO_BIRTHDAY);
    }
}

/*
 * fn_80042D10(): puts the characters of the area that is being entered in it. This runs on its entry, so
 * the routine, the hours and the birthday are in place by the next morning and the next area.
 */
static void before_the_characters_are_placed(CPUState* cpu)
{
    give_jamie_the_life_of_a_spouse(cpu);
    give_jamie_a_birthday_while_married(cpu);
}

/*
 * Talking like a spouse (see the comment at the top, and docs/SCRIPTS.md, "Las conversaciones").
 *
 * fn_8008534C(window, character, message, ...) is how talk.lsb shows a message (it is behind the script's
 * built-in 52, and nothing else calls it): it takes the path of the character's message file from the
 * table at 0x801DFD40 (a word for each character number), loads the whole file into a new buffer with
 * fn_80029E74, gives up if the message number is above the highest one of the file, finds the text
 * through the file's table, shows it and frees the buffer.
 *
 * A message file starts with the speaker's character number and the highest message number it holds
 * (16 bits each), four more bytes, and where its table and its text start (32 bits each). The table has a
 * word for each message from 0 to the highest one: where the message starts, counted from the start of
 * the text.
 */
#define MESSAGE_FILE_PATHS_ADDRESS 0x801DFD40u
#define MESSAGE_FOLDER_PREFIX "/message_" /* followed by the letter of the language: /message_e/ */
#define FOLDER_SEPARATOR '/'
#define MESSAGE_FILE_HEADER_HIGHEST_MESSAGE 2u
#define MESSAGE_FILE_HEADER_TABLE_START 8u
#define MESSAGE_FILE_HEADER_TEXT_START 12u
#define HALF_WORD_BYTES 2

/*
 * The player's own character number: 0 for the boy and 1 for the girl. fn_800A2B60 (built-in 189, "can the
 * player marry this character") lets player 0 marry characters 3 to 12 and the other one 13 to 22, and
 * talk.lsb reads it as value 1. Jamie is a woman when the player is the boy, and a man when it is the girl.
 */
#define PLAYER_CHARACTER_OFFSET 0x227Cu
#define PLAYER_CHARACTER_THE_BOY 0u

/*
 * The messages that talk.lsb shows to the spouse, by number (the same in every character's file). The
 * friendship is the counter that the script's built-in 122 reads (at save data + 0x6628, 12 bytes for each
 * character number); a first conversation of the day adds 5 to it.
 */
#define MESSAGE_GREETING_IN_THE_MORNING 3u      /* first conversation of the day, from 6 to 10 in the morning; */
#define MESSAGE_GREETING_IN_THE_DAYTIME 4u      /* from 10 in the morning to 5 in the afternoon; */
#define MESSAGE_GREETING_IN_THE_EVENING 5u      /* the rest of the day. Another message follows each greeting. */
#define MESSAGE_TALKED_TO_AGAIN_AS_THE_SPOUSE 7u /* any later conversation of the same day */
#define MESSAGE_THE_DAY_AFTER_A_TYPHOON 8u
#define MESSAGE_ASKS_WHAT_TO_CALL_THE_PLAYER 9u /* first conversation after the wedding; the list of names follows */
#define MESSAGE_PREGNANCY 10u                   /* first conversation once the baby is on the way */
#define MESSAGE_BIRTH 11u                       /* first conversation after the birth */
#define MESSAGE_RAIN_WITH_LITTLE_FRIENDSHIP 12u /* a rainy day, friendship below 400 */
#define MESSAGE_RAIN_WITH_MORE_FRIENDSHIP 13u   /* a rainy day, friendship 400 or more */
#define MESSAGE_FRIENDSHIP_BELOW_100 14u        /* any other day, by the friendship */
#define MESSAGE_FRIENDSHIP_BELOW_200 15u
#define MESSAGE_FRIENDSHIP_BELOW_300 16u
#define MESSAGE_FRIENDSHIP_BELOW_400 17u
#define MESSAGE_FRIENDSHIP_BELOW_500 18u
#define MESSAGE_FRIENDSHIP_BELOW_600 19u
#define MESSAGE_FRIENDSHIP_BELOW_700 20u
#define MESSAGE_FRIENDSHIP_BELOW_800 21u
#define MESSAGE_MARRIED_FOR_LESS_THAN_20_DAYS 26u /* friendship 800 or more, by the days of marriage */
#define MESSAGE_MARRIED_FOR_LESS_THAN_50_DAYS 27u
#define MESSAGE_MARRIED_FOR_LESS_THAN_80_DAYS 28u
#define MESSAGE_MARRIED_FOR_LESS_THAN_110_DAYS 29u
#define MESSAGE_MARRIED_FOR_110_DAYS_OR_MORE 30u
#define MESSAGE_CHILD_YOUNGER_THAN_20_DAYS 31u /* the same, once there is a child, by the child's age */
#define MESSAGE_CHILD_YOUNGER_THAN_50_DAYS 32u
#define MESSAGE_CHILD_YOUNGER_THAN_80_DAYS 33u
#define MESSAGE_CHILD_YOUNGER_THAN_110_DAYS 34u
#define MESSAGE_CHILD_110_DAYS_OR_OLDER 35u
#define MESSAGE_BIRTHDAY_PRESENT 37u            /* a present the spouse likes, on the spouse's birthday */
#define MESSAGE_THE_PLAYER_SHOWS_THE_BABY 50u   /* talking to the spouse with the baby in the arms */
#define MESSAGE_STORM 60u                       /* a stormy day */
#define MESSAGE_THE_PLAYERS_BIRTHDAY 87u        /* friendship 300 or more */
#define MESSAGE_AFTER_THE_PLAYER_CHOSE_A_NAME 100u

/*
 * The game's text. Its letters are those of the console's font, the Windows-1252 set, except that the
 * question mark is the byte 0x9D in every message file of the disc. The texts below are written in UTF-8,
 * so that they read well here, and encode_for_the_game turns them into the game's bytes. Codes that start
 * with the byte 0x01 drive the window, as in the game's own files: a new row (01 04) and a new page
 * (01 05), each followed by 0x1D; what the spouse calls the player (01 3E); the child's name (01 40); and
 * the end of the message (01 08 1F 1D).
 *
 * tests/test_jamie_as_the_spouse.py (the spouse's lines) and tests/test_jamie_pregnancy_and_birth.py (the
 * lines of the scenes) check every text below the way the game will get it: that the font has all its
 * letters, and that it fits the window (rows of up to 28 letters, pages of up to 3 rows).
 */
#define NEW_ROW "\x01\x04\x1D"
#define NEW_PAGE "\x01\x05\x1D"
#define END_OF_MESSAGE "\x01\x08\x1F\x1D"
#define PLAYER_NICKNAME "\x01\x3E"
#define CHILD_NAME "\x01\x40"
#define WINDOW_CODE 0x01u
#define WINDOW_CODE_BYTES 2u

/*
 * The messages of events start with who speaks them: the byte 0x1E and the speaker's character number,
 * which puts the speaker's name on the window (Jamie speaks with 1E 02 in her own events, such as
 * eve_rm01). Only the lines of the scenes below use it.
 */
#define JAMIE_SPEAKS "\x1E\x02"

#define ASCII_LAST 0x7Fu
#define UNICODE_LATIN_1_FIRST 0xA0u /* from here to 0xFF the letters are the font's own bytes: á, ñ, ü, ¿ ... */
#define UNICODE_LATIN_1_LAST 0xFFu
#define UNICODE_CAPITAL_OE 0x0152u
#define UNICODE_SMALL_OE 0x0153u
#define UNICODE_ELLIPSIS 0x2026u
#define FONT_QUESTION_MARK 0x9Du
#define FONT_CAPITAL_OE 0x8Cu
#define FONT_SMALL_OE 0x9Cu
#define FONT_ELLIPSIS 0x85u
#define NOT_IN_THE_FONT (-1)

/*
 * The longest message the mod writes. The game expands a message into its window text in a buffer of
 * 512 bytes, where each name grows to its letters, so this leaves room for them.
 */
#define LONGEST_SPOUSE_MESSAGE_BYTES 256u

typedef struct SpouseMessage {
    uint32_t number; /* the message number that talk.lsb asks for */
    const char* text; /* UTF-8, ending with END_OF_MESSAGE */
    const char* text_if_the_player_is_a_girl; /* when Jamie is a man and the words change; NULL otherwise */
} SpouseMessage;

/*
 * The scenes that Jamie takes part in as the spouse (see "Babies" further down): the pregnancy and the
 * birth, each in the version where Jamie is the wife (the player is the boy) or the husband (the girl).
 */
typedef enum JamieSceneId {
    SCENE_PREGNANCY_AS_THE_WIFE,
    SCENE_PREGNANCY_AS_THE_HUSBAND,
    SCENE_BIRTH_AS_THE_WIFE,
    SCENE_BIRTH_AS_THE_HUSBAND,
    JAMIE_SCENE_COUNT
} JamieSceneId;

/*
 * The messages of the partner whose scene Jamie borrows, by number (the same numbers in the files of every
 * language). Eve_conce01 is the pregnancy of partner 4, and its messages are those of every wife's pregnancy
 * (eve_conce00); Eve_conce10 is the pregnancy with partner 13, with those of every husband's (eve_conce10);
 * Eve_child01 and Eve_child10 are the births, with eve_child00 and eve_child10.
 */
#define PREGNANCY_WIFE_TELLS_THE_NEWS 4u       /* in the clinic's bed, after the doctor's congratulations */
#define PREGNANCY_WIFE_LAST_WORDS 16u          /* after the doctor's advice to the husband, and Martha */
#define PREGNANCY_HUSBAND_FINDS_THE_PLAYER 30u /* at home, where the player has collapsed */
#define PREGNANCY_HUSBAND_HEARS_THE_NEWS 3u    /* at the clinic, after the doctor */
#define PREGNANCY_HUSBAND_LAST_WORDS 15u       /* after the doctor and Martha */
#define BIRTH_WIFE_IN_LABOR 39u                /* before Martha sends the player out of the room */
#define BIRTH_WIFE_HOLDS_THE_BABY 9u           /* after Martha shows the baby */
#define BIRTH_WIFE_ASKS_FOR_A_NAME 10u         /* the screen to write the baby's name follows */
#define BIRTH_WIFE_HEARS_THE_NAME 11u
#define BIRTH_HUSBAND_DURING_THE_LABOR 2u
#define BIRTH_HUSBAND_SEES_THE_BABY 6u
#define BIRTH_HUSBAND_ASKS_FOR_A_NAME 7u
#define BIRTH_HUSBAND_HEARS_THE_NAME 8u

typedef struct SceneLine {
    JamieSceneId scene;
    uint32_t number;  /* the message number that the scene's script shows */
    const char* text; /* UTF-8, starting with JAMIE_SPEAKS and ending with END_OF_MESSAGE */
} SceneLine;

typedef struct SpouseLanguage {
    char folder_letter; /* the letter after MESSAGE_FOLDER_PREFIX in the paths of this language's files */
    const SpouseMessage* messages;
    uint32_t message_count;
    const SceneLine* scene_lines;
    uint32_t scene_line_count;
} SpouseLanguage;

/*
 * Jamie's lines as the spouse, written for this mod. Each one answers something that Jamie says as the
 * rival (that love is an illusion, that everyone ends up alone, that the Harvest Sprites are useless, that
 * birthdays are nothing to celebrate) or at the wedding (that the player showed Jamie people can be
 * kind), and uses Jamie's own words in each language of the disc: Humph, Hah and Geeez in English; Umm,
 * Bah, Cielos and "cara de idiota" in Spanish; Humf, Rahhh and "stupide" in French; Pöh, Oje and "dummes
 * Gesicht" in German; Uffa, Ehi and "stupida faccia" in Italian.
 */
static const SpouseMessage english_spouse_messages[] = {
    {.number = MESSAGE_GREETING_IN_THE_MORNING, .text = "Humph. You're finally up." END_OF_MESSAGE},
    {.number = MESSAGE_GREETING_IN_THE_DAYTIME, .text = "Hah! Taking a break" NEW_ROW "already?" END_OF_MESSAGE},
    {.number = MESSAGE_GREETING_IN_THE_EVENING, .text = "Humph. Done for today?" END_OF_MESSAGE},
    {.number = MESSAGE_TALKED_TO_AGAIN_AS_THE_SPOUSE,
     .text = "Again? Geeez…" NEW_PAGE "You follow me around like" NEW_ROW "one of my sheep." END_OF_MESSAGE},
    {.number = MESSAGE_THE_DAY_AFTER_A_TYPHOON,
     .text = "I saw that storm coming," NEW_ROW "so my field is fine." NEW_PAGE "…Come on. I'll help" NEW_ROW
             "with yours." END_OF_MESSAGE},
    {.number = MESSAGE_ASKS_WHAT_TO_CALL_THE_PLAYER,
     .text = "Humph. We're married now." NEW_ROW "I can't keep calling you" NEW_ROW "idiot, can I?" NEW_PAGE
             "So what should I call you?" END_OF_MESSAGE},
    {.number = MESSAGE_AFTER_THE_PLAYER_CHOSE_A_NAME,
     .text = PLAYER_NICKNAME "… Hah! It sounds" NEW_ROW "silly when I say it." NEW_PAGE
             "…I'll get used to it." END_OF_MESSAGE},
    {.number = MESSAGE_PREGNANCY,
     .text = "A kid, huh…" NEW_ROW "Things are going to get" NEW_ROW "rough around here." NEW_PAGE
             "…Heh. I can't wait." END_OF_MESSAGE},
    {.number = MESSAGE_BIRTH,
     .text = "So small…" NEW_PAGE "I have to get even" NEW_ROW "stronger now, to protect" NEW_ROW
             "this child. And you." END_OF_MESSAGE},
    {.number = MESSAGE_RAIN_WITH_LITTLE_FRIENDSHIP,
     .text = "Rain. I hate it." NEW_PAGE "…Did you shut the barn" NEW_ROW "door? The animals hate" NEW_ROW
             "it too." END_OF_MESSAGE},
    {.number = MESSAGE_RAIN_WITH_MORE_FRIENDSHIP,
     .text = "She never liked rainy" NEW_ROW "days either." NEW_PAGE "…These days I don't" NEW_ROW
             "mind them so much." NEW_ROW "Weird, huh?" END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_100,
     .text = "Don't think I'll go easy" NEW_ROW "on you just because" NEW_ROW "we're married. Hah!" END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_200,
     .text = "Still asking the Harvest" NEW_ROW "Sprites for help?" NEW_PAGE "Humph. Ask me instead." NEW_ROW
             "I'm better at it." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_300,
     .text = "I swore I'd never join" NEW_ROW "forces with you." NEW_PAGE "…Now look at us. Heh." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_400,
     .text = "I used to think everyone" NEW_ROW "ends up alone." NEW_PAGE "…Now I'm not so sure." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_500,
     .text = "Your silly face again." NEW_PAGE "…Heh. I'm getting used" NEW_ROW "to it." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_600,
     .text = "I always said you can only" NEW_ROW "rely on yourself." NEW_PAGE "Maybe I was wrong" NEW_ROW
             "about that." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_700,
     .text = "Let's get something" NEW_ROW "straight." NEW_PAGE "If you're ever in trouble," NEW_ROW
             "you come to me. Got it?" END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_800,
     .text = "You showed me people can" NEW_ROW "be kind." NEW_PAGE "I still don't trust most" NEW_ROW
             "of them. But I trust you." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_20_DAYS,
     .text = "Wipe that happy look off" NEW_ROW "your face." NEW_PAGE "…Geeez. Now I'm" NEW_ROW
             "smiling too." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_50_DAYS,
     .text = "Take it easy today." NEW_ROW "I'll look after the" NEW_ROW "animals." NEW_PAGE
             "They like me better" NEW_ROW "anyway." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_80_DAYS,
     .text = "Eat something before" NEW_ROW "you go out." NEW_PAGE "If you collapse, I'm the" NEW_ROW
             "one who has to carry you." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_110_DAYS,
     .text = "I once said love was" NEW_ROW "just an illusion." NEW_PAGE "…Don't you dare" NEW_ROW
             "remind me." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_110_DAYS_OR_MORE,
     .text = "When I came to this" NEW_ROW "village, I had no one." NEW_PAGE "Now I have a home." NEW_ROW
             "Thanks to you, " PLAYER_NICKNAME "." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_20_DAYS,
     .text = "The baby kept me up all" NEW_ROW "night. Geeez." NEW_PAGE "…Still, it's cute when" NEW_ROW
             "it sleeps." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_50_DAYS,
     .text = CHILD_NAME " smiled at me" NEW_ROW "today." NEW_PAGE "…Heh. Kids don't lie," NEW_ROW
             "do they?" END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_80_DAYS,
     .text = CHILD_NAME " has your silly" NEW_ROW "face." NEW_PAGE "…Lucky kid." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_110_DAYS,
     .text = CHILD_NAME " tried to milk" NEW_ROW "a cow today." NEW_PAGE "Heh. A born farmer." NEW_ROW
             "Just like me." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_110_DAYS_OR_OLDER,
     .text = "There's more than one way" NEW_ROW "to live." NEW_PAGE CHILD_NAME " can choose any" NEW_ROW
             "of them. We'll be there" NEW_ROW "either way." END_OF_MESSAGE},
    {.number = MESSAGE_BIRTHDAY_PRESENT,
     .text = "For my birthday?" NEW_PAGE "Nobody's given me anything" NEW_ROW "in a long time." NEW_PAGE
             "…Thanks." END_OF_MESSAGE},
    {.number = MESSAGE_THE_PLAYER_SHOWS_THE_BABY,
     .text = "Hey, careful!" NEW_ROW "Hold the head up." NEW_PAGE "Geeez, you're clumsy." END_OF_MESSAGE},
    {.number = MESSAGE_STORM,
     .text = "Listen to that wind." NEW_PAGE "You're not going out in" NEW_ROW "this. Stay here." END_OF_MESSAGE},
    {.number = MESSAGE_THE_PLAYERS_BIRTHDAY,
     .text = "Your birthday, huh." NEW_ROW "I used to think birthdays" NEW_ROW "were nothing to celebrate." NEW_PAGE
             "…Happy birthday, idiot." NEW_PAGE "…Sorry. Old habit." NEW_ROW
             "Happy birthday, " PLAYER_NICKNAME "." END_OF_MESSAGE},
};

static const SpouseMessage spanish_spouse_messages[] = {
    {.number = MESSAGE_GREETING_IN_THE_MORNING, .text = "Umm. Por fin te levantas." END_OF_MESSAGE},
    {.number = MESSAGE_GREETING_IN_THE_DAYTIME, .text = "¡Bah! ¿Ya estás" NEW_ROW "descansando?" END_OF_MESSAGE},
    {.number = MESSAGE_GREETING_IN_THE_EVENING, .text = "Umm. ¿Ya has terminado" NEW_ROW "por hoy?" END_OF_MESSAGE},
    {.number = MESSAGE_TALKED_TO_AGAIN_AS_THE_SPOUSE,
     .text = "¿Otra vez? Cielos..." NEW_PAGE "Me sigues a todas partes," NEW_ROW
             "como una de mis ovejas." END_OF_MESSAGE},
    {.number = MESSAGE_THE_DAY_AFTER_A_TYPHOON,
     .text = "Ya sabía que vendría esa" NEW_ROW "tormenta, así que mi campo" NEW_ROW "está bien." NEW_PAGE
             "...Venga. Te ayudo" NEW_ROW "con el tuyo." END_OF_MESSAGE},
    {.number = MESSAGE_ASKS_WHAT_TO_CALL_THE_PLAYER,
     .text = "Umm. Ahora estamos casados." NEW_ROW "No puedo seguir llamándote" NEW_ROW "idiota, ¿verdad?" NEW_PAGE
             "¿Cómo debería llamarte?" END_OF_MESSAGE},
    {.number = MESSAGE_AFTER_THE_PLAYER_CHOSE_A_NAME,
     .text = PLAYER_NICKNAME "... ¡Bah!" NEW_ROW "Suena ridículo cuando lo" NEW_ROW "digo yo." NEW_PAGE
             "...Ya me acostumbraré." END_OF_MESSAGE},
    {.number = MESSAGE_PREGNANCY,
     .text = "Así que un bebé..." NEW_ROW "Las cosas se van a poner" NEW_ROW "difíciles por aquí." NEW_PAGE
             "...Eh. Qué ganas tengo." END_OF_MESSAGE},
    {.number = MESSAGE_BIRTH,
     .text = "Qué cosita tan pequeña..." NEW_PAGE "Ahora tengo que ser aún" NEW_ROW "más fuerte, para proteger"
             NEW_ROW "a este bebé. Y a ti." END_OF_MESSAGE},
    {.number = MESSAGE_RAIN_WITH_LITTLE_FRIENDSHIP,
     .text = "Lluvia. La odio." NEW_PAGE "...¿Has cerrado el establo?" NEW_ROW "A los animales tampoco"
             NEW_ROW "les gusta." END_OF_MESSAGE},
    {.number = MESSAGE_RAIN_WITH_MORE_FRIENDSHIP,
     .text = "A ella tampoco le gustaban" NEW_ROW "los días de lluvia." NEW_PAGE "...Ahora a mí ya no me" NEW_ROW
             "molestan tanto." NEW_ROW "Qué raro, ¿no?" END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_100,
     .text = "No creas que te lo pondré" NEW_ROW "fácil por estar casados." NEW_ROW "¡Bah!" END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_200,
     .text = "¿Sigues pidiendo ayuda a" NEW_ROW "los Duendes de la cosecha?" NEW_PAGE "Umm. Pídemela a mí." NEW_ROW
             "Se me da mejor." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_300,
     .text = "Juré que nunca uniría mis" NEW_ROW "fuerzas a las tuyas." NEW_PAGE
             "...Y míranos ahora. Eh." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_400,
     .text = "Antes pensaba que al final" NEW_ROW "uno siempre acaba solo." NEW_PAGE "...Ahora ya no lo tengo"
             NEW_ROW "tan claro." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_500,
     .text = "Otra vez esa cara de" NEW_ROW "idiota." NEW_PAGE "...Eh. Me estoy" NEW_ROW
             "acostumbrando." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_600,
     .text = "Siempre decía que uno solo" NEW_ROW "puede contar consigo mismo." NEW_PAGE
             "Quizá me equivocaba." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_700,
     .text = "Dejemos las cosas claras." NEW_PAGE "Si alguna vez tienes" NEW_ROW "problemas, vienes a mí." NEW_ROW
             "¿Entendido?" END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_800,
     .text = "Tú me enseñaste que la" NEW_ROW "gente puede ser amable." NEW_PAGE "Aún no me fío de casi" NEW_ROW
             "nadie. Pero de ti, sí." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_20_DAYS,
     .text = "Quita esa cara de" NEW_ROW "felicidad." NEW_PAGE "...Cielos. Ahora yo" NEW_ROW
             "también sonrío." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_50_DAYS,
     .text = "Descansa hoy." NEW_ROW "Yo me ocupo de los" NEW_ROW "animales." NEW_PAGE "De todas formas, me"
             NEW_ROW "prefieren a mí." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_80_DAYS,
     .text = "Come algo antes de" NEW_ROW "salir." NEW_PAGE "Si te desmayas, me toca" NEW_ROW
             "a mí llevarte a cuestas." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_110_DAYS,
     .text = "Una vez dije que el amor" NEW_ROW "era solo una ilusión." NEW_PAGE "...Ni se te ocurra" NEW_ROW
             "recordármelo." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_110_DAYS_OR_MORE,
     .text = "Cuando llegué a este" NEW_ROW "pueblo, no tenía a nadie." NEW_PAGE "Ahora tengo un hogar." NEW_ROW
             "Gracias a ti, " PLAYER_NICKNAME "." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_20_DAYS,
     .text = "El bebé no me ha dejado" NEW_ROW "dormir. Cielos." NEW_PAGE "...Aun así, es una monada" NEW_ROW
             "cuando duerme." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_50_DAYS,
     .text = CHILD_NAME " me ha sonreído" NEW_ROW "hoy." NEW_PAGE "...Eh. Los niños no" NEW_ROW
             "mienten, ¿verdad?" END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_80_DAYS,
     .text = CHILD_NAME " tiene tu cara de" NEW_ROW "idiota." NEW_PAGE "...Qué suerte tiene." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_110_DAYS,
     .text = "Hoy " CHILD_NAME " ha intentado" NEW_ROW "ordeñar una vaca." NEW_PAGE "Eh. Lo lleva en la sangre."
             NEW_ROW "Como yo." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_110_DAYS_OR_OLDER,
     .text = "Hay muchas formas de" NEW_ROW "vivir." NEW_PAGE CHILD_NAME " puede elegir la" NEW_ROW
             "que quiera. Estaremos ahí" NEW_ROW "de todos modos." END_OF_MESSAGE},
    {.number = MESSAGE_BIRTHDAY_PRESENT,
     .text = "¿Por mi cumpleaños?" NEW_PAGE "Hace mucho que nadie me" NEW_ROW "regala nada." NEW_PAGE
             "...Gracias." END_OF_MESSAGE},
    {.number = MESSAGE_THE_PLAYER_SHOWS_THE_BABY,
     .text = "¡Eh, cuidado!" NEW_ROW "Sujétale la cabeza." NEW_PAGE "Cielos, qué torpe eres." END_OF_MESSAGE},
    {.number = MESSAGE_STORM,
     .text = "Escucha ese viento." NEW_PAGE "Con esto no vas a salir." NEW_ROW "Quédate aquí." END_OF_MESSAGE},
    {.number = MESSAGE_THE_PLAYERS_BIRTHDAY,
     .text = "Tu cumpleaños, ¿eh?" NEW_ROW "Antes pensaba que no había" NEW_ROW "nada que celebrar." NEW_PAGE
             "...Feliz cumpleaños," NEW_ROW "idiota." NEW_PAGE "...Perdona. La costumbre." NEW_ROW
             "Feliz cumpleaños, " PLAYER_NICKNAME "." END_OF_MESSAGE},
};

static const SpouseMessage french_spouse_messages[] = {
    {.number = MESSAGE_GREETING_IN_THE_MORNING, .text = "Humf. Enfin debout ?" END_OF_MESSAGE},
    {.number = MESSAGE_GREETING_IN_THE_DAYTIME, .text = "Ha ! Déjà en pause ?" END_OF_MESSAGE},
    {.number = MESSAGE_GREETING_IN_THE_EVENING, .text = "Humf. Tu as fini pour" NEW_ROW "aujourd'hui ?" END_OF_MESSAGE},
    {.number = MESSAGE_TALKED_TO_AGAIN_AS_THE_SPOUSE,
     .text = "Encore ? Rahhh !" NEW_PAGE "Tu me suis partout, comme" NEW_ROW "un de mes moutons." END_OF_MESSAGE},
    {.number = MESSAGE_THE_DAY_AFTER_A_TYPHOON,
     .text = "J'avais prévu cette" NEW_ROW "tempête : mon champ va" NEW_ROW "bien." NEW_PAGE
             "…Allez, viens. Je vais" NEW_ROW "t'aider avec le tien." END_OF_MESSAGE},
    {.number = MESSAGE_ASKS_WHAT_TO_CALL_THE_PLAYER,
     .text = "Humf. Maintenant qu'on est" NEW_ROW "mariés, je ne peux plus" NEW_ROW
             "t'appeler \"stupide\", hein ?" NEW_PAGE "Comment je dois t'appeler ?" END_OF_MESSAGE},
    {.number = MESSAGE_AFTER_THE_PLAYER_CHOSE_A_NAME,
     .text = PLAYER_NICKNAME "… Ha ! Ça sonne" NEW_ROW "bête dans ma bouche." NEW_PAGE
             "…Je m'y habituerai." END_OF_MESSAGE},
    {.number = MESSAGE_PREGNANCY,
     .text = "Un enfant, hein…" NEW_ROW "Ça va devenir compliqué," NEW_ROW "ici." NEW_PAGE
             "…Hé. J'ai hâte." END_OF_MESSAGE},
    {.number = MESSAGE_BIRTH,
     .text = "Si petit…" NEW_PAGE "Je dois devenir encore" NEW_ROW "plus forte, pour protéger" NEW_ROW
             "ce bébé. Et toi aussi." END_OF_MESSAGE,
     .text_if_the_player_is_a_girl = "Si petit…" NEW_PAGE "Je dois devenir encore" NEW_ROW
                                     "plus fort, pour protéger" NEW_ROW "ce bébé. Et toi aussi." END_OF_MESSAGE},
    {.number = MESSAGE_RAIN_WITH_LITTLE_FRIENDSHIP,
     .text = "La pluie. Je déteste ça." NEW_PAGE "…Tu as fermé l'étable ?" NEW_ROW "Les animaux la détestent"
             NEW_ROW "aussi." END_OF_MESSAGE},
    {.number = MESSAGE_RAIN_WITH_MORE_FRIENDSHIP,
     .text = "Elle non plus n'aimait pas" NEW_ROW "les jours de pluie." NEW_PAGE "…Moi, ça me dérange" NEW_ROW
             "moins, maintenant." NEW_ROW "Bizarre, hein ?" END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_100,
     .text = "Ne crois pas que je vais" NEW_ROW "te ménager parce qu'on" NEW_ROW "est mariés. Ha !" END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_200,
     .text = "Tu demandes encore de" NEW_ROW "l'aide aux lutins des" NEW_ROW "récoltes ?" NEW_PAGE
             "Humf. Demande-moi plutôt." NEW_ROW "Je fais ça mieux qu'eux." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_300,
     .text = "J'avais juré de ne jamais" NEW_ROW "m'associer avec toi." NEW_PAGE
             "…Et regarde-nous. Hé." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_400,
     .text = "Avant, je pensais qu'on" NEW_ROW "finissait tous seuls." NEW_PAGE "…Maintenant, j'ai des" NEW_ROW
             "doutes." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_500,
     .text = "Encore ton visage" NEW_ROW "stupide." NEW_PAGE "…Hé. Je commence à m'y" NEW_ROW
             "habituer." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_600,
     .text = "J'ai toujours dit qu'on ne" NEW_ROW "peut compter que sur soi." NEW_PAGE "Peut-être que j'avais"
             NEW_ROW "tort." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_700,
     .text = "Soyons clairs." NEW_PAGE "Si tu as des ennuis, tu" NEW_ROW "viens me voir. Compris ?" END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_800,
     .text = "Tu m'as montré que les gens" NEW_ROW "peuvent être gentils." NEW_PAGE "Je me méfie encore de la"
             NEW_ROW "plupart. Mais pas de toi." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_20_DAYS,
     .text = "Efface-moi ce sourire" NEW_ROW "béat." NEW_PAGE "…Rahhh ! Voilà que je" NEW_ROW
             "souris aussi." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_50_DAYS,
     .text = "Repose-toi aujourd'hui." NEW_ROW "Je m'occupe des animaux." NEW_PAGE "De toute façon, ils" NEW_ROW
             "me préfèrent." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_80_DAYS,
     .text = "Mange quelque chose avant" NEW_ROW "de sortir." NEW_PAGE "Si tu t'effondres, c'est" NEW_ROW
             "moi qui devrai te porter." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_110_DAYS,
     .text = "J'ai dit un jour que" NEW_ROW "l'amour était une illusion." NEW_PAGE "…Ne t'avise pas de me"
             NEW_ROW "le rappeler." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_110_DAYS_OR_MORE,
     .text = "À mon arrivée au village," NEW_ROW "je n'avais personne." NEW_PAGE "Maintenant, j'ai un foyer."
             NEW_ROW "Grâce à toi, " PLAYER_NICKNAME "." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_20_DAYS,
     .text = "Le bébé a pleuré toute" NEW_ROW "la nuit. Rahhh !" NEW_PAGE "…Mais il est mignon quand" NEW_ROW
             "il dort." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_50_DAYS,
     .text = CHILD_NAME " m'a souri" NEW_ROW "aujourd'hui." NEW_PAGE "…Hé. Les enfants ne" NEW_ROW
             "mentent pas, hein ?" END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_80_DAYS,
     .text = CHILD_NAME " a ton visage" NEW_ROW "stupide." NEW_PAGE "…Quelle chance." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_110_DAYS,
     .text = CHILD_NAME " a essayé de" NEW_ROW "traire une vache." NEW_PAGE "Hé. La ferme, c'est dans" NEW_ROW
             "le sang. Comme moi." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_110_DAYS_OR_OLDER,
     .text = "Il y a plus d'une façon" NEW_ROW "de vivre." NEW_PAGE CHILD_NAME " pourra choisir" NEW_ROW
             "la sienne. On sera là," NEW_ROW "quoi qu'il arrive." END_OF_MESSAGE},
    {.number = MESSAGE_BIRTHDAY_PRESENT,
     .text = "Pour mon anniversaire ?" NEW_PAGE "Ça fait longtemps qu'on ne" NEW_ROW "m'a rien offert." NEW_PAGE
             "…Merci." END_OF_MESSAGE},
    {.number = MESSAGE_THE_PLAYER_SHOWS_THE_BABY,
     .text = "Hé, attention !" NEW_ROW "Tiens-lui la tête." NEW_PAGE "Rahhh ! Ce que tu es" NEW_ROW
             "maladroit." END_OF_MESSAGE,
     .text_if_the_player_is_a_girl = "Hé, attention !" NEW_ROW "Tiens-lui la tête." NEW_PAGE
                                     "Rahhh ! Ce que tu es" NEW_ROW "maladroite." END_OF_MESSAGE},
    {.number = MESSAGE_STORM,
     .text = "Écoute ce vent." NEW_PAGE "Tu ne sors pas par ce" NEW_ROW "temps. Reste ici." END_OF_MESSAGE},
    {.number = MESSAGE_THE_PLAYERS_BIRTHDAY,
     .text = "Ton anniversaire, hein ?" NEW_ROW "Avant, je trouvais qu'il" NEW_ROW "n'y avait rien à fêter." NEW_PAGE
             "…Joyeux anniversaire," NEW_ROW "imbécile." NEW_PAGE "…Pardon. L'habitude." NEW_ROW
             "Joyeux anniversaire," NEW_ROW PLAYER_NICKNAME "." END_OF_MESSAGE},
};

static const SpouseMessage german_spouse_messages[] = {
    {.number = MESSAGE_GREETING_IN_THE_MORNING, .text = "Pöh. Endlich wach?" END_OF_MESSAGE},
    {.number = MESSAGE_GREETING_IN_THE_DAYTIME, .text = "Ha! Schon Pause?" END_OF_MESSAGE},
    {.number = MESSAGE_GREETING_IN_THE_EVENING, .text = "Pöh. Feierabend für heute?" END_OF_MESSAGE},
    {.number = MESSAGE_TALKED_TO_AGAIN_AS_THE_SPOUSE,
     .text = "Schon wieder? Oje …" NEW_PAGE "Du läufst mir nach wie" NEW_ROW "eins meiner Schafe." END_OF_MESSAGE},
    {.number = MESSAGE_THE_DAY_AFTER_A_TYPHOON,
     .text = "Ich hab den Sturm kommen" NEW_ROW "sehen. Mein Feld ist heil." NEW_PAGE "… Komm schon. Ich helf"
             NEW_ROW "dir mit deinem." END_OF_MESSAGE},
    {.number = MESSAGE_ASKS_WHAT_TO_CALL_THE_PLAYER,
     .text = "Pöh. Wir sind jetzt" NEW_ROW "verheiratet. Da kann ich" NEW_ROW "dich nicht Idiot nennen." NEW_PAGE
             "Wie soll ich dich nennen?" END_OF_MESSAGE},
    {.number = MESSAGE_AFTER_THE_PLAYER_CHOSE_A_NAME,
     .text = PLAYER_NICKNAME " … Ha! Klingt" NEW_ROW "albern, wenn ich das sage." NEW_PAGE
             "… Ich gewöhn mich dran." END_OF_MESSAGE},
    {.number = MESSAGE_PREGNANCY,
     .text = "Ein Kind, was? …" NEW_ROW "Das wird anstrengend" NEW_ROW "hier." NEW_PAGE
             "… Ha. Ich kann es kaum" NEW_ROW "erwarten." END_OF_MESSAGE},
    {.number = MESSAGE_BIRTH,
     .text = "So klein …" NEW_PAGE "Jetzt muss ich noch" NEW_ROW "stärker werden, um dieses" NEW_ROW
             "Kind zu schützen. Und dich." END_OF_MESSAGE},
    {.number = MESSAGE_RAIN_WITH_LITTLE_FRIENDSHIP,
     .text = "Regen. Ich hasse das." NEW_PAGE "… Ist der Stall zu?" NEW_ROW "Die Tiere hassen ihn" NEW_ROW
             "auch." END_OF_MESSAGE},
    {.number = MESSAGE_RAIN_WITH_MORE_FRIENDSHIP,
     .text = "Sie mochte Regentage" NEW_ROW "auch nie." NEW_PAGE "… Mich stören sie jetzt" NEW_ROW
             "kaum noch." NEW_ROW "Komisch, oder?" END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_100,
     .text = "Glaub nicht, dass ich dich" NEW_ROW "schone, nur weil wir" NEW_ROW
             "verheiratet sind. Ha!" END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_200,
     .text = "Bittest du immer noch die" NEW_ROW "Erntegeister um Hilfe?" NEW_PAGE "Pöh. Frag lieber mich." NEW_ROW
             "Ich kann das besser." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_300,
     .text = "Ich schwor, mich nie mit" NEW_ROW "dir zusammenzutun." NEW_PAGE "… Und jetzt sieh uns an."
             NEW_ROW "Ha." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_400,
     .text = "Früher dachte ich, am Ende" NEW_ROW "ist jeder allein." NEW_PAGE "… Jetzt bin ich mir nicht"
             NEW_ROW "mehr so sicher." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_500,
     .text = "Schon wieder dein dummes" NEW_ROW "Gesicht." NEW_PAGE "… Ha. Ich gewöhn mich" NEW_ROW
             "langsam dran." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_600,
     .text = "Ich sagte immer, man kann" NEW_ROW "nur auf sich selbst zählen." NEW_PAGE "Vielleicht lag ich da"
             NEW_ROW "falsch." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_700,
     .text = "Eins wollen wir" NEW_ROW "klarstellen." NEW_PAGE "Wenn du mal Probleme hast," NEW_ROW
             "kommst du zu mir. Klar?" END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_800,
     .text = "Du hast mir gezeigt, dass" NEW_ROW "Menschen nett sein können." NEW_PAGE
             "Den meisten traue ich noch" NEW_ROW "nicht. Aber dir schon." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_20_DAYS,
     .text = "Hör auf, so glücklich zu" NEW_ROW "gucken." NEW_PAGE "… Oje. Jetzt grinse ich" NEW_ROW
             "auch." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_50_DAYS,
     .text = "Ruh dich heute aus." NEW_ROW "Ich kümmere mich um die" NEW_ROW "Tiere." NEW_PAGE
             "Die mögen mich sowieso" NEW_ROW "lieber." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_80_DAYS,
     .text = "Iss was, bevor du" NEW_ROW "rausgehst." NEW_PAGE "Wenn du umkippst, muss" NEW_ROW
             "ich dich tragen." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_110_DAYS,
     .text = "Ich sagte mal, Liebe sei" NEW_ROW "nur eine Illusion." NEW_PAGE "… Wag es ja nicht, mich" NEW_ROW
             "daran zu erinnern." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_110_DAYS_OR_MORE,
     .text = "Als ich in dieses Dorf" NEW_ROW "kam, hatte ich niemanden." NEW_PAGE "Jetzt hab ich ein Zuhause."
             NEW_ROW "Dank dir, " PLAYER_NICKNAME "." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_20_DAYS,
     .text = "Das Baby hat mich die" NEW_ROW "ganze Nacht wach gehalten." NEW_ROW "Oje." NEW_PAGE
             "… Aber wenn es schläft," NEW_ROW "ist es süß." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_50_DAYS,
     .text = CHILD_NAME " hat mich heute" NEW_ROW "angelächelt." NEW_PAGE "… Ha. Kinder lügen nicht,"
             NEW_ROW "oder?" END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_80_DAYS,
     .text = CHILD_NAME " hat dein dummes" NEW_ROW "Gesicht." NEW_PAGE "… Glückspilz." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_110_DAYS,
     .text = CHILD_NAME " wollte heute" NEW_ROW "eine Kuh melken." NEW_PAGE "Ha. Das liegt im Blut." NEW_ROW
             "Wie bei mir." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_110_DAYS_OR_OLDER,
     .text = "Es gibt viele Arten zu" NEW_ROW "leben." NEW_PAGE CHILD_NAME " darf sich eine" NEW_ROW
             "aussuchen. Wir sind so" NEW_ROW "oder so da." END_OF_MESSAGE},
    {.number = MESSAGE_BIRTHDAY_PRESENT,
     .text = "Zu meinem Geburtstag?" NEW_PAGE "Mir hat schon lange keiner" NEW_ROW "mehr was geschenkt." NEW_PAGE
             "… Danke." END_OF_MESSAGE},
    {.number = MESSAGE_THE_PLAYER_SHOWS_THE_BABY,
     .text = "He, vorsichtig!" NEW_ROW "Halt den Kopf hoch." NEW_PAGE "Oje, bist du" NEW_ROW
             "ungeschickt." END_OF_MESSAGE},
    {.number = MESSAGE_STORM,
     .text = "Hör dir den Wind an." NEW_PAGE "Bei dem Wetter gehst du" NEW_ROW "nicht raus. Bleib hier."
             END_OF_MESSAGE},
    {.number = MESSAGE_THE_PLAYERS_BIRTHDAY,
     .text = "Dein Geburtstag, was?" NEW_ROW "Früher fand ich, das sei" NEW_ROW "kein Grund zum Feiern." NEW_PAGE
             "… Alles Gute, du Idiot." NEW_PAGE "… Entschuldige. Alte" NEW_ROW "Gewohnheit." NEW_ROW
             "Alles Gute, " PLAYER_NICKNAME "." END_OF_MESSAGE},
};

static const SpouseMessage italian_spouse_messages[] = {
    {.number = MESSAGE_GREETING_IN_THE_MORNING, .text = "Uffa. Finalmente in piedi?" END_OF_MESSAGE},
    {.number = MESSAGE_GREETING_IN_THE_DAYTIME, .text = "Ah! Già in pausa?" END_OF_MESSAGE},
    {.number = MESSAGE_GREETING_IN_THE_EVENING, .text = "Uffa. Hai finito per oggi?" END_OF_MESSAGE},
    {.number = MESSAGE_TALKED_TO_AGAIN_AS_THE_SPOUSE,
     .text = "Ancora? Ehi…" NEW_PAGE "Mi segui dappertutto, come" NEW_ROW "una delle mie pecore." END_OF_MESSAGE},
    {.number = MESSAGE_THE_DAY_AFTER_A_TYPHOON,
     .text = "Avevo previsto quella" NEW_ROW "tempesta: il mio campo sta" NEW_ROW "bene." NEW_PAGE
             "…Dai, vieni. Ti aiuto" NEW_ROW "con il tuo." END_OF_MESSAGE},
    {.number = MESSAGE_ASKS_WHAT_TO_CALL_THE_PLAYER,
     .text = "Uffa. Ora siamo sposati." NEW_ROW "Non posso continuare a" NEW_ROW "chiamarti idiota, no?" NEW_PAGE
             "Allora, come ti chiamo?" END_OF_MESSAGE},
    {.number = MESSAGE_AFTER_THE_PLAYER_CHOSE_A_NAME,
     .text = PLAYER_NICKNAME "… Ah! Detto da me" NEW_ROW "suona ridicolo." NEW_PAGE
             "…Mi ci abituerò." END_OF_MESSAGE},
    {.number = MESSAGE_PREGNANCY,
     .text = "Un bambino, eh…" NEW_ROW "Qui le cose si faranno" NEW_ROW "dure." NEW_PAGE
             "…Eh. Non vedo l'ora." END_OF_MESSAGE},
    {.number = MESSAGE_BIRTH,
     .text = "Una creatura così piccola…" NEW_PAGE "Ora devo diventare ancora" NEW_ROW
             "più forte, per proteggerla." NEW_ROW "E per proteggere te." END_OF_MESSAGE},
    {.number = MESSAGE_RAIN_WITH_LITTLE_FRIENDSHIP,
     .text = "Pioggia. La odio." NEW_PAGE "…Hai chiuso la stalla?" NEW_ROW "Neanche agli animali" NEW_ROW
             "piace." END_OF_MESSAGE},
    {.number = MESSAGE_RAIN_WITH_MORE_FRIENDSHIP,
     .text = "Neanche a lei piacevano" NEW_ROW "i giorni di pioggia." NEW_PAGE "…Ora a me non danno più"
             NEW_ROW "così fastidio." NEW_ROW "Strano, eh?" END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_100,
     .text = "Non credere che ti tratterò" NEW_ROW "coi guanti solo perché" NEW_ROW
             "siamo sposati. Ah!" END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_200,
     .text = "Chiedi ancora aiuto ai" NEW_ROW "folletti delle messi?" NEW_PAGE "Uffa. Chiedilo a me." NEW_ROW
             "Me la cavo meglio io." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_300,
     .text = "Avevo giurato che non" NEW_ROW "avrei mai unito le forze" NEW_ROW "con te." NEW_PAGE
             "…E guardaci adesso. Eh." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_400,
     .text = "Pensavo che alla fine" NEW_ROW "si restasse tutti soli." NEW_PAGE
             "…Adesso ho qualche dubbio." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_500,
     .text = "Di nuovo la tua stupida" NEW_ROW "faccia." NEW_PAGE "…Eh. Ci sto facendo" NEW_ROW
             "l'abitudine." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_600,
     .text = "Ho sempre detto che si" NEW_ROW "può contare solo su se" NEW_ROW "stessi." NEW_PAGE
             "Forse mi sbagliavo." END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_700,
     .text = "Mettiamo le cose in" NEW_ROW "chiaro." NEW_PAGE "Se mai sei nei guai," NEW_ROW
             "vieni da me. Capito?" END_OF_MESSAGE},
    {.number = MESSAGE_FRIENDSHIP_BELOW_800,
     .text = "Mi hai insegnato che la" NEW_ROW "gente sa essere gentile." NEW_PAGE "Non mi fido ancora di"
             NEW_ROW "molti. Ma di te sì." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_20_DAYS,
     .text = "Togliti quell'aria" NEW_ROW "felice dalla faccia." NEW_PAGE "…Ehi. Ora sorrido" NEW_ROW
             "anch'io." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_50_DAYS,
     .text = "Oggi riposati." NEW_ROW "Penso io agli animali." NEW_PAGE "Tanto preferiscono me." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_80_DAYS,
     .text = "Mangia qualcosa prima" NEW_ROW "di uscire." NEW_PAGE "Se crolli, tocca a me" NEW_ROW
             "portarti in braccio." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_LESS_THAN_110_DAYS,
     .text = "Una volta ho detto che" NEW_ROW "l'amore è un'illusione." NEW_PAGE "…Non osare" NEW_ROW
             "ricordarmelo." END_OF_MESSAGE},
    {.number = MESSAGE_MARRIED_FOR_110_DAYS_OR_MORE,
     .text = "Al mio arrivo al villaggio" NEW_ROW "non avevo nessuno." NEW_PAGE "Ora ho una casa." NEW_ROW
             "Grazie a te, " PLAYER_NICKNAME "." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_20_DAYS,
     .text = "Il bebè ha pianto tutta" NEW_ROW "la notte. Ehi…" NEW_PAGE "…Però quando dorme è" NEW_ROW
             "adorabile." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_50_DAYS,
     .text = "Oggi " CHILD_NAME " mi ha" NEW_ROW "sorriso." NEW_PAGE "…Eh. I bambini non" NEW_ROW
             "mentono, vero?" END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_80_DAYS,
     .text = CHILD_NAME " ha la tua stupida" NEW_ROW "faccia." NEW_PAGE "…Che fortuna." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_YOUNGER_THAN_110_DAYS,
     .text = "Oggi " CHILD_NAME " ha provato" NEW_ROW "a mungere una mucca." NEW_PAGE "Eh. Ce l'ha nel sangue."
             NEW_ROW "Come me." END_OF_MESSAGE},
    {.number = MESSAGE_CHILD_110_DAYS_OR_OLDER,
     .text = "Ci sono tanti modi di" NEW_ROW "vivere." NEW_PAGE CHILD_NAME " potrà scegliere" NEW_ROW
             "il suo. Noi ci saremo" NEW_ROW "comunque." END_OF_MESSAGE},
    {.number = MESSAGE_BIRTHDAY_PRESENT,
     .text = "Per il mio compleanno?" NEW_PAGE "Era tanto che nessuno mi" NEW_ROW "regalava niente." NEW_PAGE
             "…Grazie." END_OF_MESSAGE},
    {.number = MESSAGE_THE_PLAYER_SHOWS_THE_BABY,
     .text = "Ehi, attenzione!" NEW_ROW "Sostieni la testa." NEW_PAGE "Uffa, quanto sei" NEW_ROW
             "maldestro." END_OF_MESSAGE,
     .text_if_the_player_is_a_girl = "Ehi, attenzione!" NEW_ROW "Sostieni la testa." NEW_PAGE
                                     "Uffa, quanto sei" NEW_ROW "maldestra." END_OF_MESSAGE},
    {.number = MESSAGE_STORM,
     .text = "Senti che vento." NEW_PAGE "Con questo tempo non esci." NEW_ROW "Resta qui." END_OF_MESSAGE},
    {.number = MESSAGE_THE_PLAYERS_BIRTHDAY,
     .text = "Il tuo compleanno, eh?" NEW_ROW "Pensavo che non ci fosse" NEW_ROW "niente da festeggiare." NEW_PAGE
             "…Buon compleanno, idiota." NEW_PAGE "…Scusa. L'abitudine." NEW_ROW
             "Buon compleanno, " PLAYER_NICKNAME "." END_OF_MESSAGE},
};

/*
 * Jamie's lines in the scenes of the pregnancy and the birth, written for this mod. They answer what Jamie
 * says as the rival: that the player will collapse in the end (message 28 of Jamie's file; the French text
 * of the disc says something else there, so in French the line stands on its own), that one can only rely on
 * oneself, that without strength nobody can be protected, the player's silly face, and that Jamie is never
 * grateful for anything. A line for the wife is Jamie as a woman and a line for the husband Jamie as a man,
 * so each one is written in the words of its version.
 */
static const SceneLine english_scene_lines[] = {
    {.scene = SCENE_PREGNANCY_AS_THE_WIFE, .number = PREGNANCY_WIFE_TELLS_THE_NEWS,
     .text = JAMIE_SPEAKS "Don't you dare laugh." NEW_ROW "I always said you'd" NEW_ROW "collapse in the end." NEW_PAGE
             "…And now look at me." NEW_PAGE "The doctor says it's a" NEW_ROW "baby. Ours." NEW_ROW
             "…Hah. Look at your face." END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_WIFE, .number = PREGNANCY_WIFE_LAST_WORDS,
     .text = JAMIE_SPEAKS "Humph. You heard the" NEW_ROW "doctor. Be nice to me." NEW_PAGE "…You can start by" NEW_ROW
             "carrying me home. Hah!" END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_HUSBAND, .number = PREGNANCY_HUSBAND_FINDS_THE_PLAYER,
     .text = JAMIE_SPEAKS "Hey! …Hey! What's wrong?!" NEW_PAGE "Hang on. I'm taking you" NEW_ROW
             "to the clinic. Now!" END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_HUSBAND, .number = PREGNANCY_HUSBAND_HEARS_THE_NEWS,
     .text = JAMIE_SPEAKS "Geeez… Don't scare me" NEW_ROW "like that." NEW_PAGE "I always said you'd" NEW_ROW
             "collapse in the end." NEW_ROW "I never wanted to be right." NEW_PAGE "…A baby, huh." NEW_ROW
             "Heh. A baby." END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_HUSBAND, .number = PREGNANCY_HUSBAND_LAST_WORDS,
     .text = JAMIE_SPEAKS "From now on, the heavy" NEW_ROW "work is mine." NEW_ROW "Don't argue." NEW_PAGE
             "…Just take care of" NEW_ROW "yourself. Both of you." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_IN_LABOR,
     .text = JAMIE_SPEAKS "Nngh…! I'm fine…!" NEW_ROW "I can do this…" NEW_ROW "on my own…!" END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_HOLDS_THE_BABY,
     .text = JAMIE_SPEAKS "Look at this tiny face…" NEW_PAGE "…See? I told you I could" NEW_ROW "do it on my own."
             NEW_PAGE "…That's a lie. I'm glad" NEW_ROW "you were here." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_ASKS_FOR_A_NAME,
     .text = JAMIE_SPEAKS "You choose the name." NEW_ROW "…Nothing silly, got it?" END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_HEARS_THE_NAME,
     .text = JAMIE_SPEAKS "Humph. Not a bad name." NEW_PAGE "…Not that I'm happy or" NEW_ROW "anything."
             END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_DURING_THE_LABOR,
     .text = JAMIE_SPEAKS "H-hey… Hang in there." NEW_ROW "I'm right here." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_SEES_THE_BABY,
     .text = JAMIE_SPEAKS "…It's here. You did it." NEW_PAGE "Heh. Maybe you're" NEW_ROW "stronger than me after all."
             END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_ASKS_FOR_A_NAME,
     .text = JAMIE_SPEAKS "What do we call it?" NEW_PAGE "…You choose. Just" NEW_ROW "nothing silly." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_HEARS_THE_NAME,
     .text = JAMIE_SPEAKS "…Humph. It's a good" NEW_ROW "name." NEW_PAGE "And I'm not crying." NEW_ROW "Got it?"
             END_OF_MESSAGE},
};

static const SceneLine spanish_scene_lines[] = {
    {.scene = SCENE_PREGNANCY_AS_THE_WIFE, .number = PREGNANCY_WIFE_TELLS_THE_NEWS,
     .text = JAMIE_SPEAKS "Ni se te ocurra reírte." NEW_ROW "Siempre dije que al final" NEW_ROW
             "acabarías derrumbándote." NEW_PAGE "...Y mírame ahora." NEW_PAGE "El médico dice que es un" NEW_ROW
             "bebé. Nuestro bebé." NEW_ROW "...Ja. Menuda cara pones." END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_WIFE, .number = PREGNANCY_WIFE_LAST_WORDS,
     .text = JAMIE_SPEAKS "Umm. Ya oíste al médico." NEW_ROW "Tienes que tratarme bien." NEW_PAGE
             "...Para empezar, llévame" NEW_ROW "a casa a cuestas. ¡Ja!" END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_HUSBAND, .number = PREGNANCY_HUSBAND_FINDS_THE_PLAYER,
     .text = JAMIE_SPEAKS "¡Eh! ...¡Eh! ¿Qué te pasa?" NEW_PAGE "Aguanta. Te llevo al" NEW_ROW "hospital. ¡Ya!"
             END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_HUSBAND, .number = PREGNANCY_HUSBAND_HEARS_THE_NEWS,
     .text = JAMIE_SPEAKS "Cielos... No me des esos" NEW_ROW "sustos." NEW_PAGE "Siempre dije que al final" NEW_ROW
             "acabarías derrumbándote." NEW_ROW "No quería tener razón." NEW_PAGE "...Un bebé, ¿eh?" NEW_ROW
             "Eh. Un bebé." END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_HUSBAND, .number = PREGNANCY_HUSBAND_LAST_WORDS,
     .text = JAMIE_SPEAKS "A partir de ahora, el" NEW_ROW "trabajo duro es cosa mía." NEW_ROW "No discutas." NEW_PAGE
             "...Tú solo cuídate." NEW_ROW "Por los dos." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_IN_LABOR,
     .text = JAMIE_SPEAKS "¡Nngh...! ¡Estoy bien...!" NEW_ROW "Puedo hacerlo..." NEW_ROW "¡yo sola...!" END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_HOLDS_THE_BABY,
     .text = JAMIE_SPEAKS "Mira qué carita tan" NEW_ROW "pequeña..." NEW_PAGE "...¿Ves? Te dije que podía" NEW_ROW
             "hacerlo yo sola." NEW_PAGE "...Es mentira. Me alegro" NEW_ROW "de que estuvieras aquí."
             END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_ASKS_FOR_A_NAME,
     .text = JAMIE_SPEAKS "Elige tú el nombre." NEW_ROW "...Nada ridículo, ¿eh?" END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_HEARS_THE_NAME,
     .text = JAMIE_SPEAKS "Umm. No está mal." NEW_PAGE "...Y no es que esté" NEW_ROW "contenta ni nada."
             END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_DURING_THE_LABOR,
     .text = JAMIE_SPEAKS "E-eh... Aguanta." NEW_ROW "Estoy aquí." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_SEES_THE_BABY,
     .text = JAMIE_SPEAKS "...Ya está. Lo lograste." NEW_PAGE "Eh. Puede que seas más" NEW_ROW "fuerte que yo."
             END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_ASKS_FOR_A_NAME,
     .text = JAMIE_SPEAKS "¿Cómo lo llamamos?" NEW_PAGE "...Elige tú. Pero nada" NEW_ROW "ridículo." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_HEARS_THE_NAME,
     .text = JAMIE_SPEAKS "...Umm. Es un buen" NEW_ROW "nombre." NEW_PAGE "Y no estoy llorando." NEW_ROW
             "¿Entendido?" END_OF_MESSAGE},
};

static const SceneLine french_scene_lines[] = {
    {.scene = SCENE_PREGNANCY_AS_THE_WIFE, .number = PREGNANCY_WIFE_TELLS_THE_NEWS,
     .text = JAMIE_SPEAKS "N'essaie même pas de rire." NEW_PAGE "J'ai toujours dit que tu" NEW_ROW
             "finirais par t'effondrer." NEW_ROW "…Et regarde-moi." NEW_PAGE "Le docteur dit que c'est" NEW_ROW
             "un bébé. Le nôtre." NEW_ROW "…Ha ! Si tu voyais ta tête." END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_WIFE, .number = PREGNANCY_WIFE_LAST_WORDS,
     .text = JAMIE_SPEAKS "Humf. Tu as entendu le" NEW_ROW "docteur. Veille sur moi." NEW_PAGE
             "…Commence par me porter" NEW_ROW "jusqu'à la maison. Ha !" END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_HUSBAND, .number = PREGNANCY_HUSBAND_FINDS_THE_PLAYER,
     .text = JAMIE_SPEAKS "Hé ! …Hé ! Ça ne va pas ?" NEW_PAGE "Tiens bon. Je t'emmène à" NEW_ROW "la clinique. Vite !"
             END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_HUSBAND, .number = PREGNANCY_HUSBAND_HEARS_THE_NEWS,
     .text = JAMIE_SPEAKS "Rahhh… Ne me fais plus" NEW_ROW "peur comme ça." NEW_PAGE "J'ai toujours dit que tu" NEW_ROW
             "finirais par t'effondrer." NEW_ROW "Je voulais avoir tort." NEW_PAGE "…Un bébé, hein." NEW_ROW
             "Hé. Un bébé." END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_HUSBAND, .number = PREGNANCY_HUSBAND_LAST_WORDS,
     .text = JAMIE_SPEAKS "À partir de maintenant," NEW_ROW "les gros travaux, c'est" NEW_ROW "moi. Ne discute pas."
             NEW_PAGE "…Prends juste soin de" NEW_ROW "toi. De vous deux." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_IN_LABOR,
     .text = JAMIE_SPEAKS "Nngh… ! Ça va… !" NEW_ROW "Je peux y arriver…" NEW_ROW "toute seule… !" END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_HOLDS_THE_BABY,
     .text = JAMIE_SPEAKS "Regarde ce tout petit" NEW_ROW "visage…" NEW_PAGE "…Tu vois ? Je t'avais dit" NEW_ROW
             "que j'y arriverais seule." NEW_PAGE "…C'est faux. Je suis" NEW_ROW "contente que tu sois là."
             END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_ASKS_FOR_A_NAME,
     .text = JAMIE_SPEAKS "Choisis le prénom." NEW_ROW "…Rien de ridicule, hein ?" END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_HEARS_THE_NAME,
     .text = JAMIE_SPEAKS "Humf. Ce n'est pas mal." NEW_PAGE "…Pas que ça me fasse" NEW_ROW "plaisir, hein."
             END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_DURING_THE_LABOR,
     .text = JAMIE_SPEAKS "H-hé… Tiens bon." NEW_ROW "Je suis là." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_SEES_THE_BABY,
     .text = JAMIE_SPEAKS "…Il est là. Tu as réussi." NEW_PAGE "Hé. Tu es peut-être plus" NEW_ROW
             "forte que moi, après tout." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_ASKS_FOR_A_NAME,
     .text = JAMIE_SPEAKS "On l'appelle comment ?" NEW_PAGE "…Choisis, toi. Mais rien" NEW_ROW "de ridicule."
             END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_HEARS_THE_NAME,
     .text = JAMIE_SPEAKS "…Humf. C'est un beau" NEW_ROW "prénom." NEW_PAGE "Et je ne pleure pas." NEW_ROW "Compris ?"
             END_OF_MESSAGE},
};

static const SceneLine german_scene_lines[] = {
    {.scene = SCENE_PREGNANCY_AS_THE_WIFE, .number = PREGNANCY_WIFE_TELLS_THE_NEWS,
     .text = JAMIE_SPEAKS "Wehe, du lachst." NEW_PAGE "Ich hab immer gesagt, dass" NEW_ROW
             "du am Ende zusammenbrichst." NEW_ROW "… Und jetzt sieh mich an." NEW_PAGE "Der Arzt sagt, es ist ein"
             NEW_ROW "Baby. Unser Baby." NEW_ROW "… Ha. Dein Gesicht!" END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_WIFE, .number = PREGNANCY_WIFE_LAST_WORDS,
     .text = JAMIE_SPEAKS "Pöh. Du hast den Arzt" NEW_ROW "gehört. Sei gut zu mir." NEW_PAGE "… Fang damit an, mich"
             NEW_ROW "nach Hause zu tragen. Ha!" END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_HUSBAND, .number = PREGNANCY_HUSBAND_FINDS_THE_PLAYER,
     .text = JAMIE_SPEAKS "He! … He! Was ist los?" NEW_PAGE "Halt durch. Ich bring dich" NEW_ROW
             "in die Klinik. Sofort!" END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_HUSBAND, .number = PREGNANCY_HUSBAND_HEARS_THE_NEWS,
     .text = JAMIE_SPEAKS "Oje … Erschreck mich nie" NEW_ROW "wieder so." NEW_PAGE "Ich hab immer gesagt, dass" NEW_ROW
             "du am Ende zusammenbrichst." NEW_ROW "Ich wollte nie recht haben." NEW_PAGE "… Ein Baby, was?" NEW_ROW
             "Ha. Ein Baby." END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_HUSBAND, .number = PREGNANCY_HUSBAND_LAST_WORDS,
     .text = JAMIE_SPEAKS "Ab jetzt mache ich die" NEW_ROW "schwere Arbeit." NEW_ROW "Keine Widerrede." NEW_PAGE
             "… Pass einfach auf dich" NEW_ROW "auf. Auf euch beide." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_IN_LABOR,
     .text = JAMIE_SPEAKS "Nngh …! Geht schon …!" NEW_ROW "Ich schaff das …" NEW_ROW "allein …!" END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_HOLDS_THE_BABY,
     .text = JAMIE_SPEAKS "Sieh dir dieses winzige" NEW_ROW "Gesicht an …" NEW_PAGE "… Siehst du? Ich hab doch" NEW_ROW
             "gesagt, ich schaff das" NEW_ROW "allein." NEW_PAGE "… Gelogen. Ich bin froh," NEW_ROW
             "dass du da warst." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_ASKS_FOR_A_NAME,
     .text = JAMIE_SPEAKS "Such du den Namen aus." NEW_ROW "… Nichts Albernes, klar?" END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_HEARS_THE_NAME,
     .text = JAMIE_SPEAKS "Pöh. Kein schlechter Name." NEW_PAGE "… Nicht, dass ich mich" NEW_ROW "freue oder so."
             END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_DURING_THE_LABOR,
     .text = JAMIE_SPEAKS "H-he … Halt durch." NEW_ROW "Ich bin hier." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_SEES_THE_BABY,
     .text = JAMIE_SPEAKS "… Es ist da. Du hast es" NEW_ROW "geschafft." NEW_PAGE "Ha. Vielleicht bist du doch" NEW_ROW
             "stärker als ich." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_ASKS_FOR_A_NAME,
     .text = JAMIE_SPEAKS "Wie nennen wir es?" NEW_PAGE "… Such du aus. Nur nichts" NEW_ROW "Albernes." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_HEARS_THE_NAME,
     .text = JAMIE_SPEAKS "… Pöh. Das ist ein guter" NEW_ROW "Name." NEW_PAGE "Und ich weine nicht." NEW_ROW "Klar?"
             END_OF_MESSAGE},
};

static const SceneLine italian_scene_lines[] = {
    {.scene = SCENE_PREGNANCY_AS_THE_WIFE, .number = PREGNANCY_WIFE_TELLS_THE_NEWS,
     .text = JAMIE_SPEAKS "Non azzardarti a ridere." NEW_PAGE "Ho sempre detto che alla" NEW_ROW
             "fine saresti crollato." NEW_ROW "…E guarda me, adesso." NEW_PAGE "Il dottore dice che aspetto" NEW_ROW
             "un bambino. Il nostro." NEW_ROW "…Ah! Guarda che faccia." END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_WIFE, .number = PREGNANCY_WIFE_LAST_WORDS,
     .text = JAMIE_SPEAKS "Uffa. Hai sentito il" NEW_ROW "dottore. Trattami bene." NEW_PAGE "…Puoi cominciare" NEW_ROW
             "portandomi a casa in" NEW_ROW "braccio. Ah!" END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_HUSBAND, .number = PREGNANCY_HUSBAND_FINDS_THE_PLAYER,
     .text = JAMIE_SPEAKS "Ehi! …Ehi! Che ti succede?" NEW_PAGE "Resisti. Ti porto in" NEW_ROW "clinica. Subito!"
             END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_HUSBAND, .number = PREGNANCY_HUSBAND_HEARS_THE_NEWS,
     .text = JAMIE_SPEAKS "Uffa… Non spaventarmi" NEW_ROW "così." NEW_PAGE "Ho sempre detto che alla" NEW_ROW
             "fine saresti crollata." NEW_ROW "Non volevo avere ragione." NEW_PAGE "…Un bambino, eh." NEW_ROW
             "Eh. Un bambino." END_OF_MESSAGE},
    {.scene = SCENE_PREGNANCY_AS_THE_HUSBAND, .number = PREGNANCY_HUSBAND_LAST_WORDS,
     .text = JAMIE_SPEAKS "D'ora in poi, i lavori" NEW_ROW "pesanti li faccio io." NEW_ROW "Non discutere." NEW_PAGE
             "…Pensa solo a riguardarti." NEW_ROW "Per tutti e due." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_IN_LABOR,
     .text = JAMIE_SPEAKS "Nngh…! Sto bene…!" NEW_ROW "Ce la faccio…" NEW_ROW "da sola…!" END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_HOLDS_THE_BABY,
     .text = JAMIE_SPEAKS "Guarda che faccino" NEW_ROW "minuscolo…" NEW_PAGE "…Visto? Te l'avevo detto" NEW_ROW
             "che ce l'avrei fatta da" NEW_ROW "sola." NEW_PAGE "…Non è vero. Sono contenta" NEW_ROW
             "che tu fossi qui." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_ASKS_FOR_A_NAME,
     .text = JAMIE_SPEAKS "Scegli tu il nome." NEW_ROW "…Niente di ridicolo, eh?" END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_WIFE, .number = BIRTH_WIFE_HEARS_THE_NAME,
     .text = JAMIE_SPEAKS "Uffa. Non è un brutto" NEW_ROW "nome." NEW_PAGE "…Non che io sia contenta" NEW_ROW
             "o roba del genere." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_DURING_THE_LABOR,
     .text = JAMIE_SPEAKS "E-ehi… Resisti." NEW_ROW "Sono qui." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_SEES_THE_BABY,
     .text = JAMIE_SPEAKS "…Eccolo. Ce l'hai fatta." NEW_PAGE "Eh. Forse sei più forte" NEW_ROW "di me, dopotutto."
             END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_ASKS_FOR_A_NAME,
     .text = JAMIE_SPEAKS "Come lo chiamiamo?" NEW_PAGE "…Scegli tu. Ma niente di" NEW_ROW "ridicolo." END_OF_MESSAGE},
    {.scene = SCENE_BIRTH_AS_THE_HUSBAND, .number = BIRTH_HUSBAND_HEARS_THE_NAME,
     .text = JAMIE_SPEAKS "…Uffa. È un bel nome." NEW_PAGE "E non sto piangendo." NEW_ROW "Chiaro?" END_OF_MESSAGE},
};

/* The languages of the disc's message folders (/message_e/ and so on), with Jamie's lines in each one. */
static const SpouseLanguage spouse_languages[] = {
    {'e', english_spouse_messages, HM_MM_ARRAY_COUNT(english_spouse_messages), english_scene_lines,
     HM_MM_ARRAY_COUNT(english_scene_lines)},
    {'s', spanish_spouse_messages, HM_MM_ARRAY_COUNT(spanish_spouse_messages), spanish_scene_lines,
     HM_MM_ARRAY_COUNT(spanish_scene_lines)},
    {'f', french_spouse_messages, HM_MM_ARRAY_COUNT(french_spouse_messages), french_scene_lines,
     HM_MM_ARRAY_COUNT(french_scene_lines)},
    {'g', german_spouse_messages, HM_MM_ARRAY_COUNT(german_spouse_messages), german_scene_lines,
     HM_MM_ARRAY_COUNT(german_scene_lines)},
    {'i', italian_spouse_messages, HM_MM_ARRAY_COUNT(italian_spouse_messages), italian_scene_lines,
     HM_MM_ARRAY_COUNT(italian_scene_lines)},
};

static uint16_t read_half_word(CPUState* cpu, uint32_t address)
{
    return (uint16_t)moderngekko_mod_read(cpu, address, HALF_WORD_BYTES);
}

static void write_half_word(CPUState* cpu, uint32_t address, uint16_t value)
{
    moderngekko_mod_write(cpu, address, value, HALF_WORD_BYTES);
}

/* The language whose message folder has this letter (/message_e/ is 'e'), or NULL without Jamie's lines. */
static const SpouseLanguage* language_with_folder_letter(char folder_letter)
{
    for (uint32_t index = 0; index < HM_MM_ARRAY_COUNT(spouse_languages); ++index) {
        if (spouse_languages[index].folder_letter == folder_letter) {
            return &spouse_languages[index];
        }
    }
    return NULL;
}

/*
 * The language of a character's messages, from the folder of the file the game loads for them
 * ("/message_e/chara/Rion_e.bin" is English), or NULL for a folder without Jamie's lines.
 */
static const SpouseLanguage* language_of_the_messages_of(CPUState* cpu, uint32_t character_number)
{
    const uint32_t path = read_word(cpu, MESSAGE_FILE_PATHS_ADDRESS + character_number * WORD_BYTES);
    if (path == 0) {
        return NULL;
    }
    const uint32_t prefix_length = (uint32_t)(sizeof MESSAGE_FOLDER_PREFIX - 1);
    for (uint32_t index = 0; index < prefix_length; ++index) {
        if (read_byte(cpu, path + index) != (uint8_t)MESSAGE_FOLDER_PREFIX[index]) {
            return NULL;
        }
    }
    const char folder_letter = (char)read_byte(cpu, path + prefix_length);
    if (read_byte(cpu, path + prefix_length + 1) != (uint8_t)FOLDER_SEPARATOR) {
        return NULL;
    }
    return language_with_folder_letter(folder_letter);
}

/* Jamie's line for a message number, in the words for the player's gender, or NULL if there is none. */
static const char* spouse_text(const SpouseLanguage* language, uint32_t message_number, bool player_is_a_girl)
{
    for (uint32_t index = 0; index < language->message_count; ++index) {
        const SpouseMessage* message = &language->messages[index];
        if (message->number != message_number) {
            continue;
        }
        if (player_is_a_girl && message->text_if_the_player_is_a_girl != NULL) {
            return message->text_if_the_player_is_a_girl;
        }
        return message->text;
    }
    return NULL;
}

/* The byte of the console's font for a letter (a Unicode code point), or NOT_IN_THE_FONT. */
static int font_byte_for(uint32_t letter)
{
    if (letter == '?') {
        return FONT_QUESTION_MARK;
    }
    if (letter <= ASCII_LAST) {
        return (int)letter;
    }
    if (letter >= UNICODE_LATIN_1_FIRST && letter <= UNICODE_LATIN_1_LAST) {
        return (int)letter;
    }
    switch (letter) {
    case UNICODE_CAPITAL_OE:
        return FONT_CAPITAL_OE;
    case UNICODE_SMALL_OE:
        return FONT_SMALL_OE;
    case UNICODE_ELLIPSIS:
        return FONT_ELLIPSIS;
    default:
        return NOT_IN_THE_FONT;
    }
}

/*
 * Reads the UTF-8 letter at the start of `text` into `letter` and gives its size in bytes (1 to 3, which
 * covers every letter of the font), or 0 when the bytes are not such a letter.
 */
static size_t read_utf8_letter(const unsigned char* text, uint32_t* letter)
{
    const unsigned char first = text[0];
    if (first < 0x80u) {
        *letter = first;
        return 1;
    }
    if ((first & 0xE0u) == 0xC0u && (text[1] & 0xC0u) == 0x80u) {
        *letter = ((uint32_t)(first & 0x1Fu) << 6) | (uint32_t)(text[1] & 0x3Fu);
        return 2;
    }
    if ((first & 0xF0u) == 0xE0u && (text[1] & 0xC0u) == 0x80u && (text[2] & 0xC0u) == 0x80u) {
        *letter = ((uint32_t)(first & 0x0Fu) << 12) | ((uint32_t)(text[1] & 0x3Fu) << 6) |
                  (uint32_t)(text[2] & 0x3Fu);
        return 3;
    }
    return 0;
}

/*
 * Turns one of the texts above into the game's bytes. The window codes are copied as they are. Gives the
 * number of bytes, or 0 when a letter is not in the font or the text does not fit in `capacity` (the test
 * keeps the texts of this file clear of both).
 */
static size_t encode_for_the_game(const char* text, uint8_t* bytes, size_t capacity)
{
    const unsigned char* cursor = (const unsigned char*)text;
    size_t length = 0;
    while (*cursor != '\0') {
        if (*cursor == WINDOW_CODE) {
            if (cursor[1] == '\0' || length + WINDOW_CODE_BYTES > capacity) {
                return 0;
            }
            bytes[length++] = cursor[0];
            bytes[length++] = cursor[1];
            cursor += WINDOW_CODE_BYTES;
            continue;
        }
        uint32_t letter = 0;
        const size_t letter_size = read_utf8_letter(cursor, &letter);
        const int font_byte = letter_size == 0 ? NOT_IN_THE_FONT : font_byte_for(letter);
        if (font_byte == NOT_IN_THE_FONT || length == capacity) {
            return 0;
        }
        bytes[length++] = (uint8_t)font_byte;
        cursor += letter_size;
    }
    return length;
}

/*
 * Puts a message into the copy of a message file that the game has just loaded, under its number. The text
 * goes at the start of the file's text, over messages that are not shown this time; when the number is
 * above the file's highest one, its table entry falls inside the text, so the text goes right after it,
 * and the file's highest number becomes this one. Jamie's file has 4776 bytes of text, far more than any
 * message here, and the game frees the copy as soon as the message is shown.
 */
static void put_the_message_in_the_loaded_file(CPUState* cpu, uint32_t file, uint32_t message_number,
                                               const uint8_t* bytes, size_t length)
{
    const uint32_t highest_message = read_half_word(cpu, file + MESSAGE_FILE_HEADER_HIGHEST_MESSAGE);
    const uint32_t table = file + read_word(cpu, file + MESSAGE_FILE_HEADER_TABLE_START);
    const uint32_t text = file + read_word(cpu, file + MESSAGE_FILE_HEADER_TEXT_START);
    const uint32_t table_entry = table + message_number * WORD_BYTES;

    uint32_t destination = text;
    if (table_entry + WORD_BYTES > destination) {
        destination = table_entry + WORD_BYTES;
    }
    for (size_t index = 0; index < length; ++index) {
        write_byte(cpu, destination + (uint32_t)index, bytes[index]);
    }
    write_word(cpu, table_entry, destination - text);
    if (message_number > highest_message) {
        write_half_word(cpu, file + MESSAGE_FILE_HEADER_HIGHEST_MESSAGE, (uint16_t)message_number);
    }
}

/*
 * The message chosen when fn_8008534C starts, waiting for the file it loads (pending_message_length is 0
 * when there is none). Between the start of fn_8008534C and the loading of its file the game does nothing
 * else, so the next file that is loaded is that one.
 */
static uint32_t pending_message_number;
static uint8_t pending_message_bytes[LONGEST_SPOUSE_MESSAGE_BYTES];
static size_t pending_message_length;

/*
 * fn_8008534C(window, character, message, ...): shows a message of a character's file. This runs on its
 * entry and chooses Jamie's line when Jamie is the spouse. Running twice for the same call chooses the same.
 */
static void before_a_character_message_is_shown(CPUState* cpu)
{
    pending_message_length = 0;
    const uint32_t character_number = moderngekko_mod_arg_u32(cpu, 1);
    const uint32_t message_number = moderngekko_mod_arg_u32(cpu, 2);
    const uint32_t save_data = read_word(cpu, SAVE_DATA_POINTER_ADDRESS);
    const bool jamie_is_the_spouse = save_data != NO_SAVE_DATA &&
                                     read_word(cpu, save_data + SPOUSE_OFFSET) == JAMIE_CHARACTER_NUMBER;
    if (character_number != JAMIE_CHARACTER_NUMBER || !jamie_is_the_spouse) {
        return;
    }
    const SpouseLanguage* language = language_of_the_messages_of(cpu, character_number);
    if (language == NULL) {
        return;
    }
    const bool player_is_a_girl = read_word(cpu, save_data + PLAYER_CHARACTER_OFFSET) != PLAYER_CHARACTER_THE_BOY;
    const char* text = spouse_text(language, message_number, player_is_a_girl);
    if (text == NULL) {
        return;
    }
    pending_message_number = message_number;
    pending_message_length = encode_for_the_game(text, pending_message_bytes, sizeof pending_message_bytes);
}

/*
 * Called when fn_80029E74 has loaded a file (see after_a_file_is_loaded): while a line is waiting, the file
 * is the one that fn_8008534C asked for.
 */
static void give_the_loaded_file_the_spouse_line(CPUState* cpu, uint32_t file)
{
    if (pending_message_length == 0) {
        return;
    }
    const size_t length = pending_message_length;
    pending_message_length = 0;
    if (file == 0) {
        return;
    }
    put_the_message_in_the_loaded_file(cpu, file, pending_message_number, pending_message_bytes, length);
    HM_MM_LOG(MOD_ID, "gave Jamie spouse message %u", (unsigned int)pending_message_number);
}

/* When fn_8008534C returns: nothing stays waiting, even if it returned before loading its file. */
static void after_a_character_message_is_shown(CPUState* cpu)
{
    (void)cpu;
    pending_message_length = 0;
}

/*
 * Babies (see the comment at the top, and docs/SCRIPTS.md, "El embarazo y el parto").
 *
 * The field check (fn_80166888) chooses its checks by the area just entered, through the table of cases at
 * 0x802A341C; every house of the player uses the case at 0x801668D0. That case first tests main flag 0x1A4,
 * which the game sets every morning (fn_80032EE8) and clears once the player goes out (fn_801649FC). While
 * the flag is set the case runs the morning checks, the birth among them (from 0x8016809C): the pregnancy
 * has gone on for 60 days or more and it is not the wedding anniversary. While it is clear the case goes to
 * the pregnancy check (0x80169410): the player came in from the farm, has been married for 20 days or more,
 * the counter at save data + 0xD508 (one more each day of marriage) has reached 10, and the spouse's
 * friendship is 900 or more. Each check then goes through a chain of blocks, one for each partner: "is the
 * spouse 3? then start event 0xC8 (Eve_conce00); is it 4? then 0xC9 (Eve_conce01)..." up to 22. Jamie has no
 * block in either chain.
 *
 * ModernGekko runs a hook where the recompiled code hands control back to its dispatcher, which happens on a
 * branch or a call from one chunk of the code to another (DolRecomp cuts each code section into chunks of 4096
 * instructions, 16 KB, from the start of the section: 0x800184C0 for this one); a branch within a chunk is a
 * plain jump and runs no hook (seen in the game: the blocks of the chains never ran theirs). So the mod
 * acts at points that do run: the pregnancy check's first instruction, reached from the chunk before it; the
 * entry and the return of the field check; and the entry of fn_800FC2B4, which starts an event. When Jamie
 * is the spouse and every condition of a check holds for Jamie, the mod writes the number of the partner
 * whose scene Jamie borrows as the spouse (for the pregnancy, also Jamie's friendship as the partner's, which
 * the check reads by the spouse's number); the partner's block starts the scene and the field check returns.
 * fn_800FC2B4 starts an event only if events are allowed and the event has not started before; otherwise it
 * does nothing, and the check that asked goes on to the next one. While Jamie stands in, the only event that
 * may start is the scene's: if another one is about to start, or the scene's will not, Jamie is the spouse
 * again at once, before anything else reads the spouse. An event that will not start changes nothing, so
 * Jamie goes on standing in (the morning checks ask for several events seen long ago, such as 0x100, 0x109
 * and 0x1A, before they reach the birth's chain). The return of the field check, the shared end of its checks
 * (0x8016D55C) and the next area give Jamie back too. Between the field check's entry and its birth chain
 * nothing reads the spouse, so the birth's stand-in can start at the entry. Jamie borrows partner 4's scenes
 * as the wife and partner 13's as the husband.
 */
#define PREGNANCY_CHECK_ADDRESS 0x80169410u   /* the first instruction of the pregnancy check */
#define END_OF_THE_FIELD_CHECKS_ADDRESS 0x8016D55Cu /* where the field check goes when no check started an event */
#define FIELD_CHECK_CASES_ADDRESS 0x802A341Cu /* a word for each area: where the field check goes for it */
#define LAST_AREA_WITH_A_CASE 0x7Cu
#define HOUSE_CASE_ADDRESS 0x801668D0u
#define CURRENT_AREA_ADDRESS 0x80321D40u
#define FLAG_MORNING_CHECKS 0x1A4u
#define EVENTS_ALLOWED_ADDRESS 0x80321DC0u /* fn_800FC2B4 starts an event only while this bit is set */
#define EVENTS_ALLOWED_BIT 0x8u

#define DAYS_MARRIED_OFFSET 0xCB54u
#define DAYS_OF_MARRIAGE_BEFORE_A_PREGNANCY 20u
#define DAILY_MARRIAGE_COUNTER_OFFSET 0xD508u
#define DAILY_MARRIAGE_COUNT_BEFORE_A_PREGNANCY 10u
#define FRIENDSHIP_OFFSET 0x6628u /* a counter of 12 bytes for each character number */
#define FRIENDSHIP_BYTES_EACH 12u
#define FRIENDSHIP_BEFORE_A_PREGNANCY 900u
#define PREGNANCY_DAYS_OFFSET 0xCB60u /* value 43 of the scripts: 0, then the days of the pregnancy, 255 once born */
#define DAYS_OF_PREGNANCY_BEFORE_THE_BIRTH 60u

#define PARTNER_JAMIE_STANDS_IN_FOR_AS_THE_WIFE 4u
#define PARTNER_JAMIE_STANDS_IN_FOR_AS_THE_HUSBAND 13u
#define EVENT_PREGNANCY_OF_PARTNER_4 0xC9u       /* Eve_conce01 */
#define EVENT_PREGNANCY_WITH_PARTNER_13 0xD2u    /* Eve_conce10 */
#define EVENT_BIRTH_OF_PARTNER_4 0xDDu           /* Eve_child01 */
#define EVENT_BIRTH_WITH_PARTNER_13 0xE6u        /* Eve_child10 */

/*
 * When the birth starts, the block sets bit 5 of the spouse's character bits at an address of its own
 * (0x5570 for partner 4): talk.lsb then shows the spouse's message 11, about the birth, on the next
 * conversation, and clears the bit. With Jamie standing in, the bit lands on the partner, so the mod moves it
 * to Jamie. Nothing else in the game sets that bit on a partner who is not the spouse.
 */
#define CHARACTER_BIT_TALKS_ABOUT_THE_BIRTH 5u

/*
 * The scene's script is a file of /Script/, loaded by fn_80055068 through fn_80029E74 when the event begins.
 * Where it names the partner, it pushes the number with PUSH_INT (opcode 0x02, the number in the next byte):
 * as the actor of a built-in, as the actor of one of the script's own functions, or as the speaker of the
 * next line. tools/script_actors.py finds those places; these are the file offsets of the bytes that hold
 * the number, and the test compares them with what the tool finds on the disc. Before writing Jamie's number
 * the mod checks that every one of those bytes holds the partner's number after a PUSH_INT, so a script that
 * is not the one expected is left as it is.
 */
#define PUSH_INT_OPCODE 0x02u

static const uint16_t pregnancy_as_the_wife_actor_bytes[] = { /* Eve_conce01, partner 4 */
    0x05EC, 0x064C, 0x0655, 0x065E, 0x0694, 0x06AD, 0x081B, 0x0847,
    0x0893, 0x08D2, 0x08DD, 0x0A9C, 0x0AF2, 0x0AFD,
};

static const uint16_t pregnancy_as_the_husband_actor_bytes[] = { /* Eve_conce10, partner 13 */
    0x0608, 0x0637, 0x06D5, 0x0710, 0x0739, 0x0744, 0x076A, 0x077A,
    0x0785, 0x079B, 0x07BE, 0x07C5, 0x07D5, 0x07E7, 0x07F6, 0x0804,
    0x084C, 0x0857, 0x0886, 0x08CB, 0x0977, 0x0987, 0x09AD, 0x09D7,
    0x09F5, 0x0A42, 0x0A57, 0x0A6A, 0x0A89, 0x0A94, 0x0AF3,
};

static const uint16_t birth_as_the_wife_actor_bytes[] = { /* Eve_child01, partner 4 */
    0x0375, 0x03DE, 0x03E7, 0x03F0, 0x0423, 0x042F, 0x0440, 0x0488,
    0x04BD, 0x0868, 0x090A, 0x0913, 0x092E, 0x09FB, 0x0A2B, 0x0A41,
    0x0A86, 0x0ABD, 0x0AD3, 0x0BBC, 0x0C35, 0x0C3E, 0x0C59, 0x0CA1,
    0x0CBA, 0x0CC9,
};

static const uint16_t birth_as_the_husband_actor_bytes[] = { /* Eve_child10, partner 13 */
    0x03AF, 0x0403, 0x0455, 0x04B3, 0x0512, 0x051D, 0x053B, 0x0574,
    0x0588, 0x059A, 0x05A7, 0x05B2, 0x05E4, 0x05FA, 0x061F, 0x064F,
    0x066F, 0x0694, 0x06A6, 0x06B1, 0x06C2, 0x06CD, 0x06E3, 0x06EE,
    0x06FB, 0x0706, 0x071C, 0x0727, 0x0730, 0x073F, 0x074A, 0x0760,
    0x076B, 0x0778, 0x0783, 0x0799, 0x07A4, 0x07AD, 0x07CA, 0x0849,
    0x0858, 0x0863, 0x0876, 0x0881, 0x088A, 0x0895, 0x08A0, 0x08AF,
    0x08B8, 0x08C7, 0x08DC, 0x090E, 0x0940, 0x0A07, 0x0A12, 0x0A1E,
    0x0A27, 0x0A3D, 0x0A48, 0x0A54, 0x0A69, 0x0A8D, 0x0A98, 0x0AA4,
    0x0AB8, 0x0B09, 0x0B14, 0x0B26, 0x0C11, 0x0C42, 0x0C62, 0x0D63,
    0x0D6E, 0x0D7A, 0x0D83, 0x0D8E, 0x0DBC, 0x0DC8,
};

/*
 * A scene that Jamie borrows. Its message file stays loaded for the whole event (fn_800FC2B4 loads it when
 * the event starts and keeps it at event manager + 0xB0), and it holds the lines of every partner of the
 * same kind, so Jamie's lines go over the text of messages that this scene never shows: from
 * first_unused_message to the message before first_message_after_the_unused_ones, which are in a row in the
 * file (the test checks that they fit in every language of the disc).
 */
typedef struct JamieScene {
    const char* name;            /* for the log */
    bool jamie_is_the_wife;      /* the version for a male player */
    bool is_the_birth;           /* the birth's chain, or else the pregnancy's */
    uint32_t partner;            /* the partner whose event Jamie borrows */
    uint32_t event;              /* that partner's event in the chain */
    const char* script_name;     /* the event's script: /Script/<name>.lsb */
    const char* message_file;    /* the event's messages: /message_<language>/event/<name>_<language>.bin */
    const uint16_t* actor_bytes; /* the bytes of the script that hold the partner's number */
    uint32_t actor_byte_count;
    uint32_t first_unused_message;
    uint32_t first_message_after_the_unused_ones;
} JamieScene;

static const JamieScene jamie_scenes[JAMIE_SCENE_COUNT] = {
    [SCENE_PREGNANCY_AS_THE_WIFE] = {"the pregnancy", true, false, PARTNER_JAMIE_STANDS_IN_FOR_AS_THE_WIFE,
                                     EVENT_PREGNANCY_OF_PARTNER_4, "Eve_conce01", "Eve_conce00",
                                     pregnancy_as_the_wife_actor_bytes,
                                     HM_MM_ARRAY_COUNT(pregnancy_as_the_wife_actor_bytes), 5u, 13u},
    [SCENE_PREGNANCY_AS_THE_HUSBAND] = {"the pregnancy", false, false, PARTNER_JAMIE_STANDS_IN_FOR_AS_THE_HUSBAND,
                                        EVENT_PREGNANCY_WITH_PARTNER_13, "Eve_conce10", "Eve_conce10",
                                        pregnancy_as_the_husband_actor_bytes,
                                        HM_MM_ARRAY_COUNT(pregnancy_as_the_husband_actor_bytes), 16u, 30u},
    [SCENE_BIRTH_AS_THE_WIFE] = {"the birth", true, true, PARTNER_JAMIE_STANDS_IN_FOR_AS_THE_WIFE,
                                 EVENT_BIRTH_OF_PARTNER_4, "Eve_child01", "Eve_child00",
                                 birth_as_the_wife_actor_bytes, HM_MM_ARRAY_COUNT(birth_as_the_wife_actor_bytes),
                                 12u, 36u},
    [SCENE_BIRTH_AS_THE_HUSBAND] = {"the birth", false, true, PARTNER_JAMIE_STANDS_IN_FOR_AS_THE_HUSBAND,
                                    EVENT_BIRTH_WITH_PARTNER_13, "Eve_child10", "Eve_child10",
                                    birth_as_the_husband_actor_bytes,
                                    HM_MM_ARRAY_COUNT(birth_as_the_husband_actor_bytes), 9u, 36u},
};

/*
 * The stand-in in progress: the scene, and for the pregnancy the partner's own friendship, which Jamie's
 * replaces while the check reads it (scene is NULL when Jamie stands in for nobody).
 */
static struct {
    const JamieScene* scene;
    bool friendship_replaced;
    uint32_t partners_own_friendship;
} stand_in;

static uint32_t character_bits_of(uint32_t save_data, uint32_t character_number)
{
    return save_data + CHARACTER_BITS_OFFSET + CHARACTER_BITS_BYTES_EACH * character_number;
}

static uint32_t friendship_of(uint32_t save_data, uint32_t character_number)
{
    return save_data + FRIENDSHIP_OFFSET + FRIENDSHIP_BYTES_EACH * character_number;
}

static bool player_is_the_boy(CPUState* cpu, uint32_t save_data)
{
    return read_word(cpu, save_data + PLAYER_CHARACTER_OFFSET) == PLAYER_CHARACTER_THE_BOY;
}

/* The scene of this kind (the pregnancy or the birth) in the version for the player's gender. */
static const JamieScene* scene_for_the_player(CPUState* cpu, uint32_t save_data, bool the_birth)
{
    const bool jamie_is_the_wife = player_is_the_boy(cpu, save_data);
    if (the_birth) {
        return &jamie_scenes[jamie_is_the_wife ? SCENE_BIRTH_AS_THE_WIFE : SCENE_BIRTH_AS_THE_HUSBAND];
    }
    return &jamie_scenes[jamie_is_the_wife ? SCENE_PREGNANCY_AS_THE_WIFE : SCENE_PREGNANCY_AS_THE_HUSBAND];
}

/*
 * Whether this scene is Jamie's: Jamie is the spouse (or stands in for the scene's partner right now), and
 * the scene is the version for the player's gender.
 */
static bool scene_is_jamies(CPUState* cpu, const JamieScene* scene)
{
    const uint32_t save_data = read_word(cpu, SAVE_DATA_POINTER_ADDRESS);
    if (save_data == NO_SAVE_DATA || scene->jamie_is_the_wife != player_is_the_boy(cpu, save_data)) {
        return false;
    }
    const uint32_t spouse = read_word(cpu, save_data + SPOUSE_OFFSET);
    return spouse == JAMIE_CHARACTER_NUMBER || (scene == stand_in.scene && spouse == scene->partner);
}

/* Whether fn_800FC2B4 would start this event now: events are allowed, and this one has not started before. */
static bool event_would_start(CPUState* cpu, uint32_t save_data, uint32_t event)
{
    return (read_word(cpu, EVENTS_ALLOWED_ADDRESS) & EVENTS_ALLOWED_BIT) != 0 &&
           !is_bit_set(cpu, save_data + STARTED_EVENTS_OFFSET, event);
}

/*
 * Jamie becomes the scene's partner in the save data, for the chain that is about to run. For the pregnancy,
 * the partner's friendship becomes Jamie's while the check reads it.
 */
static void stand_in_for_the_partner(CPUState* cpu, uint32_t save_data, const JamieScene* scene)
{
    stand_in.scene = scene;
    stand_in.friendship_replaced = !scene->is_the_birth;
    if (stand_in.friendship_replaced) {
        stand_in.partners_own_friendship = read_word(cpu, friendship_of(save_data, scene->partner));
        write_word(cpu, friendship_of(save_data, scene->partner),
                   read_word(cpu, friendship_of(save_data, JAMIE_CHARACTER_NUMBER)));
    }
    write_word(cpu, save_data + SPOUSE_OFFSET, scene->partner);
    HM_MM_LOG(MOD_ID, "Jamie stands in for partner %u to start %s", (unsigned int)scene->partner, scene->name);
}

/* Jamie is the spouse again in the save data, and the partner has its own friendship back. */
static void give_jamie_back(CPUState* cpu)
{
    const JamieScene* scene = stand_in.scene;
    if (scene == NULL) {
        return;
    }
    stand_in.scene = NULL;
    const uint32_t save_data = read_word(cpu, SAVE_DATA_POINTER_ADDRESS);
    if (save_data == NO_SAVE_DATA) {
        return;
    }
    if (stand_in.friendship_replaced) {
        write_word(cpu, friendship_of(save_data, scene->partner), stand_in.partners_own_friendship);
    }
    if (read_word(cpu, save_data + SPOUSE_OFFSET) == scene->partner) {
        write_word(cpu, save_data + SPOUSE_OFFSET, JAMIE_CHARACTER_NUMBER);
    }
    HM_MM_LOG(MOD_ID, "Jamie is the spouse again after %s", scene->name);
}

/* Whether Jamie is the spouse in this save data and stands in for nobody yet: the start of every stand-in. */
static bool jamie_is_the_spouse_and_free(CPUState* cpu, uint32_t save_data)
{
    return save_data != NO_SAVE_DATA && stand_in.scene == NULL &&
           read_word(cpu, save_data + SPOUSE_OFFSET) == JAMIE_CHARACTER_NUMBER;
}

/*
 * 0x80169410, the first instruction of the pregnancy check. The check itself makes sure the player came in
 * from the farm; if that fails, it goes to the end of the field checks without reading the spouse, so only
 * the conditions after it are checked here, as the game checks them.
 */
static void at_the_start_of_the_pregnancy_check(CPUState* cpu)
{
    const uint32_t save_data = read_word(cpu, SAVE_DATA_POINTER_ADDRESS);
    if (!jamie_is_the_spouse_and_free(cpu, save_data)) {
        return; /* a hook may run twice: the second time Jamie already stands in */
    }
    const JamieScene* scene = scene_for_the_player(cpu, save_data, false);
    const bool conditions_hold =
        read_word(cpu, save_data + DAYS_MARRIED_OFFSET) >= DAYS_OF_MARRIAGE_BEFORE_A_PREGNANCY &&
        read_word(cpu, save_data + DAILY_MARRIAGE_COUNTER_OFFSET) >= DAILY_MARRIAGE_COUNT_BEFORE_A_PREGNANCY &&
        read_word(cpu, friendship_of(save_data, JAMIE_CHARACTER_NUMBER)) >= FRIENDSHIP_BEFORE_A_PREGNANCY &&
        event_would_start(cpu, save_data, scene->event);
    if (conditions_hold) {
        stand_in_for_the_partner(cpu, save_data, scene);
    }
}

/*
 * The entry of the field check, for the birth: in a house, with the morning checks on, after 60 days of
 * pregnancy and on a day that is not the wedding anniversary. The checks that run before the birth's chain
 * do not read the spouse; if one of them starts an event first, fn_800FC2B4 gives Jamie back.
 */
static void before_the_field_check_stand_in_for_the_birth(CPUState* cpu)
{
    const uint32_t save_data = read_word(cpu, SAVE_DATA_POINTER_ADDRESS);
    if (!jamie_is_the_spouse_and_free(cpu, save_data)) {
        return;
    }
    const uint32_t area = read_word(cpu, CURRENT_AREA_ADDRESS);
    if (area > LAST_AREA_WITH_A_CASE ||
        read_word(cpu, FIELD_CHECK_CASES_ADDRESS + area * WORD_BYTES) != HOUSE_CASE_ADDRESS ||
        !is_bit_set(cpu, save_data + MAIN_FLAGS_OFFSET, FLAG_MORNING_CHECKS)) {
        return;
    }
    const uint32_t pregnancy_days = read_word(cpu, save_data + PREGNANCY_DAYS_OFFSET);
    const bool anniversary =
        read_word(cpu, save_data + SEASON_OFFSET) == read_word(cpu, save_data + WEDDING_SEASON_OFFSET) &&
        read_word(cpu, save_data + DAY_OF_THE_SEASON_OFFSET) == read_word(cpu, save_data + WEDDING_DAY_OFFSET);
    const JamieScene* scene = scene_for_the_player(cpu, save_data, true);
    if (pregnancy_days >= DAYS_OF_PREGNANCY_BEFORE_THE_BIRTH && !anniversary &&
        !is_bit_set(cpu, save_data + STARTED_EVENTS_OFFSET, scene->event)) {
        stand_in_for_the_partner(cpu, save_data, scene);
    }
}

/*
 * fn_800FC2B4(event manager, event): starts an event, or does nothing if it will not start. While Jamie stands
 * in, the event that starts must be the scene's; Jamie is the spouse again before the call goes on when
 * another event is about to start, or when the scene's will not.
 */
static void before_an_event_starts(CPUState* cpu)
{
    const JamieScene* scene = stand_in.scene;
    if (scene == NULL) {
        return;
    }
    const uint32_t save_data = read_word(cpu, SAVE_DATA_POINTER_ADDRESS);
    if (save_data == NO_SAVE_DATA) {
        give_jamie_back(cpu);
        return;
    }
    const uint32_t event = moderngekko_mod_arg_u32(cpu, 1);
    const bool will_start = event_would_start(cpu, save_data, event);
    const bool another_event_starts = event != scene->event && will_start;
    const bool the_scene_will_not_start = event == scene->event && !will_start;
    if (another_event_starts || the_scene_will_not_start) {
        give_jamie_back(cpu);
    }
}

/* The bit of the birth's conversation, if the birth block set it on the partner, goes to Jamie. */
static void give_jamie_the_birth_conversation(CPUState* cpu)
{
    const uint32_t save_data = read_word(cpu, SAVE_DATA_POINTER_ADDRESS);
    if (save_data == NO_SAVE_DATA || read_word(cpu, save_data + SPOUSE_OFFSET) != JAMIE_CHARACTER_NUMBER) {
        return;
    }
    const uint32_t partner = player_is_the_boy(cpu, save_data) ? PARTNER_JAMIE_STANDS_IN_FOR_AS_THE_WIFE
                                                               : PARTNER_JAMIE_STANDS_IN_FOR_AS_THE_HUSBAND;
    const uint32_t partner_bits = character_bits_of(save_data, partner);
    if (!is_bit_set(cpu, partner_bits, CHARACTER_BIT_TALKS_ABOUT_THE_BIRTH)) {
        return;
    }
    clear_bit(cpu, partner_bits, CHARACTER_BIT_TALKS_ABOUT_THE_BIRTH);
    set_bit(cpu, character_bits_of(save_data, JAMIE_CHARACTER_NUMBER), CHARACTER_BIT_TALKS_ABOUT_THE_BIRTH);
    HM_MM_LOG(MOD_ID, "Jamie, not partner %u, is to talk about the birth", (unsigned int)partner);
}

/*
 * Everything the stand-in leaves behind, put right. It runs where the chains are surely over: when the field
 * check returns and at the shared end of its checks, and as a safety net when the next one starts and when an
 * area is entered (fn_80042D10), which happens before anybody can be talked to.
 */
static void settle_the_stand_in(CPUState* cpu)
{
    give_jamie_back(cpu);
    give_jamie_the_birth_conversation(cpu);
}

/*
 * The paths that fn_80029E74 receives. The disc's file names do not depend on case (the game itself asks for
 * "Eve_conce00_e.bin" where the disc says "eve_conce00_e.bin"), so the comparisons ignore it.
 */
#define LONGEST_PATH 96u
#define SCRIPT_FOLDER "/Script/"
#define SCRIPT_EXTENSION ".lsb"
#define EVENT_FOLDER "event/"
#define LANGUAGE_SEPARATOR '_'
#define MESSAGE_FILE_EXTENSION ".bin"

/* Copies the path at `address` into `path`; false when it does not fit, which no path of the mod's does. */
static bool read_path(CPUState* cpu, uint32_t address, char* path, size_t capacity)
{
    for (size_t index = 0; index < capacity; ++index) {
        path[index] = (char)read_byte(cpu, address + (uint32_t)index);
        if (path[index] == '\0') {
            return true;
        }
    }
    return false;
}

static char lowercase(char letter)
{
    return letter >= 'A' && letter <= 'Z' ? (char)(letter - 'A' + 'a') : letter;
}

/* What follows `prefix` in `text`, ignoring case, or NULL when `text` does not start with it. */
static const char* after_prefix(const char* text, const char* prefix)
{
    if (text == NULL) {
        return NULL;
    }
    for (; *prefix != '\0'; ++prefix, ++text) {
        if (lowercase(*text) != lowercase(*prefix)) {
            return NULL;
        }
    }
    return text;
}

static bool is_the_script_of(const char* path, const JamieScene* scene)
{
    const char* rest = after_prefix(after_prefix(after_prefix(path, SCRIPT_FOLDER), scene->script_name),
                                    SCRIPT_EXTENSION);
    return rest != NULL && *rest == '\0';
}

/* Whether `path` is the scene's message file in some language; its folder letter goes to `folder_letter`. */
static bool is_the_message_file_of(const char* path, const JamieScene* scene, char* folder_letter)
{
    const char* rest = after_prefix(path, MESSAGE_FOLDER_PREFIX);
    if (rest == NULL || rest[0] == '\0' || rest[1] != FOLDER_SEPARATOR) {
        return false;
    }
    const char letter = rest[0];
    rest = after_prefix(after_prefix(rest + 2, EVENT_FOLDER), scene->message_file);
    if (rest == NULL || rest[0] != LANGUAGE_SEPARATOR || lowercase(rest[1]) != lowercase(letter)) {
        return false;
    }
    rest = after_prefix(rest + 2, MESSAGE_FILE_EXTENSION);
    if (rest == NULL || *rest != '\0') {
        return false;
    }
    *folder_letter = letter;
    return true;
}

/* Writes Jamie's number over the partner's in the scene's script that the game has just loaded. */
static void put_jamie_in_the_loaded_script(CPUState* cpu, uint32_t file, const JamieScene* scene)
{
    for (uint32_t index = 0; index < scene->actor_byte_count; ++index) {
        const uint32_t number_byte = file + scene->actor_bytes[index];
        if (read_byte(cpu, number_byte) != scene->partner || read_byte(cpu, number_byte - 1) != PUSH_INT_OPCODE) {
            HM_MM_LOG(MOD_ID, "%s.lsb is not the script this mod knows: left as it is", scene->script_name);
            return;
        }
    }
    for (uint32_t index = 0; index < scene->actor_byte_count; ++index) {
        write_byte(cpu, file + scene->actor_bytes[index], (uint8_t)JAMIE_CHARACTER_NUMBER);
    }
    HM_MM_LOG(MOD_ID, "put Jamie in %s (%s.lsb, %u places)", scene->name, scene->script_name,
              (unsigned int)scene->actor_byte_count);
}

/*
 * Puts Jamie's lines of the scene, in the file's language, over the text of the messages that the scene
 * never shows, and points their numbers there. Stops at a line that would not fit, leaving it the game's.
 */
static void put_jamies_lines_in_the_loaded_messages(CPUState* cpu, uint32_t file, JamieSceneId scene_id,
                                                    const SpouseLanguage* language)
{
    const JamieScene* scene = &jamie_scenes[scene_id];
    const uint32_t highest_message = read_half_word(cpu, file + MESSAGE_FILE_HEADER_HIGHEST_MESSAGE);
    if (scene->first_message_after_the_unused_ones > highest_message) {
        return;
    }
    const uint32_t table = file + read_word(cpu, file + MESSAGE_FILE_HEADER_TABLE_START);
    const uint32_t text = file + read_word(cpu, file + MESSAGE_FILE_HEADER_TEXT_START);
    uint32_t destination = text + read_word(cpu, table + scene->first_unused_message * WORD_BYTES);
    const uint32_t end_of_the_room =
        text + read_word(cpu, table + scene->first_message_after_the_unused_ones * WORD_BYTES);
    uint32_t lines_written = 0;
    for (uint32_t index = 0; index < language->scene_line_count; ++index) {
        const SceneLine* line = &language->scene_lines[index];
        if (line->scene != scene_id || line->number > highest_message) {
            continue;
        }
        uint8_t bytes[LONGEST_SPOUSE_MESSAGE_BYTES];
        const size_t length = encode_for_the_game(line->text, bytes, sizeof bytes);
        if (length == 0 || destination + length > end_of_the_room) {
            break;
        }
        for (size_t byte = 0; byte < length; ++byte) {
            write_byte(cpu, destination + (uint32_t)byte, bytes[byte]);
        }
        write_word(cpu, table + line->number * WORD_BYTES, destination - text);
        destination += (uint32_t)length;
        ++lines_written;
    }
    HM_MM_LOG(MOD_ID, "gave Jamie %u lines in the messages of %s", (unsigned int)lines_written, scene->name);
}

/* What the file that fn_80029E74 is loading is to receive, decided from its path when the load starts. */
typedef enum SceneFileKind { NOT_A_SCENE_FILE, SCENE_SCRIPT, SCENE_MESSAGES } SceneFileKind;

static struct {
    SceneFileKind kind;
    JamieSceneId scene;
    const SpouseLanguage* language; /* for SCENE_MESSAGES */
} pending_scene_file;

/* fn_80029E74(path), on its entry: is the file the script or the messages of one of Jamie's scenes? */
static void before_a_file_is_loaded(CPUState* cpu)
{
    pending_scene_file.kind = NOT_A_SCENE_FILE;
    char path[LONGEST_PATH];
    if (!read_path(cpu, moderngekko_mod_arg_u32(cpu, 0), path, sizeof path)) {
        return;
    }
    for (uint32_t scene_id = 0; scene_id < JAMIE_SCENE_COUNT; ++scene_id) {
        const JamieScene* scene = &jamie_scenes[scene_id];
        char folder_letter = '\0';
        if (is_the_script_of(path, scene)) {
            if (scene_is_jamies(cpu, scene)) {
                pending_scene_file.kind = SCENE_SCRIPT;
                pending_scene_file.scene = (JamieSceneId)scene_id;
            }
            return;
        }
        if (is_the_message_file_of(path, scene, &folder_letter)) {
            const SpouseLanguage* language = language_with_folder_letter(folder_letter);
            if (language != NULL && scene_is_jamies(cpu, scene)) {
                pending_scene_file.kind = SCENE_MESSAGES;
                pending_scene_file.scene = (JamieSceneId)scene_id;
                pending_scene_file.language = language;
            }
            return;
        }
    }
}

/*
 * fn_80029E74(path): loads a whole file into a new buffer and returns its address. This runs when it
 * returns, and gives the file what was decided for it: Jamie in a scene's script, Jamie's lines in a scene's
 * messages, or a spouse line in Jamie's own file.
 */
static void after_a_file_is_loaded(CPUState* cpu)
{
    const uint32_t file = cpu->gpr[3];
    const SceneFileKind kind = pending_scene_file.kind;
    pending_scene_file.kind = NOT_A_SCENE_FILE;
    if (kind == SCENE_SCRIPT && file != 0) {
        put_jamie_in_the_loaded_script(cpu, file, &jamie_scenes[pending_scene_file.scene]);
    } else if (kind == SCENE_MESSAGES && file != 0) {
        put_jamies_lines_in_the_loaded_messages(cpu, file, pending_scene_file.scene, pending_scene_file.language);
    }
    give_the_loaded_file_the_spouse_line(cpu, file);
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
    RECOMP_HOOK(DOLRECOMP_SYMBOL_fn_80166888, settle_the_stand_in),
    RECOMP_HOOK(DOLRECOMP_SYMBOL_fn_80166888, clear_the_ending_flag_after_the_wedding),
    RECOMP_HOOK(DOLRECOMP_SYMBOL_fn_80166888, before_the_field_check_stand_in_for_the_birth),
    RECOMP_HOOK_RETURN(DOLRECOMP_SYMBOL_fn_80166888, settle_the_stand_in),
    RECOMP_HOOK(PREGNANCY_CHECK_ADDRESS, at_the_start_of_the_pregnancy_check),
    RECOMP_HOOK(END_OF_THE_FIELD_CHECKS_ADDRESS, settle_the_stand_in),
    RECOMP_HOOK(DOLRECOMP_SYMBOL_fn_800FC2B4, before_an_event_starts),
    RECOMP_HOOK(DOLRECOMP_SYMBOL_fn_80042D10, settle_the_stand_in),
    RECOMP_HOOK(DOLRECOMP_SYMBOL_fn_80042D10, before_the_characters_are_placed),
    RECOMP_HOOK(DOLRECOMP_SYMBOL_fn_8008534C, before_a_character_message_is_shown),
    RECOMP_HOOK_RETURN(DOLRECOMP_SYMBOL_fn_8008534C, after_a_character_message_is_shown),
    RECOMP_HOOK(DOLRECOMP_SYMBOL_fn_80029E74, before_a_file_is_loaded),
    RECOMP_HOOK_RETURN(DOLRECOMP_SYMBOL_fn_80029E74, after_a_file_is_loaded),
};

static const ModernGekkoModDesc descriptor = {
    .abi_version = MODERNGEKKO_MOD_ABI_VERSION,
    .cpu_abi_version = MODERNGEKKO_CPU_ABI_VERSION,
    .cpu_state_size = sizeof(CPUState),
    .game_id = HM_MM_GAME_ID,
    .id = MOD_ID,
    .version = "0.7.0",
    .display_name = "Jamie marriage does not end the game",
    .hooks = hooks,
    .num_hooks = HM_MM_ARRAY_COUNT(hooks),
};

MODERNGEKKO_MOD_EXPORT const ModernGekkoModDesc* moderngekko_get_mod(void)
{
    return &descriptor;
}
