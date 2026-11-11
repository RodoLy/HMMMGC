// The mods the game is prepared with, as their manifests describe them (mods/<id>/mod.json).
//
// Los mods con los que se prepara el juego, como los describen sus manifiestos.
//
// A manifest gives a mod's name, a short line and a description in each language, its version
// and authors, an optional warning shown when it is turned off, and its settings: a percentage
// that the launcher keeps in settings.json and passes to the game in an environment variable
// (the mod reads it when the game starts).
#pragma once

#include <filesystem>
#include <string>
#include <vector>

namespace mods {

struct Setting {
    std::string key;          // its name in settings.json
    std::string environment;  // the variable the mod reads
    int minimum = 0;
    int maximum = 100;
    int fallback = 100;
    int step = 5;
    std::string label;        // in the launcher's language
};

struct Mod {
    std::string id;
    std::string version;
    std::string authors;      // joined with commas
    std::string name;         // these four in the launcher's language
    std::string summary;
    std::string description;
    std::string warning;      // shown when it is on, to read before turning it off; may be empty
    std::vector<Setting> settings;
};

// The manifests of `ids` under `folder` (builder/mods/), in the language the launcher uses
// now. A mod without a readable manifest is listed by its id alone.
std::vector<Mod> load(const std::filesystem::path& folder, const std::vector<std::string>& ids);

}  // namespace mods
