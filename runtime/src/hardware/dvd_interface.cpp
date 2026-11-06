// See dvd_interface.h. Register layout from YAGCD 5.7 and the drive's commands from
// Dolphin's DVDInterface.cpp.

#include "hardware/dvd_interface.h"

#include "extracted_disc.h"
#include "guest_memory.h"
#include "hardware/processor_interface.h"
#include "hardware/register_image.h"
#include "runtime_log.h"

#include <set>
#include <string>

namespace dvd_interface {
namespace {

constexpr uint32_t STATUS = BASE + 0x00;
constexpr uint32_t COVER = BASE + 0x04;
constexpr uint32_t COMMAND0 = BASE + 0x08;
constexpr uint32_t COMMAND1 = BASE + 0x0C;
constexpr uint32_t DMA_ADDRESS = BASE + 0x14;
constexpr uint32_t DMA_LENGTH = BASE + 0x18;
constexpr uint32_t CONTROL = BASE + 0x1C;
constexpr uint32_t IMMEDIATE_BUFFER = BASE + 0x20;
constexpr uint32_t CONFIGURATION = BASE + 0x24;

// Status bits: each interrupt (error, transfer complete, break) has its mask bit right
// below it; the interrupt bits are acknowledged by writing 1.
constexpr uint32_t TRANSFER_INTERRUPT_MASK = 0x08u;
constexpr uint32_t TRANSFER_INTERRUPT = 0x10u;
constexpr uint32_t INTERRUPT_STATUS_BITS = 0x04u | TRANSFER_INTERRUPT | 0x40u;
// The cover register keeps only its interrupt mask: the cover is always closed.
constexpr uint32_t COVER_INTERRUPT_MASK = 0x02u;
// Control bits.
constexpr uint32_t START = 0x01u;
constexpr uint32_t DMA_TRANSFER = 0x02u;

// Drive commands (the top byte of command register 0).
constexpr uint32_t READ_COMMAND = 0xA8;
constexpr uint32_t INQUIRY_COMMAND = 0x12;
constexpr uint32_t REQUEST_ERROR_COMMAND = 0xE0;

const ExtractedDisc* g_disc = nullptr;
std::set<std::string> g_files_read;
uint64_t g_commands_run = 0;

void log_first_read_of_file(uint64_t offset, uint32_t length) {
    const std::string file = g_disc->file_at(offset);
    if (g_files_read.insert(file).second) {
        runtime_log::info("dvd", "reads %s (0x%X bytes at disc offset 0x%llX)", file.empty() ? "?" : file.c_str(),
                          length, static_cast<unsigned long long>(offset));
    }
}

void run_command() {
    const uint32_t command = register_image::read32(COMMAND0);
    const uint32_t destination = guest_memory::physical_to_cached(register_image::read32(DMA_ADDRESS) & 0x03FFFFE0u);
    const uint32_t length = register_image::read32(DMA_LENGTH);
    switch (command >> 24) {
        case READ_COMMAND: {  // offset in 32-bit words, length in bytes, into the DMA address
            const uint64_t offset = uint64_t{register_image::read32(COMMAND1)} << 2;
            uint8_t* host = guest_memory::host_pointer(destination, length);
            if (host == nullptr || !g_disc->read(offset, length, host)) {
                runtime_log::warning("dvd", "read of 0x%X bytes at 0x%llX failed", length,
                                     static_cast<unsigned long long>(offset));
            }
            log_first_read_of_file(offset, length);
            break;
        }
        case INQUIRY_COMMAND: {  // the drive's revision, release date and version (Dolphin's values)
            const uint32_t answer[] = {0x00000002u, 0x20060526u, 0x41000000u};
            for (uint32_t i = 0; i < 3 && (i + 1) * 4 <= length; ++i) {
                guest_memory::write32(destination + i * 4, answer[i]);
            }
            break;
        }
        case REQUEST_ERROR_COMMAND:  // no error
            register_image::write32(IMMEDIATE_BUFFER, 0);
            break;
        default:
            runtime_log::info("dvd", "drive command 0x%08X is answered without doing anything", command);
            break;
    }
    ++g_commands_run;
    // A DMA command leaves the address past the data and the length at zero; the DVD
    // library works out from them how much was transferred.
    const uint32_t control = register_image::read32(CONTROL);
    if (control & DMA_TRANSFER) {
        register_image::write32(DMA_ADDRESS, register_image::read32(DMA_ADDRESS) + length);
        register_image::write32(DMA_LENGTH, 0);
    }
    register_image::write32(CONTROL, control & ~START);
    const uint32_t status = register_image::read32(STATUS) | TRANSFER_INTERRUPT;
    register_image::write32(STATUS, status);
    processor_interface::set_interrupt(processor_interface::DVD_INTERRUPT, (status & TRANSFER_INTERRUPT_MASK) != 0);
}

}  // namespace

void initialize(const ExtractedDisc& disc) { g_disc = &disc; }

void write(uint32_t address, uint32_t value) {
    switch (address) {
        case STATUS: {
            const uint32_t still_pending = register_image::read32(STATUS) & INTERRUPT_STATUS_BITS & ~value;
            register_image::write32(address, (value & ~INTERRUPT_STATUS_BITS) | still_pending);
            processor_interface::set_interrupt(
                processor_interface::DVD_INTERRUPT,
                (still_pending & TRANSFER_INTERRUPT) && (value & TRANSFER_INTERRUPT_MASK));
            return;
        }
        case COVER:
            register_image::write32(address, value & COVER_INTERRUPT_MASK);
            return;
        case CONTROL:
            register_image::write32(address, value);
            if (value & START) {
                run_command();
            }
            return;
        case CONFIGURATION:
            return;  // read-only
        default:
            register_image::write32(address, value);
            return;
    }
}

void refresh(uint32_t address) {
    if (address == CONFIGURATION) {
        register_image::write32(address, 1);  // what Dolphin reports
    }
}

uint64_t commands_run() { return g_commands_run; }

}  // namespace dvd_interface
