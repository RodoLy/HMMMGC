// See ax_microcode.h. Ported from Dolphin's AX.cpp (GameCube part), with the same
// commands and arithmetic.

#include "audio/ax_microcode.h"

#include "audio/ax_voice.h"
#include "audio/dsp_programs.h"
#include "guest_memory.h"
#include "hardware/dsp_interface.h"
#include "runtime_log.h"

#include <algorithm>
#include <array>
#include <cstdlib>

namespace ax_microcode {
namespace {

// Mails from the DSP to the CPU, and from the CPU to the DSP about the task (Dolphin's
// UCodes.h names them).
constexpr uint32_t DSP_INIT = 0xDCD10000u;    // the program started
constexpr uint32_t DSP_RESUME = 0xDCD10001u;  // it resumed after the CPU asked
constexpr uint32_t DSP_YIELD = 0xDCD10002u;   // it finished a command list
constexpr uint32_t TASK_MAIL_MASK = 0xFFFF0000u;
constexpr uint32_t TASK_MAIL_TO_DSP = 0xCDD10000u;
constexpr uint32_t MAIL_RESUME = 0xCDD10000u;        // go on, and acknowledge it
constexpr uint32_t MAIL_NEW_UCODE = 0xCDD10001u;     // another program comes
constexpr uint32_t MAIL_RESET = 0xCDD10002u;         // back to the boot ROM
constexpr uint32_t MAIL_CONTINUE = 0xCDD10003u;      // go on; a command list follows
constexpr uint32_t MAIL_COMMAND_LIST = 0xBABE0000u;  // | the list's length in words

// The commands of a list, each followed by its arguments (16-bit words).
enum Command : uint16_t {
    SETUP = 0x00,
    DOWNLOAD_AND_MIX_WITH_VOLUME = 0x01,
    VOICE_LIST_ADDRESS = 0x02,
    PLAY_VOICES = 0x03,
    MIX_AUX_A = 0x04,
    MIX_AUX_B = 0x05,
    UPLOAD_MAIN = 0x06,
    SET_MAIN = 0x07,
    UNKNOWN_08 = 0x08,  // never seen; ten arguments
    MIX_AUX_B_WITHOUT_UPLOAD = 0x09,
    NOTHING_0A = 0x0A,
    NOTHING_0B = 0x0B,
    NOTHING_0C = 0x0C,
    MORE = 0x0D,  // the list continues at another address
    OUTPUT = 0x0E,
    END = 0x0F,
    MIX_AUX_B_LEFT_RIGHT = 0x10,
    SET_OPPOSITE_LEFT_RIGHT = 0x11,
    COMPRESSOR = 0x12,
    SEND_AUX_AND_MIX = 0x13,
};

constexpr uint32_t FRAME = ax_voice::SAMPLES_PER_FRAME;
constexpr uint32_t BUFFER_BYTES = FRAME * sizeof(int32_t);

enum class MailState { WAITING_FOR_LIST_LENGTH, WAITING_FOR_LIST_ADDRESS, WAITING_FOR_NEXT_TASK };

struct State {
    ax_voice::ProgramVersion version;
    uint32_t code_hash = 0;
    MailState mail_state = MailState::WAITING_FOR_LIST_LENGTH;
    std::array<uint16_t, 512> command_list{};
    uint16_t command_list_length = 0;

    // The frame's mixing buffers, 32-bit so that voices can add up past 16 bits.
    std::array<int32_t, FRAME> main_left{};
    std::array<int32_t, FRAME> main_right{};
    std::array<int32_t, FRAME> main_surround{};
    std::array<int32_t, FRAME> aux_a_left{};
    std::array<int32_t, FRAME> aux_a_right{};
    std::array<int32_t, FRAME> aux_a_surround{};
    std::array<int32_t, FRAME> aux_b_left{};
    std::array<int32_t, FRAME> aux_b_right{};
    std::array<int32_t, FRAME> aux_b_surround{};

    uint16_t compressor_position = 0;
};
State g_state;

uint32_t join(uint16_t high, uint16_t low) { return (uint32_t{high} << 16) | low; }

int16_t clamp_to_16_bits(int64_t sample) { return static_cast<int16_t>(std::clamp<int64_t>(sample, -0x8000, 0x7FFF)); }

// ---- Main memory, in the guest's byte order ----

uint32_t read_be32(const uint8_t* bytes) {
    return (uint32_t{bytes[0]} << 24) | (uint32_t{bytes[1]} << 16) | (uint32_t{bytes[2]} << 8) | bytes[3];
}

// Adds (or copies) `count` big-endian 32-bit samples from guest memory into `buffer`.
void download(uint32_t guest_address, int32_t* buffer, uint32_t count, bool add) {
    const uint8_t* bytes = guest_memory::host_pointer(guest_address, count * 4);
    if (bytes == nullptr) {
        runtime_log::warning("ax", "a mixing buffer at 0x%08X is not in memory", guest_address);
        return;
    }
    for (uint32_t i = 0; i < count; ++i) {
        const auto sample = static_cast<int32_t>(read_be32(bytes + 4 * i));
        buffer[i] = add ? buffer[i] + sample : sample;
    }
}

void upload(uint32_t guest_address, const int32_t* buffer, uint32_t count) {
    uint8_t* bytes = guest_memory::host_pointer(guest_address, count * 4);
    if (bytes == nullptr) {
        runtime_log::warning("ax", "a mixing buffer at 0x%08X is not in memory", guest_address);
        return;
    }
    for (uint32_t i = 0; i < count; ++i) {
        const auto sample = static_cast<uint32_t>(buffer[i]);
        bytes[4 * i] = static_cast<uint8_t>(sample >> 24);
        bytes[4 * i + 1] = static_cast<uint8_t>(sample >> 16);
        bytes[4 * i + 2] = static_cast<uint8_t>(sample >> 8);
        bytes[4 * i + 3] = static_cast<uint8_t>(sample);
    }
}

void read_command_list(uint32_t guest_address, uint16_t length) {
    if (length >= g_state.command_list.size()) {
        runtime_log::warning("ax", "the command list at 0x%08X is too long (%u words)", guest_address, length);
        return;
    }
    const uint8_t* bytes = guest_memory::host_pointer(guest_address, length * 2u);
    if (bytes == nullptr) {
        runtime_log::warning("ax", "the command list at 0x%08X is not in memory", guest_address);
        return;
    }
    for (uint16_t i = 0; i < length; ++i) {
        g_state.command_list[i] = static_cast<uint16_t>((bytes[2 * i] << 8) | bytes[2 * i + 1]);
    }
}

// ---- The commands ----

std::array<int32_t*, 9> all_buffers() {
    return {g_state.main_left.data(),  g_state.main_right.data(),  g_state.main_surround.data(),
            g_state.aux_a_left.data(), g_state.aux_a_right.data(), g_state.aux_a_surround.data(),
            g_state.aux_b_left.data(), g_state.aux_b_right.data(), g_state.aux_b_surround.data()};
}

// SETUP: each of the nine buffers starts as a ramp (value and delta), or as silence.
void set_up(uint32_t guest_address) {
    const uint8_t* bytes = guest_memory::host_pointer(guest_address, 9 * 3 * 2);
    if (bytes == nullptr) {
        return;
    }
    const auto buffers = all_buffers();
    for (size_t i = 0; i < buffers.size(); ++i) {
        const uint8_t* entry = bytes + i * 6;
        const auto value = static_cast<int32_t>(read_be32(entry));
        const auto delta = static_cast<int16_t>((entry[4] << 8) | entry[5]);
        for (uint32_t sample = 0; sample < FRAME; ++sample) {
            buffers[i][sample] = value == 0 ? 0 : value + static_cast<int32_t>(sample) * delta;
        }
    }
}

// DOWNLOAD_AND_MIX_WITH_VOLUME: three groups of three buffers from memory, each group
// scaled by its volume and added to main, aux A and aux B.
void download_and_mix_with_volume(uint32_t guest_address, const std::array<uint16_t, 3>& volumes) {
    const auto buffers = all_buffers();
    for (uint32_t group = 0; group < 3; ++group) {
        // The DSP reads the same three buffers from the start for each group.
        const uint8_t* bytes = guest_memory::host_pointer(guest_address, 3 * BUFFER_BYTES);
        if (bytes == nullptr) {
            return;
        }
        for (uint32_t channel = 0; channel < 3; ++channel) {
            int32_t* buffer = buffers[group * 3 + channel];
            for (uint32_t i = 0; i < FRAME; ++i) {
                const int64_t sample = static_cast<int32_t>(read_be32(bytes + 4 * (channel * FRAME + i)));
                buffer[i] += static_cast<int32_t>((sample * volumes[group]) >> 15);
            }
        }
    }
}

void play_voices(uint32_t voice_list) {
    const ax_voice::MixBuffers buffers{
        g_state.main_left.data(),  g_state.main_right.data(),  g_state.main_surround.data(),
        g_state.aux_a_left.data(), g_state.aux_a_right.data(), g_state.aux_a_surround.data(),
        g_state.aux_b_left.data(), g_state.aux_b_right.data(), g_state.aux_b_surround.data()};
    ax_voice::play_voice_list(voice_list, buffers, g_state.version);
}

// MIX_AUX_A / MIX_AUX_B: the send's three buffers go to the CPU (for its effects, unless
// the address is 0), and what the CPU made of the last frame's is added to main.
void mix_aux(int send, uint32_t upload_address, uint32_t download_address) {
    int32_t* buffers[3] = {g_state.aux_a_left.data(), g_state.aux_a_right.data(), g_state.aux_a_surround.data()};
    if (send == 1) {
        buffers[0] = g_state.aux_b_left.data();
        buffers[1] = g_state.aux_b_right.data();
        buffers[2] = g_state.aux_b_surround.data();
    }
    if (upload_address != 0) {
        for (uint32_t channel = 0; channel < 3; ++channel) {
            upload(upload_address + channel * BUFFER_BYTES, buffers[channel], FRAME);
        }
    }
    download(download_address, g_state.main_left.data(), FRAME, true);
    download(download_address + BUFFER_BYTES, g_state.main_right.data(), FRAME, true);
    download(download_address + 2 * BUFFER_BYTES, g_state.main_surround.data(), FRAME, true);
}

void upload_main(uint32_t guest_address) {
    upload(guest_address, g_state.main_left.data(), FRAME);
    upload(guest_address + BUFFER_BYTES, g_state.main_right.data(), FRAME);
    upload(guest_address + 2 * BUFFER_BYTES, g_state.main_surround.data(), FRAME);
}

// SET_MAIN: one buffer from memory becomes both main channels (surround silent).
void set_main(uint32_t guest_address) {
    download(guest_address, g_state.main_left.data(), FRAME, false);
    std::copy(g_state.main_left.begin(), g_state.main_left.end(), g_state.main_right.begin());
    g_state.main_surround.fill(0);
}

// The compressor: when a sample passes the threshold, the main channels follow one of
// the CPU's gain ramps (an attack based on the last frame, then the release ramps).
void run_compressor(uint16_t threshold, uint16_t release_frames, uint32_t table_address) {
    bool triggered = false;
    for (uint32_t i = 0; i < FRAME && !triggered; ++i) {
        triggered = std::abs(g_state.main_left[i]) > threshold || std::abs(g_state.main_right[i]) > threshold;
    }
    constexpr uint32_t FRAME_TABLE_BYTES = FRAME * sizeof(int16_t);
    constexpr uint32_t ATTACK_ENTRIES = 11;  // the release ramps follow the attack ramps
    uint32_t table_offset = 0;
    if (triggered) {
        table_offset = g_state.compressor_position * FRAME_TABLE_BYTES;
        g_state.compressor_position = release_frames;
    } else if (g_state.compressor_position != 0) {
        --g_state.compressor_position;
        table_offset = (ATTACK_ENTRIES + g_state.compressor_position) * FRAME_TABLE_BYTES;
    } else {
        return;
    }
    const uint8_t* ramp = guest_memory::host_pointer(table_address + table_offset, FRAME_TABLE_BYTES);
    if (ramp == nullptr) {
        return;
    }
    for (uint32_t i = 0; i < FRAME; ++i) {
        const uint16_t gain = static_cast<uint16_t>((ramp[2 * i] << 8) | ramp[2 * i + 1]);
        g_state.main_left[i] = static_cast<int32_t>((int64_t{g_state.main_left[i]} * gain) >> 15);
        g_state.main_right[i] = static_cast<int32_t>((int64_t{g_state.main_right[i]} * gain) >> 15);
    }
}

// OUTPUT: the main surround buffer goes to the CPU, and the final samples to the buffer
// the audio DMA plays: 16 bits, big-endian, right channel first.
void output(uint32_t samples_address, uint32_t surround_address) {
    upload(surround_address, g_state.main_surround.data(), FRAME);
    uint8_t* bytes = guest_memory::host_pointer(samples_address, FRAME * 4);
    if (bytes == nullptr) {
        runtime_log::warning("ax", "the output buffer at 0x%08X is not in memory", samples_address);
        return;
    }
    for (uint32_t i = 0; i < FRAME; ++i) {
        const auto left = static_cast<uint16_t>(clamp_to_16_bits(g_state.main_left[i]));
        const auto right = static_cast<uint16_t>(clamp_to_16_bits(g_state.main_right[i]));
        bytes[4 * i] = static_cast<uint8_t>(right >> 8);
        bytes[4 * i + 1] = static_cast<uint8_t>(right);
        bytes[4 * i + 2] = static_cast<uint8_t>(left >> 8);
        bytes[4 * i + 3] = static_cast<uint8_t>(left);
    }
}

// MIX_AUX_B_LEFT_RIGHT: aux B's left and right go to the CPU; what comes back replaces
// them and is added to main.
void mix_aux_b_left_right(uint32_t upload_address, uint32_t download_address) {
    upload(upload_address, g_state.aux_b_left.data(), FRAME);
    upload(upload_address + BUFFER_BYTES, g_state.aux_b_right.data(), FRAME);
    download(download_address, g_state.aux_b_left.data(), FRAME, false);
    download(download_address + BUFFER_BYTES, g_state.aux_b_right.data(), FRAME, false);
    for (uint32_t i = 0; i < FRAME; ++i) {
        g_state.main_left[i] += g_state.aux_b_left[i];
        g_state.main_right[i] += g_state.aux_b_right[i];
    }
}

// SET_OPPOSITE_LEFT_RIGHT: one buffer from memory, inverted on the left.
void set_opposite_left_right(uint32_t guest_address) {
    download(guest_address, g_state.main_right.data(), FRAME, false);
    for (uint32_t i = 0; i < FRAME; ++i) {
        g_state.main_left[i] = -g_state.main_right[i];
    }
    g_state.main_surround.fill(0);
}

// SEND_AUX_AND_MIX: aux A (all three channels) and aux B's surround go to the CPU; four
// buffers come back and are added to main left and right and to aux B left and right.
void send_aux_and_mix(const std::array<uint32_t, 6>& addresses) {
    upload(addresses[0], g_state.aux_a_left.data(), FRAME);
    upload(addresses[0] + BUFFER_BYTES, g_state.aux_a_right.data(), FRAME);
    upload(addresses[0] + 2 * BUFFER_BYTES, g_state.aux_a_surround.data(), FRAME);
    upload(addresses[1], g_state.aux_b_surround.data(), FRAME);
    download(addresses[2], g_state.main_left.data(), FRAME, true);
    download(addresses[3], g_state.main_right.data(), FRAME, true);
    download(addresses[4], g_state.aux_b_left.data(), FRAME, true);
    download(addresses[5], g_state.aux_b_right.data(), FRAME, true);
}

// Runs the command list in g_state.command_list.
void run_command_list() {
    const auto& list = g_state.command_list;
    uint32_t index = 0;
    uint32_t voice_list = 0;
    auto next = [&]() -> uint16_t { return index < list.size() ? list[index++] : static_cast<uint16_t>(END); };
    auto next_address = [&]() -> uint32_t {
        const uint16_t high = next();
        return join(high, next());
    };
    // A guard against a list that never ends (MORE can chain lists).
    for (int commands = 0; commands < 4096; ++commands) {
        const uint16_t command = next();
        switch (command) {
            case SETUP:
                set_up(next_address());
                break;
            case DOWNLOAD_AND_MIX_WITH_VOLUME: {
                const uint32_t address = next_address();
                const uint16_t main_volume = next();
                const uint16_t aux_a_volume = next();
                const uint16_t aux_b_volume = next();
                download_and_mix_with_volume(address, {main_volume, aux_a_volume, aux_b_volume});
                break;
            }
            case VOICE_LIST_ADDRESS:
                voice_list = next_address();
                break;
            case PLAY_VOICES:
                play_voices(voice_list);
                break;
            case MIX_AUX_A:
            case MIX_AUX_B: {
                const uint32_t upload_address = next_address();
                const uint32_t download_address = next_address();
                mix_aux(command - MIX_AUX_A, upload_address, download_address);
                break;
            }
            case UPLOAD_MAIN:
                upload_main(next_address());
                break;
            case SET_MAIN:
                set_main(next_address());
                break;
            case UNKNOWN_08:
                index += 10;
                break;
            case MIX_AUX_B_WITHOUT_UPLOAD:
                mix_aux(1, 0, next_address());
                break;
            case NOTHING_0A:
            case NOTHING_0B:
            case NOTHING_0C:
                break;
            case MORE: {
                const uint32_t address = next_address();
                const uint16_t length = next();
                read_command_list(address, length);
                index = 0;
                break;
            }
            case OUTPUT: {
                const uint32_t surround_address = next_address();
                const uint32_t samples_address = next_address();
                output(samples_address, surround_address);
                break;
            }
            case END:
                return;
            case MIX_AUX_B_LEFT_RIGHT: {
                const uint32_t upload_address = next_address();
                const uint32_t download_address = next_address();
                mix_aux_b_left_right(upload_address, download_address);
                break;
            }
            case SET_OPPOSITE_LEFT_RIGHT:
                set_opposite_left_right(next_address());
                break;
            case COMPRESSOR: {
                const uint16_t threshold = next();
                const uint16_t release_frames = next();
                run_compressor(threshold, release_frames, next_address());
                break;
            }
            case SEND_AUX_AND_MIX: {
                std::array<uint32_t, 6> addresses{};
                for (uint32_t& address : addresses) {
                    address = next_address();
                }
                send_aux_and_mix(addresses);
                break;
            }
            default:
                runtime_log::warning("ax", "unknown command 0x%04X in a command list", command);
                return;
        }
    }
    runtime_log::warning("ax", "a command list did not end");
}

void receive_task_mail(uint32_t mail) {
    if ((mail & TASK_MAIL_MASK) != TASK_MAIL_TO_DSP) {
        mail = TASK_MAIL_TO_DSP | (mail & ~TASK_MAIL_MASK);  // the program does not check the prefix
    }
    switch (mail) {
        case MAIL_RESUME:
            dsp_interface::send_mail_to_cpu(DSP_RESUME, true);
            g_state.mail_state = MailState::WAITING_FOR_LIST_LENGTH;
            return;
        case MAIL_CONTINUE:
            g_state.mail_state = MailState::WAITING_FOR_LIST_LENGTH;
            return;
        case MAIL_RESET:
            dsp_programs::return_to_boot_rom();
            return;
        case MAIL_NEW_UCODE:
            runtime_log::warning("ax", "the CPU wants to load another DSP program, which is not supported");
            g_state.mail_state = MailState::WAITING_FOR_LIST_LENGTH;
            return;
        default:
            runtime_log::warning("ax", "unknown task mail 0x%08X", mail);
            return;
    }
}

}  // namespace

bool is_known_version(uint32_t code_hash) {
    switch (code_hash) {
        case 0x3AD3B7ACu:
        case 0x3DAF59B9u:
        case 0x4E8A8B21u:
        case 0x07F88145u:
        case 0xE2136399u:
            return true;
        default:
            return false;
    }
}

void start(uint32_t code_hash) {
    g_state = State{};
    g_state.code_hash = code_hash;
    // The oldest version (0x4E8A8B21) has no low-pass filter in its voices and packs the
    // mixer control bits its own way.
    const bool oldest = code_hash == 0x4E8A8B21u;
    g_state.version.has_low_pass_filter = !oldest;
    g_state.version.has_old_mixer_control = oldest;
    runtime_log::info("ax", "the DSP runs AX (program 0x%08X)", code_hash);
    dsp_interface::send_mail_to_cpu(DSP_INIT, true);
}

void receive_mail(uint32_t mail) {
    switch (g_state.mail_state) {
        case MailState::WAITING_FOR_LIST_LENGTH:
            if ((mail & TASK_MAIL_MASK) == MAIL_COMMAND_LIST) {
                g_state.command_list_length = static_cast<uint16_t>(mail);
                g_state.mail_state = MailState::WAITING_FOR_LIST_ADDRESS;
            } else {
                runtime_log::warning("ax", "expected a command list, got mail 0x%08X", mail);
            }
            return;
        case MailState::WAITING_FOR_LIST_ADDRESS:
            read_command_list(mail, g_state.command_list_length);
            run_command_list();
            g_state.command_list_length = 0;
            dsp_interface::send_mail_to_cpu(DSP_YIELD, true);
            g_state.mail_state = MailState::WAITING_FOR_NEXT_TASK;
            return;
        case MailState::WAITING_FOR_NEXT_TASK:
            receive_task_mail(mail);
            return;
    }
}

}  // namespace ax_microcode
