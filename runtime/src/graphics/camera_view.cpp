// See camera_view.h.

#include "graphics/camera_view.h"

#include "runtime_log.h"

#include <dolphin/gx.h>

#include <algorithm>
#include <array>
#include <cmath>

namespace camera_view {
namespace {

constexpr int PERSPECTIVE = 0;
constexpr float GAME_ASPECT_RATIO = 4.f / 3.f;

Options g_options;
// The zoom now: the chosen one while the player walks outdoors, otherwise 1, reached a step
// a frame.
float g_zoom = 1.f;
// Whether the zoom goes to the chosen one, and for how many frames in a row the player has
// walked outdoors: the zoom waits for a few, because the game passes through that state for
// a frame between the title screen and its demo.
bool g_zooming = false;
int g_frames_outdoors = 0;
constexpr int FRAMES_BEFORE_ZOOMING = 10;
// Each frame of the game covers this part of the way left to the zoom it is going to:
// about a third of a second from one to the other.
constexpr float ZOOM_STEP = 0.2f;
// Closer than this, the zoom is where it goes.
constexpr float ZOOM_REACHED = 0.002f;
// Whether the game draws to the whole screen now, rather than to a picture of its own: the
// depth map of the shadows is drawn from the light into a 256x256 viewport, and widening or
// zooming that projection would move the shadows. The game sets the viewport before the
// projection of each pass.
bool g_viewport_covers_screen = true;
// The smallest viewport taken as the screen (the game's is 640x448).
constexpr float SCREEN_VIEWPORT_WIDTH = 600.f;
constexpr float SCREEN_VIEWPORT_HEIGHT = 400.f;
// In widescreen, the width of the game's 4:3 picture in the window (3/4 in a 16:9 one).
float g_four_thirds_in_window = 1.f;
// The window's size, as aurora last gave it.
unsigned g_window_width = 0;
unsigned g_window_height = 0;

// The last projection as the game set it, and how much its rows 0 and 1 were scaled (1
// when it was left as it is), for the texture matrices built from it.
std::array<float, 16> g_game_projection{};
float g_applied_x_scale = 1.f;
float g_applied_y_scale = 1.f;
// The last position matrix of slot 0, the one the game's draws use.
std::array<float, 12> g_position_matrix{};

// How close a texture matrix must be to a screen lookup, relative to its size: the game
// builds them from the same floats, so they match to rounding.
constexpr double SCREEN_LOOKUP_TOLERANCE = 1e-3;

// Rows of the game's projection times the position matrix: where a vertex lands before the
// divide (x and y), and the divisor (w).
struct ClipRows {
    std::array<double, 4> x{};
    std::array<double, 4> y{};
    std::array<double, 4> w{};
};

ClipRows clip_rows() {
    // The position matrix is 3x4; its missing row 3 is (0, 0, 0, 1).
    const auto row_times_position = [](int row) {
        std::array<double, 4> result{};
        for (int column = 0; column < 4; ++column) {
            double sum = column == 3 ? g_game_projection[row * 4 + 3] : 0.0;
            for (int k = 0; k < 3; ++k) {
                sum += static_cast<double>(g_game_projection[row * 4 + k]) * g_position_matrix[k * 4 + column];
            }
            result[column] = sum;
        }
        return result;
    };
    return {row_times_position(0), row_times_position(1), row_times_position(3)};
}

double length_of(const std::array<double, 4>& row) {
    return std::sqrt(row[0] * row[0] + row[1] * row[1] + row[2] * row[2] + row[3] * row[3]);
}

// Fits a texture matrix row as scale * clip + offset * w (least squares), which is how a
// screen lookup maps the screen to the texture. Gives the offset (the texture coordinate
// of the screen's centre), and false when the row is not of that form.
bool fit_screen_mapping(const float* row, const std::array<double, 4>& clip, const std::array<double, 4>& w,
                        double& offset) {
    double cc = 0, cw = 0, ww = 0, cr = 0, wr = 0, rr = 0;
    for (int i = 0; i < 4; ++i) {
        cc += clip[i] * clip[i];
        cw += clip[i] * w[i];
        ww += w[i] * w[i];
        cr += clip[i] * row[i];
        wr += w[i] * row[i];
        rr += static_cast<double>(row[i]) * row[i];
    }
    const double determinant = cc * ww - cw * cw;
    if (std::abs(determinant) <= 1e-12 * cc * ww || rr == 0) {
        return false;
    }
    const double scale = (cr * ww - wr * cw) / determinant;
    offset = (cc * wr - cw * cr) / determinant;
    double error = 0;
    for (int i = 0; i < 4; ++i) {
        const double difference = row[i] - (scale * clip[i] + offset * w[i]);
        error += difference * difference;
    }
    return std::sqrt(error / rr) <= SCREEN_LOOKUP_TOLERANCE;
}

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
    g_window_width = width;
    g_window_height = height;
    if (g_options.widescreen && width != 0 && height != 0) {
        g_four_thirds_in_window = GAME_ASPECT_RATIO * static_cast<float>(height) / static_cast<float>(width);
    }
}

void set_widescreen(bool widescreen) {
    if (widescreen == g_options.widescreen) {
        return;
    }
    g_options.widescreen = widescreen;
    // aurora fits the 4:3 picture in the window with bars by default, and stretches it in widescreen.
    AuroraSetViewportPolicy(widescreen ? AURORA_VIEWPORT_STRETCH : AURORA_VIEWPORT_FIT);
    g_four_thirds_in_window = 1.f;
    set_window_size(g_window_width, g_window_height);
    runtime_log::info("graphics", "camera: %s", widescreen ? "widescreen" : "4:3");
}

void set_zoom(float zoom) { g_options.zoom = std::clamp(zoom, MINIMUM_ZOOM, MAXIMUM_ZOOM); }

bool widescreen() { return g_options.widescreen; }

void follow_scene(bool zoom_applies) {
    g_frames_outdoors = zoom_applies ? std::min(g_frames_outdoors + 1, FRAMES_BEFORE_ZOOMING) : 0;
    const bool zooming = g_frames_outdoors == FRAMES_BEFORE_ZOOMING;
    if (zooming != g_zooming && g_options.zoom != 1.f) {
        runtime_log::info("graphics",
                          zooming ? "camera: zoom %.2f, walking outdoors"
                                  : "camera: the game's (zoom %.2f off: a cutscene, a room or the title)",
                          g_options.zoom);
    }
    g_zooming = zooming;
    const float goal = zooming ? g_options.zoom : 1.f;
    g_zoom += (goal - g_zoom) * ZOOM_STEP;
    if (std::abs(goal - g_zoom) < ZOOM_REACHED) {
        g_zoom = goal;
    }
}

float zoom() { return g_zoom; }

float screen_x_scale() { return g_four_thirds_in_window * g_zoom; }

float screen_y_scale() { return g_zoom; }

void set_viewport(float width, float height) {
    g_viewport_covers_screen = width >= SCREEN_VIEWPORT_WIDTH && height >= SCREEN_VIEWPORT_HEIGHT;
}

void adjust_projection(float matrix[16], int type) {
    std::copy(matrix, matrix + 16, g_game_projection.begin());
    g_applied_x_scale = 1.f;
    g_applied_y_scale = 1.f;
    if (type != PERSPECTIVE || !g_viewport_covers_screen) {
        return;
    }
    // Rows 0 and 1 map the view's x and y to the screen: their scale and, for an
    // off-centre frustum, their offset change together, which keeps the frustum's centre.
    g_applied_x_scale = screen_x_scale();
    g_applied_y_scale = screen_y_scale();
    for (int column = 0; column < 4; ++column) {
        matrix[column] *= g_applied_x_scale;
        matrix[4 + column] *= g_applied_y_scale;
    }
}

void adjust_projection_numbers(float numbers[7], float viewport_width, float viewport_height) {
    if (numbers[0] != PERSPECTIVE || viewport_width < SCREEN_VIEWPORT_WIDTH ||
        viewport_height < SCREEN_VIEWPORT_HEIGHT) {
        return;
    }
    // The entries of rows 0 and 1, as adjust_projection scales them (01, 03 and 13 are 0 in
    // a perspective projection, and GXProject leaves them out).
    numbers[1] *= screen_x_scale();
    numbers[2] *= screen_x_scale();
    numbers[3] *= screen_y_scale();
    numbers[4] *= screen_y_scale();
}

void set_position_matrix(const float matrix[12], uint32_t slot) {
    if (slot == GX_PNMTX0) {
        std::copy(matrix, matrix + 12, g_position_matrix.begin());
    }
}

void adjust_texture_matrix(float matrix[12], int type) {
    if (type != GX_MTX3x4 || (g_applied_x_scale == 1.f && g_applied_y_scale == 1.f)) {
        return;
    }
    // A screen lookup is S * projection * position matrix, where S maps the clip space to
    // the texture: s = a x + c w, t = d y + e w, q = w. Its row 2 is then the clip w row.
    const ClipRows clip = clip_rows();
    double w_error = 0;
    for (int i = 0; i < 4; ++i) {
        w_error += (matrix[8 + i] - clip.w[i]) * (matrix[8 + i] - clip.w[i]);
    }
    double x_offset = 0;
    double y_offset = 0;
    if (std::sqrt(w_error) > SCREEN_LOOKUP_TOLERANCE * length_of(clip.w) ||
        !fit_screen_mapping(matrix, clip.x, clip.w, x_offset) ||
        !fit_screen_mapping(matrix + 4, clip.y, clip.w, y_offset)) {
        return;
    }
    // With the projection's rows scaled by k, the lookup becomes s' = k a x + c w, which is
    // k s + (1 - k) c w: the same scale around the screen's centre.
    for (int i = 0; i < 4; ++i) {
        matrix[i] =
            static_cast<float>(g_applied_x_scale * matrix[i] + (1.0 - g_applied_x_scale) * x_offset * clip.w[i]);
        matrix[4 + i] =
            static_cast<float>(g_applied_y_scale * matrix[4 + i] + (1.0 - g_applied_y_scale) * y_offset * clip.w[i]);
    }
}

}  // namespace camera_view
