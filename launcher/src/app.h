// The launcher: its four tabs (the game, the options, the mods and the controls) and what they do
// (choosing the disc, preparing the game and starting it, keeping the options and the mods). What
// it does is in app.cpp; how it looks, after its design, in view.cpp.
//
// El launcher: sus cuatro pestañas (el juego, las opciones, los mods y los controles) y lo que
// hacen (elegir el disco, preparar el juego y arrancarlo, guardar las opciones y los mods). Lo que
// hace está en app.cpp; cómo se ve, según su diseño, en view.cpp.
//
// The same launcher also opens inside the Android app, before the game (in_app.h). There the disc
// is already the app's and the game is in the same program, so its Game tab only plays, chooses the
// language and sends the log; the other tabs are the same.
#pragma once

#include "child_process.h"
#include "controls.h"
#include "controls_profile.h"
#include "disc_search.h"
#include "game_start.h"
#include "mods.h"
#include "paths.h"
#include "preparation.h"
#include "settings.h"
#include "updater.h"

#include <SDL3/SDL_dialog.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_video.h>

#include <imgui.h>

#include <array>
#include <cstdint>
#include <functional>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <vector>

// What a disc is called: the console and the region for the disc this port plays, and the disc's
// own code for any other.
std::string disc_name(const std::string& code);

// What the launcher is given inside the Android app (in_app.h).
struct InApp {
    paths::Player player;                  // the app's folder: settings.json, the controls, the log
    std::vector<std::string> mods;         // the mods compiled into the game
    std::function<std::string(const std::string& id)> manifest;  // each one's mod.json
    std::function<void()> send_log;        // shares the log of the last time the game ran
};

class App {
public:
    enum class Tab { Game, Options, Mods, Cheats, Controls };
    static constexpr int TAB_COUNT = 5;

    // `disc`: an image given when the launcher was started (dropped on it), prepared at once.
    // `still`: no animations (to take pictures of the tabs that can be compared).
    App(SDL_Window* window, const std::string& disc, std::optional<Tab> first_tab, bool still);
    // Inside the Android app.
    App(SDL_Window* window, InApp in_app, bool still);

    // Draws one frame (between ImGui's NewFrame and Render); `seconds` is the time since the start.
    void frame(double seconds);
    void handle_event(const SDL_Event& event);
    // Watches the game while it runs; call it every frame, even while the window is hidden.
    void update();
    bool wants_to_quit() const { return quit_; }
    // Inside the Android app: Play was chosen (the launcher closes and the game starts).
    bool wants_to_play() const { return play_; }
    // The variables that give the mods their settings, for the game started in the same program.
    std::vector<std::pair<std::string, std::string>> game_environment() const;
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
    void start_android_app(const std::string& disc);
    void after_android_app();
    void play();
    void open_log();
    void choose_tab(Tab tab);
    void move_selection(int step);

    std::vector<Row> game_rows();
    std::vector<Row> game_rows_in_app();
    void language_row(std::vector<Row>& rows);
    void load_mods();
    std::vector<Row> options_rows();
    std::vector<Row> cheats_rows();
    std::vector<Row> controls_rows();
    std::string preparation_text() const;
    static std::string stage_text(const Preparation& preparation);
    bool preparing_anything() const {
        return preparation_.state() == Preparation::State::Running ||
               android_app_.state() == Preparation::State::Running;
    }
    // The controls tab: a part waits for the key or the button the player presses.
    void start_waiting(int slot);
    void stop_waiting();
    void give_key(int key);
    void give_button(std::uint32_t native);
    void save_controls();
    bool choosing_controls() const;
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
    void draw_scroll_bar(ImDrawList* draw);
    void draw_message(ImDrawList* draw, double seconds, const std::string& message);
    void draw_cursor(ImDrawList* draw, double seconds, float row_x, float row_center);
    // Whether the mouse is over `rect` of the design: then a hand shows it can be clicked.
    bool hovered(float x, float y, float w, float h);
    bool clicked(float x, float y, float w, float h);
    // Pressed down there (where a drag starts), with the mouse or a finger.
    bool pressed_on(float x, float y, float w, float h);
    // What a click does, done after the frame is drawn.
    void later(std::function<void()> action) { pending_ = std::move(action); }

    SDL_Window* window_;
    paths::Package package_;
    paths::Player player_;
    settings::Settings settings_;
    std::vector<std::string> missing_;
    std::optional<InApp> in_app_;  // inside the Android app
    bool quit_ = false;
    bool play_ = false;
    bool still_ = false;

    PreparedGame prepared_;
    DiscSearch disc_search_;
    Preparation preparation_;
    Preparation::State seen_state_ = Preparation::State::Idle;
    Preparation android_app_;  // the Android app being made (tools/make_android_app.py)
    Preparation::State seen_android_state_ = Preparation::State::Idle;
    std::unique_ptr<ChildProcess> game_;
    // The newest published version, asked for when the launcher opens (updater.h).
    Updater updater_;
    std::filesystem::path update_folder() const { return player_.root / "Update"; }
    void update_row(std::vector<Row>& rows);
    // The window with the new version's notes, opened by the update row: "Update" downloads it,
    // "Not now" closes it. While it is open, the keys and the mouse are its own.
    bool notes_open_ = false;
    int notes_choice_ = 0;  // 0 "Update", 1 "Not now"
    float notes_scroll_ = 0.f;
    bool drawing_notes_ = false;
    void open_notes();
    void close_notes(bool update);
    void handle_notes_keys();
    void draw_notes(ImDrawList* draw);

    // The file dialog answers from any thread: what it chose waits here for the next frame.
    static void SDLCALL disc_chosen(void* user_data, const char* const* files, int filter);
    std::mutex dialog_mutex_;
    std::optional<std::string> chosen_disc_;  // empty text: the player closed the dialog
    bool dialog_open_ = false;
    std::string filter_name_;
    SDL_DialogFileFilter filter_{};

    std::vector<mods::Mod> mods_;
    controls::Gamepads gamepads_;
    controls_profile::Profile controls_;
    int selected_device_ = 0;  // 0 is the keyboard, then the gamepads
    int waiting_slot_ = -1;    // the part that waits for a key or a button, or none
    std::uint64_t waiting_since_ = 0;
    std::uint64_t settled_until_ = 0;  // the key that was just given is still down: it does nothing yet

    // What the player sees: the tab, the chosen row, a message that answers what was just done
    // (instead of the chosen row's hint), and how far the list is scrolled.
    Tab tab_ = Tab::Game;
    int selected_ = 0;
    std::string toast_;
    float scroll_ = 0.f;
    // The list scrolls to keep the chosen row in view after the keys moved it; the wheel and the
    // scroll bar move the list by themselves and leave the chosen row where it is.
    bool follow_selection_ = true;
    float list_total_ = 0.f;   // the height of all the rows of the list on screen
    bool dragging_bar_ = false;
    // On a phone: the finger that is down has scrolled (it chooses nothing when it lifts), and
    // whether it started on a mod's description.
    bool touch_scrolled_ = false;
    bool touch_on_description_ = false;
    bool using_pad_ = false;  // the last key came from a controller, so the tab labels say L and R
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
