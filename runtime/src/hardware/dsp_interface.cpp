// See dsp_interface.h. Register layout and the DSP's behaviour from Dolphin's DSP.cpp and
// its DSP-HLE (DSPHLE.cpp, MailHandler.cpp, UCodes/ROM.cpp, UCodes/INIT.cpp).

#include "hardware/dsp_interface.h"

#include "cpu_registers.h"
#include "guest_memory.h"
#include "hardware/audio_memory.h"
#include "hardware/processor_interface.h"
#include "hardware/register_image.h"
#include "runtime_log.h"

#include <cstring>
#include <deque>

namespace dsp_interface {
namespace {

constexpr uint32_t MAIL_TO_DSP_HIGH = BASE + 0x00;
constexpr uint32_t MAIL_FROM_DSP_HIGH = BASE + 0x04;
constexpr uint32_t MAIL_FROM_DSP_LOW = BASE + 0x06;
constexpr uint32_t CONTROL = BASE + 0x0A;
constexpr uint32_t ARAM_MODE = BASE + 0x16;
constexpr uint32_t ARAM_DMA_MEMORY_ADDRESS = BASE + 0x20;  // high half at 0x20, low half at 0x22
constexpr uint32_t ARAM_DMA_ARAM_ADDRESS = BASE + 0x24;    // 0x24 and 0x26
constexpr uint32_t ARAM_DMA_COUNT = BASE + 0x28;           // 0x28 and 0x2A; writing the low half starts it

// Bits of the control register (Dolphin's UDSPControl).
constexpr uint16_t RESET = 0x0001u;
constexpr uint16_t HALT = 0x0004u;
constexpr uint16_t AUDIO_DMA_INTERRUPT = 0x0008u;
constexpr uint16_t AUDIO_DMA_INTERRUPT_MASK = 0x0010u;
constexpr uint16_t ARAM_DMA_INTERRUPT = 0x0020u;
constexpr uint16_t ARAM_DMA_INTERRUPT_MASK = 0x0040u;
constexpr uint16_t DSP_INTERRUPT = 0x0080u;
constexpr uint16_t DSP_INTERRUPT_MASK = 0x0100u;
constexpr uint16_t INIT_CODE_RUNNING = 0x0400u;
constexpr uint16_t INIT = 0x0800u;
constexpr uint16_t INTERRUPT_STATUS_BITS = AUDIO_DMA_INTERRUPT | ARAM_DMA_INTERRUPT | DSP_INTERRUPT;

// The mails Nintendo's DSP code sends when it starts: the boot ROM after a reset, and the
// small "init" program that OSInit runs once (Dolphin's DSP-HLE ROM and INIT microcodes
// send the same values).
constexpr uint32_t MAIL_FROM_BOOT_ROM = 0x8071FEEDu;
constexpr uint32_t MAIL_FROM_INIT_PROGRAM = 0x80544348u;
// How long the init program runs, in time-base ticks (Dolphin's measurement).
constexpr uint64_t INIT_PROGRAM_TICKS = 130;

constexpr uint32_t DMA_ALIGNMENT_MASK = 0x1Fu;  // the DMA moves 32-byte units
constexpr uint32_t PHYSICAL_ADDRESS_MASK = 0x03FFFFFFu;
constexpr uint32_t FROM_ARAM_TO_MEMORY = 0x80000000u;

struct DspState {
    uint16_t control = 0;
    std::deque<uint32_t> mails_from_dsp;
    uint32_t last_mail_from_dsp = 0;
    uint64_t init_program_ends_at = 0;
};
DspState g_dsp;

void update_interrupt_line() {
    const uint16_t control = g_dsp.control;
    const bool asserted = ((control & AUDIO_DMA_INTERRUPT) && (control & AUDIO_DMA_INTERRUPT_MASK)) ||
                          ((control & ARAM_DMA_INTERRUPT) && (control & ARAM_DMA_INTERRUPT_MASK)) ||
                          ((control & DSP_INTERRUPT) && (control & DSP_INTERRUPT_MASK));
    processor_interface::set_interrupt(processor_interface::DSP_INTERRUPT, asserted);
}

// A program starting on the DSP replaces whatever mails were waiting with its greeting.
void start_program(uint32_t greeting) {
    g_dsp.mails_from_dsp.clear();
    g_dsp.mails_from_dsp.push_back(greeting);
}

void write_control(uint16_t value) {
    const uint16_t before = g_dsp.control;
    uint16_t control = (before & INTERRUPT_STATUS_BITS & ~value) |  // writing 1 acknowledges
                       (value & ~(INTERRUPT_STATUS_BITS | RESET | INIT_CODE_RUNNING));
    if (value & RESET) {
        start_program(MAIL_FROM_BOOT_ROM);
    }
    if ((before & INIT) && !(value & INIT)) {
        start_program(MAIL_FROM_INIT_PROGRAM);
        g_dsp.init_program_ends_at = cpu_registers::time_base() + INIT_PROGRAM_TICKS;
    }
    if (before & INIT_CODE_RUNNING) {
        control |= INIT_CODE_RUNNING;
    }
    g_dsp.control = control;
    update_interrupt_line();
}

uint16_t read_control() {
    if (cpu_registers::time_base() < g_dsp.init_program_ends_at) {
        g_dsp.control |= INIT_CODE_RUNNING;
    } else {
        g_dsp.control &= ~INIT_CODE_RUNNING;
    }
    return g_dsp.control;
}

// The DMA engine copies between main memory and ARAM; on the console it takes time and
// interrupts at the end, here it is done at once. There is no expansion ARAM, so ARAM
// addresses past 16 MiB wrap around, which is how the game's size check (__ARChecksize)
// finds 16 MiB.
void run_aram_dma() {
    const uint32_t count = register_image::read_pair(ARAM_DMA_COUNT);
    const uint32_t length = count & ~FROM_ARAM_TO_MEMORY & ~DMA_ALIGNMENT_MASK;
    const uint32_t memory_address =
        register_image::read_pair(ARAM_DMA_MEMORY_ADDRESS) & PHYSICAL_ADDRESS_MASK & ~DMA_ALIGNMENT_MASK;
    const uint32_t aram_address =
        register_image::read_pair(ARAM_DMA_ARAM_ADDRESS) & (audio_memory::SIZE - 1) & ~DMA_ALIGNMENT_MASK;
    uint8_t* memory = guest_memory::host_pointer(guest_memory::physical_to_cached(memory_address), length);
    if (memory != nullptr && aram_address + uint64_t{length} <= audio_memory::SIZE) {
        uint8_t* aram = audio_memory::data() + aram_address;
        if (count & FROM_ARAM_TO_MEMORY) {
            std::memcpy(memory, aram, length);
        } else {
            std::memcpy(aram, memory, length);
        }
    } else {
        runtime_log::warning("dsp", "ARAM DMA out of range: memory 0x%08X, ARAM 0x%08X, 0x%X bytes", memory_address,
                             aram_address, length);
    }
    // The count reads back as zero once the transfer is over (its direction bit stays).
    register_image::write16(ARAM_DMA_COUNT, static_cast<uint16_t>((count & FROM_ARAM_TO_MEMORY) >> 16));
    register_image::write16(ARAM_DMA_COUNT + 2, 0);
    g_dsp.control |= ARAM_DMA_INTERRUPT;
    update_interrupt_line();
}

}  // namespace

void write(uint32_t address, uint16_t value) {
    switch (address) {
        case CONTROL:
            write_control(value);
            return;
        case MAIL_FROM_DSP_HIGH:
        case MAIL_FROM_DSP_LOW:
            return;  // only the DSP writes these
        case ARAM_DMA_COUNT + 2:
            register_image::write16(address, value);
            run_aram_dma();
            return;
        default:
            register_image::write16(address, value);
            return;
    }
}

// Mails from the DSP can be seen while it runs (not halted); reading the low half takes
// the mail, after which the high half reads with its "mail waiting" bit clear.
void refresh(uint32_t address) {
    const bool halted = (g_dsp.control & HALT) != 0;
    switch (address) {
        case CONTROL:
            register_image::write16(address, read_control());
            return;
        case MAIL_FROM_DSP_HIGH:
            if (!halted && !g_dsp.mails_from_dsp.empty()) {
                g_dsp.last_mail_from_dsp = g_dsp.mails_from_dsp.front();
            }
            register_image::write16(address, static_cast<uint16_t>(g_dsp.last_mail_from_dsp >> 16));
            return;
        case MAIL_FROM_DSP_LOW:
            if (!halted && !g_dsp.mails_from_dsp.empty()) {
                g_dsp.last_mail_from_dsp = g_dsp.mails_from_dsp.front();
                g_dsp.mails_from_dsp.pop_front();
            }
            register_image::write16(address, static_cast<uint16_t>(g_dsp.last_mail_from_dsp));
            g_dsp.last_mail_from_dsp &= ~0x80000000u;
            return;
        case MAIL_TO_DSP_HIGH:
            // The DSP takes every mail at once, so the "mail pending" bit is never set.
            register_image::write16(address, register_image::read16(address) & 0x7FFFu);
            return;
        case ARAM_MODE:
            register_image::write16(address, 1);  // ARAM ready
            return;
        default:
            return;
    }
}

}  // namespace dsp_interface
