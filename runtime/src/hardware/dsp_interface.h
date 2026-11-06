// The DSP interface (0xCC005000): the mailboxes between the CPU and the audio DSP, the
// DSP's control register, and the DMA engine between main memory and the audio memory
// (ARAM).
//
// La interfaz del DSP: los buzones entre el CPU y el DSP de audio, el registro de control
// del DSP, y el DMA entre la memoria principal y la memoria de audio (ARAM).
//
// There is no DSP program yet: the DSP only sends the greetings of Nintendo's boot ROM
// and of the OS's start-up program, which is what OSInit waits for. No sound comes out.
#pragma once

#include <cstdint>

namespace dsp_interface {

inline constexpr uint32_t BASE = 0xCC005000u;

// Register accesses of 2 bytes at `address`.
void write(uint32_t address, uint16_t value);
void refresh(uint32_t address);

}  // namespace dsp_interface
