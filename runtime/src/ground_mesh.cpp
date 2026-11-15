// See ground_mesh.h.

#include "ground_mesh.h"

#include "guest_memory.h"
#include "runtime_log.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstring>

// The game's own mesh, which fn_8006490C starts when the farm is loaded: what the variables
// hold unless the far_camera mod is on.
extern "C" {
uint32_t hmmm_ground_mesh_size = 20;
uint32_t hmmm_ground_mesh_squares = 19;
uint32_t hmmm_ground_mesh_points = 20 * 20;
uint32_t hmmm_ground_mesh_row = 20 * 12;
uint32_t hmmm_ground_mesh_corners = 19 * 19 * 6;
float hmmm_ground_mesh_behind = 9.0f;
uint32_t hmmm_ground_mesh_positions = 0x802B9008u;
uint32_t hmmm_ground_mesh_soil = 0x802B9008u + 4800;
uint32_t hmmm_ground_mesh_marks = 0x802B9008u + 5200;
}

namespace ground_mesh {
namespace {

// Points to a side: the game's, and for the farthest view (zoom 0.5), where the mesh reaches 24
// tiles west and north of the player and 25 to the east and south, as far as the farm's grid of
// crops (crop_grid.cpp). In between the size grows with the zoom, in steps of 6 points: a new
// size starts the mesh over (every point's height is looked up again in one frame), so it only
// changes every few presses of the game's button.
constexpr uint32_t GAME_SIZE = 20;
constexpr uint32_t FARTHEST_SIZE = 50;
constexpr uint32_t SIZE_STEP = 6;
constexpr float FARTHEST_ZOOM = 0.5f;
// The game's constant with the tiles the mesh starts west and north of the player (read only by
// fn_80064108), to check that this is the game the translation was changed for.
constexpr uint32_t BEHIND_CONSTANT = 0x80323060u;
constexpr uint32_t POSITION_BYTES = 12;  // x, y, z
// What fn_8006490C starts each point's soil and marks with.
constexpr uint8_t SOIL_AT_START = 0x0D;
constexpr uint8_t MARKS_AT_START = 6;
// A corner of a triangle in the display lists: position (3 floats), colour (4 bytes) and
// texture coordinate (2 floats).
constexpr uint32_t CORNER_BYTES = 24;
// More room than the corners take: the list's command and its end (padded to 32 bytes).
constexpr uint32_t LIST_SLACK = 4096;

// The game's room for display lists (fn_801B25C4 makes it, fn_801B250C takes from it): where it
// starts, where the next list goes, its size, how much this frame took, and how far into it the
// next list goes.
constexpr uint32_t ROOM_START = 0x80322578u;
constexpr uint32_t ROOM_NEXT = 0x80322580u;
constexpr uint32_t ROOM_SIZE = 0x80322584u;
constexpr uint32_t ROOM_TAKEN = 0x80322588u;
constexpr uint32_t ROOM_OFFSET = 0x8032258Cu;
constexpr std::array<uint32_t, 5> ROOM_WORDS{ROOM_START, ROOM_NEXT, ROOM_SIZE, ROOM_TAKEN, ROOM_OFFSET};
// Where fn_80064108 keeps the mesh's first tile of the frame before; fn_8006490C starts it at
// -10000, so that every point is new to the next update.
constexpr uint32_t PREVIOUS_FIRST_TILE_X = 0x80321E70u;
constexpr uint32_t PREVIOUS_FIRST_TILE_Z = 0x80321E74u;
constexpr uint32_t NO_PREVIOUS_TILE = static_cast<uint32_t>(-10000);

bool g_bigger = false;
// A room for each of the two display lists of a drawing (the soil, then the marks), used in
// turn, and its size.
std::array<uint32_t, 2> g_list_rooms{};
uint32_t g_list_room_size = 0;
uint32_t g_next_room = 0;
// The game's room while one of the lists is made in the runtime's.
std::array<uint32_t, ROOM_WORDS.size()> g_game_room{};
bool g_lent = false;

// The size for a zoom: the game's at zoom 1, the farthest at zoom 0.5, and in between the next
// step up from a straight line between them.
uint32_t size_for_zoom(float zoom) {
    const float far = std::clamp((1.f - zoom) / (1.f - FARTHEST_ZOOM), 0.f, 1.f);
    const float needed = static_cast<float>(GAME_SIZE) + far * static_cast<float>(FARTHEST_SIZE - GAME_SIZE);
    uint32_t size = GAME_SIZE;
    while (static_cast<float>(size) < needed && size < FARTHEST_SIZE) {
        size = std::min(size + SIZE_STEP, FARTHEST_SIZE);
    }
    return size;
}

void use_size(uint32_t size) {
    hmmm_ground_mesh_size = size;
    hmmm_ground_mesh_squares = size - 1;
    hmmm_ground_mesh_points = size * size;
    hmmm_ground_mesh_row = size * POSITION_BYTES;
    hmmm_ground_mesh_corners = (size - 1) * (size - 1) * 6;
    hmmm_ground_mesh_behind = static_cast<float>(size / 2 - 1);
}

}  // namespace

void size_for_farthest_view() {
    // Only where the game keeps it: another version of the game would hold something else.
    if (std::bit_cast<float>(guest_memory::read32(BEHIND_CONSTANT)) != GAME_SIZE / 2 - 1) {
        runtime_log::warning("farm",
                             "the size of the mesh of tilled soil is not where it was expected; it stays the game's");
        return;
    }
    // The tables and the rooms, for the farthest; the mesh starts with the game's size.
    use_size(FARTHEST_SIZE);
    const uint32_t points = hmmm_ground_mesh_points;
    hmmm_ground_mesh_positions = guest_memory::reserve_runtime_memory(points * POSITION_BYTES, 32);
    hmmm_ground_mesh_soil = guest_memory::reserve_runtime_memory(points, 32);
    hmmm_ground_mesh_marks = guest_memory::reserve_runtime_memory(points, 32);
    g_list_room_size = (hmmm_ground_mesh_corners * CORNER_BYTES + LIST_SLACK + 31) & ~31u;
    for (uint32_t& room : g_list_rooms) {
        room = guest_memory::reserve_runtime_memory(g_list_room_size, 32);
    }
    use_size(GAME_SIZE);
    g_bigger = true;
    hmmm_ground_mesh_reset();
    runtime_log::info("farm", "tilled soil drawn up to %u tiles from the player when the camera is farthest",
                      FARTHEST_SIZE / 2 - 1);
}

void follow_zoom(float zoom) {
    if (!g_bigger) {
        return;
    }
    const uint32_t size = size_for_zoom(zoom);
    if (size == hmmm_ground_mesh_size) {
        return;
    }
    use_size(size);
    hmmm_ground_mesh_reset();
    guest_memory::write32(PREVIOUS_FIRST_TILE_X, NO_PREVIOUS_TILE);
    guest_memory::write32(PREVIOUS_FIRST_TILE_Z, NO_PREVIOUS_TILE);
    runtime_log::info("farm", "tilled soil drawn up to %u tiles from the player", size / 2 - 1);
}

}  // namespace ground_mesh

using namespace ground_mesh;

extern "C" void hmmm_ground_mesh_reset() {
    if (!g_bigger) {
        return;  // fn_8006490C starts the game's tables itself
    }
    const uint32_t points = hmmm_ground_mesh_points;
    std::memset(guest_memory::host_pointer(hmmm_ground_mesh_positions, points * POSITION_BYTES), 0,
                points * POSITION_BYTES);  // 0.0f
    std::memset(guest_memory::host_pointer(hmmm_ground_mesh_soil, points), SOIL_AT_START, points);
    std::memset(guest_memory::host_pointer(hmmm_ground_mesh_marks, points), MARKS_AT_START, points);
}

extern "C" void hmmm_ground_mesh_list_begins() {
    if (!g_bigger || g_lent) {
        return;
    }
    for (size_t i = 0; i < ROOM_WORDS.size(); ++i) {
        g_game_room[i] = guest_memory::read32(ROOM_WORDS[i]);
    }
    const uint32_t room = g_list_rooms[g_next_room];
    g_next_room = (g_next_room + 1) % g_list_rooms.size();
    guest_memory::write32(ROOM_START, room);
    guest_memory::write32(ROOM_NEXT, room);
    guest_memory::write32(ROOM_SIZE, g_list_room_size);
    guest_memory::write32(ROOM_TAKEN, 0);
    guest_memory::write32(ROOM_OFFSET, 0);
    g_lent = true;
}

extern "C" void hmmm_ground_mesh_list_ends() {
    if (!g_lent) {
        return;
    }
    for (size_t i = 0; i < ROOM_WORDS.size(); ++i) {
        guest_memory::write32(ROOM_WORDS[i], g_game_room[i]);
    }
    g_lent = false;
}
