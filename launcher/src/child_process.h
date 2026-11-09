// A program the launcher starts: the preparation (tools/prepare_game.py) or the game.
//
// Un programa que arranca el launcher: la preparación o el juego.
//
// On Windows the programs are console programs; they are started without a console window
// (CREATE_NO_WINDOW), and the preparation in a job that ends everything it started when it is
// cancelled or when the launcher closes. Elsewhere they are started with SDL's processes.
#pragma once

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

class ChildProcess {
public:
    struct Options {
        std::vector<std::string> arguments;  // UTF-8; the first is the program
        std::vector<std::pair<std::string, std::string>> environment;  // added to the launcher's own
        std::string working_directory;       // UTF-8; empty: the launcher's
        // Where its output (standard output and error together) goes: a file (UTF-8 path), or,
        // when empty, a pipe that read_output reads.
        std::string output_file;
        // On Windows, whether closing the launcher or kill() also ends what it started.
        bool end_with_launcher = false;
    };

    // Starts it; nullptr and `error` when it could not.
    static std::unique_ptr<ChildProcess> start(const Options& options, std::string& error);
    virtual ~ChildProcess() = default;

    // Waits for more of its output and returns it; empty once it has closed its output.
    // Only for a process started without an output file. Blocks: call it from a thread.
    virtual std::string read_output() = 0;
    // Its exit code once it has ended, without waiting.
    virtual std::optional<int> exit_code() = 0;
    // Ends it (and, with end_with_launcher, what it started).
    virtual void kill() = 0;
};
