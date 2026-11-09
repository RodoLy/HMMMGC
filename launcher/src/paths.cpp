// See paths.h.

#include "paths.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_misc.h>
#include <SDL3/SDL_stdinc.h>

#include <system_error>

namespace paths {
namespace {

constexpr const char* PROJECT_FOLDER = "HarvestMoonMM";  // the same as the runtime's and ModernGekko's

bool path_exists(const std::filesystem::path& path) {
    std::error_code error;
    return !path.empty() && std::filesystem::exists(path, error);
}

// The first of `candidates` that exists, or empty.
std::filesystem::path first_existing(std::initializer_list<std::filesystem::path> candidates) {
    for (const auto& candidate : candidates) {
        if (path_exists(candidate)) {
            return candidate;
        }
    }
    return {};
}

std::filesystem::path application_data_folder() {
#if defined(_WIN32)
    if (const char* local_app_data = SDL_getenv("LOCALAPPDATA"); local_app_data != nullptr) {
        return from_utf8(local_app_data);
    }
#elif defined(__APPLE__)
    if (const char* home = SDL_getenv("HOME"); home != nullptr) {
        return from_utf8(home) / "Library" / "Application Support";
    }
#else
    if (const char* data_home = SDL_getenv("XDG_DATA_HOME"); data_home != nullptr && *data_home != '\0') {
        return from_utf8(data_home);
    }
    if (const char* home = SDL_getenv("HOME"); home != nullptr) {
        return from_utf8(home) / ".local" / "share";
    }
#endif
    return std::filesystem::current_path();
}

}  // namespace

std::string utf8(const std::filesystem::path& path) {
    const std::u8string text = path.u8string();
    return std::string(text.begin(), text.end());
}

std::filesystem::path from_utf8(const std::string& text) {
    return std::filesystem::path(std::u8string(text.begin(), text.end()));
}

std::vector<std::string> Package::missing() const {
    std::vector<std::string> names;
    if (python.empty()) {
        names.emplace_back("python");
    }
    const std::pair<const char*, const std::filesystem::path*> required[] = {
        {"tools/prepare_game.py", &prepare_script},
        {"kit", &kit},
        {"translator", &translator},
        {"config/G4AEE9/symbols.txt", &symbols},
    };
    for (const auto& [name, path] : required) {
        if (!path_exists(*path)) {
            names.emplace_back(name);
        }
    }
    if (!path_exists(kit / "recipe.json")) {
        names.emplace_back("kit/recipe.json");
    }
    return names;
}

Package find_package() {
    Package package;
    const char* base = SDL_GetBasePath();  // the launcher's folder, with a separator at the end
    package.root = base != nullptr ? from_utf8(base) : std::filesystem::current_path();
    package.builder = package.root / "builder";
    const auto& builder = package.builder;
    package.python = first_existing({builder / "python" / "python.exe", builder / "python" / "python",
                                     builder / "python" / "bin" / "python3"});
#if !defined(_WIN32)
    // A developer's copy on Linux may use the system's Python.
    if (package.python.empty()) {
        package.python = first_existing({"/usr/bin/python3", "/usr/local/bin/python3"});
    }
#endif
    package.prepare_script = builder / "tools" / "prepare_game.py";
    package.kit = builder / "kit";
    package.translator = first_existing({builder / "translator" / "Translator.Cli.exe",
                                         builder / "translator" / "Translator.Cli.dll"});
    package.toolchain = first_existing({builder / "llvm-mingw"});
    package.nodtool = first_existing({builder / "nodtool" / "nodtool.exe", builder / "nodtool" / "nodtool"});
    package.symbols = builder / "config" / "G4AEE9" / "symbols.txt";
    package.mods = builder / "mods";
    return package;
}

Player find_player() {
    Player player;
    player.root = application_data_folder() / PROJECT_FOLDER;
    player.native = player.root / "Native";
    player.game = player.native / "Game";
    player.settings = player.root / "settings.json";
    player.prepare_log = player.native / "prepare.log";
    player.game_log = player.native / "native.log";
    std::error_code error;
    std::filesystem::create_directories(player.native, error);
    return player;
}

void open_in_system(const std::filesystem::path& path) {
    // A file URL: forward slashes, and the few characters that a URL gives a meaning escaped.
    std::string url = "file://";
#if defined(_WIN32)
    url += "/";  // file:///C:/...
#endif
    const std::u8string generic = path.generic_u8string();
    for (const char8_t letter : generic) {
        switch (letter) {
        case u8' ': url += "%20"; break;
        case u8'#': url += "%23"; break;
        case u8'%': url += "%25"; break;
        case u8'?': url += "%3F"; break;
        default: url += static_cast<char>(letter); break;
        }
    }
    SDL_OpenURL(url.c_str());
}

}  // namespace paths
