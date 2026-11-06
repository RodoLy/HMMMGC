// See register_image.h.

#include "hardware/register_image.h"

#include "guest_memory.h"

namespace register_image {
namespace {

uint8_t* bytes_at(uint32_t address) {
    return guest_memory::hardware_register_image() + (address - guest_memory::HARDWARE_REGISTERS_BASE);
}

}  // namespace

uint32_t load(uint32_t address, unsigned width) {
    const uint8_t* bytes = bytes_at(address);
    uint32_t value = 0;
    for (unsigned i = 0; i < width; ++i) {
        value = (value << 8) | bytes[i];
    }
    return value;
}

void store(uint32_t address, uint32_t value, unsigned width) {
    uint8_t* bytes = bytes_at(address);
    for (unsigned i = 0; i < width; ++i) {
        bytes[i] = static_cast<uint8_t>(value >> (8 * (width - 1 - i)));
    }
}

}  // namespace register_image
