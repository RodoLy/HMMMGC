// How much of the ground the game's shadows cover, made to follow the zoom.
//
// Cuánto del suelo cubren las sombras del juego, hecho para seguir al zoom.
//
// The game draws its shadows with a depth map: each frame it draws the scene from the light
// into a 256x256 picture and copies its depths to a texture, and the ground and everything
// else look that texture up to know whether something stands between them and the light.
// The light is a camera of its own (the view at 0x802AEDA0): far up along the light's
// direction, looking at the point the main camera looks at, with a field of view of 1
// degree that covers about 9 units around that point, a little more than the game's camera
// shows. The game builds both the projection of the depth map and the texture matrix that
// looks it up from that camera's field of view, every frame (fn_801B5588). Past the edge of
// the map the lookup repeats the depths at its edge: zoomed out, the screen showed those
// depths stretched across the ground, as dark bands and long shadows.
//
// While the zoom shows more than the game's camera, the runtime widens that field of view
// by how much farther the screen's corners reach on the ground along the light camera's
// axes (main_view.h), so the map covers what the screen shows with the game's margin. The
// map keeps its size (256x256 of the game's pixels, 256 times the internal resolution's
// scale on the PC), so the widened shadows are softer: zoomed out to 0.5 they keep the
// console's detail from an internal resolution of 3x up. With the game's camera (zoom 1,
// also in widescreen) the field of view is the game's.
#pragma once

namespace shadow_range {

// Called once a frame of the game, after camera_view::follow_scene (aurora_host.cpp): sets
// the light camera's field of view for the next frame.
void update();

}  // namespace shadow_range
