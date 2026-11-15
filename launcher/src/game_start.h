// The prepared game and how it is started: what the window's Play does (app.cpp), and what the
// launcher does without its window for another launcher (headless.h).
//
// El juego preparado y cómo se arranca: lo que hace Jugar en la ventana, y lo que hace el launcher
// sin su ventana para otro launcher.
#pragma once

#include "child_process.h"
#include "mods.h"
#include "paths.h"
#include "settings.h"

#include <string>
#include <vector>

// What the preparation left (prepared.json in the player's game folder, tools/prepare_game.py).
struct PreparedGame {
    bool exists = false;
    std::string game;             // the disc's code
    std::string disc;             // the disc image the game reads, as UTF-8
    std::string source_disc;      // the image the player chose, when it was converted
    std::string program;          // the game's program, in the game folder
    std::string package_version;  // the package that prepared it

    static PreparedGame read(const paths::Player& player);
    // Whether it can be played: prepared by this package, and its program and disc still there.
    bool is_current(const paths::Player& player) const;
};

// The mods the package's runtime has (the kit's recipe), which are the game's.
std::vector<std::string> kit_mods(const paths::Package& package);

// The game's program and what it is given: the disc, the options and the mods the player chose.
ChildProcess::Options game_start_options(const paths::Package& package, const paths::Player& player,
                                         const settings::Settings& settings, const std::vector<mods::Mod>& mods,
                                         const PreparedGame& prepared);
