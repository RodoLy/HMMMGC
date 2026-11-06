// See audio_interface.h. Register layout and behaviour from Dolphin's AudioInterface.cpp.

#include "hardware/audio_interface.h"

#include "cpu_registers.h"
#include "hardware/processor_interface.h"
#include "hardware/register_image.h"

namespace audio_interface {
namespace {

constexpr uint32_t CONTROL = BASE + 0x00;
constexpr uint32_t SAMPLE_COUNTER = BASE + 0x08;
constexpr uint32_t INTERRUPT_TIMING = BASE + 0x0C;

constexpr uint32_t PLAYING = 0x01u;         // PSTAT: the counter runs
constexpr uint32_t RATE_48_KHZ = 0x02u;     // AISFR: 48 kHz instead of 32 kHz
constexpr uint32_t INTERRUPT_MASK = 0x04u;  // AIINTMSK
constexpr uint32_t INTERRUPT = 0x08u;       // AIINT: acknowledged by writing 1
constexpr uint32_t RESET_COUNTER = 0x20u;   // SCRESET: writing 1 resets the counter

struct CounterState {
    uint32_t value_at_start = 0;  // the counter when it last started or was written
    uint64_t started_at = 0;      // the time base then
    uint32_t last_seen = 0;       // the value at the last interrupt check
};
CounterState g_counter;

uint32_t sample_rate() { return (register_image::read32(CONTROL) & RATE_48_KHZ) ? 48000u : 32000u; }

uint32_t sample_counter() {
    if ((register_image::read32(CONTROL) & PLAYING) == 0) {
        return g_counter.value_at_start;
    }
    const uint64_t elapsed = cpu_registers::time_base() - g_counter.started_at;
    return g_counter.value_at_start +
           static_cast<uint32_t>(elapsed * sample_rate() / cpu_registers::TIME_BASE_TICKS_PER_SECOND);
}

void restart_counter(uint32_t value) {
    g_counter.value_at_start = value;
    g_counter.started_at = cpu_registers::time_base();
    g_counter.last_seen = value;
}

void update_interrupt() {
    const uint32_t control = register_image::read32(CONTROL);
    processor_interface::set_interrupt(processor_interface::AUDIO_INTERRUPT,
                                       (control & INTERRUPT) && (control & INTERRUPT_MASK));
}

}  // namespace

void write(uint32_t address, uint32_t value) {
    switch (address) {
        case CONTROL: {
            const uint32_t before = register_image::read32(address);
            const uint32_t counter = sample_counter();
            uint32_t control = value & ~(INTERRUPT | RESET_COUNTER);
            if ((before & INTERRUPT) && !(value & INTERRUPT)) {
                control |= INTERRUPT;  // still pending: only writing 1 acknowledges it
            }
            register_image::write32(address, control);
            if (value & RESET_COUNTER) {
                restart_counter(0);
            } else if ((before ^ control) & (PLAYING | RATE_48_KHZ)) {
                restart_counter(counter);  // the count carries over a start, a stop or a new rate
            }
            update_interrupt();
            return;
        }
        case SAMPLE_COUNTER:
            restart_counter(value);
            return;
        default:
            register_image::write32(address, value);
            return;
    }
}

void refresh(uint32_t address) {
    if (address == SAMPLE_COUNTER) {
        register_image::write32(address, sample_counter());
    }
}

void advance() {
    if ((register_image::read32(CONTROL) & PLAYING) == 0) {
        return;
    }
    // Whether the target lies in (last_seen, counter], with the wrap-around of 32 bits.
    const uint32_t counter = sample_counter();
    const uint32_t target = register_image::read32(INTERRUPT_TIMING);
    if (target - g_counter.last_seen - 1 < counter - g_counter.last_seen) {
        register_image::write32(CONTROL, register_image::read32(CONTROL) | INTERRUPT);
        update_interrupt();
    }
    g_counter.last_seen = counter;
}

}  // namespace audio_interface
