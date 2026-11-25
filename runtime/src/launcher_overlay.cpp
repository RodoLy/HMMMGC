// See launcher_overlay.h.

#include "launcher_overlay.h"

#if HMMM_LAUNCHER_IN_THE_GAME

#include "android_app.h"
#include "controls_editor.h"
#include "game_clock.h"
#include "in_app.h"
#include "input/touch_buttons.h"
#include "lettering.h"
#include "look.h"
#include "mods/native_mods.h"
#include "paths.h"
#include "player_folder.h"
#include "player_options.h"
#include "runtime_log.h"
#include "theme.h"

#include <aurora/aurora.h>
#include <aurora/event.h>
#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>

#include <SDL3/SDL_timer.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <thread>

#endif

namespace launcher_overlay {

namespace {
bool g_requested = false;
}  // namespace

void request() { g_requested = true; }

bool requested() { return g_requested; }

#if HMMM_LAUNCHER_IN_THE_GAME

namespace {

// The launcher's design as big as it fits in the window, centered, as in in_app.cpp; the
// lettering for that size is made before the frame (aurora makes the font texture again when it
// starts the frame).
void fit_the_design(SDL_Window* window) {
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(window, &width, &height);
    const float scale = std::round(std::min(static_cast<float>(width) / look::WIDTH,
                                            static_cast<float>(height) / look::HEIGHT) * 100.f) / 100.f;
    look::set_canvas(ImVec2(std::round((static_cast<float>(width) - look::WIDTH * scale) * 0.5f),
                            std::round((static_cast<float>(height) - look::HEIGHT * scale) * 0.5f)),
                     scale);
    lettering::build(scale);
}

}  // namespace

bool show() {
    runtime_log::info("launcher", "the launcher is over the game; the game waits");
    game_clock::stop();
    SDL_Window* window = aurora_get_window();
    ImGuiIO& io = ImGui::GetIO();
    const ImGuiConfigFlags flags_before = io.ConfigFlags;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoFirst);
    lettering::forget();

    bool exit = false;
    {
        InApp in_app{.player = paths::player_in(player_folder::path()),
                     .mods = native_mods::compiled_in(),
                     .manifest = in_app::mod_manifest,
                     .send_log = android_app::share_log,
                     .in_game = true};
        App app(window, std::move(in_app), false);
        while (!app.wants_to_quit() && !app.wants_to_play()) {
            for (const AuroraEvent* event = aurora_update(); event != nullptr && event->type != AURORA_NONE; ++event) {
                if (event->type == AURORA_EXIT) {
                    exit = true;
                } else if (event->type == AURORA_SDL_EVENT) {
                    app.handle_event(event->sdl);
                    // The touch buttons follow the fingers, so that none stays pressed when the
                    // game goes on (the Options button only asks again, which is undone below).
                    touch_buttons::handle_event(event->sdl);
                }
            }
            if (exit) {
                break;
            }
            app.update();
            fit_the_design(window);
            if (!aurora_begin_frame()) {
                std::this_thread::sleep_for(std::chrono::milliseconds(16));  // in the background
                continue;
            }
            // Behind the design, the launcher's dark blue, over the stopped game's picture.
            ImGui::GetBackgroundDrawList()->AddRectFilled(ImVec2(0.f, 0.f), io.DisplaySize,
                                                          IM_COL32(0x0b, 0x0a, 0x26, 0xff));
            app.frame(static_cast<double>(SDL_GetTicks()) / 1000.0);
            aurora_end_frame();
        }
        exit = exit || app.wants_to_quit();
    }

    // The game's lettering again, and the options as the launcher left them.
    lettering::forget();
    theme::forget_fonts();
    io.ConfigFlags = flags_before;
    player_options::reload();
    controls_editor::apply_saved();
    g_requested = false;
    game_clock::go_on();
    runtime_log::info("launcher", exit ? "Exit in the launcher: the game ends" : "back to the game");
    return !exit;
}

#else

bool show() {
    g_requested = false;
    return true;
}

#endif

}  // namespace launcher_overlay
