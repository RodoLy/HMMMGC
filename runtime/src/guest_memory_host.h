// The part of guest_memory.cpp that depends on the host system: placing the guest's
// memory in the host's address space. One file per system implements it
// (guest_memory_posix.cpp, guest_memory_windows.cpp).
//
// La parte de guest_memory.cpp que depende del sistema: poner la memoria del juego en el
// espacio de direcciones del host. Cada sistema la implementa en su archivo.
#pragma once

#include <cstddef>
#include <cstdint>

namespace guest_memory::host {

struct GuestSpace {
    uint8_t* flat_base;       // guest address 0
    uint8_t* register_image;  // the runtime's own view of the hardware registers
};

// Reserves the 4 GiB guest space at its fixed host address (GuestFlat::kFixedFlatGuestBase)
// and maps into it: the main memory three times (one memory seen at its physical, cached
// and uncached addresses), the runtime's memory, the locked cache, and the hardware
// registers. The guest view of the registers is inaccessible, or read-only with
// `readable_registers`; the runtime writes them through a second view,
// GuestSpace::register_image. Throws std::runtime_error when the host refuses a mapping.
GuestSpace map_guest_space(bool readable_registers);

}  // namespace guest_memory::host
