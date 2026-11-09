// See character_range.h.

#include "graphics/character_range.h"

#include "graphics/camera_view.h"
#include "graphics/main_view.h"
#include "guest_memory.h"
#include "runtime_log.h"

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdint>
#include <optional>

namespace character_range {
namespace {

// The distance from the camera task's point within which characters are drawn and move (a
// float the characters' frame handler reads, and nothing else does).
constexpr uint32_t CHARACTER_DISTANCE = 0x803227DCu;
constexpr float GAME_CHARACTER_DISTANCE = 25.f;
// Far enough for every character of any place.
constexpr float LONGEST_CHARACTER_DISTANCE = 10000.f;

bool g_checked = false;
bool g_available = false;
float g_written = GAME_CHARACTER_DISTANCE;
// For the log: the zoom of the frame before, and the distance last written to the log,
// which tells it once the zoom has settled and the distance changed by this much.
float g_previous_zoom = 1.f;
float g_logged = GAME_CHARACTER_DISTANCE;
constexpr float LOGGED_CHANGE = 2.f;

float read_float(uint32_t address) { return std::bit_cast<float>(guest_memory::read32(address)); }

// How many times farther the camera shows the ground now than with the game's projection,
// in every direction (the characters measure a straight distance); 1 when it cannot be
// measured.
double reach_ratio() {
    const std::optional<main_view::GroundCorners> corners = main_view::ground_corners();
    if (!corners) {
        return 1;
    }
    return main_view::reach_ratio(
        *corners, [](const main_view::Vector& offset) { return std::sqrt(offset.x * offset.x + offset.z * offset.z); });
}

void write_distance(float distance) {
    if (distance != g_written) {
        guest_memory::write32(CHARACTER_DISTANCE, std::bit_cast<uint32_t>(distance));
        g_written = distance;
    }
    const float zoom = camera_view::zoom();
    if (zoom == g_previous_zoom && std::abs(distance - g_logged) >= LOGGED_CHANGE) {
        runtime_log::info("graphics", "characters: drawn and moving up to %.0f from the camera (the game's %.0f)",
                          distance, GAME_CHARACTER_DISTANCE);
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
        g_available = read_float(CHARACTER_DISTANCE) == GAME_CHARACTER_DISTANCE;
        if (!g_available) {
            runtime_log::warning("graphics",
                                 "characters: their distance is not where it was expected; they are drawn as "
                                 "the game does");
        }
    }
    if (!g_available) {
        return;
    }
    float distance = GAME_CHARACTER_DISTANCE;
    if (camera_view::zoom() < 1.f) {
        distance =
            static_cast<float>(std::min<double>(GAME_CHARACTER_DISTANCE * reach_ratio(), LONGEST_CHARACTER_DISTANCE));
    }
    write_distance(distance);
}

}  // namespace character_range
