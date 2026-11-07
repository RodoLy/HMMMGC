// See ax_voice.h. Ported from Dolphin (AXStructs.h, AXVoice.h, DSPAccelerator.cpp), with
// the same arithmetic, so that the voices sound the same.

#include "audio/ax_voice.h"

#include "guest_memory.h"
#include "hardware/audio_memory.h"
#include "runtime_log.h"

#include <algorithm>
#include <array>
#include <bit>
#include <cstddef>
#include <cstring>

namespace ax_voice {
namespace {

// ---- The parameter block (AXPB), as the GameCube's AX lays it out ----
//
// Every field is 16 bits, big-endian in the guest's memory; a 32-bit value is split in a
// high and a low half.

struct Volume {
    uint16_t volume;  // 1.15 fixed point
    uint16_t delta;   // added after each sample when the mix ramps
};

struct Mixer {
    Volume main_left, main_right;
    Volume aux_a_left, aux_a_right;
    Volume aux_b_left, aux_b_right;
    // This order lets the DSP's ROM mix two surround channels with one routine.
    Volume aux_b_surround;
    Volume main_surround;
    Volume aux_a_surround;
};

struct InitialTimeDelay {
    uint16_t on;
    uint16_t memory_address_high;
    uint16_t memory_address_low;
    uint16_t offset_left;
    uint16_t offset_right;
    uint16_t target_left;
    uint16_t target_right;
};

// Changes that the CPU queues for each millisecond of the frame (for precise timing of
// note events): how many in each millisecond, and where the list of them is.
struct UpdateCounts {
    uint16_t per_millisecond[MILLISECONDS_PER_FRAME];
    uint16_t list_address_high;
    uint16_t list_address_low;
};

// The last sample each voice added to each buffer, which the CPU uses to fade out stopped
// voices without a click ("depop").
struct LastSamples {
    int16_t main_left, aux_a_left, aux_b_left;
    int16_t main_right, aux_a_right, aux_b_right;
    int16_t main_surround, aux_a_surround, aux_b_surround;
};

struct VolumeEnvelope {
    int16_t current;  // at the start of the frame
    int16_t delta;    // added after each sample
};

struct SampleAddresses {
    uint16_t looping;
    uint16_t format;             // the accelerator's format register (see Accelerator)
    uint16_t loop_address_high;  // where the loop starts (a shared silent buffer for one-shot voices)
    uint16_t loop_address_low;
    uint16_t end_address_high;  // the last sample, included
    uint16_t end_address_low;
    uint16_t current_address_high;
    uint16_t current_address_low;
};

struct Adpcm {
    int16_t coefficients[16];  // eight pairs, chosen by the predictor in pred_scale
    uint16_t gain;
    uint16_t pred_scale;  // predictor (bits 4-6) and scale (bits 0-3)
    int16_t history1;     // the last two decoded samples (yn1, yn2)
    int16_t history2;
};

// The sample-rate converter. The ratio is 16.16 fixed point (input samples per output
// sample), and so is the position, whose integer part lives in the sample addresses.
struct RateConverter {
    uint16_t ratio_high;
    uint16_t ratio_low;
    uint16_t current_fraction;
    int16_t last_samples[4];
};

// The ADPCM state to restore when a looping voice goes back to its loop start.
struct AdpcmLoop {
    uint16_t pred_scale;
    int16_t history1;
    int16_t history2;
};

struct LowPassFilter {
    uint16_t on;
    int16_t history;  // yn1
    uint16_t a0;
    int16_t b0;
};

struct ParameterBlock {
    uint16_t next_block_high;
    uint16_t next_block_low;
    uint16_t this_block_high;
    uint16_t this_block_low;

    uint16_t rate_converter_type;  // RateConverterType
    uint16_t coefficient_select;   // the polyphase filter's coefficient set
    uint16_t mixer_control;        // which buffers the voice goes to (see MixControl)

    uint16_t running;  // 1: playing
    uint16_t is_stream;

    Mixer mixer;
    InitialTimeDelay initial_time_delay;
    UpdateCounts updates;
    LastSamples last_mixed;
    VolumeEnvelope volume_envelope;
    uint16_t reserved[3];
    SampleAddresses addresses;
    Adpcm adpcm;
    RateConverter rate_converter;
    AdpcmLoop adpcm_loop;
    LowPassFilter low_pass_filter;  // not in the oldest version's layout
    uint16_t loop_counter;

    uint16_t padding[24];
};
static_assert(sizeof(ParameterBlock) % sizeof(uint16_t) == 0);
constexpr size_t PARAMETER_BLOCK_WORDS = sizeof(ParameterBlock) / sizeof(uint16_t);

struct Update {
    uint16_t word_offset;  // into the parameter block
    uint16_t value;
};
constexpr size_t MAX_UPDATES = 32;

enum RateConverterType : uint16_t {
    POLYPHASE = 0,  // a 4-tap filter whose coefficients are in the DSP's ROM: done as LINEAR
    LINEAR = 1,
    NEAREST = 2,  // no conversion
};

// Which buffers a voice is mixed into, and whether its volume ramps (Dolphin's
// AXMixControl). The block's mixer_control is converted to these bits.
enum MixControl : uint32_t {
    MIX_MAIN_LEFT = 0x000001,
    MIX_MAIN_LEFT_RAMP = 0x000002,
    MIX_MAIN_RIGHT = 0x000004,
    MIX_MAIN_RIGHT_RAMP = 0x000008,
    MIX_MAIN_SURROUND = 0x000010,
    MIX_MAIN_SURROUND_RAMP = 0x000020,
    MIX_AUX_A_LEFT = 0x000040,
    MIX_AUX_A_LEFT_RAMP = 0x000080,
    MIX_AUX_A_RIGHT = 0x000100,
    MIX_AUX_A_RIGHT_RAMP = 0x000200,
    MIX_AUX_A_SURROUND = 0x000400,
    MIX_AUX_A_SURROUND_RAMP = 0x000800,
    MIX_AUX_B_LEFT = 0x001000,
    MIX_AUX_B_LEFT_RAMP = 0x002000,
    MIX_AUX_B_RIGHT = 0x004000,
    MIX_AUX_B_RIGHT_RAMP = 0x008000,
    MIX_AUX_B_SURROUND = 0x010000,
    MIX_AUX_B_SURROUND_RAMP = 0x020000,
    MIX_ALL_RAMPS = 0x02AAAA,
};

uint32_t join(uint16_t high, uint16_t low) { return (uint32_t{high} << 16) | low; }

int16_t clamp_to_16_bits(int64_t sample) { return static_cast<int16_t>(std::clamp<int64_t>(sample, -0x8000, 0x7FFF)); }

// ---- Reading and writing the block ----

void read_words(uint32_t guest_address, uint16_t* words, size_t count) {
    const uint8_t* bytes = guest_memory::host_pointer(guest_address, count * sizeof(uint16_t));
    for (size_t i = 0; i < count; ++i) {
        words[i] = bytes != nullptr ? static_cast<uint16_t>((bytes[2 * i] << 8) | bytes[2 * i + 1]) : 0;
    }
}

void write_words(uint32_t guest_address, const uint16_t* words, size_t count) {
    uint8_t* bytes = guest_memory::host_pointer(guest_address, count * sizeof(uint16_t));
    if (bytes == nullptr) {
        return;
    }
    for (size_t i = 0; i < count; ++i) {
        bytes[2 * i] = static_cast<uint8_t>(words[i] >> 8);
        bytes[2 * i + 1] = static_cast<uint8_t>(words[i]);
    }
}

// The oldest version of AX has no low-pass filter in its blocks: in memory, loop_counter
// and the rest follow adpcm_loop directly.
constexpr size_t LOW_PASS_FILTER_WORD = offsetof(ParameterBlock, low_pass_filter) / sizeof(uint16_t);
constexpr size_t LOOP_COUNTER_WORD = offsetof(ParameterBlock, loop_counter) / sizeof(uint16_t);

ParameterBlock read_block(uint32_t guest_address, const ProgramVersion& version) {
    std::array<uint16_t, PARAMETER_BLOCK_WORDS> words{};
    if (version.has_low_pass_filter) {
        read_words(guest_address, words.data(), words.size());
    } else {
        read_words(guest_address, words.data(), LOW_PASS_FILTER_WORD);
        read_words(guest_address + LOW_PASS_FILTER_WORD * 2, words.data() + LOOP_COUNTER_WORD,
                   words.size() - LOOP_COUNTER_WORD);
    }
    return std::bit_cast<ParameterBlock>(words);
}

void write_block(uint32_t guest_address, const ParameterBlock& block, const ProgramVersion& version) {
    const auto words = std::bit_cast<std::array<uint16_t, PARAMETER_BLOCK_WORDS>>(block);
    if (version.has_low_pass_filter) {
        write_words(guest_address, words.data(), words.size());
    } else {
        write_words(guest_address, words.data(), LOW_PASS_FILTER_WORD);
        write_words(guest_address + LOW_PASS_FILTER_WORD * 2, words.data() + LOOP_COUNTER_WORD,
                    words.size() - LOOP_COUNTER_WORD);
    }
}

std::array<Update, MAX_UPDATES> read_updates(const ParameterBlock& block) {
    std::array<uint16_t, MAX_UPDATES * 2> words{};
    read_words(join(block.updates.list_address_high, block.updates.list_address_low), words.data(), words.size());
    return std::bit_cast<std::array<Update, MAX_UPDATES>>(words);
}

// The updates queued for one millisecond: each one writes a word of the block.
void apply_updates(uint32_t millisecond, ParameterBlock& block, const std::array<Update, MAX_UPDATES>& updates) {
    auto words = std::bit_cast<std::array<uint16_t, PARAMETER_BLOCK_WORDS>>(block);
    uint32_t first = 0;
    for (uint32_t earlier = 0; earlier < millisecond; ++earlier) {
        first += block.updates.per_millisecond[earlier];
    }
    const uint32_t last = std::min<uint32_t>(first + block.updates.per_millisecond[millisecond], MAX_UPDATES);
    for (uint32_t i = first; i < last; ++i) {
        if (updates[i].word_offset < words.size()) {
            words[updates[i].word_offset] = updates[i].value;
        }
    }
    block = std::bit_cast<ParameterBlock>(words);
}

// The block's mixer_control bits, in the meaning of this version of the program.
uint32_t convert_mixer_control(uint16_t mixer_control, const ProgramVersion& version) {
    uint32_t mix = 0;
    if (version.has_old_mixer_control) {
        mix |= MIX_MAIN_LEFT | MIX_MAIN_RIGHT;
        if (mixer_control & 0x0010) {  // Dolby Pro Logic II
            if ((mixer_control & 0x0006) == 0) {
                mix |= MIX_AUX_B_LEFT | MIX_AUX_B_RIGHT;
            }
            if ((mixer_control & 0x0007) == 1) {
                mix |= MIX_AUX_A_LEFT | MIX_AUX_A_RIGHT | MIX_AUX_A_SURROUND;
            }
        } else {
            if (mixer_control & 0x0001) {
                mix |= MIX_AUX_A_LEFT | MIX_AUX_A_RIGHT;
            }
            if (mixer_control & 0x0002) {
                mix |= MIX_AUX_B_LEFT | MIX_AUX_B_RIGHT;
            }
            if (mixer_control & 0x0004) {
                mix |= MIX_MAIN_SURROUND;
                if (mix & MIX_AUX_A_LEFT) {
                    mix |= MIX_AUX_A_SURROUND;
                }
                if (mix & MIX_AUX_B_LEFT) {
                    mix |= MIX_AUX_B_SURROUND;
                }
            }
        }
        if (mixer_control & 0x0008) {
            mix |= MIX_ALL_RAMPS;
        }
        return mix;
    }
    // One bit for each channel, and one ramp bit for each group.
    constexpr std::pair<uint16_t, uint32_t> BITS[] = {
        {0x0001, MIX_MAIN_LEFT},
        {0x0002, MIX_MAIN_RIGHT},
        {0x0004, MIX_MAIN_SURROUND},
        {0x0008, MIX_MAIN_LEFT_RAMP | MIX_MAIN_RIGHT_RAMP | MIX_MAIN_SURROUND_RAMP},
        {0x0010, MIX_AUX_A_LEFT},
        {0x0020, MIX_AUX_A_RIGHT},
        {0x0040, MIX_AUX_A_LEFT_RAMP | MIX_AUX_A_RIGHT_RAMP},
        {0x0080, MIX_AUX_A_SURROUND},
        {0x0100, MIX_AUX_A_SURROUND_RAMP},
        {0x0200, MIX_AUX_B_LEFT},
        {0x0400, MIX_AUX_B_RIGHT},
        {0x0800, MIX_AUX_B_LEFT_RAMP | MIX_AUX_B_RIGHT_RAMP},
        {0x1000, MIX_AUX_B_SURROUND},
        {0x2000, MIX_AUX_B_SURROUND_RAMP},
    };
    for (const auto& [bit, channels] : BITS) {
        if (mixer_control & bit) {
            mix |= channels;
        }
    }
    return mix;
}

// ---- The accelerator ----
//
// The DSP reads samples from the audio memory through its accelerator, which decodes
// them on the way. Addresses count samples: nibbles for 4-bit formats, bytes for 8-bit
// ones, 16-bit words for 16-bit ones. The format register gives the size (bits 0-1:
// 4, 8 or 16 bits), the decoding (bits 2-3: ADPCM or PCM) and, for PCM, the gain's
// scale (bits 4-5).
class Accelerator {
public:
    explicit Accelerator(ParameterBlock& block) : block_(block) {
        start_address_ = join(block.addresses.loop_address_high, block.addresses.loop_address_low) & ADDRESS_MASK;
        end_address_ = join(block.addresses.end_address_high, block.addresses.end_address_low) & ADDRESS_MASK;
        current_address_ =
            join(block.addresses.current_address_high, block.addresses.current_address_low) & CURRENT_ADDRESS_MASK;
        format_ = block.addresses.format;
        history1_ = block.adpcm.history1;
        history2_ = block.adpcm.history2;
        gain_ = static_cast<int16_t>(block.adpcm.gain);
        pred_scale_ = block.adpcm.pred_scale & 0x7F;
    }

    // One decoded sample, and the step to the next one; at the end of the sample it goes
    // back to the loop start, and stops a voice that does not loop.
    int16_t read_sample() {
        if (reads_stopped_) {
            return 0;
        }
        const uint16_t raw = current_raw_sample();
        const int index = (pred_scale_ >> 4) & 0x7;
        const int32_t coefficient1 = block_.adpcm.coefficients[index * 2];
        const int32_t coefficient2 = block_.adpcm.coefficients[index * 2 + 1];
        int16_t value = 0;
        uint32_t step = 0;
        switch ((format_ >> 2) & 0x3) {
            case DECODE_ADPCM: {
                int32_t nibble = raw & 0xF;
                if (nibble >= 8) {
                    nibble -= 16;
                }
                const int32_t scale = 1 << (pred_scale_ & 0xF);
                const int32_t decoded =
                    scale * nibble + ((0x400 + coefficient1 * history1_ + coefficient2 * history2_) >> 11);
                value = static_cast<int16_t>(std::clamp(decoded, -0x7FFF, 0x7FFF));
                step = 2;
                history2_ = history1_;
                history1_ = value;
                current_address_ += 1;
                // Every 16 nibbles a frame header byte holds the next predictor and scale.
                // Two end addresses are handled apart, as the hardware does.
                if ((end_address_ & 0xF) == 0x0 && current_address_ == end_address_) {
                    current_address_ = start_address_ + 1;
                } else if ((end_address_ & 0xF) == 0x1 && current_address_ == end_address_ - 1) {
                    current_address_ = start_address_;
                } else if ((current_address_ & 15) == 0) {
                    pred_scale_ = audio_memory::read8((current_address_ & ~15u) >> 1);
                    current_address_ += 2;
                    step += 2;
                }
                break;
            }
            default: {  // PCM: the gain, scaled, and the same two-tap filter as ADPCM
                static constexpr int GAIN_SHIFTS[] = {11, 0, 16, 0};
                const int shift = GAIN_SHIFTS[(format_ >> 4) & 0x3];
                const int32_t decoded = ((static_cast<int32_t>(gain_) * static_cast<int16_t>(raw)) >> shift) +
                                        ((coefficient1 * history1_) >> shift) + ((coefficient2 * history2_) >> shift);
                value = static_cast<int16_t>(decoded);
                history2_ = history1_;
                history1_ = value;
                step = 2;
                current_address_ += 1;
                break;
            }
        }
        if (current_address_ == end_address_ + step - 1) {
            current_address_ = start_address_;
            reads_stopped_ = true;
            on_end_of_sample();
        }
        current_address_ &= CURRENT_ADDRESS_MASK;
        return value;
    }

    // Writes the position and the decoder's state back to the block.
    void store() const {
        block_.addresses.current_address_high = static_cast<uint16_t>(current_address_ >> 16);
        block_.addresses.current_address_low = static_cast<uint16_t>(current_address_);
        block_.adpcm.history1 = history1_;
        block_.adpcm.history2 = history2_;
        block_.adpcm.pred_scale = pred_scale_;
    }

private:
    static constexpr uint32_t ADDRESS_MASK = 0x3FFFFFFFu;
    static constexpr uint32_t CURRENT_ADDRESS_MASK = 0xBFFFFFFFu;
    static constexpr uint16_t DECODE_ADPCM = 0;

    uint16_t current_raw_sample() const {
        switch (format_ & 0x3) {
            case 0: {  // 4 bits
                const uint8_t byte = audio_memory::read8(current_address_ >> 1);
                return (current_address_ & 1) ? (byte & 0xF) : (byte >> 4);
            }
            case 1:  // 8 bits
                return audio_memory::read8(current_address_);
            case 2:  // 16 bits
                return static_cast<uint16_t>((audio_memory::read8(current_address_ * 2) << 8) |
                                             audio_memory::read8(current_address_ * 2 + 1));
            default:
                return 0;
        }
    }

    // What the DSP program does when the accelerator reaches the end of a sample: a
    // looping voice continues from the loop start with the loop's ADPCM state (a stream
    // keeps its state and counts the loop); any other voice stops.
    void on_end_of_sample() {
        if (block_.addresses.looping) {
            pred_scale_ = block_.adpcm_loop.pred_scale & 0x7F;
            if (block_.is_stream != 1) {
                history1_ = block_.adpcm_loop.history1;
                history2_ = block_.adpcm_loop.history2;
            } else {
                block_.loop_counter++;
            }
            reads_stopped_ = false;  // writing the history resumes the reads
        } else {
            block_.running = 0;
        }
    }

    ParameterBlock& block_;
    uint32_t start_address_ = 0;
    uint32_t end_address_ = 0;
    uint32_t current_address_ = 0;
    uint16_t format_ = 0;
    int16_t history1_ = 0;
    int16_t history2_ = 0;
    int16_t gain_ = 0;
    uint16_t pred_scale_ = 0;
    bool reads_stopped_ = false;
};

// ---- Sample-rate conversion ----

// Reads samples through the accelerator and converts them to `count` samples at 32 kHz.
// The position starts at the block's fraction, so that the conversion continues without
// a seam from the last frame; the four last input samples are kept in the block.
void convert_rate(Accelerator& accelerator, ParameterBlock& block, int16_t* output, uint32_t count) {
    int16_t* last = block.rate_converter.last_samples;
    if (block.rate_converter_type == NEAREST) {
        for (uint32_t i = 0; i < count; ++i) {
            output[i] = accelerator.read_sample();
        }
        std::memcpy(last, output + count - 4, 4 * sizeof(int16_t));
        return;
    }
    // LINEAR, and POLYPHASE without the ROM's coefficients: a circular buffer of the
    // last input samples, interpolated between the two oldest.
    const uint32_t ratio = join(block.rate_converter.ratio_high, block.rate_converter.ratio_low);
    uint32_t position = block.rate_converter.current_fraction;
    int16_t history[4] = {last[0], last[1], last[2], last[3]};
    uint32_t index = 4;
    for (uint32_t i = 0; i < count; ++i) {
        position += ratio;
        while (position >= 0x10000) {
            history[index++ & 3] = accelerator.read_sample();
            position -= 0x10000;
        }
        const uint16_t fraction = position & 0xFFFF;
        if (fraction != 0) {
            const int32_t first = history[index++ & 3];
            const int32_t second = history[index++ & 3];
            const uint16_t complement = static_cast<uint16_t>(-fraction);
            output[i] = static_cast<int16_t>((first * complement + second * fraction) >> 16);
            index += 2;
        } else {
            output[i] = history[index++ & 3];
            index += 3;
        }
    }
    last[3] = history[--index & 3];
    last[2] = history[--index & 3];
    last[1] = history[--index & 3];
    last[0] = history[--index & 3];
    block.rate_converter.current_fraction = static_cast<uint16_t>(position & 0xFFFF);
}

// Adds the samples to a buffer at the voice's volume for it, ramping the volume if asked,
// and keeps the last sample for the depop.
void mix_into(int32_t* buffer, const int16_t* samples, uint32_t count, Volume& volume, int16_t& last_mixed, bool ramp) {
    const uint16_t delta = ramp ? volume.delta : 0;
    for (uint32_t i = 0; i < count; ++i) {
        const int16_t sample = clamp_to_16_bits((int64_t{samples[i]} * volume.volume) >> 15);
        buffer[i] += sample;
        volume.volume = static_cast<uint16_t>(volume.volume + delta);
        last_mixed = sample;
    }
}

// One millisecond of a voice.
void play_millisecond(ParameterBlock& block, const MixBuffers& buffers, uint32_t mix) {
    if (block.running != 1) {
        return;
    }
    constexpr uint32_t COUNT = SAMPLES_PER_MILLISECOND;
    int16_t samples[COUNT];
    Accelerator accelerator(block);
    convert_rate(accelerator, block, samples, COUNT);
    accelerator.store();

    // The volume envelope (signed on the GameCube).
    for (uint32_t i = 0; i < COUNT; ++i) {
        samples[i] = clamp_to_16_bits((int32_t{samples[i]} * block.volume_envelope.current) >> 15);
        block.volume_envelope.current =
            static_cast<int16_t>(block.volume_envelope.current + block.volume_envelope.delta);
    }

    if (block.low_pass_filter.on != 0) {
        LowPassFilter& filter = block.low_pass_filter;
        for (uint32_t i = 0; i < COUNT; ++i) {
            filter.history = samples[i] =
                clamp_to_16_bits((filter.a0 * int32_t{samples[i]} + filter.b0 * int32_t{filter.history}) >> 15);
        }
    }

    struct Destination {
        uint32_t mix_bit;
        uint32_t ramp_bit;
        int32_t* buffer;
        Volume& volume;
        int16_t& last_mixed;
    };
    Mixer& mixer = block.mixer;
    LastSamples& last = block.last_mixed;
    const Destination destinations[] = {
        {MIX_MAIN_LEFT, MIX_MAIN_LEFT_RAMP, buffers.main_left, mixer.main_left, last.main_left},
        {MIX_MAIN_RIGHT, MIX_MAIN_RIGHT_RAMP, buffers.main_right, mixer.main_right, last.main_right},
        {MIX_MAIN_SURROUND, MIX_MAIN_SURROUND_RAMP, buffers.main_surround, mixer.main_surround, last.main_surround},
        {MIX_AUX_A_LEFT, MIX_AUX_A_LEFT_RAMP, buffers.aux_a_left, mixer.aux_a_left, last.aux_a_left},
        {MIX_AUX_A_RIGHT, MIX_AUX_A_RIGHT_RAMP, buffers.aux_a_right, mixer.aux_a_right, last.aux_a_right},
        {MIX_AUX_A_SURROUND, MIX_AUX_A_SURROUND_RAMP, buffers.aux_a_surround, mixer.aux_a_surround,
         last.aux_a_surround},
        {MIX_AUX_B_LEFT, MIX_AUX_B_LEFT_RAMP, buffers.aux_b_left, mixer.aux_b_left, last.aux_b_left},
        {MIX_AUX_B_RIGHT, MIX_AUX_B_RIGHT_RAMP, buffers.aux_b_right, mixer.aux_b_right, last.aux_b_right},
        {MIX_AUX_B_SURROUND, MIX_AUX_B_SURROUND_RAMP, buffers.aux_b_surround, mixer.aux_b_surround,
         last.aux_b_surround},
    };
    for (const Destination& destination : destinations) {
        if (mix & destination.mix_bit) {
            mix_into(destination.buffer, samples, COUNT, destination.volume, destination.last_mixed,
                     (mix & destination.ramp_bit) != 0);
        }
    }
}

}  // namespace

void play_voice_list(uint32_t first_block, const MixBuffers& buffers, const ProgramVersion& version) {
    // A guard against a list that loops on itself (a game bug, or a stale pointer).
    constexpr int MAX_VOICES = 256;
    uint32_t address = first_block;
    for (int voice = 0; address != 0 && voice < MAX_VOICES; ++voice) {
        ParameterBlock block = read_block(address, version);
        const std::array<Update, MAX_UPDATES> updates = read_updates(block);
        MixBuffers millisecond_buffers = buffers;
        for (uint32_t millisecond = 0; millisecond < MILLISECONDS_PER_FRAME; ++millisecond) {
            apply_updates(millisecond, block, updates);
            play_millisecond(block, millisecond_buffers, convert_mixer_control(block.mixer_control, version));
            for (int32_t** buffer :
                 {&millisecond_buffers.main_left, &millisecond_buffers.main_right, &millisecond_buffers.main_surround,
                  &millisecond_buffers.aux_a_left, &millisecond_buffers.aux_a_right,
                  &millisecond_buffers.aux_a_surround, &millisecond_buffers.aux_b_left,
                  &millisecond_buffers.aux_b_right, &millisecond_buffers.aux_b_surround}) {
                *buffer += SAMPLES_PER_MILLISECOND;
            }
        }
        write_block(address, block, version);
        address = join(block.next_block_high, block.next_block_low);
    }
    if (address != 0) {
        runtime_log::warning("ax", "the voice list at 0x%08X has more than %d voices", first_block, MAX_VOICES);
    }
}

}  // namespace ax_voice
