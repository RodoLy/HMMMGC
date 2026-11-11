// The serial interface (SI, 0xCC006400): the four controller ports. No controller is
// plugged in yet, so every transfer ends with "no response".
//
// La interfaz serie (SI): los cuatro puertos de control. Todavía no hay ningún control
// conectado, así que cada transferencia termina con "sin respuesta".
#pragma once

#include <cstdint>

namespace serial_interface {

inline constexpr uint32_t BASE = 0xCC006400u;

// Register accesses of 4 bytes at `address`.
void write(uint32_t address, uint32_t value);

}  // namespace serial_interface
