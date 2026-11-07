// What runs on the audio DSP: its boot ROM, and the programs the ROM loads for the game.
//
// Lo que corre en el DSP de audio: su ROM de arranque y los programas que carga para el
// juego.
//
// The DSP runs code of its own, which the runtime does not execute: it recognizes each
// program by a hash of its code, as Dolphin does, and runs a version of it written in C++
// (ax_microcode.h for AX, the only one this game loads). After a reset the boot ROM greets
// the CPU (mail 0x8071FEED) and waits for a task: the CPU's DSP library sends, in pairs
// of mails, where the program's code is in main memory, its length, where it goes in the
// DSP's memory, the length of its data, and the address to start at (0x80F3xxxx, then
// the value). With the last one the program starts.
//
// Mails from the CPU arrive here (dsp_interface.h passes them on); the programs answer
// with dsp_interface::send_mail_to_cpu.
#pragma once

#include <cstdint>

namespace dsp_programs {

// The DSP was reset: its boot ROM runs and greets the CPU.
void reset();

// A mail from the CPU, for whatever runs now.
void receive_mail(uint32_t mail);

// The running program hands the DSP back to the boot ROM (AX's "reset" mail).
void return_to_boot_rom();

}  // namespace dsp_programs
