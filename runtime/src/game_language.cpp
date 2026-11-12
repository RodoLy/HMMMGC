// See game_language.h.

#include "game_language.h"

#include "guest_memory.h"
#include "runtime_log.h"

#include <cstring>
#include <string_view>

namespace game_language {
namespace {

// Where the program's data is searched: from the start of the program to past its end (the
// program of this game ends before 0x80400000).
constexpr uint32_t SEARCH_START = 0x80003100;
constexpr uint32_t SEARCH_END = 0x80400000;
// The paths of the English files: "/message_e/chara/Rion_e.bin". The letter of the language is in
// the folder and at the end of the name.
constexpr std::string_view ENGLISH_FOLDER = "/message_e/";
constexpr size_t FOLDER_LETTER = ENGLISH_FOLDER.size() - 2;
constexpr std::string_view ENGLISH_ENDING = "_e.bin";
constexpr size_t LONGEST_PATH = 96;

// The letter of the language's folder: message_s is Spanish, message_g German.
char folder_letter(const std::string& language) {
    if (language == "es") return 's';
    if (language == "fr") return 'f';
    if (language == "de") return 'g';
    if (language == "it") return 'i';
    return 'e';
}

}  // namespace

void apply(const GameDisc& disc, const std::string& language) {
    const char letter = folder_letter(language);
    if (letter == 'e') {
        return;
    }
    const size_t length = SEARCH_END - SEARCH_START;
    char* memory = reinterpret_cast<char*>(guest_memory::host_pointer(SEARCH_START, length));
    const std::string_view program(memory, length);
    unsigned changed = 0;
    unsigned kept = 0;
    for (size_t at = program.find(ENGLISH_FOLDER); at != std::string_view::npos;
         at = program.find(ENGLISH_FOLDER, at + 1)) {
        const size_t path_length = strnlen(memory + at, LONGEST_PATH);
        std::string path(memory + at, path_length);
        if (path_length == LONGEST_PATH || !path.ends_with(ENGLISH_ENDING)) {
            continue;
        }
        const size_t ending_letter = path_length - ENGLISH_ENDING.size() + 1;
        path[FOLDER_LETTER] = letter;
        path[ending_letter] = letter;
        if (!disc.has_file("files" + path)) {
            ++kept;
            continue;
        }
        memory[at + FOLDER_LETTER] = letter;
        memory[at + ending_letter] = letter;
        ++changed;
    }
    runtime_log::info("language", "the game's text is read from message_%c: %u files (%u stay in English)", letter,
                      changed, kept);
}

}  // namespace game_language
