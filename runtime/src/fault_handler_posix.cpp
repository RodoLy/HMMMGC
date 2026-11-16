// fault_handler.h on Linux, Android and macOS: signal handlers for the faults, and the
// register reads through pointers. On x86-64 those take the single-step trap: the
// register's page is opened for the one load and closed again when the processor stops
// after it. On ARM64, which has no single-step for a program, the handler carries out the
// load itself (arm64_register_read.h). What differs between the systems is only where
// each keeps the faulting thread's registers in the signal's context.
//
// fault_handler.h en Linux, Android y macOS. En x86-64 las lecturas de registros con un
// puntero usan el paso a paso del procesador; en ARM64 el manejador hace la carga él mismo.

#if !defined(_WIN32)

#include "fault_handler.h"

#include "arm64_register_read.h"
#include "fault_handler_host.h"
#include "guest_memory.h"
#include "guest_symbols.h"
#include "hardware/hardware_registers.h"
#include "runtime_log.h"

#include "guest_abi/guest_abi.h"

#if defined(__ANDROID__)
#include <android/log.h>
#include <unwind.h>
#else
#include <execinfo.h>
#endif
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#if !defined(__APPLE__)
#include <ucontext.h>  // on macOS, ucontext_t comes with signal.h; this header wants _XOPEN_SOURCE
#endif
#include <unistd.h>

#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace fault_handler {

#if defined(__ANDROID__)

// Android's C library has no backtrace(): the frames come from the unwinder.
namespace {
struct Frames {
    void** frames;
    int count;
    int maximum;
};
_Unwind_Reason_Code add_frame(_Unwind_Context* context, void* argument) {
    auto* frames = static_cast<Frames*>(argument);
    if (frames->count >= frames->maximum) {
        return _URC_END_OF_STACK;
    }
    frames->frames[frames->count++] = reinterpret_cast<void*>(_Unwind_GetIP(context));
    return _URC_NO_REASON;
}
}  // namespace

void print_host_backtrace(int maximum_frames) {
    void* frames[64];
    Frames found{frames, 0, maximum_frames < 64 ? maximum_frames : 64};
    _Unwind_Backtrace(add_frame, &found);
    for (int i = 0; i < found.count; ++i) {
        __android_log_print(ANDROID_LOG_ERROR, "HarvestMoonMM", "  %p", found.frames[i]);
    }
}

#else

void print_host_backtrace(int maximum_frames) {
    void* frames[64];
    const int count = backtrace(frames, maximum_frames < 64 ? maximum_frames : 64);
    backtrace_symbols_fd(frames, count, STDERR_FILENO);
}

#endif

namespace {

#if defined(__x86_64__)

constexpr uint64_t TRAP_FLAG = 0x100;           // RFLAGS.TF: trap after the next instruction
constexpr uint64_t PAGE_FAULT_WAS_WRITE = 0x2;  // bit of the page-fault error code
constexpr long REGISTER_PAGE_SIZE = 4096;       // x86-64 pages are 4 KiB on Linux and macOS

// Where Linux and macOS keep, in the signal's context, the three values the single step uses.
#if defined(__APPLE__)
uint64_t& instruction_pointer(ucontext_t* context) { return context->uc_mcontext->__ss.__rip; }
uint64_t& flags(ucontext_t* context) { return context->uc_mcontext->__ss.__rflags; }
uint64_t page_fault_error(const ucontext_t* context) { return context->uc_mcontext->__es.__err; }
#else
uint64_t& instruction_pointer(ucontext_t* context) {
    return reinterpret_cast<uint64_t&>(context->uc_mcontext.gregs[REG_RIP]);
}
uint64_t& flags(ucontext_t* context) { return reinterpret_cast<uint64_t&>(context->uc_mcontext.gregs[REG_EFL]); }
uint64_t page_fault_error(const ucontext_t* context) {
    return static_cast<uint64_t>(context->uc_mcontext.gregs[REG_ERR]);
}
#endif

// The register page opened for the one instruction that is being single-stepped.
thread_local uint8_t* t_opened_register_page = nullptr;

// Brings the register up to date before the load sees it, opens its page, and asks the
// processor to stop again right after the load (handled in on_single_step).
void open_for_one_load(uint32_t guest_address, ucontext_t* context) {
    hardware_registers::prepare_trapped_read(guest_address,
                                             reinterpret_cast<const void*>(instruction_pointer(context)));
    auto* page = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(guest_memory::flat_base() + guest_address) &
                                            ~static_cast<uintptr_t>(REGISTER_PAGE_SIZE - 1));
    mprotect(page, REGISTER_PAGE_SIZE, PROT_READ);
    t_opened_register_page = page;
    flags(context) |= TRAP_FLAG;
}

void on_single_step(int, siginfo_t*, void* raw_context) {
    auto* context = static_cast<ucontext_t*>(raw_context);
    if (t_opened_register_page == nullptr) {
        report_fatal("unexpected trap");
    }
    mprotect(t_opened_register_page, REGISTER_PAGE_SIZE, PROT_NONE);
    t_opened_register_page = nullptr;
    flags(context) &= ~TRAP_FLAG;
}

#elif defined(__aarch64__)

#if defined(__APPLE__)

// The registers of a thread as macOS keeps them: X0 to X28, and the frame pointer (X29) and the
// link register (X30) by their names.
class SignalRegisters final : public arm64_register_read::Registers {
public:
    explicit SignalRegisters(ucontext_t* context) : state_(context->uc_mcontext->__ss), neon_(context->uc_mcontext->__ns) {}
    uint64_t pc() const override { return state_.__pc; }
    void set_pc(uint64_t value) override { state_.__pc = value; }
    uint64_t general(unsigned number) const override {
        return number < 29 ? state_.__x[number] : number == 29 ? state_.__fp : state_.__lr;
    }
    void set_general(unsigned number, uint64_t value) override {
        (number < 29 ? state_.__x[number] : number == 29 ? state_.__fp : state_.__lr) = value;
    }
    uint64_t stack_pointer() const override { return state_.__sp; }
    void set_stack_pointer(uint64_t value) override { state_.__sp = value; }
    uint8_t* vector(unsigned number) override { return reinterpret_cast<uint8_t*>(&neon_.__v[number]); }

private:
    __darwin_arm_thread_state64& state_;
    __darwin_arm_neon_state64& neon_;
};

#else

// The registers of a thread as Linux and Android keep them. The vector registers are in a record
// of the context's extra space (the kernel's struct fpsimd_context), found by its mark.
class SignalRegisters final : public arm64_register_read::Registers {
public:
    explicit SignalRegisters(ucontext_t* context) : context_(context->uc_mcontext) {}
    uint64_t pc() const override { return context_.pc; }
    void set_pc(uint64_t value) override { context_.pc = value; }
    uint64_t general(unsigned number) const override { return context_.regs[number]; }
    void set_general(unsigned number, uint64_t value) override { context_.regs[number] = value; }
    uint64_t stack_pointer() const override { return context_.sp; }
    void set_stack_pointer(uint64_t value) override { context_.sp = value; }
    uint8_t* vector(unsigned number) override {
        constexpr uint32_t FLOATING_POINT_RECORD = 0x46508001;  // FPSIMD_MAGIC
        constexpr size_t VECTORS_OFFSET = 16;  // after the record's mark, its size, fpsr and fpcr
        auto* record = reinterpret_cast<uint8_t*>(context_.__reserved);
        const uint8_t* end = record + sizeof(context_.__reserved);
        while (record + 8 <= end) {
            uint32_t mark = 0;
            uint32_t size = 0;
            std::memcpy(&mark, record, 4);
            std::memcpy(&size, record + 4, 4);
            if (mark == FLOATING_POINT_RECORD) {
                return record + VECTORS_OFFSET + 16 * number;
            }
            if (mark == 0 || size == 0) {
                break;
            }
            record += size;
        }
        return nullptr;
    }

private:
    mcontext_t& context_;
};

#endif

#else
#error "fault_handler_posix.cpp handles x86-64 and ARM64 hosts"
#endif

void on_segmentation_fault(int, siginfo_t* info, void* raw_context) {
    auto* context = static_cast<ucontext_t*>(raw_context);
    const auto fault_address = reinterpret_cast<uintptr_t>(info->si_addr);
    const auto flat_base = reinterpret_cast<uintptr_t>(guest_memory::flat_base());
    const bool inside_guest_space = flat_base != 0 && fault_address - flat_base < GuestFlat::kGuestSpaceSize;
    if (!inside_guest_space) {
        report_fatal("host segmentation fault outside the guest address space");
    }
    const auto guest_address = static_cast<uint32_t>(fault_address - flat_base);
#if defined(__x86_64__)
    const bool is_write = (page_fault_error(context) & PAGE_FAULT_WAS_WRITE) != 0;
    if (guest_memory::is_hardware_register(guest_address) && !is_write) {
        open_for_one_load(guest_address, context);
        return;
    }
    report_invalid_access(is_write ? "write" : "read", guest_address, 0);
#else
    // A load from the registers is carried out here; anything else there (a store) is not a load
    // that arm64_register_read knows, and is reported.
    SignalRegisters registers(context);
    if (guest_memory::is_hardware_register(guest_address) && arm64_register_read::carry_out(registers)) {
        return;
    }
    report_invalid_access("access", guest_address, 0);
#endif
}

void on_abort(int) {
    // Restore the default action first, so that the abort below really ends the program.
    signal(SIGABRT, SIG_DFL);
    runtime_log::error("runtime", "abort");
    print_guest_location();
    print_host_backtrace(24);
    abort();
}

void on_watchdog(int) {
    runtime_log::info("watchdog",
                      "still running (%s); the game is at:", hardware_registers::describe_activity().c_str());
    print_guest_location();
    print_host_backtrace(10);
}

void install_alternate_signal_stack() {
    // A fault caused by a broken stack must still be reported.
    static uint8_t alternate_stack[64 * 1024];
    stack_t stack{};
    stack.ss_sp = alternate_stack;
    stack.ss_size = sizeof(alternate_stack);
    sigaltstack(&stack, nullptr);
}

void install_handler(int signal_number, void (*handler)(int, siginfo_t*, void*)) {
    struct sigaction action {};
    action.sa_sigaction = handler;
    action.sa_flags = SA_SIGINFO | SA_ONSTACK | SA_NODEFER;
    sigemptyset(&action.sa_mask);
    sigaction(signal_number, &action, nullptr);
}

}  // namespace

void install(unsigned watchdog_seconds) {
    install_alternate_signal_stack();
    install_handler(SIGSEGV, on_segmentation_fault);
    install_handler(SIGBUS, on_segmentation_fault);
#if defined(__x86_64__)
    install_handler(SIGTRAP, on_single_step);
#endif
    signal(SIGABRT, on_abort);
    if (watchdog_seconds != 0) {
        signal(SIGALRM, on_watchdog);
        itimerval interval{};
        interval.it_interval.tv_sec = watchdog_seconds;
        interval.it_value.tv_sec = watchdog_seconds;
        setitimer(ITIMER_REAL, &interval, nullptr);
    }
}

}  // namespace fault_handler

#endif  // !defined(_WIN32)
