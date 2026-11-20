// The second screen: on a handheld with two screens, such as the AYN Thor, the options menu has the
// lower screen to itself and the game the upper one, so the menu never covers the game.
//
// La segunda pantalla: en una consola con dos pantallas, como el AYN Thor, el menú de opciones
// tiene la pantalla de abajo para él solo y el juego la de arriba, así que el menú nunca tapa el
// juego.
//
// How it works
// ------------
// The app puts a surface on the other screen (android/.../SecondScreen.java, an Android
// Presentation) and hands it here with attach_android_window(). aurora draws the game with WebGPU
// on its render thread; the second screen gets its own WebGPU surface on the same device, and
// everything done with it (making it, drawing a frame, letting it go) is queued on that same
// thread, so the device is only ever used from one place.
//
// The menu is drawn with ImGui, in a second ImGui context of its own (its own size, its own mouse)
// that shares the font atlas with aurora's: frame() makes that context current, runs the drawing it
// is given and gives aurora's back. aurora's ImGui renderer then draws the result on the second
// surface, with the same fonts and pipelines it uses for the game's screen.
//
// The fingers on the second screen come from the app (touch()) and act as ImGui's mouse there.
// When the menu has the controller (in_game_menu.h), give_controller() passes it the controller's
// buttons as ImGui's gamepad.
//
// On a computer, the environment variable HMMM_SECOND_SCREEN=<width>x<height> opens a second
// window that stands for the lower screen, with the mouse as the finger: that is how this is
// tried without the handheld (docs/NATIVE_RUNTIME.md).
#pragma once

#include <SDL3/SDL_events.h>
#include <SDL3/SDL_gamepad.h>

#include <functional>

struct ANativeWindow;
struct ImVec2;

namespace second_screen {

// What a finger did on the second screen, as Android's MotionEvent says it.
enum class Touch { Down = 0, Up = 1, Move = 2, Cancel = 3 };

// From the app's thread: the second screen's surface is there (with its size in pixels), or gone.
// detach waits until nothing draws on the surface any more, as Android asks before it takes it.
void attach_android_window(ANativeWindow* window, int width, int height);
void detach();

// From the app's thread: a finger on the second screen, at x and y from 0 to 1 of its size.
void touch(Touch action, int finger, float x, float y);

// Before each frame of the game (in_game_menu.cpp): makes a surface given since the last one, and
// tells the app whether to put the menu on the other screen at all (the option "No second screen",
// settings.json's secondScreenOff), only when that changes.
void update(bool wanted);

// Once, after aurora started: the test window, when HMMM_SECOND_SCREEN asks for one.
void open_test_window_if_asked();

// Every event of the game's windows: the mouse and the keys of the test window.
void handle_event(const SDL_Event& event);

// Before aurora shuts down (aurora_host.cpp): the surface goes before the device it was made on.
void shutdown();

// Whether there is a second screen to draw on.
bool available();

// Runs `draw` with the second screen's ImGui context current, as one frame of it, and queues that
// frame to be shown. `controller` is the gamepad whose buttons move in the menu, or null when the
// menu does not have the controller.
void frame(SDL_Gamepad* controller, const std::function<void(ImVec2 size)>& draw);

}  // namespace second_screen
