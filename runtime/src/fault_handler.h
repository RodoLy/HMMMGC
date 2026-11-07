// What the runtime does when the translated game touches memory it should not, and how
// it sees the game's reads of hardware registers through plain pointers.
//
// Lo que hace el runtime cuando el juego traducido toca memoria que no debe, y cómo ve
// las lecturas de registros de hardware que el juego hace con punteros comunes.
//
// Most reads of hardware registers use a constant address, which the translator turns
// into a call of the runtime (see hardware_registers.h). A read through a pointer is a
// plain host load instead, from the guest view of the registers. That view is kept
// inaccessible, so the load faults; the handler then writes the register's current value
// into the view, lets that one instruction run with the page readable (the processor's
// single-step trap), and closes the page again. Windows on ARM64 has no single-step for
// a program to use, so there the handler does the load itself: it decodes the faulting
// instruction, reads the register, and writes the result to the instruction's
// destination register. These are aids of the port in progress: a finished runtime
// replaces the functions of the game that read registers this way, as WiiCompiled does,
// instead of trapping their loads.
#pragma once

#include <cstddef>
#include <cstdint>

namespace fault_handler {

// Installs the handlers for invalid accesses, trapped register reads, aborts, and a
// watchdog that prints where the game is every `watchdog_seconds` (0: no watchdog).
void install(unsigned watchdog_seconds);

// Prints what the game was doing (its function, the guest registers that tell where it
// came from, and the host call stack) and stops the program.
[[noreturn]] void report_invalid_access(const char* kind, uint32_t guest_address, size_t size);

// Same report for any other fatal condition of the runtime.
[[noreturn]] void report_fatal(const char* reason);

// Writes the guest backtrace part of the report (also used by the watchdog).
void print_guest_location();

}  // namespace fault_handler
