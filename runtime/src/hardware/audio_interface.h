// The audio interface (AI, 0xCC006C00): the sample counter of the disc-audio stream,
// which the game's audio code measures (__AI_SRC_INIT) and can ask to interrupt it at a
// given count. No sound comes out yet.
//
// La interfaz de audio (AI): el contador de muestras del audio del disco, que el código
// de audio del juego mide y al que puede pedir una interrupción. Todavía no sale sonido.
#pragma once

#include <cstdint>

namespace audio_interface {

inline constexpr uint32_t BASE = 0xCC006C00u;

// Register accesses of 4 bytes at `address`.
void write(uint32_t address, uint32_t value);
void refresh(uint32_t address);

// Raises the interrupt if the counter has passed the programmed value.
void advance();

}  // namespace audio_interface
