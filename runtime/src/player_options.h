// The player's options in the runtime: how fast the game runs, how the picture is shown and
// which mods are on.
//
// Las opciones del jugador en el runtime: qué tan rápido corre el juego, cómo se muestra la
// imagen y qué mods están activos.
//
// Each option is the one the command line gives (main.cpp), or else the one in settings.json,
// or else its default; so the game started by hand plays as the launcher left it. settings.json
// is the launcher's file (shared/settings.h, with its names and limits), where
// player_folder::settings_file() says or where --settings says.
#pragma once

#include "settings.h"

#include <filesystem>
#include <optional>
#include <string>
#include <vector>

namespace player_options {

// What the command line gave; what it leaves out is read from settings.json.
struct Given {
    std::optional<unsigned> game_speed_percent;
    std::optional<float> resolution_scale;
    std::optional<bool> fullscreen;
    std::optional<bool> widescreen;
    std::optional<float> zoom;
    std::optional<unsigned> frame_rate;
    std::optional<std::vector<std::string>> mods;
};

// Reads `file` (when it does not exist, every option not given is its default) and settles every
// option. Called once, before the game starts.
void load(const std::filesystem::path& file, const Given& given);

unsigned game_speed_percent();  // 100 to 400
float resolution_scale();       // 0: the window's; otherwise a multiple of the console's
bool fullscreen();
bool widescreen();
float zoom();                   // 0.5 to 2
unsigned frame_rate();          // 30, 60 or 120
// Whether the mod `id` is on: in the command line's list, or not turned off in settings.json.
bool mod_enabled(const std::string& id);

}  // namespace player_options
