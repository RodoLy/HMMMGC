// The audio memory (ARAM): 16 MiB of memory attached to the DSP, where the game keeps the
// samples of its sounds and music. The CPU reaches it only through the DMA engine of the
// DSP interface (dsp_interface.h); the DSP reads samples from it through its accelerator
// (audio/ax_voice.h).
//
// La memoria de audio (ARAM): 16 MiB unidos al DSP, donde el juego guarda las muestras
// de sus sonidos y su música.
#pragma once

#include <cstdint>

namespace audio_memory {

inline constexpr uint32_t SIZE = 16u * 1024u * 1024u;

// The whole memory, SIZE bytes, in the console's byte order.
uint8_t* data();

// One byte; addresses wrap around the 16 MiB, as on a console without expansion memory.
inline uint8_t read8(uint32_t address) { return data()[address & (SIZE - 1)]; }

}  // namespace audio_memory
