// The DSP interface (0xCC005000): the mailboxes between the CPU and the audio DSP, the
// DSP's control register, the DMA engine between main memory and the audio memory
// (ARAM), and the audio DMA, which plays the sound the DSP leaves in main memory.
//
// La interfaz del DSP: los buzones entre el CPU y el DSP de audio, el registro de control
// del DSP, el DMA entre la memoria principal y la memoria de audio (ARAM), y el DMA de
// audio, que toca el sonido que el DSP deja en la memoria principal.
//
// What runs on the DSP is in audio/dsp_programs.h: the mails of the CPU go there, and the
// DSP's programs answer through send_mail_to_cpu. The audio DMA hands what it plays to
// audio/audio_output.h.
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

// Plays the blocks of the audio DMA that came due by the current time base, raising its
// interrupt each time a buffer starts; and tells when the buffer playing now ends
// (UINT64_MAX while the audio DMA is off).
void advance();
uint64_t next_audio_buffer_time();

}  // namespace dsp_interface
