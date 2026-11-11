/*
 * player_speed: the player's character walks and runs faster.
 * player_speed: el personaje del jugador camina y corre más rápido.
 *
 * How the game moves the character (found by watching, in the running game, which function writes the
 * character's position and which values feed it):
 *
 *   - Every frame the game updates each actor (a character, an animal, an object) with fn_8001AC20. Only the
 *     player's actor goes on to fn_80049D28, the player's own update. That update reads the stick and keeps
 *     two things in the actor: a direction (a vector at actor + 0x44) and a speed (a float at actor + 0x58).
 *     The speed grows by 0.2 each frame up to a cap (3.0 walking, 5.0 running) and falls again when the
 *     stick is released.
 *   - Near the end of fn_8001AC20 the game stores direction * speed as the velocity of the actor's physics
 *     node (the node is the word at actor + 0x30; its velocity is three floats at node + 0x98, in the
 *     actor's own frame, where z is "forward").
 *   - The next frame, after fn_80049D28 has run, the game moves the node by that velocity times the time step.
 *
 * So a frame goes: fn_80049D28 -> the node moves by the velocity stored one frame ago -> the game stores the
 * new velocity (direction * speed) for the next frame.
 *
 * This mod hooks the entry of fn_80049D28. At that moment the node still holds exactly what the game stored
 * one frame ago, so the mod multiplies it by the chosen factor, and the move that follows is longer by that
 * factor. At the end of the frame the game stores the plain direction * speed again, so the factor is applied
 * once per frame and never builds up. Nothing is remembered between frames, which also means that it does not
 * matter when the game loads a save, changes area or starts a new day.
 *
 * Why not something simpler. The caps 3.0 and 5.0 are constants that the game shares with unrelated code, so
 * changing them would change other things too, and not how fast the speed grows. The direction is written
 * once when the character starts to walk and kept for the whole walk, so scaling it would scale it again on
 * every frame. The speed is read by other code besides the physics.
 *
 * The mod only touches a velocity that it recognises: one that is, bit for bit, the single-precision product
 * of the actor's direction and speed (the multiplication the game itself does). If something else has set the
 * velocity (a push, a knock-back), it is left alone. Only the horizontal components are scaled: the vertical
 * one belongs to falling. The same check makes the mod safe when ModernGekko runs a hook more than once for the
 * same call, which can happen (see mods/README.md): once the velocity has been multiplied it is no longer equal
 * to direction * speed, so a second run leaves it alone.
 *
 * It only acts in free play (the game's main state is 0). In events, such as the cutscenes, scripts walk the
 * character to a place and the other characters move at the same time; a faster character could overshoot
 * the place or leave the others behind.
 *
 * How much faster is chosen with the environment variable HM_MM_PLAYER_SPEED_PERCENT, a whole number: 100 is
 * the speed of the original game, 150 is one and a half times as fast. When the variable is not set the mod
 * uses DEFAULT_SPEED_PERCENT. packaging/windows/play.ps1 sets the variable from its -PlayerSpeed option.
 * The character's animation is not changed, so at high values the feet may look as if they slide.
 *
 * The highest value is MAXIMUM_SPEED_PERCENT. A faster character takes a longer step on every frame, and the
 * game's walls were made for the steps of the original speed. At 400 the character went through the closed
 * door of the player's house, in every one of the six tries, and fell out of the map; at 300 it did not in
 * six tries (docs/MOVEMENT.md). Thin walls elsewhere in the game were not tried, so values above 200 are the
 * player's own risk.
 *
 * The hook runs about once per frame, because only the player's actor reaches the hooked function. In the
 * cloud its cost could not be told apart from the variation between two runs of the same thing, which is
 * more than 10 % (docs/MOVEMENT.md); it was not measured on Windows.
 *
 * Cómo mueve el juego al personaje (se vio en el juego en ejecución, mirando qué función escribe la posición
 * del personaje y qué valores la alimentan):
 *
 *   - En cada cuadro el juego actualiza a cada actor (un personaje, un animal, un objeto) con fn_8001AC20.
 *     Solo el actor del jugador pasa además a fn_80049D28, la actualización propia del jugador. Esa
 *     actualización lee el stick y guarda dos cosas en el actor: una dirección (un vector en actor + 0x44) y
 *     una velocidad escalar (un float en actor + 0x58). La velocidad escalar sube 0.2 por cuadro hasta un
 *     tope (3.0 caminando, 5.0 corriendo) y vuelve a bajar cuando se suelta el stick.
 *   - Cerca del final de fn_8001AC20 el juego guarda dirección * velocidad escalar como la velocidad del nodo
 *     de física del actor (el nodo es la palabra en actor + 0x30; su velocidad son tres floats en
 *     nodo + 0x98, en el marco del propio actor, donde z es «hacia adelante»).
 *   - El cuadro siguiente, después de que corre fn_80049D28, el juego mueve el nodo según esa velocidad por el
 *     paso de tiempo.
 *
 * Así que un cuadro va así: fn_80049D28 -> el nodo se mueve con la velocidad guardada hace un cuadro -> el
 * juego guarda la nueva velocidad (dirección * velocidad escalar) para el cuadro siguiente.
 *
 * Este mod engancha la entrada de fn_80049D28. En ese momento el nodo todavía tiene exactamente lo que el
 * juego guardó hace un cuadro, así que el mod lo multiplica por el factor elegido y el movimiento que sigue
 * resulta más largo por ese factor. Al final del cuadro el juego vuelve a guardar la dirección * velocidad
 * escalar sin tocar, así que el factor se aplica una vez por cuadro y nunca se acumula. No se recuerda nada
 * entre cuadros, así que tampoco importa cuando el juego carga una partida, cambia de área o empieza un día.
 *
 * Por qué no algo más sencillo. Los topes 3.0 y 5.0 son constantes que el juego comparte con código que no
 * tiene que ver, así que cambiarlos cambiaría también otras cosas, y no cómo crece la velocidad escalar. La
 * dirección se escribe una vez cuando el personaje empieza a caminar y se conserva durante toda la caminata,
 * así que escalarla la escalaría otra vez en cada cuadro. La velocidad escalar la leen otras partes además de
 * la física.
 *
 * El mod solo toca una velocidad que reconoce: la que es, bit por bit, el producto en precisión simple de la
 * dirección y la velocidad escalar del actor (la multiplicación que hace el propio juego). Si otra cosa puso
 * la velocidad (un empujón, un retroceso), la deja como está. Solo se escalan las componentes horizontales: la
 * vertical es de la caída. La misma comprobación hace que el mod sea seguro cuando ModernGekko corre un gancho
 * más de una vez por la misma llamada, cosa que puede pasar (ver mods/README.md): una vez multiplicada, la
 * velocidad ya no es igual a dirección * velocidad escalar, así que una segunda pasada no la toca.
 *
 * Solo actúa en juego libre (el estado principal del juego es 0). En los eventos, como las escenas, los
 * scripts llevan al personaje a un lugar y los demás personajes se mueven al mismo tiempo; un personaje más
 * rápido podría pasarse del lugar o dejar atrás a los demás.
 *
 * Cuánto más rápido se elige con la variable de entorno HM_MM_PLAYER_SPEED_PERCENT, un número entero: 100 es
 * la velocidad del juego original y 150 es una vez y media más rápido. Si la variable no está definida, el
 * mod usa DEFAULT_SPEED_PERCENT. packaging/windows/play.ps1 define la variable con su opción -PlayerSpeed. La
 * animación del personaje no cambia, así que con valores altos los pies podrían verse resbalar.
 *
 * El valor más alto es MAXIMUM_SPEED_PERCENT. Un personaje más rápido da un paso más largo en cada cuadro, y
 * las paredes del juego se hicieron para los pasos de la velocidad original. Con 400 el personaje atravesó la
 * puerta cerrada de la casa del jugador, en los seis intentos, y cayó fuera del mapa; con 300 no pasó en
 * seis intentos (docs/MOVEMENT.md). No se probaron las paredes delgadas del resto del juego, así que los
 * valores por encima de 200 son bajo el riesgo del jugador.
 *
 * El gancho corre más o menos una vez por cuadro, porque solo el actor del jugador llega a la función
 * enganchada. En la nube su costo no se distinguió de la variación entre dos corridas de lo mismo, que es
 * de más del 10 % (docs/MOVEMENT.md); no se midió en Windows.
 */

#include <stdlib.h>
#include <string.h>

#include "hm_mm_mod.h"

#define MOD_ID "player_speed"

/*
 * How much faster the character is, in percent of the original game's speed: 100 changes nothing. The player
 * chooses it with this environment variable, a whole number between the two limits.
 */
#define SPEED_PERCENT_VARIABLE "HM_MM_PLAYER_SPEED_PERCENT"
#define DEFAULT_SPEED_PERCENT 100
#define MINIMUM_SPEED_PERCENT 25
#define MAXIMUM_SPEED_PERCENT 300
#define PERCENT_OF_ORIGINAL_SPEED 100.0f

#define WORD_BYTES 4
#define BYTE_BYTES 1
#define NO_ADDRESS 0u

/*
 * The game's state machine keeps its current state in this word (docs/SCRIPTS.md). It is 0 in free play, 4
 * while an event runs, and other values in the transitions (the day change, the field check).
 */
#define MAIN_STATE_ADDRESS 0x80321D18u
#define MAIN_STATE_FREE_PLAY 0u

/* fn_80049D28(actor, input, time_step): the player's update. Its first argument is the player's actor. */
#define PLAYER_UPDATE_FUNCTION DOLRECOMP_SYMBOL_fn_80049D28

/* Where the things that the mod reads are in an actor. The type byte is 1 for the player. */
#define ACTOR_PHYSICS_NODE_OFFSET 0x30u
#define ACTOR_DIRECTION_X_OFFSET 0x44u
#define ACTOR_DIRECTION_Z_OFFSET 0x4Cu
#define ACTOR_SPEED_OFFSET 0x58u
#define ACTOR_TYPE_OFFSET 0x94u
#define ACTOR_TYPE_PLAYER 1u

/* Where the velocity is in a physics node (x is sideways, y is vertical, z is forward). */
#define NODE_VELOCITY_X_OFFSET 0x98u
#define NODE_VELOCITY_Z_OFFSET 0xA0u

/* The factor that the hook applies. choose_the_speed_multiplier sets it when the mod is loaded. */
static float speed_multiplier = (float)DEFAULT_SPEED_PERCENT / PERCENT_OF_ORIGINAL_SPEED;

static uint32_t read_word(CPUState* cpu, uint32_t address)
{
    return (uint32_t)moderngekko_mod_read(cpu, address, WORD_BYTES);
}

static float bits_to_float(uint32_t bits)
{
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static uint32_t float_to_bits(float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    return bits;
}

static float read_float(CPUState* cpu, uint32_t address)
{
    return bits_to_float(read_word(cpu, address));
}

static void write_float(CPUState* cpu, uint32_t address, float value)
{
    moderngekko_mod_write(cpu, address, float_to_bits(value), WORD_BYTES);
}

/*
 * One horizontal component of the node's velocity. If it still holds what the game stored (the actor's
 * direction times its speed, which the game computes in single precision) it is multiplied by the chosen
 * factor. Any other value was put there by something else, and stays as it is.
 */
static void scale_velocity_component(CPUState* cpu, uint32_t direction_address, float speed, uint32_t velocity_address)
{
    const float direction = read_float(cpu, direction_address);
    const float velocity_the_game_stored = direction * speed;
    if (read_word(cpu, velocity_address) != float_to_bits(velocity_the_game_stored)) {
        return;
    }
    write_float(cpu, velocity_address, velocity_the_game_stored * speed_multiplier);
}

/*
 * fn_80049D28(actor, input, time_step): the player's update. This runs when the game enters it, before the
 * node is moved by the velocity that the game stored one frame ago.
 */
static void make_the_player_faster(CPUState* cpu)
{
    const uint32_t actor = moderngekko_mod_arg_u32(cpu, 0);
    if (read_word(cpu, MAIN_STATE_ADDRESS) != MAIN_STATE_FREE_PLAY) {
        return;
    }
    if (moderngekko_mod_read(cpu, actor + ACTOR_TYPE_OFFSET, BYTE_BYTES) != ACTOR_TYPE_PLAYER) {
        return;
    }
    const uint32_t physics_node = read_word(cpu, actor + ACTOR_PHYSICS_NODE_OFFSET);
    if (physics_node == NO_ADDRESS) {
        return;
    }

    const float speed = read_float(cpu, actor + ACTOR_SPEED_OFFSET);
    scale_velocity_component(cpu, actor + ACTOR_DIRECTION_X_OFFSET, speed, physics_node + NODE_VELOCITY_X_OFFSET);
    scale_velocity_component(cpu, actor + ACTOR_DIRECTION_Z_OFFSET, speed, physics_node + NODE_VELOCITY_Z_OFFSET);
}

/*
 * The percentage that the player asked for in the environment variable, or DEFAULT_SPEED_PERCENT when the
 * variable is not set, is not a whole number, or is outside the limits. It is read as a whole number, not as a
 * decimal, so that the language and region settings of the computer cannot change how it is understood.
 */
static long read_requested_speed_percent(void)
{
    const char* text = getenv(SPEED_PERCENT_VARIABLE);
    if (text == NULL || text[0] == '\0') {
        return DEFAULT_SPEED_PERCENT;
    }
    char* end_of_number = NULL;
    const long percent = strtol(text, &end_of_number, 10);
    if (*end_of_number != '\0' || percent < MINIMUM_SPEED_PERCENT || percent > MAXIMUM_SPEED_PERCENT) {
        HM_MM_LOG(MOD_ID, "%s=\"%s\" is not a whole number from %d to %d: using %d",
                  SPEED_PERCENT_VARIABLE, text, MINIMUM_SPEED_PERCENT, MAXIMUM_SPEED_PERCENT,
                  DEFAULT_SPEED_PERCENT);
        return DEFAULT_SPEED_PERCENT;
    }
    return percent;
}

/* Called once when the mod is loaded, before the game runs. */
static void choose_the_speed_multiplier(const ModernGekkoModHostApi* host_api)
{
    (void)host_api; /* The mod does not call back into the runner. */
    const long speed_percent = read_requested_speed_percent();
    speed_multiplier = (float)speed_percent / PERCENT_OF_ORIGINAL_SPEED;
    HM_MM_LOG(MOD_ID, "the player moves at %ld%% of the original speed", speed_percent);
}

static const ModernGekkoModHook hooks[] = {
    RECOMP_HOOK(PLAYER_UPDATE_FUNCTION, make_the_player_faster),
};

static const ModernGekkoModDesc descriptor = {
    .abi_version = MODERNGEKKO_MOD_ABI_VERSION,
    .cpu_abi_version = MODERNGEKKO_CPU_ABI_VERSION,
    .cpu_state_size = sizeof(CPUState),
    .game_id = HM_MM_GAME_ID,
    .id = MOD_ID,
    .version = "0.1.0",
    .display_name = "Faster walking and running",
    .hooks = hooks,
    .num_hooks = HM_MM_ARRAY_COUNT(hooks),
    .on_load = choose_the_speed_multiplier,
};

MODERNGEKKO_MOD_EXPORT const ModernGekkoModDesc* moderngekko_get_mod(void)
{
    return &descriptor;
}
