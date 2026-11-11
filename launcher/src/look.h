// The pieces the launcher's design is drawn with (view.cpp puts them together): its coordinates,
// its colors, boxes with rounded corners, borders, gradients and shadows, text with letter spacing,
// outlines and shadows, and what moves (the easing of the animations).
//
// Las piezas con las que se dibuja el diseño del launcher: sus coordenadas, sus colores, cajas con
// esquinas redondeadas, bordes, degradados y sombras, texto con espaciado, contornos y sombras, y
// lo que se mueve.
//
// Everything is written as the design's CSS writes it, in its pixels: the design is 1280x752 (the
// window's title bar and the launcher under it), and the canvas turns its pixels into the window's.
#pragma once

#include "lettering.h"

#include <imgui.h>

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace look {

inline constexpr float WIDTH = 1280.f;
inline constexpr float HEIGHT = 752.f;

// Where the design is in the window and how big: `scale` window pixels for each of its pixels.
void set_canvas(ImVec2 origin, float scale);
float scale();
ImVec2 to_window(ImVec2 point);
float to_window(float length);
ImVec2 to_design(ImVec2 window_point);

// A color as CSS writes it: 0xRRGGBB and an opacity.
constexpr ImU32 rgb(uint32_t hex, float opacity = 1.f) {
    return IM_COL32((hex >> 16) & 0xFF, (hex >> 8) & 0xFF, hex & 0xFF, static_cast<int>(opacity * 255.f + 0.5f));
}
// The color `top` leaves over `bottom`.
ImU32 over(ImU32 top, ImU32 bottom);
// The color at `amount` (0 to 1) of the way from `from` to `to`.
ImU32 mix(ImU32 from, ImU32 to, float amount);

struct Rect {
    float x = 0.f;
    float y = 0.f;
    float w = 0.f;
    float h = 0.f;

    float right() const { return x + w; }
    float bottom() const { return y + h; }
    float center_x() const { return x + w * 0.5f; }
    float center_y() const { return y + h * 0.5f; }
    bool contains(ImVec2 point) const;
    Rect inset(float top, float right, float bottom, float left) const;
    Rect inset(float all) const { return inset(all, all, all, all); }
    Rect moved(float dx, float dy) const { return {x + dx, y + dy, w, h}; }
};

// The radius of each corner, in the order of border-radius.
struct Corners {
    float top_left = 0.f;
    float top_right = 0.f;
    float bottom_right = 0.f;
    float bottom_left = 0.f;

    constexpr Corners() = default;
    constexpr Corners(float all) : top_left(all), top_right(all), bottom_right(all), bottom_left(all) {}
    constexpr Corners(float tl, float tr, float br, float bl)
        : top_left(tl), top_right(tr), bottom_right(br), bottom_left(bl) {}
    // The corners of the inside of a border of these widths, as CSS rounds them.
    Corners inside(float top, float right, float bottom, float left) const;
    Corners grown(float by) const;
};

// The width of a border on each side.
struct Sides {
    float top = 0.f;
    float right = 0.f;
    float bottom = 0.f;
    float left = 0.f;

    constexpr Sides() = default;
    constexpr Sides(float all) : top(all), right(all), bottom(all), left(all) {}
    constexpr Sides(float t, float r, float b, float l) : top(t), right(r), bottom(b), left(l) {}
};

// An outer shadow of a box: a copy of its shape, moved and grown (the design's have no blur).
struct Shadow {
    float x = 0.f;
    float y = 0.f;
    float spread = 0.f;
    ImU32 color = 0;
};

// A box as CSS paints it: its shadows (the first on top), its border and its background, a color or
// a gradient from top to bottom; and a ring inside the border (an inset shadow that only spreads).
struct Box {
    Rect rect{};
    Corners radius{};
    ImU32 background = 0;
    ImU32 background_bottom = 0;  // not 0: the background goes from `background` to this
    Sides border{};
    ImU32 border_color = 0;
    std::array<Shadow, 2> shadows{};
    float inner_ring = 0.f;
    ImU32 inner_ring_color = 0;
};
void draw_box(ImDrawList* draw, const Box& box);

// A rounded shape filled with one color, or with a gradient from top to bottom.
void fill(ImDrawList* draw, const Rect& rect, Corners radius, ImU32 color, ImU32 bottom_color = 0);
// A border of `width` drawn in dashes, as CSS draws "dashed".
void dashed_border(ImDrawList* draw, const Rect& rect, Corners radius, float width, ImU32 color);
// Diagonal bands `band` wide, `first` and `second` in turn (repeating-linear-gradient at 135deg),
// counted from `shift` pixels to the right of the top left corner.
void stripes(ImDrawList* draw, const Rect& rect, Corners radius, ImU32 first, ImU32 second, float band,
             float shift = 0.f);
// The darker band an inset shadow of 0 -`height`px leaves at the bottom of a shape.
void inset_bottom_shadow(ImDrawList* draw, const Rect& rect, Corners radius, float height, ImU32 color);
void circle(ImDrawList* draw, ImVec2 center, float radius, ImU32 color);
// The rings of a repeating-radial-gradient: from `inner` to `outer` pixels out of every `period`.
void rings(ImDrawList* draw, ImVec2 center, float largest, float period, float inner, float outer, ImU32 color);
// Squares of `size` on every other place of a checkerboard over `rect`, which starts at `offset`.
void checkerboard(ImDrawList* draw, const Rect& rect, float size, ImVec2 offset, ImU32 color);
// A soap bubble with `note` inside it (the design's background), `diameter` across its inside.
void bubble(ImDrawList* draw, ImVec2 center, float diameter, std::string_view note, int note_size);

// -- Text ------------------------------------------------------------------------------------------

struct TextStyle {
    lettering::Face face = lettering::Face::Dosis700;
    int size = 16;
    float spacing = 0.f;  // letter-spacing, in ems
    ImU32 color = IM_COL32_WHITE;
    float stroke = 0.f;   // -webkit-text-stroke, painted under the letters
    ImU32 stroke_color = 0;
    float shadow_x = 0.f;  // text-shadow, without blur
    float shadow_y = 0.f;
    ImU32 shadow_color = 0;
};

// How wide `text` is, with the spacing after every letter as a browser counts it.
float text_width(const TextStyle& style, std::string_view text);
// The baseline of a line of `style` centered at `center_y` (line-height: normal).
float baseline_centered(const TextStyle& style, float center_y);
// The baseline of a line of `style` in a line box from `top` that is `line_height` tall.
float baseline_in_line(const TextStyle& style, float top, float line_height);
// How tall a line of `style` is with line-height: normal.
float line_height(const TextStyle& style);
// Writes `text` from `x` on the line of `baseline`.
void draw_text(ImDrawList* draw, const TextStyle& style, float x, float baseline, std::string_view text);
// The lines `text` takes at `width`, broken between words. `balanced` keeps the last line from
// holding a single word, as text-wrap: pretty does.
std::vector<std::string> wrap(const TextStyle& style, std::string_view text, float width, bool balanced = false);
// `text`, cut with "…" so that it fits in `width`.
std::string fit(const TextStyle& style, std::string_view text, float width);

// -- Movement --------------------------------------------------------------------------------------

// CSS's timing functions, for progress from 0 to 1.
float ease_out(float progress);
float ease_in_out(float progress);
// How far an animation that starts at `delay` seconds and lasts `duration` is at `seconds`.
float progress(double seconds, float delay, float duration);

// What is drawn between its construction and its end is moved by (`dx`, `dy`), grown `scale` times
// around `pivot` and faded to `opacity`, as a CSS transform and opacity do.
class Moved {
public:
    Moved(ImDrawList* draw, float dx, float dy, float opacity = 1.f, float scale = 1.f, ImVec2 pivot = {});
    Moved(const Moved&) = delete;
    Moved& operator=(const Moved&) = delete;
    ~Moved();

private:
    ImDrawList* draw_;
    int first_vertex_;
    float dx_;
    float dy_;
    float opacity_;
    float scale_;
    ImVec2 pivot_;
};

}  // namespace look
