// See dsp_programs.h. The boot ROM's mails follow Dolphin's DSP-HLE (UCodes/ROM.cpp) and
// the hash is Dolphin's (Common::HashEctor), so that a program has the same number here
// as in Dolphin's tables.

#include "audio/dsp_programs.h"

#include "audio/ax_microcode.h"
#include "guest_memory.h"
#include "hardware/dsp_interface.h"
#include "runtime_log.h"

namespace dsp_programs {
namespace {

constexpr uint32_t BOOT_ROM_GREETING = 0x8071FEEDu;
constexpr uint32_t COMMAND_MASK = 0xFFFF0000u;
constexpr uint32_t BOOT_COMMAND = 0x80F30000u;
// The boot ROM's commands: each one is followed by its value in the next mail.
constexpr uint32_t CODE_ADDRESS = 0x80F3A001u;      // the program's code in main memory
constexpr uint32_t CODE_LENGTH = 0x80F3A002u;       // in bytes
constexpr uint32_t DATA_LENGTH = 0x80F3B002u;       // data to copy too (none here)
constexpr uint32_t CODE_DESTINATION = 0x80F3C002u;  // where it goes in the DSP's memory
constexpr uint32_t START_ADDRESS = 0x80F3D001u;     // where it starts: this one boots it
// The boot ROM answers any other mail with this prefix and the mail's low half.
constexpr uint32_t UNKNOWN_MAIL_REPLY = 0xFEEE0000u;

enum class Program { BOOT_ROM, AX, UNKNOWN };

struct BootRequest {
    uint32_t pending_command = 0;  // the command whose value comes next, or 0
    uint32_t code_address = 0;
    uint32_t code_length = 0;
};

Program g_program = Program::BOOT_ROM;
BootRequest g_boot;

// Dolphin's hash of a DSP program (Common::HashEctor).
uint32_t hash_code(const uint8_t* code, uint32_t length) {
    uint32_t hash = 0;
    for (uint32_t i = 0; i < length; ++i) {
        hash ^= code[i];
        hash = (hash << 3) | (hash >> 29);
    }
    return hash;
}

void boot_program() {
    const uint8_t* code = guest_memory::host_pointer(g_boot.code_address, g_boot.code_length);
    if (code == nullptr) {
        runtime_log::warning("dsp", "the DSP program at 0x%08X (%u bytes) is not in memory", g_boot.code_address,
                             g_boot.code_length);
        g_program = Program::UNKNOWN;
        return;
    }
    const uint32_t hash = hash_code(code, g_boot.code_length);
    if (ax_microcode::is_known_version(hash)) {
        g_program = Program::AX;
        ax_microcode::start(hash);
        return;
    }
    runtime_log::warning("dsp", "the DSP was given an unknown program (hash 0x%08X): there will be no sound", hash);
    g_program = Program::UNKNOWN;
}

void receive_mail_in_boot_rom(uint32_t mail) {
    if (g_boot.pending_command == 0) {
        if ((mail & COMMAND_MASK) == BOOT_COMMAND) {
            g_boot.pending_command = mail;
        } else {
            dsp_interface::send_mail_to_cpu(UNKNOWN_MAIL_REPLY | (mail & 0xFFFFu), false);
        }
        return;
    }
    const uint32_t command = g_boot.pending_command;
    g_boot.pending_command = 0;
    switch (command) {
        case CODE_ADDRESS:
            g_boot.code_address = mail;
            return;
        case CODE_LENGTH:
            g_boot.code_length = mail & 0xFFFFu;
            return;
        case START_ADDRESS:
            boot_program();
            return;
        case DATA_LENGTH:
        case CODE_DESTINATION:
        default:
            return;  // nothing for a program written in C++ to do with these
    }
}

}  // namespace

void reset() {
    g_program = Program::BOOT_ROM;
    g_boot = BootRequest{};
    dsp_interface::send_mail_to_cpu(BOOT_ROM_GREETING, false);
}

void receive_mail(uint32_t mail) {
    switch (g_program) {
        case Program::BOOT_ROM:
            receive_mail_in_boot_rom(mail);
            return;
        case Program::AX:
            ax_microcode::receive_mail(mail);
            return;
        case Program::UNKNOWN:
            return;
    }
}

void return_to_boot_rom() { reset(); }

}  // namespace dsp_programs
