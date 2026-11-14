// The controls the player chose: which key plays each part of the GameCube controller, and which
// button of each gamepad. They are kept in controls.json, beside settings.json, and both programs
// read and write it: the launcher's controls tab and the options menu inside the game (F10). Only
// what the player changed is in the file; a part that is not there plays with the game's own
// mapping (the keys of Dolphin, and the automatic mapping of each kind of gamepad).
//
// Los controles que eligió el jugador: qué tecla juega cada parte del control de GameCube y qué
// botón de cada gamepad. Se guardan en controls.json, junto a settings.json, y los dos programas lo
// leen y lo escriben: la pestaña de controles del launcher y el menú de opciones dentro del juego
// (F10). Solo está en el archivo lo que el jugador cambió; una parte que no está juega con la
// asignación del propio juego (las teclas de Dolphin y la asignación automática de cada gamepad).
#pragma once

#include <SDL3/SDL_gamepad.h>
#include <nlohmann/json.hpp>

#include <cstdint>
#include <filesystem>
#include <optional>
#include <string>

namespace controls_profile {

// The parts the player can change, in the order the screens list them. The first twelve are
// buttons (a gamepad can map those); the sticks are keys only (a gamepad's own sticks play them).
// The turbo goes on both.
enum class Slot {
    A, B, X, Y, Z, L, R, Start,
    DPadUp, DPadDown, DPadLeft, DPadRight,
    StickUp, StickDown, StickLeft, StickRight,
    CStickUp, CStickDown, CStickLeft, CStickRight,
    Turbo,  // not the GameCube's: held, the game runs faster (Tab by default, no button on a gamepad)
    Count
};
inline constexpr int SLOT_COUNT = static_cast<int>(Slot::Count);
inline constexpr int BUTTON_SLOT_COUNT = static_cast<int>(Slot::StickUp);

// What a key can be besides an SDL scancode (the same numbers as aurora's PAD_KEY_*).
inline constexpr int NO_KEY = -1;
inline constexpr int MOUSE_LEFT = -2;
inline constexpr int MOUSE_MIDDLE = -3;
inline constexpr int MOUSE_RIGHT = -4;
inline constexpr int MOUSE_X1 = -5;
inline constexpr int MOUSE_X2 = -6;

// What a gamepad's button is for aurora (PADSetButtonMapping): an SDL gamepad button, or a trigger
// read as a button (PADEncodeAxisButton). NO_BUTTON leaves the part without one.
inline constexpr std::uint32_t NO_BUTTON = 0xfffffffeu;
constexpr std::uint32_t axis_as_button(int axis) { return 0x10000u | static_cast<std::uint32_t>(axis) | (50u << 8); }
constexpr bool is_axis_button(std::uint32_t native) { return (native & 0xffff0000u) == 0x10000u; }
constexpr int axis_of(std::uint32_t native) { return static_cast<int>(native & 0x7fu); }

// Whether a gamepad can play the part (the buttons and the turbo, not the sticks).
constexpr bool on_gamepad(Slot slot) { return static_cast<int>(slot) < BUTTON_SLOT_COUNT || slot == Slot::Turbo; }

// How the part is called on the screens ("A", "Control Stick up", ...).
std::string slot_name(Slot slot);
// The key the game gives the part by itself.
int default_key(Slot slot);
// The button a gamepad of this kind gives a button part by itself (a Slot below BUTTON_SLOT_COUNT).
std::uint32_t default_button(SDL_GamepadType type, Slot slot);

// What a key, or a gamepad's button, is called for the player.
std::string key_label(int key);
std::string button_label(SDL_GamepadType type, std::uint32_t native);

class Profile {
public:
    static Profile load(const std::filesystem::path& file);
    // Writes the file (and its folder); false when it could not.
    bool save(const std::filesystem::path& file) const;

    // What the player chose for a part, or nothing when it plays with the game's mapping.
    std::optional<int> key(Slot slot) const;
    std::optional<std::uint32_t> button(const std::string& gamepad, Slot slot) const;
    void set_key(Slot slot, int key);
    void set_button(const std::string& gamepad, Slot slot, std::uint32_t native);
    // Back to the game's mapping, for the keyboard or for one gamepad.
    void clear_keyboard();
    void clear_gamepad(const std::string& gamepad);

    // What plays the part now: the player's choice, or else the game's.
    int key_or_default(Slot slot) const;
    std::uint32_t button_or_default(const std::string& gamepad, SDL_GamepadType type, Slot slot) const;

private:
    nlohmann::json document_ = nlohmann::json::object();
};

// The file next to settings.json.
std::filesystem::path file_beside(const std::filesystem::path& settings_file);

}  // namespace controls_profile
