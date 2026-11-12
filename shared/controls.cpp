// See controls.h.

#include "controls.h"

#include "texts.h"
#include "theme.h"

#include <SDL3/SDL_keyboard.h>

#include <algorithm>
#include <cmath>

namespace controls {
namespace {

using texts::Text;

// aurora's automatic mapping (aurora-main/lib/dolphin/pad/pad.cpp): the face buttons go by their
// place, and Xbox and GameCube gamepads put B on the left of A, as the GameCube does.
struct FaceButtons {
    SDL_GamepadButton a, b, x, y;
};

FaceButtons face_buttons(SDL_GamepadType type) {
    switch (type) {
    case SDL_GAMEPAD_TYPE_XBOX360:
    case SDL_GAMEPAD_TYPE_XBOXONE:
    case SDL_GAMEPAD_TYPE_GAMECUBE:
        return {SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_NORTH};
    default:
        return {SDL_GAMEPAD_BUTTON_SOUTH, SDL_GAMEPAD_BUTTON_EAST, SDL_GAMEPAD_BUTTON_WEST, SDL_GAMEPAD_BUTTON_NORTH};
    }
}

enum class Family { Xbox, PlayStation, Nintendo, GameCube, Other };

Family family(SDL_GamepadType type) {
    switch (type) {
    case SDL_GAMEPAD_TYPE_XBOX360:
    case SDL_GAMEPAD_TYPE_XBOXONE:
        return Family::Xbox;
    case SDL_GAMEPAD_TYPE_PS3:
    case SDL_GAMEPAD_TYPE_PS4:
    case SDL_GAMEPAD_TYPE_PS5:
        return Family::PlayStation;
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_PRO:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_LEFT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_RIGHT:
    case SDL_GAMEPAD_TYPE_NINTENDO_SWITCH_JOYCON_PAIR:
        return Family::Nintendo;
    case SDL_GAMEPAD_TYPE_GAMECUBE:
        return Family::GameCube;
    default:
        return Family::Other;
    }
}

// The label printed on a face button of this kind of gamepad.
std::string face_label(SDL_GamepadType type, SDL_GamepadButton button) {
    switch (SDL_GetGamepadButtonLabelForType(type, button)) {
    case SDL_GAMEPAD_BUTTON_LABEL_A: return "A";
    case SDL_GAMEPAD_BUTTON_LABEL_B: return "B";
    case SDL_GAMEPAD_BUTTON_LABEL_X: return "X";
    case SDL_GAMEPAD_BUTTON_LABEL_Y: return "Y";
    case SDL_GAMEPAD_BUTTON_LABEL_CROSS: return "\xE2\x9C\x95";     // ✕
    case SDL_GAMEPAD_BUTTON_LABEL_CIRCLE: return "\xE2\x97\x8B";    // ○
    case SDL_GAMEPAD_BUTTON_LABEL_SQUARE: return "\xE2\x96\xA1";    // □
    case SDL_GAMEPAD_BUTTON_LABEL_TRIANGLE: return "\xE2\x96\xB3";  // △
    default: break;
    }
    switch (button) {
    case SDL_GAMEPAD_BUTTON_SOUTH: return "\xE2\x86\x93";  // ↓ (the bottom one)
    case SDL_GAMEPAD_BUTTON_EAST: return "\xE2\x86\x92";
    case SDL_GAMEPAD_BUTTON_WEST: return "\xE2\x86\x90";
    default: return "\xE2\x86\x91";
    }
}

// The names of the other buttons the mapping uses, by family: the right shoulder (Z), the left
// and right triggers (L and R) and the start button.
struct OtherNames {
    const char* shoulder;
    const char* left_trigger;
    const char* right_trigger;
    const char* start;
};

OtherNames other_names(Family kind) {
    switch (kind) {
    case Family::Xbox: return {"RB", "LT", "RT", "Menu"};
    case Family::PlayStation: return {"R1", "L2", "R2", "Options"};
    case Family::Nintendo: return {"R", "ZL", "ZR", "+"};
    case Family::GameCube: return {"Z", "L", "R", "Start"};
    case Family::Other: break;
    }
    return {"RB", "LT", "RT", "Start"};
}

// The runtime's keyboard (Dolphin's default keys), as SDL scancodes, for the parts that are one key.
struct KeyBinding {
    Part part;
    SDL_Scancode key;
    const char* name;
};
constexpr KeyBinding KEYS[] = {
    {Part::A, SDL_SCANCODE_X, "X"},     {Part::B, SDL_SCANCODE_Z, "Z"},     {Part::X, SDL_SCANCODE_C, "C"},
    {Part::Y, SDL_SCANCODE_S, "S"},     {Part::Z, SDL_SCANCODE_D, "D"},     {Part::L, SDL_SCANCODE_Q, "Q"},
    {Part::R, SDL_SCANCODE_W, "W"},     {Part::Start, SDL_SCANCODE_RETURN, "Enter"},
};
constexpr SDL_Scancode STICK_KEYS[] = {SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT};
constexpr SDL_Scancode C_STICK_KEYS[] = {SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L};
constexpr SDL_Scancode D_PAD_KEYS[] = {SDL_SCANCODE_T, SDL_SCANCODE_G, SDL_SCANCODE_F, SDL_SCANCODE_H};

constexpr float AXIS_PRESSED = 0.35f;

ImVec2 stick_from_keys(const bool* keys, const SDL_Scancode (&codes)[4]) {
    return ImVec2((keys[codes[3]] ? 1.f : 0.f) - (keys[codes[2]] ? 1.f : 0.f),
                  (keys[codes[0]] ? 1.f : 0.f) - (keys[codes[1]] ? 1.f : 0.f));
}

float axis(SDL_Gamepad* gamepad, SDL_GamepadAxis which) {
    return static_cast<float>(SDL_GetGamepadAxis(gamepad, which)) / 32767.f;
}

bool moved(ImVec2 stick) { return std::abs(stick.x) > AXIS_PRESSED || std::abs(stick.y) > AXIS_PRESSED; }

}  // namespace

Gamepads::~Gamepads() { close(); }

void Gamepads::close() {
    for (auto& [id, gamepad] : open_) {
        SDL_CloseGamepad(gamepad);
    }
    open_.clear();
}

std::vector<Device> Gamepads::devices() {
    std::vector<Device> found;
    int count = 0;
    SDL_JoystickID* ids = SDL_GetGamepads(&count);
    for (int i = 0; i < count; ++i) {
        SDL_Gamepad*& gamepad = open_[ids[i]];
        if (gamepad == nullptr) {
            gamepad = SDL_OpenGamepad(ids[i]);
        }
        if (gamepad == nullptr) {
            open_.erase(ids[i]);
            continue;
        }
        const char* name = SDL_GetGamepadName(gamepad);
        found.push_back({ids[i], name != nullptr ? name : "Gamepad", SDL_GetGamepadType(gamepad)});
    }
    SDL_free(ids);
    // Gamepads that were unplugged.
    for (auto it = open_.begin(); it != open_.end();) {
        const bool connected = std::ranges::any_of(found, [&](const Device& device) { return device.id == it->first; });
        if (!connected) {
            SDL_CloseGamepad(it->second);
            it = open_.erase(it);
        } else {
            ++it;
        }
    }
    found.push_back({0, texts::text(Text::Keyboard), SDL_GAMEPAD_TYPE_UNKNOWN});
    return found;
}

State Gamepads::state(const Device& device) {
    State state;
    if (device.id == 0) {
        const bool* keys = SDL_GetKeyboardState(nullptr);
        for (const KeyBinding& binding : KEYS) {
            state.pressed[static_cast<size_t>(binding.part)] = keys[binding.key];
        }
        state.stick = stick_from_keys(keys, STICK_KEYS);
        state.c_stick = stick_from_keys(keys, C_STICK_KEYS);
        for (size_t i = 0; i < 4; ++i) {
            state.d_pad[i] = keys[D_PAD_KEYS[i]];
        }
    } else {
        const auto found = open_.find(device.id);
        if (found == open_.end()) {
            return state;
        }
        SDL_Gamepad* gamepad = found->second;
        const FaceButtons face = face_buttons(device.type);
        state.pressed[static_cast<size_t>(Part::A)] = SDL_GetGamepadButton(gamepad, face.a);
        state.pressed[static_cast<size_t>(Part::B)] = SDL_GetGamepadButton(gamepad, face.b);
        state.pressed[static_cast<size_t>(Part::X)] = SDL_GetGamepadButton(gamepad, face.x);
        state.pressed[static_cast<size_t>(Part::Y)] = SDL_GetGamepadButton(gamepad, face.y);
        state.pressed[static_cast<size_t>(Part::Z)] = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER);
        state.pressed[static_cast<size_t>(Part::Start)] = SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_START);
        state.pressed[static_cast<size_t>(Part::L)] = axis(gamepad, SDL_GAMEPAD_AXIS_LEFT_TRIGGER) > AXIS_PRESSED;
        state.pressed[static_cast<size_t>(Part::R)] = axis(gamepad, SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) > AXIS_PRESSED;
        // SDL's y axes point down; the GameCube's up.
        state.stick = ImVec2(axis(gamepad, SDL_GAMEPAD_AXIS_LEFTX), -axis(gamepad, SDL_GAMEPAD_AXIS_LEFTY));
        state.c_stick = ImVec2(axis(gamepad, SDL_GAMEPAD_AXIS_RIGHTX), -axis(gamepad, SDL_GAMEPAD_AXIS_RIGHTY));
        state.d_pad = {SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_UP),
                       SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_DOWN),
                       SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_LEFT),
                       SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_DPAD_RIGHT)};
    }
    state.pressed[static_cast<size_t>(Part::Stick)] = moved(state.stick);
    state.pressed[static_cast<size_t>(Part::CStick)] = moved(state.c_stick);
    state.pressed[static_cast<size_t>(Part::DPad)] = std::ranges::any_of(state.d_pad, [](bool down) { return down; });
    return state;
}

std::string native_button_label(SDL_GamepadType type, int button) {
    const OtherNames names = other_names(family(type));
    const bool nintendo = family(type) == Family::Nintendo || family(type) == Family::GameCube;
    switch (static_cast<SDL_GamepadButton>(button)) {
    case SDL_GAMEPAD_BUTTON_SOUTH:
    case SDL_GAMEPAD_BUTTON_EAST:
    case SDL_GAMEPAD_BUTTON_WEST:
    case SDL_GAMEPAD_BUTTON_NORTH: return face_label(type, static_cast<SDL_GamepadButton>(button));
    case SDL_GAMEPAD_BUTTON_START: return names.start;
    case SDL_GAMEPAD_BUTTON_BACK: return family(type) == Family::PlayStation ? "Share" : nintendo ? "-" : "Back";
    case SDL_GAMEPAD_BUTTON_LEFT_STICK: return family(type) == Family::PlayStation ? "L3" : "LS";
    case SDL_GAMEPAD_BUTTON_RIGHT_STICK: return family(type) == Family::PlayStation ? "R3" : "RS";
    case SDL_GAMEPAD_BUTTON_LEFT_SHOULDER:
        return family(type) == Family::Xbox ? "LB" : family(type) == Family::PlayStation ? "L1" : "L";
    case SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER: return names.shoulder;
    case SDL_GAMEPAD_BUTTON_DPAD_UP: return "\xE2\x86\x91";     // ↑
    case SDL_GAMEPAD_BUTTON_DPAD_DOWN: return "\xE2\x86\x93";   // ↓
    case SDL_GAMEPAD_BUTTON_DPAD_LEFT: return "\xE2\x86\x90";   // ←
    case SDL_GAMEPAD_BUTTON_DPAD_RIGHT: return "\xE2\x86\x92";  // →
    default: break;
    }
    return "#" + std::to_string(button);
}

std::string native_axis_label(SDL_GamepadType type, int axis) {
    const OtherNames names = other_names(family(type));
    if (axis == SDL_GAMEPAD_AXIS_LEFT_TRIGGER) {
        return names.left_trigger;
    }
    if (axis == SDL_GAMEPAD_AXIS_RIGHT_TRIGGER) {
        return names.right_trigger;
    }
    return texts::text(Text::AnalogStick);
}

std::string part_name(Part part) {
    switch (part) {
    case Part::A: return "A";
    case Part::B: return "B";
    case Part::X: return "X";
    case Part::Y: return "Y";
    case Part::Z: return "Z";
    case Part::L: return "L";
    case Part::R: return "R";
    case Part::Start: return "START";
    case Part::Stick: return texts::text(Text::ControlStick);
    case Part::CStick: return texts::text(Text::CStick);
    case Part::DPad: return texts::text(Text::DPad);
    case Part::Count: break;
    }
    return {};
}

std::string binding(const Device& device, Part part) {
    if (device.id == 0) {
        for (const KeyBinding& key : KEYS) {
            if (key.part == part) {
                return key.name;
            }
        }
        switch (part) {
        case Part::Stick: return texts::text(Text::ArrowKeys);
        case Part::CStick: return "I  J  K  L";
        case Part::DPad: return "T  F  G  H";
        default: return {};
        }
    }
    const FaceButtons face = face_buttons(device.type);
    const OtherNames names = other_names(family(device.type));
    switch (part) {
    case Part::A: return face_label(device.type, face.a);
    case Part::B: return face_label(device.type, face.b);
    case Part::X: return face_label(device.type, face.x);
    case Part::Y: return face_label(device.type, face.y);
    case Part::Z: return names.shoulder;
    case Part::L: return names.left_trigger;
    case Part::R: return names.right_trigger;
    case Part::Start: return names.start;
    case Part::Stick: return texts::text(Text::LeftStick);
    case Part::CStick: return texts::text(Text::RightStick);
    case Part::DPad: return texts::text(Text::DPad);
    case Part::Count: break;
    }
    return {};
}

void draw_controller(ImDrawList* draw, ImVec2 min, ImVec2 size, const State& state) {
    // Proportions of a GameCube controller seen from the front, in shares of `size`.
    const auto at = [&](float x, float y) { return ImVec2(min.x + x * size.x, min.y + y * size.y); };
    const float unit = std::min(size.x, size.y * 1.6f);
    const auto lit = [&](Part part) { return state.pressed[static_cast<size_t>(part)]; };
    const auto glow = [&](ImVec2 center, float radius) {
        draw->AddCircleFilled(center, radius * 1.35f, IM_COL32(255, 255, 200, 90), 40);
    };
    constexpr ImU32 BODY = IM_COL32(92, 84, 170, 255);
    constexpr ImU32 BODY_EDGE = IM_COL32(52, 46, 110, 255);
    constexpr ImU32 GRAY = IM_COL32(196, 198, 210, 255);
    constexpr ImU32 GRAY_LIT = IM_COL32(255, 255, 255, 255);
    constexpr ImU32 EDGE = IM_COL32(40, 36, 80, 255);

    // Shoulders: L, R and Z.
    const auto shoulder = [&](ImVec2 a, ImVec2 b, ImU32 color, bool on) {
        draw->AddRectFilled(a, b, on ? GRAY_LIT : color, unit * 0.03f);
        draw->AddRect(a, b, EDGE, unit * 0.03f, 0, theme::px(2));
    };
    shoulder(at(0.13f, 0.08f), at(0.37f, 0.21f), GRAY, lit(Part::L));
    shoulder(at(0.63f, 0.08f), at(0.87f, 0.21f), GRAY, lit(Part::R));
    shoulder(at(0.66f, 0.0f), at(0.84f, 0.075f), lit(Part::Z) ? GRAY_LIT : IM_COL32(124, 112, 230, 255), lit(Part::Z));

    // The body and its two grips.
    for (const float x : {0.27f, 0.73f}) {
        draw->AddCircleFilled(at(x, 0.66f), unit * 0.17f, BODY, 48);
        draw->AddCircle(at(x, 0.66f), unit * 0.17f, BODY_EDGE, 48, theme::px(3));
    }
    draw->AddRectFilled(at(0.08f, 0.18f), at(0.92f, 0.68f), BODY, unit * 0.16f);
    draw->AddRect(at(0.08f, 0.18f), at(0.92f, 0.68f), BODY_EDGE, unit * 0.16f, 0, theme::px(3));
    for (const float x : {0.27f, 0.73f}) {
        draw->AddCircleFilled(at(x, 0.62f), unit * 0.15f, BODY, 48);
    }

    // The control stick, which moves with the state.
    const ImVec2 stick = at(0.25f, 0.36f);
    const float stick_radius = unit * 0.085f;
    draw->AddCircleFilled(stick, stick_radius, IM_COL32(60, 54, 120, 255), 40);
    if (lit(Part::Stick)) {
        glow(stick, stick_radius);
    }
    const ImVec2 knob = stick + ImVec2(state.stick.x, -state.stick.y) * stick_radius * 0.45f;
    draw->AddCircleFilled(knob, stick_radius * 0.72f, lit(Part::Stick) ? GRAY_LIT : GRAY, 40);
    draw->AddCircle(knob, stick_radius * 0.72f, EDGE, 40, theme::px(2));

    // The D-pad: a cross whose arms light up.
    const ImVec2 pad = at(0.35f, 0.6f);
    const float arm = unit * 0.045f;
    const float thick = unit * 0.018f;
    const ImVec2 arms[4][2] = {{ImVec2(-thick, -arm), ImVec2(thick, 0)},
                               {ImVec2(-thick, 0), ImVec2(thick, arm)},
                               {ImVec2(-arm, -thick), ImVec2(0, thick)},
                               {ImVec2(0, -thick), ImVec2(arm, thick)}};
    for (size_t i = 0; i < 4; ++i) {
        draw->AddRectFilled(pad + arms[i][0], pad + arms[i][1], state.d_pad[i] ? GRAY_LIT : GRAY, theme::px(2));
    }
    draw->AddRect(pad - ImVec2(thick, arm), pad + ImVec2(thick, arm), EDGE, theme::px(2), 0, theme::px(1.5f));
    draw->AddRect(pad - ImVec2(arm, thick), pad + ImVec2(arm, thick), EDGE, theme::px(2), 0, theme::px(1.5f));

    // The C-stick, yellow.
    const ImVec2 c_stick = at(0.65f, 0.61f);
    const float c_radius = unit * 0.06f;
    draw->AddCircleFilled(c_stick, c_radius, IM_COL32(60, 54, 120, 255), 40);
    if (lit(Part::CStick)) {
        glow(c_stick, c_radius);
    }
    const ImVec2 c_knob = c_stick + ImVec2(state.c_stick.x, -state.c_stick.y) * c_radius * 0.45f;
    draw->AddCircleFilled(c_knob, c_radius * 0.7f, lit(Part::CStick) ? IM_COL32(255, 240, 150, 255)
                                                                      : IM_COL32(244, 204, 52, 255), 40);
    draw->AddCircle(c_knob, c_radius * 0.7f, EDGE, 40, theme::px(2));

    // START, in the middle.
    const ImVec2 start = at(0.5f, 0.38f);
    draw->AddCircleFilled(start, unit * 0.022f, lit(Part::Start) ? GRAY_LIT : GRAY, 24);
    draw->AddCircle(start, unit * 0.022f, EDGE, 24, theme::px(1.5f));

    // A (big, green), B (small, red), X and Y (kidneys, around A).
    const ImVec2 a = at(0.73f, 0.36f);
    const float a_radius = unit * 0.07f;
    if (lit(Part::A)) {
        glow(a, a_radius);
    }
    draw->AddCircleFilled(a, a_radius, lit(Part::A) ? IM_COL32(150, 255, 190, 255) : IM_COL32(46, 190, 120, 255), 40);
    draw->AddCircle(a, a_radius, EDGE, 40, theme::px(2));
    const ImVec2 b = at(0.62f, 0.43f);
    const float b_radius = unit * 0.038f;
    if (lit(Part::B)) {
        glow(b, b_radius);
    }
    draw->AddCircleFilled(b, b_radius, lit(Part::B) ? IM_COL32(255, 170, 170, 255) : IM_COL32(220, 64, 64, 255), 32);
    draw->AddCircle(b, b_radius, EDGE, 32, theme::px(2));
    const ImVec2 x = at(0.83f, 0.31f);
    draw->AddEllipseFilled(x, ImVec2(unit * 0.03f, unit * 0.058f), lit(Part::X) ? GRAY_LIT : GRAY, 0.25f, 32);
    draw->AddEllipse(x, ImVec2(unit * 0.03f, unit * 0.058f), EDGE, 0.25f, 32, theme::px(2));
    const ImVec2 y = at(0.7f, 0.235f);
    draw->AddEllipseFilled(y, ImVec2(unit * 0.058f, unit * 0.028f), lit(Part::Y) ? GRAY_LIT : GRAY, -0.2f, 32);
    draw->AddEllipse(y, ImVec2(unit * 0.058f, unit * 0.028f), EDGE, -0.2f, 32, theme::px(2));

    // The letters on the buttons.
    ImFont* font = theme::fonts().small;
    const auto letter = [&](ImVec2 center, const char* text, ImU32 color) {
        const ImVec2 extent = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.f, text);
        draw->AddText(font, font->FontSize, center - extent * 0.5f, color, text);
    };
    letter(a, "A", IM_COL32(20, 70, 40, 255));
    letter(b, "B", IM_COL32(90, 20, 20, 255));
    letter(x, "X", EDGE);
    letter(y, "Y", EDGE);
    letter(at(0.25f, 0.13f), "L", EDGE);
    letter(at(0.84f, 0.13f), "R", EDGE);
    letter(at(0.75f, 0.0375f), "Z", IM_COL32(255, 255, 255, 255));
}

void draw_screen(Gamepads& gamepads, int& selected_device, bool focus) {
    theme::paragraph(texts::text(Text::ControlsIntro), theme::TEXT_SOFT);
    theme::paragraph(texts::text(Text::GameShortcuts), theme::TEXT_SOFT);
    const std::vector<Device> devices = gamepads.devices();
    selected_device = std::clamp(selected_device, 0, static_cast<int>(devices.size()) - 1);
    std::vector<std::string> names;
    for (const Device& device : devices) {
        names.push_back(device.name);
    }
    if (focus) {
        ImGui::SetKeyboardFocusHere();
    }
    theme::choice("device", &selected_device, names, std::min(ImGui::GetContentRegionAvail().x * 0.5f, theme::px(560)));
    const Device& device = devices[static_cast<size_t>(selected_device)];
    // Beside it: whether the game maps that controller by itself.
    ImGui::SameLine(0, theme::px(20));
    ImGui::SetCursorPosY(ImGui::GetCursorPosY() + theme::px(10));
    if (devices.size() == 1) {
        theme::paragraph(texts::text(Text::NoGamepads), theme::TEXT_SOFT);
    } else if (device.id != 0) {
        theme::paragraph(std::string("\xE2\x9C\x93 ") + texts::text(Text::AutomaticMapping), theme::GREEN);
    } else {
        ImGui::NewLine();
    }

    // The controller with what is pressed lit up, and the table of what plays each part.
    const State state = gamepads.state(device);
    const float height = ImGui::GetContentRegionAvail().y;
    const float drawing_width = std::min(ImGui::GetContentRegionAvail().x * 0.5f, height * 1.55f);
    ImGui::Dummy(ImVec2(drawing_width, height - theme::px(30)));
    const ImVec2 drawing_min = ImGui::GetItemRectMin();
    draw_controller(ImGui::GetWindowDrawList(), drawing_min,
                    ImVec2(drawing_width, std::min(height - theme::px(30), drawing_width / 1.55f)), state);
    ImGui::SetCursorScreenPos(drawing_min + ImVec2(0, height - theme::px(26)));
    theme::paragraph(texts::text(Text::TryButtons), theme::TEXT_SOFT);
    ImGui::SetCursorScreenPos(drawing_min + ImVec2(drawing_width + theme::px(24), 0));
    ImGui::BeginChild("##bindings", ImVec2(0, height));
    ImGui::PushFont(theme::fonts().small);
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(theme::px(10), theme::px(3)));
    if (ImGui::BeginTable("##table", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
        ImGui::TableSetupColumn(texts::text(Text::GameCubeButton), ImGuiTableColumnFlags_WidthStretch, 0.45f);
        ImGui::TableSetupColumn(texts::text(Text::YourButton), ImGuiTableColumnFlags_WidthStretch, 0.55f);
        ImGui::TableHeadersRow();
        for (size_t index = 0; index < PART_COUNT; ++index) {
            const auto part = static_cast<Part>(index);
            ImGui::TableNextRow();
            if (state.pressed[index]) {
                ImGui::TableSetBgColor(ImGuiTableBgTarget_RowBg0, IM_COL32(255, 220, 140, 200));
            }
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(part_name(part).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(binding(device, part).c_str());
        }
        ImGui::EndTable();
    }
    ImGui::PopStyleVar();
    ImGui::PopFont();
    ImGui::EndChild();
}

}  // namespace controls
