// See preparation.h.

#include "preparation.h"

#include <SDL3/SDL_cpuinfo.h>

#include <algorithm>
#include <cstdlib>
#include <numeric>

namespace {

// How long each step usually takes, as a share of the whole (measured on Linux with the game's
// RVZ: the compile is most of it).
constexpr std::array<float, Preparation::STAGES.size()> STAGE_WEIGHTS = {0.04f, 0.16f, 0.04f, 0.68f, 0.06f, 0.02f};
constexpr size_t KEPT_LINES = 400;

}  // namespace

Preparation::~Preparation() {
    if (process_ != nullptr && state_ == State::Running) {
        process_->kill();
    }
    if (reader_.joinable()) {
        reader_.join();
    }
}

bool Preparation::start(const paths::Package& package, const paths::Player& player, const std::string& disc,
                        const std::string& package_version) {
    if (reader_.joinable()) {
        reader_.join();
    }
    state_ = State::Running;
    stage_ = -1;
    done_ = total_ = 0;
    error_reason_.clear();
    error_text_.clear();
    lines_.clear();
    pending_.clear();
    partial_line_.clear();
    output_closed_ = false;
    started_ = std::chrono::steady_clock::now();
    log_ = std::ofstream(player.prepare_log, std::ios::binary | std::ios::trunc);

    ChildProcess::Options options;
    options.arguments = {
        paths::utf8(package.python),
        paths::utf8(package.prepare_script),
        "--disc",
        disc,
        "--kit",
        paths::utf8(package.kit),
        "--translator",
        paths::utf8(package.translator),
        "--out",
        paths::utf8(player.game),
        "--package-version",
        package_version,
        "--jobs",
        std::to_string(std::max(1, SDL_GetNumLogicalCPUCores())),
    };
    if (!package.toolchain.empty()) {
        options.arguments.insert(options.arguments.end(), {"--toolchain", paths::utf8(package.toolchain)});
    }
    if (!package.nodtool.empty()) {
        options.arguments.insert(options.arguments.end(), {"--nodtool", paths::utf8(package.nodtool)});
    }
    // UTF-8 whatever the language of Windows, and no bytecode written into the package.
    options.environment = {{"PYTHONUTF8", "1"}, {"PYTHONIOENCODING", "utf-8"}, {"PYTHONDONTWRITEBYTECODE", "1"}};
    options.working_directory = paths::utf8(package.builder);
    options.end_with_launcher = true;

    std::string error;
    process_ = ChildProcess::start(options, error);
    if (process_ == nullptr) {
        error_reason_ = "not_started";
        error_text_ = error;
        finish(State::Failed);
        return false;
    }
    reader_ = std::thread(&Preparation::read_output, this);
    return true;
}

void Preparation::read_output() {
    for (std::string chunk = process_->read_output(); !chunk.empty(); chunk = process_->read_output()) {
        const std::lock_guard lock(mutex_);
        partial_line_ += chunk;
        size_t end;
        while ((end = partial_line_.find('\n')) != std::string::npos) {
            std::string line = partial_line_.substr(0, end);
            if (!line.empty() && line.back() == '\r') {
                line.pop_back();
            }
            pending_.push_back(std::move(line));
            partial_line_.erase(0, end + 1);
        }
    }
    const std::lock_guard lock(mutex_);
    if (!partial_line_.empty()) {
        pending_.push_back(std::move(partial_line_));
        partial_line_.clear();
    }
    output_closed_ = true;
}

void Preparation::take_line(const std::string& line) {
    if (log_) {
        log_ << line << '\n';
    }
    if (line.starts_with("@stage ")) {
        const std::string name = line.substr(7, line.find(' ', 7) - 7);
        const auto found = std::ranges::find_if(STAGES, [&](const char* stage) { return name == stage; });
        if (found != STAGES.end()) {
            stage_ = static_cast<int>(found - STAGES.begin());
            done_ = total_ = 0;
        }
    } else if (line.starts_with("@progress ")) {
        done_ = std::atoi(line.c_str() + 10);
        const size_t space = line.find(' ', 10);
        total_ = space == std::string::npos ? 0 : std::atoi(line.c_str() + space + 1);
    } else if (line.starts_with("@error ")) {
        const size_t space = line.find(' ', 7);
        error_reason_ = line.substr(7, space == std::string::npos ? std::string::npos : space - 7);
        error_text_ = space == std::string::npos ? "" : line.substr(space + 1);
    }
    // The details show the tools' lines and the errors; the steps and the progress show themselves.
    if (!line.starts_with("@stage ") && !line.starts_with("@progress ") && !line.starts_with("@done ")) {
        lines_.push_back(line);
        while (lines_.size() > KEPT_LINES) {
            lines_.pop_front();
        }
    }
}

void Preparation::update() {
    if (state_ != State::Running) {
        return;
    }
    std::vector<std::string> lines;
    {
        const std::lock_guard lock(mutex_);
        lines.swap(pending_);
    }
    for (const std::string& line : lines) {
        take_line(line);
    }
    if (!output_closed_) {
        return;
    }
    const std::optional<int> code = process_->exit_code();
    if (!code) {
        return;
    }
    if (*code == 0 && error_reason_.empty()) {
        finish(State::Succeeded);
    } else {
        if (error_reason_.empty()) {
            error_reason_ = "tool_failed";
        }
        finish(State::Failed);
    }
}

void Preparation::cancel() {
    if (state_ == State::Running && process_ != nullptr) {
        process_->kill();
        if (reader_.joinable()) {
            reader_.join();
        }
        finish(State::Cancelled);
    }
}

void Preparation::finish(State state) {
    state_ = state;
    ended_ = std::chrono::steady_clock::now();
    if (log_) {
        log_.flush();
    }
}

float Preparation::overall() const {
    if (state_ == State::Succeeded) {
        return 1.f;
    }
    if (stage_ < 0) {
        return 0.f;
    }
    const float before = std::accumulate(STAGE_WEIGHTS.begin(), STAGE_WEIGHTS.begin() + stage_, 0.f);
    const float within = total_ > 0 ? std::clamp(static_cast<float>(done_) / static_cast<float>(total_), 0.f, 1.f) : 0.f;
    return before + STAGE_WEIGHTS[static_cast<size_t>(stage_)] * within;
}

double Preparation::seconds() const {
    const auto end = state_ == State::Running ? std::chrono::steady_clock::now() : ended_;
    return std::chrono::duration<double>(end - started_).count();
}
