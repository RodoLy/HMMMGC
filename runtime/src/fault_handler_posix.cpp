// fault_handler.h on Linux x86-64: signal handlers, and the single-step trap for the
// register reads through pointers.

#if !defined(_WIN32)

#include "fault_handler.h"

#include "fault_handler_host.h"
#include "guest_memory.h"
#include "guest_symbols.h"
#include "hardware/hardware_registers.h"
#include "runtime_log.h"

#include "guest_abi/guest_abi.h"

#include <execinfo.h>
#include <signal.h>
#include <sys/mman.h>
#include <sys/time.h>
#include <ucontext.h>
#include <unistd.h>

#include <cstdlib>

namespace fault_handler {
namespace {

constexpr long HOST_PAGE_SIZE = 4096;
constexpr greg_t TRAP_FLAG = 0x100;           // EFLAGS.TF: trap after the next instruction
constexpr greg_t PAGE_FAULT_WAS_WRITE = 0x2;  // bit of the page-fault error code

// The register page opened for the one instruction that is being single-stepped.
thread_local uint8_t* t_opened_register_page = nullptr;

}  // namespace

void print_host_backtrace(int maximum_frames) {
    void* frames[64];
    const int count = backtrace(frames, maximum_frames < 64 ? maximum_frames : 64);
    backtrace_symbols_fd(frames, count, STDERR_FILENO);
}

namespace {

void on_trapped_register_read(uint32_t guest_address, ucontext_t* context) {
    // Bring the register up to date before the load sees it, open its page, and ask the
    // processor to stop again right after the load (handled in on_single_step).
    const auto instruction = reinterpret_cast<const void*>(context->uc_mcontext.gregs[REG_RIP]);
    hardware_registers::prepare_trapped_read(guest_address, instruction);
    auto* page = reinterpret_cast<uint8_t*>(reinterpret_cast<uintptr_t>(guest_memory::flat_base() + guest_address) &
                                            ~(HOST_PAGE_SIZE - 1));
    mprotect(page, HOST_PAGE_SIZE, PROT_READ);
    t_opened_register_page = page;
    context->uc_mcontext.gregs[REG_EFL] |= TRAP_FLAG;
}

void on_segmentation_fault(int, siginfo_t* info, void* raw_context) {
    auto* context = static_cast<ucontext_t*>(raw_context);
    const auto fault_address = reinterpret_cast<uintptr_t>(info->si_addr);
    const auto flat_base = reinterpret_cast<uintptr_t>(guest_memory::flat_base());
    const bool inside_guest_space = flat_base != 0 && fault_address - flat_base < GuestFlat::kGuestSpaceSize;
    if (!inside_guest_space) {
        report_fatal("host segmentation fault outside the guest address space");
    }
    const auto guest_address = static_cast<uint32_t>(fault_address - flat_base);
    const bool is_write = (context->uc_mcontext.gregs[REG_ERR] & PAGE_FAULT_WAS_WRITE) != 0;
    if (guest_memory::is_hardware_register(guest_address) && !is_write) {
        on_trapped_register_read(guest_address, context);
        return;
    }
    report_invalid_access(is_write ? "write" : "read", guest_address, 0);
}

void on_single_step(int, siginfo_t*, void* raw_context) {
    auto* context = static_cast<ucontext_t*>(raw_context);
    if (t_opened_register_page == nullptr) {
        report_fatal("unexpected trap");
    }
    mprotect(t_opened_register_page, HOST_PAGE_SIZE, PROT_NONE);
    t_opened_register_page = nullptr;
    context->uc_mcontext.gregs[REG_EFL] &= ~TRAP_FLAG;
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
    install_handler(SIGTRAP, on_single_step);
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
