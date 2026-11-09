// See main_view.h.

#include "graphics/main_view.h"

#include "graphics/camera_view.h"
#include "guest_memory.h"

#include <bit>
#include <cmath>
#include <cstdint>
#include <numbers>

namespace main_view {
namespace {

// The game's main view and what it holds: its projection type (0 perspective), its camera,
// and the view matrix (world to camera, 3x4, row by row) it drew the last frame with. The
// camera holds its vertical field of view in degrees and its aspect ratio.
constexpr uint32_t MAIN_VIEW_POINTER = 0x802AED70u;
constexpr uint32_t VIEW_CAMERA = 0x04u;
constexpr uint32_t VIEW_PROJECTION_TYPE = 0x34u;
constexpr uint32_t VIEW_MATRIX = 0xD0u;
constexpr uint32_t CAMERA_FIELD_OF_VIEW = 0x58u;
constexpr uint32_t CAMERA_ASPECT_RATIO = 0x5Cu;
constexpr int PERSPECTIVE = 0;
constexpr size_t VIEW_MATRIX_FLOATS = 12;

// A line of sight that looks down less steeply than this (the sine of its angle below the
// horizon) is taken as reaching the horizon.
constexpr double SMALLEST_DOWNWARD_LOOK = 0.05;

float read_float(uint32_t address) { return std::bit_cast<float>(guest_memory::read32(address)); }

// The camera of the main view: the view matrix, and the tangents of half the width and
// half the height of its projection.
struct Camera {
    std::array<float, VIEW_MATRIX_FLOATS> matrix{};
    double half_width = 0;
    double half_height = 0;
};

std::optional<Camera> read_camera() {
    const uint32_t view = guest_memory::read32(MAIN_VIEW_POINTER);
    if (!guest_memory::is_cached_main_memory(view) ||
        static_cast<int8_t>(guest_memory::read8(view + VIEW_PROJECTION_TYPE)) != PERSPECTIVE) {
        return std::nullopt;
    }
    const uint32_t camera_address = guest_memory::read32(view + VIEW_CAMERA);
    if (!guest_memory::is_cached_main_memory(camera_address)) {
        return std::nullopt;
    }
    const double field_of_view = read_float(camera_address + CAMERA_FIELD_OF_VIEW);
    const double aspect_ratio = read_float(camera_address + CAMERA_ASPECT_RATIO);
    if (!(field_of_view > 0 && field_of_view < 180 && aspect_ratio > 0)) {
        return std::nullopt;
    }
    Camera camera;
    for (size_t i = 0; i < camera.matrix.size(); ++i) {
        camera.matrix[i] = read_float(view + VIEW_MATRIX + static_cast<uint32_t>(i * sizeof(float)));
    }
    camera.half_height = std::tan(field_of_view * std::numbers::pi / 360.0);
    camera.half_width = camera.half_height * aspect_ratio;
    return camera;
}

// Where the corners of a projection of the given half width and half height meet the
// level of the point one unit ahead of the camera, as offsets from that point. False when
// the camera or a corner looks at the horizon or above it.
bool corners_on_ground(const std::array<float, VIEW_MATRIX_FLOATS>& view, double half_width, double half_height,
                       std::array<Vector, 4>& corners) {
    // The view matrix's rows are the camera's axes in the world; a direction in the camera
    // goes to the world through their sum.
    const auto to_world = [&view](double x, double y, double z) {
        return Vector{x * view[0] + y * view[4] + z * view[8], x * view[1] + y * view[5] + z * view[9],
                      x * view[2] + y * view[6] + z * view[10]};
    };
    const Vector ahead = to_world(0, 0, -1);  // the camera looks down its -z
    if (ahead.y > -SMALLEST_DOWNWARD_LOOK) {
        return false;
    }
    size_t index = 0;
    for (const double side : {-1.0, 1.0}) {
        for (const double up : {-1.0, 1.0}) {
            const Vector corner = to_world(side * half_width, up * half_height, -1);
            const double length = std::sqrt(corner.x * corner.x + corner.y * corner.y + corner.z * corner.z);
            if (corner.y > -SMALLEST_DOWNWARD_LOOK * length) {
                return false;
            }
            const double along = ahead.y / corner.y;  // down to the level of the point ahead
            corners[index++] = {along * corner.x - ahead.x, 0, along * corner.z - ahead.z};
        }
    }
    return true;
}

}  // namespace

std::optional<GroundCorners> ground_corners() {
    const std::optional<Camera> camera = read_camera();
    if (!camera) {
        return std::nullopt;
    }
    GroundCorners corners;
    if (!corners_on_ground(camera->matrix, camera->half_width, camera->half_height, corners.game)) {
        return std::nullopt;
    }
    corners.now_reaches_horizon = !corners_on_ground(camera->matrix, camera->half_width / camera_view::screen_x_scale(),
                                                     camera->half_height / camera_view::screen_y_scale(), corners.now);
    return corners;
}

}  // namespace main_view
