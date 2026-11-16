// The touch buttons: a GameCube controller drawn over the game for a phone or a tablet, which
// plays as controller 1 together with whatever else plays there (a controller, the keyboard). They
// are drawn as the game draws the controller's buttons on its own screen (coins and beans with a
// dark blue edge and the letter pressed in), in the GameCube controller's colors, and see-through,
// so that the game shows under them also where a wide screen puts it there.
//
// Los botones táctiles: un control de GameCube dibujado sobre el juego para un celular o una
// tablet, que juega como el control 1 junto con lo demás. Se dibujan como el juego dibuja los
// botones del control en su pantalla, con los colores del control de GameCube, y semitransparentes.
//
// On the left, the Control Stick (the finger moves its knob, as far as its ring) and the D-Pad; on
// the right, A (the big green one), B, X and Y where the GameCube has them; L and R at the top
// corners with Z under R; START at the bottom in the middle, and at the top in the middle a button
// that opens the options menu (in_game_menu.h), as F10 does; under L, TURBO runs the game faster
// while it is held, as the turbo's key (Tab) does (the option "No TURBO touch button",
// settings.json's hideTouchTurbo, takes it off the screen). Several fingers press several buttons
// at once, and a finger that slides from one button onto another moves to it.
//
// They show when the screen is touched (and from the start on Android, unless a controller is
// connected, and again on Android when the controller is disconnected), and hide when a controller
// or the keyboard is used, or while the options menu is open, where touching works as the mouse
// does. With the option "No touch buttons with a controller" (settings.json's hideTouchButtons)
// they do not show at all while a controller is connected, not even when the screen is touched.
#pragma once

#include <dolphin/pad.h>

#include <SDL3/SDL_events.h>

namespace touch_buttons {

// Every event of the window (aurora_host.cpp): the fingers, and the controller and keyboard that
// hide the buttons.
void handle_event(const SDL_Event& event);

// Whether they are showing (in_game_menu.cpp then gets the lettering ready).
bool visible();

// Inside each frame of the game, before it ends (aurora_host.cpp).
void draw();

// Whether a finger holds TURBO (aurora_host.cpp then runs the game faster, see game_clock.h).
bool turbo_held();

// What the fingers press, added to what controller 1 reads (input/pad_natives.cpp).
void add_to(PADStatus& status);

}  // namespace touch_buttons
