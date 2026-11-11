// The preparation of the game (tools/prepare_game.py), run by the launcher and followed through
// the lines it prints (its protocol: @stage, @progress, @error, @done).
//
// La preparación del juego, que el launcher corre y sigue por las líneas que imprime.
#pragma once

#include "child_process.h"
#include "paths.h"

#include <array>
#include <atomic>
#include <chrono>
#include <deque>
#include <fstream>
#include <memory>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

class Preparation {
public:
    enum class State { Idle, Running, Succeeded, Failed, Cancelled };
    // The steps, in the order the script names them.
    static constexpr std::array<const char*, 6> STAGES = {"disc", "translate", "adapt", "compile", "link", "finish"};

    ~Preparation();

    // Starts preparing the game from `disc` (UTF-8). False, with error_text(), when it could not start.
    bool start(const paths::Package& package, const paths::Player& player, const std::string& disc,
               const std::string& package_version);
    // Takes in what the script printed since the last call. Call it once a frame.
    void update();
    void cancel();

    State state() const { return state_; }
    // The step it is in (-1 before the first), and how far that step is (0 to 1).
    int stage() const { return stage_; }
    int stage_done() const { return done_; }
    int stage_total() const { return total_; }
    // How far the whole preparation is, weighing each step by how long it usually takes.
    float overall() const;
    double seconds() const;
    // The reason of the script's @error (not_a_disc, wrong_game, ...) and its text.
    const std::string& error_reason() const { return error_reason_; }
    const std::string& error_text() const { return error_text_; }
    // The last lines it printed, for the details.
    const std::deque<std::string>& lines() const { return lines_; }

private:
    void read_output();
    void take_line(const std::string& line);
    void finish(State state);

    std::unique_ptr<ChildProcess> process_;
    std::thread reader_;
    std::mutex mutex_;
    std::vector<std::string> pending_;  // lines read by the reader thread, taken by update()
    std::string partial_line_;
    std::atomic<bool> output_closed_{false};
    std::ofstream log_;

    State state_ = State::Idle;
    int stage_ = -1;
    int done_ = 0;
    int total_ = 0;
    std::string error_reason_;
    std::string error_text_;
    std::deque<std::string> lines_;
    std::chrono::steady_clock::time_point started_{};
    std::chrono::steady_clock::time_point ended_{};
};
