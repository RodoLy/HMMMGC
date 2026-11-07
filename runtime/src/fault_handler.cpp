// See fault_handler.h. The part every host system shares: the reports. Catching the
// faults is in fault_handler_posix.cpp and fault_handler_windows.cpp.

#include "fault_handler.h"

#include "fault_handler_host.h"
#include "guest_memory.h"
#include "guest_symbols.h"
#include "runtime_log.h"

#include "guest_abi/guest_abi.h"

#include <cstdio>
#include <cstdlib>

namespace fault_handler {
namespace {

bool is_guest_stack_address(uint32_t address) {
    return (address & 3u) == 0 && guest_memory::host_pointer(address, 8) != nullptr;
}

}  // namespace

void print_guest_location() {
    const uint32_t translated_function = RecompMod::g_currentTranslatedExecutionAddress;
    std::fprintf(stderr, "  last dispatched function: %s\n", guest_symbols::describe(translated_function).c_str());
    const CpuContext* cpu = TryGetCpuContext();
    if (cpu == nullptr) {
        std::fprintf(stderr, "  (no guest CPU context on this thread)\n");
        return;
    }
    std::fprintf(stderr, "  lr %s  r1 0x%08X  r3 0x%08X  r4 0x%08X  r5 0x%08X  msr 0x%08X\n",
                 guest_symbols::describe(cpu->lr).c_str(), cpu->gpr[1], cpu->gpr[3], cpu->gpr[4], cpu->gpr[5],
                 cpu->msr);
    // The guest stack: each frame starts with the address of the caller's frame, and the
    // word after that frame's start is where the callee saved its return address.
    std::fprintf(stderr, "  guest call stack:\n");
    uint32_t frame = cpu->gpr[1];
    for (int depth = 0; depth < 24 && is_guest_stack_address(frame); ++depth) {
        const uint32_t caller_frame = guest_memory::read32(frame);
        if (!is_guest_stack_address(caller_frame) || caller_frame <= frame) {
            break;
        }
        const uint32_t return_address = guest_memory::read32(caller_frame + 4);
        std::fprintf(stderr, "    %s\n", guest_symbols::describe(return_address).c_str());
        frame = caller_frame;
    }
}

void report_invalid_access(const char* kind, uint32_t guest_address, size_t size) {
    runtime_log::error("memory", "invalid guest %s of %zu byte(s) at 0x%08X", kind, size, guest_address);
    print_guest_location();
    print_host_backtrace(24);
    std::_Exit(3);
}

void report_fatal(const char* reason) {
    runtime_log::error("runtime", "%s", reason);
    print_guest_location();
    print_host_backtrace(24);
    std::_Exit(3);
}

}  // namespace fault_handler
