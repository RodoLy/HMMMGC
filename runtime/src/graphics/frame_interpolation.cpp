// See frame_interpolation.h.

#include "graphics/frame_interpolation.h"

#include "runtime_log.h"

#include <aurora/aurora.h>
#include <aurora/gfx.h>

#include <algorithm>
#include <chrono>

namespace frame_interpolation {
namespace {

using Clock = std::chrono::steady_clock;

// The time of a frame of the game at normal speed: two video fields of an NTSC GameCube.
constexpr auto NORMAL_GAME_FRAME_TIME = std::chrono::nanoseconds(1'000'000'000LL * 2 * 1001 / 60000);
// Frames further apart than this (loading, a pause) start the schedule over.
constexpr auto LONGEST_GAME_FRAME_TIME = std::chrono::milliseconds(100);
// A frame that took longer than this many frame times did not keep up: aurora shows it
// without frames in between, and makes fewer of them if that keeps happening.
constexpr double LATE_FRAME = 1.5;
// How often the log tells how the frames went.
constexpr auto REPORT_INTERVAL = std::chrono::seconds(30);

unsigned g_frame_rate = GAME_FRAME_RATE;
Clock::time_point g_last_frame;
Clock::duration g_game_frame_time = NORMAL_GAME_FRAME_TIME;  // smoothed over a few frames
Clock::time_point g_next_due;                                // when the next frame is due
Clock::time_point g_last_report = Clock::now();
unsigned g_late_frames = 0;

uint64_t nanoseconds_of(Clock::time_point time) {
    return static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(time.time_since_epoch()).count());
}

void report_now_and_then(Clock::time_point now) {
    if (now - g_last_report < REPORT_INTERVAL) {
        return;
    }
    g_last_report = now;
    AuroraPresentTiming timing{};
    aurora_get_present_timing(&timing);
    if (!is_on()) {
        runtime_log::info("graphics", "%.1f frames a second shown (%.1f ms apart, the slowest 5%% %.1f ms)",
                          timing.framesPerSecond, timing.averageFrameTimeMs, timing.p95FrameTimeMs);
        return;
    }
    AuroraFrameInterpolationDiagnostics diagnostics{};
    aurora_get_frame_interpolation_diagnostics(&diagnostics);
    runtime_log::info("graphics",
                      "%.1f frames a second shown (%.1f ms apart, the slowest 5%% %.1f ms); last frame: %u of %u "
                      "draws matched, %u frames in between; %llu frames without them for too few matches, %u late",
                      timing.framesPerSecond, timing.averageFrameTimeMs, timing.p95FrameTimeMs, diagnostics.matches,
                      diagnostics.candidates, diagnostics.activeSamples,
                      static_cast<unsigned long long>(diagnostics.framesLowMatch), g_late_frames);
    g_late_frames = 0;
}

}  // namespace

void configure(unsigned frame_rate) {
    g_frame_rate = frame_rate >= MAXIMUM_FRAME_RATE    ? MAXIMUM_FRAME_RATE
                   : frame_rate >= 2 * GAME_FRAME_RATE ? 2 * GAME_FRAME_RATE
                                                       : GAME_FRAME_RATE;
}

void apply_to_aurora() {
    if (!is_on()) {
        return;
    }
    // aurora's rates are for a game at 60 (see frame_interpolation.h).
    aurora_set_frame_interpolation_fps(g_frame_rate * 2);
    runtime_log::info("graphics", "%u frames a second: %u made between each two of the game", g_frame_rate,
                      g_frame_rate / GAME_FRAME_RATE - 1);
    if (!aurora_display_shows_interpolation()) {
        runtime_log::warning("graphics", "the screen refreshes too slowly to show the frames in between");
    }
}

void set_frame_rate(unsigned frame_rate) {
    const bool was_on = is_on();
    configure(frame_rate);
    if (is_on()) {
        apply_to_aurora();
    } else if (was_on) {
        // No frames in between, and each frame shown as soon as it comes.
        aurora_set_frame_interpolation_fps(0);
        aurora_set_present_schedule(0, 0);
        runtime_log::info("graphics", "30 frames a second: none made in between");
    }
    g_last_frame = Clock::time_point{};  // the schedule starts over with the next frame
}

bool is_on() { return g_frame_rate > GAME_FRAME_RATE; }

void schedule_frame() {
    const Clock::time_point now = Clock::now();
    if (!is_on()) {
        report_now_and_then(now);
        return;
    }
    const Clock::duration since_last = now - g_last_frame;
    g_last_frame = now;
    if (since_last > LONGEST_GAME_FRAME_TIME) {
        // A long wait: no movement to fill in. Show this frame now and start the schedule over.
        g_next_due = now + g_game_frame_time;
        aurora_report_producer_paced(false);
        aurora_set_present_schedule(0, 0);
        return;
    }
    g_game_frame_time += (since_last - g_game_frame_time) / 8;
    const bool kept_up = since_last <= g_game_frame_time * LATE_FRAME;
    if (!kept_up) {
        ++g_late_frames;
    }
    aurora_report_producer_paced(kept_up);
    // The frames go out on one grid, a game frame apart: this frame's group starts where the
    // last one ended, or now if the game fell behind it. The frames in between come first,
    // and the game's own frame last, one frame in between before the next group.
    const Clock::time_point due = std::max(g_next_due, now);
    g_next_due = due + g_game_frame_time;
    aurora_set_present_schedule(
        nanoseconds_of(due),
        static_cast<uint64_t>(std::chrono::duration_cast<std::chrono::nanoseconds>(g_game_frame_time).count()));
    report_now_and_then(now);
}

}  // namespace frame_interpolation
