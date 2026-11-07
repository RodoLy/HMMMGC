// See camera_view.h.

#include "graphics/camera_view.h"

#include "runtime_log.h"

#include <dolphin/gx.h>

#include <algorithm>

namespace camera_view {
namespace {

constexpr int PERSPECTIVE = 0;
constexpr float GAME_ASPECT_RATIO = 4.f / 3.f;

Options g_options;
// In widescreen, the width of the game's 4:3 picture in the window (3/4 in a 16:9 one).
float g_four_thirds_in_window = 1.f;

}  // namespace

void configure(const Options& options) {
    g_options = options;
    g_options.zoom = std::clamp(options.zoom, MINIMUM_ZOOM, MAXIMUM_ZOOM);
}

void apply_to_aurora() {
    if (g_options.widescreen) {
        AuroraSetViewportPolicy(AURORA_VIEWPORT_STRETCH);
    }
    if (g_options.widescreen || g_options.zoom != 1.f) {
        runtime_log::info("graphics", "camera: %s, zoom %.2f", g_options.widescreen ? "widescreen" : "4:3",
                          g_options.zoom);
    }
}

void set_window_size(unsigned width, unsigned height) {
    if (g_options.widescreen && width != 0 && height != 0) {
        g_four_thirds_in_window = GAME_ASPECT_RATIO * static_cast<float>(height) / static_cast<float>(width);
    }
}

bool widescreen() { return g_options.widescreen; }

void adjust_projection(float matrix[16], int type) {
    if (type != PERSPECTIVE) {
        return;
    }
    // Rows 0 and 1 map the view's x and y to the screen: their scale and, for an
    // off-centre frustum, their offset change together, which keeps the frustum's centre.
    const float x_scale = g_four_thirds_in_window * g_options.zoom;
    const float y_scale = g_options.zoom;
    for (int column = 0; column < 4; ++column) {
        matrix[column] *= x_scale;
        matrix[4 + column] *= y_scale;
    }
}

}  // namespace camera_view
