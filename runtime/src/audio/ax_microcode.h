// AX's DSP program, written in C++: what the DSP does for the GameCube's sound library.
//
// El programa de AX para el DSP, escrito en C++: lo que hace el DSP para la biblioteca de
// sonido del GameCube.
//
// Ported from Dolphin's high-level emulation of AX (Source/Core/Core/HW/DSPHLE/UCodes/
// AX.cpp and AX.h; Copyright 2008 Dolphin Emulator Project, GPL-2.0-or-later).
//
// Every 5 ms, when the audio DMA starts playing a buffer, AX on the CPU sends the DSP a
// command list for the next one (two mails: 0xBABE and the list's length, then its
// address). The commands clear the mixing buffers, play the voice list (ax_voice.h), pass
// the auxiliary sends to the CPU's effects and take them back, and write the final
// stereo samples to the buffer that the audio DMA will play next. The DSP then yields
// (mail 0xDCD10002, with an interrupt) and waits for the CPU's next mail.
#pragma once

#include <cstdint>

namespace ax_microcode {

// Starts the program that the DSP's boot ROM loaded; `code_hash` (Dolphin's hash of the
// program's code) tells its version. Sends the CPU the program's first mail.
void start(uint32_t code_hash);

// A mail from the CPU.
void receive_mail(uint32_t mail);

// Whether `code_hash` is a version of AX for the GameCube that this file plays.
bool is_known_version(uint32_t code_hash);

}  // namespace ax_microcode
