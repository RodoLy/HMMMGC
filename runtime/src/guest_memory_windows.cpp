// guest_memory_host.h on Windows: the reservation is a placeholder (VirtualAlloc2), split
// where each piece goes, and each piece replaced by a view of a section (MapViewOfFile3)
// or by plain memory. Needs Windows 10 version 1803 or later.

#if defined(_WIN32)

// VirtualAlloc2 and MapViewOfFile3 came with Windows 10 version 1803.
#if !defined(NTDDI_VERSION)
#define NTDDI_VERSION 0x0A000005  // NTDDI_WIN10_RS4
#endif

#include "guest_memory.h"
#include "guest_memory_host.h"

#include "guest_abi/guest_abi.h"

#include <windows.h>

#include <stdexcept>
#include <string>

namespace guest_memory::host {
namespace {

std::string system_error(const char* what) { return std::string(what) + ": error " + std::to_string(GetLastError()); }

// Windows places mappings at multiples of 64 KiB; the locked cache (16 KiB) takes a
// whole piece of that size.
constexpr size_t ALLOCATION_GRANULARITY = 64 * 1024;

// Splits the piece [address, address + size) off the reserved placeholder.
void split_placeholder(uint8_t* address, size_t size) {
    if (!VirtualFree(address, size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER)) {
        throw std::runtime_error(system_error("splitting the guest address space"));
    }
}

HANDLE create_section(size_t size) {
    HANDLE section =
        CreateFileMappingW(INVALID_HANDLE_VALUE, nullptr, PAGE_READWRITE, 0, static_cast<DWORD>(size), nullptr);
    if (section == nullptr) {
        throw std::runtime_error(system_error("creating a shared memory section"));
    }
    return section;
}

// Maps `section` in the place of the placeholder piece at `address`.
void map_view_in_place(HANDLE section, uint8_t* address, size_t size, ULONG protection) {
    split_placeholder(address, size);
    void* view =
        MapViewOfFile3(section, GetCurrentProcess(), address, 0, size, MEM_REPLACE_PLACEHOLDER, protection, nullptr, 0);
    if (view != address) {
        throw std::runtime_error(system_error("mapping a view of guest memory"));
    }
}

}  // namespace

GuestSpace map_guest_space(bool readable_registers) {
    auto* wanted = reinterpret_cast<uint8_t*>(GuestFlat::kFixedFlatGuestBase);
    void* reserved = VirtualAlloc2(GetCurrentProcess(), wanted, GuestFlat::kGuestSpaceSize,
                                   MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, nullptr, 0);
    if (reserved != wanted) {
        throw std::runtime_error(system_error("reserving the 4 GiB guest address space"));
    }
    uint8_t* flat_base = wanted;

    HANDLE main_memory = create_section(MAIN_MEMORY_SIZE);
    for (const uint32_t base : {PHYSICAL_MAIN_MEMORY_BASE, CACHED_MAIN_MEMORY_BASE, UNCACHED_MAIN_MEMORY_BASE}) {
        map_view_in_place(main_memory, flat_base + base, MAIN_MEMORY_SIZE, PAGE_READWRITE);
    }
    CloseHandle(main_memory);  // the views keep the section alive

    split_placeholder(flat_base + LOCKED_CACHE_BASE, ALLOCATION_GRANULARITY);
    if (VirtualAlloc2(GetCurrentProcess(), flat_base + LOCKED_CACHE_BASE, ALLOCATION_GRANULARITY,
                      MEM_RESERVE | MEM_COMMIT | MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, nullptr,
                      0) != flat_base + LOCKED_CACHE_BASE) {
        throw std::runtime_error(system_error("mapping the locked cache"));
    }

    // A view cannot be mapped inaccessible: it is mapped read-only and then closed.
    HANDLE registers = create_section(HARDWARE_REGISTERS_SIZE);
    uint8_t* guest_view = flat_base + HARDWARE_REGISTERS_BASE;
    map_view_in_place(registers, guest_view, HARDWARE_REGISTERS_SIZE, PAGE_READONLY);
    DWORD previous = 0;
    if (!readable_registers && !VirtualProtect(guest_view, HARDWARE_REGISTERS_SIZE, PAGE_NOACCESS, &previous)) {
        throw std::runtime_error(system_error("closing the guest view of the hardware registers"));
    }
    void* image = MapViewOfFile(registers, FILE_MAP_ALL_ACCESS, 0, 0, HARDWARE_REGISTERS_SIZE);
    if (image == nullptr) {
        throw std::runtime_error(system_error("mapping the hardware register image"));
    }
    CloseHandle(registers);
    return {flat_base, static_cast<uint8_t*>(image)};
}

}  // namespace guest_memory::host

#endif  // defined(_WIN32)
