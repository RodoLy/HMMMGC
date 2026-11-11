// The external interface (EXI, 0xCC006800): three serial buses for the memory cards, the
// console's clock and settings (SRAM), and accessories.
//
// La interfaz externa (EXI): tres buses serie para las memory cards, el reloj y la
// configuración de la consola (SRAM), y accesorios.
//
// Only the boot ROM's chip on channel 0 answers (clock and SRAM, which OSInit reads);
// the memory card slots are empty for now.
#pragma once

#include <cstdint>

namespace external_interface {

inline constexpr uint32_t BASE = 0xCC006800u;
inline constexpr uint32_t SIZE = 0x40u;

void initialize();

// Register accesses of 4 bytes at `address`.
void write(uint32_t address, uint32_t value);

}  // namespace external_interface
