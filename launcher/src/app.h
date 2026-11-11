// The launcher: its four tabs (the game, the options, the mods and the controls) and what they do
// (choosing the disc, preparing the game and starting it, keeping the options and the mods). What
// it does is in app.cpp; how it looks, after its design, in view.cpp.
//
// El launcher: sus cuatro pestañas (el juego, las opciones, los mods y los controles) y lo que
// hacen (elegir el disco, preparar el juego y arrancarlo, guardar las opciones y los mods). Lo que
// hace está en app.cpp; cómo se ve, según su diseño, en view.cpp.
#pragma once

#include "child_process.h"
#include "controls.h"
#include "mods.h"
#include "paths.h"
#include "preparation.h"
#include "settings.h"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include <imgui.h>

#include <array>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

class App {
public:
    enum class Tab { Game, Options, Mods, Controls };

    // `disc`: an image given when the launcher was started (dropped on it), prepared at once.
    // `still`: no animations (to take pictures of the tabs that can be compared).
    App(SDL_Window* window, const std::string& disc, std::optional<Tab> first_tab, bool still);

    // Draws one frame (between ImGui's NewFrame and Render); `seconds` is the time since the start.
    void frame(double seconds);
    void handle_event(const SDL_Event& event);
    // Watches the game while it runs; call it every frame, even while the window is hidden.
    void update();
    bool wants_to_quit() const { return quit_; }
    bool game_running() const { return game_ != nullptr; }
    // Whether the window is dragged from this point of it (the title bar, but not its buttons).
    static bool moves_window(ImVec2 window_point);

    // How a value is shown at the right of a row (the colors of its chip).
    enum class Tone { None, On, Off, Value, Warning, Key };

private:

    // A row of the game's, the options' or the controls' tab.
    struct Row {
        const char* symbol = "\xE2\x99\xAA";  // ♪, in the square at its left
        std::string label;
        std::string value;
        Tone tone = Tone::None;
        bool play = false;     // the big orange row
        bool arrows = false;   // left and right change its value
        float progress = -1.f; // how far the preparation is, on the play row
        std::string hint;      // what the message at the bottom says when it is chosen
        std::function<void()> activate;
        std::function<void(int)> change;  // -1 or +1
    };

    // -- What the launcher does (app.cpp) --
    void apply_language();
    void save_settings();
    void read_prepared();
    bool prepared_is_current() const;
    std::string remembered_disc() const;
    std::vector<std::string> package_mods() const;
    std::string error_message(const std::string& reason) const;
    void choose_disc();
    void start_preparation(const std::string& disc);
    void after_preparation();
    void play();
    void open_log();
    void choose_tab(Tab tab);
    void move_selection(int step);

    std::vector<Row> game_rows();
    std::vector<Row> options_rows();
    std::vector<Row> controls_rows();
    std::string preparation_text() const;
    void watch_controller();
    void follow_disc();

    // -- How it looks (view.cpp) --
    void handle_keys(const std::vector<Row>& rows);
    void draw(double seconds, const std::vector<Row>& rows);
    void draw_title_bar(ImDrawList* draw);
    void draw_background(ImDrawList* draw, double seconds);
    void draw_bubbles(ImDrawList* draw, double seconds);
    void draw_header(ImDrawList* draw, double seconds);
    void draw_tabs(ImDrawList* draw, double seconds);
    void draw_panel(ImDrawList* draw, double seconds, const std::vector<Row>& rows);
    void draw_rows(ImDrawList* draw, double seconds, const std::vector<Row>& rows, float width);
    void draw_mods(ImDrawList* draw, double seconds);
    void draw_mod_details(ImDrawList* draw, const mods::Mod& mod);
    void draw_art(ImDrawList* draw);
    void draw_scroll_bar(ImDrawList* draw, int count);
    void draw_message(ImDrawList* draw, double seconds, const std::string& message);
    void draw_cursor(ImDrawList* draw, double seconds, float row_x, float row_center);
    // Whether the mouse is over `rect` of the design: then a hand shows it can be clicked.
    bool hovered(float x, float y, float w, float h);
    bool clicked(float x, float y, float w, float h);
    // What a click does, done after the frame is drawn.
    void later(std::function<void()> action) { pending_ = std::move(action); }

    SDL_Window* window_;
    paths::Package package_;
    paths::Player player_;
    settings::Settings settings_;
    std::vector<std::string> missing_;
    bool quit_ = false;
    bool still_ = false;

    struct Prepared {
        bool exists = false;
        std::string game;
        std::string disc;
        std::string source_disc;
        std::string program;
        std::string package_version;
    };
    Prepared prepared_;
    Preparation preparation_;
    Preparation::State seen_state_ = Preparation::State::Idle;
    std::unique_ptr<ChildProcess> game_;

    // The file dialog answers from any thread: what it chose waits here for the next frame.
    static void SDLCALL disc_chosen(void* user_data, const char* const* files, int filter);
    std::mutex dialog_mutex_;
    std::optional<std::string> chosen_disc_;  // empty text: the player closed the dialog
    bool dialog_open_ = false;
    std::string filter_name_;
    SDL_DialogFileFilter filter_{};

    std::vector<mods::Mod> mods_;
    controls::Gamepads gamepads_;
    std::array<bool, controls::PART_COUNT> pressed_before_{};

    // What the player sees: the tab, the chosen row, a message that answers what was just done
    // (instead of the chosen row's hint), and how far the list is scrolled.
    Tab tab_ = Tab::Game;
    int selected_ = 0;
    std::string toast_;
    float scroll_ = 0.f;
    float description_scroll_ = 0.f;
    double started_ = -1.0;
    ImVec2 last_mouse_{-1.f, -1.f};
    bool mouse_moved_ = false;
    std::function<void()> pending_;

    // The disc the launcher shows (the prepared one, or else the one chosen last) and its code,
    // read from it once.
    std::string shown_disc_;
    std::string disc_code_;
};
