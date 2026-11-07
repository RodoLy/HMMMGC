// The window and the graphics device, through aurora.
//
// La ventana y el dispositivo gráfico, por medio de aurora.
//
// aurora (https://github.com/encounter/aurora) implements the GameCube's graphics
// library (GX) on top of WebGPU (Dawn): Direct3D 12 on Windows, Vulkan on Linux, Metal on
// macOS. The game's own GX functions are replaced by natives that call aurora's (see
// gx_natives_*.cpp); this module owns aurora itself: it opens the window, frames what the
// game draws between two copies to the screen, and lets the window's events through.
#pragma once

namespace aurora_host {

// Opens the window and the graphics device, and starts the first frame. Call once,
// before the game's first graphics call. Exits the program with an error when no
// graphics device can be used.
void initialize();

bool is_initialized();

// Ends the frame that the game has drawn so far, shows it, handles the window's events
// and starts the next frame. Called when the game copies a finished picture to the
// screen (GXCopyDisp).
void present_frame();

// Gives aurora the pending window events without drawing anything; returns false once
// the player has closed the window.
bool process_events();

void shutdown();

}  // namespace aurora_host
