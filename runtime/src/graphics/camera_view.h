// How the game's picture is framed on the PC: widescreen and zoom, applied to the
// projections the game gives GX (GXSetProjection) and to aurora's framebuffer.
//
// Cómo se encuadra la imagen del juego en la PC: pantalla ancha y zoom, aplicados a las
// proyecciones que el juego le da a GX y al framebuffer de aurora.
//
// Widescreen: aurora stretches the game's 4:3 picture to the whole window (16:9 when it
// opens; the 16:10 of a laptop's screen in fullscreen), and the horizontal scale of every
// perspective projection shrinks by as much (3/4 for 16:9), so that the 3D scene keeps
// its shape and shows more at the sides. The 2D layer (orthographic projections: backgrounds, the
// HUD, menus, text) is stretched to the width, as with Dolphin's widescreen hacks: scaling
// it back would leave the full-screen backgrounds 4:3 with bars at the sides.
// Zoom: a perspective projection's scale is multiplied by the zoom, which widens the camera's
// field of view below 1: 0.7 shows more of the farm around the player, and 0.5 is the most.
// The zoom never goes above 1, so the camera never comes closer than the game's. The 2D layer
// does not change. The zoom is for
// walking outdoors (game_scene.h): on the title screen, in cutscenes and in rooms the
// camera is the game's, and the change goes smoothly over a few frames.
// Screen marks: the game works out where a point of the scene lands on the screen
// (GXProject) to draw 2D marks over it, such as the bubble over a character's head; those
// points get the same change as the picture.
// Screen lookups: to show the player through what stands in front, the game copies the
// picture and draws those things again with that copy, looking it up with a texture matrix
// that it builds from its own projection (the screen position of each vertex). Those
// matrices get the same change as the projection, or the see-through parts would show the
// picture from somewhere else.
#pragma once

#include <cstdint>

namespace camera_view {

inline constexpr float MINIMUM_ZOOM = 0.5f;
inline constexpr float MAXIMUM_ZOOM = 1.f;

struct Options {
    bool widescreen = false;
    float zoom = 1.f;
};

// Called before the game starts (main.cpp).
void configure(const Options& options);

// Change them while the game runs (the options menu, in_game_menu.h): widescreen at the next
// projection the game sets, the zoom over a few frames, as when the player walks outdoors.
void set_widescreen(bool widescreen);
void set_zoom(float zoom);

// Called by aurora_host.cpp once aurora is running.
void apply_to_aurora();

// The window's size, when aurora starts and whenever it changes (aurora_host.cpp).
void set_window_size(unsigned width, unsigned height);

// Whether the picture is 16:9 (the window opens with that shape).
bool widescreen();

// Called once a frame of the game (aurora_host.cpp), with whether the zoom applies to the
// next one (game_scene::player_walks_outdoors): the zoom moves a step towards the chosen
// one, or towards 1.
void follow_scene(bool zoom_applies);

// The zoom now (1 when the camera is the game's).
float zoom();

// How much a perspective projection of the screen is scaled now, horizontally (zoom and
// widescreen) and vertically (zoom): below 1 the camera shows more than the game's.
float screen_x_scale();
float screen_y_scale();

// The viewport the game draws to (GXSetViewport): only the projections of the screen are
// changed, not those of pictures the game draws for itself (the shadows' depth map).
void set_viewport(float width, float height);

// Changes a projection matrix (row-major, as GXSetProjection takes it) of the given GX
// projection type (0 perspective, 1 orthographic) in place, when the viewport is the screen.
void adjust_projection(float matrix[16], int type);

// Changes the projection of a GXProject call in place, when it is a perspective one and its
// viewport is the screen: the seven numbers GXGetProjectionv gives (the type, 0 for a
// perspective one, then the matrix's entries 00, 02, 11, 12, 22 and 23).
void adjust_projection_numbers(float numbers[7], float viewport_width, float viewport_height);

// A position matrix the game loads (GXLoadPosMtxImm), for adjust_texture_matrix.
void set_position_matrix(const float matrix[12], uint32_t slot);

// Changes a texture matrix (GXLoadTexMtxImm, row-major) of the given GX type in place when
// it is a screen lookup of the current draw: the game's projection times the position
// matrix of slot 0 (loaded first), mapped to the texture.
void adjust_texture_matrix(float matrix[12], int type);

}  // namespace camera_view
