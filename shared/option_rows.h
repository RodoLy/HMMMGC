// The rows of the player's options, as the launcher's Options screen and the options menu inside
// the game show them: on the left the option's name with a hint under it, on the right the
// control that changes it in settings.json (shared/settings.h).
//
// Las filas de las opciones del jugador, como las muestran la pantalla de Opciones del launcher y
// el menú de opciones dentro del juego: a la izquierda el nombre de la opción con una pista
// abajo, a la derecha el control que la cambia en settings.json.
#pragma once

#include "settings.h"

namespace option_rows {

// A table of two columns for the rows below; true when it is shown, and then end() closes it.
bool begin(const char* id);
void end();

// Starts a row of a table with the option's name and hint, and leaves the cursor in the right
// cell for its control (for an option of only one program, such as the launcher's language).
void next_row(const char* label, const char* hint = nullptr);
// How wide a control on the right is.
float control_width();

// One row each. True when the player changed the value, which is then in `settings`; `focus`
// puts the keyboard's and the controller's focus on its control (the first row of a screen).
bool resolution(settings::Settings& settings, bool focus = false);
bool fullscreen(settings::Settings& settings, bool focus = false);
bool widescreen(settings::Settings& settings, bool focus = false);
bool frame_rate(settings::Settings& settings, bool focus = false);
bool show_frame_rate(settings::Settings& settings, bool focus = false);
bool zoom(settings::Settings& settings, bool focus = false);
bool volume(settings::Settings& settings, bool focus = false);

}  // namespace option_rows
