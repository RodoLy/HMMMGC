// The audio interface (AI, 0xCC006C00): the sample counter of the disc-audio stream,
// which the game's audio code measures (__AI_SRC_INIT) and can ask to interrupt it at a
// given count, and the sample rate of the sound the DSP makes, which the audio DMA plays
// (dsp_interface.h). The game does not stream audio from the disc.
//
// La interfaz de audio (AI): el contador de muestras del audio del disco, que el código
// de audio del juego mide y al que puede pedir una interrupción, y la frecuencia del
// sonido que hace el DSP, que toca el DMA de audio. El juego no toca audio del disco.
#pragma once

#include <cstdint>

namespace audio_interface {

inline constexpr uint32_t BASE = 0xCC006C00u;

// Register accesses of 4 bytes at `address`.
void write(uint32_t address, uint32_t value);
void refresh(uint32_t address);

// Raises the interrupt if the counter has passed the programmed value.
void advance();

// The rate of the DSP's sound in samples per second: 32000 or 48000, as the game chose
// (AISetDSPSampleRate).
uint32_t dsp_sample_rate();

}  // namespace audio_interface
