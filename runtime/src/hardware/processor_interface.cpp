// See processor_interface.h. Register layout from YAGCD 5.4 and Dolphin's
// ProcessorInterface.cpp.

#include "hardware/processor_interface.h"

#include "guest_memory.h"
#include "hardware/register_image.h"
#include "runtime_log.h"

namespace processor_interface {
namespace {

constexpr uint32_t INTERRUPT_CAUSE = BASE + 0x00;
constexpr uint32_t INTERRUPT_MASK = BASE + 0x04;
constexpr uint32_t FIFO_BASE = BASE + 0x0C;
constexpr uint32_t FIFO_END = BASE + 0x10;
constexpr uint32_t FIFO_WRITE_POINTER = BASE + 0x14;
constexpr uint32_t FLIPPER_REVISION = BASE + 0x2C;

// Bit 16 of the cause register is not an interrupt but the state of the reset button:
// set while it is not pressed.
constexpr uint32_t RESET_BUTTON_RELEASED = 0x00010000u;
// The revision retail consoles report (and Dolphin, as FLIPPER_REV_C).
constexpr uint32_t FLIPPER_REVISION_C = 0x246500B1u;
// The FIFO pointers hold physical addresses; their low 26 bits are the address.
constexpr uint32_t FIFO_ADDRESS_MASK = 0x03FFFFFFu;

uint32_t g_interrupt_causes = RESET_BUTTON_RELEASED;
uint32_t g_interrupt_causes_seen = 0;

}  // namespace

void initialize() { register_image::write32(INTERRUPT_CAUSE, g_interrupt_causes); }

void set_interrupt(uint32_t cause, bool asserted) {
    g_interrupt_causes = asserted ? (g_interrupt_causes | cause) : (g_interrupt_causes & ~cause);
    if (asserted && (g_interrupt_causes_seen & cause) == 0) {
        g_interrupt_causes_seen |= cause;
        runtime_log::info("interrupts", "first interrupt of cause 0x%X", cause);
    }
}

uint32_t pending_interrupts() { return g_interrupt_causes & ~RESET_BUTTON_RELEASED; }

uint32_t enabled_interrupts() { return register_image::read32(INTERRUPT_MASK); }

void write(uint32_t address, uint32_t value) {
    switch (address) {
        case INTERRUPT_CAUSE:
            // Writing 1 to a cause acknowledges it (a device whose line is still asserted
            // raises it again when it next updates).
            g_interrupt_causes &= ~(value & ~RESET_BUTTON_RELEASED);
            return;
        case FLIPPER_REVISION:
            return;  // read-only
        default:
            register_image::write32(address, value);
            return;
    }
}

void refresh(uint32_t address) {
    if (address == INTERRUPT_CAUSE) {
        register_image::write32(address, g_interrupt_causes);
    } else if (address == FLIPPER_REVISION) {
        register_image::write32(address, FLIPPER_REVISION_C);
    }
}

void append_to_fifo(const uint8_t* bytes, size_t size) {
    const uint32_t base = register_image::read32(FIFO_BASE) & FIFO_ADDRESS_MASK & ~0x1Fu;
    const uint32_t end = register_image::read32(FIFO_END) & FIFO_ADDRESS_MASK & ~0x1Fu;
    if (end <= base) {
        return;  // the game has not set the FIFO up yet
    }
    uint32_t write = register_image::read32(FIFO_WRITE_POINTER) & FIFO_ADDRESS_MASK;
    for (size_t i = 0; i < size; ++i) {
        if (uint8_t* host = guest_memory::host_pointer(guest_memory::physical_to_cached(write), 1)) {
            *host = bytes[i];
        }
        write = write + 1 >= end ? base : write + 1;
    }
    register_image::write32(FIFO_WRITE_POINTER,
                            (register_image::read32(FIFO_WRITE_POINTER) & ~FIFO_ADDRESS_MASK) | write);
}

}  // namespace processor_interface
