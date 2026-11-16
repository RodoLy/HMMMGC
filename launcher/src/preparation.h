// The preparation of the game (tools/prepare_game.py), or the making of the Android app
// (tools/make_android_app.py, which does the same steps and makes the app at the end), run by the
// launcher and followed through the lines they print (their protocol: @stage, @progress, @error,
// @done, and @disc for the app).
//
// La preparación del juego, o la app de Android, que el launcher corre y sigue por las líneas que
// imprimen.
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
    // The steps, in the order the script names them (make_android_app.py's last one is "app").
    static constexpr std::array<const char*, 6> STAGES = {"disc", "translate", "adapt", "compile", "link", "finish"};

    ~Preparation();

    // Starts preparing the game from `disc` (UTF-8). False, with error_text(), when it could not start.
    bool start(const paths::Package& package, const paths::Player& player, const std::string& disc,
               const std::string& package_version);
    // Starts making the Android app from `disc`, into the player's Android folder.
    bool start_android_app(const paths::Package& package, const paths::Player& player, const std::string& disc);
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
    // What it made (@done: the program, or the app), and for the app the disc image to copy to the
    // phone (@disc).
    const std::string& made() const { return made_; }
    const std::string& disc_to_copy() const { return disc_to_copy_; }

private:
    bool run(const std::vector<std::string>& arguments, const paths::Package& package,
             const std::filesystem::path& log);
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
    std::string made_;
    std::string disc_to_copy_;
    std::deque<std::string> lines_;
    std::chrono::steady_clock::time_point started_{};
    std::chrono::steady_clock::time_point ended_{};
};
