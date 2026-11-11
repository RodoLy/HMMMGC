// Where the launcher finds the package's parts, and where it keeps the player's things.
//
// Dónde encuentra el launcher las partes del paquete, y dónde guarda las cosas del jugador.
//
// The package (docs/NATIVE_PACKAGE.md) has the launcher at its root and, in builder/, what
// prepares the game, laid out like this repository so that its tools find each other:
//
//     builder/tools/prepare_game.py, builder/kit/, builder/config/G4AEE9/symbols.txt,
//     builder/mods/<id>/mod.json, builder/runtime/src/ (read by the translator),
//     builder/python/, builder/translator/, builder/llvm-mingw/, builder/nodtool/
//
// The player's things go where the runtime keeps them (runtime/src/player_folder.cpp):
// %LOCALAPPDATA%\HarvestMoonMM on Windows, ~/.local/share/HarvestMoonMM on Linux.
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace paths {

struct Package {
    std::filesystem::path root;
    std::filesystem::path builder;
    std::filesystem::path python;          // empty when there is none
    std::filesystem::path prepare_script;
    std::filesystem::path kit;
    std::filesystem::path translator;      // Translator.Cli(.exe), or its .dll in a developer's copy
    std::filesystem::path toolchain;       // empty when the PATH's compiler is used
    std::filesystem::path nodtool;         // empty when there is none
    std::filesystem::path symbols;
    std::filesystem::path mods;

    // The parts that are missing, by name ("python", "kit", ...); empty when it is complete.
    std::vector<std::string> missing() const;
};

struct Player {
    std::filesystem::path root;         // HarvestMoonMM: settings.json
    std::filesystem::path native;       // HarvestMoonMM/Native: the runtime's folder (saves)
    std::filesystem::path game;         // HarvestMoonMM/Native/Game: the prepared program
    std::filesystem::path settings;
    std::filesystem::path prepare_log;
    std::filesystem::path game_log;
};

Package find_package();
Player find_player();

// Paths as UTF-8 text and back, the same on every system.
std::string utf8(const std::filesystem::path& path);
std::filesystem::path from_utf8(const std::string& text);

// Opens a folder or a file with the system's own program (the file manager, a text editor).
void open_in_system(const std::filesystem::path& path);

}  // namespace paths
