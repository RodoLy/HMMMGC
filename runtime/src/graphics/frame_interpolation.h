// More frames than the game draws: 60 or 120 a second from the game's 30.
//
// Más cuadros de los que dibuja el juego: 60 o 120 por segundo a partir de sus 30.
//
// The game works out and draws 30 frames a second. To show more, the frames in between
// are made by drawing a frame again with its objects and camera part of the way between
// where they were in the frame before and where they are now. The game's logic is not
// touched: only what it draws is.
//
// While this is on, the GX natives do not hand their work to aurora right away: each one
// records a step (gx_frame steps, run with run()), and when the game finishes its frame
// (GXCopyDisp) the whole frame is drawn several times. Every object of this game is drawn
// the same way: the game loads its position (and normal) matrix and calls a display list.
// A matrix is matched with the one of the frame before by the draw that follows it (the
// display list's address, and how many times the frame drew that list before), and each
// frame in between draws it at a mix of the two. A matrix without a match, or one that
// jumped too far (a camera cut, an object put somewhere else), is drawn as it is now.
//
// The frames in between are shown before the game's own frame, spread over the time the
// game takes for one: the picture is that much later (half a game frame at 60).
#pragma once

#include <array>
#include <cstdint>
#include <functional>

namespace frame_interpolation {

constexpr unsigned GAME_FRAME_RATE = 30;
constexpr unsigned MAXIMUM_FRAME_RATE = 120;

// Called before the game starts (main.cpp): 30 (off), 60 or 120.
void configure(unsigned frame_rate);

// Whether the GX natives record their work instead of handing it to aurora now. Not while
// the game builds a display list: aurora writes that into the list as it goes.
bool is_recording();

// Records one step of the frame.
void record(std::function<void()> step);

// Runs a GX step now, or records it for the end of the frame. The step must hold copies of
// everything it uses: the game changes its memory and its objects before the frame ends.
template <typename Step>
void run(Step&& step) {
    if (is_recording()) {
        record(std::function<void()>(std::forward<Step>(step)));
    } else {
        step();
    }
}

// The steps that load matrices, which the frames in between draw mixed with the frame
// before. Matrices are row by row, as GX takes them; a projection keeps its 4x4.
void record_position_matrix(const std::array<float, 12>& matrix, uint32_t slot);
void record_normal_matrix(const std::array<float, 12>& matrix, uint32_t slot);
void record_projection(const std::array<float, 16>& matrix, uint32_t type);

// Tells which draw the matrices loaded since the last one belong to: a display list (its
// address) or a primitive drawn right away (0 and its vertex count).
void mark_draw(uint32_t display_list_address, uint32_t size);

// GXCopyDisp: the game's frame is complete. Draws the frames in between and the game's
// own, calling `present` after each.
void finish_frame(const std::function<void()>& present);

}  // namespace frame_interpolation
