// The player's options in the runtime: how fast the game runs, how the picture is shown, how
// loud the sound is and which mods are on.
//
// Las opciones del jugador en el runtime: qué tan rápido corre el juego, cómo se muestra la
// imagen, qué tan fuerte suena y qué mods están activos.
//
// Each option is the one the command line gives (main.cpp), or else the one in settings.json,
// or else its default; so the game started by hand plays as the launcher left it. settings.json
// is the launcher's file (shared/settings.h, with its names and limits), where
// player_folder::settings_file() says or where --settings says. The options menu inside the game
// (in_game_menu.h) changes them while it runs, and they are written back to the file, keeping
// whatever else it holds (the disc, the language, the mods).
#pragma once

#include "settings.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace player_options {

// What the command line gave; what it leaves out is read from settings.json.
struct Given {
    std::optional<float> resolution_scale;
    std::optional<bool> fullscreen;
    std::optional<bool> widescreen;
    std::optional<float> zoom;
    std::optional<unsigned> frame_rate;
    std::optional<unsigned> volume_percent;
    std::optional<std::vector<std::string>> mods;
};

// Reads `file` (when it does not exist, every option not given is its default) and settles every
// option. Called once, before the game starts.
void load(const std::filesystem::path& file, const Given& given);

float resolution_scale();       // 0: the window's; otherwise a multiple of the console's
bool fullscreen();
bool widescreen();
float zoom();                   // 0.5 (pulled back the most) to 1 (the game's camera)
unsigned frame_rate();          // 30, 60 or 120
unsigned volume_percent();      // 0 (silent) to 100 (the game's own)
bool show_frame_rate();         // over the game, at the top right (in_game_menu.h)
// Whether the mod `id` is on: in the command line's list, or not turned off in settings.json.
bool mod_enabled(const std::string& id);
// The mods the game started with (main.cpp tells it): the menu compares them with the settings.
void note_running_mods(const std::vector<std::string>& ids);
bool mod_is_running(const std::string& id);
// Turns a mod on or off in the settings (the menu saves them); the game keeps the mods it started with.
void set_mod_enabled(const std::string& id, bool enabled);
// The language of the game's dialogues for the next start ("en", "es"...); the menu saves it.
void set_game_language(const std::string& language);
// Whether the settings ask for something the game only takes when it starts (the mods, the
// language of the dialogues) and that is not what this run started with.
bool restart_pending();

// While the game runs (in_game_menu.h). current() is settings.json as the game runs with it (with
// what the command line gave), for the menu's rows, which change it; apply_changes() then gives
// each option that changed to the part of the runtime that uses it, at once.
settings::Settings& current();
void apply_changes();
// Full screen as it is now: Alt+Enter changes it without the menu.
void note_fullscreen(bool fullscreen);
// Writes current() to settings.json when the menu changed something since the last time.
void save_if_changed();

}  // namespace player_options
