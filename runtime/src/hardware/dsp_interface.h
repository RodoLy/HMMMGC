// The DSP interface (0xCC005000): the mailboxes between the CPU and the audio DSP, the
// DSP's control register, and the DMA engine between main memory and the audio memory
// (ARAM).
//
// La interfaz del DSP: los buzones entre el CPU y el DSP de audio, el registro de control
// del DSP, y el DMA entre la memoria principal y la memoria de audio (ARAM).
//
// What runs on the DSP is in audio/dsp_programs.h: the mails of the CPU go there, and the
// DSP's programs answer through send_mail_to_cpu. The audio DMA, which plays the sound
// that the DSP leaves in main memory, is not modelled yet: no sound comes out.
#pragma once

#include <cstdint>

namespace dsp_interface {

inline constexpr uint32_t BASE = 0xCC005000u;

// Register accesses of 2 bytes at `address`.
void write(uint32_t address, uint16_t value);
void refresh(uint32_t address);

// A mail from the DSP to the CPU. With `interrupt`, the DSP interrupt goes with it, as for
// the mails the DSP library has to act on.
void send_mail_to_cpu(uint32_t mail, bool interrupt);

}  // namespace dsp_interface
