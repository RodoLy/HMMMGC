// The start of AX's program on the DSP (fn_801811A0, called by AXInit), done here instead of
// translated, because of the wait at its end.
//
// The game fills in the DSP task of AX's program, hands it to the DSP library (DSPAddTask) and then
// waits, in a loop that only reads a flag, until the DSP's first interrupt runs the task's init
// callback (fn_80181110), which sets the flag. On the console the interrupt stops that loop at any
// instruction. Translated code is only interrupted where the game turns interrupts on
// (os/guest_os.h), so the loop alone never sees the flag change: an endless loop with nothing in
// it. On a computer the compiler drops such a loop (C++ lets it assume that a loop with no effects
// ends), and the game goes on: the DSP's interrupt is taken a moment later, when AIRegisterDMACallback
// turns interrupts back on, and the flag is set then. On Android the compiler kept the loop, and the
// game stayed there forever, on a grey screen. This does what the translated function does, without
// the loop, as it runs on a computer.
//
// El arranque del programa de AX en el DSP, hecho aquí en vez de traducido, por la espera del final:
// el juego espera en un bucle vacío a que la interrupción del DSP ponga una bandera. En la consola la
// interrupción corta el bucle; aquí no, y en Android el juego se quedaba ahí para siempre, en gris.
// Esto hace lo mismo que la función traducida, sin el bucle, como ya corre en la computadora.

#include "guest_memory.h"
#include "native_functions.h"

#include "guest_abi/guest_abi.h"

namespace {

using guest_memory::read16;
using guest_memory::write16;
using guest_memory::write32;

// AX's DSP task (a DSPTaskInfo) in the game's data, and what fills it in.
constexpr uint32_t AX_TASK = 0x80302940u;
constexpr uint32_t AX_DSP_PROGRAM = 0x802A75A0u;      // the program's code in main memory
constexpr uint32_t AX_DSP_DATA = 0x802FE940u;         // its data memory's copy
constexpr uint32_t AX_DSP_DATA_SIZE = 0x2000u;
constexpr uint16_t AX_DSP_DATA_ADDRESS = 0x0010u;     // where the data goes in the DSP
constexpr uint16_t AX_DSP_START = 0x0030u;            // where the program starts
constexpr uint32_t AX_INIT_CALLBACK = 0x80181110u;    // sets the flag the loop waited for
constexpr uint32_t AX_RESUME_CALLBACK = 0x8018111Cu;
constexpr uint32_t AX_DONE_CALLBACK = 0x80181174u;

// Small-data offsets from r13.
constexpr int32_t PROGRAM_SIZE = -0x75E0;   // a halfword: the program's length
constexpr int32_t DSP_READY = -0x6CCC;      // the flag the init callback sets
constexpr int32_t DSP_FRAME_DONE = -0x6CC8;
constexpr int32_t DSP_THREAD_QUEUE = -0x6CC0;

// The functions of the game's libraries it calls.
constexpr uint32_t OS_INIT_THREAD_QUEUE = 0x8017494Cu;
constexpr uint32_t DSP_CHECK_INIT = 0x801878ECu;
constexpr uint32_t DSP_INIT = 0x80187828u;
constexpr uint32_t DSP_ADD_TASK = 0x801878F4u;

uint32_t call_guest(CpuContext* cpu, uint32_t function, uint32_t argument = 0) {
    cpu->gpr[3] = argument;
    InvokeIndirectCpu(function, cpu);
    return cpu->gpr[3];
}

void StartAxOnTheDspNative(CpuContext* cpu) {
    const uint32_t small_data = cpu->gpr[13];
    // The task, field by field as the game fills it in (DSPTaskInfo: +0x04 its state, +0x0C the
    // program, +0x10 its length, +0x18 the data, +0x24 and +0x26 where they go in the DSP,
    // +0x28 to +0x30 the callbacks).
    write32(AX_TASK + 0x0C, AX_DSP_PROGRAM);
    write32(AX_TASK + 0x10, read16(small_data + PROGRAM_SIZE));
    write32(AX_TASK + 0x14, 0);
    write32(AX_TASK + 0x18, AX_DSP_DATA);
    write32(AX_TASK + 0x1C, AX_DSP_DATA_SIZE);
    write32(AX_TASK + 0x20, 0);
    write16(AX_TASK + 0x24, AX_DSP_DATA_ADDRESS);
    write16(AX_TASK + 0x26, AX_DSP_START);
    write32(AX_TASK + 0x28, AX_INIT_CALLBACK);
    write32(AX_TASK + 0x2C, AX_RESUME_CALLBACK);
    write32(AX_TASK + 0x30, AX_DONE_CALLBACK);
    write32(AX_TASK + 0x34, 0);
    write32(AX_TASK + 0x04, 0);
    write32(small_data + DSP_READY, 0);
    write32(small_data + DSP_FRAME_DONE, 0);

    call_guest(cpu, OS_INIT_THREAD_QUEUE, small_data + DSP_THREAD_QUEUE);
    if (call_guest(cpu, DSP_CHECK_INIT) == 0) {
        call_guest(cpu, DSP_INIT);
    }
    call_guest(cpu, DSP_ADD_TASK, AX_TASK);
    // No wait for DSP_READY: see above. The DSP has answered already (dsp_programs.h), and its
    // interrupt is taken the next time the game turns interrupts on.
}

}  // namespace

PPC_NATIVE_OVERRIDE_VOID(801811A0, StartAxOnTheDspNative, (CpuContext * cpu), (cpu));
