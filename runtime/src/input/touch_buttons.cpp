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
#include <cmath>
#include <cstdlib>
#include <map>

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
// How much the game shows through the buttons, at rest and pressed.
constexpr float OPACITY = 0.55f;
constexpr float OPACITY_PRESSED = 0.85f;
// A controller's stick moved this far hides the buttons (a stick at rest drifts a little).
constexpr Sint16 STICK_MOVED = 12000;

enum class Shape { Round, Pill };

// The D-Pad's arrows are drawn, not written: the game's lettering has no arrows.
enum class Arrow { None, Up, Down, Left, Right };

struct Button {
    const char* label;  // nullptr: from the texts (the menu's)
    u16 pad_bits;       // 0: the options menu
    theme::ButtonColor color;
    Shape shape;
    // Where it is: its middle as units from a corner (negative: from the right or the bottom edge,
    // or, with `centered`, from the middle), and its size in units.
    float x, y, width, height;
    bool centered = false;
    Arrow arrow = Arrow::None;
};

const std::array<Button, 13> BUTTONS = {{
    {"A", PAD_BUTTON_A, theme::ButtonColor::Green, Shape::Round, -2.3f, -2.0f, 1.7f, 1.7f},
    {"B", PAD_BUTTON_B, theme::ButtonColor::Orange, Shape::Round, -4.15f, -1.15f, 1.1f, 1.1f},
    {"X", PAD_BUTTON_X, theme::ButtonColor::Gray, Shape::Round, -0.85f, -3.35f, 1.05f, 1.05f},
    {"Y", PAD_BUTTON_Y, theme::ButtonColor::Gray, Shape::Round, -3.0f, -3.95f, 1.05f, 1.05f},
    {"L", PAD_TRIGGER_L, theme::ButtonColor::Gray, Shape::Pill, 1.4f, 0.75f, 2.2f, 0.85f},
    {"R", PAD_TRIGGER_R, theme::ButtonColor::Gray, Shape::Pill, -1.4f, 0.75f, 2.2f, 0.85f},
    {"Z", PAD_TRIGGER_Z, theme::ButtonColor::Gray, Shape::Pill, -1.4f, 1.75f, 1.6f, 0.7f},
    {"START", PAD_BUTTON_START, theme::ButtonColor::Gray, Shape::Pill, 0.f, -0.7f, 2.0f, 0.75f, true},
    {nullptr, 0, theme::ButtonColor::Orange, Shape::Pill, 0.f, 0.65f, 2.2f, 0.75f, true},
    {"", PAD_BUTTON_UP, theme::ButtonColor::Gray, Shape::Round, 2.4f, -6.15f, 0.75f, 0.75f, false, Arrow::Up},
    {"", PAD_BUTTON_DOWN, theme::ButtonColor::Gray, Shape::Round, 2.4f, -4.75f, 0.75f, 0.75f, false, Arrow::Down},
    {"", PAD_BUTTON_LEFT, theme::ButtonColor::Gray, Shape::Round, 1.7f, -5.45f, 0.75f, 0.75f, false, Arrow::Left},
    {"", PAD_BUTTON_RIGHT, theme::ButtonColor::Gray, Shape::Round, 3.1f, -5.45f, 0.75f, 0.75f, false, Arrow::Right},
}};
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
    if (control >= 0 && BUTTONS[control].pad_bits == 0) {
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
        found->second.control = control >= 0 && BUTTONS[control].pad_bits == 0 ? NOTHING : control;
        if (found->second.control == STICK) {
            found->second.control = NOTHING;
        }
    }
}

// A white arrow with the button's dark edge, pointing the D-Pad's way, in the middle of the button.
void draw_arrow(ImDrawList* draw, ImVec2 min, ImVec2 max, Arrow arrow, bool pressed, float opacity) {
    const ImVec2 center = (min + max) * 0.5f + ImVec2(0, pressed ? (max.y - min.y) * 0.04f : 0.f);
    const float size = (max.x - min.x) * 0.24f;
    ImVec2 tip(0, -1), left(-0.9f, 0.6f), right(0.9f, 0.6f);  // pointing up
    const auto turned = [arrow](ImVec2 point) {
        switch (arrow) {
        case Arrow::Down: return ImVec2(point.x, -point.y);
        case Arrow::Left: return ImVec2(point.y, point.x);
        case Arrow::Right: return ImVec2(-point.y, point.x);
        default: return point;
        }
    };
    const ImVec2 a = center + turned(tip) * size;
    const ImVec2 b = center + turned(left) * size;
    const ImVec2 c = center + turned(right) * size;
    const auto alpha = static_cast<int>(255 * opacity);
    draw->AddTriangle(a, b, c, IM_COL32(98, 106, 116, alpha), std::max(2.f, size * 0.25f));
    draw->AddTriangleFilled(a, b, c, IM_COL32(255, 255, 255, alpha));
}

void draw_stick(ImDrawList* draw) {
    const float u = unit();
    const ImVec2 center = stick_center();
    const float ring = STICK_RING * u;
    const auto alpha = [](float opacity) { return static_cast<int>(255 * opacity); };
    // The ring, like the options' panels: pale, with an edge of the wood of the title's sign.
    draw->AddCircleFilled(center, ring, IM_COL32(236, 242, 250, alpha(OPACITY * 0.6f)), 48);
    draw->AddCircle(center, ring, IM_COL32(176, 112, 50, alpha(OPACITY)), 48, std::max(2.f, 0.06f * u));
    const ImVec2 position = stick_position();
    const ImVec2 knob_center = center + ImVec2(position.x, -position.y) * ring;
    const float knob = STICK_KNOB * u;
    theme::button_shape(draw, knob_center - ImVec2(knob, knob), knob_center + ImVec2(knob, knob), "",
                        theme::ButtonColor::Orange, held(STICK), held(STICK) ? OPACITY_PRESSED : OPACITY);
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
    for (int index = 0; index < static_cast<int>(BUTTONS.size()); ++index) {
        const Button& button = BUTTONS[index];
        const auto [min, max] = rectangle(button);
        const bool pressed = held(index);
        const char* label = button.label != nullptr ? button.label : texts::text(texts::Text::TouchOptions);
        const float opacity = pressed ? OPACITY_PRESSED : OPACITY;
        theme::button_shape(draw, min, max, label, button.color, pressed, opacity);
        if (button.arrow != Arrow::None) {
            draw_arrow(draw, min, max, button.arrow, pressed, opacity);
        }
    }
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
