// The screen where the player chooses which key or button plays each part of the GameCube
// controller. The keyboard and the mouse are one device (the keys are aurora's keyboard
// bindings), and each gamepad is another, with its own mapping that aurora keeps by the
// gamepad's name (PADSerializeMappings). It lives in the options menu of the game.
//
// La pantalla donde se elige qué tecla o botón juega cada parte del control de GameCube. El
// teclado y el ratón son un dispositivo y cada gamepad es otro, con su propia asignación.
#pragma once

#include <SDL3/SDL_events.h>

namespace controls_editor {

// The content of the page, in the panel of the options menu.
void draw(bool focus);
// Takes the key or the mouse button the player presses while a part waits for one; true when
// the event was used.
bool handle_event(const SDL_Event& event);
// Whether a part is waiting for a key or a button (or just stopped waiting): the menu does not
// go back on Escape or B then.
bool busy();
// Stops waiting (when the page is left).
void stop();

}  // namespace controls_editor
