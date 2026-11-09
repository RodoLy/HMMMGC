// The launcher's screens and what they do: choosing the disc, preparing the game, the main menu,
// the options, the mods and the controls, and starting the game.
//
// Las pantallas del launcher y lo que hacen: elegir el disco, preparar el juego, el menú
// principal, las opciones, los mods y los controles, y arrancar el juego.
#pragma once

#include "child_process.h"
#include "controls.h"
#include "mods.h"
#include "paths.h"
#include "preparation.h"
#include "settings.h"
#include "theme.h"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

class App {
public:
    enum class Screen { Home, Disc, Preparing, Options, Mods, Controls };

    // `disc`: an image given when the launcher was started (dropped on it, or for trying it).
    App(SDL_Window* window, const std::string& disc, std::optional<Screen> first_screen);

    // Draws one frame of the current screen (between ImGui's NewFrame and Render).
    void frame(double seconds);
    void handle_event(const SDL_Event& event);
    // Watches the game while it runs; call it every frame, even while the window is hidden.
    void update();
    bool wants_to_quit() const { return quit_; }
    bool game_running() const { return game_ != nullptr; }

private:
    struct Prepared {
        bool exists = false;
        std::string disc;
        std::string source_disc;
        std::string program;
        std::string package_version;
    };

    void home(ImVec2 size, double seconds);
    void disc_screen(ImVec2 size);
    void preparing(ImVec2 size, double seconds);
    void options(ImVec2 size);
    void mods_screen(ImVec2 size);
    void controls_screen(ImVec2 size);
    // The panel of a screen other than the main one, under the small sign, with a "Back"
    // button under it when `can_go_back`. Returns the panel's content area through ImGui.
    void begin_screen(ImVec2 size, const char* title, float height_share = 1.f);
    bool end_screen(ImVec2 size, bool can_go_back);

    void go(Screen screen);
    void choose_disc();
    void start_preparation(const std::string& disc);
    void play();
    void read_prepared();
    bool prepared_is_current() const;
    std::string remembered_disc() const;
    void save_settings();
    void apply_language();
    std::vector<std::string> package_mods() const;
    std::string error_message(const std::string& reason) const;

    SDL_Window* window_;
    paths::Package package_;
    paths::Player player_;
    settings::Settings settings_;
    std::vector<std::string> missing_;
    Prepared prepared_;
    Screen screen_ = Screen::Home;
    bool focus_first_ = true;  // a screen that just appeared puts the focus on its first item
    bool quit_ = false;

    Preparation preparation_;
    bool show_details_ = false;
    std::unique_ptr<ChildProcess> game_;
    std::string message_;  // a problem to show on the main screen (the game failed, ...)
    bool message_has_log_ = false;

    // The file dialog answers from any thread: what it chose waits here for the next frame.
    static void SDLCALL disc_chosen(void* user_data, const char* const* files, int filter);
    std::mutex dialog_mutex_;
    std::optional<std::string> chosen_disc_;  // empty text: the player closed the dialog
    bool dialog_open_ = false;
    std::string filter_name_;
    SDL_DialogFileFilter filter_{};

    std::vector<mods::Mod> mods_;
    size_t selected_mod_ = 0;
    controls::Gamepads gamepads_;
    int selected_device_ = 0;
};
