// See map_draw_range.h.

#include "graphics/map_draw_range.h"

#include "graphics/camera_view.h"
#include "graphics/main_view.h"
#include "guest_memory.h"
#include "runtime_log.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>

namespace map_draw_range {
namespace {

// The distance from the player within which the game draws the map's pieces (a float the
// field's update reads each frame, and nothing else does).
constexpr uint32_t PIECE_DISTANCE = 0x80322BE0u;
constexpr float GAME_PIECE_DISTANCE = 12.f;
// Far enough for every piece of any place (the biggest is 7 pieces of 24).
constexpr float LONGEST_PIECE_DISTANCE = 10000.f;

bool g_checked = false;
bool g_available = false;
float g_written = GAME_PIECE_DISTANCE;
// For the log: the zoom of the frame before, and the distance last written to the log,
// which tells it once the zoom has settled and the distance changed by this much.
float g_previous_zoom = 1.f;
float g_logged = GAME_PIECE_DISTANCE;
constexpr float LOGGED_CHANGE = 2.f;

float read_float(uint32_t address) { return std::bit_cast<float>(guest_memory::read32(address)); }

// How many times farther the camera shows the ground now than with the game's projection,
// along the map's axes (the pieces are squares along them); 1 when it cannot be measured.
double reach_ratio() {
    const std::optional<main_view::GroundCorners> corners = main_view::ground_corners();
    if (!corners) {
        return 1;
    }
    return main_view::reach_ratio(
        *corners, [](const main_view::Vector& offset) { return std::max(std::abs(offset.x), std::abs(offset.z)); });
}

void write_distance(float distance) {
    if (distance != g_written) {
        guest_memory::write32(PIECE_DISTANCE, std::bit_cast<uint32_t>(distance));
        g_written = distance;
    }
    const float zoom = camera_view::zoom();
    if (zoom == g_previous_zoom && std::abs(distance - g_logged) >= LOGGED_CHANGE) {
        runtime_log::info("graphics", "map: pieces drawn up to %.0f from the player (the game's %.0f)", distance,
                          GAME_PIECE_DISTANCE);
        g_logged = distance;
    }
    g_previous_zoom = zoom;
}

}  // namespace

void update() {
    if (!g_checked) {
        // Only where the game keeps it: another version of the game would hold something
        // else there.
        g_checked = true;
        g_available = read_float(PIECE_DISTANCE) == GAME_PIECE_DISTANCE;
        if (!g_available) {
            runtime_log::warning("graphics",
                                 "map: the distance of the pieces is not where it was expected; the map "
                                 "is drawn as the game does");
        }
    }
    if (!g_available) {
        return;
    }
    float distance = GAME_PIECE_DISTANCE;
    if (camera_view::zoom() < 1.f) {
        const double ratio = reach_ratio();
        distance =
            static_cast<float>(std::min<double>(GAME_PIECE_DISTANCE * std::max(1.0, ratio), LONGEST_PIECE_DISTANCE));
    }
    write_distance(distance);
}

}  // namespace map_draw_range
