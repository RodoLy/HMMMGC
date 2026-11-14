/*
 * far_camera: the game's own button that pulls the camera back goes farther than the game allows.
 * far_camera: el botón del propio juego que aleja la cámara llega más lejos de lo que el juego permite.
 *
 * How the game zooms (found in its code, and watched in the running game):
 *
 *   - fn_80031580(controller, time_step) updates the camera that follows the player. It reads a float of the
 *     camera (controller + 0x30 points to the camera, the float is at camera + 0x58, read by fn_801BBC04 and
 *     written by fn_801BBC0C) and, when the game lets the player move the camera, adds 1.0 to it for each frame
 *     that the D-pad is held down, up to 20.0, and takes 1.0 from it for each frame that the D-pad is held up,
 *     down to 10.0. At 20.0 the camera is as far as the game lets it go (and it is where the game starts); at
 *     10.0 the picture is twice as big.
 *   - The limits are the floats at 0x80322A74 (10.0) and 0x80322A80 (20.0); the game also takes its starting
 *     value from the second one (fn_800314B8), so the limit cannot be changed there.
 *
 * What this mod does: it lets the player keep pulling the camera back past 20.0, without touching the game's
 * number. While the camera is at the game's limit, the mod lends the game a value one step below it (19.0) for
 * the length of fn_80031580 and looks at what the game made of it: 20.0 means the game took a "down" of the
 * D-pad, 18.0 an "up", and 19.0 neither. That way the mod follows the same button under the same conditions as
 * the game (not in a menu, not in a scene), without reading the controller itself. Each "down" moves the mod's
 * own amount a step towards 1, each "up" a step back towards 0, and only when it is back at 0 does an "up"
 * reach the game's own zoom. The game's number is put back before the function returns.
 *
 * The amount (0: the game's limit, 1: the farthest) is what hm_mm_far_camera_amount() gives. The native runtime
 * reads it every frame and widens the picture by as much (runtime/src/graphics/camera_view.h), which also makes
 * the game draw the map, the characters and the shadows that far. So the mod works in the native version; a
 * loader that only runs the hooks keeps the game's camera.
 *
 * Only outdoors (the fields and the towns, as the runtime's game_scene.h tells them apart): in a room the camera
 * would see past the walls. The amount is kept while the game runs, so the camera is as far again after
 * coming out of a house.
 *
 * Cómo hace zoom el juego (se vio en su código y en el juego en ejecución):
 *
 *   - fn_80031580(controlador, paso) actualiza la cámara que sigue al jugador. Lee un float de la cámara
 *     (controlador + 0x30 apunta a la cámara; el float está en cámara + 0x58, lo lee fn_801BBC04 y lo escribe
 *     fn_801BBC0C) y, cuando el juego deja mover la cámara, le suma 1.0 por cada cuadro con la cruceta hacia
 *     abajo, hasta 20.0, y le resta 1.0 por cada cuadro con la cruceta hacia arriba, hasta 10.0. En 20.0 la
 *     cámara está lo más lejos que el juego permite (y así empieza); en 10.0 la imagen se ve al doble.
 *   - Los topes son los floats en 0x80322A74 (10.0) y 0x80322A80 (20.0); el juego también toma su valor inicial
 *     del segundo (fn_800314B8), así que el tope no se puede cambiar ahí.
 *
 * Lo que hace este mod: deja seguir alejando la cámara más allá de 20.0, sin tocar el número del juego. Mientras
 * la cámara está en el tope del juego, el mod le presta al juego un valor un paso abajo (19.0) durante
 * fn_80031580 y mira qué hizo el juego con él: 20.0 quiere decir que el juego tomó un «abajo» de la cruceta,
 * 18.0 un «arriba» y 19.0 ninguno. Así el mod sigue el mismo botón con las mismas condiciones que el juego (no
 * en un menú ni en una escena), sin leer el control. Cada «abajo» mueve la cantidad del mod un paso hacia 1,
 * cada «arriba» un paso hacia 0, y solo cuando vuelve a 0 un «arriba» llega al zoom del propio juego. El número
 * del juego se repone antes de que la función regrese.
 *
 * La cantidad (0: el tope del juego, 1: lo más lejos) es lo que da hm_mm_far_camera_amount(). El runtime nativo
 * la lee cada cuadro y ensancha la imagen en esa medida (runtime/src/graphics/camera_view.h), con lo que el
 * juego también dibuja el mapa, los personajes y las sombras hasta ahí. Por eso el mod funciona en la versión
 * nativa; un cargador que solo corre los ganchos se queda con la cámara del juego.
 *
 * Solo al aire libre (los campos y los pueblos, como los distingue game_scene.h del runtime): en un cuarto la
 * cámara vería más allá de las paredes. La cantidad se conserva mientras el juego corre, así que la cámara
 * vuelve a estar igual de lejos al salir de una casa.
 */

#include <string.h>

#include "hm_mm_mod.h"

#define MOD_ID "far_camera"

#define WORD_BYTES 4
#define BYTE_BYTES 1
#define NO_ADDRESS 0u

/* fn_80031580(controller, time_step): the update of the camera that follows the player. */
#define CAMERA_UPDATE_FUNCTION DOLRECOMP_SYMBOL_fn_80031580

/* The controller points to its camera, and the camera holds the number the D-pad changes. */
#define CONTROLLER_CAMERA_OFFSET 0x30u
#define CAMERA_ZOOM_OFFSET 0x58u

/* The game's limit (the float at 0x80322A80) and its step for each frame the D-pad is held (0x80322A7C). */
#define GAME_FARTHEST 20.0f
#define GAME_STEP 1.0f
#define HALF_A_STEP 0.5f

/* How much the mod's amount changes for each frame the D-pad is held: the whole way in two thirds of a second. */
#define AMOUNT_STEP 0.05f

/* The area whose map is loaded, and the map of each area: pieces across and pieces deep (game_scene.cpp). */
#define CURRENT_AREA_ADDRESS 0x80321D40u
#define AREA_MAPS_ADDRESS 0x80216830u
#define AREA_MAP_BYTES 4u
#define AREA_COUNT 129u
#define FIELD_AREA_COUNT 16u

/* How far past the game's limit the camera is: 0 at the limit, 1 the farthest. */
static float amount = 0.0f;
/* The camera whose number the mod lowered at the entry of the update, to put it back at the return. */
static uint32_t lent_camera = NO_ADDRESS;

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

/* The fields are outdoors, and so is any other place made of more than one piece; a room is one piece. */
static int the_place_is_outdoors(CPUState* cpu)
{
    const uint32_t area = read_word(cpu, CURRENT_AREA_ADDRESS);
    if (area >= AREA_COUNT) {
        return 0;
    }
    if (area < FIELD_AREA_COUNT) {
        return 1;
    }
    const uint32_t map = AREA_MAPS_ADDRESS + area * AREA_MAP_BYTES;
    const unsigned pieces_across = (unsigned)moderngekko_mod_read(cpu, map, BYTE_BYTES);
    const unsigned pieces_deep = (unsigned)moderngekko_mod_read(cpu, map + 1, BYTE_BYTES);
    return pieces_across * pieces_deep > 1;
}

/* At the entry of the camera's update: at the game's limit, lends the game a value one step below it. */
static void lend_the_game_a_step(CPUState* cpu)
{
    lent_camera = NO_ADDRESS;
    const uint32_t controller = moderngekko_mod_arg_u32(cpu, 0);
    if (controller == NO_ADDRESS || !the_place_is_outdoors(cpu)) {
        return;
    }
    const uint32_t camera = read_word(cpu, controller + CONTROLLER_CAMERA_OFFSET);
    if (camera == NO_ADDRESS) {
        return;
    }
    if (read_float(cpu, camera + CAMERA_ZOOM_OFFSET) < GAME_FARTHEST - HALF_A_STEP) {
        /* The game's own zoom is in use: the camera is closer than its limit. */
        amount = 0.0f;
        return;
    }
    write_float(cpu, camera + CAMERA_ZOOM_OFFSET, GAME_FARTHEST - GAME_STEP);
    lent_camera = camera;
}

/* At the return of the camera's update: what the game did with the lent step says which way the D-pad went. */
static void follow_the_d_pad(CPUState* cpu)
{
    if (lent_camera == NO_ADDRESS) {
        return;
    }
    const uint32_t camera = lent_camera;
    lent_camera = NO_ADDRESS;
    const float lent = GAME_FARTHEST - GAME_STEP;
    const float after = read_float(cpu, camera + CAMERA_ZOOM_OFFSET);
    float game_value = GAME_FARTHEST;
    if (after > lent + HALF_A_STEP) {
        /* Down: farther. */
        amount += AMOUNT_STEP;
        if (amount > 1.0f) {
            amount = 1.0f;
        }
    } else if (after < lent - HALF_A_STEP) {
        /* Up: closer, first by the mod's amount and then by the game's own zoom. */
        if (amount > 0.0f) {
            amount -= AMOUNT_STEP;
            if (amount < 0.0f) {
                amount = 0.0f;
            }
        } else {
            game_value = GAME_FARTHEST - GAME_STEP;
        }
    }
    write_float(cpu, camera + CAMERA_ZOOM_OFFSET, game_value);
}

/* For the native runtime (camera_view.h): how far past the game's limit the camera is, from 0 to 1. */
float hm_mm_far_camera_amount(void)
{
    return amount;
}

static const ModernGekkoModHook hooks[] = {
    RECOMP_HOOK(CAMERA_UPDATE_FUNCTION, lend_the_game_a_step),
    RECOMP_HOOK_RETURN(CAMERA_UPDATE_FUNCTION, follow_the_d_pad),
};

static const ModernGekkoModDesc descriptor = {
    .abi_version = MODERNGEKKO_MOD_ABI_VERSION,
    .cpu_abi_version = MODERNGEKKO_CPU_ABI_VERSION,
    .cpu_state_size = sizeof(CPUState),
    .game_id = HM_MM_GAME_ID,
    .id = MOD_ID,
    .version = "0.1.0",
    .display_name = "Farther camera",
    .hooks = hooks,
    .num_hooks = HM_MM_ARRAY_COUNT(hooks),
};

MODERNGEKKO_MOD_EXPORT const ModernGekkoModDesc* moderngekko_get_mod(void)
{
    return &descriptor;
}
