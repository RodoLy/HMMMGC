// On Android, the game's Options button brings back the launcher (launcher/src/in_app.h), the same
// one the app shows before the game, over the game: the game stops where it is (its time too), and
// "Back to the game" goes on from there. The options the launcher changes apply at once (the
// picture, the sound, the controls); the mods and the cheats, at the next start, as on a computer.
// It is drawn with the game's own window and Dear ImGui (aurora), between two of its frames.
//
// En Android, el botón Options del juego trae de vuelta el launcher, sobre el juego: el juego se
// detiene donde está (también su tiempo), y "Volver al juego" sigue desde ahí. Las opciones que
// cambia el launcher valen al momento; los mods y los trucos, al volver a abrir la app.
//
// On a computer there is no such thing: F10 opens the game's own options menu (in_game_menu.h). The
// runtime built with HMMM_LAUNCHER_IN_THE_GAME (runtime/CMakeLists.txt) has it there too, to try it.
#pragma once

namespace launcher_overlay {

// Asks for the launcher over the game (the Options button, on Android): it opens after this frame.
void request();
bool requested();

// Shows the launcher over the stopped game until the player goes back to it. False when the
// player chose Exit: the game then ends. Called between two of the game's frames.
bool show();

}  // namespace launcher_overlay
