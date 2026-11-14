// The far_camera mod's side in the runtime (mods/far_camera/mod.c): the mod follows the game's own
// button for pulling the camera back and says how far past the game's limit the player has gone;
// this turns that into the zoom of the picture (camera_view.h), which is what shows more.
//
// El lado del mod far_camera en el runtime: el mod sigue el botón del propio juego para alejar la
// cámara y dice cuánto pasó el jugador del tope del juego; esto lo convierte en el zoom de la
// imagen (camera_view.h), que es lo que muestra más.
#pragma once

namespace far_camera {

inline constexpr const char* MOD_ID = "far_camera";

// The zoom for the mod's amount now: 1 at the game's limit (and when the mod is not compiled in or
// is off, since its hooks do not run then), down to camera_view::MINIMUM_ZOOM at the farthest.
float zoom();

}  // namespace far_camera
