// The player's folder: where the runtime keeps what belongs to the player (the memory
// card, and aurora's settings and caches).
//
// La carpeta del jugador: donde el runtime guarda lo que es del jugador (la memory card,
// y los ajustes y cachés de aurora).
//
// By default it is HarvestMoonMM/Native in the platform's folder for application data:
// %LOCALAPPDATA% on Windows (inside the folder of the version on ModernGekko, so that both
// stay together), ~/Library/Application Support on macOS, and $XDG_DATA_HOME
// (~/.local/share) elsewhere; --user-folder chooses another one. While the native runtime
// is in progress it keeps a memory card of its own, apart from the saves of the version on
// ModernGekko (HarvestMoonMM/GC/USA/Card A), so that it cannot damage them; a .gci file
// can be copied from one card to the other.
#pragma once

#include <filesystem>
#include <string>

namespace player_folder {

// Chooses the folder (`requested`, or the default when it is empty) and creates it.
// Throws std::filesystem::filesystem_error when it cannot be created.
void choose(const std::string& requested);

// The chosen folder. choose() must have been called first.
const std::filesystem::path& path();

// The settings.json that the launcher writes (shared/settings.h), which the runtime reads and
// writes too (player_options.h): in the HarvestMoonMM folder, above the default player folder,
// or inside the folder given to choose().
const std::filesystem::path& settings_file();

}  // namespace player_folder
