// guest_memory_host.h on Linux and Android: one memory file per shared memory, mapped with
// mmap at fixed addresses inside the reservation. (macOS: guest_memory_macos.cpp.)

#if !defined(_WIN32) && !defined(__APPLE__)

#include "guest_memory.h"
#include "guest_memory_host.h"

#include "guest_abi/guest_abi.h"

#include <sys/mman.h>
#include <sys/syscall.h>
#include <unistd.h>

#include <cerrno>
#include <cstring>
#include <stdexcept>
#include <string>
#include <tuple>

namespace guest_memory::host {
namespace {

std::string system_error(const char* what) { return std::string(what) + ": " + std::strerror(errno); }

int create_memory_file(const char* name, size_t size) {
#if defined(__ANDROID__)
    // Android's C library has memfd_create only from Android 11; the system call is older.
    const int fd = static_cast<int>(syscall(__NR_memfd_create, name, MFD_CLOEXEC));
#else
    const int fd = memfd_create(name, MFD_CLOEXEC);
#endif
    if (fd < 0 || ftruncate(fd, static_cast<off_t>(size)) != 0) {
        throw std::runtime_error(system_error("creating a memory file"));
    }
    return fd;
}

// Maps `size` bytes of the file `fd` (from its start) at `wanted`.
void map_shared_view(int fd, uint8_t* wanted, size_t size, int protection) {
    void* mapped = mmap(wanted, size, protection, MAP_SHARED | MAP_FIXED, fd, 0);
    if (mapped != wanted) {
        throw std::runtime_error(system_error("mapping a view of guest memory"));
    }
}

}  // namespace

GuestSpace map_guest_space(bool readable_registers) {
    // The whole 32-bit guest space, reserved without committing memory.
    void* wanted = reinterpret_cast<void*>(GuestFlat::kFixedFlatGuestBase);
    void* reserved = mmap(wanted, GuestFlat::kGuestSpaceSize, PROT_NONE,
                          MAP_PRIVATE | MAP_ANONYMOUS | MAP_NORESERVE | MAP_FIXED_NOREPLACE, -1, 0);
    if (reserved != wanted) {
        throw std::runtime_error(system_error("reserving the 4 GiB guest address space"));
    }
    auto* flat_base = static_cast<uint8_t*>(reserved);

    const int main_memory = create_memory_file("gamecube-main-memory", MAIN_MEMORY_SIZE);
    for (const uint32_t base : {PHYSICAL_MAIN_MEMORY_BASE, CACHED_MAIN_MEMORY_BASE, UNCACHED_MAIN_MEMORY_BASE}) {
        map_shared_view(main_memory, flat_base + base, MAIN_MEMORY_SIZE, PROT_READ | PROT_WRITE);
    }
    close(main_memory);

    for (const auto& [base, size, what] : {std::tuple{RUNTIME_MEMORY_BASE, RUNTIME_MEMORY_SIZE, "the runtime's memory"},
                                           std::tuple{LOCKED_CACHE_BASE, LOCKED_CACHE_SIZE, "the locked cache"}}) {
        void* mapped =
            mmap(flat_base + base, size, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
        if (mapped != flat_base + base) {
            throw std::runtime_error(system_error((std::string("mapping ") + what).c_str()));
        }
    }

    const int registers = create_memory_file("gamecube-hardware-registers", HARDWARE_REGISTERS_SIZE);
    map_shared_view(registers, flat_base + HARDWARE_REGISTERS_BASE, HARDWARE_REGISTERS_SIZE,
                    readable_registers ? PROT_READ : PROT_NONE);
    void* image = mmap(nullptr, HARDWARE_REGISTERS_SIZE, PROT_READ | PROT_WRITE, MAP_SHARED, registers, 0);
    if (image == MAP_FAILED) {
        throw std::runtime_error(system_error("mapping the hardware register image"));
    }
    close(registers);
    return {flat_base, static_cast<uint8_t*>(image)};
}

}  // namespace guest_memory::host

#endif  // !defined(_WIN32) && !defined(__APPLE__)
