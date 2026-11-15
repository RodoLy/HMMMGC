// See disc_search.h.

#include "disc_search.h"

#include "paths.h"

#include <SDL3/SDL_filesystem.h>
#include <SDL3/SDL_stdinc.h>

#include <algorithm>
#include <array>
#include <cctype>
#include <fstream>
#include <iterator>
#include <set>
#include <system_error>

namespace {

// Where the disc's own header is in each kind of image: at the start of an .iso or a .gcm; in an
// .rvz or a .wia, the copy of its first 0x80 bytes in the image's header (after the file's head of
// 0x48 bytes and four numbers of its disc part); in a .ciso, after the header and the map of blocks.
constexpr std::streamoff WIA_DISC_HEADER = 0x58;
constexpr std::streamoff CISO_DATA = 0x8000;
// Every GameCube disc has this number at 0x1C of its header.
constexpr std::array<unsigned char, 4> GAMECUBE_MAGIC = {0xC2, 0x33, 0x9F, 0x3D};
constexpr std::streamoff GAMECUBE_MAGIC_OFFSET = 0x1C;
constexpr size_t CODE_LETTERS = 6;

// The images it looks at: those that keep the header readable (not .gcz).
constexpr const char* SEARCHED_EXTENSIONS[] = {".iso", ".gcm", ".rvz", ".wia", ".ciso"};
// A disc image is far bigger than this; anything smaller is not opened.
constexpr std::uintmax_t SMALLEST_IMAGE = 16u * 1024u * 1024u;
// How deep it goes in each place, and how many entries it looks at in all, so that a big folder
// does not keep it busy.
constexpr int DEEPEST = 3;
constexpr int MOST_ENTRIES = 60000;

bool read_at(std::ifstream& file, std::streamoff offset, unsigned char* bytes, std::streamsize count) {
    file.seekg(offset);
    return static_cast<bool>(file.read(reinterpret_cast<char*>(bytes), count));
}

std::string lower(std::string text) {
    std::ranges::transform(text, text.begin(), [](unsigned char c) { return static_cast<char>(std::tolower(c)); });
    return text;
}

// The folders of Dolphin's game list, from its Dolphin.ini (ISOPath0 = ..., ISOPath1 = ...).
std::vector<std::filesystem::path> dolphin_game_folders() {
    std::vector<std::filesystem::path> settings_files;
    if (const char* documents = SDL_GetUserFolder(SDL_FOLDER_DOCUMENTS)) {
        settings_files.push_back(paths::from_utf8(documents) / "Dolphin Emulator" / "Config" / "Dolphin.ini");
    }
    if (const char* roaming = SDL_getenv("APPDATA")) {
        settings_files.push_back(paths::from_utf8(roaming) / "Dolphin Emulator" / "Config" / "Dolphin.ini");
    }
    if (const char* home = SDL_GetUserFolder(SDL_FOLDER_HOME)) {
        settings_files.push_back(paths::from_utf8(home) / ".config" / "dolphin-emu" / "Dolphin.ini");
    }
    std::vector<std::filesystem::path> folders;
    for (const std::filesystem::path& file : settings_files) {
        std::ifstream stream(file);
        for (std::string line; std::getline(stream, line);) {
            if (!line.starts_with("ISOPath") || line.starts_with("ISOPaths")) {
                continue;
            }
            const size_t equals = line.find('=');
            if (equals == std::string::npos) {
                continue;
            }
            std::string folder = line.substr(equals + 1);
            folder.erase(0, folder.find_first_not_of(" \t"));
            folder.erase(folder.find_last_not_of(" \t\r") + 1);
            if (!folder.empty()) {
                folders.push_back(paths::from_utf8(folder));
            }
        }
    }
    return folders;
}

}  // namespace

std::string game_code(const std::filesystem::path& disc_image) {
    std::ifstream file(disc_image, std::ios::binary);
    unsigned char head[4] = {};
    if (!read_at(file, 0, head, sizeof(head))) {
        return {};
    }
    const std::string magic(head, head + 4);
    std::streamoff header = 0;
    if (magic == std::string("RVZ\x01", 4) || magic == std::string("WIA\x01", 4)) {
        header = WIA_DISC_HEADER;
    } else if (magic == "CISO") {
        header = CISO_DATA;
    }
    unsigned char disc[0x20] = {};
    if (!read_at(file, header, disc, sizeof(disc))) {
        return {};
    }
    const bool gamecube = std::equal(GAMECUBE_MAGIC.begin(), GAMECUBE_MAGIC.end(), disc + GAMECUBE_MAGIC_OFFSET);
    std::string code(disc, disc + CODE_LETTERS);
    const bool readable =
        std::ranges::all_of(code, [](char c) { return (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9'); });
    return gamecube && readable ? code : "";
}

DiscSearch::~DiscSearch() {
    stop_ = true;
    if (thread_.joinable()) {
        thread_.join();
    }
}

void DiscSearch::start(const std::filesystem::path& launcher_folder, const std::string& code) {
    if (thread_.joinable()) {
        return;
    }
    std::vector<std::filesystem::path> places = {launcher_folder};
    for (const SDL_Folder folder : {SDL_FOLDER_DOWNLOADS, SDL_FOLDER_DOCUMENTS, SDL_FOLDER_DESKTOP}) {
        if (const char* path = SDL_GetUserFolder(folder)) {
            places.push_back(paths::from_utf8(path));
        }
    }
    if (const char* home = SDL_GetUserFolder(SDL_FOLDER_HOME)) {
        places.push_back(paths::from_utf8(home) / "Games");
        places.push_back(paths::from_utf8(home));
    }
    const std::vector<std::filesystem::path> dolphin = dolphin_game_folders();
    places.insert(places.end(), dolphin.begin(), dolphin.end());
    thread_ = std::thread(&DiscSearch::look, this, std::move(places), code);
}

std::vector<std::string> DiscSearch::found() const {
    const std::lock_guard lock(mutex_);
    return found_;
}

void DiscSearch::look(std::vector<std::filesystem::path> places, std::string code) {
    std::set<std::filesystem::path> seen;
    int entries = 0;
    for (const std::filesystem::path& place : places) {
        std::error_code error;
        auto walk = std::filesystem::recursive_directory_iterator(
            place, std::filesystem::directory_options::skip_permission_denied, error);
        for (; !error && walk != std::filesystem::recursive_directory_iterator(); walk.increment(error)) {
            if (stop_ || ++entries > MOST_ENTRIES) {
                finished_ = true;
                return;
            }
            const std::filesystem::directory_entry& entry = *walk;
            // What is asked of an entry has its own error: one that cannot be read is passed over.
            std::error_code asked;
            const std::string name = entry.path().filename().string();
            if (entry.is_directory(asked)) {
                // Hidden folders and those of programs hold no one's disc.
                if (walk.depth() >= DEEPEST - 1 || name.starts_with('.') || lower(name) == "appdata" ||
                    lower(name) == "node_modules") {
                    walk.disable_recursion_pending();
                }
                continue;
            }
            const std::string extension = lower(entry.path().extension().string());
            if (std::ranges::find(SEARCHED_EXTENSIONS, extension) == std::end(SEARCHED_EXTENSIONS)) {
                continue;
            }
            const std::uintmax_t size = entry.file_size(asked);
            if (asked || size < SMALLEST_IMAGE) {
                continue;
            }
            const std::filesystem::path canonical = std::filesystem::weakly_canonical(entry.path(), asked);
            if (asked || !seen.insert(canonical).second) {
                continue;
            }
            if (game_code(entry.path()) == code) {
                const std::lock_guard lock(mutex_);
                found_.push_back(paths::utf8(entry.path()));
            }
        }
    }
    finished_ = true;
}
