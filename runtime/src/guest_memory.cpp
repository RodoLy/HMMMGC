// See guest_memory.h. This file also defines the memory half of the guest ABI that the
// translated code links against (runtime/include/guest_abi/memory_access.h in WiiCompiled):
// the page tables its inline fast paths read, and the out-of-line slow paths they fall
// back to (hardware registers, the locked cache, and accesses that cross a mapping).
//
// Ver guest_memory.h. Este archivo también define la mitad de memoria de la ABI con la
// que se enlaza el código traducido: las tablas de páginas que leen sus caminos rápidos
// y los caminos lentos a los que recurren.

#include "guest_memory.h"

#include "fault_handler.h"
#include "guest_memory_host.h"
#include "hardware/hardware_registers.h"
#include "runtime_log.h"

#include "guest_abi/guest_abi.h"

#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>

// ---- Guest ABI: page tables read by the translated code's inline accessors ----

namespace MemoryInline {
PageEntry g_pageTable[kPageCount];
uintptr_t g_fullPageBias[kPageCount];
uintptr_t g_fullReadablePageBias[kPageCount];
uintptr_t g_fullWritablePageBias[kPageCount];
const SparseWritablePageTable* g_sparseWritablePageTables[kPageCount];
uint8_t g_deferredReadCoveredPages[kPageCount];
}  // namespace MemoryInline

namespace RecompMod {
// The executable-write guard of WiiCompiled stops a game from patching its own
// translated code. Nothing here registers executable ranges yet, so every page stays
// writable (0).
std::atomic<bool> g_executableWriteGuardEnabled{false};
std::atomic<uint8_t> g_executableWriteGuardPages[kExecutableWriteGuardPageCount];
std::atomic<uint8_t> g_executableWriteGuardCoarsePages[kExecutableWriteGuardCoarsePageCount];
std::atomic<uint8_t> g_executableWriteGuardMidPages[kExecutableWriteGuardMidPageCount];
}  // namespace RecompMod

namespace guest_memory {
namespace {

uint8_t* g_flat_base = nullptr;
uint8_t* g_hardware_register_image = nullptr;
// How much of the runtime's memory reserve_runtime_memory has given out.
uint32_t g_runtime_memory_used = 0;

// Lets the inline fast paths of the translated code reach a 1 MiB page directly.
// The bias is `host address - guest address`, plus one so that zero can mean "not
// mapped, take the slow path". In the flat layout it is the same for every page.
void publish_full_page(uint32_t page) {
    const uintptr_t encoded_bias = reinterpret_cast<uintptr_t>(g_flat_base) + 1u;
    MemoryInline::g_pageTable[page] = {g_flat_base + (static_cast<uint64_t>(page) << MemoryInline::kPageShift),
                                       MemoryInline::kPageSize};
    MemoryInline::g_fullPageBias[page] = encoded_bias;
    MemoryInline::g_fullReadablePageBias[page] = encoded_bias;
    MemoryInline::g_fullWritablePageBias[page] = encoded_bias;
}

void publish_main_memory_view(uint32_t base) {
    const uint32_t first_page = base >> MemoryInline::kPageShift;
    const uint32_t page_count = MAIN_MEMORY_SIZE >> MemoryInline::kPageShift;
    for (uint32_t page = first_page; page < first_page + page_count; ++page) {
        publish_full_page(page);
    }
}

bool in_range(uint32_t address, size_t length, uint32_t base, uint32_t size) {
    return address >= base && length <= size && address - base <= size - length;
}

}  // namespace

void initialize() {
    // HMMM_NATIVE_READABLE_REGISTERS=1 leaves the guest view of the hardware registers
    // readable (pointer reads then see the last value the runtime wrote), for running
    // under a debugger, which needs the trap that the register reads use.
    const bool readable_registers = std::getenv("HMMM_NATIVE_READABLE_REGISTERS") != nullptr;
    const host::GuestSpace space = host::map_guest_space(readable_registers);
    g_flat_base = space.flat_base;
    g_hardware_register_image = space.register_image;

    for (const uint32_t base : {PHYSICAL_MAIN_MEMORY_BASE, CACHED_MAIN_MEMORY_BASE, UNCACHED_MAIN_MEMORY_BASE}) {
        publish_main_memory_view(base);
    }
    static_assert(RUNTIME_MEMORY_SIZE == MemoryInline::kPageSize, "the runtime's memory is one page");
    publish_full_page(RUNTIME_MEMORY_BASE >> MemoryInline::kPageShift);
    // The locked cache is smaller than a 1 MiB page, so it gets a page-table entry with
    // a limit but no fast-path bias: its accesses take the slow paths below.
    MemoryInline::g_pageTable[LOCKED_CACHE_BASE >> MemoryInline::kPageShift] = {g_flat_base + LOCKED_CACHE_BASE,
                                                                                LOCKED_CACHE_SIZE};
    runtime_log::info("memory",
                      "guest space at %p: MEM1 at 0x00000000/0x80000000/0xC0000000, runtime memory at "
                      "0x81800000, locked cache at 0xE0000000, hardware registers at 0xCC000000",
                      static_cast<void*>(g_flat_base));
}

uint8_t* flat_base() { return g_flat_base; }

uint8_t* hardware_register_image() { return g_hardware_register_image; }

bool is_hardware_register(uint32_t guest_address) {
    return guest_address - HARDWARE_REGISTERS_BASE < HARDWARE_REGISTERS_SIZE;
}

uint8_t* host_pointer(uint32_t guest_address, size_t length) {
    for (const uint32_t base : {CACHED_MAIN_MEMORY_BASE, UNCACHED_MAIN_MEMORY_BASE, PHYSICAL_MAIN_MEMORY_BASE}) {
        if (in_range(guest_address, length, base, MAIN_MEMORY_SIZE)) {
            return g_flat_base + guest_address;
        }
    }
    if (in_range(guest_address, length, RUNTIME_MEMORY_BASE, RUNTIME_MEMORY_SIZE) ||
        in_range(guest_address, length, LOCKED_CACHE_BASE, LOCKED_CACHE_SIZE)) {
        return g_flat_base + guest_address;
    }
    return nullptr;
}

uint32_t reserve_runtime_memory(uint32_t size, uint32_t alignment) {
    const uint32_t start = (g_runtime_memory_used + alignment - 1) & ~(alignment - 1);
    if (start > RUNTIME_MEMORY_SIZE || size > RUNTIME_MEMORY_SIZE - start) {
        throw std::runtime_error("the runtime's memory is used up");
    }
    g_runtime_memory_used = start + size;
    return RUNTIME_MEMORY_BASE + start;
}

namespace {

template <typename T>
T load_big_endian(const uint8_t* host) {
    T value{};
    std::memcpy(&value, host, sizeof(T));
    return MemoryInline::MaybeByteSwap(value);
}

template <typename T>
void store_big_endian(uint8_t* host, T value) {
    const T swapped = MemoryInline::MaybeByteSwap(value);
    std::memcpy(host, &swapped, sizeof(T));
}

}  // namespace

// The slow paths of every access that is not plain RAM: hardware registers go to their
// device, RAM and the locked cache are read in place, and anything else is a bug of the
// game or of the runtime, reported with where it happened.
//
// `host_caller` is the return address into the translated function that made the access,
// so that the log of hardware accesses can name the game function.
template <typename T>
T read_slow(uint32_t guest_address, const void* host_caller = nullptr) {
    if (is_hardware_register(guest_address)) {
        if constexpr (sizeof(T) <= 4) {
            return static_cast<T>(hardware_registers::read(guest_address, sizeof(T), host_caller));
        } else {
            const uint64_t high = hardware_registers::read(guest_address, 4, host_caller);
            return static_cast<T>((high << 32) | hardware_registers::read(guest_address + 4, 4, host_caller));
        }
    }
    if (const uint8_t* host = host_pointer(guest_address, sizeof(T))) {
        return load_big_endian<T>(host);
    }
    fault_handler::report_invalid_access("read", guest_address, sizeof(T));
}

template <typename T>
void write_slow(uint32_t guest_address, T value, const void* host_caller = nullptr) {
    if (is_hardware_register(guest_address)) {
        if constexpr (sizeof(T) <= 4) {
            hardware_registers::write(guest_address, static_cast<uint32_t>(value), sizeof(T), host_caller);
        } else {
            hardware_registers::write(guest_address, static_cast<uint32_t>(value >> 32), 4, host_caller);
            hardware_registers::write(guest_address + 4, static_cast<uint32_t>(value), 4, host_caller);
        }
        return;
    }
    if (uint8_t* host = host_pointer(guest_address, sizeof(T))) {
        store_big_endian<T>(host, value);
        return;
    }
    fault_handler::report_invalid_access("write", guest_address, sizeof(T));
}

uint8_t read8(uint32_t guest_address) { return read_slow<uint8_t>(guest_address); }
uint16_t read16(uint32_t guest_address) { return read_slow<uint16_t>(guest_address); }
uint32_t read32(uint32_t guest_address) { return read_slow<uint32_t>(guest_address); }
void write8(uint32_t guest_address, uint8_t value) { write_slow(guest_address, value); }
void write16(uint32_t guest_address, uint16_t value) { write_slow(guest_address, value); }
void write32(uint32_t guest_address, uint32_t value) { write_slow(guest_address, value); }
void write64(uint32_t guest_address, uint64_t value) { write_slow(guest_address, value); }

}  // namespace guest_memory

// ---- Guest ABI: out-of-line accessors of the translated code ----

namespace MemoryInline {

namespace {
float bits_to_float(uint32_t bits) {
    float value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
double bits_to_double(uint64_t bits) {
    double value;
    std::memcpy(&value, &bits, sizeof(value));
    return value;
}
uint64_t double_to_bits(double value) {
    uint64_t bits;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}
}  // namespace

// These are only ever called from translated code, so their return address names the
// game function that made the access.
#define CALLER __builtin_return_address(0)

uint8_t Read8Slow(uint32_t addr) { return guest_memory::read_slow<uint8_t>(addr, CALLER); }
uint16_t Read16Slow(uint32_t addr) { return guest_memory::read_slow<uint16_t>(addr, CALLER); }
uint32_t Read32Slow(uint32_t addr) { return guest_memory::read_slow<uint32_t>(addr, CALLER); }
uint64_t Read64Slow(uint32_t addr) { return guest_memory::read_slow<uint64_t>(addr, CALLER); }
float ReadFloat32Slow(uint32_t addr) { return bits_to_float(guest_memory::read_slow<uint32_t>(addr, CALLER)); }
double ReadFloat64Slow(uint32_t addr) { return bits_to_double(guest_memory::read_slow<uint64_t>(addr, CALLER)); }

void Write8Slow(uint32_t addr, uint8_t val) { guest_memory::write_slow(addr, val, CALLER); }
void Write16Slow(uint32_t addr, uint16_t val) { guest_memory::write_slow(addr, val, CALLER); }
void Write32Slow(uint32_t addr, uint32_t val) { guest_memory::write_slow(addr, val, CALLER); }
void Write64Slow(uint32_t addr, uint64_t val) { guest_memory::write_slow(addr, val, CALLER); }
void WriteFloat32Slow(uint32_t addr, double val) {
    guest_memory::write_slow(addr, ConvertPpcDoubleToSingleBits(val), CALLER);
}
void WriteFloat64Slow(uint32_t addr, double val) { guest_memory::write_slow(addr, double_to_bits(val), CALLER); }

// The "resolved" accessors resolve a whole range once and fall back here per access
// when the range was not plain memory.
template <typename T>
T ReadResolvedFallback(uint32_t addr) {
    return guest_memory::read_slow<T>(addr, CALLER);
}
template <typename T>
void WriteResolvedFallback(uint32_t addr, T value) {
    guest_memory::write_slow(addr, value, CALLER);
}
template uint8_t ReadResolvedFallback<uint8_t>(uint32_t);
template uint16_t ReadResolvedFallback<uint16_t>(uint32_t);
template uint32_t ReadResolvedFallback<uint32_t>(uint32_t);
template uint64_t ReadResolvedFallback<uint64_t>(uint32_t);
template void WriteResolvedFallback<uint8_t>(uint32_t, uint8_t);
template void WriteResolvedFallback<uint16_t>(uint32_t, uint16_t);
template void WriteResolvedFallback<uint32_t>(uint32_t, uint32_t);
template void WriteResolvedFallback<uint64_t>(uint32_t, uint64_t);

float ReadResolvedFallbackFloat32(uint32_t addr) { return ReadFloat32Slow(addr); }
double ReadResolvedFallbackFloat64(uint32_t addr) { return ReadFloat64Slow(addr); }
void WriteResolvedFallbackFloat32(uint32_t addr, double val) { WriteFloat32Slow(addr, val); }
void WriteResolvedFallbackFloat64(uint32_t addr, double val) { WriteFloat64Slow(addr, val); }

#undef CALLER

// Deferred reads (WiiCompiled uses them for copies of the frame buffer that are only
// made when the game reads them) are not used here: nothing is ever pending.
bool ResolveDeferredReads(uint32_t, size_t) { return true; }

}  // namespace MemoryInline

// ---- Guest ABI: the Memory API used by generated data initialisers and helpers ----

bool Memory::Contains(uint32_t addr, size_t length) { return guest_memory::host_pointer(addr, length) != nullptr; }

uint8_t* Memory::GetPointer(uint32_t addr) { return Memory::GetPointer(addr, 1); }

uint8_t* Memory::GetPointer(uint32_t addr, size_t length) {
    uint8_t* host = guest_memory::host_pointer(addr, length);
    if (host == nullptr) {
        throw AccessViolation(addr, length, "not ordinary guest memory");
    }
    return host;
}

Memory::AccessViolation::AccessViolation(uint32_t address, size_t length, std::string_view reason)
    : std::runtime_error(std::string(reason)), address_(address), length_(length), reason_(reason) {}
