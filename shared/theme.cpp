// See theme.h.

#include "theme.h"

#include <imgui_internal.h>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <cstdio>
#include <numbers>

// The fonts inside the program (shared/shared.cmake).
extern const unsigned char FONT_ROUNDED_MEDIUM[];
extern const size_t FONT_ROUNDED_MEDIUM_size;
extern const unsigned char FONT_ROUNDED_EXTRABOLD[];
extern const size_t FONT_ROUNDED_EXTRABOLD_size;

namespace theme {
namespace {

// The game's colors: its sky, grass and wood, the orange of the box of the date, the panels of
// the dialogue boxes.
constexpr ImU32 SKY_TOP = IM_COL32(43, 88, 172, 255);
constexpr ImU32 SKY_MIDDLE = IM_COL32(88, 140, 214, 255);
constexpr ImU32 SKY_HORIZON = IM_COL32(184, 216, 242, 255);
constexpr ImU32 CLOUD = IM_COL32(250, 252, 255, 255);
constexpr ImU32 CLOUD_SHADOW = IM_COL32(200, 219, 240, 255);
constexpr ImU32 HILL_FAR = IM_COL32(86, 160, 70, 255);
constexpr ImU32 HILL_BACK = IM_COL32(74, 146, 56, 255);
constexpr ImU32 HILL_FRONT = IM_COL32(108, 188, 70, 255);
constexpr ImU32 GRASS_LIGHT = IM_COL32(146, 212, 96, 255);
constexpr ImU32 WOOD_LIGHT = IM_COL32(226, 170, 98, 255);
constexpr ImU32 WOOD_DARK = IM_COL32(176, 112, 50, 255);
constexpr ImU32 WOOD_EDGE = IM_COL32(104, 64, 32, 255);
constexpr ImU32 WOOD_INNER = IM_COL32(248, 214, 154, 170);
constexpr ImU32 TEAL = IM_COL32(40, 168, 210, 255);
constexpr ImU32 RIBBON = IM_COL32(250, 238, 210, 255);
constexpr ImU32 RIBBON_EDGE = IM_COL32(190, 140, 80, 255);
constexpr ImU32 ORANGE_TOP = IM_COL32(253, 200, 96, 255);
constexpr ImU32 ORANGE_BOTTOM = IM_COL32(238, 132, 38, 255);
constexpr ImU32 ORANGE_EDGE = IM_COL32(160, 76, 18, 255);
constexpr ImU32 GREEN_TOP = IM_COL32(130, 218, 96, 255);
constexpr ImU32 GREEN_BOTTOM = IM_COL32(52, 156, 50, 255);
constexpr ImU32 GREEN_EDGE = IM_COL32(28, 98, 30, 255);
constexpr ImU32 GRAY_TOP = IM_COL32(214, 220, 226, 255);
constexpr ImU32 GRAY_BOTTOM = IM_COL32(164, 172, 182, 255);
constexpr ImU32 GRAY_EDGE = IM_COL32(98, 106, 116, 255);
constexpr ImU32 PANEL_TOP = IM_COL32(249, 251, 254, 250);
constexpr ImU32 PANEL_BOTTOM = IM_COL32(228, 236, 245, 250);
constexpr ImU32 PANEL_SHADOW = IM_COL32(18, 42, 88, 70);
constexpr ImU32 SECTION = IM_COL32(196, 104, 28, 255);
constexpr ImU32 TRACK = IM_COL32(205, 214, 224, 255);
constexpr ImU32 SWITCH_OFF = IM_COL32(178, 188, 199, 255);

// The characters the fonts are built with (the subset in fonts/README.md), in ascending order.
constexpr ImWchar GLYPHS[] = {0x0020, 0x017F, 0x2010, 0x205E, 0x20AC, 0x20AC, 0x2190, 0x2193, 0x25A0, 0x25CF,
                              0x2605, 0x2605, 0x266A, 0x266B, 0x2713, 0x2715, 0};

constexpr float PI = std::numbers::pi_v<float>;

float g_scale = 0.f;
Fonts g_fonts;

ImFont* add_font(const unsigned char* data, size_t size, float pixels) {
    ImFontConfig config;
    config.FontDataOwnedByAtlas = false;  // the data is the program's own array
    config.OversampleH = 2;
    config.OversampleV = 1;
    return ImGui::GetIO().Fonts->AddFontFromMemoryTTF(const_cast<unsigned char*>(data), static_cast<int>(size),
                                                      pixels, &config, GLYPHS);
}

ImVec4 color(ImU32 packed) { return ImGui::ColorConvertU32ToFloat4(packed); }

void apply_style(float scale) {
    ImGuiStyle style;
    style.WindowPadding = ImVec2(0, 0);
    style.WindowBorderSize = 0;
    style.ChildBorderSize = 0;
    style.FramePadding = ImVec2(10, 6);
    style.ItemSpacing = ImVec2(14, 14);
    style.ItemInnerSpacing = ImVec2(10, 8);
    style.ScrollbarSize = 14;
    style.ScrollbarRounding = 7;
    style.FrameRounding = 10;
    style.CellPadding = ImVec2(8, 10);
    style.ScaleAllSizes(scale);
    ImVec4* colors = style.Colors;
    colors[ImGuiCol_Text] = color(TEXT);
    colors[ImGuiCol_TextDisabled] = color(TEXT_SOFT);
    colors[ImGuiCol_WindowBg] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_ChildBg] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_PopupBg] = color(PANEL_TOP);
    colors[ImGuiCol_ScrollbarBg] = ImVec4(0.2f, 0.3f, 0.45f, 0.08f);
    colors[ImGuiCol_ScrollbarGrab] = color(IM_COL32(236, 160, 70, 200));
    colors[ImGuiCol_ScrollbarGrabHovered] = color(ORANGE_TOP);
    colors[ImGuiCol_ScrollbarGrabActive] = color(ORANGE_BOTTOM);
    colors[ImGuiCol_TableHeaderBg] = color(IM_COL32(255, 232, 196, 255));
    colors[ImGuiCol_TableRowBg] = ImVec4(0, 0, 0, 0);
    colors[ImGuiCol_TableRowBgAlt] = ImVec4(1, 1, 1, 0.35f);
    colors[ImGuiCol_TableBorderLight] = color(LINE);
    colors[ImGuiCol_Separator] = color(LINE);
    // The widgets draw their own focus, in the game's style.
    colors[ImGuiCol_NavCursor] = ImVec4(0, 0, 0, 0);
    ImGui::GetStyle() = style;
}

ImVec2 text_size(ImFont* font, float size, const char* text) {
    return font->CalcTextSizeA(size, FLT_MAX, 0.f, text);
}

// A rounded rectangle shaded from `top` to `bottom`.
void gradient_rect(ImDrawList* draw, ImVec2 min, ImVec2 max, ImU32 top, ImU32 bottom, float rounding) {
    const int first = draw->VtxBuffer.Size;
    draw->AddRectFilled(min, max, IM_COL32_WHITE, rounding);
    ImGui::ShadeVertsLinearColorGradientKeepAlpha(draw, first, draw->VtxBuffer.Size, min, ImVec2(min.x, max.y), top,
                                                  bottom);
}

// Whether the last item has the keyboard's or the controller's focus (not just the mouse's).
bool nav_focused() { return ImGui::IsItemFocused() && GImGui->NavCursorVisible; }

// A ring that pulses around a focused widget.
void focus_ring(ImDrawList* draw, ImVec2 min, ImVec2 max, float rounding) {
    const float pulse = 0.5f + 0.5f * std::sin(static_cast<float>(ImGui::GetTime()) * 5.f);
    const float gap = px(4);
    draw->AddRect(min - ImVec2(gap, gap), max + ImVec2(gap, gap),
                  IM_COL32(255, 255, 255, static_cast<int>(150 + 100 * pulse)), rounding + gap, 0, px(3));
}

// The left and right keys (and the controller's) for the focused item, which keeps them from
// moving the focus: -1, 0 or 1 (or more, when several are pressed).
int left_right(ImGuiID id) {
    int delta = 0;
    const ImGuiKey left[] = {ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadLStickLeft};
    const ImGuiKey right[] = {ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadLStickRight};
    for (const ImGuiKey key : left) {
        ImGui::SetKeyOwner(key, id);
        delta -= ImGui::IsKeyPressed(key, ImGuiInputFlags_Repeat, id) ? 1 : 0;
    }
    for (const ImGuiKey key : right) {
        ImGui::SetKeyOwner(key, id);
        delta += ImGui::IsKeyPressed(key, ImGuiInputFlags_Repeat, id) ? 1 : 0;
    }
    return delta;
}

void draw_cloud(ImDrawList* draw, ImVec2 center, float radius) {
    struct Puff {
        float x, y, size;
    };
    constexpr Puff PUFFS[] = {{0.f, 0.f, 1.f},       {-1.05f, 0.3f, 0.7f}, {1.08f, 0.26f, 0.76f},
                              {-0.5f, -0.4f, 0.74f}, {0.52f, -0.46f, 0.7f}, {0.1f, 0.36f, 0.86f}};
    for (const Puff& puff : PUFFS) {
        draw->AddCircleFilled(center + ImVec2(puff.x, puff.y + 0.16f) * radius, puff.size * radius, CLOUD_SHADOW, 40);
    }
    for (const Puff& puff : PUFFS) {
        draw->AddCircleFilled(center + ImVec2(puff.x, puff.y) * radius, puff.size * radius, CLOUD, 40);
    }
}

// A fence post with a pointed top, like the ones of the title screen.
void draw_post(ImDrawList* draw, ImVec2 bottom, float width, float height) {
    const ImVec2 points[] = {
        bottom + ImVec2(-width * 0.5f, 0.f),
        bottom + ImVec2(-width * 0.5f, -height),
        bottom + ImVec2(0.f, -height - width * 0.55f),
        bottom + ImVec2(width * 0.5f, -height),
        bottom + ImVec2(width * 0.5f, 0.f),
    };
    draw->AddConvexPolyFilled(points, 5, WOOD_DARK);
    draw->AddPolyline(points, 5, WOOD_EDGE, ImDrawFlags_Closed, px(2.5f));
    draw->AddLine(points[1] + ImVec2(width * 0.3f, width * 0.2f), points[0] + ImVec2(width * 0.3f, -width * 0.2f),
                  IM_COL32(255, 220, 160, 60), px(2));
}

}  // namespace

const Fonts& fonts() { return g_fonts; }

float px(float design_pixels) { return design_pixels * g_scale; }

bool set_scale(float scale) {
    scale = std::clamp(scale, 0.5f, 4.f);
    if (g_scale > 0.f && std::abs(scale - g_scale) < 0.03f) {
        return false;
    }
    g_scale = scale;
    ImGuiIO& io = ImGui::GetIO();
    io.Fonts->Clear();
    g_fonts.small = add_font(FONT_ROUNDED_MEDIUM, FONT_ROUNDED_MEDIUM_size, px(18));
    g_fonts.body = add_font(FONT_ROUNDED_MEDIUM, FONT_ROUNDED_MEDIUM_size, px(22));
    g_fonts.heading = add_font(FONT_ROUNDED_EXTRABOLD, FONT_ROUNDED_EXTRABOLD_size, px(30));
    g_fonts.button = add_font(FONT_ROUNDED_EXTRABOLD, FONT_ROUNDED_EXTRABOLD_size, px(27));
    g_fonts.subtitle = add_font(FONT_ROUNDED_EXTRABOLD, FONT_ROUNDED_EXTRABOLD_size, px(38));
    g_fonts.title = add_font(FONT_ROUNDED_EXTRABOLD, FONT_ROUNDED_EXTRABOLD_size, px(66));
    io.FontDefault = g_fonts.body;
    io.Fonts->Build();
    apply_style(scale);
    return true;
}

void draw_background(ImDrawList* draw, ImVec2 size, double seconds) {
    const float w = size.x;
    const float h = size.y;
    const float time = static_cast<float>(seconds);
    const float horizon = h * 0.74f;

    // The sky, darker above.
    draw->AddRectFilledMultiColor(ImVec2(0, 0), ImVec2(w, h * 0.42f), SKY_TOP, SKY_TOP, SKY_MIDDLE, SKY_MIDDLE);
    draw->AddRectFilledMultiColor(ImVec2(0, h * 0.42f), ImVec2(w, horizon + h * 0.1f), SKY_MIDDLE, SKY_MIDDLE,
                                  SKY_HORIZON, SKY_HORIZON);

    // Clouds drifting to the right, coming back in from the left.
    struct Cloud {
        float x, y, size, speed;
    };
    constexpr Cloud CLOUDS[] = {{0.08f, 0.16f, 0.9f, 7.f},  {0.36f, 0.08f, 0.62f, 5.f}, {0.62f, 0.2f, 1.1f, 9.f},
                                {0.86f, 0.1f, 0.7f, 6.f},   {0.22f, 0.42f, 0.8f, 11.f}, {0.74f, 0.46f, 0.66f, 8.f},
                                {0.5f, 0.56f, 0.5f, 12.f}};
    for (const Cloud& cloud : CLOUDS) {
        const float radius = px(46) * cloud.size;
        const float span = w + radius * 6.f;
        const float x = std::fmod(cloud.x * span + px(cloud.speed) * time, span) - radius * 3.f;
        draw_cloud(draw, ImVec2(x, cloud.y * h), radius);
    }

    // Rolling hills, the farthest first.
    draw->AddEllipseFilled(ImVec2(w * 0.12f, horizon + h * 0.30f), ImVec2(w * 0.42f, h * 0.36f), HILL_FAR, 0, 96);
    draw->AddEllipseFilled(ImVec2(w * 0.86f, horizon + h * 0.32f), ImVec2(w * 0.48f, h * 0.38f), HILL_FAR, 0, 96);
    draw->AddEllipseFilled(ImVec2(w * 0.5f, horizon + h * 0.42f), ImVec2(w * 0.72f, h * 0.42f), HILL_BACK, 0, 128);
    draw->AddEllipseFilled(ImVec2(w * 0.5f, h * 1.62f), ImVec2(w * 1.1f, h * 0.76f), GRASS_LIGHT, 0, 160);
    draw->AddEllipseFilled(ImVec2(w * 0.5f, h * 1.64f), ImVec2(w * 1.08f, h * 0.75f), HILL_FRONT, 0, 160);

    // The fence in front.
    const float post_width = px(16);
    const float post_height = px(58);
    const float ground = h - px(10);
    const float spacing = px(96);
    const ImVec2 rail_size(w, px(9));
    for (const float rail_y : {ground - post_height * 0.78f, ground - post_height * 0.38f}) {
        draw->AddRectFilled(ImVec2(0, rail_y), ImVec2(w, rail_y + rail_size.y), WOOD_DARK);
        draw->AddRect(ImVec2(-px(4), rail_y), ImVec2(w + px(4), rail_y + rail_size.y), WOOD_EDGE, 0, 0, px(2));
    }
    for (float x = std::fmod(w * 0.5f, spacing) - spacing; x < w + spacing; x += spacing) {
        draw_post(draw, ImVec2(x, ground + px(10)), post_width, post_height);
    }

    // Musical notes rising slowly, as in the game's own name.
    constexpr const char* NOTES[] = {"\xE2\x99\xAA", "\xE2\x99\xAB"};  // ♪ ♫
    ImFont* font = g_fonts.heading;
    for (int i = 0; i < 7; ++i) {
        const float phase = static_cast<float>(i) * 0.37f;
        const float travel = std::fmod(time * 0.045f + phase, 1.f);
        const float x = w * (0.06f + 0.14f * static_cast<float>(i)) + std::sin(time * 1.3f + phase * 9.f) * px(18);
        const float y = horizon + px(40) - travel * h * 0.62f;
        const float alpha = std::sin(travel * PI);
        draw->AddText(font, font->FontSize * (0.8f + 0.12f * static_cast<float>(i % 3)), ImVec2(x, y),
                      IM_COL32(255, 252, 230, static_cast<int>(200 * alpha)), NOTES[i % 2]);
    }
}

void draw_title_sign(ImDrawList* draw, ImVec2 center, bool small, const char* ribbon) {
    const float k = small ? 0.52f : 1.f;
    const ImVec2 size(px(600) * k, px(178) * k);
    const ImVec2 min = center - size * 0.5f;
    const ImVec2 max = center + size * 0.5f;
    const float rounding = px(42) * k;

    if (!small) {
        // Two posts that hold the sign.
        for (const float share : {0.24f, 0.76f}) {
            const float x = min.x + size.x * share;
            draw->AddRectFilled(ImVec2(x - px(13), max.y - px(20)), ImVec2(x + px(13), max.y + px(46)), WOOD_DARK);
            draw->AddRect(ImVec2(x - px(13), max.y - px(20)), ImVec2(x + px(13), max.y + px(46)), WOOD_EDGE, 0, 0,
                          px(3));
        }
    }
    draw->AddRectFilled(min + ImVec2(0, px(8) * k), max + ImVec2(0, px(8) * k), IM_COL32(60, 30, 10, 90), rounding);
    gradient_rect(draw, min, max, WOOD_LIGHT, WOOD_DARK, rounding);
    for (int line = 1; line < 5; ++line) {
        const float y = min.y + size.y * static_cast<float>(line) / 5.f;
        draw->AddLine(ImVec2(min.x + rounding * 0.7f, y), ImVec2(max.x - rounding * 0.7f, y), IM_COL32(110, 64, 28, 38),
                      px(2) * k);
    }
    const ImVec2 inset(px(9) * k, px(9) * k);
    draw->AddRect(min + inset, max - inset, WOOD_INNER, rounding - inset.x, 0, px(2.5f) * k);
    draw->AddRect(min, max, WOOD_EDGE, rounding, 0, px(5) * k);

    const char* title = "Harvest Moon";
    const char* subtitle = "Magical Melody";
    const float title_size = g_fonts.title->FontSize * k;
    const float subtitle_size = g_fonts.subtitle->FontSize * k;
    const ImVec2 title_extent = text_size(g_fonts.title, title_size, title);
    const ImVec2 subtitle_extent = text_size(g_fonts.subtitle, subtitle_size, subtitle);
    outlined_text(draw, g_fonts.title, title_size, ImVec2(center.x - title_extent.x * 0.5f, min.y + size.y * 0.1f), WHITE,
                  WOOD_EDGE, px(4.5f) * k, title);
    outlined_text(draw, g_fonts.subtitle, subtitle_size,
                  ImVec2(center.x - subtitle_extent.x * 0.5f, min.y + size.y * 0.56f), TEAL, WHITE, px(3.5f) * k,
                  subtitle);

    if (!small && ribbon != nullptr) {
        // A cream ribbon under the sign, with what this is.
        ImFont* font = g_fonts.body;
        const ImVec2 extent = text_size(font, font->FontSize, ribbon);
        const ImVec2 ribbon_min(center.x - extent.x * 0.5f - px(22), max.y + px(12));
        const ImVec2 ribbon_max(center.x + extent.x * 0.5f + px(22), max.y + px(12) + extent.y + px(12));
        draw->AddRectFilled(ribbon_min + ImVec2(0, px(4)), ribbon_max + ImVec2(0, px(4)), IM_COL32(60, 30, 10, 60),
                            px(12));
        draw->AddRectFilled(ribbon_min, ribbon_max, RIBBON, px(12));
        draw->AddRect(ribbon_min, ribbon_max, RIBBON_EDGE, px(12), 0, px(2));
        draw->AddText(font, font->FontSize, ribbon_min + ImVec2(px(22), px(6)), WOOD_EDGE, ribbon);
    }
}

bool menu_button(const char* label, ImVec2 size, ButtonColor color_name, bool enabled) {
    ImGui::PushID(label);
    ImGui::BeginDisabled(!enabled);
    const bool pressed = ImGui::InvisibleButton("##button", size, ImGuiButtonFlags_EnableNav);
    const bool hovered = ImGui::IsItemHovered();
    const bool held = ImGui::IsItemActive();
    const bool focused = nav_focused();
    ImGui::EndDisabled();
    ImGui::PopID();

    ImU32 top = ORANGE_TOP;
    ImU32 bottom = ORANGE_BOTTOM;
    ImU32 edge = ORANGE_EDGE;
    if (color_name == ButtonColor::Green) {
        top = GREEN_TOP;
        bottom = GREEN_BOTTOM;
        edge = GREEN_EDGE;
    }
    if (color_name == ButtonColor::Gray || !enabled) {
        top = GRAY_TOP;
        bottom = GRAY_BOTTOM;
        edge = GRAY_EDGE;
    }
    if ((hovered || focused) && enabled && !held) {
        top = ImAlphaBlendColors(top, IM_COL32(255, 255, 255, 60));
        bottom = ImAlphaBlendColors(bottom, IM_COL32(255, 255, 255, 40));
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float rounding = size.y * 0.5f;
    const float depth = px(5);
    const ImVec2 shift(0, held && enabled ? depth * 0.7f : ((hovered || focused) && enabled ? -px(2) : 0.f));
    if (focused) {
        focus_ring(draw, min + ImVec2(0, std::min(0.f, shift.y)), max + ImVec2(0, depth), rounding);
    }
    draw->AddRectFilled(min + ImVec2(0, depth), max + ImVec2(0, depth), edge, rounding);
    gradient_rect(draw, min + shift, max + shift, top, bottom, rounding);
    draw->AddRectFilled(min + shift + ImVec2(rounding * 0.6f, px(4)),
                        ImVec2(max.x - rounding * 0.6f, min.y + shift.y + size.y * 0.42f), IM_COL32(255, 255, 255, 60),
                        rounding);
    draw->AddRect(min + shift, max + shift, edge, rounding, 0, px(2.5f));

    ImFont* font = g_fonts.button;
    const ImVec2 extent = text_size(font, font->FontSize, label);
    outlined_text(draw, font, font->FontSize, (min + max) * 0.5f + shift - extent * 0.5f - ImVec2(0, px(1)), WHITE,
                  edge, px(2.5f), label);
    return pressed && enabled;
}

void begin_panel(const char* id, ImVec2 min, ImVec2 max, const char* title) {
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float rounding = px(28);
    const float border = px(5);
    draw->AddRectFilled(min + ImVec2(px(3), px(9)), max + ImVec2(px(3), px(9)), PANEL_SHADOW, rounding);
    draw->AddRectFilled(min, max, WHITE, rounding);
    gradient_rect(draw, min + ImVec2(border, border), max - ImVec2(border, border), PANEL_TOP, PANEL_BOTTOM,
                  rounding - border);
    float top = px(24);
    if (title != nullptr) {
        ImFont* font = g_fonts.heading;
        draw->AddText(font, font->FontSize, min + ImVec2(px(38), px(20)), NAME_RED, title);
        top = px(20) + font->FontSize + px(12);
        draw->AddLine(ImVec2(min.x + px(30), min.y + top), ImVec2(max.x - px(30), min.y + top), LINE, px(2));
        top += px(16);
    }
    const ImVec2 content_min = min + ImVec2(px(38), top);
    const ImVec2 content_max = max - ImVec2(px(30), px(24));
    ImGui::SetCursorScreenPos(content_min);
    ImGui::BeginChild(id, content_max - content_min, ImGuiChildFlags_NavFlattened, ImGuiWindowFlags_NoBackground);
}

void end_panel() { ImGui::EndChild(); }

bool toggle(const char* id, bool* value) {
    const ImVec2 size(px(70), px(38));
    ImGui::PushID(id);
    bool changed = ImGui::InvisibleButton("##toggle", size, ImGuiButtonFlags_EnableNav);
    const ImGuiID item = ImGui::GetItemID();
    if (ImGui::IsItemFocused()) {
        const int delta = left_right(item);
        changed |= (delta < 0 && *value) || (delta > 0 && !*value);
    }
    const bool focused = nav_focused();
    const bool hovered = ImGui::IsItemHovered();
    ImGui::PopID();
    if (changed) {
        *value = !*value;
    }

    // The knob slides to its side.
    float* position = ImGui::GetStateStorage()->GetFloatRef(item, *value ? 1.f : 0.f);
    const float target = *value ? 1.f : 0.f;
    *position += (target - *position) * std::min(1.f, ImGui::GetIO().DeltaTime * 14.f);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float radius = size.y * 0.5f;
    if (focused) {
        focus_ring(draw, min, max, radius);
    }
    const ImU32 track = ImGui::ColorConvertFloat4ToU32(
        ImLerp(ImGui::ColorConvertU32ToFloat4(SWITCH_OFF), ImGui::ColorConvertU32ToFloat4(GREEN), *position));
    draw->AddRectFilled(min, max, track, radius);
    draw->AddRect(min, max, IM_COL32(0, 0, 0, hovered ? 50 : 30), radius, 0, px(2));
    const ImVec2 knob(min.x + radius + *position * (size.x - size.y), min.y + radius);
    draw->AddCircleFilled(knob + ImVec2(0, px(2)), radius - px(4), IM_COL32(0, 0, 0, 50), 32);
    draw->AddCircleFilled(knob, radius - px(4), WHITE, 32);
    return changed;
}

bool choice(const char* id, int* index, const std::vector<std::string>& labels, float width) {
    const int count = static_cast<int>(labels.size());
    if (count == 0) {
        return false;
    }
    const ImVec2 size(width, px(46));
    ImGui::PushID(id);
    const bool pressed = ImGui::InvisibleButton("##choice", size, ImGuiButtonFlags_EnableNav);
    const ImGuiID item = ImGui::GetItemID();
    const bool hovered = ImGui::IsItemHovered();
    int delta = ImGui::IsItemFocused() ? left_right(item) : 0;
    const bool focused = nav_focused();
    ImGui::PopID();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    if (pressed) {
        // A click on the left arrow goes back; anywhere else, and the controller's button, forward.
        const bool on_left = hovered && ImGui::GetIO().MousePos.x < min.x + size.x * 0.3f;
        delta += on_left ? -1 : 1;
    }
    const int previous = *index;
    if (delta != 0) {
        *index = ((*index + delta) % count + count) % count;
    }

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float rounding = size.y * 0.5f;
    if (focused) {
        focus_ring(draw, min, max, rounding);
    }
    draw->AddRectFilled(min, max, IM_COL32(255, 255, 255, 235), rounding);
    draw->AddRect(min, max, hovered || focused ? ORANGE : LINE, rounding, 0, px(2));
    ImFont* font = g_fonts.body;
    const char* left_arrow = "\xE2\x97\x80";   // ◀
    const char* right_arrow = "\xE2\x96\xB6";  // ▶
    const float arrow_size = font->FontSize * 0.8f;
    const float middle = min.y + (size.y - arrow_size) * 0.5f;
    draw->AddText(font, arrow_size, ImVec2(min.x + px(16), middle), ORANGE, left_arrow);
    draw->AddText(font, arrow_size, ImVec2(max.x - px(16) - text_size(font, arrow_size, right_arrow).x, middle),
                  ORANGE, right_arrow);
    const char* label = labels[static_cast<size_t>(std::clamp(*index, 0, count - 1))].c_str();
    const ImVec2 extent = text_size(font, font->FontSize, label);
    draw->PushClipRect(min + ImVec2(px(40), 0), max - ImVec2(px(40), 0), true);
    draw->AddText(font, font->FontSize, ImVec2((min.x + max.x - extent.x) * 0.5f, min.y + (size.y - extent.y) * 0.5f),
                  TEXT, label);
    draw->PopClipRect();
    return *index != previous;
}

bool percent(const char* id, int* value, int minimum, int maximum, int step, float width) {
    const float value_width = px(84);
    const ImVec2 size(width, px(44));
    ImGui::PushID(id);
    ImGui::InvisibleButton("##percent", size, ImGuiButtonFlags_EnableNav);
    const ImGuiID item = ImGui::GetItemID();
    const bool active = ImGui::IsItemActive();
    const bool hovered = ImGui::IsItemHovered();
    const int delta = ImGui::IsItemFocused() ? left_right(item) : 0;
    const bool focused = nav_focused();
    ImGui::PopID();

    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float knob_radius = px(13);
    const float track_left = min.x + knob_radius;
    const float track_right = max.x - value_width - knob_radius;
    const int previous = *value;
    if (active && ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
        const float share = std::clamp((ImGui::GetIO().MousePos.x - track_left) / (track_right - track_left), 0.f, 1.f);
        const float raw = static_cast<float>(minimum) + share * static_cast<float>(maximum - minimum);
        *value = minimum + static_cast<int>(std::lround((raw - static_cast<float>(minimum)) / static_cast<float>(step))) * step;
    }
    *value = std::clamp(*value + delta * step, minimum, maximum);

    ImDrawList* draw = ImGui::GetWindowDrawList();
    const float share = static_cast<float>(*value - minimum) / static_cast<float>(maximum - minimum);
    const float middle = (min.y + max.y) * 0.5f;
    const float knob_x = track_left + share * (track_right - track_left);
    if (focused) {
        focus_ring(draw, min, ImVec2(track_right + knob_radius, max.y), px(22));
    }
    const ImVec2 track_min(track_left - px(4), middle - px(7));
    const ImVec2 track_max(track_right + px(4), middle + px(7));
    draw->AddRectFilled(track_min, track_max, TRACK, px(7));
    gradient_rect(draw, track_min, ImVec2(knob_x, track_max.y), ORANGE_TOP, ORANGE_BOTTOM, px(7));
    draw->AddCircleFilled(ImVec2(knob_x, middle + px(2)), knob_radius, IM_COL32(0, 0, 0, 50), 32);
    draw->AddCircleFilled(ImVec2(knob_x, middle), knob_radius, WHITE, 32);
    draw->AddCircle(ImVec2(knob_x, middle), knob_radius - px(1.5f), hovered || active || focused ? ORANGE_EDGE : ORANGE,
                    32, px(3));
    char text[16];
    std::snprintf(text, sizeof(text), "%d%%", *value);
    ImFont* font = g_fonts.heading;
    const float text_height = font->FontSize * 0.8f;
    const ImVec2 extent = text_size(font, text_height, text);
    draw->AddText(font, text_height, ImVec2(max.x - extent.x, middle - extent.y * 0.5f), TEXT, text);
    return *value != previous;
}

void row_label(const char* label, const char* hint) {
    ImGui::PushFont(g_fonts.body);
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextUnformatted(label);
    ImGui::PopTextWrapPos();
    ImGui::PopFont();
    if (hint != nullptr) {
        ImGui::PushFont(g_fonts.small);
        ImGui::PushStyleColor(ImGuiCol_Text, TEXT_SOFT);
        ImGui::PushTextWrapPos(0.f);
        ImGui::TextUnformatted(hint);
        ImGui::PopTextWrapPos();
        ImGui::PopStyleColor();
        ImGui::PopFont();
    }
}

float row_control_width() { return ImGui::GetContentRegionAvail().x; }

void progress_bar(float fraction, ImVec2 size, double seconds) {
    ImGui::Dummy(size);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    const ImVec2 max = ImGui::GetItemRectMax();
    const float rounding = size.y * 0.5f;
    draw->AddRectFilled(min, max, TRACK, rounding);
    const float filled = std::clamp(fraction, 0.f, 1.f) * size.x;
    if (filled > 1.f) {
        const ImVec2 fill_max(min.x + std::max(filled, size.y), max.y);
        gradient_rect(draw, min, fill_max, GREEN_TOP, GREEN_BOTTOM, rounding);
        // Stripes that move, so that the bar looks alive while a long step runs.
        draw->PushClipRect(min, fill_max, true);
        const float stripe = px(26);
        const float offset = std::fmod(static_cast<float>(seconds) * px(40), stripe * 2.f);
        for (float x = min.x - size.y - stripe * 2.f + offset; x < fill_max.x; x += stripe * 2.f) {
            draw->AddQuadFilled(ImVec2(x, max.y), ImVec2(x + stripe, max.y), ImVec2(x + stripe + size.y, min.y),
                                ImVec2(x + size.y, min.y), IM_COL32(255, 255, 255, 40));
        }
        draw->PopClipRect();
    }
    draw->AddRect(min, max, IM_COL32(0, 0, 0, 40), rounding, 0, px(2));
}

void note(NoteKind kind, const std::string& text) {
    ImU32 fill = IM_COL32(225, 238, 252, 255);
    ImU32 edge = IM_COL32(120, 168, 222, 255);
    const char* mark = "\xE2\x99\xAA";  // ♪
    switch (kind) {
    case NoteKind::Info: break;
    case NoteKind::Warning:
        fill = IM_COL32(255, 244, 205, 255);
        edge = IM_COL32(228, 178, 60, 255);
        mark = "!";
        break;
    case NoteKind::Error:
        fill = IM_COL32(253, 226, 222, 255);
        edge = IM_COL32(214, 84, 74, 255);
        mark = "\xE2\x9C\x95";  // ✕
        break;
    case NoteKind::Success:
        fill = IM_COL32(222, 246, 214, 255);
        edge = IM_COL32(88, 176, 72, 255);
        mark = "\xE2\x9C\x93";  // ✓
        break;
    }
    const float width = ImGui::GetContentRegionAvail().x;
    const float padding = px(14);
    const float mark_width = px(34);
    ImFont* font = g_fonts.body;
    const ImVec2 extent = font->CalcTextSizeA(font->FontSize, FLT_MAX, width - mark_width - padding * 2.f, text.c_str());
    const ImVec2 size(width, extent.y + padding * 2.f);
    ImGui::Dummy(size);
    ImDrawList* draw = ImGui::GetWindowDrawList();
    const ImVec2 min = ImGui::GetItemRectMin();
    draw->AddRectFilled(min, min + size, fill, px(14));
    draw->AddRect(min, min + size, edge, px(14), 0, px(2));
    draw->AddText(g_fonts.heading, g_fonts.heading->FontSize * 0.8f, min + ImVec2(padding, padding - px(3)), edge,
                  mark);
    draw->AddText(font, font->FontSize, min + ImVec2(padding + mark_width, padding), TEXT, text.c_str(), nullptr,
                  width - mark_width - padding * 2.f);
}

void outlined_text(ImDrawList* draw, ImFont* font, float size, ImVec2 position, ImU32 fill, ImU32 outline,
                   float thickness, const char* text) {
    const int steps = thickness > 2.5f ? 16 : 8;
    for (int step = 0; step < steps; ++step) {
        const float angle = 2.f * PI * static_cast<float>(step) / static_cast<float>(steps);
        draw->AddText(font, size, position + ImVec2(std::cos(angle), std::sin(angle)) * thickness, outline, text);
    }
    draw->AddText(font, size, position, fill, text);
}

void paragraph(const std::string& text, ImU32 color) {
    ImGui::PushFont(g_fonts.body);
    ImGui::PushStyleColor(ImGuiCol_Text, color != 0 ? color : TEXT);
    ImGui::PushTextWrapPos(0.f);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopTextWrapPos();
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

void heading(const std::string& text) {
    ImGui::PushFont(g_fonts.heading);
    ImGui::PushStyleColor(ImGuiCol_Text, SECTION);
    ImGui::TextUnformatted(text.c_str());
    ImGui::PopStyleColor();
    ImGui::PopFont();
}

}  // namespace theme
