// The processor interface (PI, 0xCC003000): the console's interrupt controller, and the
// CPU's end of the graphics FIFO.
//
// La interfaz del procesador (PI): el controlador de interrupciones de la consola, y el
// extremo del CPU del FIFO de gráficos.
#pragma once

#include <cstddef>
#include <cstdint>

namespace processor_interface {

inline constexpr uint32_t BASE = 0xCC003000u;

// Interrupt causes: bits of the PI's cause register (INTSR), as Dolphin names them in
// its ProcessorInterface.h. Each device raises its own.
enum InterruptCause : uint32_t {
    DVD_INTERRUPT = 0x004u,
    SERIAL_INTERRUPT = 0x008u,
    EXTERNAL_INTERRUPT = 0x010u,
    AUDIO_INTERRUPT = 0x020u,
    DSP_INTERRUPT = 0x040u,
    VIDEO_INTERRUPT = 0x100u,
    PIXEL_ENGINE_TOKEN_INTERRUPT = 0x200u,
    PIXEL_ENGINE_FINISH_INTERRUPT = 0x400u,
};

void initialize();

// A device's interrupt line: the cause stays pending while it is asserted.
void set_interrupt(uint32_t cause, bool asserted);

// The causes pending now, and the ones the game has unmasked (INTMR).
uint32_t pending_interrupts();
uint32_t enabled_interrupts();

// Register accesses of 4 bytes at `address`.
void write(uint32_t address, uint32_t value);
void refresh(uint32_t address);

// Stores graphics commands that reached the write-gather pipe in the CPU FIFO, the ring
// in main memory that the game set up with GXSetCPUFifo.
void append_to_fifo(const uint8_t* bytes, size_t size);

}  // namespace processor_interface
