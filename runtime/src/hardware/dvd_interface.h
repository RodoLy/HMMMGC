// The DVD interface (DI, 0xCC006000): the disc drive. A read command copies the bytes
// from the disc (game_disc.h) into memory at once and interrupts, as the drive would when
// done.
//
// La interfaz de DVD (DI): la unidad de disco. Un comando de lectura copia los bytes del
// disco a la memoria de inmediato e interrumpe, como haría la unidad al terminar.
#pragma once

#include <cstdint>

class GameDisc;

namespace dvd_interface {

inline constexpr uint32_t BASE = 0xCC006000u;

// `disc` must outlive the runtime.
void initialize(const GameDisc& disc);

// Register accesses of 4 bytes at `address`.
void write(uint32_t address, uint32_t value);
void refresh(uint32_t address);

uint64_t commands_run();

}  // namespace dvd_interface
