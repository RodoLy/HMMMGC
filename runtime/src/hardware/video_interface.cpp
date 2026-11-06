// See video_interface.h. Register layout from YAGCD 5.3 and Dolphin's VideoInterface.cpp.

#include "hardware/video_interface.h"

#include "cpu_registers.h"
#include "hardware/processor_interface.h"
#include "hardware/register_image.h"

namespace video_interface {
namespace {

constexpr uint32_t VERTICAL_POSITION = BASE + 0x2C;
constexpr uint32_t HORIZONTAL_POSITION = BASE + 0x2E;
// The four display interrupts: each fires when the beam reaches the line it holds.
constexpr uint32_t DISPLAY_INTERRUPTS[] = {BASE + 0x30, BASE + 0x34, BASE + 0x38, BASE + 0x3C};
constexpr uint16_t INTERRUPT_STATUS = 0x8000u;  // bits of the high half of each
constexpr uint16_t INTERRUPT_ENABLE = 0x1000u;

// NTSC: 59.94 fields per second (two per interlaced frame), 263 lines each.
constexpr uint64_t TICKS_PER_FIELD = cpu_registers::TIME_BASE_TICKS_PER_SECOND * 1001 / 60000;
constexpr uint32_t LINES_PER_FIELD = 263;
constexpr uint64_t TICKS_PER_LINE = TICKS_PER_FIELD / LINES_PER_FIELD;

uint64_t g_fields_shown = 0;

void update_interrupt() {
    bool asserted = false;
    for (const uint32_t reg : DISPLAY_INTERRUPTS) {
        const uint16_t high = register_image::read16(reg);
        asserted = asserted || ((high & INTERRUPT_STATUS) && (high & INTERRUPT_ENABLE));
    }
    processor_interface::set_interrupt(processor_interface::VIDEO_INTERRUPT, asserted);
}

}  // namespace

void write(uint32_t address, uint16_t value) {
    register_image::write16(address, value);
    for (const uint32_t reg : DISPLAY_INTERRUPTS) {
        if (address == reg) {
            update_interrupt();  // the game acknowledges by writing the status bit as 0
        }
    }
}

void refresh(uint32_t address) {
    if (address == VERTICAL_POSITION || address == HORIZONTAL_POSITION) {
        const uint64_t line = (cpu_registers::time_base() % TICKS_PER_FIELD) / TICKS_PER_LINE;
        register_image::write16(VERTICAL_POSITION, static_cast<uint16_t>(1 + line % LINES_PER_FIELD));
        register_image::write16(HORIZONTAL_POSITION, 1);
    }
}

// Once per field the beam passes every line, so every enabled display interrupt fires.
// Which line each one waits for does not matter while nothing is drawn.
void advance() {
    const uint64_t field = cpu_registers::time_base() / TICKS_PER_FIELD;
    if (field <= g_fields_shown) {
        return;
    }
    g_fields_shown = field;
    for (const uint32_t reg : DISPLAY_INTERRUPTS) {
        const uint16_t high = register_image::read16(reg);
        if (high & INTERRUPT_ENABLE) {
            register_image::write16(reg, high | INTERRUPT_STATUS);
        }
    }
    update_interrupt();
}

uint64_t next_field_time() { return (g_fields_shown + 1) * TICKS_PER_FIELD; }

uint64_t fields_shown() { return g_fields_shown; }

}  // namespace video_interface
