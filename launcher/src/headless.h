// What the launcher does without its window, for another launcher that sets the game up and starts
// it (RecompLauncher reads how from recomplauncher.json, packaging/native/recomplauncher.json).
//
// Lo que hace el launcher sin su ventana, para otro launcher que prepara el juego y lo arranca.
//
//   HarvestMoonMM --prepare <disc image>
//       Prepares the game from the disc as the window's Prepare does, and remembers the disc. It
//       prints a line "progress <0 to 1> <step>" as it goes, and exits with 0 when the game is
//       ready, or with PREPARE_FAILED after a line "error <reason> <text>".
//   HarvestMoonMM --play
//       Starts the prepared game as the window's Play does, with the player's options and mods,
//       waits for it and exits with its exit code; with NOT_PREPARED when the game has not been
//       prepared by this package, or NOT_STARTED when it could not be started.
//
// Without these arguments the launcher opens its window, as it always has.
#pragma once

#include <string>

namespace headless {

inline constexpr int PREPARE_FAILED = 1;
inline constexpr int NOT_PREPARED = 3;
inline constexpr int NOT_STARTED = 4;

int prepare(const std::string& disc);
int play();

}  // namespace headless
