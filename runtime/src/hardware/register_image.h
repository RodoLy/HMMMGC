// The values of the hardware registers, kept where the guest view of 0xCC000000 finds
// them (see guest_memory.h): big-endian, at the offset of each register.
//
// Los valores de los registros de hardware, guardados donde los encuentra la vista del
// juego de 0xCC000000: big-endian, en el desplazamiento de cada registro.
#pragma once

#include <cstdint>

namespace register_image {

// `width` is 1, 2 or 4 bytes.
uint32_t load(uint32_t address, unsigned width);
void store(uint32_t address, uint32_t value, unsigned width);

inline uint16_t read16(uint32_t address) { return static_cast<uint16_t>(load(address, 2)); }
inline uint32_t read32(uint32_t address) { return load(address, 4); }
inline void write16(uint32_t address, uint16_t value) { store(address, value, 2); }
inline void write32(uint32_t address, uint32_t value) { store(address, value, 4); }

// A 32-bit value kept in two 16-bit registers, high half first (DMA addresses and lengths).
inline uint32_t read_pair(uint32_t address) { return (uint32_t{read16(address)} << 16) | read16(address + 2); }

}  // namespace register_image
