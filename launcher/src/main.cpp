// The launcher of the native version of Harvest Moon: Magical Melody (app.h): a window drawn with
// SDL3's 2D renderer and Dear ImGui, after its design (view.cpp). The window has no frame of the
// system: the design draws its own title bar, from which it is dragged.
//
// El launcher de la versión nativa de Harvest Moon: Magical Melody: una ventana dibujada con el
// renderizador 2D de SDL3 y Dear ImGui, según su diseño. La ventana no tiene el marco del sistema:
// el diseño dibuja su propia barra de título, desde la que se arrastra.
//
// Usage: HarvestMoonMM [disc image]
//   A disc image given (or dropped on the program) is prepared right away, as if chosen.
//   --screen game|options|mods|controls opens that tab first (to look at it).
//   --still draws without animations (to take pictures of the tabs that can be compared).

#include "app.h"
#include "lettering.h"
#include "look.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#ifdef _WIN32
#include <dwmapi.h>
#endif

#include <algorithm>
#include <cmath>
#include <optional>
#include <string>

namespace {

constexpr const char* WINDOW_TITLE = "Harvest Moon: Magical Melody";

struct Arguments {
    std::string disc;
    std::optional<App::Tab> tab;
    bool still = false;
};

Arguments parse_arguments(int argc, char** argv) {
    Arguments arguments;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--screen" && i + 1 < argc) {
            const std::string name = argv[++i];
            const std::pair<const char*, App::Tab> tabs[] = {
                {"game", App::Tab::Game},       {"home", App::Tab::Game},         {"disc", App::Tab::Game},
                {"options", App::Tab::Options}, {"mods", App::Tab::Mods},         {"controls", App::Tab::Controls},
            };
            for (const auto& [tab_name, tab] : tabs) {
                if (name == tab_name) {
                    arguments.tab = tab;
                }
            }
        } else if (argument == "--still") {
            arguments.still = true;
        } else if (!argument.starts_with("-")) {
            arguments.disc = argument;
        }
    }
    return arguments;
}

// The design's size at the scaling of the window's screen, but never bigger than the screen.
void design_window_size(SDL_DisplayID display, int& width, int& height) {
    float scale = SDL_GetDisplayContentScale(display);
    if (scale <= 0.f) {
        scale = 1.f;
    }
    SDL_Rect usable;
    if (SDL_GetDisplayUsableBounds(display, &usable)) {
        scale = std::min({scale, usable.w * 0.95f / look::WIDTH, usable.h * 0.95f / look::HEIGHT});
    }
    width = static_cast<int>(std::lround(look::WIDTH * scale));
    height = static_cast<int>(std::lround(look::HEIGHT * scale));
}

// The title bar drags the window, as the system's would.
SDL_HitTestResult SDLCALL hit_test(SDL_Window* /*window*/, const SDL_Point* point, void* /*data*/) {
    return App::moves_window(ImVec2(static_cast<float>(point->x), static_cast<float>(point->y))) ? SDL_HITTEST_DRAGGABLE
                                                                                                  : SDL_HITTEST_NORMAL;
}

// On Windows 11 the window without a frame gets its rounded corners back, as the design has them.
void round_corners(SDL_Window* window) {
#ifdef _WIN32
    auto* handle = static_cast<HWND>(
        SDL_GetPointerProperty(SDL_GetWindowProperties(window), SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
    // DWMWA_WINDOW_CORNER_PREFERENCE and DWMWCP_ROUND, which older headers do not name.
    constexpr DWORD CORNER_PREFERENCE = 33;
    const int round = 2;
    if (handle != nullptr) {
        DwmSetWindowAttribute(handle, CORNER_PREFERENCE, &round, sizeof(round));
    }
#else
    (void)window;
#endif
}

}  // namespace

int main(int argc, char** argv) {
    const Arguments arguments = parse_arguments(argc, argv);
    SDL_SetAppMetadata(WINDOW_TITLE, HMMM_PACKAGE_VERSION, "io.github.rodoly.hmmmgc.launcher");
    if (!SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMEPAD)) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, WINDOW_TITLE, SDL_GetError(), nullptr);
        return 1;
    }
    int width = 0;
    int height = 0;
    design_window_size(SDL_GetPrimaryDisplay(), width, height);
    SDL_Window* window = SDL_CreateWindow(WINDOW_TITLE, width, height, SDL_WINDOW_BORDERLESS | SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window != nullptr ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, WINDOW_TITLE, SDL_GetError(), window);
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowHitTest(window, hit_test, nullptr);
    round_corners(window);
    SDL_SetRenderVSync(renderer, 1);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // nothing of ImGui is kept between runs
    io.LogFilename = nullptr;
    // ImGui's own navigation is not used (the launcher reads the keys itself), but the SDL backend
    // only passes the gamepad's buttons on when it is on.
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoAll);

    {
        App app(window, renderer, arguments.disc, arguments.tab, arguments.still);
        while (!app.wants_to_quit()) {
            SDL_Event event;
            if (app.game_running()) {
                // The window is hidden while the game runs: wait for it instead of drawing.
                if (SDL_WaitEventTimeout(&event, 250) && event.type == SDL_EVENT_QUIT) {
                    break;
                }
                app.update();
                continue;
            }
            bool quit = false;
            while (SDL_PollEvent(&event)) {
                ImGui_ImplSDL3_ProcessEvent(&event);
                quit |= event.type == SDL_EVENT_QUIT;
                quit |= event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED &&
                        event.window.windowID == SDL_GetWindowID(window);
                if (event.type == SDL_EVENT_WINDOW_DISPLAY_SCALE_CHANGED) {
                    // On a screen with another scaling, the window takes the design's size there.
                    design_window_size(SDL_GetDisplayForWindow(window), width, height);
                    SDL_SetWindowSize(window, width, height);
                }
                app.handle_event(event);
            }
            if (quit) {
                break;
            }
            app.update();
            if ((SDL_GetWindowFlags(window) & (SDL_WINDOW_MINIMIZED | SDL_WINDOW_HIDDEN)) != 0) {
                SDL_Delay(50);
                continue;
            }

            // The design fills the window, as big as it fits (in hundredths, as the fonts are made).
            int window_width = 0;
            int window_height = 0;
            SDL_GetWindowSize(window, &window_width, &window_height);
            const float scale = std::round(std::min(static_cast<float>(window_width) / look::WIDTH,
                                                    static_cast<float>(window_height) / look::HEIGHT) * 100.f) / 100.f;
            look::set_canvas(ImVec2(std::round((static_cast<float>(window_width) - look::WIDTH * scale) * 0.5f),
                                    std::round((static_cast<float>(window_height) - look::HEIGHT * scale) * 0.5f)),
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
    }

    ImGui_ImplSDLRenderer3_Shutdown();
    ImGui_ImplSDL3_Shutdown();
    ImGui::DestroyContext();
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}
