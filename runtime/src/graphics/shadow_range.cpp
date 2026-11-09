// See shadow_range.h.

#include "graphics/shadow_range.h"

#include "graphics/camera_view.h"
#include "graphics/main_view.h"
#include "guest_memory.h"
#include "runtime_log.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cmath>
#include <cstdint>
#include <numbers>
#include <optional>

namespace shadow_range {
namespace {

// The light's view, and what it holds as every view does (main_view.cpp): its projection
// type (0 perspective), its camera, and its view matrix (world to camera, 3x4, row by
// row). The camera holds its vertical field of view in degrees and its aspect ratio.
constexpr uint32_t LIGHT_VIEW_POINTER = 0x802AEDA0u;
constexpr uint32_t VIEW_CAMERA = 0x04u;
constexpr uint32_t VIEW_PROJECTION_TYPE = 0x34u;
constexpr uint32_t VIEW_MATRIX = 0xD0u;
constexpr uint32_t CAMERA_FIELD_OF_VIEW = 0x58u;
constexpr uint32_t CAMERA_ASPECT_RATIO = 0x5Cu;
constexpr int PERSPECTIVE = 0;
// The light camera as the game makes it: square, and narrow (1 degree). A camera that is
// not square, or a field of view outside these, is taken as not the shadows' one.
constexpr float LIGHT_ASPECT_RATIO = 1.f;
constexpr float WIDEST_GAME_FIELD_OF_VIEW = 10.f;
// The most the area is widened (when the screen's corners reach the horizon).
constexpr double LARGEST_WIDENING = 8;

// The light camera's field of view as the game set it, and the one the runtime last wrote:
// a value that is neither means the game set a new one.
float g_game_field_of_view = 0.f;
float g_written_field_of_view = 0.f;
// For the log: the zoom of the frame before, and the widening last written to the log,
// which tells it once the zoom has settled and the widening changed by this much.
float g_previous_zoom = 1.f;
double g_logged_widening = 1;
constexpr double LOGGED_CHANGE = 0.25;

float read_float(uint32_t address) { return std::bit_cast<float>(guest_memory::read32(address)); }

struct LightView {
    uint32_t view = 0;
    uint32_t camera = 0;
};

// The light's view and its camera, when they are the shadows' as the game makes them.
std::optional<LightView> light_view() {
    const uint32_t view = guest_memory::read32(LIGHT_VIEW_POINTER);
    if (!guest_memory::is_cached_main_memory(view) ||
        static_cast<int8_t>(guest_memory::read8(view + VIEW_PROJECTION_TYPE)) != PERSPECTIVE) {
        return std::nullopt;
    }
    const uint32_t camera = guest_memory::read32(view + VIEW_CAMERA);
    if (!guest_memory::is_cached_main_memory(camera) ||
        read_float(camera + CAMERA_ASPECT_RATIO) != LIGHT_ASPECT_RATIO) {
        return std::nullopt;
    }
    return LightView{view, camera};
}

// How many times farther the screen's corners reach now than with the game's projection,
// along the light camera's horizontal and vertical axes (the depth map is a square along
// them); 1 when it cannot be measured.
double reach_ratio(uint32_t light_view_address) {
    const std::optional<main_view::GroundCorners> corners = main_view::ground_corners();
    if (!corners) {
        return 1;
    }
    // Rows 0 and 1 of the light's view matrix: its camera's right and up in the world.
    std::array<float, 8> axes{};
    for (size_t i = 0; i < axes.size(); ++i) {
        axes[i] = read_float(light_view_address + VIEW_MATRIX + static_cast<uint32_t>(i * sizeof(float)));
    }
    return main_view::reach_ratio(*corners, [&axes](const main_view::Vector& offset) {
        const double right = offset.x * axes[0] + offset.y * axes[1] + offset.z * axes[2];
        const double up = offset.x * axes[4] + offset.y * axes[5] + offset.z * axes[6];
        return std::max(std::abs(right), std::abs(up));
    });
}

// The field of view whose half covers `widening` times the ground the game's half covers.
float widened_field_of_view(float game_field_of_view, double widening) {
    const double half_angle = game_field_of_view * std::numbers::pi / 360.0;
    return static_cast<float>(std::atan(std::tan(half_angle) * widening) * 360.0 / std::numbers::pi);
}

void log_widening(double widening, float field_of_view) {
    const float zoom = camera_view::zoom();
    if (zoom == g_previous_zoom && std::abs(widening - g_logged_widening) >= LOGGED_CHANGE) {
        runtime_log::info("graphics",
                          "shadows: cover %.1f times as far as the game's (light field of view %.2f, the game's %.2f)",
                          widening, field_of_view, g_game_field_of_view);
        g_logged_widening = widening;
    }
    g_previous_zoom = zoom;
}

}  // namespace

void update() {
    const std::optional<LightView> light = light_view();
    if (!light) {
        return;
    }
    const float current = read_float(light->camera + CAMERA_FIELD_OF_VIEW);
    if (current != g_written_field_of_view) {
        g_game_field_of_view = current;  // the game set it
    }
    if (!(g_game_field_of_view > 0.f && g_game_field_of_view < WIDEST_GAME_FIELD_OF_VIEW)) {
        return;  // not the field of view the game gives its shadows
    }
    const double widening = camera_view::zoom() < 1.f ? std::min(reach_ratio(light->view), LARGEST_WIDENING) : 1.0;
    const float field_of_view =
        widening > 1 ? widened_field_of_view(g_game_field_of_view, widening) : g_game_field_of_view;
    if (field_of_view != current) {
        guest_memory::write32(light->camera + CAMERA_FIELD_OF_VIEW, std::bit_cast<uint32_t>(field_of_view));
    }
    g_written_field_of_view = field_of_view;
    log_widening(widening, field_of_view);
}

}  // namespace shadow_range
