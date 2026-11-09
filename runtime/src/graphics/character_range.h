// How far from the camera the game keeps its characters (people, children, pets and farm
// animals) drawn and moving, made to follow the zoom.
//
// Hasta qué distancia de la cámara el juego mantiene a sus personajes (gente, niños,
// mascotas y animales de granja) dibujados y en movimiento, hecho para seguir al zoom.
//
// Every frame each character measures its distance from the camera task's point (the
// object at 0x80321D68, which follows the player) and, past 25 (a constant of the game at
// 0x803227DC, read only by fn_80019AA4, the frame handler that CharaTask and all its kinds
// of character share), hides its model and stops until it is near again. The game's
// measure adds the squared distance up three times before the square root, so 25 is about
// 14 units (25 / sqrt(3)). With the game's camera that is beyond the screen's corners.
// Zoomed out, the screen shows farther, and people and chickens appeared from nowhere
// near its edges.
//
// While the zoom shows more than the game's camera, the runtime multiplies that distance by
// how much farther the screen's corners reach on the ground (main_view.h), so the game keeps
// the same margin around what the screen shows. With the game's camera (zoom 1, also in
// widescreen) the distance is the game's, and so is the work.
#pragma once

namespace character_range {

// Called once a frame of the game, after camera_view::follow_scene (aurora_host.cpp): sets
// the distance for the next frame.
void update();

}  // namespace character_range
