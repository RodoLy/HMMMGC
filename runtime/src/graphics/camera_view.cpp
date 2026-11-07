// See camera_view.h.

#include "graphics/camera_view.h"

#include "runtime_log.h"

#include <algorithm>

namespace camera_view {
namespace {

constexpr int PERSPECTIVE = 0;

Options g_options;

}  // namespace

void configure(const Options& options) {
    g_options.zoom = std::clamp(options.zoom, MINIMUM_ZOOM, MAXIMUM_ZOOM);
    if (g_options.zoom != 1.f) {
        runtime_log::info("graphics", "camera zoom %.2f", g_options.zoom);
    }
}

void adjust_projection(float matrix[16], int type) {
    if (type != PERSPECTIVE || g_options.zoom == 1.f) {
        return;
    }
    // Rows 0 and 1 map the view's x and y to the screen: their scale and, for an
    // off-centre frustum, their offset grow together, which keeps the frustum's centre.
    for (int column = 0; column < 4; ++column) {
        matrix[column] *= g_options.zoom;
        matrix[4 + column] *= g_options.zoom;
    }
}

}  // namespace camera_view
