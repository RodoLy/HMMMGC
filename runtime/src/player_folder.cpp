// See player_folder.h.

#include "player_folder.h"

#include "runtime_log.h"

#include <cstdlib>

namespace player_folder {
namespace {

constexpr const char* PROJECT_FOLDER = "HarvestMoonMM";  // the same as the version on ModernGekko
constexpr const char* RUNTIME_FOLDER = "Native";

std::filesystem::path g_path;

// The platform's folder for application data that stays on this computer.
std::filesystem::path application_data_folder() {
#if defined(_WIN32)
    if (const char* local_app_data = std::getenv("LOCALAPPDATA"); local_app_data != nullptr) {
        return local_app_data;
    }
#elif defined(__APPLE__)
    if (const char* home = std::getenv("HOME"); home != nullptr) {
        return std::filesystem::path(home) / "Library" / "Application Support";
    }
#else
    if (const char* data_home = std::getenv("XDG_DATA_HOME"); data_home != nullptr && *data_home != '\0') {
        return data_home;
    }
    if (const char* home = std::getenv("HOME"); home != nullptr) {
        return std::filesystem::path(home) / ".local" / "share";
    }
#endif
    return std::filesystem::current_path();
}

}  // namespace

void choose(const std::string& requested) {
    g_path = requested.empty() ? application_data_folder() / PROJECT_FOLDER / RUNTIME_FOLDER
                               : std::filesystem::path(requested);
    std::filesystem::create_directories(g_path);
    runtime_log::info("player", "player folder: %s", g_path.string().c_str());
}

const std::filesystem::path& path() { return g_path; }

}  // namespace player_folder
