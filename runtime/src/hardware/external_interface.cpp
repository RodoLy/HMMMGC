// See external_interface.h. Register layout from YAGCD 5.9; the boot ROM chip's protocol
// and the SRAM layout from Dolphin's EXI_DeviceIPL.cpp and Sram.cpp/h.

#include "hardware/external_interface.h"

#include "guest_memory.h"
#include "hardware/processor_interface.h"
#include "hardware/register_image.h"

#include <array>
#include <ctime>

namespace external_interface {
namespace {

// Each channel has five registers.
constexpr uint32_t CHANNEL_COUNT = 3;
constexpr uint32_t CHANNEL_STRIDE = 0x14;
constexpr uint32_t STATUS_OFFSET = 0x00;
constexpr uint32_t DMA_ADDRESS_OFFSET = 0x04;
constexpr uint32_t DMA_LENGTH_OFFSET = 0x08;
constexpr uint32_t CONTROL_OFFSET = 0x0C;
constexpr uint32_t DATA_OFFSET = 0x10;

// Status bits: the interrupts are acknowledged by writing 1; bits 7-9 select device 0,
// 1 or 2 of the channel.
constexpr uint32_t TRANSFER_INTERRUPT_MASK = 0x004u;
constexpr uint32_t TRANSFER_INTERRUPT = 0x008u;
constexpr uint32_t INTERRUPT_STATUS_BITS = 0x002u | TRANSFER_INTERRUPT | 0x800u;
constexpr uint32_t DEVICE_SELECT_SHIFT = 7;
// Control bits.
constexpr uint32_t TRANSFER_START = 0x1u;
constexpr uint32_t DMA_TRANSFER = 0x2u;
constexpr uint32_t DIRECTION_WRITE = 1;  // bits 2-3: 0 read, 1 write, 2 read and write

// The boot ROM's chip (channel 0, device 1) takes a 4-byte command (bit 31: write;
// bits 6-30: address) and then moves data. Address 0x800000 is the real-time clock
// (4 bytes) followed by the SRAM (64 bytes of console settings).
constexpr unsigned BOOT_ROM_CHANNEL = 0;
constexpr int BOOT_ROM_DEVICE = 1;
constexpr uint32_t SRAM_ADDRESS = 0x800000u;
constexpr uint32_t CLOCK_SIZE = 4;
constexpr uint32_t SRAM_SIZE = 64;

struct BootRomChip {
    uint32_t command = 0;
    unsigned command_bytes = 0;
    uint32_t cursor = 0;
    std::array<uint8_t, CLOCK_SIZE + SRAM_SIZE> clock_and_sram{};
};
BootRomChip g_boot_rom;

// Seconds since 2000-01-01, the GameCube's epoch.
uint32_t clock_seconds() {
    constexpr int64_t UNIX_TIME_OF_2000 = 946684800;
    return static_cast<uint32_t>(std::time(nullptr) - UNIX_TIME_OF_2000);
}

// The settings Dolphin starts with: stereo sound, first-time setup done, English, no
// clock bias. Bytes 0-3 of the settings are a checksum and its complement, sums of the
// 16-bit words from the clock bias (byte 12) to the end of the settings block (byte 20).
void initialize_sram() {
    auto& bytes = g_boot_rom.clock_and_sram;
    bytes.fill(0);
    constexpr size_t SETTINGS = CLOCK_SIZE;
    constexpr size_t FLAGS = SETTINGS + 0x13;
    constexpr uint8_t STEREO_AND_SETUP_DONE = 0x2C;
    bytes[FLAGS] = STEREO_AND_SETUP_DONE;
    uint16_t checksum = 0;
    uint16_t inverse = 0;
    for (size_t offset = SETTINGS + 0x0C; offset < SETTINGS + 0x14; offset += 2) {
        const auto word = static_cast<uint16_t>((bytes[offset] << 8) | bytes[offset + 1]);
        checksum = static_cast<uint16_t>(checksum + word);
        inverse = static_cast<uint16_t>(inverse + static_cast<uint16_t>(~word));
    }
    bytes[SETTINGS + 0] = static_cast<uint8_t>(checksum >> 8);
    bytes[SETTINGS + 1] = static_cast<uint8_t>(checksum);
    bytes[SETTINGS + 2] = static_cast<uint8_t>(inverse >> 8);
    bytes[SETTINGS + 3] = static_cast<uint8_t>(inverse);
}

uint8_t transfer_with_boot_rom(uint8_t sent) {
    if (g_boot_rom.command_bytes < 4) {
        g_boot_rom.command = (g_boot_rom.command << 8) | sent;
        if (++g_boot_rom.command_bytes == 4) {
            const uint32_t seconds = clock_seconds();
            for (uint32_t i = 0; i < CLOCK_SIZE; ++i) {
                g_boot_rom.clock_and_sram[i] = static_cast<uint8_t>(seconds >> (24 - 8 * i));
            }
        }
        return 0xFF;
    }
    const bool is_write = (g_boot_rom.command & 0x80000000u) != 0;
    const uint32_t address = (g_boot_rom.command >> 6) & 0x01FFFFFFu;
    if (address >= SRAM_ADDRESS && address < SRAM_ADDRESS + CLOCK_SIZE + SRAM_SIZE) {
        const uint32_t index = (address - SRAM_ADDRESS + g_boot_rom.cursor++) % (CLOCK_SIZE + SRAM_SIZE);
        if (is_write) {
            g_boot_rom.clock_and_sram[index] = sent;
            return 0;
        }
        return g_boot_rom.clock_and_sram[index];
    }
    return 0;  // the boot ROM itself, its fonts and the debug UART are not modelled
}

int selected_device(uint32_t status) {
    switch ((status >> DEVICE_SELECT_SHIFT) & 7u) {
        case 1:
            return 0;
        case 2:
            return 1;
        case 4:
            return 2;
        default:
            return -1;
    }
}

uint8_t transfer_byte(unsigned channel, int device, uint8_t sent) {
    if (channel == BOOT_ROM_CHANNEL && device == BOOT_ROM_DEVICE) {
        return transfer_with_boot_rom(sent);
    }
    return 0xFF;  // nothing is connected: memory card slots read as empty
}

void run_transfer(unsigned channel) {
    const uint32_t registers = BASE + channel * CHANNEL_STRIDE;
    const uint32_t control = register_image::read32(registers + CONTROL_OFFSET);
    const int device = selected_device(register_image::read32(registers + STATUS_OFFSET));
    const uint32_t direction = (control >> 2) & 3u;
    if (control & DMA_TRANSFER) {
        const uint32_t address =
            guest_memory::physical_to_cached(register_image::read32(registers + DMA_ADDRESS_OFFSET) & 0x03FFFFE0u);
        const uint32_t length = register_image::read32(registers + DMA_LENGTH_OFFSET) & 0x03FFFFE0u;
        uint8_t* host = guest_memory::host_pointer(address, length);
        for (uint32_t i = 0; host != nullptr && i < length; ++i) {
            const uint8_t received = transfer_byte(channel, device, direction == DIRECTION_WRITE ? host[i] : 0);
            if (direction != DIRECTION_WRITE) {
                host[i] = received;
            }
        }
    } else {
        // An immediate transfer moves 1 to 4 bytes through the data register, first byte
        // in its top bits.
        const unsigned length = ((control >> 4) & 3u) + 1;
        const uint32_t sent = register_image::read32(registers + DATA_OFFSET);
        uint32_t received = 0;
        for (unsigned i = 0; i < length; ++i) {
            const auto byte = static_cast<uint8_t>(sent >> (24 - 8 * i));
            received |= uint32_t{transfer_byte(channel, device, byte)} << (24 - 8 * i);
        }
        if (direction != DIRECTION_WRITE) {
            register_image::write32(registers + DATA_OFFSET, received);
        }
    }
    register_image::write32(registers + CONTROL_OFFSET, control & ~TRANSFER_START);
    const uint32_t status = register_image::read32(registers + STATUS_OFFSET) | TRANSFER_INTERRUPT;
    register_image::write32(registers + STATUS_OFFSET, status);
    processor_interface::set_interrupt(processor_interface::EXTERNAL_INTERRUPT,
                                       (status & TRANSFER_INTERRUPT_MASK) != 0);
}

}  // namespace

void initialize() { initialize_sram(); }

void write(uint32_t address, uint32_t value) {
    const unsigned channel = (address - BASE) / CHANNEL_STRIDE;
    const uint32_t offset = (address - BASE) % CHANNEL_STRIDE;
    if (channel >= CHANNEL_COUNT) {
        register_image::write32(address, value);
        return;
    }
    if (offset == STATUS_OFFSET) {
        const uint32_t before = register_image::read32(address);
        const uint32_t still_pending = before & INTERRUPT_STATUS_BITS & ~value;
        register_image::write32(address, (value & ~INTERRUPT_STATUS_BITS) | still_pending);
        if (channel == BOOT_ROM_CHANNEL && selected_device(value) != selected_device(before)) {
            g_boot_rom.command = 0;  // a new selection starts a new command
            g_boot_rom.command_bytes = 0;
            g_boot_rom.cursor = 0;
        }
        return;
    }
    register_image::write32(address, value);
    if (offset == CONTROL_OFFSET && (value & TRANSFER_START)) {
        run_transfer(channel);
    }
}

}  // namespace external_interface
