// See cpu_registers.h. This file defines the CPU half of the guest ABI: the helpers that
// translated mfspr/mtspr, mftb, mcrxr, tw and sc call, and the generic paired-single
// quantized loads and stores whose quantization register is only known at run time.
//
// Ver cpu_registers.h. Este archivo define la mitad de CPU de la ABI: las funciones que
// llaman las instrucciones traducidas que leen o escriben registros especiales.

#include "cpu_registers.h"

#include "fault_handler.h"
#include "guest_memory.h"
#include "runtime_log.h"

#include "guest_abi/guest_abi.h"

#include <array>
#include <bitset>
#include <chrono>
#include <cstdint>
#include <cstring>

namespace cpu_registers {
namespace {

// Special-purpose register numbers, from the IBM PowerPC 750CL user's manual (the Gekko
// is a 750 with the paired-single, quantization and locked-cache extensions).
enum SpecialPurposeRegister : uint32_t {
    INTEGER_EXCEPTION = 1,  // XER
    LINK = 8,
    COUNT = 9,
    DECREMENTER = 22,
    SAVE_RESTORE0 = 26,
    SAVE_RESTORE1 = 27,
    TIME_BASE_LOW_READ = 268,
    TIME_BASE_HIGH_READ = 269,
    TIME_BASE_LOW_WRITE = 284,
    TIME_BASE_HIGH_WRITE = 285,
    PROCESSOR_VERSION = 287,
    FIRST_QUANTIZATION = 912,  // GQR0..GQR7: how psq_l/psq_st convert values
    LAST_QUANTIZATION = 919,
    HARDWARE_IMPLEMENTATION2 = 920,    // HID2: enables paired singles and the locked cache
    DIRECT_MEMORY_ACCESS_UPPER = 922,  // DMA_U and DMA_L drive the locked-cache DMA engine
    DIRECT_MEMORY_ACCESS_LOWER = 923,
    HARDWARE_IMPLEMENTATION0 = 1008,
    HARDWARE_IMPLEMENTATION1 = 1009,
};

// The processor version register of the GameCube's CPU; Dolphin reports the same value.
constexpr uint32_t GEKKO_PROCESSOR_VERSION = 0x00083214u;

// SPRs without behaviour of their own (BATs, SPRG0-3, L2CR, performance monitors, ...)
// read back what was last written to them.
std::array<uint32_t, 1024> g_stored_special_registers{};

const auto RUNTIME_START = std::chrono::steady_clock::now();
int64_t g_time_base_adjustment = 0;  // what the game's writes to the time base moved it by

uint32_t g_decrementer_written = 0;
uint64_t g_decrementer_written_at = 0;
bool g_decrementer_armed = false;  // a value was written and has not run out yet

uint64_t ticks_since_start() {
    const auto elapsed = std::chrono::steady_clock::now() - RUNTIME_START;
    const auto nanoseconds =
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(elapsed).count());
    // 40.5 ticks per microsecond: nanoseconds * 40'500'000 / 1'000'000'000.
    return nanoseconds * 81u / 2000u;
}

void set_time_base(uint64_t value) { g_time_base_adjustment = static_cast<int64_t>(value - ticks_since_start()); }

// The locked cache's DMA engine copies whole 32-byte cache lines between main memory and
// the locked cache (the game's LCLoadBlocks and LCStoreBlocks). DMA_U holds the memory
// address and the high bits of the line count; writing DMA_L with its trigger bit starts
// the copy, which finishes at once here.
void run_locked_cache_transfer(uint32_t upper, uint32_t lower) {
    constexpr uint32_t TRIGGER = 0x2u;
    constexpr uint32_t LOAD_INTO_CACHE = 0x10u;
    constexpr uint32_t CACHE_LINE_SIZE = 32u;
    if ((lower & TRIGGER) == 0) {
        return;
    }
    uint32_t lines = ((upper & 0x1Fu) << 2) | ((lower >> 2) & 0x3u);
    if (lines == 0) {
        lines = 128;  // a count of zero means the largest transfer
    }
    const uint32_t memory_address = upper & ~0x1Fu;
    const uint32_t cache_address = lower & ~0x1Fu;
    const size_t length = size_t{lines} * CACHE_LINE_SIZE;
    uint8_t* memory = guest_memory::host_pointer(memory_address, length);
    uint8_t* cache = guest_memory::host_pointer(cache_address, length);
    if (memory == nullptr || cache == nullptr) {
        fault_handler::report_fatal("locked-cache DMA outside memory");
    }
    if (lower & LOAD_INTO_CACHE) {
        std::memcpy(cache, memory, length);
    } else {
        std::memcpy(memory, cache, length);
    }
}

}  // namespace

uint64_t time_base() { return ticks_since_start() + static_cast<uint64_t>(g_time_base_adjustment); }

uint64_t decrementer_deadline() {
    return g_decrementer_armed ? g_decrementer_written_at + g_decrementer_written + 1 : UINT64_MAX;
}

bool take_decrementer_exception() {
    if (!g_decrementer_armed || time_base() < decrementer_deadline()) {
        return false;
    }
    g_decrementer_armed = false;
    return true;
}

uint32_t read_special_register(CpuContext* cpu, uint32_t number) {
    if (number >= FIRST_QUANTIZATION && number <= LAST_QUANTIZATION) {
        return cpu->gqr[number - FIRST_QUANTIZATION];
    }
    switch (number) {
        case INTEGER_EXCEPTION:
            return cpu->xer;
        case LINK:
            return cpu->lr;
        case COUNT:
            return cpu->ctr;
        case SAVE_RESTORE0:
            return cpu->srr0;
        case SAVE_RESTORE1:
            return cpu->srr1;
        case TIME_BASE_LOW_READ:
            return static_cast<uint32_t>(time_base());
        case TIME_BASE_HIGH_READ:
            return static_cast<uint32_t>(time_base() >> 32);
        case PROCESSOR_VERSION:
            return GEKKO_PROCESSOR_VERSION;
        case HARDWARE_IMPLEMENTATION0:
            return cpu->hid0;
        case HARDWARE_IMPLEMENTATION1:
            return cpu->hid1;
        case HARDWARE_IMPLEMENTATION2:
            return cpu->hid2;
        case DECREMENTER:
            return g_decrementer_written - static_cast<uint32_t>(time_base() - g_decrementer_written_at);
        default:
            return g_stored_special_registers[number & 1023u];
    }
}

void write_special_register(CpuContext* cpu, uint32_t number, uint32_t value) {
    if (number >= FIRST_QUANTIZATION && number <= LAST_QUANTIZATION) {
        cpu->gqr[number - FIRST_QUANTIZATION] = value;
        return;
    }
    switch (number) {
        case INTEGER_EXCEPTION:
            cpu->xer = value;
            return;
        case LINK:
            cpu->lr = value;
            return;
        case COUNT:
            cpu->ctr = value;
            return;
        case SAVE_RESTORE0:
            cpu->srr0 = value;
            return;
        case SAVE_RESTORE1:
            cpu->srr1 = value;
            return;
        case TIME_BASE_LOW_WRITE:
            set_time_base((time_base() & 0xFFFFFFFF00000000ull) | value);
            return;
        case TIME_BASE_HIGH_WRITE:
            set_time_base((time_base() & 0xFFFFFFFFull) | (uint64_t{value} << 32));
            return;
        case HARDWARE_IMPLEMENTATION0:
            cpu->hid0 = value;
            return;
        case HARDWARE_IMPLEMENTATION1:
            cpu->hid1 = value;
            return;
        case HARDWARE_IMPLEMENTATION2:
            cpu->hid2 = value;
            return;
        case DECREMENTER:
            // The exception comes when the count goes from 0 to -1; a negative value
            // written is already past that point.
            g_decrementer_written = value;
            g_decrementer_written_at = time_base();
            g_decrementer_armed = static_cast<int32_t>(value) >= 0;
            return;
        case DIRECT_MEMORY_ACCESS_LOWER:
            g_stored_special_registers[number] = value & ~0x2u;  // the trigger bit reads back as done
            run_locked_cache_transfer(g_stored_special_registers[DIRECT_MEMORY_ACCESS_UPPER], value);
            return;
        default:
            g_stored_special_registers[number & 1023u] = value;
            return;
    }
}

}  // namespace cpu_registers

// ---- Guest ABI ----

namespace {
// The CPU of the guest thread that is calling the helper.
CpuContext* running_cpu() {
    CpuContext* cpu = TryGetCpuContext();
    if (cpu == nullptr) {
        fault_handler::report_fatal("a CPU helper ran outside of guest code");
    }
    return cpu;
}
}  // namespace

CpuContext& GetPersistentCpuContext() {
    thread_local CpuContext context{};
    return context;
}

extern "C" uint32_t PPC_ReadSpr(uint32_t spr) { return cpu_registers::read_special_register(running_cpu(), spr); }

extern "C" void PPC_WriteSpr(uint32_t spr, uint32_t value) {
    cpu_registers::write_special_register(running_cpu(), spr, value);
}

extern "C" uint32_t PPC_Mftb() { return static_cast<uint32_t>(cpu_registers::time_base()); }

extern "C" uint32_t PPC_Mftbu() { return static_cast<uint32_t>(cpu_registers::time_base() >> 32); }

// mcrxr: copy XER[SO, OV, CA] into a condition register field and clear them in XER.
extern "C" uint32_t PPC_Mcrxr(uint32_t cr_field) {
    CpuContext* cpu = running_cpu();
    const uint32_t shift = (7u - (cr_field & 7u)) * 4u;
    cpu->cr = (cpu->cr & ~(0xFu << shift)) | (((cpu->xer >> 28) & 0xFu) << shift);
    cpu->xer &= ~0xF0000000u;
    return cpu->cr;
}

// tw/twi: the game traps on purpose (failed assertions of the SDK), so a trap that fires
// is reported where it happened.
extern "C" void PPC_TrapWord(uint32_t conditions, uint32_t left, uint32_t right) {
    const auto signed_left = static_cast<int32_t>(left);
    const auto signed_right = static_cast<int32_t>(right);
    const bool fires = ((conditions & 0x10u) && signed_left < signed_right) ||
                       ((conditions & 0x08u) && signed_left > signed_right) ||
                       ((conditions & 0x04u) && left == right) || ((conditions & 0x02u) && left < right) ||
                       ((conditions & 0x01u) && left > right);
    if (fires) {
        fault_handler::report_fatal("the game executed a trap instruction (tw)");
    }
}

// sc: the OS's only system call is PPCSync, which makes the CPU finish its pending
// memory accesses (the graphics code uses it around FIFO changes). Translated code has
// nothing pending, so there is nothing to do.
extern "C" uint32_t OSSystemCall() { return 0; }

// psq_l/psq_st whose quantization register is chosen at run time (the translator only
// emits these when it cannot prove which GQR is used).
extern "C" double PPC_PsqL(uint32_t addr, uint32_t w, uint32_t i) {
    const uint32_t gqr = running_cpu()->gqr[i & 7u];
    return w == 0 ? PPC_PsqLStateFallback<0u, false>(gqr, addr) : PPC_PsqLStateFallback<1u, false>(gqr, addr);
}

extern "C" void PPC_PsqSt(uint32_t addr, double value, uint32_t w, uint32_t i) {
    const uint32_t gqr = running_cpu()->gqr[i & 7u];
    if (w == 0) {
        PPC_PsqStStateFallback<0u, false>(gqr, addr, value);
    } else {
        PPC_PsqStStateFallback<1u, false>(gqr, addr, value);
    }
}
