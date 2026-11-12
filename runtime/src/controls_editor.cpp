// See controls_editor.h.

#include "controls_editor.h"

#include "controls.h"
#include "option_rows.h"
#include "texts.h"
#include "theme.h"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_timer.h>
#include <dolphin/pad.h>
#include <imgui.h>

#include <string>
#include <vector>

namespace controls_editor {
namespace {

using texts::Text;
using texts::text;

constexpr u32 PORT = PAD_CHAN0;
// A part that waits longer than this gives up (the controller's B closes the menu, so it cannot cancel).
constexpr Uint64 WAIT_LIMIT_MS = 6000;
// After a part got its key, the menu ignores Escape and clicks for a moment: the key that was
// just pressed is still down (Enter, for example, would press the button again).
constexpr Uint64 SETTLE_MS = 300;

// A part of the controller the player can change: its button, and the axis a key moves (the
// sticks, and the triggers, which are both a button and an axis).
struct Row {
    std::string label;
    PADButton button = 0;
    s32 axis = -1;
    s32 second_axis = -1;  // L and R are a button and a trigger axis
};

std::string direction(controls::Part part, Text way) { return controls::part_name(part) + " " + text(way); }

std::vector<Row> keyboard_rows() {
    using controls::Part;
    std::vector<Row> rows = {
        {controls::part_name(Part::A), PAD_BUTTON_A},
        {controls::part_name(Part::B), PAD_BUTTON_B},
        {controls::part_name(Part::X), PAD_BUTTON_X},
        {controls::part_name(Part::Y), PAD_BUTTON_Y},
        {controls::part_name(Part::Z), PAD_TRIGGER_Z},
        {controls::part_name(Part::L), PAD_TRIGGER_L, PAD_AXIS_TRIGGER_L},
        {controls::part_name(Part::R), PAD_TRIGGER_R, PAD_AXIS_TRIGGER_R},
        {controls::part_name(Part::Start), PAD_BUTTON_START},
        {direction(Part::DPad, Text::DirectionUp), PAD_BUTTON_UP},
        {direction(Part::DPad, Text::DirectionDown), PAD_BUTTON_DOWN},
        {direction(Part::DPad, Text::DirectionLeft), PAD_BUTTON_LEFT},
        {direction(Part::DPad, Text::DirectionRight), PAD_BUTTON_RIGHT},
    };
    const struct {
        Part part;
        s32 up, down, left, right;
    } sticks[] = {
        {Part::Stick, PAD_AXIS_LEFT_Y_POS, PAD_AXIS_LEFT_Y_NEG, PAD_AXIS_LEFT_X_NEG, PAD_AXIS_LEFT_X_POS},
        {Part::CStick, PAD_AXIS_RIGHT_Y_POS, PAD_AXIS_RIGHT_Y_NEG, PAD_AXIS_RIGHT_X_NEG, PAD_AXIS_RIGHT_X_POS},
    };
    for (const auto& stick : sticks) {
        rows.push_back({direction(stick.part, Text::DirectionUp), 0, stick.up});
        rows.push_back({direction(stick.part, Text::DirectionDown), 0, stick.down});
        rows.push_back({direction(stick.part, Text::DirectionLeft), 0, stick.left});
        rows.push_back({direction(stick.part, Text::DirectionRight), 0, stick.right});
    }
    return rows;
}

// The gamepad's buttons (its sticks go with the controller's own).
std::vector<Row> gamepad_rows() {
    std::vector<Row> rows = keyboard_rows();
    std::erase_if(rows, [](const Row& row) { return row.button == 0; });
    return rows;
}

// What is being edited.
int g_device = 0;  // 0: the keyboard and the mouse, 1: the gamepad that plays
int g_waiting_row = -1;
bool g_waiting_for_release = false;  // a gamepad's button that is still down from before
Uint64 g_wait_started = 0;
Uint64 g_stopped_at = 0;

bool gamepad_connected() { return PADGetIndexForPort(PORT) >= 0; }

void stop_waiting() {
    if (g_waiting_row >= 0) {
        g_stopped_at = SDL_GetTicks();
    }
    g_waiting_row = -1;
}

void start_waiting(int row) {
    g_waiting_row = row;
    g_waiting_for_release = true;
    g_wait_started = SDL_GetTicks();
}

// aurora keeps the bindings of the keyboard only while it plays the port (it is switched
// back at the next read of the controllers, pad_natives.cpp).
void wake_keyboard_bindings() { PADSetKeyboardActive(PORT, TRUE); }

s32 key_of_button(PADButton button) {
    wake_keyboard_bindings();
    u32 count = 0;
    if (const PADKeyButtonBinding* bindings = PADGetKeyButtonBindings(PORT, &count)) {
        for (u32 i = 0; i < count; ++i) {
            if (bindings[i].padButton == button) {
                return bindings[i].scancode;
            }
        }
    }
    return PAD_KEY_INVALID;
}

PADKeyAxisBinding* axis_binding(s32 axis) {
    wake_keyboard_bindings();
    u32 count = 0;
    if (PADKeyAxisBinding* bindings = PADGetKeyAxisBindings(PORT, &count)) {
        for (u32 i = 0; i < count; ++i) {
            if (bindings[i].padAxis == axis) {
                return &bindings[i];
            }
        }
    }
    return nullptr;
}

std::string key_name(s32 code) {
    switch (code) {
    case PAD_KEY_INVALID: return text(Text::NoKey);
    case PAD_KEY_MOUSE_LEFT: return text(Text::MouseLeft);
    case PAD_KEY_MOUSE_MIDDLE: return text(Text::MouseMiddle);
    case PAD_KEY_MOUSE_RIGHT: return text(Text::MouseRight);
    case PAD_KEY_MOUSE_X1:
    case PAD_KEY_MOUSE_X2: return text(Text::MouseOther);
    default: break;
    }
    const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(code));
    return name != nullptr && *name != '\0' ? name : text(Text::NoKey);
}

SDL_GamepadType gamepad_type() {
    SDL_Gamepad* gamepad = PADGetSDLGamepadForIndex(static_cast<u32>(std::max(PADGetIndexForPort(PORT), 0)));
    return gamepad != nullptr ? SDL_GetGamepadType(gamepad) : SDL_GAMEPAD_TYPE_UNKNOWN;
}

std::string gamepad_label(PADButton button) {
    u32 count = 0;
    const PADButtonMapping* mappings = PADGetButtonMappings(PORT, &count);
    for (u32 i = 0; mappings != nullptr && i < count; ++i) {
        if (mappings[i].padButton != button) {
            continue;
        }
        const u32 native = mappings[i].nativeButton;
        if (native == PAD_NATIVE_BUTTON_DISABLED || native == static_cast<u32>(-1)) {
            return text(Text::NoKey);
        }
        if (PADIsAxisButton(native)) {
            return controls::native_axis_label(gamepad_type(), static_cast<int>(PADAxisButtonAxis(native)));
        }
        return controls::native_button_label(gamepad_type(), static_cast<int>(native));
    }
    return text(Text::NoKey);
}

std::string binding_label(const Row& row) {
    if (g_device == 1) {
        return gamepad_label(row.button);
    }
    if (row.button != 0) {
        return key_name(key_of_button(row.button));
    }
    const PADKeyAxisBinding* binding = axis_binding(row.axis);
    return binding != nullptr ? key_name(binding->scancode) : text(Text::NoKey);
}

// Gives the key to the part, and saves it.
void assign_key(const Row& row, s32 code) {
    if (row.button != 0) {
        PADSetKeyButtonBinding(PORT, {code, row.button});
    }
    for (const s32 axis : {row.axis, row.second_axis}) {
        if (axis >= 0) {
            const PADKeyAxisBinding* current = axis_binding(axis);
            PADSetKeyAxisBinding(PORT, {code, static_cast<PADAxis>(axis), current != nullptr ? current->influence : s16{0}});
        }
    }
    PADSerializeMappings();
}

void assign_button(const Row& row, s32 native) {
    PADSetButtonMapping(PORT, {static_cast<u32>(native), row.button});
    PADSerializeMappings();
}

void restore_defaults() {
    if (g_device == 1) {
        PADRestoreDefaultMapping(PORT);
    } else {
        PADRestoreDefaultKeyBindings(PORT);
    }
    PADSerializeMappings();
}

// A gamepad's part waits for the next button, once the buttons that were down are let go.
void listen_to_gamepad(const std::vector<Row>& rows) {
    const s32 pressed = PADGetNativeButtonPressed(PORT);
    if (g_waiting_for_release) {
        g_waiting_for_release = pressed >= 0;
        return;
    }
    if (pressed >= 0) {
        assign_button(rows[static_cast<size_t>(g_waiting_row)], pressed);
        stop_waiting();
    }
}

}  // namespace

bool busy() { return g_waiting_row >= 0 || SDL_GetTicks() - g_stopped_at < SETTLE_MS; }

void stop() {
    stop_waiting();
    g_device = 0;
}

bool handle_event(const SDL_Event& event) {
    if (g_waiting_row < 0 || g_device != 0) {
        return false;
    }
    const std::vector<Row> rows = keyboard_rows();
    s32 code = PAD_KEY_INVALID;
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat) {
        if (event.key.scancode == SDL_SCANCODE_F10) {
            return false;  // the menu's own key
        }
        if (event.key.scancode != SDL_SCANCODE_ESCAPE) {
            code = static_cast<s32>(event.key.scancode);
        }
    } else if (event.type == SDL_EVENT_MOUSE_BUTTON_DOWN) {
        // The left button is how the menu is used: it cancels, it is not given to a part.
        switch (event.button.button) {
        case SDL_BUTTON_MIDDLE: code = PAD_KEY_MOUSE_MIDDLE; break;
        case SDL_BUTTON_RIGHT: code = PAD_KEY_MOUSE_RIGHT; break;
        case SDL_BUTTON_X1: code = PAD_KEY_MOUSE_X1; break;
        case SDL_BUTTON_X2: code = PAD_KEY_MOUSE_X2; break;
        default: break;
        }
    } else {
        return false;
    }
    if (code != PAD_KEY_INVALID) {
        assign_key(rows[static_cast<size_t>(g_waiting_row)], code);
    }
    stop_waiting();
    return true;
}

void draw(bool focus) {
    const bool connected = gamepad_connected();
    if (!connected) {
        g_device = 0;
    }
    const std::vector<Row> rows = g_device == 1 ? gamepad_rows() : keyboard_rows();
    if (g_waiting_row >= static_cast<int>(rows.size()) || SDL_GetTicks() - g_wait_started > WAIT_LIMIT_MS) {
        stop_waiting();
    }
    if (g_waiting_row >= 0 && g_device == 1) {
        listen_to_gamepad(rows);
    }

    std::vector<std::string> devices = {text(Text::Keyboard)};
    if (connected) {
        const char* name = PADGetName(PORT);
        devices.push_back(name != nullptr ? name : text(Text::Controller));
    }
    if (focus) {
        ImGui::SetKeyboardFocusHere();
    }
    int device = g_device;
    if (theme::choice("##editor device", &device, devices, std::min(ImGui::GetContentRegionAvail().x * 0.6f, theme::px(420)))) {
        stop_waiting();
        g_device = device;
    }
    theme::paragraph(text(g_device == 1 ? Text::EditorIntroGamepad : Text::EditorIntroKeyboard), theme::TEXT_SOFT);
    if (!connected) {
        theme::paragraph(text(Text::EditorConnectGamepad), theme::TEXT_SOFT);
    }

    if (option_rows::begin("##editor rows")) {
        const bool settled = SDL_GetTicks() - g_stopped_at >= SETTLE_MS;
        for (size_t i = 0; i < rows.size(); ++i) {
            option_rows::next_row(rows[i].label.c_str(), nullptr);
            const bool waiting = g_waiting_row == static_cast<int>(i);
            const std::string label = waiting ? text(g_device == 1 ? Text::PressButton : Text::PressKey) : binding_label(rows[i]);
            ImGui::PushID(static_cast<int>(i));
            if (theme::menu_button(label.c_str(), ImVec2(ImGui::GetContentRegionAvail().x, theme::px(30)),
                                   waiting ? theme::ButtonColor::Orange : theme::ButtonColor::Gray) &&
                settled && !waiting) {
                start_waiting(static_cast<int>(i));
            }
            ImGui::PopID();
        }
        option_rows::end();
    }
    if (theme::menu_button(text(Text::RestoreMapping), ImVec2(theme::px(220), theme::px(34)), theme::ButtonColor::Green)) {
        stop_waiting();
        restore_defaults();
    }
}

}  // namespace controls_editor
