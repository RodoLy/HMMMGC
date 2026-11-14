// See controls_profile.h.

#include "controls_profile.h"

#include "controls.h"
#include "texts.h"

#include <SDL3/SDL_keyboard.h>

#include <fstream>
#include <system_error>

namespace controls_profile {
namespace {

using controls::Part;
using texts::Text;
using texts::text;

constexpr const char* KEYBOARD = "keyboard";
constexpr const char* GAMEPADS = "gamepads";

// The names in the file, in the order of Slot.
constexpr const char* NAMES[SLOT_COUNT] = {
    "a",       "b",         "x",         "y",          "z",          "l",           "r",
    "start",   "dpad_up",   "dpad_down", "dpad_left",  "dpad_right", "stick_up",    "stick_down",
    "stick_left", "stick_right", "cstick_up", "cstick_down", "cstick_left", "cstick_right",
};

// The keys of Dolphin, which the runtime gives the keyboard (runtime/src/input/pad_natives.cpp).
constexpr SDL_Scancode DEFAULT_KEYS[SLOT_COUNT] = {
    SDL_SCANCODE_X,    SDL_SCANCODE_Z,     SDL_SCANCODE_C,    SDL_SCANCODE_S,    SDL_SCANCODE_D,
    SDL_SCANCODE_Q,    SDL_SCANCODE_W,     SDL_SCANCODE_RETURN,
    SDL_SCANCODE_T,    SDL_SCANCODE_G,     SDL_SCANCODE_F,    SDL_SCANCODE_H,
    SDL_SCANCODE_UP,   SDL_SCANCODE_DOWN,  SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
    SDL_SCANCODE_I,    SDL_SCANCODE_K,     SDL_SCANCODE_J,    SDL_SCANCODE_L,
};

const char* name_of(Slot slot) { return NAMES[static_cast<int>(slot)]; }

Part part_of(Slot slot) {
    switch (slot) {
    case Slot::A: return Part::A;
    case Slot::B: return Part::B;
    case Slot::X: return Part::X;
    case Slot::Y: return Part::Y;
    case Slot::Z: return Part::Z;
    case Slot::L: return Part::L;
    case Slot::R: return Part::R;
    case Slot::Start: return Part::Start;
    case Slot::DPadUp:
    case Slot::DPadDown:
    case Slot::DPadLeft:
    case Slot::DPadRight: return Part::DPad;
    case Slot::StickUp:
    case Slot::StickDown:
    case Slot::StickLeft:
    case Slot::StickRight: return Part::Stick;
    default: return Part::CStick;
    }
}

Text direction_of(Slot slot) {
    switch (static_cast<int>(slot)) {
    case static_cast<int>(Slot::DPadUp):
    case static_cast<int>(Slot::StickUp):
    case static_cast<int>(Slot::CStickUp): return Text::DirectionUp;
    case static_cast<int>(Slot::DPadDown):
    case static_cast<int>(Slot::StickDown):
    case static_cast<int>(Slot::CStickDown): return Text::DirectionDown;
    case static_cast<int>(Slot::DPadLeft):
    case static_cast<int>(Slot::StickLeft):
    case static_cast<int>(Slot::CStickLeft): return Text::DirectionLeft;
    default: return Text::DirectionRight;
    }
}

}  // namespace

std::string slot_name(Slot slot) {
    if (slot <= Slot::Start) {
        return controls::part_name(part_of(slot));
    }
    return controls::part_name(part_of(slot)) + " " + text(direction_of(slot));
}

int default_key(Slot slot) { return static_cast<int>(DEFAULT_KEYS[static_cast<int>(slot)]); }

std::uint32_t default_button(SDL_GamepadType type, Slot slot) {
    switch (slot) {
    case Slot::A:
    case Slot::B:
    case Slot::X:
    case Slot::Y: return static_cast<std::uint32_t>(controls::face_button(type, part_of(slot)));
    case Slot::Z: return SDL_GAMEPAD_BUTTON_RIGHT_SHOULDER;
    case Slot::L: return axis_as_button(SDL_GAMEPAD_AXIS_LEFT_TRIGGER);
    case Slot::R: return axis_as_button(SDL_GAMEPAD_AXIS_RIGHT_TRIGGER);
    case Slot::Start: return SDL_GAMEPAD_BUTTON_START;
    case Slot::DPadUp: return SDL_GAMEPAD_BUTTON_DPAD_UP;
    case Slot::DPadDown: return SDL_GAMEPAD_BUTTON_DPAD_DOWN;
    case Slot::DPadLeft: return SDL_GAMEPAD_BUTTON_DPAD_LEFT;
    case Slot::DPadRight: return SDL_GAMEPAD_BUTTON_DPAD_RIGHT;
    default: return NO_BUTTON;
    }
}

std::string key_label(int key) {
    switch (key) {
    case NO_KEY: return text(Text::NoKey);
    case MOUSE_LEFT: return text(Text::MouseLeft);
    case MOUSE_MIDDLE: return text(Text::MouseMiddle);
    case MOUSE_RIGHT: return text(Text::MouseRight);
    case MOUSE_X1:
    case MOUSE_X2: return text(Text::MouseOther);
    default: break;
    }
    if (key == SDL_SCANCODE_RETURN) {
        return "Enter";
    }
    const char* name = SDL_GetScancodeName(static_cast<SDL_Scancode>(key));
    return name != nullptr && *name != '\0' ? name : text(Text::NoKey);
}

std::string button_label(SDL_GamepadType type, std::uint32_t native) {
    if (native == NO_BUTTON || native == static_cast<std::uint32_t>(-1)) {
        return text(Text::NoKey);
    }
    if (is_axis_button(native)) {
        return controls::native_axis_label(type, axis_of(native));
    }
    return controls::native_button_label(type, static_cast<int>(native));
}

Profile Profile::load(const std::filesystem::path& file) {
    Profile profile;
    std::ifstream stream(file, std::ios::binary);
    if (stream) {
        // A file that cannot be read (damaged, or written by hand with a mistake) reads as empty:
        // the game's own mapping plays.
        nlohmann::json parsed = nlohmann::json::parse(stream, nullptr, false);
        if (parsed.is_object()) {
            profile.document_ = std::move(parsed);
        }
    }
    return profile;
}

bool Profile::save(const std::filesystem::path& file) const {
    std::error_code error;
    std::filesystem::create_directories(file.parent_path(), error);
    // Written beside it and then moved over it, so that a failed write never leaves half a file.
    const std::filesystem::path partial = file.parent_path() / (file.filename().string() + ".new");
    {
        std::ofstream stream(partial, std::ios::binary | std::ios::trunc);
        if (!stream) {
            return false;
        }
        stream << document_.dump(2) << "\n";
        if (!stream) {
            return false;
        }
    }
    std::filesystem::rename(partial, file, error);
    return !error;
}

std::optional<int> Profile::key(Slot slot) const {
    const auto section = document_.find(KEYBOARD);
    if (section == document_.end() || !section->is_object()) {
        return std::nullopt;
    }
    const auto found = section->find(name_of(slot));
    if (found == section->end() || !found->is_number_integer()) {
        return std::nullopt;
    }
    return found->get<int>();
}

std::optional<std::uint32_t> Profile::button(const std::string& gamepad, Slot slot) const {
    const auto section = document_.find(GAMEPADS);
    if (section == document_.end() || !section->is_object()) {
        return std::nullopt;
    }
    const auto device = section->find(gamepad);
    if (device == section->end() || !device->is_object()) {
        return std::nullopt;
    }
    const auto found = device->find(name_of(slot));
    if (found == device->end() || !found->is_number_unsigned()) {
        return std::nullopt;
    }
    return found->get<std::uint32_t>();
}

void Profile::set_key(Slot slot, int key) { document_[KEYBOARD][name_of(slot)] = key; }

void Profile::set_button(const std::string& gamepad, Slot slot, std::uint32_t native) {
    document_[GAMEPADS][gamepad][name_of(slot)] = native;
}

void Profile::clear_keyboard() { document_.erase(KEYBOARD); }

void Profile::clear_gamepad(const std::string& gamepad) {
    const auto section = document_.find(GAMEPADS);
    if (section != document_.end() && section->is_object()) {
        section->erase(gamepad);
    }
}

int Profile::key_or_default(Slot slot) const { return key(slot).value_or(default_key(slot)); }

std::uint32_t Profile::button_or_default(const std::string& gamepad, SDL_GamepadType type, Slot slot) const {
    return button(gamepad, slot).value_or(default_button(type, slot));
}

std::filesystem::path file_beside(const std::filesystem::path& settings_file) {
    return settings_file.parent_path() / "controls.json";
}

}  // namespace controls_profile
