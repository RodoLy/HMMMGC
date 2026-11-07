// See guest_os.h. The addresses of the OS's variables are those of this game's copy of
// the SDK (found by reading SelectThread, __OSSetExceptionHandler and their callers);
// the structure layouts are the Dolphin SDK's OSContext and OSThread.
//
// Ver guest_os.h.

#include "os/guest_os.h"

#include "cpu_registers.h"
#include "fault_handler.h"
#include "guest_memory.h"
#include "hardware/hardware_registers.h"
#include "native_functions.h"
#include "runtime_log.h"

#include <chrono>
#include <thread>

namespace guest_os {
namespace {

constexpr uint32_t EXTERNAL_INTERRUPTS_ENABLED = 0x8000u;  // MSR[EE]

// Exception numbers of the OS (its __OSException values), which index its table of
// exception handlers.
constexpr uint32_t EXTERNAL_INTERRUPT_EXCEPTION = 4;
constexpr uint32_t DECREMENTER_EXCEPTION = 8;

// OS globals in low memory (YAGCD 4.2.1).
constexpr uint32_t CURRENT_CONTEXT_ADDRESS = 0x800000D4u;  // OSGetCurrentContext()
constexpr uint32_t CURRENT_THREAD_ADDRESS = 0x800000E4u;   // OSGetCurrentThread()

// Variables of this game's OS. The ready bits (the SDK's RunQueueBits) have bit 31 - p set
// while a thread of priority p is ready (0 is the highest priority); the ready queues
// (RunQueue) hold the head and the tail of the threads of each priority.
constexpr uint32_t EXCEPTION_HANDLER_TABLE_POINTER = 0x8032209Cu;
constexpr uint32_t READY_PRIORITIES_ADDRESS = 0x80322138u;
constexpr uint32_t RESCHEDULE_HINT_ADDRESS = 0x8032213Cu;  // RunQueueHint
constexpr uint32_t SCHEDULER_DISABLED_COUNT_ADDRESS = 0x80322140u;
constexpr uint32_t SWITCH_THREAD_CALLBACK_ADDRESS = 0x80321A10u;
constexpr uint32_t READY_QUEUES_ADDRESS = 0x802F9DB8u;
constexpr uint32_t READY_QUEUE_SIZE = 8;
constexpr uint32_t IDLE_CONTEXT_ADDRESS = READY_QUEUES_ADDRESS + 0x730u;

// OSContext (Dolphin SDK).
constexpr uint32_t CONTEXT_GENERAL_REGISTERS = 0x000;
constexpr uint32_t CONTEXT_CONDITION_REGISTER = 0x080;
constexpr uint32_t CONTEXT_LINK_REGISTER = 0x084;
constexpr uint32_t CONTEXT_COUNT_REGISTER = 0x088;
constexpr uint32_t CONTEXT_INTEGER_EXCEPTION = 0x08C;
constexpr uint32_t CONTEXT_SAVE_RESTORE0 = 0x198;
constexpr uint32_t CONTEXT_SAVE_RESTORE1 = 0x19C;
constexpr uint32_t CONTEXT_STATE = 0x1A2;
constexpr uint16_t CONTEXT_SAVED_BY_EXCEPTION = 0x0002;

// OSThread (Dolphin SDK); the thread starts with its OSContext.
constexpr uint32_t THREAD_STATE = 0x2C8;
constexpr uint32_t THREAD_PRIORITY = 0x2D0;
constexpr uint32_t THREAD_QUEUE = 0x2DC;
constexpr uint32_t THREAD_NEXT = 0x2E0;
constexpr uint32_t THREAD_PREVIOUS = 0x2E4;
constexpr uint16_t THREAD_READY = 1;
constexpr uint16_t THREAD_RUNNING = 2;

// Guest functions the natives call, as SelectThread does.
constexpr uint32_t OS_SAVE_CONTEXT = 0x8017100Cu;
constexpr uint32_t OS_SET_CURRENT_CONTEXT = 0x80170FA4u;
constexpr uint32_t OS_CLEAR_CONTEXT = 0x8017116Cu;

// Thrown by OSLoadContext at the end of an interrupt handler, and caught where the
// interrupt was taken: on the console OSLoadContext never returns, it jumps back to the
// interrupted code, so here it unwinds the handler's frames back to that point.
struct ReturnFromInterrupt {
    uint32_t context;
};

bool g_taking_interrupt = false;
uint32_t g_main_thread = 0;  // the game's only thread: the one running at the first SelectThread

// Marks that an interrupt handler is running, until the end of the scope.
class InterruptInProgress {
public:
    InterruptInProgress() { g_taking_interrupt = true; }
    ~InterruptInProgress() { g_taking_interrupt = false; }
    InterruptInProgress(const InterruptInProgress&) = delete;
    InterruptInProgress& operator=(const InterruptInProgress&) = delete;
};

using guest_memory::read16;
using guest_memory::read32;
using guest_memory::write16;
using guest_memory::write32;

// Calls the guest function `function` with up to two arguments, as a call from the
// translated code that called the native would.
uint32_t call_guest(CpuContext* cpu, uint32_t function, uint32_t first = 0, uint32_t second = 0) {
    cpu->gpr[3] = first;
    cpu->gpr[4] = second;
    InvokeIndirectCpu(function, cpu);
    return cpu->gpr[3];
}

// cntlzw of the ready bits: the highest ready priority (0 is the highest), 32 if none.
int32_t highest_ready_priority() {
    const uint32_t ready = read32(READY_PRIORITIES_ADDRESS);
    return ready == 0 ? 32 : __builtin_clz(ready);
}

// SetRun of the OS: puts `thread` at the end of the ready queue of its priority.
void make_ready(uint32_t thread) {
    const uint32_t priority = read32(thread + THREAD_PRIORITY);
    const uint32_t queue = READY_QUEUES_ADDRESS + priority * READY_QUEUE_SIZE;
    write32(thread + THREAD_QUEUE, queue);
    const uint32_t previous = read32(queue + 4);
    if (previous == 0) {
        write32(queue, thread);
    } else {
        write32(previous + THREAD_NEXT, thread);
    }
    write32(thread + THREAD_PREVIOUS, previous);
    write32(thread + THREAD_NEXT, 0);
    write32(queue + 4, thread);
    write32(READY_PRIORITIES_ADDRESS, read32(READY_PRIORITIES_ADDRESS) | (1u << (31 - priority)));
    write32(RESCHEDULE_HINT_ADDRESS, 1);
}

// Takes the first thread of the highest-priority ready queue.
uint32_t take_highest_priority_thread() {
    const uint32_t priority = static_cast<uint32_t>(__builtin_clz(read32(READY_PRIORITIES_ADDRESS)));
    const uint32_t queue = READY_QUEUES_ADDRESS + priority * READY_QUEUE_SIZE;
    const uint32_t thread = read32(queue);
    const uint32_t next = read32(thread + THREAD_NEXT);
    if (next == 0) {
        write32(queue + 4, 0);
    } else {
        write32(next + THREAD_PREVIOUS, 0);
    }
    write32(queue, next);
    if (next == 0) {
        write32(READY_PRIORITIES_ADDRESS, read32(READY_PRIORITIES_ADDRESS) & ~(1u << (31 - priority)));
    }
    return thread;
}

// Sleeps the host until the next thing that can raise an interrupt is due.
void sleep_until_next_event() {
    const uint64_t now = cpu_registers::time_base();
    uint64_t next = hardware_registers::next_event_time();
    if (const uint64_t alarm = cpu_registers::decrementer_deadline(); alarm < next) {
        next = alarm;
    }
    if (next > now) {
        const uint64_t ticks = next - now;
        std::this_thread::sleep_for(std::chrono::nanoseconds(ticks * 2000u / 81u));
    }
}

// The idle loop of SelectThread: no thread is ready, so wait (interrupts on) until an
// interrupt handler makes one ready.
void wait_for_ready_thread(CpuContext* cpu, uint32_t previous_thread) {
    call_guest(cpu, read32(SWITCH_THREAD_CALLBACK_ADDRESS), previous_thread, 0);
    write32(CURRENT_THREAD_ADDRESS, 0);
    call_guest(cpu, OS_SET_CURRENT_CONTEXT, IDLE_CONTEXT_ADDRESS);
    while (read32(READY_PRIORITIES_ADDRESS) == 0) {
        cpu->msr |= EXTERNAL_INTERRUPTS_ENABLED;
        take_pending_interrupts(cpu);
        if (read32(READY_PRIORITIES_ADDRESS) == 0) {
            sleep_until_next_event();
        }
    }
    cpu->msr &= ~EXTERNAL_INTERRUPTS_ENABLED;
    call_guest(cpu, OS_CLEAR_CONTEXT, IDLE_CONTEXT_ADDRESS);
}

// SelectThread of the OS, with the switch itself replaced: see guest_os.h. Returns what
// the original returns to a thread that resumes, 0.
uint32_t select_thread(CpuContext* cpu, bool yield) {
    if (static_cast<int32_t>(read32(SCHEDULER_DISABLED_COUNT_ADDRESS)) > 0) {
        return 0;
    }
    const uint32_t current = read32(CURRENT_THREAD_ADDRESS);
    if (read32(CURRENT_CONTEXT_ADDRESS) != current) {
        return 0;  // called from an interrupt taken while idle
    }
    if (current != 0) {
        if (g_main_thread == 0) {
            g_main_thread = current;
        }
        if (read16(current + THREAD_STATE) == THREAD_RUNNING) {
            if (!yield && static_cast<int32_t>(read32(current + THREAD_PRIORITY)) <= highest_ready_priority()) {
                return 0;
            }
            write16(current + THREAD_STATE, THREAD_READY);
            make_ready(current);
        }
        if ((read16(current + CONTEXT_STATE) & CONTEXT_SAVED_BY_EXCEPTION) == 0) {
            call_guest(cpu, OS_SAVE_CONTEXT, current);  // keeps the OS's copy of the registers current
        }
    }
    if (read32(READY_PRIORITIES_ADDRESS) == 0) {
        wait_for_ready_thread(cpu, current);
    }
    write32(RESCHEDULE_HINT_ADDRESS, 0);
    const uint32_t next = take_highest_priority_thread();
    write32(next + THREAD_QUEUE, 0);
    write16(next + THREAD_STATE, THREAD_RUNNING);
    call_guest(cpu, read32(SWITCH_THREAD_CALLBACK_ADDRESS), read32(CURRENT_THREAD_ADDRESS), next);
    write32(CURRENT_THREAD_ADDRESS, next);
    call_guest(cpu, OS_SET_CURRENT_CONTEXT, next);
    if (next != g_main_thread) {
        fault_handler::report_fatal("the OS switched to a second thread; this runtime runs one thread only");
    }
    return 0;
}

// Runs the guest's handler of `exception` as the console's exception vector would: the
// interrupted registers are saved in the current OSContext, and the handler gets the
// exception number and that context.
void take_exception(CpuContext* cpu, uint32_t exception) {
    const uint32_t context = read32(CURRENT_CONTEXT_ADDRESS);
    const uint32_t table = read32(EXCEPTION_HANDLER_TABLE_POINTER);
    const uint32_t handler = table != 0 ? read32(table + 4 * exception) : 0;
    if (context == 0 || handler == 0) {
        return;  // the OS has not set this exception up yet
    }
    // The vector saves these; the handler saves r0-r2 and r6-r31 itself.
    for (const unsigned reg : {3u, 4u, 5u}) {
        write32(context + CONTEXT_GENERAL_REGISTERS + 4 * reg, cpu->gpr[reg]);
    }
    write32(context + CONTEXT_CONDITION_REGISTER, cpu->cr);
    write32(context + CONTEXT_LINK_REGISTER, cpu->lr);
    write32(context + CONTEXT_COUNT_REGISTER, cpu->ctr);
    write32(context + CONTEXT_INTEGER_EXCEPTION, cpu->xer);
    write32(context + CONTEXT_SAVE_RESTORE0, cpu->lr);  // where the game goes on: after the native's call
    write32(context + CONTEXT_SAVE_RESTORE1, cpu->msr);
    write16(context + CONTEXT_STATE, read16(context + CONTEXT_STATE) | CONTEXT_SAVED_BY_EXCEPTION);

    CpuContext handler_cpu = *cpu;
    handler_cpu.gpr[3] = exception;
    handler_cpu.gpr[4] = context;
    handler_cpu.srr0 = cpu->lr;
    handler_cpu.srr1 = cpu->msr;
    handler_cpu.msr = cpu->msr & ~EXTERNAL_INTERRUPTS_ENABLED;

    InterruptInProgress interrupt;
    try {
        CpuContextScope scope(&handler_cpu);
        InvokeIndirectCpu(handler, &handler_cpu);
        runtime_log::warning("os", "the handler of exception %u returned without OSLoadContext", exception);
    } catch (const ReturnFromInterrupt& resumed) {
        if (resumed.context != context) {
            runtime_log::error("os", "exception %u was taken with context 0x%08X but its handler resumed 0x%08X",
                               exception, context, resumed.context);
            fault_handler::report_fatal("an interrupt handler resumed a different context");
        }
    }
}

// ---- The natives ----

void OSDisableInterruptsNative(CpuContext* cpu) {
    const uint32_t before = cpu->msr;
    cpu->msr = before & ~EXTERNAL_INTERRUPTS_ENABLED;
    cpu->gpr[3] = (before & EXTERNAL_INTERRUPTS_ENABLED) != 0;
}
PPC_NATIVE_OVERRIDE_VOID(80172824, OSDisableInterruptsNative, (CpuContext * cpu), (cpu));

void OSEnableInterruptsNative(CpuContext* cpu) {
    const uint32_t before = cpu->msr;
    cpu->msr = before | EXTERNAL_INTERRUPTS_ENABLED;
    cpu->gpr[3] = (before & EXTERNAL_INTERRUPTS_ENABLED) != 0;
    take_pending_interrupts(cpu);
}
PPC_NATIVE_OVERRIDE_VOID(80172838, OSEnableInterruptsNative, (CpuContext * cpu), (cpu));

void OSRestoreInterruptsNative(CpuContext* cpu) {
    const uint32_t before = cpu->msr;
    const bool enable = cpu->gpr[3] != 0;
    cpu->msr = enable ? (before | EXTERNAL_INTERRUPTS_ENABLED) : (before & ~EXTERNAL_INTERRUPTS_ENABLED);
    cpu->gpr[3] = (before & EXTERNAL_INTERRUPTS_ENABLED) != 0;
    if (enable) {
        take_pending_interrupts(cpu);
    }
}
PPC_NATIVE_OVERRIDE_VOID(8017284C, OSRestoreInterruptsNative, (CpuContext * cpu), (cpu));

void SelectThreadNative(CpuContext* cpu) { cpu->gpr[3] = select_thread(cpu, cpu->gpr[3] != 0); }
PPC_NATIVE_OVERRIDE_VOID(80174C4C, SelectThreadNative, (CpuContext * cpu), (cpu));

void OSLoadContextNative(CpuContext* cpu) {
    const uint32_t context = cpu->gpr[3];
    if (!g_taking_interrupt) {
        fault_handler::report_fatal("OSLoadContext outside of an interrupt handler");
    }
    write16(context + CONTEXT_STATE, read16(context + CONTEXT_STATE) & ~CONTEXT_SAVED_BY_EXCEPTION);
    throw ReturnFromInterrupt{context};
}
PPC_NATIVE_OVERRIDE_VOID(8017108C, OSLoadContextNative, (CpuContext * cpu), (cpu));

}  // namespace

void take_pending_interrupts(CpuContext* cpu) {
    if ((cpu->msr & EXTERNAL_INTERRUPTS_ENABLED) == 0 || g_taking_interrupt) {
        return;
    }
    hardware_registers::advance_time();
    if (cpu_registers::take_decrementer_exception()) {
        take_exception(cpu, DECREMENTER_EXCEPTION);
    }
    // The OS's dispatcher handles one interrupt and returns, so take them one by one. An
    // interrupt the OS keeps masked stays pending in the hardware until the library that
    // owns it unmasks it, hence the bound on the passes.
    //
    // After each one the devices advance again: a handler can let a device go on, and the
    // audio DMA waits for the game to take the interrupt of each buffer before it plays the
    // next (dsp_interface.h). When the interrupts come in late, the buffers that fell due
    // meanwhile are caught up here, two passes each (the audio DMA's, then the DSP's).
    constexpr int MAXIMUM_INTERRUPTS_AT_ONCE = 32;
    for (int pass = 0; pass < MAXIMUM_INTERRUPTS_AT_ONCE; ++pass) {
        if ((hardware_registers::pending_interrupts() & hardware_registers::enabled_interrupts()) == 0) {
            break;
        }
        take_exception(cpu, EXTERNAL_INTERRUPT_EXCEPTION);
        hardware_registers::advance_time();
    }
}

}  // namespace guest_os
