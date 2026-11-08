// See aurora_host.h.

#include "graphics/aurora_host.h"

#include "audio/audio_output.h"
#include "game_clock.h"
#include "game_scene.h"
#include "graphics/camera_view.h"
#include "graphics/frame_interpolation.h"
#include "player_folder.h"
#include "runtime_log.h"

#include <aurora/aurora.h>
#include <aurora/event.h>
#include <aurora/gfx.h>
#include <dolphin/vi.h>

#include <SDL3/SDL.h>

#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <string>
#include <thread>

namespace aurora_host {
namespace {

constexpr const char* WINDOW_TITLE = "Harvest Moon: Magical Melody";
// The window starts at the size of the console's picture and can be resized. (The game
// copies 640x448 to the screen, and the video interface centres it in the 480 lines
// with a border above and below; aurora keeps its proportions the same way.)
constexpr uint32_t WINDOW_WIDTH = 640;
constexpr uint32_t WIDESCREEN_WINDOW_WIDTH = 854;
constexpr uint32_t WINDOW_HEIGHT = 480;
// How long to wait for the window to accept frames again (while it is minimized).
constexpr auto WAIT_FOR_WINDOW = std::chrono::milliseconds(16);

bool g_initialized = false;
bool g_window_closed = false;

// Where aurora keeps its settings (controller mappings, texture replacements) and the
// memory card, and where it keeps its caches (compiled pipelines).
DisplayOptions g_display_options;
std::string g_user_folder;
std::string g_cache_folder;

// The frame rate, counted over each second and shown in the window's title with the
// game's speed when it is not 100% (the game presents 30 pictures a second at 100%; fewer
// means the PC is not keeping up).
struct FrameRate {
    std::chrono::steady_clock::time_point second_started = std::chrono::steady_clock::now();
    unsigned frames = 0;
};
FrameRate g_frame_rate;

void count_frame() {
    ++g_frame_rate.frames;
    const auto now = std::chrono::steady_clock::now();
    const std::chrono::duration<double> elapsed = now - g_frame_rate.second_started;
    if (elapsed.count() < 1.0) {
        return;
    }
    // With frames made in between, aurora shows them from its render thread: it counts them.
    double frames_per_second = g_frame_rate.frames / elapsed.count();
    if (frame_interpolation::is_on()) {
        AuroraPresentTiming timing{};
        aurora_get_present_timing(&timing);
        frames_per_second = timing.framesPerSecond;
    }
    char title[128];
    const unsigned speed = game_clock::speed_percent();
    if (speed == game_clock::NORMAL_SPEED_PERCENT) {
        std::snprintf(title, sizeof(title), "%s - %.0f fps", WINDOW_TITLE, frames_per_second);
    } else {
        std::snprintf(title, sizeof(title), "%s - %.0f fps - %u%%", WINDOW_TITLE, frames_per_second, speed);
    }
    SDL_SetWindowTitle(aurora_get_window(), title);
    g_frame_rate = FrameRate{now, 0};
}

void log_from_aurora(AuroraLogLevel level, const char* module, const char* message, unsigned int length) {
    const int text_length = static_cast<int>(length);
    switch (level) {
        case LOG_DEBUG:
        case LOG_INFO:
            runtime_log::info("aurora", "%s: %.*s", module, text_length, message);
            return;
        case LOG_WARNING:
            runtime_log::warning("aurora", "%s: %.*s", module, text_length, message);
            return;
        case LOG_ERROR:
        case LOG_FATAL:
            runtime_log::error("aurora", "%s: %.*s", module, text_length, message);
            return;
    }
}

// aurora draws into the frame opened by aurora_begin_frame. It refuses to open one while
// the window cannot show anything (minimized), so wait for it, still handling events.
void begin_next_frame() {
    while (!aurora_begin_frame()) {
        if (!process_events()) {
            return;
        }
        std::this_thread::sleep_for(WAIT_FOR_WINDOW);
    }
}

void exit_if_window_closed() {
    if (g_window_closed) {
        runtime_log::info("graphics", "the window was closed");
        shutdown();
        std::exit(EXIT_SUCCESS);
    }
}

}  // namespace

void configure(const DisplayOptions& options) { g_display_options = options; }

void initialize() {
    if (g_initialized) {
        return;
    }
    const std::filesystem::path cache_folder = player_folder::path() / "Cache";
    std::filesystem::create_directories(cache_folder);  // aurora's databases do not create it
    g_user_folder = player_folder::path().string();
    g_cache_folder = cache_folder.string();

    AuroraConfig config{};
    config.appName = WINDOW_TITLE;
    config.desiredBackend = BACKEND_AUTO;
    // A software device (such as Mesa's lavapipe) is accepted when there is no GPU, which
    // is how the runtime is tested on machines without one.
    config.allowCpuAdapter = true;
    // The game paces itself on the video interface's retraces (hardware/video_interface.h);
    // waiting for the screen's refresh as well would pace it twice.
    config.vsync = false;
    // A 16:9 window for the widescreen picture: as tall as the 4:3 one, and wider.
    config.windowWidth = camera_view::widescreen() ? WIDESCREEN_WINDOW_WIDTH : WINDOW_WIDTH;
    config.windowHeight = WINDOW_HEIGHT;
    config.startFullscreen = g_display_options.fullscreen;
    config.logCallback = log_from_aurora;
    config.logLevel = LOG_INFO;
    // The game's memory belongs to the runtime (guest_memory.h); aurora needs none of its own.
    config.mem1Size = 0;
    config.mem2Size = 0;
    // Given explicitly: aurora's default is a folder named after the window title, which
    // has a colon, and Windows does not allow one in a folder name.
    config.userPath = g_user_folder.c_str();
    config.cachePath = g_cache_folder.c_str();

    const AuroraInfo info = aurora_initialize(0, nullptr, &config);
    if (info.initializationStatus != AURORA_INITIALIZATION_SUCCESS) {
        runtime_log::error("graphics", "no graphics device can be used: %s",
                           info.initializationError != nullptr ? info.initializationError : "unknown error");
        std::exit(EXIT_FAILURE);
    }
    camera_view::apply_to_aurora();
    frame_interpolation::apply_to_aurora();
    camera_view::set_window_size(info.windowSize.width, info.windowSize.height);
    if (g_display_options.resolution_scale > 0.f) {
        VISetFrameBufferScale(g_display_options.resolution_scale);
        // The picture is drawn larger than the window and reduced to it: averaging every
        // pixel it covers (rather than the four nearest, bilinear) keeps the edges smooth
        // without the shimmer of skipped pixels.
        aurora_set_resampler(SAMPLER_AREA);
    }
    runtime_log::info("graphics", "aurora is drawing with backend %d in a %ux%u window, internal resolution %s",
                      static_cast<int>(info.backend), info.windowSize.width, info.windowSize.height,
                      g_display_options.resolution_scale > 0.f ? "fixed" : "following the window");
    g_initialized = true;
    begin_next_frame();
    exit_if_window_closed();
}

bool is_initialized() { return g_initialized; }

void present_frame() {
    frame_interpolation::schedule_frame();
    aurora_end_frame();
    count_frame();
    process_events();
    // What the next frame shows: the game's camera, or the zoom while walking outdoors.
    camera_view::follow_scene(game_scene::player_walks_outdoors());
    // The turbo: the whole game runs at game_clock::TURBO_SPEED_PERCENT while Tab is held.
    game_clock::set_turbo(SDL_GetKeyboardState(nullptr)[SDL_SCANCODE_TAB]);
    begin_next_frame();
    exit_if_window_closed();
}

bool process_events() {
    for (const AuroraEvent* event = aurora_update(); event != nullptr && event->type != AURORA_NONE; ++event) {
        if (event->type == AURORA_EXIT) {
            g_window_closed = true;
        } else if (event->type == AURORA_WINDOW_RESIZED) {
            const AuroraWindowSize& size = event->windowSize;
            camera_view::set_window_size(size.width, size.height);
            runtime_log::info("graphics", "window %ux%u, the game is drawn at %ux%u", size.width, size.height,
                              size.fb_width, size.fb_height);
        }
    }
    return !g_window_closed;
}

void shutdown() {
    if (g_initialized) {
        g_initialized = false;
        audio_output::shutdown();  // the sound plays through the SDL that aurora started
        aurora_shutdown();
    }
}

}  // namespace aurora_host
