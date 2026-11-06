// The video interface (VI, 0xCC002000): the timing of the TV signal. Its display
// interrupts, once per field, are what the game's frame loop waits for
// (VIWaitForRetrace).
//
// La interfaz de video (VI): el tiempo de la señal de TV. Sus interrupciones de pantalla,
// una por campo, son lo que espera el bucle de cuadros del juego.
#pragma once

#include <cstdint>

namespace video_interface {

inline constexpr uint32_t BASE = 0xCC002000u;

// Register accesses of 2 bytes at `address`.
void write(uint32_t address, uint16_t value);
void refresh(uint32_t address);

// Raises the display interrupts of the fields that have passed by now, and tells when
// the next field starts (in time-base ticks) and how many have been shown.
void advance();
uint64_t next_field_time();
uint64_t fields_shown();

}  // namespace video_interface
