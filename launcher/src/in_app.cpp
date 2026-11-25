// See in_app.h. The same frame as the launcher's main.cpp, in a window that fills the phone's screen.

#include "in_app.h"

#include "lettering.h"
#include "look.h"

#include <SDL3/SDL.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <cmath>

namespace in_app {

namespace {

constexpr const char* WINDOW_TITLE = "Harvest Moon: Magical Melody";

}  // namespace

Result run(InApp in_app, bool still) {
    Result result;
    if (!SDL_InitSubSystem(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        result.play = true;  // without a window there is no launcher: the game, as before it had one
        return result;
    }
#if defined(__ANDROID__)
    // The phone's whole screen, as the game.
    SDL_Window* window = SDL_CreateWindow(WINDOW_TITLE, 0, 0, SDL_WINDOW_FULLSCREEN);
#else
    // On a computer, the size of a phone's screen in landscape (20:9), to see it as there.
    SDL_Window* window = SDL_CreateWindow(WINDOW_TITLE, 1200, 540, 0);
#endif
    SDL_Renderer* renderer = window != nullptr ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        if (window != nullptr) {
            SDL_DestroyWindow(window);
        }
        result.play = true;
        return result;
    }
    SDL_SetRenderVSync(renderer, 1);

    ImGuiContext* context = ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoAll);
    lettering::forget();  // the fonts are made again for this ImGui (the game's is another)

    {
        App app(window, std::move(in_app), still);
        while (!app.wants_to_quit() && !app.wants_to_play()) {
            SDL_Event event;
            bool quit = false;
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL3_ProcessEvent(&event);
                quit |= event.type == SDL_EVENT_QUIT;
                quit |= event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED;
                app.handle_event(event);
            }
            if (quit) {
                break;
            }
            app.update();
            // In the background (another app on the screen), nothing is drawn.
            if ((SDL_GetWindowFlags(window) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) != 0) {
                SDL_Delay(50);
                continue;
            }
            // The design as big as it fits, centered (in hundredths, as the fonts are made).
            int width = 0;
            int height = 0;
            SDL_GetWindowSizeInPixels(window, &width, &height);
            const float scale = std::round(std::min(static_cast<float>(width) / look::WIDTH,
                                                    static_cast<float>(height) / look::HEIGHT) * 100.f) / 100.f;
            look::set_canvas(ImVec2(std::round((static_cast<float>(width) - look::WIDTH * scale) * 0.5f),
                                    std::round((static_cast<float>(height) - look::HEIGHT * scale) * 0.5f)),
                             scale);
            if (lettering::build(scale)) {
                ImGui_ImplSDLRenderer3_DestroyFontsTexture();
                ImGui_ImplSDLRenderer3_CreateFontsTexture();
            }
            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            app.frame(static_cast<double>(SDL_GetTicks()) / 1000.0);
            ImGui::Render();
            SDL_SetRenderDrawColor(renderer, 0x0b, 0x0a, 0x26, 255);
            SDL_RenderClear(renderer);
            ImGui_ImplSDLRenderer3_RenderDrawData(ImGui::GetDrawData(), renderer);
            SDL_RenderPresent(renderer);
        }
        result.play = app.wants_to_play();
        result.environment = app.game_environment();
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext(context);
    lettering::forget();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    return result;
}

}  // namespace in_app
