// The part of the game's operating system (Nintendo's "Dolphin OS") that has to be
// native: turning interrupts on and off, taking interrupts, and the thread scheduler's
// switch and idle wait.
//
// La parte del sistema operativo del juego que tiene que ser nativa: encender y apagar
// las interrupciones, atenderlas, y el cambio de hilo y la espera ociosa del planificador.
//
// On the console an interrupt stops the CPU between two instructions and jumps to the
// OS. Translated code cannot be stopped at any instruction, so interrupts are taken at
// the points where the game can be interrupted on purpose: when it turns interrupts back
// on (OSEnableInterrupts, OSRestoreInterrupts) and while the scheduler waits for one
// (SelectThread with nothing to run). The game's own interrupt handlers then run as on
// the console, translated.
//
// This game creates no threads of its own (it never calls OSCreateThread): the OS only
// has the main thread and its idle loop, so a thread switch is always a return to the
// same thread and needs no second host stack.
#pragma once

struct CpuContext;

namespace guest_os {

// Takes the interrupts and exceptions that are due, if the guest has them enabled.
void take_pending_interrupts(CpuContext* cpu);

}  // namespace guest_os
