// See map_draw_range.h.

#include "graphics/map_draw_range.h"

#include "graphics/camera_view.h"
#include "guest_memory.h"
#include "runtime_log.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <numbers>

namespace map_draw_range {
namespace {

// The distance from the player within which the game draws the map's pieces (a float the
// field's update reads each frame, and nothing else does).
constexpr uint32_t PIECE_DISTANCE = 0x80322BE0u;
constexpr float GAME_PIECE_DISTANCE = 12.f;
// Far enough for every piece of any place (the biggest is 7 pieces of 24).
constexpr float LONGEST_PIECE_DISTANCE = 10000.f;

// The game's main view (the one the field's draw compares with the view being drawn) and
// what it holds: its projection type (0 perspective), its camera, and the view matrix
// (world to camera, 3x4, row by row) it drew the last frame with. The camera holds its
// vertical field of view in degrees and its aspect ratio.
constexpr uint32_t MAIN_VIEW_POINTER = 0x802AED70u;
constexpr uint32_t VIEW_CAMERA = 0x04u;
constexpr uint32_t VIEW_PROJECTION_TYPE = 0x34u;
constexpr uint32_t VIEW_MATRIX = 0xD0u;
constexpr uint32_t CAMERA_FIELD_OF_VIEW = 0x58u;
constexpr uint32_t CAMERA_ASPECT_RATIO = 0x5Cu;
constexpr int PERSPECTIVE = 0;

// A line of sight that looks down less steeply than this (the sine of its angle below the
// horizon) is taken as reaching the horizon.
constexpr double SMALLEST_DOWNWARD_LOOK = 0.05;

constexpr double INFINITE_REACH = std::numeric_limits<double>::infinity();

bool g_checked = false;
bool g_available = false;
float g_written = GAME_PIECE_DISTANCE;
// For the log: the zoom of the frame before, and the distance last written to the log,
// which tells it once the zoom has settled and the distance changed by this much.
float g_previous_zoom = 1.f;
float g_logged = GAME_PIECE_DISTANCE;
constexpr float LOGGED_CHANGE = 2.f;

float read_float(uint32_t address) { return std::bit_cast<float>(guest_memory::read32(address)); }

struct Vector {
    double x = 0, y = 0, z = 0;
};

// How far the screen's corners reach on the ground from the point the camera looks at,
// along the map's axes, in units of the camera's distance to that point: the ground is the
// level of that point, and the corners are where the projection's frustum meets it, given
// the tangents of half its width and half its height. Infinite when a corner looks at the
// horizon or above it.
double ground_reach(const std::array<float, 12>& view, double half_width, double half_height) {
    // The view matrix's rows are the camera's axes in the world; a direction in the camera
    // goes to the world through their sum, and the camera's position is -R^T t.
    const auto to_world = [&view](double x, double y, double z) {
        return Vector{x * view[0] + y * view[4] + z * view[8], x * view[1] + y * view[5] + z * view[9],
                      x * view[2] + y * view[6] + z * view[10]};
    };
    const Vector translation{view[3], view[7], view[11]};
    const Vector eye = to_world(-translation.x, -translation.y, -translation.z);
    const Vector ahead = to_world(0, 0, -1);  // the camera looks down its -z
    if (ahead.y > -SMALLEST_DOWNWARD_LOOK) {
        return INFINITE_REACH;
    }
    const Vector target{eye.x + ahead.x, eye.y + ahead.y, eye.z + ahead.z};
    double reach = 0;
    for (const double side : {-1.0, 1.0}) {
        for (const double up : {-1.0, 1.0}) {
            const Vector corner = to_world(side * half_width, up * half_height, -1);
            if (corner.y >
                -SMALLEST_DOWNWARD_LOOK * std::sqrt(corner.x * corner.x + corner.y * corner.y + corner.z * corner.z)) {
                return INFINITE_REACH;
            }
            const double along = ahead.y / corner.y;  // to the target's level
            reach = std::max(
                {reach, std::abs(eye.x + along * corner.x - target.x), std::abs(eye.z + along * corner.z - target.z)});
        }
    }
    return reach;
}

// How many times farther the camera shows the ground now than with the game's projection;
// 1 when it cannot be measured (no perspective view, or one that does not look down).
double reach_ratio() {
    const uint32_t view = guest_memory::read32(MAIN_VIEW_POINTER);
    if (!guest_memory::is_cached_main_memory(view) ||
        static_cast<int8_t>(guest_memory::read8(view + VIEW_PROJECTION_TYPE)) != PERSPECTIVE) {
        return 1;
    }
    const uint32_t camera = guest_memory::read32(view + VIEW_CAMERA);
    if (!guest_memory::is_cached_main_memory(camera)) {
        return 1;
    }
    const double field_of_view = read_float(camera + CAMERA_FIELD_OF_VIEW);
    const double aspect_ratio = read_float(camera + CAMERA_ASPECT_RATIO);
    if (!(field_of_view > 0 && field_of_view < 180 && aspect_ratio > 0)) {
        return 1;
    }
    std::array<float, 12> matrix{};
    for (size_t i = 0; i < matrix.size(); ++i) {
        matrix[i] = read_float(view + VIEW_MATRIX + static_cast<uint32_t>(i * sizeof(float)));
    }
    const double half_height = std::tan(field_of_view * std::numbers::pi / 360.0);
    const double half_width = half_height * aspect_ratio;
    const double game_reach = ground_reach(matrix, half_width, half_height);
    if (!(game_reach > 0) || std::isinf(game_reach)) {
        return 1;
    }
    return ground_reach(matrix, half_width / camera_view::screen_x_scale(),
                        half_height / camera_view::screen_y_scale()) /
           game_reach;
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
