// The launcher of the native version of Harvest Moon: Magical Melody (app.h): a window drawn with
// SDL3's 2D renderer and Dear ImGui, in the game's style (theme.h).
//
// El launcher de la versión nativa de Harvest Moon: Magical Melody: una ventana dibujada con el
// renderizador 2D de SDL3 y Dear ImGui, con el estilo del juego.
//
// Usage: HarvestMoonMM [disc image]
//   A disc image given (or dropped on the program) is prepared right away, as if chosen.
//   --screen home|disc|options|mods|controls opens that screen first (to look at it).

#include "app.h"
#include "theme.h"

#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>
#include <imgui.h>
#include <imgui_impl_sdl3.h>
#include <imgui_impl_sdlrenderer3.h>

#include <algorithm>
#include <optional>
#include <string>

namespace {

constexpr float DESIGN_WIDTH = 1280.f;
constexpr float DESIGN_HEIGHT = 720.f;
constexpr const char* WINDOW_TITLE = "Harvest Moon: Magical Melody";

struct Arguments {
    std::string disc;
    std::optional<App::Screen> screen;
};

Arguments parse_arguments(int argc, char** argv) {
    Arguments arguments;
    for (int i = 1; i < argc; ++i) {
        const std::string argument = argv[i];
        if (argument == "--screen" && i + 1 < argc) {
            const std::string name = argv[++i];
            const std::pair<const char*, App::Screen> screens[] = {
                {"home", App::Screen::Home},       {"disc", App::Screen::Disc}, {"options", App::Screen::Options},
                {"mods", App::Screen::Mods},       {"controls", App::Screen::Controls},
            };
            for (const auto& [screen_name, screen] : screens) {
                if (name == screen_name) {
                    arguments.screen = screen;
                }
            }
        } else if (!argument.starts_with("-")) {
            arguments.disc = argument;
        }
    }
    return arguments;
}

// A window as big as the design at the screen's scaling, but never bigger than the screen.
void initial_window_size(int& width, int& height) {
    const SDL_DisplayID display = SDL_GetPrimaryDisplay();
    float scale = SDL_GetDisplayContentScale(display);
    if (scale <= 0.f) {
        scale = 1.f;
    }
    width = static_cast<int>(DESIGN_WIDTH * scale);
    height = static_cast<int>(DESIGN_HEIGHT * scale);
    SDL_Rect usable;
    if (SDL_GetDisplayUsableBounds(display, &usable)) {
        const float fit = std::min({1.f, usable.w * 0.92f / static_cast<float>(width),
                                    usable.h * 0.9f / static_cast<float>(height)});
        width = static_cast<int>(static_cast<float>(width) * fit);
        height = static_cast<int>(static_cast<float>(height) * fit);
    }
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
    initial_window_size(width, height);
    SDL_Window* window = SDL_CreateWindow(WINDOW_TITLE, width, height, SDL_WINDOW_RESIZABLE | SDL_WINDOW_HIDDEN);
    SDL_Renderer* renderer = window != nullptr ? SDL_CreateRenderer(window, nullptr) : nullptr;
    if (renderer == nullptr) {
        SDL_ShowSimpleMessageBox(SDL_MESSAGEBOX_ERROR, WINDOW_TITLE, SDL_GetError(), window);
        SDL_Quit();
        return 1;
    }
    SDL_SetWindowMinimumSize(window, 800, 450);
    SDL_SetRenderVSync(renderer, 1);
    SDL_SetWindowPosition(window, SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED);
    SDL_ShowWindow(window);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;  // nothing of ImGui is kept between runs
    io.LogFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    ImGui_ImplSDL3_InitForSDLRenderer(window, renderer);
    ImGui_ImplSDLRenderer3_Init(renderer);
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_AutoAll);

    {
        App app(window, arguments.disc, arguments.screen);
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
                quit |= event.type == SDL_EVENT_WINDOW_CLOSE_REQUESTED && event.window.windowID == SDL_GetWindowID(window);
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

            // The design is for 1280x720: everything grows and shrinks with the window.
            int window_width = 0;
            int window_height = 0;
            SDL_GetWindowSize(window, &window_width, &window_height);
            const float scale = std::min(static_cast<float>(window_width) / DESIGN_WIDTH,
                                         static_cast<float>(window_height) / DESIGN_HEIGHT);
            if (theme::set_scale(scale)) {
                ImGui_ImplSDLRenderer3_DestroyFontsTexture();
                ImGui_ImplSDLRenderer3_CreateFontsTexture();
            }

            ImGui_ImplSDLRenderer3_NewFrame();
            ImGui_ImplSDL3_NewFrame();
            ImGui::NewFrame();
            app.frame(static_cast<double>(SDL_GetTicks()) / 1000.0);
            ImGui::Render();
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
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
