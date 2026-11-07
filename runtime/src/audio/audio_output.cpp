// See audio_output.h.

#include "audio/audio_output.h"

#include "game_clock.h"
#include "runtime_log.h"

#include <SDL3/SDL.h>

#include <algorithm>
#include <cmath>
#include <vector>

namespace audio_output {
namespace {

constexpr int CHANNELS = 2;
constexpr int BYTES_PER_FRAME = CHANNELS * static_cast<int>(sizeof(int16_t));

// The sound arrives in bursts, as the game takes its audio interrupts (see
// dsp_interface.h). The device starts once this much sound is waiting, and the speed
// control below keeps about this much waiting: the margin that covers the time between
// bursts.
constexpr double TARGET_MILLISECONDS = 60;
// Beyond this much waiting, new sound is dropped instead of piling up.
constexpr double MAX_QUEUED_MILLISECONDS = 250;
// The audio DMA plays 8 frames at a time; they go to SDL in batches of 5 ms at 32 kHz.
constexpr size_t BATCH_FRAMES = 160;

// Speed control, as in Dolphin's mixer. The PC's clock paces the audio DMA and the sound
// card plays by its own clock; the two never run at exactly the same speed, so over
// minutes the waiting sound would drain (gaps) or pile up (dropped sound). The playback
// speed follows how far the waiting sound is from the target, averaged over about 30
// batches: at most half a percent faster or slower, which the ear does not notice.
constexpr double MAX_SPEED_CHANGE = 0.005;
constexpr double SPEED_CHANGE_PER_MILLISECOND = MAX_SPEED_CHANGE / 50;  // 50 ms off: the maximum
constexpr double AVERAGING = 1.0 / 32;
// The speed is only set again when it moves by more than this.
constexpr double SPEED_RESOLUTION = 0.00005;

struct Output {
    SDL_AudioStream* stream = nullptr;
    uint32_t sample_rate = 0;
    bool playing = false;  // the device opens paused, and pauses again if the sound runs out
    bool failed = false;
    double average_queued_milliseconds = TARGET_MILLISECONDS;
    double speed = 1.0;
    std::vector<int16_t> batch;  // host order, left channel first
};
Output g_output;

bool open_stream(uint32_t sample_rate) {
    if (g_output.failed) {
        return false;
    }
    if (SDL_WasInit(SDL_INIT_AUDIO) == 0 && !SDL_InitSubSystem(SDL_INIT_AUDIO)) {
        runtime_log::warning("audio", "no sound: SDL's audio did not start (%s)", SDL_GetError());
        g_output.failed = true;
        return false;
    }
    const SDL_AudioSpec spec{SDL_AUDIO_S16, CHANNELS, static_cast<int>(sample_rate)};
    g_output.stream = SDL_OpenAudioDeviceStream(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec, nullptr, nullptr);
    if (g_output.stream == nullptr) {
        runtime_log::warning("audio", "no sound: the playback device did not open (%s)", SDL_GetError());
        g_output.failed = true;
        return false;
    }
    g_output.sample_rate = sample_rate;
    SDL_AudioSpec device{};
    int device_frames = 0;
    SDL_GetAudioDeviceFormat(SDL_GetAudioStreamDevice(g_output.stream), &device, &device_frames);
    runtime_log::info("audio", "playing at %u Hz on %s (the device plays %d Hz, %d frames at a time)", sample_rate,
                      SDL_GetCurrentAudioDriver(), device.freq, device_frames);
    return true;
}

// When the whole game runs faster (game_clock.h) its sound comes that much faster too, and
// plays that much faster (and higher): the speed control works around the game's speed,
// and the margin, counted in the game's sound, grows with it.
void adjust_speed(double queued_milliseconds) {
    const double game_speed = game_clock::speed_percent() / static_cast<double>(game_clock::NORMAL_SPEED_PERCENT);
    g_output.average_queued_milliseconds += (queued_milliseconds - g_output.average_queued_milliseconds) * AVERAGING;
    const double target = TARGET_MILLISECONDS * game_speed;
    const double speed =
        game_speed * (1.0 + std::clamp((g_output.average_queued_milliseconds - target) * SPEED_CHANGE_PER_MILLISECOND,
                                       -MAX_SPEED_CHANGE, MAX_SPEED_CHANGE));
    if (std::abs(speed - g_output.speed) > SPEED_RESOLUTION) {
        g_output.speed = speed;
        SDL_SetAudioStreamFrequencyRatio(g_output.stream, static_cast<float>(speed));
    }
}

// Hands the batch to SDL, and starts, pauses or adjusts the device by what is waiting.
void flush_batch() {
    if (g_output.batch.empty()) {
        return;
    }
    // SDL counts what is waiting in the format it was given: 16-bit stereo at the DSP's rate.
    const double bytes_per_millisecond = g_output.sample_rate / 1000.0 * BYTES_PER_FRAME;
    const int batch_bytes = static_cast<int>(g_output.batch.size() * sizeof(int16_t));
    int queued_bytes = SDL_GetAudioStreamQueued(g_output.stream);
    if (g_output.playing && queued_bytes == 0) {
        // The sound ran out (the game kept its interrupts off for a while): wait for the
        // margin again rather than play each burst as it comes, with a gap after each.
        SDL_PauseAudioStreamDevice(g_output.stream);
        g_output.playing = false;
    }
    if (queued_bytes <= MAX_QUEUED_MILLISECONDS * bytes_per_millisecond) {
        SDL_PutAudioStreamData(g_output.stream, g_output.batch.data(), batch_bytes);
        queued_bytes += batch_bytes;
    }
    g_output.batch.clear();

    const double queued_milliseconds = queued_bytes / bytes_per_millisecond;
    if (!g_output.playing) {
        if (queued_milliseconds >= TARGET_MILLISECONDS) {
            SDL_ResumeAudioStreamDevice(g_output.stream);
            g_output.playing = true;
            g_output.average_queued_milliseconds = queued_milliseconds;
        }
        return;
    }
    adjust_speed(queued_milliseconds);
}

}  // namespace

void play(const uint8_t* frames, uint32_t frame_count, uint32_t sample_rate) {
    if (g_output.stream == nullptr && !open_stream(sample_rate)) {
        return;
    }
    if (sample_rate != g_output.sample_rate) {
        flush_batch();
        const SDL_AudioSpec spec{SDL_AUDIO_S16, CHANNELS, static_cast<int>(sample_rate)};
        SDL_SetAudioStreamFormat(g_output.stream, &spec, nullptr);
        g_output.sample_rate = sample_rate;
    }
    for (uint32_t i = 0; i < frame_count; ++i) {
        const uint8_t* frame = frames + i * 4;
        const auto right = static_cast<int16_t>((frame[0] << 8) | frame[1]);
        const auto left = static_cast<int16_t>((frame[2] << 8) | frame[3]);
        g_output.batch.push_back(left);
        g_output.batch.push_back(right);
    }
    if (g_output.batch.size() >= BATCH_FRAMES * CHANNELS) {
        flush_batch();
    }
}

void shutdown() {
    if (g_output.stream != nullptr) {
        SDL_DestroyAudioStream(g_output.stream);
        g_output = Output{};
    }
}

}  // namespace audio_output
