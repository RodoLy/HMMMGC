// See player_folder.h.

#include "player_folder.h"

#include "runtime_log.h"

#include <cstdlib>

namespace player_folder {
namespace {

constexpr const char* PROJECT_FOLDER = "HarvestMoonMM";  // the same as the version on ModernGekko
constexpr const char* RUNTIME_FOLDER = "Native";
constexpr const char* SETTINGS_FILE = "settings.json";  // the launcher's (shared/settings.h)

std::filesystem::path g_path;
std::filesystem::path g_settings_file;

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
    if (requested.empty()) {
        const std::filesystem::path project = application_data_folder() / PROJECT_FOLDER;
        g_path = project / RUNTIME_FOLDER;
        g_settings_file = project / SETTINGS_FILE;
    } else {
        g_path = std::filesystem::path(requested);
        g_settings_file = g_path / SETTINGS_FILE;
    }
    std::filesystem::create_directories(g_path);
    runtime_log::info("player", "player folder: %s", g_path.string().c_str());
}

const std::filesystem::path& path() { return g_path; }

const std::filesystem::path& settings_file() { return g_settings_file; }

}  // namespace player_folder
