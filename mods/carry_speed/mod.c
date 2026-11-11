/*
 * carry_speed: the character does not slow down when carrying something or holding a tool.
 * carry_speed: el personaje no se frena al cargar algo o traer una herramienta.
 *
 * Why the character is slower with something in the hands (found in the game's code and watched in the
 * running game):
 *
 *   - The player's update (fn_80049D28) is a state machine on the halfword at actor + 0xC8. With empty hands the
 *     character walks in state 8 (the speed grows to 3.0) and, if the stick stays fully pushed (54.5 or more, the
 *     float at input + 0x1C; 72 is the most), the game moves it to state 0xC, running (the speed keeps growing to
 *     5.0). That change is made only from state 8, and state 0xC is entered from nowhere else.
 *   - With something in the hands the character is in other states: the game has a copy of the walking states
 *     for each way of holding things (0x25 to 0x28, 0x38 to 0x3B and 0x46), which run the same walking code with
 *     the same top speed of 3.0, but none of them can change to running. So with something in the hands the
 *     character never goes faster than 3.0, while empty-handed it reaches 5.0.
 *
 * What this mod does: in those holding-walk states, with the stick fully pushed, it makes the velocity 5.0 / 3.0
 * times longer, which is what the character would move with if it ran. The way to do it is the same as in
 * player_speed (docs/MOVEMENT.md): the game stores direction * speed as the velocity of the physics node at the end
 * of each frame and moves the node with it in the next one, so at the entry of fn_80049D28 the node still holds
 * what the game stored and the move that follows uses the longer velocity. Nothing is remembered between frames.
 *
 * It acts after player_speed when both are on (the runtime runs the mods in the order of NATIVE_MODS), so it
 * works on a velocity that player_speed already made longer: it recognises a velocity that points the same way
 * as direction * speed, not only one that is equal to it. A velocity that points elsewhere (a push, a
 * knock-back) is left alone. Only the horizontal components are scaled, and only in free play (main state 0).
 * With half the stick, the character still walks, as an empty-handed one does.
 *
 * Por qué el personaje va más lento con algo en las manos (se vio en el código del juego y en el juego en
 * ejecución):
 *
 *   - La actualización del jugador (fn_80049D28) es una máquina de estados sobre la media palabra en
 *     actor + 0xC8. Con las manos vacías el personaje camina en el estado 8 (la velocidad crece hasta 3.0) y, si
 *     el stick sigue a fondo (54.5 o más, el float en input + 0x1C; 72 es lo máximo), el juego lo pasa al estado
 *     0xC, correr (la velocidad sigue creciendo hasta 5.0). Ese cambio se hace solo desde el estado 8, y al
 *     estado 0xC no se entra desde ningún otro.
 *   - Con algo en las manos el personaje está en otros estados: el juego tiene una copia de los estados de
 *     caminar para cada forma de cargar cosas (0x25 a 0x28, 0x38 a 0x3B y 0x46), que corren el mismo código de
 *     caminar con el mismo tope de 3.0, pero ninguno puede pasar a correr. Así que con algo en las manos el
 *     personaje nunca va más rápido que 3.0, y con las manos vacías llega a 5.0.
 *
 * Lo que hace este mod: en esos estados de caminar cargando, con el stick a fondo, alarga la velocidad 5.0 / 3.0
 * veces, que es con lo que se movería el personaje si corriera. Se hace igual que en player_speed
 * (docs/MOVEMENT.md): al final de cada cuadro el juego guarda dirección * velocidad escalar como la velocidad del
 * nodo de física y mueve el nodo con ella en el cuadro siguiente, así que a la entrada de fn_80049D28 el nodo
 * todavía tiene lo que guardó el juego y el movimiento que sigue usa la velocidad más larga. No se recuerda
 * nada entre cuadros.
 *
 * Actúa después de player_speed cuando los dos están activos (el runtime corre los mods en el orden de
 * NATIVE_MODS), así que trabaja sobre una velocidad que player_speed ya alargó: reconoce una velocidad que
 * apunta hacia donde dirección * velocidad escalar, no solo una igual. Una velocidad que apunta a otro lado (un
 * empujón, un retroceso) se deja. Solo se escalan las componentes horizontales, y solo en juego libre (estado
 * principal 0). Con medio stick el personaje sigue caminando, como uno con las manos vacías.
 */

#include <string.h>

#include "hm_mm_mod.h"

#define MOD_ID "carry_speed"

#define WORD_BYTES 4
#define HALFWORD_BYTES 2
#define BYTE_BYTES 1
#define NO_ADDRESS 0u

/* The game's state machine keeps its current state here: 0 in free play (docs/SCRIPTS.md). */
#define MAIN_STATE_ADDRESS 0x80321D18u
#define MAIN_STATE_FREE_PLAY 0u

/* fn_80049D28(actor, input, time_step): the player's update. */
#define PLAYER_UPDATE_FUNCTION DOLRECOMP_SYMBOL_fn_80049D28

/* Where the things that the mod reads are in an actor (the type byte is 1 for the player). */
#define ACTOR_PHYSICS_NODE_OFFSET 0x30u
#define ACTOR_DIRECTION_X_OFFSET 0x44u
#define ACTOR_DIRECTION_Z_OFFSET 0x4Cu
#define ACTOR_SPEED_OFFSET 0x58u
#define ACTOR_TYPE_OFFSET 0x94u
#define ACTOR_TYPE_PLAYER 1u
#define ACTOR_STATE_OFFSET 0xC8u

/* Where the velocity is in a physics node (x is sideways, y is vertical, z is forward). */
#define NODE_VELOCITY_X_OFFSET 0x98u
#define NODE_VELOCITY_Z_OFFSET 0xA0u

/* How far the stick is pushed: a float in the input that the game gives to the player's update. */
#define INPUT_STICK_OFFSET 0x1Cu
#define STICK_PUSHED_FAR_ENOUGH_TO_RUN 54.5f

/*
 * The walking states of the player while holding something: the copies of state 8 and of the states that
 * follow it. Empty-handed, state 8 is the only one from which the game changes to running (state 0xC).
 */
#define HOLDING_WALK_FIRST_A 0x25
#define HOLDING_WALK_LAST_A 0x28
#define HOLDING_WALK_FIRST_B 0x38
#define HOLDING_WALK_LAST_B 0x3B
#define HOLDING_WALK_C 0x46

/* The top speeds of the game: walking 3.0 (the constant at 0x80322818) and running 5.0. */
#define WALKING_TOP_SPEED 3.0f
#define RUNNING_TOP_SPEED 5.0f

/*
 * How close to the same direction a velocity must be, to be taken as the game's (maybe made longer by another
 * mod): the square of the sine of the angle between them, 1e-6, is an angle of about a twentieth of a degree.
 */
#define SAME_DIRECTION_TOLERANCE 1e-6f
#define NOTHING 1e-12f

static uint32_t read_word(CPUState* cpu, uint32_t address)
{
    return (uint32_t)moderngekko_mod_read(cpu, address, WORD_BYTES);
}

static float read_float(CPUState* cpu, uint32_t address)
{
    const uint32_t bits = read_word(cpu, address);
    float value;
    memcpy(&value, &bits, sizeof value);
    return value;
}

static void write_float(CPUState* cpu, uint32_t address, float value)
{
    uint32_t bits;
    memcpy(&bits, &value, sizeof bits);
    moderngekko_mod_write(cpu, address, bits, WORD_BYTES);
}

static int is_a_holding_walk_state(int state)
{
    return (state >= HOLDING_WALK_FIRST_A && state <= HOLDING_WALK_LAST_A) ||
           (state >= HOLDING_WALK_FIRST_B && state <= HOLDING_WALK_LAST_B) || state == HOLDING_WALK_C;
}

/*
 * Makes the node's horizontal velocity longer when it points the way the game's own velocity (direction * speed)
 * does: that holds for the game's, and for one that another mod made longer.
 */
static void make_the_velocity_longer(CPUState* cpu, uint32_t actor, uint32_t physics_node)
{
    const float speed = read_float(cpu, actor + ACTOR_SPEED_OFFSET);
    const float game_x = read_float(cpu, actor + ACTOR_DIRECTION_X_OFFSET) * speed;
    const float game_z = read_float(cpu, actor + ACTOR_DIRECTION_Z_OFFSET) * speed;
    const float velocity_x = read_float(cpu, physics_node + NODE_VELOCITY_X_OFFSET);
    const float velocity_z = read_float(cpu, physics_node + NODE_VELOCITY_Z_OFFSET);

    const float game_size = game_x * game_x + game_z * game_z;
    const float velocity_size = velocity_x * velocity_x + velocity_z * velocity_z;
    if (!(game_size > NOTHING) || !(velocity_size > NOTHING)) {
        return;
    }
    const float along = velocity_x * game_x + velocity_z * game_z;
    const float across = velocity_x * game_z - velocity_z * game_x;
    if (along <= 0.0f || across * across > SAME_DIRECTION_TOLERANCE * game_size * velocity_size) {
        return;
    }
    const float longer = RUNNING_TOP_SPEED / WALKING_TOP_SPEED;
    write_float(cpu, physics_node + NODE_VELOCITY_X_OFFSET, velocity_x * longer);
    write_float(cpu, physics_node + NODE_VELOCITY_Z_OFFSET, velocity_z * longer);
}

/* fn_80049D28(actor, input, time_step): runs when the game enters the player's update. */
static void let_the_player_run_with_things_in_the_hands(CPUState* cpu)
{
    const uint32_t actor = moderngekko_mod_arg_u32(cpu, 0);
    const uint32_t input = moderngekko_mod_arg_u32(cpu, 1);
    if (read_word(cpu, MAIN_STATE_ADDRESS) != MAIN_STATE_FREE_PLAY) {
        return;
    }
    if (moderngekko_mod_read(cpu, actor + ACTOR_TYPE_OFFSET, BYTE_BYTES) != ACTOR_TYPE_PLAYER) {
        return;
    }
    const int state = (int16_t)moderngekko_mod_read(cpu, actor + ACTOR_STATE_OFFSET, HALFWORD_BYTES);
    if (!is_a_holding_walk_state(state)) {
        return;
    }
    if (read_float(cpu, input + INPUT_STICK_OFFSET) < STICK_PUSHED_FAR_ENOUGH_TO_RUN) {
        return;
    }
    const uint32_t physics_node = read_word(cpu, actor + ACTOR_PHYSICS_NODE_OFFSET);
    if (physics_node == NO_ADDRESS) {
        return;
    }
    make_the_velocity_longer(cpu, actor, physics_node);
}

static const ModernGekkoModHook hooks[] = {
    RECOMP_HOOK(PLAYER_UPDATE_FUNCTION, let_the_player_run_with_things_in_the_hands),
};

static const ModernGekkoModDesc descriptor = {
    .abi_version = MODERNGEKKO_MOD_ABI_VERSION,
    .cpu_abi_version = MODERNGEKKO_CPU_ABI_VERSION,
    .cpu_state_size = sizeof(CPUState),
    .game_id = HM_MM_GAME_ID,
    .id = MOD_ID,
    .version = "0.1.0",
    .display_name = "No slowdown when carrying",
    .hooks = hooks,
    .num_hooks = HM_MM_ARRAY_COUNT(hooks),
};

MODERNGEKKO_MOD_EXPORT const ModernGekkoModDesc* moderngekko_get_mod(void)
{
    return &descriptor;
}
