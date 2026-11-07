// See aurora_host.h.

#include "graphics/aurora_host.h"

#include "audio/audio_output.h"
#include "player_folder.h"
#include "runtime_log.h"

#include <aurora/aurora.h>
#include <aurora/event.h>

#include <chrono>
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
constexpr uint32_t WINDOW_HEIGHT = 480;
// How long to wait for the window to accept frames again (while it is minimized).
constexpr auto WAIT_FOR_WINDOW = std::chrono::milliseconds(16);

bool g_initialized = false;
bool g_window_closed = false;

// Where aurora keeps its settings (controller mappings, texture replacements) and the
// memory card, and where it keeps its caches (compiled pipelines).
std::string g_user_folder;
std::string g_cache_folder;

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
    config.windowWidth = WINDOW_WIDTH;
    config.windowHeight = WINDOW_HEIGHT;
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
    runtime_log::info("graphics", "aurora is drawing with backend %d in a %ux%u window", static_cast<int>(info.backend),
                      info.windowSize.width, info.windowSize.height);
    g_initialized = true;
    begin_next_frame();
    exit_if_window_closed();
}

bool is_initialized() { return g_initialized; }

void present_frame() {
    aurora_end_frame();
    process_events();
    begin_next_frame();
    exit_if_window_closed();
}

bool process_events() {
    for (const AuroraEvent* event = aurora_update(); event != nullptr && event->type != AURORA_NONE; ++event) {
        if (event->type == AURORA_EXIT) {
            g_window_closed = true;
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
