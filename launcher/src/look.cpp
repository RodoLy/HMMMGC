// See look.h.

#include "look.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>

namespace look {
namespace {

ImVec2 g_origin;
float g_scale = 1.f;

constexpr float PI = 3.14159265358979f;

float channel(ImU32 color, int shift) { return static_cast<float>((color >> shift) & 0xFF) / 255.f; }

ImU32 pack(float r, float g, float b, float a) {
    const auto byte = [](float value) { return static_cast<ImU32>(std::clamp(value, 0.f, 1.f) * 255.f + 0.5f); };
    return byte(r) << IM_COL32_R_SHIFT | byte(g) << IM_COL32_G_SHIFT | byte(b) << IM_COL32_B_SHIFT |
           byte(a) << IM_COL32_A_SHIFT;
}

// The outline of a rounded rectangle in window pixels, as ImGui's current path. CSS makes the
// corners smaller, all by the same factor, when two of them do not fit on a side.
void path_rounded(ImDrawList* draw, ImVec2 min, ImVec2 max, Corners radius) {
    const float width = max.x - min.x;
    const float height = max.y - min.y;
    float factor = 1.f;
    const auto limit = [&](float side, float a, float b) {
        if (a + b > side && a + b > 0.f) {
            factor = std::min(factor, side / (a + b));
        }
    };
    limit(width, radius.top_left, radius.top_right);
    limit(width, radius.bottom_left, radius.bottom_right);
    limit(height, radius.top_left, radius.bottom_left);
    limit(height, radius.top_right, radius.bottom_right);
    const float tl = radius.top_left * factor;
    const float tr = radius.top_right * factor;
    const float br = radius.bottom_right * factor;
    const float bl = radius.bottom_left * factor;
    // Each corner from where the side before it ends; a corner without radius is its point.
    const auto arc = [&](float r, ImVec2 point, ImVec2 center, float from) {
        if (r < 0.5f) {
            draw->PathLineTo(point);
        } else {
            draw->PathArcTo(center, r, from, from + PI * 0.5f);
        }
    };
    arc(tl, min, ImVec2(min.x + tl, min.y + tl), PI);
    arc(tr, ImVec2(max.x, min.y), ImVec2(max.x - tr, min.y + tr), PI * 1.5f);
    arc(br, max, ImVec2(max.x - br, max.y - br), 0.f);
    arc(bl, ImVec2(min.x, max.y), ImVec2(min.x + bl, max.y - bl), PI * 0.5f);
}

Corners in_window(Corners radius) {
    return {to_window(radius.top_left), to_window(radius.top_right), to_window(radius.bottom_right),
            to_window(radius.bottom_left)};
}

// The points of a rounded rectangle's outline, in window pixels (to clip shapes with it).
std::vector<ImVec2> outline(const Rect& rect, Corners radius) {
    ImDrawList scratch(ImGui::GetDrawListSharedData());
    scratch._ResetForNewFrame();
    path_rounded(&scratch, to_window(ImVec2(rect.x, rect.y)), to_window(ImVec2(rect.right(), rect.bottom())),
                 in_window(radius));
    return {scratch._Path.Data, scratch._Path.Data + scratch._Path.Size};
}

// The part of the convex polygon `shape` inside the convex polygon `clip` (both clockwise on
// screen), by Sutherland and Hodgman's method.
std::vector<ImVec2> clip_convex(std::vector<ImVec2> shape, const std::vector<ImVec2>& clip) {
    for (size_t i = 0; i < clip.size() && !shape.empty(); ++i) {
        const ImVec2 a = clip[i];
        const ImVec2 b = clip[(i + 1) % clip.size()];
        const auto inside = [&](ImVec2 p) { return (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x) >= 0.f; };
        const auto crossing = [&](ImVec2 p, ImVec2 q) {
            const float d1 = (b.x - a.x) * (p.y - a.y) - (b.y - a.y) * (p.x - a.x);
            const float d2 = (b.x - a.x) * (q.y - a.y) - (b.y - a.y) * (q.x - a.x);
            return p + (q - p) * (d1 / (d1 - d2));
        };
        std::vector<ImVec2> kept;
        for (size_t j = 0; j < shape.size(); ++j) {
            const ImVec2 p = shape[j];
            const ImVec2 q = shape[(j + 1) % shape.size()];
            if (inside(p)) {
                kept.push_back(p);
                if (!inside(q)) {
                    kept.push_back(crossing(p, q));
                }
            } else if (inside(q)) {
                kept.push_back(crossing(p, q));
            }
        }
        shape = std::move(kept);
    }
    return shape;
}

// -- Text ------------------------------------------------------------------------------------------

struct Placed {
    const ImFontGlyph* glyph;
    float x;
    float scale;  // how much bigger than ImGui measures it (lettering::glyph_scale)
    float thick;  // the browser's thickening, in window pixels
};

// The glyphs of `text` from `x`, with the spacing after each one; `end` is where the last ends.
std::vector<Placed> place(const TextStyle& style, ImFont* font, float x, std::string_view text, float& end) {
    std::vector<Placed> placed;
    const float size = static_cast<float>(style.size) * g_scale;
    const char* at = text.data();
    const char* stop = text.data() + text.size();
    ImWchar previous = 0;
    while (at < stop) {
        unsigned int c = 0;
        at += ImTextCharFromUtf8(&c, at, stop);
        const ImFontGlyph* glyph = font->FindGlyph(static_cast<ImWchar>(c));
        if (glyph == nullptr) {
            continue;
        }
        if (previous != 0) {
            x += lettering::kerning(style.face, previous, static_cast<ImWchar>(c)) * size;
        }
        const float scale = lettering::glyph_scale(style.face, style.size, static_cast<ImWchar>(c));
        placed.push_back({glyph, x, scale, lettering::thickening(static_cast<ImWchar>(c), style.size) * g_scale});
        x += glyph->AdvanceX * scale + style.spacing * size;
        previous = static_cast<ImWchar>(c);
    }
    end = x;
    return placed;
}

// The glyphs on the line of `baseline` (ImGui keeps them measured from the top of a line `ascent`
// above it), moved by `offset`.
void put_glyphs(ImDrawList* draw, const std::vector<Placed>& placed, float baseline, float ascent, ImVec2 offset,
                ImU32 color, bool whole_pixels) {
    for (const Placed& p : placed) {
        if (!p.glyph->Visible) {
            continue;
        }
        const float x = whole_pixels ? std::round(p.x + offset.x) : p.x + offset.x;
        const float y = baseline + offset.y;
        draw->PrimReserve(6, 4);
        draw->PrimRectUV(ImVec2(x + p.glyph->X0 * p.scale, y + (p.glyph->Y0 - ascent) * p.scale),
                         ImVec2(x + p.glyph->X1 * p.scale, y + (p.glyph->Y1 - ascent) * p.scale),
                         ImVec2(p.glyph->U0, p.glyph->V0), ImVec2(p.glyph->U1, p.glyph->V1), color);
    }
}

// Copies of the glyphs all around `offset` at `radius`: the letters grown by that much, as an
// outline painted under them, or a font made bold by the browser.
void put_grown(ImDrawList* draw, const std::vector<Placed>& placed, float baseline, float ascent, ImVec2 offset,
               float radius, ImU32 color, bool whole_pixels) {
    put_glyphs(draw, placed, baseline, ascent, offset, color, whole_pixels);
    if (radius < 0.2f) {
        return;
    }
    for (const float ring : {radius, radius * 0.5f}) {
        const int steps = std::max(8, static_cast<int>(std::ceil(2.f * PI * ring / 1.2f)));
        for (int step = 0; step < steps; ++step) {
            const float angle = 2.f * PI * static_cast<float>(step) / static_cast<float>(steps);
            put_glyphs(draw, placed, baseline, ascent, offset + ImVec2(std::cos(angle), std::sin(angle)) * ring, color,
                       false);
        }
    }
}

float cubic_bezier(float x1, float y1, float x2, float y2, float x) {
    if (x <= 0.f || x >= 1.f) {
        return std::clamp(x, 0.f, 1.f);
    }
    const auto at = [](float a, float b, float t) {
        const float u = 1.f - t;
        return 3.f * u * u * t * a + 3.f * u * t * t * b + t * t * t;
    };
    float low = 0.f;
    float high = 1.f;
    float t = x;
    for (int i = 0; i < 40; ++i) {
        const float value = at(x1, x2, t);
        if (std::fabs(value - x) < 1e-5f) {
            break;
        }
        (value < x ? low : high) = t;
        t = (low + high) * 0.5f;
    }
    return at(y1, y2, t);
}

}  // namespace

// -- The canvas and the colors ---------------------------------------------------------------------

void set_canvas(ImVec2 origin, float scale) {
    g_origin = origin;
    g_scale = scale;
}

float scale() { return g_scale; }
ImVec2 to_window(ImVec2 point) { return g_origin + point * g_scale; }
float to_window(float length) { return length * g_scale; }
ImVec2 to_design(ImVec2 window_point) { return (window_point - g_origin) / g_scale; }

ImU32 over(ImU32 top, ImU32 bottom) {
    const float ta = channel(top, IM_COL32_A_SHIFT);
    const float ba = channel(bottom, IM_COL32_A_SHIFT) * (1.f - ta);
    const float alpha = ta + ba;
    if (alpha <= 0.f) {
        return 0;
    }
    const auto blend = [&](int shift) { return (channel(top, shift) * ta + channel(bottom, shift) * ba) / alpha; };
    return pack(blend(IM_COL32_R_SHIFT), blend(IM_COL32_G_SHIFT), blend(IM_COL32_B_SHIFT), alpha);
}

ImU32 mix(ImU32 from, ImU32 to, float amount) {
    const auto lerp = [&](int shift) {
        return channel(from, shift) + (channel(to, shift) - channel(from, shift)) * amount;
    };
    return pack(lerp(IM_COL32_R_SHIFT), lerp(IM_COL32_G_SHIFT), lerp(IM_COL32_B_SHIFT), lerp(IM_COL32_A_SHIFT));
}

bool Rect::contains(ImVec2 point) const {
    return point.x >= x && point.x < right() && point.y >= y && point.y < bottom();
}

Rect Rect::inset(float top, float right_side, float bottom_side, float left) const {
    return {x + left, y + top, std::max(0.f, w - left - right_side), std::max(0.f, h - top - bottom_side)};
}

Corners Corners::inside(float top, float right, float bottom, float left) const {
    return {std::max(0.f, top_left - std::max(top, left)), std::max(0.f, top_right - std::max(top, right)),
            std::max(0.f, bottom_right - std::max(bottom, right)), std::max(0.f, bottom_left - std::max(bottom, left))};
}

Corners Corners::grown(float by) const {
    const auto grow = [&](float r) { return r > 0.f ? r + by : 0.f; };
    return {grow(top_left), grow(top_right), grow(bottom_right), grow(bottom_left)};
}

// -- Shapes ----------------------------------------------------------------------------------------

void fill(ImDrawList* draw, const Rect& rect, Corners radius, ImU32 color, ImU32 bottom_color) {
    if (rect.w <= 0.f || rect.h <= 0.f) {
        return;
    }
    const ImVec2 min = to_window(ImVec2(rect.x, rect.y));
    const ImVec2 max = to_window(ImVec2(rect.right(), rect.bottom()));
    path_rounded(draw, min, max, in_window(radius));
    const int first = draw->VtxBuffer.Size;
    draw->PathFillConvex(color);
    if (bottom_color != 0) {
        ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw, first, draw->VtxBuffer.Size, min, ImVec2(min.x, max.y),
                                                      color, bottom_color);
    }
}

void draw_box(ImDrawList* draw, const Box& box) {
    for (auto shadow = box.shadows.rbegin(); shadow != box.shadows.rend(); ++shadow) {
        if (shadow->color != 0) {
            fill(draw, box.rect.moved(shadow->x, shadow->y).inset(-shadow->spread), box.radius.grown(shadow->spread),
                 shadow->color);
        }
    }
    const Sides& border = box.border;
    const bool bordered = border.top > 0.f || border.right > 0.f || border.bottom > 0.f || border.left > 0.f;
    if (bordered) {
        fill(draw, box.rect, box.radius, box.border_color);
    }
    Rect inner = box.rect.inset(border.top, border.right, border.bottom, border.left);
    Corners inner_radius = box.radius.inside(border.top, border.right, border.bottom, border.left);
    if (box.inner_ring > 0.f) {
        fill(draw, inner, inner_radius, box.inner_ring_color);
        inner = inner.inset(box.inner_ring);
        inner_radius = inner_radius.inside(box.inner_ring, box.inner_ring, box.inner_ring, box.inner_ring);
    }
    if (box.background != 0) {
        fill(draw, inner, inner_radius, box.background, box.background_bottom);
    }
}

void dashed_border(ImDrawList* draw, const Rect& rect, Corners radius, float width, ImU32 color) {
    // Along the middle of the border: dashes twice as long as the border is wide, gaps as long.
    const Rect middle = rect.inset(width * 0.5f);
    const std::vector<ImVec2> points = outline(middle, radius.inside(width * 0.5f, width * 0.5f, width * 0.5f,
                                                                     width * 0.5f));
    const float dash = to_window(width * 2.f);
    const float gap = to_window(width);
    const float thickness = to_window(width);
    float position = 0.f;  // along the current dash or gap
    bool drawing = true;
    std::vector<ImVec2> current = {points.empty() ? ImVec2() : points[0]};
    for (size_t i = 0; i < points.size(); ++i) {
        ImVec2 from = points[i];
        const ImVec2 to = points[(i + 1) % points.size()];
        float left = ImSqrt(ImLengthSqr(to - from));
        while (left > 0.f) {
            const float wanted = (drawing ? dash : gap) - position;
            const float step = std::min(wanted, left);
            const ImVec2 direction = (to - from) / std::max(left, 1e-4f);
            from = from + direction * step;
            left -= step;
            position += step;
            if (drawing) {
                current.push_back(from);
            }
            if (position >= (drawing ? dash : gap) - 1e-3f) {
                if (drawing && current.size() > 1) {
                    draw->AddPolyline(current.data(), static_cast<int>(current.size()), color, ImDrawFlags_None,
                                      thickness);
                }
                drawing = !drawing;
                position = 0.f;
                current = {from};
            }
        }
    }
    if (drawing && current.size() > 1) {
        draw->AddPolyline(current.data(), static_cast<int>(current.size()), color, ImDrawFlags_None, thickness);
    }
}

void inset_bottom_shadow(ImDrawList* draw, const Rect& rect, Corners radius, float height, ImU32 color) {
    // The shape's bottom edge and the same edge `height` higher: the band between them, a strip of
    // quads along the edge (where it curves up at the corners, the band curves with it).
    const ImVec2 min = to_window(ImVec2(rect.x, rect.y));
    const ImVec2 max = to_window(ImVec2(rect.right(), rect.bottom()));
    const float left = to_window(radius.bottom_left);
    const float right = to_window(radius.bottom_right);
    ImDrawList scratch(ImGui::GetDrawListSharedData());
    scratch._ResetForNewFrame();
    if (left >= 0.5f) {
        scratch.PathArcTo(ImVec2(min.x + left, max.y - left), left, PI, PI * 0.5f);
    } else {
        scratch.PathLineTo(ImVec2(min.x, max.y));
    }
    if (right >= 0.5f) {
        scratch.PathArcTo(ImVec2(max.x - right, max.y - right), right, PI * 0.5f, 0.f);
    } else {
        scratch.PathLineTo(ImVec2(max.x, max.y));
    }
    const int points = scratch._Path.Size;
    if (points < 2) {
        return;
    }
    const ImVec2 rise(0.f, to_window(height));
    const ImVec2 uv = draw->_Data->TexUvWhitePixel;
    draw->PrimReserve((points - 1) * 6, points * 2);
    const auto first = static_cast<ImDrawIdx>(draw->_VtxCurrentIdx);
    for (const ImVec2& point : scratch._Path) {
        draw->PrimWriteVtx(point, uv, color);
        draw->PrimWriteVtx(ImVec2(point.x, std::max(min.y, point.y - rise.y)), uv, color);
    }
    for (int i = 0; i + 1 < points; ++i) {
        const auto at = static_cast<ImDrawIdx>(first + i * 2);
        draw->PrimWriteIdx(at);
        draw->PrimWriteIdx(static_cast<ImDrawIdx>(at + 2));
        draw->PrimWriteIdx(static_cast<ImDrawIdx>(at + 3));
        draw->PrimWriteIdx(at);
        draw->PrimWriteIdx(static_cast<ImDrawIdx>(at + 3));
        draw->PrimWriteIdx(static_cast<ImDrawIdx>(at + 1));
    }
}

void circle(ImDrawList* draw, ImVec2 center, float radius, ImU32 color) {
    draw->AddCircleFilled(to_window(center), to_window(radius), color, 0);
}

void rings(ImDrawList* draw, ImVec2 center, float largest, float period, float inner, float outer, ImU32 color) {
    for (float start = inner; start < largest; start += period) {
        draw->AddCircle(to_window(center), to_window(start + (outer - inner) * 0.5f), color, 0,
                        to_window(outer - inner));
    }
}

void checkerboard(ImDrawList* draw, const Rect& rect, float size, ImVec2 offset, ImU32 color) {
    // Squares on the places where the column and the row add up to an odd number.
    const float first_x = rect.x + std::fmod(offset.x, size * 2.f) - size * 2.f;
    const float first_y = rect.y + std::fmod(offset.y, size * 2.f) - size * 2.f;
    draw->PushClipRect(to_window(ImVec2(rect.x, rect.y)), to_window(ImVec2(rect.right(), rect.bottom())), true);
    int row = 0;
    for (float y = first_y; y < rect.bottom(); y += size, ++row) {
        int column = 0;
        for (float x = first_x; x < rect.right(); x += size, ++column) {
            if ((row + column) % 2 == 1) {
                draw->AddRectFilled(to_window(ImVec2(x, y)), to_window(ImVec2(x + size, y + size)), color);
            }
        }
    }
    draw->PopClipRect();
}

void stripes(ImDrawList* draw, const Rect& rect, Corners radius, ImU32 first, ImU32 second, float band, float shift) {
    // repeating-linear-gradient(135deg, first 0 band, second band 2*band): bands across the
    // diagonal, counted from the top left corner moved `shift` along the top side.
    fill(draw, rect, radius, second);
    const std::vector<ImVec2> shape = outline(rect, radius);
    const float step = band * std::sqrt(2.f);  // a band's width along a side
    const float span = rect.w + rect.h;
    for (float start = std::fmod(shift, step * 2.f) - step * 2.f; start < span; start += step * 2.f) {
        // The band from `start` to `start + step` along the top side, going down to the left.
        const float extent = span;
        std::vector<ImVec2> piece = {
            to_window(ImVec2(rect.x + start, rect.y)), to_window(ImVec2(rect.x + start + step, rect.y)),
            to_window(ImVec2(rect.x + start + step - extent, rect.y + extent)),
            to_window(ImVec2(rect.x + start - extent, rect.y + extent))};
        piece = clip_convex(std::move(piece), shape);
        if (piece.size() >= 3) {
            draw->AddConvexPolyFilled(piece.data(), static_cast<int>(piece.size()), first);
        }
    }
}

void bubble(ImDrawList* draw, ImVec2 center, float diameter, std::string_view note, int note_size) {
    // radial-gradient(circle at 32% 28%, white .9 up to 10%, .3 at 22%, .1 at 60%, .4 at 100%),
    // under a border of 2 pixels of white at .8.
    const float inside = diameter * 0.5f;
    const float outside = inside + 2.f;
    const ImVec2 light = center + ImVec2(-0.18f, -0.22f) * diameter;
    const float reach = diameter * std::sqrt(0.68f * 0.68f + 0.72f * 0.72f);
    const auto opacity = [&](ImVec2 point) {
        const float t = ImSqrt(ImLengthSqr(point - light)) / reach;
        constexpr float STOPS[][2] = {{0.f, 0.9f}, {0.1f, 0.9f}, {0.22f, 0.3f}, {0.6f, 0.1f}, {1.f, 0.4f}};
        for (size_t i = 1; i < std::size(STOPS); ++i) {
            if (t <= STOPS[i][0]) {
                const float part = (t - STOPS[i - 1][0]) / (STOPS[i][0] - STOPS[i - 1][0]);
                return STOPS[i - 1][1] + (STOPS[i][1] - STOPS[i - 1][1]) * std::max(0.f, part);
            }
        }
        return 0.4f;
    };
    // A disc of rings of vertices, each with the gradient's opacity where it is.
    constexpr int RINGS = 24;
    constexpr int SEGMENTS = 64;
    const ImVec2 uv = draw->_Data->TexUvWhitePixel;
    const int vertices = 1 + RINGS * SEGMENTS;
    const int indices = SEGMENTS * 3 + (RINGS - 1) * SEGMENTS * 6;
    draw->PrimReserve(indices, vertices);
    const ImDrawIdx first = static_cast<ImDrawIdx>(draw->_VtxCurrentIdx);
    const auto vertex = [&](ImVec2 point) {
        draw->PrimWriteVtx(to_window(point), uv, IM_COL32(255, 255, 255, static_cast<int>(opacity(point) * 255.f)));
    };
    vertex(center);
    for (int ring = 1; ring <= RINGS; ++ring) {
        const float r = outside * static_cast<float>(ring) / RINGS;
        for (int segment = 0; segment < SEGMENTS; ++segment) {
            const float angle = 2.f * PI * static_cast<float>(segment) / SEGMENTS;
            vertex(center + ImVec2(std::cos(angle), std::sin(angle)) * r);
        }
    }
    const auto at = [&](int ring, int segment) {
        return static_cast<ImDrawIdx>(first + 1 + (ring - 1) * SEGMENTS + (segment % SEGMENTS));
    };
    for (int segment = 0; segment < SEGMENTS; ++segment) {
        draw->PrimWriteIdx(first);
        draw->PrimWriteIdx(at(1, segment));
        draw->PrimWriteIdx(at(1, segment + 1));
    }
    for (int ring = 1; ring < RINGS; ++ring) {
        for (int segment = 0; segment < SEGMENTS; ++segment) {
            draw->PrimWriteIdx(at(ring, segment));
            draw->PrimWriteIdx(at(ring + 1, segment));
            draw->PrimWriteIdx(at(ring + 1, segment + 1));
            draw->PrimWriteIdx(at(ring, segment));
            draw->PrimWriteIdx(at(ring + 1, segment + 1));
            draw->PrimWriteIdx(at(ring, segment + 1));
        }
    }
    draw->AddCircle(to_window(center), to_window(inside + 1.f), rgb(0xffffff, 0.8f), 0, to_window(2.f));
    const TextStyle style{.face = lettering::Face::Baloo800, .size = note_size, .color = IM_COL32_WHITE};
    draw_text(draw, style, center.x - text_width(style, note) * 0.5f, baseline_centered(style, center.y), note);
}

// -- Text ------------------------------------------------------------------------------------------

float text_width(const TextStyle& style, std::string_view text) {
    float end = 0.f;
    place(style, lettering::font(style.face, style.size), 0.f, text, end);
    return end / g_scale;
}

namespace {

// A line's ascent and descent as a browser lays it out: each a whole number of pixels of the window.
struct LineMetrics {
    float ascent;
    float descent;
};

LineMetrics line_metrics(const TextStyle& style) {
    const float size = static_cast<float>(style.size) * g_scale;
    return {std::round(lettering::ascent(style.face) * size) / g_scale,
            std::round(lettering::descent(style.face) * size) / g_scale};
}

}  // namespace

float line_height(const TextStyle& style) {
    const LineMetrics metrics = line_metrics(style);
    return metrics.ascent + metrics.descent;
}

float baseline_centered(const TextStyle& style, float center_y) {
    const LineMetrics metrics = line_metrics(style);
    return center_y - (metrics.ascent + metrics.descent) * 0.5f + metrics.ascent;
}

float baseline_in_line(const TextStyle& style, float top, float line) {
    // The leading above the letters is a whole number of pixels, rounded down.
    const LineMetrics metrics = line_metrics(style);
    const float leading = std::floor((line - metrics.ascent - metrics.descent) * 0.5f * g_scale) / g_scale;
    return top + leading + metrics.ascent;
}

void draw_text(ImDrawList* draw, const TextStyle& style, float x, float baseline, std::string_view text) {
    ImFont* font = lettering::font(style.face, style.size);
    const ImVec2 start = to_window(ImVec2(x, baseline));
    // On a whole pixel, as a browser puts a line's baseline.
    const float line = std::round(start.y);
    // ImGui measures a glyph from the top of its line, a whole number of pixels above the baseline.
    const float ascent = std::round(font->Ascent);
    float end = 0.f;
    const std::vector<Placed> placed = place(style, font, start.x, text, end);
    // Big letters are made without fractions of a pixel: they go on whole pixels to stay sharp.
    const bool whole = static_cast<float>(style.size) * g_scale > 34.f;
    const float stroke = to_window(style.stroke * 0.5f);
    if (style.shadow_color != 0) {
        put_grown(draw, placed, line, ascent, to_window(ImVec2(style.shadow_x, style.shadow_y)), stroke,
                  style.shadow_color, whole);
    }
    if (style.stroke > 0.f && style.stroke_color != 0) {
        put_grown(draw, placed, line, ascent, ImVec2(), stroke, style.stroke_color, whole);
    }
    put_glyphs(draw, placed, line, ascent, ImVec2(), style.color, whole);
    // What the browser makes bold itself.
    for (const Placed& glyph : placed) {
        if (glyph.thick > 0.f) {
            put_grown(draw, {glyph}, line, ascent, ImVec2(), glyph.thick, style.color, whole);
        }
    }
}

std::vector<std::string> wrap(const TextStyle& style, std::string_view text, float width, bool balanced) {
    std::vector<std::string> lines;
    size_t paragraph_start = 0;
    while (paragraph_start <= text.size()) {
        size_t paragraph_end = text.find('\n', paragraph_start);
        if (paragraph_end == std::string_view::npos) {
            paragraph_end = text.size();
        }
        std::vector<std::string_view> words;
        std::string_view paragraph = text.substr(paragraph_start, paragraph_end - paragraph_start);
        while (!paragraph.empty()) {
            const size_t space = paragraph.find(' ');
            const std::string_view word = paragraph.substr(0, space);
            if (!word.empty()) {
                words.push_back(word);
            }
            paragraph = space == std::string_view::npos ? std::string_view() : paragraph.substr(space + 1);
        }
        std::vector<std::vector<std::string_view>> paragraph_lines;
        for (const std::string_view word : words) {
            if (!paragraph_lines.empty()) {
                std::string candidate;
                for (const std::string_view previous : paragraph_lines.back()) {
                    candidate.append(previous).push_back(' ');
                }
                candidate.append(word);
                if (text_width(style, candidate) <= width) {
                    paragraph_lines.back().push_back(word);
                    continue;
                }
            }
            paragraph_lines.push_back({word});
        }
        // text-wrap: pretty: a last line of a single word takes one from the line before it.
        const size_t count = paragraph_lines.size();
        if (balanced && count >= 2 && paragraph_lines[count - 1].size() == 1 && paragraph_lines[count - 2].size() > 2) {
            paragraph_lines[count - 1].insert(paragraph_lines[count - 1].begin(), paragraph_lines[count - 2].back());
            paragraph_lines[count - 2].pop_back();
        }
        for (const auto& line_words : paragraph_lines) {
            std::string line;
            for (size_t i = 0; i < line_words.size(); ++i) {
                line.append(i == 0 ? "" : " ").append(line_words[i]);
            }
            lines.push_back(std::move(line));
        }
        if (words.empty()) {
            lines.emplace_back();
        }
        paragraph_start = paragraph_end + 1;
    }
    return lines;
}

std::string fit(const TextStyle& style, std::string_view text, float width) {
    if (text_width(style, text) <= width) {
        return std::string(text);
    }
    std::string cut(text);
    while (!cut.empty()) {
        // Back to the start of the last character (UTF-8 continuation bytes are 10xxxxxx).
        do {
            cut.pop_back();
        } while (!cut.empty() && (static_cast<unsigned char>(cut.back()) & 0xC0) == 0x80);
        if (text_width(style, cut + "\xE2\x80\xA6") <= width) {
            break;
        }
    }
    return cut + "\xE2\x80\xA6";
}

// -- Movement --------------------------------------------------------------------------------------

float ease_out(float progress) { return cubic_bezier(0.f, 0.f, 0.58f, 1.f, progress); }
float ease_in_out(float progress) { return cubic_bezier(0.42f, 0.f, 0.58f, 1.f, progress); }

float progress(double seconds, float delay, float duration) {
    return std::clamp(static_cast<float>((seconds - delay) / duration), 0.f, 1.f);
}

Moved::Moved(ImDrawList* draw, float dx, float dy, float opacity, float scale, ImVec2 pivot)
    : draw_(draw), first_vertex_(draw->VtxBuffer.Size), dx_(dx), dy_(dy), opacity_(opacity), scale_(scale),
      pivot_(pivot) {}

Moved::~Moved() {
    if (dx_ == 0.f && dy_ == 0.f && scale_ == 1.f && opacity_ >= 1.f) {
        return;
    }
    const ImVec2 pivot = to_window(pivot_);
    const ImVec2 shift = ImVec2(dx_, dy_) * g_scale;
    for (int i = first_vertex_; i < draw_->VtxBuffer.Size; ++i) {
        ImDrawVert& vertex = draw_->VtxBuffer[i];
        vertex.pos = pivot + (vertex.pos - pivot) * scale_ + shift;
        const ImU32 alpha = (vertex.col >> IM_COL32_A_SHIFT) & 0xFF;
        const auto faded = static_cast<ImU32>(static_cast<float>(alpha) * std::clamp(opacity_, 0.f, 1.f) + 0.5f);
        vertex.col = (vertex.col & ~IM_COL32_A_MASK) | (faded << IM_COL32_A_SHIFT);
    }
}

}  // namespace look
