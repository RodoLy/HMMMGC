// The options menu inside the game: F10 (or the controller's Select button) opens it over the
// game and closes it again.
//
// El menú de opciones dentro del juego: F10 (o el botón Select del control) lo abre sobre el
// juego y lo vuelve a cerrar.
//
// It has the launcher's look and its options (shared/): the picture, the game's speed and the
// volume, which change at once (player_options::apply_changes) and are kept in settings.json
// when it closes, and the controls screen. Under it the game goes on, but gets nothing from the
// controllers or the keyboard: PADRead reads them as let go while the menu is open, and after it
// closes until the buttons that closed it are let go. The frame rate can show over the game at
// the top right. It is drawn with Dear ImGui through aurora, which draws it over each frame of
// the game, so it only moves while the game draws.
#pragma once

#include <dolphin/pad.h>

#include <SDL3/SDL_events.h>

namespace in_game_menu {

// A key of the window (aurora_host.cpp passes every event): F10 opens and closes the menu, and
// Escape closes it.
void handle_event(const SDL_Event& event);

// Before each frame of the game starts (aurora_host.cpp): the controller's button for the menu,
// and the lettering for the window's size.
void before_frame();

// Inside each frame of the game, before it ends (aurora_host.cpp): the menu when it is open, and
// the frame rate when the player wants it shown.
void draw();

bool is_open();

// Whether the player chose "Quit the game" (aurora_host.cpp then closes it, as the window's close
// button does).
bool quit_requested();

// Called with what PADRead read (input/pad_natives.cpp): while the menu is open, and after it
// closes until every button and stick is let go, the four ports read as let go.
void hold_back_controllers(PADStatus statuses[PAD_CHANMAX]);

}  // namespace in_game_menu
