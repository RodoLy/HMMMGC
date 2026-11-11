// The farm's grid of crops and items around the player, made bigger when the camera is
// zoomed out.
//
// La cuadrícula de cultivos y objetos de la granja alrededor del jugador, agrandada cuando
// la cámara está alejada.
//
// On the farm the ground is made of tiles of 1 by 1, and what stands on each (crops,
// weeds, stones, branches, stumps) is drawn by an object the game calls CROP. It does not
// keep a model for every tile of the farm: it keeps a square of 25 by 25 tiles around the
// player (12 to each side, two tiles further to the north, where the camera looks), in
// tables of 25 by 25 indexed by the tile's position modulo 25, so that the square slides
// over them as the player walks. The tiles of the four outer rows and columns only keep
// what stands out (the game's kind 6 and a few other items): most items are drawn up to 8
// tiles from the player. With the game's camera that covers the screen; zoomed out to 50%
// the camera sees past it, and the items there appeared as the player walked.
//
// Zoomed out, the runtime gives the game a bigger square: tools/size_crop_grid.py changes
// the game's translated CROP functions (fn_80072B0C and the eight that share its tables) to
// take the size of the square and the addresses of its tables from the variables below,
// and size_for_view() sets them, putting the bigger tables in the runtime's memory
// (guest_memory.h). The square's half size is a constant of the game (12.0 at 0x803230C8,
// read only by fn_80072B0C), written with them. At zoom 1 or closer the variables keep the
// game's own size and tables, so the game draws exactly what it always did.
#pragma once

#include <cstdint>

// The grid as the translated game reads it (C names, declared by tools/size_crop_grid.py
// in the translated code). Rows of the tables are indexed by x and columns by z.
extern "C" {
extern uint32_t hmmm_crop_grid_size;         // tiles to a side (odd)
extern uint32_t hmmm_crop_grid_inner_end;    // the last tile, from the square's first, of the part with all items
extern uint32_t hmmm_crop_grid_model_row;    // bytes in a row of the 8-word tables
extern uint32_t hmmm_crop_grid_item_row;     // bytes in a row of the halfword table
extern uint32_t hmmm_crop_grid_word_row;     // bytes in a row of the one-word tables
extern uint32_t hmmm_crop_grid_models;       // [size][size][8] models of what stands on the tile
extern uint32_t hmmm_crop_grid_attachments;  // [size][size][8] what each model carries (made with fn_801AC7E4)
extern uint32_t hmmm_crop_grid_items;        // [size][size] the item number (0x52D: nothing)
extern uint32_t hmmm_crop_grid_kinds;        // [size][size] the item's kind (fn_80030944)
extern uint32_t hmmm_crop_grid_effects;      // [size][size] the tile's effect object (CELL_EFFECT)
extern uint32_t hmmm_crop_grid_flags;        // [size][size] 1: second model list, 2: owns its models, 4: has an effect
extern uint32_t hmmm_crop_grid_heights;      // [size][size] the ground's height under the tile
}

namespace crop_grid {

// Called before the game runs, once its data is in memory (main.cpp), with the zoom the
// player chose and whether the picture is widescreen: sizes the grid for the view of the
// farm's camera.
void size_for_view(float zoom, bool widescreen);

// Whether the grid sized when the game started also covers the view of that zoom and shape;
// when it does not, the crops and items farthest from the player appear as the player walks
// (the options menu says so, in_game_menu.h).
bool covers_view(float zoom, bool widescreen);

}  // namespace crop_grid
