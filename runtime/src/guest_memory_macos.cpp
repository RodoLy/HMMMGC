// guest_memory_host.h on macOS: the guest space is reserved with Mach's virtual memory calls,
// and each view of a shared memory (the main memory's three, the registers' two) is the same
// memory mapped again at its place with mach_vm_remap, as WiiCompiled does on macOS. macOS has no
// memory files (memfd_create) and no mmap flag that refuses an address already in use.
//
// En macOS el espacio del juego se reserva con las llamadas de memoria virtual de Mach, y cada
// vista de una memoria compartida es la misma memoria puesta otra vez en su lugar con
// mach_vm_remap, como lo hace WiiCompiled en macOS.

#if defined(__APPLE__)

#include "guest_memory.h"
#include "guest_memory_host.h"

#include "guest_abi/guest_abi.h"

#include <mach/mach.h>
#include <mach/mach_error.h>
#include <mach/mach_vm.h>
#include <sys/mman.h>

#include <stdexcept>
#include <string>
#include <tuple>

namespace guest_memory::host {
namespace {

std::string mach_failure(const char* what, kern_return_t result) {
    return std::string(what) + ": " + mach_error_string(result);
}

// A memory of `size` bytes that other views can share.
uint8_t* new_shared_memory(size_t size, const char* what) {
    void* memory = mmap(nullptr, size, PROT_READ | PROT_WRITE, MAP_SHARED | MAP_ANON, -1, 0);
    if (memory == MAP_FAILED) {
        throw std::runtime_error(std::string("allocating ") + what);
    }
    return static_cast<uint8_t*>(memory);
}

// Maps `size` bytes of `memory` again at `wanted`, sharing them, in place of what was there.
void map_shared_view(uint8_t* memory, uint8_t* wanted, size_t size) {
    mach_vm_address_t target = reinterpret_cast<mach_vm_address_t>(wanted);
    vm_prot_t current = VM_PROT_NONE;
    vm_prot_t maximum = VM_PROT_NONE;
    const kern_return_t result =
        mach_vm_remap(mach_task_self(), &target, size, 0, VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE, mach_task_self(),
                      reinterpret_cast<mach_vm_address_t>(memory), FALSE, &current, &maximum, VM_INHERIT_NONE);
    if (result != KERN_SUCCESS || target != reinterpret_cast<mach_vm_address_t>(wanted)) {
        throw std::runtime_error(mach_failure("mapping a view of guest memory", result));
    }
}

void protect(uint8_t* start, size_t size, vm_prot_t protection) {
    const kern_return_t result =
        mach_vm_protect(mach_task_self(), reinterpret_cast<mach_vm_address_t>(start), size, FALSE, protection);
    if (result != KERN_SUCCESS) {
        throw std::runtime_error(mach_failure("protecting guest memory", result));
    }
}

}  // namespace

GuestSpace map_guest_space(bool readable_registers) {
    // The whole 32-bit guest space at its fixed address, inaccessible until a view is put there.
    mach_vm_address_t address = GuestFlat::kFixedFlatGuestBase;
    const kern_return_t reserved =
        mach_vm_allocate(mach_task_self(), &address, GuestFlat::kGuestSpaceSize, VM_FLAGS_FIXED);
    if (reserved != KERN_SUCCESS || address != GuestFlat::kFixedFlatGuestBase) {
        throw std::runtime_error(mach_failure("reserving the 4 GiB guest address space", reserved));
    }
    auto* flat_base = reinterpret_cast<uint8_t*>(address);
    protect(flat_base, GuestFlat::kGuestSpaceSize, VM_PROT_NONE);

    uint8_t* main_memory = new_shared_memory(MAIN_MEMORY_SIZE, "the main memory");
    for (const uint32_t base : {PHYSICAL_MAIN_MEMORY_BASE, CACHED_MAIN_MEMORY_BASE, UNCACHED_MAIN_MEMORY_BASE}) {
        map_shared_view(main_memory, flat_base + base, MAIN_MEMORY_SIZE);
    }

    for (const auto& [base, size, what] : {std::tuple{RUNTIME_MEMORY_BASE, RUNTIME_MEMORY_SIZE, "the runtime's memory"},
                                           std::tuple{LOCKED_CACHE_BASE, LOCKED_CACHE_SIZE, "the locked cache"}}) {
        mach_vm_address_t place = reinterpret_cast<mach_vm_address_t>(flat_base + base);
        const kern_return_t result =
            mach_vm_allocate(mach_task_self(), &place, size, VM_FLAGS_FIXED | VM_FLAGS_OVERWRITE);
        if (result != KERN_SUCCESS) {
            throw std::runtime_error(mach_failure((std::string("mapping ") + what).c_str(), result));
        }
    }

    // The runtime writes the registers through `image`; the game's view of them faults on a read.
    uint8_t* image = new_shared_memory(HARDWARE_REGISTERS_SIZE, "the hardware registers");
    map_shared_view(image, flat_base + HARDWARE_REGISTERS_BASE, HARDWARE_REGISTERS_SIZE);
    protect(flat_base + HARDWARE_REGISTERS_BASE, HARDWARE_REGISTERS_SIZE,
            readable_registers ? VM_PROT_READ : VM_PROT_NONE);
    return {flat_base, image};
}

}  // namespace guest_memory::host

#endif  // defined(__APPLE__)
