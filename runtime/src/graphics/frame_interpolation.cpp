// See frame_interpolation.h.

#include "graphics/frame_interpolation.h"

#include "graphics/aurora_host.h"
#include "graphics/gx_draw.h"
#include "runtime_log.h"

#include <dolphin/gx.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <optional>
#include <thread>
#include <unordered_map>
#include <utility>
#include <vector>

namespace frame_interpolation {
namespace {

using Clock = std::chrono::steady_clock;
using Matrix = std::array<float, 16>;  // big enough for every kind; 3x4 matrices use 12

enum class StepKind { ACTION, POSITION_MATRIX, NORMAL_MATRIX, PROJECTION };

struct Step {
    StepKind kind = StepKind::ACTION;
    std::function<void()> action;  // ACTION
    Matrix matrix{};               // the others
    uint32_t slot_or_type = 0;     // the matrix slot, or the projection type
    uint64_t key = 0;              // what finds the same matrix in the frame before; 0: none
    std::optional<Matrix> before;  // that matrix, when it was found and did not jump
};

// The game's frames slower than this (loading, a pause) are shown as they are: there is no
// steady movement to fill in.
constexpr auto SLOWEST_GAME_FRAME = std::chrono::milliseconds(70);
// How far a matrix may move in one game frame and still be mixed (see is_steady).
constexpr float LARGEST_ROTATION_CHANGE = 0.5f;     // per element of the 3x3 part
constexpr float LARGEST_RELATIVE_MOVE = 0.35f;      // of the distance from the camera
constexpr float LARGEST_PROJECTION_CHANGE = 0.25f;  // relative, per element

unsigned g_frames_per_game_frame = 1;
std::vector<Step> g_steps;
std::vector<size_t> g_matrices_waiting_for_draw;  // indices in g_steps
std::unordered_map<uint64_t, uint32_t> g_draw_occurrences;
uint32_t g_projections_this_frame = 0;
std::unordered_map<uint64_t, Matrix> g_matrices_before;  // the frame before's, by key
Clock::time_point g_last_frame_end;
Clock::duration g_game_frame_time = std::chrono::microseconds(1'000'000 / GAME_FRAME_RATE);

uint64_t mix_key(uint64_t key, uint64_t value) {
    // splitmix64's finalizer over the combination: different draws get different keys.
    uint64_t z = key ^ (value + 0x9E3779B97F4A7C15ull + (key << 6) + (key >> 2));
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ull;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBull;
    return z ^ (z >> 31);
}

float length3(float x, float y, float z) { return std::sqrt(x * x + y * y + z * z); }

// Whether a matrix moved little enough between two game frames to be mixed. A camera cut
// or an object placed somewhere else changes it far more than steady movement does.
bool is_steady(StepKind kind, const Matrix& before, const Matrix& now) {
    if (kind == StepKind::PROJECTION) {
        for (size_t i = 0; i < 16; ++i) {
            const float scale = std::max(std::abs(before[i]), std::abs(now[i]));
            if (std::abs(now[i] - before[i]) > LARGEST_PROJECTION_CHANGE * std::max(scale, 1e-3f)) {
                return false;
            }
        }
        return true;
    }
    // 3x4, row by row: the 3x3 part in columns 0 to 2, the translation in column 3.
    for (size_t row = 0; row < 3; ++row) {
        for (size_t column = 0; column < 3; ++column) {
            if (std::abs(now[row * 4 + column] - before[row * 4 + column]) > LARGEST_ROTATION_CHANGE) {
                return false;
            }
        }
    }
    if (kind == StepKind::NORMAL_MATRIX) {
        return true;
    }
    const float moved = length3(now[3] - before[3], now[7] - before[7], now[11] - before[11]);
    const float distance = std::max(length3(now[3], now[7], now[11]), length3(before[3], before[7], before[11]));
    return moved <= LARGEST_RELATIVE_MOVE * distance;
}

void record_matrix(StepKind kind, const float* values, size_t count, uint32_t slot_or_type, uint64_t key) {
    Step step;
    step.kind = kind;
    std::copy(values, values + count, step.matrix.begin());
    step.slot_or_type = slot_or_type;
    step.key = key;
    g_steps.push_back(std::move(step));
}

void load_matrix(const Step& step, const Matrix& matrix) {
    switch (step.kind) {
        case StepKind::POSITION_MATRIX:
            GXLoadPosMtxImm(matrix.data(), step.slot_or_type);
            break;
        case StepKind::NORMAL_MATRIX:
            GXLoadNrmMtxImm(matrix.data(), step.slot_or_type);
            break;
        case StepKind::PROJECTION:
            GXSetProjection(matrix.data(), static_cast<GXProjectionType>(step.slot_or_type));
            break;
        case StepKind::ACTION:
            break;
    }
}

// Draws the recorded frame `progress` of the way from the frame before (0) to this one (1).
void draw(float progress) {
    for (const Step& step : g_steps) {
        if (step.kind == StepKind::ACTION) {
            step.action();
        } else if (!step.before || progress >= 1.f) {
            load_matrix(step, step.matrix);
        } else {
            Matrix mixed;
            for (size_t i = 0; i < mixed.size(); ++i) {
                mixed[i] = (*step.before)[i] + (step.matrix[i] - (*step.before)[i]) * progress;
            }
            load_matrix(step, mixed);
        }
    }
}

// Finds each matrix of this frame in the frame before, and keeps this frame's for the next.
void match_matrices() {
    std::unordered_map<uint64_t, Matrix> matrices_now;
    for (Step& step : g_steps) {
        if (step.kind == StepKind::ACTION || step.key == 0) {
            continue;
        }
        matrices_now.emplace(step.key, step.matrix);
        const auto before = g_matrices_before.find(step.key);
        if (before != g_matrices_before.end() && is_steady(step.kind, before->second, step.matrix)) {
            step.before = before->second;
        }
    }
    g_matrices_before = std::move(matrices_now);
}

void start_next_frame() {
    g_steps.clear();
    g_matrices_waiting_for_draw.clear();
    g_draw_occurrences.clear();
    g_projections_this_frame = 0;
}

}  // namespace

void configure(unsigned frame_rate) {
    const unsigned rate = std::clamp(frame_rate, GAME_FRAME_RATE, MAXIMUM_FRAME_RATE);
    g_frames_per_game_frame = rate / GAME_FRAME_RATE;
    if (g_frames_per_game_frame > 1) {
        runtime_log::info("graphics", "%u frames a second: %u for each frame of the game",
                          g_frames_per_game_frame * GAME_FRAME_RATE, g_frames_per_game_frame);
    }
}

bool is_recording() {
    return g_frames_per_game_frame > 1 && aurora_host::is_initialized() && !gx_draw::is_building_display_list();
}

void record(std::function<void()> step) {
    Step recorded;
    recorded.action = std::move(step);
    g_steps.push_back(std::move(recorded));
}

void record_position_matrix(const std::array<float, 12>& matrix, uint32_t slot) {
    g_matrices_waiting_for_draw.push_back(g_steps.size());
    record_matrix(StepKind::POSITION_MATRIX, matrix.data(), matrix.size(), slot, 0);
}

void record_normal_matrix(const std::array<float, 12>& matrix, uint32_t slot) {
    g_matrices_waiting_for_draw.push_back(g_steps.size());
    record_matrix(StepKind::NORMAL_MATRIX, matrix.data(), matrix.size(), slot, 0);
}

void record_projection(const std::array<float, 16>& matrix, uint32_t type) {
    const uint64_t key = mix_key(mix_key(0x50524F4Aull, type), g_projections_this_frame++);
    record_matrix(StepKind::PROJECTION, matrix.data(), matrix.size(), type, key);
}

void mark_draw(uint32_t display_list_address, uint32_t size) {
    if (g_matrices_waiting_for_draw.empty()) {
        return;
    }
    const uint64_t draw = mix_key(display_list_address, size);
    const uint32_t occurrence = g_draw_occurrences[draw]++;
    for (const size_t index : g_matrices_waiting_for_draw) {
        Step& step = g_steps[index];
        step.key = mix_key(mix_key(mix_key(draw, occurrence), static_cast<uint64_t>(step.kind)), step.slot_or_type);
    }
    g_matrices_waiting_for_draw.clear();
}

void finish_frame(const std::function<void()>& present) {
    const Clock::time_point start = Clock::now();
    const Clock::duration since_last = start - g_last_frame_end;
    g_last_frame_end = start;
    match_matrices();

    if (since_last > SLOWEST_GAME_FRAME) {
        draw(1.f);
        present();
        start_next_frame();
        return;
    }
    // The time of a game frame, smoothed over a few frames.
    g_game_frame_time += (since_last - g_game_frame_time) / 8;
    for (unsigned frame = 1; frame <= g_frames_per_game_frame; ++frame) {
        if (frame > 1) {
            std::this_thread::sleep_until(start + g_game_frame_time * (frame - 1) / g_frames_per_game_frame);
        }
        draw(static_cast<float>(frame) / static_cast<float>(g_frames_per_game_frame));
        present();
    }
    start_next_frame();
}

}  // namespace frame_interpolation
