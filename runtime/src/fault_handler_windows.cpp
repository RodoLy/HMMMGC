// fault_handler.h on Windows: a vectored exception handler sees the faults. A read of a
// hardware register through a pointer is carried out here on ARM64 (see fault_handler.h);
// any other fault in the guest space is reported. Faults outside the guest space are left
// to whoever handles them, and reported if nobody does.

#if defined(_WIN32)

#include "fault_handler.h"

#include "arm64_load.h"
#include "fault_handler_host.h"
#include "guest_memory.h"
#include "hardware/hardware_registers.h"
#include "runtime_log.h"

#include "guest_abi/guest_abi.h"

#include <windows.h>

#include <chrono>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <thread>

namespace fault_handler {
namespace {

constexpr ULONG_PTR ACCESS_WAS_WRITE = 1;  // ExceptionInformation[0] of an access violation

bool is_inside_guest_space(uintptr_t host_address) {
    const auto flat_base = reinterpret_cast<uintptr_t>(guest_memory::flat_base());
    return flat_base != 0 && host_address - flat_base < GuestFlat::kGuestSpaceSize;
}

#if defined(_M_ARM64) || defined(__aarch64__)

uint64_t general_register(const CONTEXT& context, unsigned number, bool number_31_is_stack_pointer) {
    if (number == 31) {
        return number_31_is_stack_pointer ? context.Sp : 0;
    }
    return context.X[number];
}

// Carries out the load at the faulting instruction from the current value of the
// register, and steps over it. False when the instruction is not a load this knows.
bool emulate_register_read(CONTEXT& context) {
    uint32_t instruction = 0;
    std::memcpy(&instruction, reinterpret_cast<const void*>(context.Pc), sizeof(instruction));
    const std::optional<arm64_load::Load> load = arm64_load::decode(instruction);
    if (!load) {
        runtime_log::error("memory", "a register read through a pointer uses instruction 0x%08X, which is not handled",
                           instruction);
        return false;
    }
    const uint64_t base = general_register(context, load->base, true);
    const int64_t offset = load->uses_offset_register
                               ? static_cast<int64_t>(arm64_load::extended_offset(
                                     *load, general_register(context, load->offset_register, false)))
                               : load->immediate_offset;
    const uint64_t address = load->post_index ? base : base + offset;
    const auto guest_address = static_cast<uint32_t>(address - reinterpret_cast<uintptr_t>(guest_memory::flat_base()));
    if (!is_inside_guest_space(address) || !guest_memory::is_hardware_register(guest_address) ||
        !guest_memory::is_hardware_register(guest_address + load->size - 1)) {
        return false;
    }

    // Bring the registers it covers up to date, then read them as the instruction would.
    const auto* pc = reinterpret_cast<const void*>(context.Pc);
    for (unsigned covered = 0; covered < load->size; covered += 4) {
        hardware_registers::prepare_trapped_read(guest_address + covered, pc);
    }
    uint8_t bytes[16] = {};
    std::memcpy(bytes,
                guest_memory::hardware_register_image() + (guest_address - guest_memory::HARDWARE_REGISTERS_BASE),
                load->size);

    if (load->to_vector_register) {
        ARM64_NT_NEON128& vector = context.V[load->destination];
        std::memset(&vector, 0, sizeof(vector));
        std::memcpy(&vector, bytes, load->size);
    } else if (load->destination != 31) {
        uint64_t value = 0;
        std::memcpy(&value, bytes, load->size);
        if (load->sign_extend) {
            const unsigned unused_bits = 64 - load->size * 8;
            value = static_cast<uint64_t>(static_cast<int64_t>(value << unused_bits) >> unused_bits);
            if (!load->extend_to_64_bits) {
                value = static_cast<uint32_t>(value);
            }
        }
        context.X[load->destination] = value;
    }
    if (load->writes_back) {
        if (load->base == 31) {
            context.Sp = base + offset;
        } else {
            context.X[load->base] = base + offset;
        }
    }
    context.Pc += 4;
    return true;
}

#endif

LONG CALLBACK on_exception(EXCEPTION_POINTERS* exception) {
    const EXCEPTION_RECORD& record = *exception->ExceptionRecord;
    if (record.ExceptionCode != EXCEPTION_ACCESS_VIOLATION || record.NumberParameters < 2) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const uintptr_t fault_address = record.ExceptionInformation[1];
    if (!is_inside_guest_space(fault_address)) {
        return EXCEPTION_CONTINUE_SEARCH;
    }
    const auto guest_address =
        static_cast<uint32_t>(fault_address - reinterpret_cast<uintptr_t>(guest_memory::flat_base()));
    const bool is_write = record.ExceptionInformation[0] == ACCESS_WAS_WRITE;
#if defined(_M_ARM64) || defined(__aarch64__)
    if (!is_write && guest_memory::is_hardware_register(guest_address) &&
        emulate_register_read(*exception->ContextRecord)) {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
#endif
    report_invalid_access(is_write ? "write" : "read", guest_address, 0);
}

LONG CALLBACK on_unhandled_exception(EXCEPTION_POINTERS* exception) {
    char reason[96];
    std::snprintf(reason, sizeof(reason), "host exception 0x%08lX at %p",
                  static_cast<unsigned long>(exception->ExceptionRecord->ExceptionCode),
                  exception->ExceptionRecord->ExceptionAddress);
    report_fatal(reason);
}

void on_abort(int) {
    // Restore the default action first, so that the abort below really ends the program.
    std::signal(SIGABRT, SIG_DFL);
    runtime_log::error("runtime", "abort");
    print_guest_location();
    print_host_backtrace(24);
    std::abort();
}

// Windows has no timer signal: the watchdog is a thread. It cannot see the guest
// registers of the game's thread, only the last function the game entered.
void run_watchdog(unsigned seconds) {
    while (true) {
        std::this_thread::sleep_for(std::chrono::seconds(seconds));
        runtime_log::info("watchdog",
                          "still running (%s); the game is at:", hardware_registers::describe_activity().c_str());
        print_guest_location();
    }
}

}  // namespace

void print_host_backtrace(int maximum_frames) {
    void* frames[64];
    const USHORT count = CaptureStackBackTrace(1, maximum_frames < 64 ? maximum_frames : 64, frames, nullptr);
    for (USHORT i = 0; i < count; ++i) {
        // Module and offset: enough to find the function with the linker's map.
        HMODULE module = nullptr;
        char name[MAX_PATH] = "?";
        if (GetModuleHandleExA(GET_MODULE_HANDLE_EX_FLAG_FROM_ADDRESS | GET_MODULE_HANDLE_EX_FLAG_UNCHANGED_REFCOUNT,
                               static_cast<LPCSTR>(frames[i]), &module) &&
            GetModuleFileNameA(module, name, sizeof(name)) != 0) {
            const char* file = std::strrchr(name, '\\');
            std::fprintf(stderr, "  %s+0x%llX\n", file != nullptr ? file + 1 : name,
                         static_cast<unsigned long long>(reinterpret_cast<uintptr_t>(frames[i]) -
                                                         reinterpret_cast<uintptr_t>(module)));
        } else {
            std::fprintf(stderr, "  %p\n", frames[i]);
        }
    }
}

void install(unsigned watchdog_seconds) {
    AddVectoredExceptionHandler(1, on_exception);
    SetUnhandledExceptionFilter(on_unhandled_exception);
    std::signal(SIGABRT, on_abort);
    if (watchdog_seconds != 0) {
        std::thread(run_watchdog, watchdog_seconds).detach();
    }
}

}  // namespace fault_handler

#endif  // defined(_WIN32)
