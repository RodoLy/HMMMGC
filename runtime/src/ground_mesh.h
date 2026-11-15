// The farm's mesh of tilled soil around the player, made bigger for the far_camera mod.
//
// La malla de tierra labrada de la granja alrededor del jugador, agrandada para el mod
// far_camera.
//
// The farm's map has no tilled soil of its own: an object the game calls GROUND draws the
// tilled, watered and marked soil over the ground, as a mesh of 20 by 20 points that starts
// 9 tiles west and north of the player (fn_80064108, its update, every frame). The points'
// positions and two bytes for each (what soil it is, and what marks lie over it) are kept in
// tables at 0x802B9008 indexed by the point's position modulo 20, so that the mesh slides
// over them as the player walks; its drawing (fn_80063554) turns the 19 by 19 squares into
// triangles, in two display lists of 2166 corners (51 984 bytes: the game's room for display
// lists, fn_801B250C, keeps each under 64 KiB). With the game's camera that covers the
// screen; with the far_camera mod the camera sees past it, and the soil there appeared as
// the player walked.
//
// With the mod, the runtime gives the game a bigger mesh: tools/size_ground_mesh.py changes
// the game's translated GROUND functions to take the mesh's size and the addresses of its
// tables from the variables below; size_for_farthest_view() puts tables for the farthest view
// in the runtime's memory (guest_memory.h), and follow_zoom() sizes the mesh for the zoom of
// each frame, so that with the camera close the game does no more work than it did. Its display lists no longer fit in
// the game's room, so each of the two gets a room of its own in the runtime's memory, lent
// to the game's functions just while it is made (hmmm_ground_mesh_list_begins and _ends).
// Without the mod the variables keep the game's own size and tables, so the game draws
// exactly what it always did.
#pragma once

#include <cstdint>

// The mesh as the translated game reads it (C names, declared by tools/size_ground_mesh.py
// in the translated code). Rows of the tables are indexed by x and columns by z.
extern "C" {
extern uint32_t hmmm_ground_mesh_size;       // points to a side (the game's 20)
extern uint32_t hmmm_ground_mesh_squares;    // squares to a side, one less
extern uint32_t hmmm_ground_mesh_points;     // points in all
extern uint32_t hmmm_ground_mesh_row;        // bytes in a row of the positions
extern uint32_t hmmm_ground_mesh_corners;    // corners of the triangles of one drawing (6 a square)
extern float hmmm_ground_mesh_behind;        // tiles the mesh starts west and north of the player
extern uint32_t hmmm_ground_mesh_positions;  // [size][size] x, y, z of the point
extern uint32_t hmmm_ground_mesh_soil;       // [size][size] the soil of the point's tile
extern uint32_t hmmm_ground_mesh_marks;      // [size][size] what lies over it

// Called by fn_8006490C, which starts the game's tables over when the farm is loaded: starts
// the bigger ones over the same way.
void hmmm_ground_mesh_reset();
// Called just before and just after the drawing makes one of its display lists: lends it a
// room of its own for the list, then gives the game its room back.
void hmmm_ground_mesh_list_begins();
void hmmm_ground_mesh_list_ends();
}

namespace ground_mesh {

// Called before the game runs, once its data is in memory (main.cpp), when the far_camera
// mod is on: room for the mesh of the farthest the camera goes (as far as the farm's grid of
// crops, crop_grid.h). The mesh keeps the game's size until follow_zoom() changes it.
void size_for_farthest_view();

// Called before every frame (aurora_host.cpp) with the zoom of the picture (camera_view.h): the
// mesh's size for it, from the game's at zoom 1 to the farthest at zoom 0.5. Does nothing
// without size_for_farthest_view().
void follow_zoom(float zoom);

}  // namespace ground_mesh
