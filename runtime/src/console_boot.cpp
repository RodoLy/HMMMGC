// See console_boot.h. The layout of the low-memory globals is "Dolphin OS globals" in
// YAGCD (section 4.2.1); the values are those that Dolphin's boot of this game leaves
// (read from its memory at the first frame), so that both start the game the same way.

#include "console_boot.h"

#include "cpu_registers.h"
#include "game_disc.h"
#include "guest_memory.h"
#include "runtime_log.h"

#include "guest_abi/guest_abi.h"

#include <cstring>
#include <ctime>

namespace console_boot {
namespace {

constexpr uint32_t GLOBALS = 0x80000000u;
constexpr uint32_t DISC_ID_SIZE = 0x20;  // game code, maker, disc number, version, magic

constexpr uint32_t BOOT_MAGIC_ADDRESS = GLOBALS + 0x20;
constexpr uint32_t BOOT_MAGIC = 0x0D15EA5Eu;  // "booted from the boot ROM"
constexpr uint32_t APPLOADER_VERSION_ADDRESS = GLOBALS + 0x24;
constexpr uint32_t MEMORY_SIZE_ADDRESS = GLOBALS + 0x28;
constexpr uint32_t CONSOLE_TYPE_ADDRESS = GLOBALS + 0x2C;
// The latest development kit, what Dolphin reports (retail is 3); some games take
// different paths with the retail value, and this keeps the boot identical to Dolphin's.
constexpr uint32_t CONSOLE_TYPE_LATEST_DEVKIT = 0x10000006u;
constexpr uint32_t ARENA_HIGH_ADDRESS = GLOBALS + 0x34;
constexpr uint32_t FST_ADDRESS_ADDRESS = GLOBALS + 0x38;
constexpr uint32_t FST_MAXIMUM_SIZE_ADDRESS = GLOBALS + 0x3C;
constexpr uint32_t VIDEO_MODE_ADDRESS = GLOBALS + 0xCC;
constexpr uint32_t VIDEO_MODE_NTSC = 0;
constexpr uint32_t ARAM_SIZE_ADDRESS = GLOBALS + 0xD0;
constexpr uint32_t ARAM_SIZE = 0x01000000u;
constexpr uint32_t TOP_OF_MEMORY_ADDRESS = GLOBALS + 0xEC;
constexpr uint32_t SIMULATED_MEMORY_SIZE_ADDRESS = GLOBALS + 0xF0;
constexpr uint32_t BOOT_INFO2_ADDRESS_ADDRESS = GLOBALS + 0xF4;
constexpr uint32_t BUS_CLOCK_ADDRESS = GLOBALS + 0xF8;
constexpr uint32_t BUS_CLOCK_HZ = 162'000'000;
constexpr uint32_t CPU_CLOCK_ADDRESS = GLOBALS + 0xFC;
constexpr uint32_t CPU_CLOCK_HZ = 486'000'000;
// The OS adds this to the time base to get the date: ticks since 2000-01-01 at boot.
constexpr uint32_t SYSTEM_TIME_ADDRESS = GLOBALS + 0x30D8;

// The boot ROM leaves a "return from interrupt" in the exception vectors that the OS
// later replaces with its own handlers.
constexpr uint32_t RETURN_FROM_INTERRUPT_INSTRUCTION = 0x4C000064u;
constexpr uint32_t DEFAULT_EXCEPTION_VECTORS[] = {GLOBALS + 0x300, GLOBALS + 0x800, GLOBALS + 0xC00};

constexpr uint32_t END_OF_MEMORY = guest_memory::CACHED_MAIN_MEMORY_BASE + guest_memory::MAIN_MEMORY_SIZE;

void copy_to_guest(uint32_t address, const std::vector<uint8_t>& bytes) {
    std::memcpy(guest_memory::host_pointer(address, bytes.size()), bytes.data(), bytes.size());
}

uint64_t seconds_since_2000() {
    constexpr int64_t UNIX_TIME_OF_2000 = 946684800;
    return static_cast<uint64_t>(std::time(nullptr) - UNIX_TIME_OF_2000);
}

}  // namespace

void prepare_memory(const GameDisc& disc) {
    const auto& header = disc.disc_header();
    std::memcpy(guest_memory::host_pointer(GLOBALS, DISC_ID_SIZE), header.data(), DISC_ID_SIZE);

    // The apploader puts the file system table at the top of memory, 32-byte aligned,
    // and the debug settings (bi2.bin) right below it; the arena ends where they start.
    const auto& fst = disc.file_system_table();
    const auto& boot_info2 = disc.boot_info2();
    const uint32_t fst_address = END_OF_MEMORY - ((static_cast<uint32_t>(fst.size()) + 31u) & ~31u);
    const uint32_t boot_info2_address = fst_address - static_cast<uint32_t>(boot_info2.size());
    copy_to_guest(fst_address, fst);
    copy_to_guest(boot_info2_address, boot_info2);

    guest_memory::write32(BOOT_MAGIC_ADDRESS, BOOT_MAGIC);
    guest_memory::write32(APPLOADER_VERSION_ADDRESS, 1);
    guest_memory::write32(MEMORY_SIZE_ADDRESS, guest_memory::MAIN_MEMORY_SIZE);
    guest_memory::write32(CONSOLE_TYPE_ADDRESS, CONSOLE_TYPE_LATEST_DEVKIT);
    guest_memory::write32(ARENA_HIGH_ADDRESS, fst_address);
    guest_memory::write32(FST_ADDRESS_ADDRESS, fst_address);
    guest_memory::write32(FST_MAXIMUM_SIZE_ADDRESS, static_cast<uint32_t>(fst.size()));
    guest_memory::write32(VIDEO_MODE_ADDRESS, VIDEO_MODE_NTSC);
    guest_memory::write32(ARAM_SIZE_ADDRESS, ARAM_SIZE);
    guest_memory::write32(TOP_OF_MEMORY_ADDRESS, END_OF_MEMORY);
    guest_memory::write32(SIMULATED_MEMORY_SIZE_ADDRESS, guest_memory::MAIN_MEMORY_SIZE);
    guest_memory::write32(BOOT_INFO2_ADDRESS_ADDRESS, boot_info2_address);
    guest_memory::write32(BUS_CLOCK_ADDRESS, BUS_CLOCK_HZ);
    guest_memory::write32(CPU_CLOCK_ADDRESS, CPU_CLOCK_HZ);
    guest_memory::write64(SYSTEM_TIME_ADDRESS, seconds_since_2000() * cpu_registers::TIME_BASE_TICKS_PER_SECOND);
    for (const uint32_t vector : DEFAULT_EXCEPTION_VECTORS) {
        guest_memory::write32(vector, RETURN_FROM_INTERRUPT_INSTRUCTION);
    }
    runtime_log::info("boot", "FST at 0x%08X (%zu bytes), bi2 at 0x%08X", fst_address, fst.size(), boot_info2_address);
}

void prepare_cpu(CpuContext& cpu) {
    cpu = CpuContext{};
    // Floating point available, instruction and data address translation on (the BATs
    // map 0x80000000 and 0xC0000000 to memory), recoverable exception.
    cpu.msr = 0x00002032u;
    // The stack and small-data pointers the boot ROM leaves for NTSC consoles (Dolphin's
    // values); the game's __init_registers sets its own before using them.
    cpu.gpr[1] = 0x81566550u;
    cpu.gpr[2] = 0x81465CC0u;
    cpu.gpr[13] = 0x81465320u;
}

uint32_t entry_point(const GameDisc& disc) {
    // The disc header gives where main.dol starts; the DOL header, where its code starts.
    constexpr uint64_t DOL_OFFSET_FIELD = 0x420;
    constexpr uint64_t DOL_ENTRY_POINT_FIELD = 0xE0;
    const uint32_t dol_offset = disc.read_big_endian32(DOL_OFFSET_FIELD);
    return disc.read_big_endian32(dol_offset + DOL_ENTRY_POINT_FIELD);
}

}  // namespace console_boot
