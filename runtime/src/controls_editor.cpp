// See controls_editor.h.

#include "controls_editor.h"

#include "controls.h"
#include "controls_profile.h"
#include "player_folder.h"
#include "texts.h"
#include "theme.h"

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_keyboard.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_timer.h>
#include <dolphin/pad.h>
#include <imgui.h>

#include <cfloat>
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

// A part the player can change: its button, and the axis a key moves (the sticks, and the
// triggers, which are both a button and an axis). The turbo has neither: it is not the GameCube's,
// and the runtime reads its key or button itself (turbo_held).
struct Row {
    controls_profile::Slot slot;
    PADButton button = 0;
    s32 axis = -1;
    s32 second_axis = -1;  // L and R are a button and a trigger axis
};

std::vector<Row> keyboard_rows() {
    using controls_profile::Slot;
    return {
        {Slot::A, PAD_BUTTON_A},
        {Slot::B, PAD_BUTTON_B},
        {Slot::X, PAD_BUTTON_X},
        {Slot::Y, PAD_BUTTON_Y},
        {Slot::Z, PAD_TRIGGER_Z},
        {Slot::L, PAD_TRIGGER_L, PAD_AXIS_TRIGGER_L},
        {Slot::R, PAD_TRIGGER_R, PAD_AXIS_TRIGGER_R},
        {Slot::Start, PAD_BUTTON_START},
        {Slot::DPadUp, PAD_BUTTON_UP},
        {Slot::DPadDown, PAD_BUTTON_DOWN},
        {Slot::DPadLeft, PAD_BUTTON_LEFT},
        {Slot::DPadRight, PAD_BUTTON_RIGHT},
        {Slot::StickUp, 0, PAD_AXIS_LEFT_Y_POS},
        {Slot::StickDown, 0, PAD_AXIS_LEFT_Y_NEG},
        {Slot::StickLeft, 0, PAD_AXIS_LEFT_X_NEG},
        {Slot::StickRight, 0, PAD_AXIS_LEFT_X_POS},
        {Slot::CStickUp, 0, PAD_AXIS_RIGHT_Y_POS},
        {Slot::CStickDown, 0, PAD_AXIS_RIGHT_Y_NEG},
        {Slot::CStickLeft, 0, PAD_AXIS_RIGHT_X_NEG},
        {Slot::CStickRight, 0, PAD_AXIS_RIGHT_X_POS},
        {Slot::Turbo},
    };
}

// The gamepad's buttons and the turbo (its sticks go with the controller's own).
std::vector<Row> gamepad_rows() {
    std::vector<Row> rows = keyboard_rows();
    std::erase_if(rows, [](const Row& row) { return !controls_profile::on_gamepad(row.slot); });
    return rows;
}

// What is being edited.
int g_device = 0;  // 0: the keyboard and the mouse, 1: the gamepad that plays
int g_waiting_row = -1;
bool g_waiting_for_release = false;  // a gamepad's button that is still down from before
Uint64 g_wait_started = 0;
Uint64 g_stopped_at = 0;

bool gamepad_connected() { return PADGetIndexForPort(PORT) >= 0; }

// What the player chose, in controls.json beside settings.json: the launcher writes it too, so it is
// read again when the game starts, and what is changed here is written at once.
controls_profile::Profile g_profile;
bool g_profile_read = false;

std::filesystem::path profile_file() { return controls_profile::file_beside(player_folder::settings_file()); }

controls_profile::Profile& profile() {
    if (!g_profile_read) {
        g_profile = controls_profile::Profile::load(profile_file());
        g_profile_read = true;
    }
    return g_profile;
}

std::string gamepad_name() {
    const char* name = PADGetName(PORT);
    return name != nullptr ? name : "";
}

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

std::string key_name(s32 code) { return controls_profile::key_label(code); }

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
    if (row.slot == controls_profile::Slot::Turbo) {
        return g_device == 1 ? controls_profile::button_label(
                                   gamepad_type(), profile().button_or_default(gamepad_name(), gamepad_type(), row.slot))
                             : key_name(profile().key_or_default(row.slot));
    }
    if (g_device == 1) {
        return gamepad_label(row.button);
    }
    if (row.button != 0) {
        return key_name(key_of_button(row.button));
    }
    const PADKeyAxisBinding* binding = axis_binding(row.axis);
    return binding != nullptr ? key_name(binding->scancode) : text(Text::NoKey);
}

// Gives the key to the part on aurora's side (not saved).
void apply_key(const Row& row, s32 code) {
    if (row.button != 0) {
        PADSetKeyButtonBinding(PORT, {code, row.button});
    }
    for (const s32 axis : {row.axis, row.second_axis}) {
        if (axis >= 0) {
            const PADKeyAxisBinding* current = axis_binding(axis);
            PADSetKeyAxisBinding(PORT, {code, static_cast<PADAxis>(axis), current != nullptr ? current->influence : s16{0}});
        }
    }
}

// Gives the key to the part, and saves it.
void assign_key(size_t row, s32 code) {
    const Row part = keyboard_rows()[row];
    apply_key(part, code);
    profile().set_key(part.slot, code);
    profile().save(profile_file());
    PADSerializeMappings();
}

void assign_button(size_t row, s32 native) {
    const Row part = gamepad_rows()[row];
    if (part.button != 0) {
        PADSetButtonMapping(PORT, {static_cast<u32>(native), part.button});
    }
    profile().set_button(gamepad_name(), part.slot, static_cast<u32>(native));
    profile().save(profile_file());
    PADSerializeMappings();
}

void restore_defaults() {
    if (g_device == 1) {
        PADRestoreDefaultMapping(PORT);
        profile().clear_gamepad(gamepad_name());
    } else {
        PADRestoreDefaultKeyBindings(PORT);
        profile().clear_keyboard();
    }
    profile().save(profile_file());
    PADSerializeMappings();
}

// A gamepad's part waits for the next button, once the buttons that were down are let go.
void listen_to_gamepad() {
    const s32 pressed = PADGetNativeButtonPressed(PORT);
    if (g_waiting_for_release) {
        g_waiting_for_release = pressed >= 0;
        return;
    }
    if (pressed >= 0) {
        assign_button(static_cast<size_t>(g_waiting_row), pressed);
        stop_waiting();
    }
}

}  // namespace

void apply_saved() {
    // The keyboard once, and each gamepad when it is connected: what aurora kept by itself is
    // dropped, so that controls.json (which the launcher edits too) is the one that decides.
    static bool keyboard_done = false;
    static std::string gamepad_done;
    if (!keyboard_done) {
        keyboard_done = true;
        wake_keyboard_bindings();
        PADRestoreDefaultKeyBindings(PORT);
        const std::vector<Row> rows = keyboard_rows();
        for (size_t i = 0; i < rows.size(); ++i) {
            if (const auto key = profile().key(rows[i].slot)) {
                apply_key(rows[i], *key);
            }
        }
    }
    const std::string name = gamepad_name();
    if (!name.empty() && name != gamepad_done) {
        gamepad_done = name;
        PADRestoreDefaultMapping(PORT);
        const std::vector<Row> rows = gamepad_rows();
        for (size_t i = 0; i < rows.size(); ++i) {
            const auto native = profile().button(name, rows[i].slot);
            if (native && rows[i].button != 0) {
                PADSetButtonMapping(PORT, {*native, rows[i].button});
            }
        }
    }
}

bool turbo_held() {
    using controls_profile::Slot;
    const int key = profile().key_or_default(Slot::Turbo);
    bool held = false;
    if (key >= 0 && key < SDL_SCANCODE_COUNT) {
        held = SDL_GetKeyboardState(nullptr)[key];
    } else if (key <= controls_profile::MOUSE_LEFT) {
        constexpr int BUTTONS[] = {SDL_BUTTON_LEFT, SDL_BUTTON_MIDDLE, SDL_BUTTON_RIGHT, SDL_BUTTON_X1, SDL_BUTTON_X2};
        const int index = controls_profile::MOUSE_LEFT - key;
        held = index < 5 && (SDL_GetMouseState(nullptr, nullptr) & SDL_BUTTON_MASK(BUTTONS[index])) != 0;
    }
    if (!held && gamepad_connected()) {
        const auto native = profile().button(gamepad_name(), Slot::Turbo);
        SDL_Gamepad* gamepad = PADGetSDLGamepadForIndex(static_cast<u32>(PADGetIndexForPort(PORT)));
        if (native && gamepad != nullptr) {
            held = controls_profile::is_axis_button(*native)
                       ? SDL_GetGamepadAxis(gamepad, static_cast<SDL_GamepadAxis>(controls_profile::axis_of(*native))) > 16000
                       : SDL_GetGamepadButton(gamepad, static_cast<SDL_GamepadButton>(*native));
        }
    }
    return held;
}

bool busy() { return g_waiting_row >= 0 || SDL_GetTicks() - g_stopped_at < SETTLE_MS; }

void stop() {
    stop_waiting();
    g_device = 0;
}

bool handle_event(const SDL_Event& event) {
    if (g_waiting_row < 0 || g_device != 0) {
        return false;
    }
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
        assign_key(static_cast<size_t>(g_waiting_row), code);
    }
    stop_waiting();
    return true;
}

bool draw_window(bool* open) {
    const bool connected = gamepad_connected();
    if (!connected) {
        g_device = 0;
    }
    const std::vector<Row> rows = g_device == 1 ? gamepad_rows() : keyboard_rows();
    if (g_waiting_row >= static_cast<int>(rows.size()) || SDL_GetTicks() - g_wait_started > WAIT_LIMIT_MS) {
        stop_waiting();
    }
    if (g_waiting_row >= 0 && g_device == 1) {
        listen_to_gamepad();
    }

    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Appearing, ImVec2(0.5f, 0.5f));
    ImGui::SetNextWindowSize(ImVec2(theme::px(560), theme::px(600)), ImGuiCond_Appearing);
    const bool shown = ImGui::Begin(text(Text::ControllerSettings), open, ImGuiWindowFlags_NoSavedSettings);
    if (shown) {
        std::vector<std::string> devices = {text(Text::Keyboard)};
        if (connected) {
            const char* name = PADGetName(PORT);
            devices.push_back(name != nullptr ? name : text(Text::Controller));
        }
        ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
        if (ImGui::BeginCombo("##device", devices[static_cast<size_t>(g_device)].c_str())) {
            for (size_t i = 0; i < devices.size(); ++i) {
                if (ImGui::Selectable(devices[i].c_str(), static_cast<int>(i) == g_device)) {
                    stop_waiting();
                    g_device = static_cast<int>(i);
                }
            }
            ImGui::EndCombo();
        }
        ImGui::TextWrapped("%s", text(g_device == 1 ? Text::EditorIntroGamepad : Text::EditorIntroKeyboard));
        if (!connected) {
            ImGui::TextDisabled("%s", text(Text::EditorConnectGamepad));
        }
        ImGui::Spacing();
        const float footer = ImGui::GetFrameHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
        if (ImGui::BeginChild("##rows", ImVec2(0, -footer)) &&
            ImGui::BeginTable("##editor rows", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_SizingStretchProp)) {
            ImGui::TableSetupColumn("part", ImGuiTableColumnFlags_WidthStretch, 0.45f);
            ImGui::TableSetupColumn("key", ImGuiTableColumnFlags_WidthStretch, 0.55f);
            const bool settled = SDL_GetTicks() - g_stopped_at >= SETTLE_MS;
            for (size_t i = 0; i < rows.size(); ++i) {
                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(controls_profile::slot_name(rows[i].slot).c_str());
                ImGui::TableNextColumn();
                const bool waiting = g_waiting_row == static_cast<int>(i);
                const std::string label =
                    waiting ? text(g_device == 1 ? Text::PressButton : Text::PressKey) : binding_label(rows[i]);
                ImGui::PushID(static_cast<int>(i));
                if (waiting) {
                    ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));
                }
                if (ImGui::Button(label.c_str(), ImVec2(-FLT_MIN, 0)) && settled && !waiting) {
                    start_waiting(static_cast<int>(i));
                }
                if (waiting) {
                    ImGui::PopStyleColor();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::EndChild();
        if (ImGui::Button(text(Text::RestoreMapping))) {
            stop_waiting();
            restore_defaults();
        }
    }
    ImGui::End();
    return *open;
}

}  // namespace controls_editor
