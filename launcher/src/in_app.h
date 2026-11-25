// The launcher inside the Android app: before the game starts, the app shows the same launcher as
// on a computer (app.h), drawn the same way (SDL3's 2D renderer and Dear ImGui), so that the player
// chooses the options, the mods, the cheats and the controls on the phone. Its Play closes it, and
// the game starts in the same program; its Exit closes the app.
//
// It draws with its own window and renderer (OpenGL ES on a phone), apart from the game's graphics
// (aurora, Vulkan), and both are gone before the game opens its window. So it shows even on a phone
// where the game does not draw, and its "Send the log" sends the log of the last time the game ran.
//
// El launcher dentro de la app de Android: antes de que arranque el juego, la app muestra el mismo
// launcher que en una computadora, para elegir las opciones, los mods, los trucos y los controles en
// el celular. Su Jugar lo cierra y el juego arranca en el mismo programa; su Salir cierra la app.
// Dibuja con su propia ventana, aparte de los gráficos del juego, así que se ve aunque el juego no
// dibuje, y su "Enviar el registro" manda el registro de la última vez que corrió el juego.
//
// On a computer, `HarvestMoonMM --in-app <folder>` opens it in a window as the phone would, with
// <folder> as the app's folder: to look at it and take pictures of it.
#pragma once

#include "app.h"

#include <string>
#include <utility>
#include <vector>

namespace in_app {

struct Result {
    bool play = false;  // Play was chosen; otherwise Exit (or the app was closed)
    // The variables that give the mods their settings (the game reads them when it starts).
    std::vector<std::pair<std::string, std::string>> environment;
};

// Shows the launcher until Play or Exit. `still`: no animations (for pictures).
Result run(InApp in_app, bool still = false);

// The text of a mod's manifest (mods/<id>/mod.json), which in_app.cmake writes into the game's
// library for the Android app; empty for a mod it does not know.
std::string mod_manifest(const std::string& id);

}  // namespace in_app
