// See hardware_registers.h. The blocks and their register widths are those of YAGCD
// chapter 5 and Dolphin's HW/ sources.

#include "hardware/hardware_registers.h"

#include "graphics/aurora_host.h"
#include "graphics/gx_draw.h"
#include "guest_symbols.h"
#include "hardware/audio_interface.h"
#include "hardware/dsp_interface.h"
#include "hardware/dvd_interface.h"
#include "hardware/external_interface.h"
#include "hardware/graphics_status.h"
#include "hardware/processor_interface.h"
#include "hardware/register_image.h"
#include "hardware/serial_interface.h"
#include "hardware/video_interface.h"
#include "runtime_log.h"

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <set>
#include <utility>

namespace hardware_registers {
namespace {

constexpr uint32_t MEMORY_INTERFACE_BASE = 0xCC004000u;  // memory protection: no model needed

enum class Device {
    COMMAND_PROCESSOR,
    PIXEL_ENGINE,
    VIDEO,
    PROCESSOR,
    MEMORY,
    DSP,
    DVD,
    SERIAL,
    EXTERNAL,
    AUDIO,
    GATHER_PIPE,
};

struct Block {
    Device device;
    uint32_t base;
    uint32_t size;
    const char* name;
    unsigned register_width;  // the devices have 16-bit or 32-bit registers
};

constexpr Block BLOCKS[] = {
    {Device::COMMAND_PROCESSOR, graphics_status::COMMAND_PROCESSOR_BASE, 0x80, "CP", 2},
    {Device::PIXEL_ENGINE, graphics_status::PIXEL_ENGINE_BASE, 0x100, "PE", 2},
    {Device::VIDEO, video_interface::BASE, 0x100, "VI", 2},
    {Device::PROCESSOR, processor_interface::BASE, 0x100, "PI", 4},
    {Device::MEMORY, MEMORY_INTERFACE_BASE, 0x80, "MI", 2},
    {Device::DSP, dsp_interface::BASE, 0x200, "DSP", 2},
    {Device::DVD, dvd_interface::BASE, 0x40, "DI", 4},
    {Device::SERIAL, serial_interface::BASE, 0x100, "SI", 4},
    {Device::EXTERNAL, external_interface::BASE, external_interface::SIZE, "EXI", 4},
    {Device::AUDIO, audio_interface::BASE, 0x20, "AI", 4},
    {Device::GATHER_PIPE, graphics_status::GATHER_PIPE_BASE, 0x100, "GX FIFO", 4},
};

const Block* find_block(uint32_t address) {
    for (const Block& block : BLOCKS) {
        if (address - block.base < block.size) {
            return &block;
        }
    }
    return nullptr;
}

std::set<std::pair<uint32_t, bool>> g_logged_accesses;

void log_first_access(uint32_t address, bool is_write, uint32_t value, unsigned width, const void* host_caller) {
    if (!g_logged_accesses.insert({address, is_write}).second) {
        return;
    }
    const Block* block = find_block(address);
    runtime_log::info("hardware", "first %s of %s+0x%03X (%u-bit) %s 0x%0*X in %s", is_write ? "write" : "read",
                      block != nullptr ? block->name : "?", block != nullptr ? address - block->base : address,
                      width * 8, is_write ? "<-" : "=", static_cast<int>(width * 2), value,
                      guest_symbols::describe_host_code(host_caller).c_str());
}

// Writes `value` to the register of the device's width at `address`.
void write_register(const Block& block, uint32_t address, uint32_t value) {
    switch (block.device) {
        case Device::PIXEL_ENGINE:
            graphics_status::write_pixel_engine(address, static_cast<uint16_t>(value));
            return;
        case Device::VIDEO:
            video_interface::write(address, static_cast<uint16_t>(value));
            return;
        case Device::PROCESSOR:
            processor_interface::write(address, value);
            return;
        case Device::DSP:
            dsp_interface::write(address, static_cast<uint16_t>(value));
            return;
        case Device::DVD:
            dvd_interface::write(address, value);
            return;
        case Device::SERIAL:
            serial_interface::write(address, value);
            return;
        case Device::EXTERNAL:
            external_interface::write(address, value);
            return;
        case Device::AUDIO:
            audio_interface::write(address, value);
            return;
        default:
            register_image::store(address, value, block.register_width);
            return;
    }
}

// Brings the image of the register of the device's width at `address` up to date.
void refresh_register(const Block& block, uint32_t address) {
    switch (block.device) {
        case Device::COMMAND_PROCESSOR:
            graphics_status::refresh_command_processor(address);
            return;
        case Device::VIDEO:
            video_interface::refresh(address);
            return;
        case Device::PROCESSOR:
            processor_interface::refresh(address);
            return;
        case Device::DSP:
            dsp_interface::refresh(address);
            return;
        case Device::DVD:
            dvd_interface::refresh(address);
            return;
        case Device::AUDIO:
            audio_interface::refresh(address);
            return;
        default:
            return;
    }
}

// The registers of the device's width that an access of `width` bytes at `address` touches.
template <typename Visit>
void for_each_register(const Block& block, uint32_t address, unsigned width, Visit visit) {
    for (uint32_t reg = address & ~(block.register_width - 1); reg < address + width; reg += block.register_width) {
        visit(reg);
    }
}

}  // namespace

void initialize(const ExtractedDisc& disc) {
    processor_interface::initialize();
    dvd_interface::initialize(disc);
    external_interface::initialize();
}

uint32_t read(uint32_t address, unsigned width, const void* host_caller) {
    const Block* block = find_block(address);
    if (block == nullptr) {
        runtime_log::warning("hardware", "read of unknown register 0x%08X in %s", address,
                             guest_symbols::describe_host_code(host_caller).c_str());
        return 0;
    }
    for_each_register(*block, address, width, [&](uint32_t reg) { refresh_register(*block, reg); });
    const uint32_t value = register_image::load(address, width);
    log_first_access(address, false, value, width, host_caller);
    return value;
}

void write(uint32_t address, uint32_t value, unsigned width, const void* host_caller) {
    log_first_access(address, true, value, width, host_caller);
    const Block* block = find_block(address);
    if (block == nullptr) {
        runtime_log::warning("hardware", "write of unknown register 0x%08X in %s", address,
                             guest_symbols::describe_host_code(host_caller).c_str());
        return;
    }
    uint8_t bytes[4];
    for (unsigned i = 0; i < width; ++i) {
        bytes[i] = static_cast<uint8_t>(value >> (8 * (width - 1 - i)));
    }
    if (block->device == Device::GATHER_PIPE) {
        write_to_gather_pipe(bytes, width);
        return;
    }
    // An access narrower or wider than the device's registers is merged into, or split
    // across, whole registers, which are then written one by one.
    const uint32_t first = address & ~(block->register_width - 1);
    const unsigned span =
        ((address + width - first + block->register_width - 1) / block->register_width) * block->register_width;
    std::array<uint8_t, 8> merged{};
    for (unsigned i = 0; i < span; ++i) {
        merged[i] = static_cast<uint8_t>(register_image::load(first + i, 1));
    }
    std::memcpy(merged.data() + (address - first), bytes, width);
    for (unsigned offset = 0; offset < span; offset += block->register_width) {
        uint32_t register_value = 0;
        for (unsigned i = 0; i < block->register_width; ++i) {
            register_value = (register_value << 8) | merged[offset + i];
        }
        write_register(*block, first + offset, register_value);
    }
}

void prepare_trapped_read(uint32_t address, const void* host_caller) {
    const Block* block = find_block(address);
    if (block != nullptr) {
        // The width of the load is not known; refresh the 4 bytes around it.
        for_each_register(*block, address & ~3u, 4, [&](uint32_t reg) { refresh_register(*block, reg); });
    }
    log_first_access(address, false, register_image::load(address & ~1u, 2), 2, host_caller);
}

void write_to_gather_pipe(const uint8_t* bytes, size_t size) {
    // From GXInit on, aurora is the GPU: what the game stores in the pipe is the vertices of
    // its primitives and the commands it writes by itself, and gx_draw.h hands them to
    // aurora. Before that, the modelled command processor takes it.
    if (aurora_host::is_initialized()) {
        gx_draw::write_to_pipe(bytes, static_cast<uint32_t>(size));
        return;
    }
    graphics_status::write_to_gather_pipe(bytes, size);
}

uint32_t pending_interrupts() { return processor_interface::pending_interrupts(); }

uint32_t enabled_interrupts() { return processor_interface::enabled_interrupts(); }

void advance_time() {
    video_interface::advance();
    audio_interface::advance();
    dsp_interface::advance();
}

uint64_t next_event_time() {
    return std::min(video_interface::next_field_time(), dsp_interface::next_audio_buffer_time());
}

std::string describe_activity() {
    char text[160];
    std::snprintf(text, sizeof(text), "%llu video fields, %llu bytes of graphics commands, %llu disc commands",
                  static_cast<unsigned long long>(video_interface::fields_shown()),
                  static_cast<unsigned long long>(graphics_status::bytes_sent()),
                  static_cast<unsigned long long>(dvd_interface::commands_run()));
    return text;
}

}  // namespace hardware_registers
