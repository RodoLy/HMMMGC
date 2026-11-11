// The values that the game passes to its graphics functions by address, and those it asks
// them for back.
//
// Los valores que el juego pasa por dirección a sus funciones de gráficos, y los que les
// pide de vuelta.
//
// The game's GX calls pass small structures (a GXColor) and matrices by address, in the
// game's memory and byte order (big-endian). aurora takes them as host values, so the
// natives copy them out first.
#pragma once

#include "guest_memory.h"

#include <dolphin/gx.h>

#include <array>
#include <bit>
#include <cstddef>
#include <cstdint>

namespace guest_values {

// A GXColor: four bytes, red, green, blue and alpha. (CodeWarrior passes a structure
// argument as the address of a copy of it.)
inline GXColor read_color(uint32_t guest_address) {
    return GXColor{guest_memory::read8(guest_address), guest_memory::read8(guest_address + 1),
                   guest_memory::read8(guest_address + 2), guest_memory::read8(guest_address + 3)};
}

inline float read_float(uint32_t guest_address) { return std::bit_cast<float>(guest_memory::read32(guest_address)); }

// A float the game asked for by address (an output argument).
inline void write_float(uint32_t guest_address, float value) {
    guest_memory::write32(guest_address, std::bit_cast<uint32_t>(value));
}

// `Count` floats in a row: a matrix (3x4 for Mtx, 4x4 for Mtx44, 2x4 for a texture
// matrix of two rows), row by row.
template <size_t Count>
std::array<float, Count> read_floats(uint32_t guest_address) {
    std::array<float, Count> values{};
    for (size_t i = 0; i < Count; ++i) {
        values[i] = read_float(guest_address + static_cast<uint32_t>(i * sizeof(float)));
    }
    return values;
}

// The host address of `size` bytes of the game's memory, for aurora to read later (an
// image, a vertex array, a display list). nullptr when the range is not ordinary memory.
inline const void* host_data(uint32_t guest_address, size_t size) {
    return guest_address == 0 ? nullptr : guest_memory::host_pointer(guest_address, size);
}

}  // namespace guest_values
