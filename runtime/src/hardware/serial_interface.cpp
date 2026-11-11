// See serial_interface.h. Register layout from YAGCD 5.8 and Dolphin's SI.cpp.

#include "hardware/serial_interface.h"

#include "hardware/processor_interface.h"
#include "hardware/register_image.h"

namespace serial_interface {
namespace {

constexpr uint32_t COMMUNICATION_CONTROL = BASE + 0x34;
constexpr uint32_t STATUS = BASE + 0x38;

constexpr uint32_t TRANSFER_START = 0x00000001u;
constexpr uint32_t TRANSFER_INTERRUPT = 0x80000000u;  // acknowledged by writing 1
constexpr uint32_t TRANSFER_INTERRUPT_MASK = 0x40000000u;
constexpr uint32_t COMMUNICATION_ERROR = 0x20000000u;
// In the status register each port has a byte, high port first; bit 3 is "no response".
constexpr uint32_t NO_RESPONSE = 0x08u;

}  // namespace

void write(uint32_t address, uint32_t value) {
    if (address != COMMUNICATION_CONTROL) {
        register_image::write32(address, value);
        return;
    }
    const uint32_t before = register_image::read32(address);
    uint32_t control = (before & TRANSFER_INTERRUPT & ~value) | (value & ~TRANSFER_INTERRUPT);
    if (value & TRANSFER_START) {
        const unsigned port = (value >> 1) & 3u;
        register_image::write32(STATUS, register_image::read32(STATUS) | (NO_RESPONSE << (8 * (3 - port))));
        control = (control & ~TRANSFER_START) | TRANSFER_INTERRUPT | COMMUNICATION_ERROR;
    }
    register_image::write32(address, control);
    processor_interface::set_interrupt(processor_interface::SERIAL_INTERRUPT,
                                       (control & TRANSFER_INTERRUPT) && (control & TRANSFER_INTERRUPT_MASK));
}

}  // namespace serial_interface
