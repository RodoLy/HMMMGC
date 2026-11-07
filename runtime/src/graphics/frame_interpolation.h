// More frames than the game draws: 60 or 120 a second from the game's 30.
//
// Más cuadros de los que dibuja el juego: 60 o 120 por segundo a partir de sus 30.
//
// The game works out and draws 30 frames a second (one every two video fields). To show
// more, aurora makes the frames in between: it keeps each frame's draws on the GPU and
// draws them again with every perspective draw's transforms (position, normals,
// projection) part of the way between the frame before and this one, matching the draws
// of the two frames by what they draw. The 2D layer (HUD, menus) is drawn as it is, and
// the game's logic is not touched: only what it shows.
//
// aurora counts in frames between those of a game running at 60: its 120 is one frame in
// between, its 240 three. This game runs at 30, so 120 there is 60 here and 240 is 120.
// The runtime tells aurora when each frame of the game is due (present_schedule), and
// aurora shows the frames in between spread over that time, from its render thread: the
// picture is shown up to one game frame later.
#pragma once

namespace frame_interpolation {

constexpr unsigned GAME_FRAME_RATE = 30;
constexpr unsigned MAXIMUM_FRAME_RATE = 120;

// Called before the game starts (main.cpp): 30 (off), 60 or 120.
void configure(unsigned frame_rate);

// Called by aurora_host.cpp once aurora is running.
void apply_to_aurora();

// Whether frames are made in between.
bool is_on();

// Called by aurora_host.cpp right before each frame of the game goes to aurora: tells aurora
// when the frame is due. Now and then it writes to the log how the frames went.
void schedule_frame();

}  // namespace frame_interpolation
