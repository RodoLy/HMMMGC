// See touch_buttons.h.

#include "input/touch_buttons.h"

#include "in_game_menu.h"
#include "player_options.h"
#include "texts.h"
#include "theme.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <map>
#include <vector>

namespace touch_buttons {
namespace {

// Everything is measured in a unit of a ninth of the screen's short side, so the buttons are the
// same size for the thumbs on any screen, and they keep to the screen's edges and corners.
constexpr float UNITS_ON_THE_SHORT_SIDE = 9.f;
// The stick: its ring, its knob, and how far from its middle a finger still takes it.
constexpr float STICK_RING = 1.6f;
constexpr float STICK_KNOB = 0.7f;
constexpr float STICK_REACH = 2.5f;
// How far the GameCube's stick goes (its octagon's corners are a little past this).
constexpr float STICK_FULL = 100.f;
// How much of the buttons covers the game, at rest and pressed: little, so that the game shows
// through them, also where a wide screen puts the game under them.
constexpr float OPACITY = 0.42f;
constexpr float OPACITY_PRESSED = 0.8f;
// The letters, the arrows and the edges are drawn stronger than the faces, to be read over the game.
constexpr float STRONGER = 2.f;
// A controller's stick moved this far hides the buttons (a stick at rest drifts a little).
constexpr Sint16 STICK_MOVED = 12000;

// How each button looks: drawn as the game draws the controller's buttons on its screen (its farm
// shows A, B and X at the top right: coins and a bean with a dark blue edge and the letter pressed
// in), in the colors of the GameCube's controller.
//   GreenCoin, RedCoin  A and B, round coins;
//   Bean                X and Y, pale beans curved around A;
//   GrayCapsule         L, R and START, pale capsules (a rectangle with round ends);
//   PurpleCapsule       Z;
//   CreamCapsule        the menu's and the turbo's, which the GameCube does not have, in the cream of
//                       the game's boxes;
//   Pad                 an arm of the D-Pad, a pale cross drawn once for the four.
enum class Look { GreenCoin, RedCoin, Bean, GrayCapsule, PurpleCapsule, CreamCapsule, Pad };

// The D-Pad's arrows are drawn, not written: the game's lettering has no arrows.
enum class Arrow { None, Up, Down, Left, Right };

// What a button does: press the controller's buttons, open the options menu (as F10 does), or
// run the game faster while it is held (as the turbo's key, Tab, does).
enum class Job { Pad, Menu, Turbo };

struct Button {
    const char* label;  // nullptr: from the texts (the menu's)
    Job job;
    u16 pad_bits;  // the controller's buttons it presses
    Look look;
    // Where it is: its middle as units from a corner (negative: from the right or the bottom edge,
    // or, with `centered`, from the middle), and its size in units.
    float x, y, width, height;
    bool centered = false;
    Arrow arrow = Arrow::None;
};

const std::array<Button, 14> BUTTONS = {{
    {"A", Job::Pad, PAD_BUTTON_A, Look::GreenCoin, -2.3f, -2.0f, 1.7f, 1.7f},
    {"B", Job::Pad, PAD_BUTTON_B, Look::RedCoin, -4.15f, -1.15f, 1.1f, 1.1f},
    {"X", Job::Pad, PAD_BUTTON_X, Look::Bean, -0.85f, -3.35f, 1.15f, 1.15f},
    {"Y", Job::Pad, PAD_BUTTON_Y, Look::Bean, -3.0f, -3.95f, 1.15f, 1.15f},
    {"L", Job::Pad, PAD_TRIGGER_L, Look::GrayCapsule, 1.4f, 0.75f, 2.2f, 0.8f},
    {"R", Job::Pad, PAD_TRIGGER_R, Look::GrayCapsule, -1.4f, 0.75f, 2.2f, 0.8f},
    {"Z", Job::Pad, PAD_TRIGGER_Z, Look::PurpleCapsule, -1.4f, 1.75f, 1.6f, 0.68f},
    {"START", Job::Pad, PAD_BUTTON_START, Look::GrayCapsule, 0.f, -0.7f, 2.0f, 0.7f, true},
    {nullptr, Job::Menu, 0, Look::CreamCapsule, 0.f, 0.65f, 2.2f, 0.7f, true},
    {"TURBO", Job::Turbo, 0, Look::CreamCapsule, 1.4f, 1.75f, 1.6f, 0.68f},  // under L, as Z is under R
    {"", Job::Pad, PAD_BUTTON_UP, Look::Pad, 2.4f, -6.15f, 0.75f, 0.75f, false, Arrow::Up},
    {"", Job::Pad, PAD_BUTTON_DOWN, Look::Pad, 2.4f, -4.75f, 0.75f, 0.75f, false, Arrow::Down},
    {"", Job::Pad, PAD_BUTTON_LEFT, Look::Pad, 1.7f, -5.45f, 0.75f, 0.75f, false, Arrow::Left},
    {"", Job::Pad, PAD_BUTTON_RIGHT, Look::Pad, 3.1f, -5.45f, 0.75f, 0.75f, false, Arrow::Right},
}};
// The middle of the D-Pad's cross (as units from the bottom left corner), how far its arms reach
// from it and how wide they are.
constexpr float PAD_X = 2.4f, PAD_Y = -5.45f;
constexpr float PAD_REACH = 1.08f, PAD_ARM = 0.72f;
// Whether a button is on the screen: all are, but TURBO when the player took it off in the options
// (settings.json's hideTouchTurbo).
bool shown(const Button& button) { return button.job != Job::Turbo || !player_options::hide_touch_turbo(); }

constexpr int STICK = -1;  // what a finger holds when it is on the stick
constexpr int NOTHING = -2;

struct Finger {
    int control = NOTHING;  // an index of BUTTONS, STICK or NOTHING
    ImVec2 position;
};

// A phone is played by touch until a controller says otherwise; a computer shows them once its
// screen is touched, or from the start with HMMM_TOUCH_BUTTONS=1 (to try them, or for a tablet).
#if defined(__ANDROID__)
bool g_visible = true;
#else
bool g_visible = std::getenv("HMMM_TOUCH_BUTTONS") != nullptr;
#endif
std::map<SDL_FingerID, Finger> g_fingers;
ImVec2 g_screen;  // the size of the screen in ImGui's units, as of the last frame

float unit() { return std::min(g_screen.x, g_screen.y) / UNITS_ON_THE_SHORT_SIDE; }

ImVec2 stick_center() { return ImVec2(2.4f * unit(), g_screen.y - 2.4f * unit()); }

// The rectangle of a button on this screen.
std::pair<ImVec2, ImVec2> rectangle(const Button& button) {
    const float u = unit();
    const float x = button.centered ? g_screen.x * 0.5f + button.x * u
                    : button.x < 0  ? g_screen.x + button.x * u
                                    : button.x * u;
    const float y = button.y < 0 ? g_screen.y + button.y * u : button.y * u;
    const ImVec2 half(button.width * u * 0.5f, button.height * u * 0.5f);
    return {ImVec2(x, y) - half, ImVec2(x, y) + half};
}

// What a finger at `point` holds: the stick when it is near enough, or the button under it (a
// little past its edge counts, as a thumb is not precise), or nothing.
int control_at(ImVec2 point) {
    const ImVec2 to_stick = point - stick_center();
    if (ImLengthSqr(to_stick) <= std::pow(STICK_REACH * unit(), 2.f)) {
        return STICK;
    }
    const float slack = 0.15f * unit();
    int closest = NOTHING;
    float closest_distance = 0.f;
    for (int index = 0; index < static_cast<int>(BUTTONS.size()); ++index) {
        if (!shown(BUTTONS[index])) {
            continue;
        }
        const auto [min, max] = rectangle(BUTTONS[index]);
        if (point.x < min.x - slack || point.x > max.x + slack || point.y < min.y - slack || point.y > max.y + slack) {
            continue;
        }
        const float distance = ImLengthSqr(point - (min + max) * 0.5f);
        if (closest == NOTHING || distance < closest_distance) {
            closest = index;
            closest_distance = distance;
        }
    }
    return closest;
}

bool held(int control) {
    return std::any_of(g_fingers.begin(), g_fingers.end(),
                       [control](const auto& entry) { return entry.second.control == control; });
}

// The stick's position, from -1 to 1 on each axis (y up), or nothing when no finger is on it.
ImVec2 stick_position() {
    for (const auto& [id, finger] : g_fingers) {
        if (finger.control == STICK) {
            ImVec2 offset = (finger.position - stick_center()) / (STICK_RING * unit());
            const float length = std::sqrt(ImLengthSqr(offset));
            if (length > 1.f) {
                offset = offset / length;
            }
            return ImVec2(offset.x, -offset.y);
        }
    }
    return ImVec2(0.f, 0.f);
}

void hide() {
    g_visible = false;
    g_fingers.clear();
}

// The player's choice in the options (settings.json's hideTouchButtons): none while a controller
// plays as controller 1, whatever touches the screen.
bool kept_away_by_controller() { return player_options::hide_touch_buttons() && PADGetIndexForPort(PAD_CHAN0) >= 0; }

// A phone that starts with a controller connected starts without the buttons, and one whose
// controller goes away (unplugged, out of battery) gets them back at once, so that the game can
// still be played.
void watch_controller() {
#if defined(__ANDROID__)
    static bool first_frame = true;
    static bool had_controller = false;
    const bool has_controller = PADGetIndexForPort(PAD_CHAN0) >= 0;
    if (first_frame && has_controller) {
        hide();
    } else if (had_controller && !has_controller) {
        g_visible = true;
    }
    first_frame = false;
    had_controller = has_controller;
#endif
}

void finger_down(SDL_FingerID id, ImVec2 point) {
    g_visible = true;
    const int control = control_at(point);
    if (control >= 0 && BUTTONS[control].job == Job::Menu) {
        in_game_menu::open_from_touch();  // the menu's button: the options, as F10 opens them
        return;
    }
    g_fingers[id] = Finger{control, point};
}

void finger_moved(SDL_FingerID id, ImVec2 point) {
    auto found = g_fingers.find(id);
    if (found == g_fingers.end()) {
        return;
    }
    found->second.position = point;
    // A finger keeps the stick while it is down; on the buttons it slides from one to the next.
    if (found->second.control != STICK) {
        const int control = control_at(point);
        found->second.control = control >= 0 && BUTTONS[control].job == Job::Menu ? NOTHING : control;
        if (found->second.control == STICK) {
            found->second.control = NOTHING;
        }
    }
}

// The colors of the buttons, each drawn at the button's opacity: the face of a button,
// its side (the thickness under the face), its letter, and a shine on the face.
struct Colors {
    ImU32 face, side, letter, shine;
};
constexpr Colors GREEN = {IM_COL32(72, 206, 108, 255), IM_COL32(28, 140, 72, 255), IM_COL32(30, 128, 66, 255),
                          IM_COL32(160, 238, 176, 255)};
constexpr Colors RED = {IM_COL32(246, 86, 52, 255), IM_COL32(172, 36, 26, 255), IM_COL32(136, 24, 22, 255),
                        IM_COL32(255, 168, 140, 255)};
constexpr Colors PALE = {IM_COL32(236, 240, 250, 255), IM_COL32(150, 164, 196, 255), IM_COL32(84, 100, 134, 255),
                         IM_COL32(255, 255, 255, 255)};
constexpr Colors PURPLE = {IM_COL32(124, 100, 206, 255), IM_COL32(74, 54, 146, 255), IM_COL32(58, 38, 120, 255),
                           IM_COL32(186, 168, 240, 255)};
constexpr Colors CREAM = {IM_COL32(250, 240, 212, 255), IM_COL32(204, 150, 86, 255), IM_COL32(204, 120, 48, 255),
                          IM_COL32(255, 255, 255, 255)};
constexpr ImU32 OUTLINE = IM_COL32(44, 44, 96, 255);  // the dark blue around the game's buttons

using Shape = std::vector<ImVec2>;

ImU32 faded(ImU32 color, float opacity) {
    const auto alpha = static_cast<ImU32>(((color >> IM_COL32_A_SHIFT) & 0xFF) * std::min(opacity, 1.f));
    return (color & ~IM_COL32_A_MASK) | (alpha << IM_COL32_A_SHIFT);
}

ImVec2 direction(float angle) { return ImVec2(std::cos(angle), std::sin(angle)); }

Shape moved(const Shape& shape, ImVec2 offset) {
    Shape result = shape;
    for (ImVec2& point : result) {
        point = point + offset;
    }
    return result;
}

Shape ellipse(ImVec2 center, ImVec2 radius) {
    constexpr int POINTS = 40;
    Shape shape;
    for (int i = 0; i < POINTS; ++i) {
        const ImVec2 way = direction(2.f * IM_PI * static_cast<float>(i) / POINTS);
        shape.push_back(center + ImVec2(way.x * radius.x, way.y * radius.y));
    }
    return shape;
}

// A rectangle with round ends, as the game's date and time are written in.
Shape capsule(ImVec2 min, ImVec2 max) {
    constexpr int POINTS = 14;  // on each end
    const float radius = (max.y - min.y) * 0.5f;
    const float middle = (min.y + max.y) * 0.5f;
    Shape shape;
    for (int i = 0; i <= POINTS; ++i) {  // the right end, from the top down
        shape.push_back(ImVec2(max.x - radius, middle) + direction(-IM_PI * 0.5f + IM_PI * i / POINTS) * radius);
    }
    for (int i = 0; i <= POINTS; ++i) {  // the left end, from the bottom up
        shape.push_back(ImVec2(min.x + radius, middle) + direction(IM_PI * 0.5f + IM_PI * i / POINTS) * radius);
    }
    return shape;
}

// A bean: a thick arc with round ends, around `center` at `radius`, from the angle `from` to `to`.
Shape bean(ImVec2 center, float radius, float from, float to, float thickness) {
    constexpr int POINTS = 16;
    const float half = thickness * 0.5f;
    Shape shape;
    for (int i = 0; i <= POINTS; ++i) {  // the outer side
        shape.push_back(center + direction(from + (to - from) * i / POINTS) * (radius + half));
    }
    const ImVec2 end = center + direction(to) * radius;
    for (int i = 1; i < POINTS; ++i) {  // round, from outside to inside
        const float turn = IM_PI * i / POINTS;
        shape.push_back(end + (direction(to) * std::cos(turn) + direction(to + IM_PI * 0.5f) * std::sin(turn)) * half);
    }
    for (int i = 0; i <= POINTS; ++i) {  // the inner side, back
        shape.push_back(center + direction(to + (from - to) * i / POINTS) * (radius - half));
    }
    const ImVec2 start = center + direction(from) * radius;
    for (int i = 1; i < POINTS; ++i) {  // round, from inside to outside
        const float turn = IM_PI * i / POINTS;
        shape.push_back(start -
                        (direction(from) * std::cos(turn) + direction(from + IM_PI * 0.5f) * std::sin(turn)) * half);
    }
    return shape;
}

// The D-Pad's cross, its arms `reach` long from the middle and `arm` wide.
Shape cross(ImVec2 center, float reach, float arm) {
    const float w = arm * 0.5f;
    const float r = reach;
    const ImVec2 corners[] = {{-w, -r}, {w, -r}, {w, -w}, {r, -w}, {r, w},   {w, w},
                              {w, r},   {-w, r}, {-w, w}, {-r, w}, {-r, -w}, {-w, -w}};
    Shape shape;
    for (const ImVec2& corner : corners) {
        shape.push_back(center + corner);
    }
    return shape;
}

// A piece of the game's screen standing a little out of it: its side `depth` below, the face on
// it (lower when pressed, as if pushed in), a dark blue edge around both, and a shine at the top
// of the face. Returns how far the face went down.
ImVec2 draw_raised(ImDrawList* draw, const Shape& face, const Colors& colors, float depth, bool pressed,
                   float opacity, float edge) {
    const ImVec2 sink(0, pressed ? depth * 0.7f : 0.f);
    const Shape side = moved(face, ImVec2(0, depth));
    const Shape top = moved(face, sink);
    draw->AddConcavePolyFilled(side.data(), static_cast<int>(side.size()), faded(colors.side, opacity));
    draw->AddPolyline(side.data(), static_cast<int>(side.size()), faded(OUTLINE, opacity * STRONGER),
                      ImDrawFlags_Closed, edge);
    draw->AddConcavePolyFilled(top.data(), static_cast<int>(top.size()), faded(colors.face, opacity));
    draw->AddPolyline(top.data(), static_cast<int>(top.size()), faded(OUTLINE, opacity * STRONGER),
                      ImDrawFlags_Closed, edge);
    return sink;
}

// The letter of a button in the middle of `at`, pressed into it: in a darker color, with a light
// line under it.
void draw_letter(ImDrawList* draw, const char* label, ImVec2 at, float size, const Colors& colors, float opacity) {
    opacity *= STRONGER;
    ImFont* font = theme::fonts().button;
    if (label == nullptr || *label == '\0' || font == nullptr) {
        return;
    }
    const ImVec2 extent = font->CalcTextSizeA(size, FLT_MAX, 0.f, label);
    const ImVec2 position = at - extent * 0.5f;
    draw->AddText(font, size, position + ImVec2(0, std::max(1.f, size * 0.05f)), faded(colors.shine, opacity), label);
    draw->AddText(font, size, position, faded(colors.letter, opacity), label);
}

// A button but the D-Pad's, with the look of its kind.
void draw_button(ImDrawList* draw, const Button& button, bool pressed) {
    const float u = unit();
    const float opacity = pressed ? OPACITY_PRESSED : OPACITY;
    const float edge = std::max(1.5f, 0.045f * u);
    const auto [min, max] = rectangle(button);
    const ImVec2 middle = (min + max) * 0.5f;
    const float height = max.y - min.y;
    const char* label = button.label != nullptr ? button.label : texts::text(texts::Text::TouchOptions);
    switch (button.look) {
    case Look::GreenCoin:
    case Look::RedCoin: {
        const Colors& colors = button.look == Look::GreenCoin ? GREEN : RED;
        const ImVec2 radius(height * 0.5f, height * 0.46f);
        const ImVec2 sink = draw_raised(draw, ellipse(middle, radius), colors, height * 0.1f, pressed, opacity, edge);
        // The shine: a light band along the top of the face.
        const Shape shine = ellipse(middle + sink - ImVec2(0, radius.y * 0.42f), radius * ImVec2(0.62f, 0.34f));
        draw->AddConcavePolyFilled(shine.data(), static_cast<int>(shine.size()), faded(colors.shine, opacity * 0.45f));
        draw_letter(draw, label, middle + sink, height * 0.52f, colors, opacity);
        break;
    }
    case Look::Bean: {
        // Curved around A, as far from A's middle as the button's middle is.
        const auto [a_min, a_max] = rectangle(BUTTONS[0]);
        const ImVec2 a_middle = (a_min + a_max) * 0.5f;
        const ImVec2 away = middle - a_middle;
        const float radius = std::sqrt(ImLengthSqr(away));
        const float angle = std::atan2(away.y, away.x);
        const float span = height * 0.5f / radius;
        const Shape shape = bean(a_middle, radius, angle - span, angle + span, height * 0.6f);
        const ImVec2 sink = draw_raised(draw, shape, PALE, height * 0.09f, pressed, opacity, edge);
        draw_letter(draw, label, middle + sink, height * 0.4f, PALE, opacity);
        break;
    }
    case Look::GrayCapsule:
    case Look::PurpleCapsule:
    case Look::CreamCapsule:
    case Look::Pad: {
        const Colors& colors = button.look == Look::PurpleCapsule ? PURPLE
                               : button.look == Look::CreamCapsule ? CREAM
                                                                   : PALE;
        const ImVec2 sink = draw_raised(draw, capsule(min, max), colors, height * 0.12f, pressed, opacity, edge);
        // A lighter band along the top, as on the coins.
        const Shape shine = capsule(min + sink + ImVec2(height * 0.2f, height * 0.1f),
                                    ImVec2(max.x - height * 0.2f, middle.y) + sink);
        draw->AddConcavePolyFilled(shine.data(), static_cast<int>(shine.size()), faded(colors.shine, opacity * 0.5f));
        draw_letter(draw, label, middle + sink, height * 0.5f, colors, opacity);
        break;
    }
    }
}

// The D-Pad: one pale cross, an arrow on each arm, and the pressed arms darker.
void draw_pad(ImDrawList* draw) {
    const float u = unit();
    const ImVec2 center(PAD_X * u, g_screen.y + PAD_Y * u);
    bool any_pressed = false;
    for (int index = 0; index < static_cast<int>(BUTTONS.size()); ++index) {
        any_pressed |= BUTTONS[index].look == Look::Pad && held(index);
    }
    const float opacity = any_pressed ? OPACITY_PRESSED : OPACITY;
    draw_raised(draw, cross(center, PAD_REACH * u, PAD_ARM * u), PALE, 0.1f * u, false, opacity,
                std::max(1.5f, 0.045f * u));
    for (int index = 0; index < static_cast<int>(BUTTONS.size()); ++index) {
        const Button& button = BUTTONS[index];
        if (button.look != Look::Pad) {
            continue;
        }
        const bool pressed = held(index);
        ImVec2 way(0, -1);  // up
        if (button.arrow == Arrow::Down) {
            way = ImVec2(0, 1);
        } else if (button.arrow == Arrow::Left) {
            way = ImVec2(-1, 0);
        } else if (button.arrow == Arrow::Right) {
            way = ImVec2(1, 0);
        }
        const ImVec2 across(-way.y, way.x);
        const float w = PAD_ARM * u * 0.5f;
        if (pressed) {  // the arm pushed in: its color goes to the side's
            const ImVec2 near = center + way * w;
            const ImVec2 far = center + way * (PAD_REACH * u);
            draw->AddQuadFilled(near - across * w, near + across * w, far + across * w, far - across * w,
                                faded(PALE.side, opacity * 0.7f));
        }
        const ImVec2 arrow_middle = center + way * (PAD_REACH * u * 0.62f);
        const float size = PAD_ARM * u * 0.3f;
        draw->AddTriangleFilled(arrow_middle + way * size, arrow_middle - way * size * 0.6f + across * size * 0.9f,
                                arrow_middle - way * size * 0.6f - across * size * 0.9f,
                                faded(pressed ? PALE.face : PALE.letter, opacity * STRONGER));
    }
}

// The Control Stick: the GameCube's octagon, pale with a gray edge, and a pale knob in it that
// follows the finger.
void draw_stick(ImDrawList* draw) {
    const float u = unit();
    const ImVec2 center = stick_center();
    const float ring = STICK_RING * u;
    const bool pressed = held(STICK);
    const float opacity = pressed ? OPACITY_PRESSED : OPACITY;
    Shape octagon;
    for (int i = 0; i < 8; ++i) {
        octagon.push_back(center + direction(IM_PI * (0.125f + 0.25f * i)) * ring);
    }
    draw->AddConcavePolyFilled(octagon.data(), 8, faded(PALE.face, opacity * 0.5f));
    draw->AddPolyline(octagon.data(), 8, faded(PALE.side, opacity * STRONGER), ImDrawFlags_Closed,
                      std::max(2.f, 0.07f * u));
    const ImVec2 position = stick_position();
    const ImVec2 knob_center = center + ImVec2(position.x, -position.y) * ring;
    const float knob = STICK_KNOB * u;
    const ImVec2 sink = draw_raised(draw, ellipse(knob_center, ImVec2(knob, knob * 0.92f)), PALE, knob * 0.2f,
                                    pressed, opacity, std::max(1.5f, 0.045f * u));
    const Shape shine = ellipse(knob_center + sink - ImVec2(0, knob * 0.4f), ImVec2(knob * 0.6f, knob * 0.32f));
    draw->AddConcavePolyFilled(shine.data(), static_cast<int>(shine.size()), faded(PALE.shine, opacity * 0.6f));
}

}  // namespace

void handle_event(const SDL_Event& event) {
    switch (event.type) {
    case SDL_EVENT_FINGER_DOWN:
    case SDL_EVENT_FINGER_MOTION:
    case SDL_EVENT_FINGER_UP:
    case SDL_EVENT_FINGER_CANCELED: {
        if (in_game_menu::is_open() || g_screen.x <= 0.f || kept_away_by_controller()) {
            g_fingers.clear();
            return;
        }
        const ImVec2 point(event.tfinger.x * g_screen.x, event.tfinger.y * g_screen.y);
        if (event.type == SDL_EVENT_FINGER_DOWN) {
            finger_down(event.tfinger.fingerID, point);
        } else if (event.type == SDL_EVENT_FINGER_MOTION) {
            finger_moved(event.tfinger.fingerID, point);
        } else {
            g_fingers.erase(event.tfinger.fingerID);
        }
        break;
    }
    // A controller or the keyboard in use: the buttons step aside.
    case SDL_EVENT_GAMEPAD_BUTTON_DOWN:
    case SDL_EVENT_KEY_DOWN:
        hide();
        break;
    case SDL_EVENT_GAMEPAD_AXIS_MOTION:
        if (std::abs(event.gaxis.value) > STICK_MOVED) {
            hide();
        }
        break;
    default:
        break;
    }
}

bool visible() { return g_visible && !in_game_menu::is_open() && !kept_away_by_controller(); }

void draw() {
    g_screen = ImGui::GetIO().DisplaySize;
    watch_controller();
    // The lettering is made in in_game_menu::before_frame, after the first frame.
    if (!visible() || g_screen.x <= 0.f || theme::fonts().button == nullptr) {
        return;
    }
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    draw_stick(draw);
    draw_pad(draw);
    for (int index = 0; index < static_cast<int>(BUTTONS.size()); ++index) {
        if (BUTTONS[index].look != Look::Pad && shown(BUTTONS[index])) {
            draw_button(draw, BUTTONS[index], held(index));
        }
    }
}

bool turbo_held() {
    if (!visible()) {
        return false;
    }
    for (int index = 0; index < static_cast<int>(BUTTONS.size()); ++index) {
        if (BUTTONS[index].job == Job::Turbo && shown(BUTTONS[index]) && held(index)) {
            return true;
        }
    }
    return false;
}

void add_to(PADStatus& status) {
    if (!visible() || g_fingers.empty()) {
        return;
    }
    status.err = PAD_ERR_NONE;  // the touch buttons are a controller in port 1
    for (int index = 0; index < static_cast<int>(BUTTONS.size()); ++index) {
        if (held(index)) {
            status.button |= BUTTONS[index].pad_bits;
        }
    }
    if (status.button & PAD_TRIGGER_L) {
        status.triggerLeft = 255;
    }
    if (status.button & PAD_TRIGGER_R) {
        status.triggerRight = 255;
    }
    if (held(STICK)) {
        const ImVec2 position = stick_position();
        status.stickX = static_cast<s8>(std::lround(position.x * STICK_FULL));
        status.stickY = static_cast<s8>(std::lround(position.y * STICK_FULL));
    }
}

}  // namespace touch_buttons
