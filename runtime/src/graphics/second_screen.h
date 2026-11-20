// The second screen: on a handheld with two screens, such as the AYN Thor, the game's own menu
// (the one Y opens: belongings, farm, animals, the map...) goes to the lower screen, and the upper
// one keeps the farm as it was when the menu opened, still. Under the menu, buttons for the finger
// (the stick's four ways, L, R, B and A) play it as controller 1.
//
// La segunda pantalla: en una consola con dos pantallas, como el AYN Thor, el menú del propio
// juego (el que abre Y: pertenencias, granja, animales, el mapa...) va a la pantalla de abajo, y la
// de arriba se queda con el rancho como estaba al abrirlo, quieto. Debajo del menú hay botones
// para el dedo (las cuatro direcciones del stick, L, R, B y A) que juegan como el control 1.
//
// How it works
// ------------
// The app puts a surface on the other screen (android/.../SecondScreen.java, an Android
// Presentation) and hands it here with attach_android_window(). aurora draws the game with WebGPU
// on its render thread; the second screen gets its own WebGPU surface on the same device, made, used
// and let go on that same thread, so the device is only ever used from one place.
//
// The game draws its menu into the same picture as the farm, so the picture is what moves: aurora
// calls this code with each frame's picture before its window shows it (patches/aurora/
// show-the-picture-elsewhere.patch). While the menu is closed the picture is kept, one frame
// behind; while it is open, the picture is drawn on the second screen and the window is given the
// kept one, the farm before the menu, instead.
//
// The buttons under the menu are drawn with ImGui, in a second ImGui context of its own that shares
// aurora's fonts and renderer. Each finger on the second screen presses the button under it.
//
// On a computer, the environment variable HMMM_SECOND_SCREEN=<width>x<height> opens a second
// window that stands for the lower screen, with the mouse as the finger: that is how this is tried
// without the handheld (docs/NATIVE_RUNTIME.md).
#pragma once

#include <dolphin/pad.h>

#include <SDL3/SDL_events.h>

struct ANativeWindow;

namespace second_screen {

// What a finger did on the second screen, as Android's MotionEvent says it.
enum class Touch { Down = 0, Up = 1, Move = 2, Cancel = 3 };

// From the app's thread: the second screen's surface is there (with its size in pixels), or gone.
// detach waits until nothing draws on the surface any more, as Android asks before it takes it.
void attach_android_window(ANativeWindow* window, int width, int height);
void detach();

// From the app's thread: a finger on the second screen, at x and y from 0 to 1 of its size.
void touch(Touch action, int finger, float x, float y);

// Once, after aurora started: the test window, when HMMM_SECOND_SCREEN asks for one.
void open_test_window_if_asked();

// Every event of the game's windows: the test window's mouse.
void handle_event(const SDL_Event& event);

// Before each frame of the game (aurora_host.cpp): makes a surface given since the last one, and
// tells the app whether to use the other screen at all (the option "No second screen",
// settings.json's secondScreenOff), only when that changes.
void update(bool wanted);

// Inside each frame, before it ends (aurora_host.cpp): whether the game's menu is open, which puts
// it on the second screen with its buttons for this frame, and whether the player walks (where
// Y opens the menu).
void draw(bool game_menu_open, bool player_walks);

// Controller 1 as the game reads it (input/pad_natives.cpp): the buttons under the menu that the
// fingers hold are added to it, and its Y is watched, which starts opening the menu.
void add_to(PADStatus& status);

// Before aurora shuts down (aurora_host.cpp): the surface goes before the device it was made on.
void shutdown();

// Whether there is a second screen to draw on.
bool available();

}  // namespace second_screen
