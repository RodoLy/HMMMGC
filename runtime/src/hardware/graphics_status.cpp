// See graphics_status.h. Register layout from YAGCD 5.1-5.2 and Dolphin's
// CommandProcessor.cpp and PixelEngine.cpp.

#include "hardware/graphics_status.h"

#include "hardware/processor_interface.h"
#include "hardware/register_image.h"
#include "runtime_log.h"

namespace graphics_status {
namespace {

constexpr uint32_t CP_STATUS = COMMAND_PROCESSOR_BASE + 0x00;
constexpr uint16_t CP_READ_IDLE = 0x0004u;
constexpr uint16_t CP_COMMAND_IDLE = 0x0008u;

constexpr uint32_t PE_INTERRUPT_CONTROL = PIXEL_ENGINE_BASE + 0x0A;
constexpr uint32_t PE_TOKEN = PIXEL_ENGINE_BASE + 0x0E;
constexpr uint16_t PE_TOKEN_INTERRUPT_ENABLE = 0x1u;
constexpr uint16_t PE_FINISH_INTERRUPT_ENABLE = 0x2u;
constexpr uint16_t PE_TOKEN_INTERRUPT = 0x4u;
constexpr uint16_t PE_FINISH_INTERRUPT = 0x8u;

// The game writes a register of the graphics chip's "BP" block as the byte 0x61 followed
// by a 32-bit word whose top byte is the register: 0x45 is "draw done", 0x47 sets the
// token and 0x48 sets it and interrupts. Without a parser of the whole command stream,
// these are recognised as two consecutive stores to the pipe; vertex data that happened
// to look the same would be taken for them, which is acceptable while nothing draws.
constexpr uint8_t LOAD_BP_REGISTER_COMMAND = 0x61;
constexpr uint8_t BP_DRAW_DONE = 0x45;
constexpr uint8_t BP_SET_TOKEN = 0x47;
constexpr uint8_t BP_SET_TOKEN_AND_INTERRUPT = 0x48;

uint64_t g_bytes_sent = 0;
bool g_previous_store_was_load_bp = false;

void update_pixel_engine_interrupts() {
    const uint16_t control = register_image::read16(PE_INTERRUPT_CONTROL);
    processor_interface::set_interrupt(processor_interface::PIXEL_ENGINE_TOKEN_INTERRUPT,
                                       (control & PE_TOKEN_INTERRUPT) && (control & PE_TOKEN_INTERRUPT_ENABLE));
    processor_interface::set_interrupt(processor_interface::PIXEL_ENGINE_FINISH_INTERRUPT,
                                       (control & PE_FINISH_INTERRUPT) && (control & PE_FINISH_INTERRUPT_ENABLE));
}

void signal_draw_done() {
    register_image::write16(PE_INTERRUPT_CONTROL, register_image::read16(PE_INTERRUPT_CONTROL) | PE_FINISH_INTERRUPT);
    update_pixel_engine_interrupts();
}

void signal_token(uint16_t token, bool interrupt) {
    register_image::write16(PE_TOKEN, token);
    if (interrupt) {
        register_image::write16(PE_INTERRUPT_CONTROL,
                                register_image::read16(PE_INTERRUPT_CONTROL) | PE_TOKEN_INTERRUPT);
        update_pixel_engine_interrupts();
    }
}

void watch_for_pixel_engine_commands(const uint8_t* bytes, size_t size) {
    if (size == 1) {
        g_previous_store_was_load_bp = bytes[0] == LOAD_BP_REGISTER_COMMAND;
        return;
    }
    if (g_previous_store_was_load_bp && size == 4) {
        const auto value = static_cast<uint16_t>((bytes[2] << 8) | bytes[3]);
        switch (bytes[0]) {
            case BP_DRAW_DONE:
                signal_draw_done();
                break;
            case BP_SET_TOKEN:
                signal_token(value, false);
                break;
            case BP_SET_TOKEN_AND_INTERRUPT:
                signal_token(value, true);
                break;
            default:
                break;
        }
    }
    g_previous_store_was_load_bp = false;
}

}  // namespace

void refresh_command_processor(uint32_t address) {
    if (address == CP_STATUS) {
        // Nothing executes the FIFO, so the graphics chip always looks idle.
        register_image::write16(address, CP_READ_IDLE | CP_COMMAND_IDLE);
    }
}

void write_pixel_engine(uint32_t address, uint16_t value) {
    if (address == PE_INTERRUPT_CONTROL) {
        constexpr uint16_t STATUS_BITS = PE_TOKEN_INTERRUPT | PE_FINISH_INTERRUPT;
        const uint16_t still_pending =
            register_image::read16(address) & STATUS_BITS & ~value;  // writing 1 acknowledges
        register_image::write16(address, static_cast<uint16_t>((value & ~STATUS_BITS) | still_pending));
        update_pixel_engine_interrupts();
        return;
    }
    register_image::write16(address, value);
}

void write_to_gather_pipe(const uint8_t* bytes, size_t size) {
    if (g_bytes_sent == 0) {
        runtime_log::info("graphics", "first graphics command sent through the gather pipe");
    }
    g_bytes_sent += size;
    processor_interface::append_to_fifo(bytes, size);
    watch_for_pixel_engine_commands(bytes, size);
}

uint64_t bytes_sent() { return g_bytes_sent; }

}  // namespace graphics_status
