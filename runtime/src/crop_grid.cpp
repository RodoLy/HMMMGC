// See crop_grid.h.

#include "crop_grid.h"

#include "guest_memory.h"
#include "runtime_log.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <limits>
#include <numbers>

// The game's own grid, which fn_80073628 sets up when the farm is loaded: what the variables
// hold unless the view needs more.
extern "C" {
uint32_t hmmm_crop_grid_size = 25;
uint32_t hmmm_crop_grid_inner_end = 20;
uint32_t hmmm_crop_grid_model_row = 25 * 8 * 4;
uint32_t hmmm_crop_grid_item_row = 25 * 2;
uint32_t hmmm_crop_grid_word_row = 25 * 4;
uint32_t hmmm_crop_grid_models = 0x802CF1D8u;
uint32_t hmmm_crop_grid_attachments = 0x802D3FF8u;
uint32_t hmmm_crop_grid_items = 0x802D8E18u;
uint32_t hmmm_crop_grid_kinds = 0x802D92FCu;
uint32_t hmmm_crop_grid_effects = 0x802D9CC0u;
uint32_t hmmm_crop_grid_flags = 0x802DA684u;
uint32_t hmmm_crop_grid_heights = 0x802DB048u;
}

namespace crop_grid {
namespace {

// Tiles from the player to each side of the square: the game's, and the most the
// runtime's memory is given for (51 tiles to a side, enough for zoom 0.5 in 16:9).
constexpr uint32_t GAME_HALF_SIZE = 12;
constexpr uint32_t LARGEST_HALF_SIZE = 25;
// The rows and columns at the square's edge that keep only what stands out (fn_80072B0C
// keeps everything from the square's first tile + 4 to its last - 4).
constexpr uint32_t EDGE_BAND = 4;
// The game starts the square this many tiles further north than south.
constexpr double NORTH_SHIFT = 2;
// The game's constant with the square's half size, as a float (read only by fn_80072B0C).
constexpr uint32_t HALF_SIZE_CONSTANT = 0x803230C8u;
constexpr uint32_t MODELS_PER_TILE = 8;
constexpr uint32_t WORD = 4;
constexpr uint32_t HALFWORD = 2;

// The camera on the farm, measured from the game's main view: it looks north, down at 45
// degrees, with a vertical field of view of 20 degrees on a 4:3 picture, from 14.1 above
// the player and 14.1 to the south.
constexpr double CAMERA_PITCH_DEGREES = 45;
constexpr double CAMERA_FIELD_OF_VIEW_DEGREES = 20;
constexpr double CAMERA_HEIGHT = 14.1;
constexpr double GAME_ASPECT_RATIO = 4.0 / 3.0;
constexpr double WIDESCREEN_ASPECT_RATIO = 16.0 / 9.0;

// How far from the player the farm's camera sees the ground: to the sides, to the north
// and to the south, in tiles.
struct GroundReach {
    double sides = 0;
    double north = 0;
    double south = 0;
};

constexpr double INFINITE = std::numeric_limits<double>::infinity();

// The ground the corners of the screen look at, with the camera zoomed by `zoom` on a
// picture of `aspect_ratio`. Infinite when the top of the screen looks at the horizon.
GroundReach ground_reach(double zoom, double aspect_ratio) {
    const double pitch = CAMERA_PITCH_DEGREES * std::numbers::pi / 180;
    const double half_height = std::tan(CAMERA_FIELD_OF_VIEW_DEGREES * std::numbers::pi / 360) / zoom;
    const double half_width = half_height * aspect_ratio;
    // The camera's axes in the world, with x to the east, y up and z to the south: it looks
    // ahead and down, its up leans to the north, and it stands south of the player.
    const double ahead_y = -std::sin(pitch);
    const double ahead_z = -std::cos(pitch);
    const double up_y = std::cos(pitch);
    const double up_z = -std::sin(pitch);
    const double eye_z = CAMERA_HEIGHT / std::tan(pitch);
    GroundReach reach;
    for (const double side : {-1.0, 1.0}) {
        for (const double up : {-1.0, 1.0}) {
            const double x = side * half_width;
            const double y = ahead_y + up * half_height * up_y;
            const double z = ahead_z + up * half_height * up_z;
            if (y >= 0) {
                return {INFINITE, INFINITE, INFINITE};
            }
            const double along = CAMERA_HEIGHT / -y;  // down to the player's level
            const double ground_z = eye_z + along * z;
            reach.sides = std::max(reach.sides, std::abs(along * x));
            reach.north = std::max(reach.north, -ground_z);
            reach.south = std::max(reach.south, ground_z);
        }
    }
    return reach;
}

// The half size whose square keeps every item the camera sees: the part with all items
// reaches the half size - 4 to the sides, - 2 to the north and - 6 to the south, and one
// tile more covers what stands on the tiles at the edge of the view. For the game's
// camera this gives the game's 12.
uint32_t half_size_for(const GroundReach& reach) {
    const double needed = std::max({reach.sides + EDGE_BAND, reach.north + EDGE_BAND - NORTH_SHIFT,
                                    reach.south + EDGE_BAND + NORTH_SHIFT}) +
                          1;
    if (!(needed < LARGEST_HALF_SIZE)) {
        return LARGEST_HALF_SIZE;
    }
    return std::max(GAME_HALF_SIZE, static_cast<uint32_t>(std::ceil(needed)));
}

float read_float(uint32_t address) { return std::bit_cast<float>(guest_memory::read32(address)); }

}  // namespace

void size_for_view(float zoom, bool widescreen) {
    if (zoom >= 1) {
        return;  // the game's camera, or closer: the game's square
    }
    const uint32_t half_size =
        half_size_for(ground_reach(zoom, widescreen ? WIDESCREEN_ASPECT_RATIO : GAME_ASPECT_RATIO));
    if (half_size == GAME_HALF_SIZE) {
        return;
    }
    // Only where the game keeps it: another version of the game would hold something else.
    if (read_float(HALF_SIZE_CONSTANT) != static_cast<float>(GAME_HALF_SIZE)) {
        runtime_log::warning("farm", "the size of the grid of crops is not where it was expected; it stays the game's");
        return;
    }
    const uint32_t size = 2 * half_size + 1;
    const uint32_t tiles = size * size;
    hmmm_crop_grid_size = size;
    hmmm_crop_grid_inner_end = size - 1 - EDGE_BAND;
    hmmm_crop_grid_model_row = size * MODELS_PER_TILE * WORD;
    hmmm_crop_grid_item_row = size * HALFWORD;
    hmmm_crop_grid_word_row = size * WORD;
    hmmm_crop_grid_models = guest_memory::reserve_runtime_memory(tiles * MODELS_PER_TILE * WORD, 32);
    hmmm_crop_grid_attachments = guest_memory::reserve_runtime_memory(tiles * MODELS_PER_TILE * WORD, 32);
    hmmm_crop_grid_items = guest_memory::reserve_runtime_memory(tiles * HALFWORD, WORD);
    hmmm_crop_grid_kinds = guest_memory::reserve_runtime_memory(tiles * WORD, WORD);
    hmmm_crop_grid_effects = guest_memory::reserve_runtime_memory(tiles * WORD, WORD);
    hmmm_crop_grid_flags = guest_memory::reserve_runtime_memory(tiles * WORD, WORD);
    hmmm_crop_grid_heights = guest_memory::reserve_runtime_memory(tiles * WORD, WORD);
    guest_memory::write32(HALF_SIZE_CONSTANT, std::bit_cast<uint32_t>(static_cast<float>(half_size)));
    runtime_log::info("farm", "crops and items drawn up to %u tiles to the sides of the player (the game's %u)",
                      half_size - EDGE_BAND, GAME_HALF_SIZE - EDGE_BAND);
}

}  // namespace crop_grid
