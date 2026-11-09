// The game's main view as the runtime reads it from the game's memory: where the screen's
// corners meet the ground, with the game's own projection and with the one the screen has
// now (widescreen and zoom, camera_view.h).
//
// La vista principal del juego tal como el runtime la lee de la memoria del juego: dónde
// tocan el suelo las esquinas de la pantalla, con la proyección del juego y con la que
// tiene ahora la pantalla (panorámica y zoom).
//
// Zoomed out, the camera shows more of the ground than the game expects, and the game keeps
// some things only around what its own camera shows, such as the map's pieces
// (map_draw_range.h). Each of those ranges is widened by how much farther the screen's
// corners reach now than with the game's projection; this is where both are measured, from
// the game's main view: its camera's field of view and aspect ratio, and the view matrix
// it drew the last frame with.
#pragma once

#include <algorithm>
#include <array>
#include <cstddef>
#include <limits>
#include <optional>

namespace main_view {

// A point or a direction in the game's world: x to the east, y up, z to the south.
struct Vector {
    double x = 0;
    double y = 0;
    double z = 0;
};

// Where the screen's four corners meet the ground: at the level of the point the camera
// looks at, as offsets from that point, in units of the camera's distance to it.
struct GroundCorners {
    std::array<Vector, 4> game;  // with the game's projection
    std::array<Vector, 4> now;   // with the screen's now
    // Whether a corner of the screen now looks at the horizon or above it (`now` is then
    // not filled in).
    bool now_reaches_horizon = false;
};

// None when the game's memory holds no main view drawn in perspective, or when its camera
// does not look down at the ground with the game's projection.
std::optional<GroundCorners> ground_corners();

// How many times farther the corners reach now than with the game's projection, by a
// measure of how far an offset reaches (the farthest corner now over the farthest with the
// game's): at least 1, and infinite when the corners now reach the horizon.
template <typename Measure>
double reach_ratio(const GroundCorners& corners, Measure measure) {
    if (corners.now_reaches_horizon) {
        return std::numeric_limits<double>::infinity();
    }
    double game = 0;
    double now = 0;
    for (size_t i = 0; i < corners.game.size(); ++i) {
        game = std::max(game, measure(corners.game[i]));
        now = std::max(now, measure(corners.now[i]));
    }
    return game > 0 && now > game ? now / game : 1;
}

}  // namespace main_view
