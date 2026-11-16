// fault_handler.h on Windows: a vectored exception handler sees the faults. A read of a
// hardware register through a pointer is answered here: on x86-64 with the processor's single
// step (the register page is opened for that one load and closed again when the processor stops
// after it, as fault_handler_posix.cpp does on Linux), on ARM64 by carrying out the load
// (arm64_register_read.h). Any other fault in the guest space is reported. Faults outside the
// guest space are left to whoever handles them, and reported if nobody does.
//
// fault_handler.h en Windows. Las lecturas de registros del hardware con un puntero se atienden
// aquí: en x86-64 con el paso a paso del procesador, en ARM64 haciendo la carga.

#if defined(_WIN32)

#include "fault_handler.h"

#include "arm64_register_read.h"
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

#if defined(_M_X64) || defined(__x86_64__)

constexpr ULONG_PTR ACCESS_WAS_READ = 0;    // ExceptionInformation[0] of an access violation
constexpr DWORD TRAP_FLAG = 0x100;          // EFLAGS.TF: trap after the next instruction
constexpr SIZE_T REGISTER_PAGE_SIZE = 4096;  // x86-64 pages are 4 KiB on Windows

// The register page opened for the one instruction that is being single-stepped.
thread_local uint8_t* t_opened_register_page = nullptr;

// Brings the register up to date before the load sees it, opens its page, and asks the
// processor to stop again right after the load (handled in close_after_one_load).
bool open_for_one_load(uint32_t guest_address, CONTEXT& context) {
    hardware_registers::prepare_trapped_read(guest_address, reinterpret_cast<const void*>(context.Rip));
    auto* page = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(guest_memory::flat_base() + guest_address) &
                                            ~static_cast<uintptr_t>(REGISTER_PAGE_SIZE - 1));
    DWORD previous = 0;
    if (!VirtualProtect(page, REGISTER_PAGE_SIZE, PAGE_READONLY, &previous)) {
        return false;
    }
    t_opened_register_page = page;
    context.EFlags |= TRAP_FLAG;
    return true;
}

// The processor stopped after the load: the page is closed again.
LONG close_after_one_load(CONTEXT& context) {
    DWORD previous = 0;
    VirtualProtect(t_opened_register_page, REGISTER_PAGE_SIZE, PAGE_NOACCESS, &previous);
    t_opened_register_page = nullptr;
    context.EFlags &= ~TRAP_FLAG;
    return EXCEPTION_CONTINUE_EXECUTION;
}

#elif defined(_M_ARM64) || defined(__aarch64__)

// The registers of a thread as Windows keeps them in the CONTEXT of an exception.
class WindowsRegisters final : public arm64_register_read::Registers {
public:
    explicit WindowsRegisters(CONTEXT& context) : context_(context) {}
    uint64_t pc() const override { return context_.Pc; }
    void set_pc(uint64_t value) override { context_.Pc = value; }
    uint64_t general(unsigned number) const override { return context_.X[number]; }
    void set_general(unsigned number, uint64_t value) override { context_.X[number] = value; }
    uint64_t stack_pointer() const override { return context_.Sp; }
    void set_stack_pointer(uint64_t value) override { context_.Sp = value; }
    uint8_t* vector(unsigned number) override { return reinterpret_cast<uint8_t*>(&context_.V[number]); }

private:
    CONTEXT& context_;
};

#endif

LONG CALLBACK on_exception(EXCEPTION_POINTERS* exception) {
    const EXCEPTION_RECORD& record = *exception->ExceptionRecord;
#if defined(_M_X64) || defined(__x86_64__)
    if (record.ExceptionCode == EXCEPTION_SINGLE_STEP && t_opened_register_page != nullptr) {
        return close_after_one_load(*exception->ContextRecord);
    }
#endif
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
#if defined(_M_X64) || defined(__x86_64__)
    if (record.ExceptionInformation[0] == ACCESS_WAS_READ && guest_memory::is_hardware_register(guest_address) &&
        open_for_one_load(guest_address, *exception->ContextRecord)) {
        return EXCEPTION_CONTINUE_EXECUTION;
    }
#elif defined(_M_ARM64) || defined(__aarch64__)
    WindowsRegisters registers(*exception->ContextRecord);
    if (!is_write && guest_memory::is_hardware_register(guest_address) && arm64_register_read::carry_out(registers)) {
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
