// The controls screen's knowledge: the PC's gamepads, how the game maps each one to a GameCube
// controller (aurora's automatic mapping, by the kind of gamepad), the keyboard's keys (the
// runtime's, runtime/src/input/pad_natives.cpp), and a GameCube controller drawn with what is
// pressed lit up.
//
// Lo que sabe la pantalla de controles: los gamepads de la PC, cómo los asigna el juego a un
// control de GameCube, las teclas del teclado y un control de GameCube dibujado que se ilumina
// con lo que se presiona.
#pragma once

#include <SDL3/SDL_gamepad.h>
#include <imgui.h>

#include <array>
#include <map>
#include <string>
#include <vector>

namespace controls {

// The parts of a GameCube controller, in the order the screen lists them.
enum class Part { A, B, X, Y, Z, L, R, Start, Stick, CStick, DPad, Count };
inline constexpr size_t PART_COUNT = static_cast<size_t>(Part::Count);

struct Device {
    SDL_JoystickID id = 0;  // 0 is the keyboard
    std::string name;
    SDL_GamepadType type = SDL_GAMEPAD_TYPE_UNKNOWN;
};

// What is pressed right now, as GameCube parts.
struct State {
    std::array<bool, PART_COUNT> pressed{};
    ImVec2 stick{};   // -1 to 1, y up
    ImVec2 c_stick{};
    std::array<bool, 4> d_pad{};  // up, down, left, right
};

class Gamepads {
public:
    ~Gamepads();
    // The gamepads connected now (opened, to read them), then the keyboard.
    std::vector<Device> devices();
    State state(const Device& device);
    // Closes the gamepads (when the screen is left).
    void close();

private:
    std::map<SDL_JoystickID, SDL_Gamepad*> open_;
};

// The GameCube part's name ("A", "Z", "Palanca C", ...).
std::string part_name(Part part);
// What plays that part on `device`: a button's label ("A", "✕", "RB"), a stick, or keys.
std::string binding(const Device& device, Part part);
// The GameCube controller, drawn in the rectangle from `min`, `size` wide and tall, with the
// parts of `state` lit up.
void draw_controller(ImDrawList* draw, ImVec2 min, ImVec2 size, const State& state);

// The controls screen of the launcher and of the options menu inside the game, in the space left:
// the controller chosen among the gamepads and the keyboard (`selected_device`), whether the game
// maps it by itself, the controller drawn with what is pressed lit up, and what plays each part.
// `focus` puts the keyboard's and the controller's focus on the choice of the controller.
void draw_screen(Gamepads& gamepads, int& selected_device, bool focus);

}  // namespace controls
