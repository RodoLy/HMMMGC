// See game_clock.h.

#include "game_clock.h"

#include <algorithm>

namespace game_clock {
namespace {

using Clock = std::chrono::steady_clock;

// The clock is read and changed on the game's thread only.
struct State {
    Clock::time_point speed_changed_at = Clock::now();
    uint64_t ticks_when_speed_changed = 0;
    bool turbo = false;
};
State g_clock;

// 40.5 time-base ticks per microsecond: nanoseconds * 81 / 2000, at 100%.
constexpr uint64_t TICKS_PER_2000_NANOSECONDS = 81;

void change_speed(bool turbo) {
    const unsigned before = speed_percent();
    const uint64_t now_ticks = elapsed_ticks();
    g_clock.turbo = turbo;
    if (speed_percent() != before) {
        // The ticks so far stay as counted; from now on they count at the new speed.
        g_clock.ticks_when_speed_changed = now_ticks;
        g_clock.speed_changed_at = Clock::now();
    }
}

}  // namespace

unsigned speed_percent() {
    return g_clock.turbo ? TURBO_SPEED_PERCENT : NORMAL_SPEED_PERCENT;
}

uint64_t elapsed_ticks() {
    const auto since_change =
        std::chrono::duration_cast<std::chrono::nanoseconds>(Clock::now() - g_clock.speed_changed_at);
    const auto nanoseconds = static_cast<uint64_t>(since_change.count());
    return g_clock.ticks_when_speed_changed +
           nanoseconds * TICKS_PER_2000_NANOSECONDS * speed_percent() / (2000u * NORMAL_SPEED_PERCENT);
}

std::chrono::nanoseconds real_duration(uint64_t ticks) {
    return std::chrono::nanoseconds(ticks * 2000u * NORMAL_SPEED_PERCENT /
                                    (TICKS_PER_2000_NANOSECONDS * speed_percent()));
}

void set_turbo(bool held) { change_speed(held); }

}  // namespace game_clock
