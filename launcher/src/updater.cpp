// See updater.h.

#include "updater.h"

#include "child_process.h"

#include <nlohmann/json.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <optional>
#include <string>
#include <system_error>
#include <vector>

namespace {

// Where the published versions are (GitHub's API for the repository's releases).
constexpr const char* LATEST_RELEASE = "https://api.github.com/repos/RodoLy/HMMMGC/releases/latest";
constexpr const char* USER_AGENT = "HarvestMoonMM-launcher";
// How long the check may take before it gives up without a word, and how long a download may stall.
constexpr const char* CHECK_SECONDS = "8";
constexpr const char* STALLED_SECONDS = "60";
// What an old file is renamed to while the new one takes its place (deleted at the next start).
constexpr const char* OLD_SUFFIX = ".old-update";

// The package of this system among the release's files: HarvestMoonMM-native-<version>-<this>.
#if defined(_WIN32)
#if defined(_M_ARM64) || defined(__aarch64__)
constexpr const char* THIS_SYSTEM = "windows-arm64.zip";
#else
constexpr const char* THIS_SYSTEM = "windows-x64.zip";
#endif
#elif defined(__APPLE__)
#if defined(__aarch64__)
constexpr const char* THIS_SYSTEM = "macos-arm64.zip";
#else
constexpr const char* THIS_SYSTEM = "macos-x64.zip";
#endif
#else
#if defined(__aarch64__)
constexpr const char* THIS_SYSTEM = "linux-arm64.tar.gz";
#else
constexpr const char* THIS_SYSTEM = "linux-x64.tar.gz";
#endif
#endif

// The system's own program: on Windows from its System32 folder, so that no other copy is taken
// by mistake; elsewhere from the PATH.
std::string system_program(const char* name) {
#if defined(_WIN32)
    const char* root = std::getenv("SystemRoot");
    return std::string(root != nullptr ? root : "C:\\Windows") + "\\System32\\" + name + ".exe";
#else
    return name;
#endif
}

// Runs a program to its end, polling, and gives its exit code (nothing when it could not start).
// While it runs, `while_running` is called a few times a second.
template <typename Callback>
std::optional<int> run(const std::vector<std::string>& arguments, const std::filesystem::path& output_file,
                       Callback while_running) {
    ChildProcess::Options options;
    options.arguments = arguments;
    options.output_file = paths::utf8(output_file);  // a file: a pipe nobody reads could fill up
    std::string error;
    std::unique_ptr<ChildProcess> process = ChildProcess::start(options, error);
    if (process == nullptr) {
        return std::nullopt;
    }
    for (;;) {
        if (const std::optional<int> code = process->exit_code()) {
            return code;
        }
        while_running();
        std::this_thread::sleep_for(std::chrono::milliseconds(200));
    }
}

std::optional<int> run(const std::vector<std::string>& arguments, const std::filesystem::path& output_file) {
    return run(arguments, output_file, [] {});
}

// "v0.4.2" as {0, 4, 2}; nothing when it is not a published version (a developer's "dev").
std::optional<std::vector<int>> version_numbers(const std::string& version) {
    if (version.size() < 2 || version[0] != 'v') {
        return std::nullopt;
    }
    std::vector<int> numbers;
    std::string number;
    for (size_t i = 1; i <= version.size(); ++i) {
        if (i == version.size() || version[i] == '.') {
            if (number.empty()) {
                return std::nullopt;
            }
            numbers.push_back(std::atoi(number.c_str()));
            number.clear();
        } else if (version[i] >= '0' && version[i] <= '9') {
            number += version[i];
        } else {
            return std::nullopt;
        }
    }
    return numbers;
}

// The folder the package is in, as it was unpacked: on a Mac the one that holds HarvestMoonMM.app
// (the launcher's folder is HarvestMoonMM.app/Contents/Resources), elsewhere the launcher's own.
std::filesystem::path package_folder(const paths::Package& package) {
#if defined(__APPLE__)
    return package.root.parent_path().parent_path().parent_path();
#else
    return package.root;
#endif
}

// Puts every file of `from` over `to`: the old one renamed out of the way, the new one copied in.
// Links (the Linux package keeps some) are made again as links.
void put_over(const std::filesystem::path& from, const std::filesystem::path& to) {
    namespace fs = std::filesystem;
    for (auto entry = fs::recursive_directory_iterator(from); entry != fs::recursive_directory_iterator(); ++entry) {
        const fs::path target = to / fs::relative(entry->path(), from);
        if (entry->is_directory() && !entry->is_symlink()) {
            fs::create_directories(target);
            continue;
        }
        std::error_code ignored;
        if (fs::exists(fs::symlink_status(target))) {
            fs::path old = target;
            old += OLD_SUFFIX;
            fs::remove_all(old, ignored);
            fs::rename(target, old);
        }
        if (entry->is_symlink()) {
            fs::copy_symlink(entry->path(), target);
        } else {
            fs::copy_file(entry->path(), target);
            fs::permissions(target, entry->status().permissions());
        }
    }
}

// A line of the notes without what Markdown marks: **bold**, `code`, and [text](link) as its text.
std::string plain(std::string line) {
    for (const char* mark : {"**", "__", "`"}) {
        for (size_t at = line.find(mark); at != std::string::npos; at = line.find(mark)) {
            line.erase(at, std::char_traits<char>::length(mark));
        }
    }
    for (size_t open = line.find('['); open != std::string::npos; open = line.find('[', open + 1)) {
        const size_t middle = line.find("](", open);
        const size_t close = middle == std::string::npos ? middle : line.find(')', middle);
        if (close == std::string::npos) {
            break;
        }
        line = line.substr(0, open) + line.substr(open + 1, middle - open - 1) + line.substr(close + 1);
    }
    return line;
}

std::string trimmed(const std::string& line) {
    const size_t first = line.find_first_not_of(" \t\r");
    const size_t last = line.find_last_not_of(" \t\r");
    return first == std::string::npos ? std::string() : line.substr(first, last - first + 1);
}

}  // namespace

Updater::~Updater() {
    if (thread_.joinable()) {
        thread_.detach();  // a check or a download that is still going ends with the launcher
    }
}

void Updater::check(const std::string& current) {
    if (state_ != State::Idle || !version_numbers(current)) {
        return;
    }
    state_ = State::Checking;
    thread_ = std::thread(&Updater::check_on_thread, this, current);
}

void Updater::check_on_thread(std::string current) {
    // GitHub's answer goes to a file beside nothing of the player's: the system's temporary folder.
    std::error_code ignored;
    const std::filesystem::path answer = std::filesystem::temp_directory_path(ignored) / "HarvestMoonMM-latest.json";
    const std::optional<int> code =
        run({system_program("curl"), "--silent", "--fail", "--location", "--max-time", CHECK_SECONDS, "--user-agent",
             USER_AGENT, "--header", "Accept: application/vnd.github+json", "--output", paths::utf8(answer),
             LATEST_RELEASE},
            answer.parent_path() / "HarvestMoonMM-latest.log");
    if (code != 0) {
        state_ = State::UpToDate;  // no connection, or GitHub did not answer: nothing to offer
        return;
    }
    nlohmann::json release = nlohmann::json::parse(std::ifstream(answer), nullptr, false);
    std::filesystem::remove(answer, ignored);
    std::filesystem::remove(answer.parent_path() / "HarvestMoonMM-latest.log", ignored);
    if (!release.is_object() || !release["tag_name"].is_string()) {
        state_ = State::UpToDate;
        return;
    }
    const std::string tag = release["tag_name"];
    const auto newest = version_numbers(tag);
    if (!newest || *newest <= *version_numbers(current)) {
        state_ = State::UpToDate;
        return;
    }
    const std::string wanted = "HarvestMoonMM-native-" + tag + "-" + THIS_SYSTEM;
    for (const nlohmann::json& asset : release.value("assets", nlohmann::json::array())) {
        if (asset.value("name", "") == wanted && asset["browser_download_url"].is_string()) {
            std::lock_guard lock(mutex_);
            latest_ = tag;
            url_ = asset["browser_download_url"];
            asset_ = wanted;
            size_ = asset.value("size", std::uint64_t{0});
            body_ = release["body"].is_string() ? release["body"].get<std::string>() : "";
            state_ = State::Available;
            return;
        }
    }
    state_ = State::UpToDate;  // the newest version has no package for this system (yet)
}

void Updater::install(const paths::Package& package, const std::filesystem::path& work) {
    if (state_ != State::Available && state_ != State::Failed) {
        return;
    }
    if (thread_.joinable()) {
        thread_.join();
    }
    state_ = State::Downloading;
    arrived_ = 0;
    thread_ = std::thread(&Updater::install_on_thread, this, package, work);
}

void Updater::install_on_thread(paths::Package package, std::filesystem::path work) {
    namespace fs = std::filesystem;
    std::string url;
    std::string asset;
    {
        std::lock_guard lock(mutex_);
        url = url_;
        asset = asset_;
    }
    std::error_code ignored;
    fs::remove_all(work, ignored);
    fs::create_directories(work / "unpacked", ignored);
    const fs::path download = work / asset;

    // The download, followed by the size of the file as it grows.
    const std::optional<int> downloaded =
        run({system_program("curl"), "--silent", "--fail", "--location", "--user-agent", USER_AGENT, "--speed-time",
             STALLED_SECONDS, "--speed-limit", "1", "--output", paths::utf8(download), url},
            work / "download.log", [&] {
                std::error_code error;
                const std::uintmax_t bytes = fs::file_size(download, error);
                if (!error) {
                    arrived_ = bytes;
                }
            });
    if (downloaded != 0) {
        fail(downloaded ? "the download did not finish" : "curl could not be started");
        return;
    }
    arrived_ = size_.load();

    state_ = State::Installing;
    const std::optional<int> unpacked =
        run({system_program("tar"), "-x", "-f", paths::utf8(download), "-C", paths::utf8(work / "unpacked")},
            work / "unpack.log");
    if (unpacked != 0) {
        fail(unpacked ? "the package could not be unpacked" : "tar could not be started");
        return;
    }
    // The package's own folder inside (HarvestMoonMM-<system>), whose files go over this one's.
    fs::path top;
    for (const fs::directory_entry& entry : fs::directory_iterator(work / "unpacked", ignored)) {
        if (entry.is_directory()) {
            top = entry.path();
        }
    }
    if (top.empty()) {
        fail("the package has no folder inside");
        return;
    }
    try {
        const fs::path folder = package_folder(package);
        put_over(top, folder);
        // An Android package unpacked inside the old one (builder/android) belongs to the old
        // version: it goes, and the new version's Android zip can be unpacked there again.
        if (!package.android.empty()) {
            fs::path old = package.android;
            old += OLD_SUFFIX;
            fs::remove_all(old, ignored);
            fs::rename(package.android, old, ignored);
        }
    } catch (const fs::filesystem_error& error) {
        fail(error.what());
        return;
    }
    fs::remove_all(work, ignored);
    state_ = State::Installed;
}

bool Updater::restart(const paths::Package& package) {
    ChildProcess::Options options;
#if defined(_WIN32)
    options.arguments = {paths::utf8(package.root / "HarvestMoonMM.exe")};
#elif defined(__APPLE__)
    options.arguments = {paths::utf8(package.root.parent_path() / "MacOS" / "HarvestMoonMM")};
#else
    options.arguments = {paths::utf8(package.root / "HarvestMoonMM")};
#endif
    std::string error;
    return ChildProcess::start(options, error) != nullptr;
}

void Updater::clean_up(const paths::Package& package, const std::filesystem::path& work) {
    namespace fs = std::filesystem;
    std::error_code ignored;
    fs::remove_all(work, ignored);
    const fs::path folder = package_folder(package);
    std::vector<fs::path> old;
    for (auto entry = fs::recursive_directory_iterator(folder, ignored); entry != fs::recursive_directory_iterator();
         entry.increment(ignored)) {
        const std::string name = paths::utf8(entry->path().filename());
        if (name.size() > std::char_traits<char>::length(OLD_SUFFIX) && name.ends_with(OLD_SUFFIX)) {
            old.push_back(entry->path());
            entry.disable_recursion_pending();
        }
    }
    for (const fs::path& path : old) {
        fs::remove_all(path, ignored);  // one still in use (Windows) goes the next time
    }
}

std::string Updater::latest() const {
    std::lock_guard lock(mutex_);
    return latest_;
}

float Updater::progress() const {
    const std::uint64_t total = size_;
    return total == 0 ? 0.f : std::min(1.f, static_cast<float>(arrived_) / static_cast<float>(total));
}

std::vector<std::string> Updater::notes(bool spanish) const {
    std::string body;
    {
        std::lock_guard lock(mutex_);
        body = body_;
    }
    // The lines of the section of this language: from its title to the next title. Without one,
    // the whole text (notes written in one language only).
    std::vector<std::string> all;
    for (size_t start = 0; start <= body.size();) {
        const size_t end = std::min(body.find('\n', start), body.size());
        all.push_back(trimmed(body.substr(start, end - start)));
        start = end + 1;
    }
    const std::string wanted = spanish ? "novedades" : "what's new";
    std::vector<std::string> section;
    bool inside = false;
    bool found = false;
    for (const std::string& line : all) {
        if (line.starts_with("#")) {
            std::string title = trimmed(line.substr(line.find_first_not_of('#')));
            std::ranges::transform(title, title.begin(), [](unsigned char c) { return std::tolower(c); });
            inside = title.starts_with(wanted);
            found = found || inside;
            continue;
        }
        if (inside) {
            section.push_back(line);
        }
    }
    if (!found) {
        section = all;
    }
    // The points of the list (their next lines joined to them) and the paragraphs. A paragraph
    // after the list is not of this language's section (the notes end with one in both).
    std::vector<std::string> lines;
    bool joining = false;
    bool listed = false;
    for (const std::string& line : section) {
        if (line.empty() || line == "---") {
            joining = false;
            continue;
        }
        const bool point = line.starts_with("- ") || line.starts_with("* ");
        if (listed && !joining && !point) {
            break;
        }
        listed = listed || point;
        if (point) {
            lines.push_back("\xE2\x80\xA2 " + plain(trimmed(line.substr(2))));  // •
            joining = true;
        } else if (joining && !lines.empty()) {
            lines.back() += " " + plain(line);
        } else {
            lines.push_back(plain(line));
            joining = true;
        }
    }
    return lines;
}

std::string Updater::error() const {
    std::lock_guard lock(mutex_);
    return error_;
}

void Updater::fail(const std::string& why) {
    {
        std::lock_guard lock(mutex_);
        error_ = why;
    }
    state_ = State::Failed;
}
