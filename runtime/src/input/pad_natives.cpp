// The game's controller functions (PAD), replaced by aurora's, which read the PC's
// gamepads (through SDL) and its keyboard. Addresses from config/G4AEE9/symbols.txt.
//
// Las funciones de control del juego (PAD), reemplazadas por las de aurora, que leen los
// gamepads de la PC (por medio de SDL) y su teclado.
//
// On the console PADRead returns what the serial interface (SI) last polled from the four
// controller ports. aurora gives the same PADStatus from SDL: a gamepad connected to the
// PC plays as controller 1, and while there is none the keyboard does, with Dolphin's
// default keys, so that a player who knows Dolphin finds them where they expect them:
//
//   Control Stick: arrow keys   C Stick: I J K L   D-Pad: T F G H
//   A: X   B: Z   X: C   Y: S   Z: D   L: Q   R: W   START: Enter

#include "guest_memory.h"
#include "native_functions.h"

#include <SDL3/SDL_scancode.h>
#include <dolphin/pad.h>

#include <cstdint>

namespace {

// The port that the keyboard plays on while no gamepad is connected to it.
constexpr uint32_t KEYBOARD_PORT = PAD_CHAN0;

// A PADStatus as the game lays it out: 12 bytes, big-endian (aurora's has one more field,
// so each one is copied by itself).
constexpr uint32_t GUEST_STATUS_SIZE = 12;
constexpr uint32_t BUTTONS_OFFSET = 0x0;        // u16
constexpr uint32_t STICK_X_OFFSET = 0x2;        // s8
constexpr uint32_t STICK_Y_OFFSET = 0x3;        // s8
constexpr uint32_t C_STICK_X_OFFSET = 0x4;      // s8
constexpr uint32_t C_STICK_Y_OFFSET = 0x5;      // s8
constexpr uint32_t TRIGGER_LEFT_OFFSET = 0x6;   // u8
constexpr uint32_t TRIGGER_RIGHT_OFFSET = 0x7;  // u8
constexpr uint32_t ANALOG_A_OFFSET = 0x8;       // u8
constexpr uint32_t ANALOG_B_OFFSET = 0x9;       // u8
constexpr uint32_t ERROR_OFFSET = 0xA;          // s8: 0, or PAD_ERR_NO_CONTROLLER and the like

// Dolphin's default keyboard layout for a GameCube controller (Core/HW/GCPadEmu.cpp,
// GCPad::LoadDefaults). aurora wants a key for every button and axis; L and R are both
// the digital click and the analog trigger. (aurora moves an axis all the way while its
// key is held.)
constexpr PADDefaultKeyBindings DOLPHIN_KEYS = {
    .buttons =
        {
            {SDL_SCANCODE_X, PAD_BUTTON_A},
            {SDL_SCANCODE_Z, PAD_BUTTON_B},
            {SDL_SCANCODE_C, PAD_BUTTON_X},
            {SDL_SCANCODE_S, PAD_BUTTON_Y},
            {SDL_SCANCODE_RETURN, PAD_BUTTON_START},
            {SDL_SCANCODE_D, PAD_TRIGGER_Z},
            {SDL_SCANCODE_Q, PAD_TRIGGER_L},
            {SDL_SCANCODE_W, PAD_TRIGGER_R},
            {SDL_SCANCODE_T, PAD_BUTTON_UP},
            {SDL_SCANCODE_G, PAD_BUTTON_DOWN},
            {SDL_SCANCODE_F, PAD_BUTTON_LEFT},
            {SDL_SCANCODE_H, PAD_BUTTON_RIGHT},
        },
    .axes =
        {
            {SDL_SCANCODE_RIGHT, PAD_AXIS_LEFT_X_POS, 0},
            {SDL_SCANCODE_LEFT, PAD_AXIS_LEFT_X_NEG, 0},
            {SDL_SCANCODE_UP, PAD_AXIS_LEFT_Y_POS, 0},
            {SDL_SCANCODE_DOWN, PAD_AXIS_LEFT_Y_NEG, 0},
            {SDL_SCANCODE_L, PAD_AXIS_RIGHT_X_POS, 0},
            {SDL_SCANCODE_J, PAD_AXIS_RIGHT_X_NEG, 0},
            {SDL_SCANCODE_I, PAD_AXIS_RIGHT_Y_POS, 0},
            {SDL_SCANCODE_K, PAD_AXIS_RIGHT_Y_NEG, 0},
            {SDL_SCANCODE_Q, PAD_AXIS_TRIGGER_L, 0},
            {SDL_SCANCODE_W, PAD_AXIS_TRIGGER_R, 0},
        },
};

// aurora reads a port either from the keyboard or from its gamepad, never both: the
// keyboard plays only while no gamepad is connected to its port. Checked at every read,
// so a gamepad can be plugged in or out at any moment.
void choose_keyboard_or_gamepad() {
    const bool gamepad_connected = PADGetIndexForPort(KEYBOARD_PORT) >= 0;
    PADSetKeyboardActive(KEYBOARD_PORT, gamepad_connected ? FALSE : TRUE);
}

void write_status(uint32_t guest_address, const PADStatus& status) {
    guest_memory::write16(guest_address + BUTTONS_OFFSET, status.button);
    guest_memory::write8(guest_address + STICK_X_OFFSET, static_cast<uint8_t>(status.stickX));
    guest_memory::write8(guest_address + STICK_Y_OFFSET, static_cast<uint8_t>(status.stickY));
    guest_memory::write8(guest_address + C_STICK_X_OFFSET, static_cast<uint8_t>(status.substickX));
    guest_memory::write8(guest_address + C_STICK_Y_OFFSET, static_cast<uint8_t>(status.substickY));
    guest_memory::write8(guest_address + TRIGGER_LEFT_OFFSET, status.triggerLeft);
    guest_memory::write8(guest_address + TRIGGER_RIGHT_OFFSET, status.triggerRight);
    guest_memory::write8(guest_address + ANALOG_A_OFFSET, status.analogA);
    guest_memory::write8(guest_address + ANALOG_B_OFFSET, status.analogB);
    guest_memory::write8(guest_address + ERROR_OFFSET, static_cast<uint8_t>(status.err));
}

// BOOL PADInit(void). The keyboard layout has to be given before aurora's PADInit, which
// takes it as the starting layout.
uint32_t PADInitNative() {
    PADSetDefaultKeyBindings(KEYBOARD_PORT, &DOLPHIN_KEYS);
    const BOOL initialized = PADInit();
    choose_keyboard_or_gamepad();
    return static_cast<uint32_t>(initialized);
}
PPC_NATIVE_OVERRIDE(8017C890, PADInitNative, uint32_t, (), ());

// u32 PADRead(PADStatus status[4]): the four ports, in the game's array; returns the ports
// whose controller can rumble, as the console's PADRead does.
uint32_t PADReadNative(uint32_t status_address) {
    choose_keyboard_or_gamepad();
    PADStatus statuses[PAD_CHANMAX]{};
    const uint32_t rumble_ports = PADRead(statuses);
    for (uint32_t port = 0; port < PAD_CHANMAX; ++port) {
        write_status(status_address + port * GUEST_STATUS_SIZE, statuses[port]);
    }
    return rumble_ports;
}
PPC_NATIVE_OVERRIDE(8017C9E0, PADReadNative, uint32_t, (uint32_t status_address), (status_address));

// BOOL PADReset(u32 mask) and BOOL PADRecalibrate(u32 mask): on the console they ask the
// controllers in `mask` to identify themselves again (and to take their current stick
// positions as the centre). SDL keeps the gamepads ready, so there is nothing to do.
uint32_t PADResetNative(uint32_t port_mask) { return static_cast<uint32_t>(PADReset(port_mask)); }
PPC_NATIVE_OVERRIDE(8017C66C, PADResetNative, uint32_t, (uint32_t port_mask), (port_mask));

uint32_t PADRecalibrateNative(uint32_t port_mask) { return static_cast<uint32_t>(PADRecalibrate(port_mask)); }
PPC_NATIVE_OVERRIDE(8017C77C, PADRecalibrateNative, uint32_t, (uint32_t port_mask), (port_mask));

// void PADControlMotor(s32 port, u32 command): starts or stops the rumble of a controller
// (PAD_MOTOR_RUMBLE, PAD_MOTOR_STOP or PAD_MOTOR_STOP_HARD).
void PADControlMotorNative(uint32_t port, uint32_t command) { PADControlMotor(port, command); }
PPC_NATIVE_OVERRIDE_VOID(8017CCE0, PADControlMotorNative, (uint32_t port, uint32_t command), (port, command));

}  // namespace
