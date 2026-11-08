// How much of the map the game draws around the player, made to follow the zoom.
//
// Cuánto del mapa dibuja el juego alrededor del jugador, hecho para seguir al zoom.
//
// The game builds the places outdoors from square pieces (the farm is 7 by 7 pieces of 24
// by 24) and, each frame, draws only the pieces within 12 of the player along the map's
// axes (the field's update, fn_8003E89C, tests the player's position against each piece's
// square widened by that distance, a constant of the game at 0x80322BE0). On the farm its
// camera shows about half of that, so the pieces always cover the screen. Zoomed out, the
// camera shows farther than those pieces reach: the top of the screen showed black where
// the ground should be, and things came and went at the edges.
//
// While the zoom shows more than the game's camera, the runtime widens that distance in the
// game's memory by as much as the camera now shows farther: it measures how far from the
// point the camera looks at the screen's corners reach on the ground, with the zoom and
// with the game's own projection, and multiplies the 12 by the ratio. With the game's
// camera (zoom 1, also in widescreen) the distance is the game's, and so is the work.
#pragma once

namespace map_draw_range {

// Called once a frame of the game, after camera_view::follow_scene (aurora_host.cpp): sets
// the distance for the next frame.
void update();

}  // namespace map_draw_range
