// The GameCube's memory as the translated game sees it.
//
// La memoria del GameCube tal como la ve el juego traducido.
//
// The translator (WiiCompiled) turns every load and store of the game into an access to
// `kFlatGuestBase + guest address`: one host address space of 4 GiB that mirrors the
// 32-bit address space of the console's CPU. This module reserves that space and maps
// into it what the console has at each address:
//
//   0x00000000  main memory (MEM1, 24 MiB) by physical address, as DMA engines see it
//   0x80000000  the same memory through the data cache (where the game runs)
//   0x81800000  1 MiB the console does not have, right after its memory, for tables the
//               runtime gives the game in place of smaller ones of its own
//   0xC0000000  the same memory again, bypassing the cache (hardware buffers)
//   0xCC000000  the hardware registers (video, audio, disc, controllers, ...)
//   0xE0000000  the 16 KiB of the data cache that the game can lock as scratch memory
//
// The three views of MEM1 are the same physical pages, so a write through one is seen
// through the others, as on the console. Everything else stays unmapped, so a stray
// pointer of the game faults instead of silently reading zeros.
#pragma once

#include <cstddef>
#include <cstdint>

namespace guest_memory {

inline constexpr uint32_t MAIN_MEMORY_SIZE = 24u * 1024u * 1024u;
inline constexpr uint32_t PHYSICAL_MAIN_MEMORY_BASE = 0x00000000u;
inline constexpr uint32_t CACHED_MAIN_MEMORY_BASE = 0x80000000u;
inline constexpr uint32_t UNCACHED_MAIN_MEMORY_BASE = 0xC0000000u;

inline constexpr uint32_t LOCKED_CACHE_BASE = 0xE0000000u;
inline constexpr uint32_t LOCKED_CACHE_SIZE = 16u * 1024u;

inline constexpr uint32_t RUNTIME_MEMORY_BASE = 0x81800000u;
inline constexpr uint32_t RUNTIME_MEMORY_SIZE = 1024u * 1024u;

inline constexpr uint32_t HARDWARE_REGISTERS_BASE = 0xCC000000u;
inline constexpr uint32_t HARDWARE_REGISTERS_SIZE = 0x00010000u;

// Reserves the 4 GiB guest space and maps the memory above. Call once, before any
// translated code runs. Throws std::runtime_error when the host refuses a mapping.
void initialize();

// The host address where the guest bytes [guest_address, guest_address + length) live,
// or nullptr when that range is not entirely ordinary memory (RAM, the runtime's memory
// or the locked cache).
uint8_t* host_pointer(uint32_t guest_address, size_t length);

// Sets aside `size` bytes of the runtime's memory (at RUNTIME_MEMORY_BASE, zeroed) and
// gives their guest address, aligned to `alignment` (a power of two). Throws
// std::runtime_error when it is used up. Call after initialize().
uint32_t reserve_runtime_memory(uint32_t size, uint32_t alignment);

bool is_hardware_register(uint32_t guest_address);

// The start of the 4 GiB host reservation (guest address 0).
uint8_t* flat_base();

// The host bytes behind the guest view of the hardware registers. The guest view is
// kept inaccessible so that every read of a register can be seen (see fault_handler.h);
// the runtime writes the current value of a register here before such a read completes.
uint8_t* hardware_register_image();

// Big-endian accessors for runtime code that works on guest memory directly.
uint8_t read8(uint32_t guest_address);
uint16_t read16(uint32_t guest_address);
uint32_t read32(uint32_t guest_address);
void write8(uint32_t guest_address, uint8_t value);
void write16(uint32_t guest_address, uint16_t value);
void write32(uint32_t guest_address, uint32_t value);
void write64(uint32_t guest_address, uint64_t value);

// Physical address (what DMA registers hold) to the cached address the CPU uses.
constexpr uint32_t physical_to_cached(uint32_t physical_address) {
    return (physical_address & 0x1FFFFFFFu) | CACHED_MAIN_MEMORY_BASE;
}

// Whether an address is in main memory as the game's pointers hold it (cached), for runtime
// code that follows the game's pointers.
constexpr bool is_cached_main_memory(uint32_t address) {
    return address >= CACHED_MAIN_MEMORY_BASE && address - CACHED_MAIN_MEMORY_BASE < MAIN_MEMORY_SIZE;
}

}  // namespace guest_memory
