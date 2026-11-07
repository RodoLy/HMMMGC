// How the game's 3D picture is framed on the PC: the zoom, applied to the perspective
// projections the game gives GX (GXSetProjection).
//
// Cómo se encuadra la imagen 3D del juego en la PC: el zoom, aplicado a las proyecciones
// en perspectiva que el juego le da a GX.
//
// The zoom multiplies a perspective projection's scale, which narrows (above 1) or widens
// (below 1) the camera's field of view: 0.7 shows more of the farm around the player, 1.3
// brings the camera in. The 2D layer (the HUD, menus and text uses orthographic
// projections) does not change.
#pragma once

namespace camera_view {

inline constexpr float MINIMUM_ZOOM = 0.5f;
inline constexpr float MAXIMUM_ZOOM = 2.f;

struct Options {
    float zoom = 1.f;
};

// Called before the game starts (main.cpp).
void configure(const Options& options);

// Changes a projection matrix (row-major, as GXSetProjection takes it) of the given GX
// projection type (0 perspective, 1 orthographic) in place.
void adjust_projection(float matrix[16], int type);

}  // namespace camera_view
