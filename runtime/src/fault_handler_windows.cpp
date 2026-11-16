// fault_handler.h on Windows: a vectored exception handler sees the faults. A read of a
// hardware register through a pointer is carried out here on ARM64 (see fault_handler.h);
// any other fault in the guest space is reported. Faults outside the guest space are left
// to whoever handles them, and reported if nobody does.

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

#if defined(_M_ARM64) || defined(__aarch64__)

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
