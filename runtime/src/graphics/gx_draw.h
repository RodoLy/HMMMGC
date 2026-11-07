// The game's draws, handed to aurora: primitives whose vertices the game writes to the
// write-gather pipe, and display lists.
//
// Los dibujos del juego, entregados a aurora: primitivas cuyos vértices el juego escribe en
// la tubería de escritura (write-gather pipe), y listas de visualización.
//
// On the console GXBegin sends the GPU a draw command with a vertex count, and the game
// then stores each vertex, value by value, at 0xCC008000; the GPU takes them as they come.
// The translated game's stores to that address arrive here (abi_hooks.cpp). A primitive
// is collected until it has all its vertices and then handed to aurora in one piece,
// after telling aurora how much of each array its indices use (aurora copies that much to
// the GPU first). The same is done for a display list that came from the game's files:
// its draws are walked for their highest indices before aurora reads it.
//
// Every GX native calls finish_primitive() first, so that the state a primitive was
// drawn with is the state the game had when it wrote it.
#pragma once

#include <cstdint>

namespace gx_draw {

// GXBegin: a primitive (GXPrimitive) of `vertex_count` vertices in a vertex format.
void begin_primitive(uint32_t primitive, uint32_t vertex_format, uint32_t vertex_count);

// Bytes that the game stored at the write-gather pipe, in the order it stored them.
void write_to_pipe(const uint8_t* bytes, uint32_t size);

// Draws the open primitive, if any.
void finish_primitive();

// GXBeginDisplayList and GXEndDisplayList: aurora records into the game's buffer.
void begin_display_list(uint32_t guest_address, uint32_t size);
uint32_t end_display_list();

// GXCallDisplayList.
void call_display_list(uint32_t guest_address, uint32_t size);

// Whether the game is building a display list (between GXBeginDisplayList and
// GXEndDisplayList).
bool is_building_display_list();

}  // namespace gx_draw
