// See game_start.h.

#include "game_start.h"

#include <SDL3/SDL_stdinc.h>
#include <nlohmann/json.hpp>

#include <algorithm>
#include <fstream>
#include <system_error>
#include <utility>

namespace {

bool file_exists(const std::filesystem::path& path) {
    std::error_code error;
    return !path.empty() && std::filesystem::is_regular_file(path, error);
}

}  // namespace

PreparedGame PreparedGame::read(const paths::Player& player) {
    PreparedGame prepared;
    std::ifstream stream(player.game / "prepared.json", std::ios::binary);
    const nlohmann::json document = nlohmann::json::parse(stream, nullptr, /*allow_exceptions=*/false);
    if (!document.is_object()) {
        return prepared;
    }
    prepared.exists = true;
    prepared.game = document.value("game", "");
    prepared.disc = document.value("disc", "");
    prepared.source_disc = document.value("source_disc", "");
    prepared.program = document.value("program", "");
    prepared.package_version = document.value("package_version", "");
    return prepared;
}

bool PreparedGame::is_current(const paths::Player& player) const {
    return exists && package_version == HMMM_PACKAGE_VERSION && !program.empty() &&
           file_exists(player.game / paths::from_utf8(program)) && file_exists(paths::from_utf8(disc));
}

std::vector<std::string> kit_mods(const paths::Package& package) {
    std::ifstream stream(package.kit / "recipe.json", std::ios::binary);
    const nlohmann::json recipe = nlohmann::json::parse(stream, nullptr, /*allow_exceptions=*/false);
    std::vector<std::string> ids;
    if (recipe.is_object() && recipe.contains("mods") && recipe["mods"].is_array()) {
        for (const auto& id : recipe["mods"]) {
            if (id.is_string()) {
                ids.push_back(id.get<std::string>());
            }
        }
    }
    return ids;
}

ChildProcess::Options game_start_options(const paths::Package& package, const paths::Player& player,
                                         const settings::Settings& settings, const std::vector<mods::Mod>& mods,
                                         const PreparedGame& prepared) {
    std::vector<std::string> enabled;
    std::vector<std::pair<std::string, std::string>> environment;
    for (const mods::Mod& mod : mods) {
        if (settings.mod_enabled(mod.id)) {
            enabled.push_back(mod.id);
        }
        for (const mods::Setting& setting : mod.settings) {
            if (!setting.environment.empty()) {
                const int value =
                    settings.number(setting.key.c_str(), setting.fallback, setting.minimum, setting.maximum);
                environment.emplace_back(setting.environment, std::to_string(value));
            }
        }
    }
    // The cheats mod is not in the Mods tab: it goes with the cheats that are on.
    if (!settings.cheats_on().empty() && std::ranges::find(enabled, settings::CHEATS_MOD) == enabled.end()) {
        enabled.emplace_back(settings::CHEATS_MOD);
    }
    std::string mod_list;
    for (const std::string& id : enabled) {
        mod_list += (mod_list.empty() ? "" : ",") + id;
    }
    // SDL writes numbers with a point whatever the computer's language: the game reads "1.5".
    char resolution[16];
    SDL_snprintf(resolution, sizeof(resolution), "%g",
                 settings::resolution_scale(settings.one_of(settings::RESOLUTION, settings::RESOLUTIONS)));

    ChildProcess::Options options;
    options.arguments = {
        paths::utf8(player.game / paths::from_utf8(prepared.program)),
        "--disc", prepared.disc,
        "--symbols", paths::utf8(package.symbols),
        "--settings", paths::utf8(player.settings),
        "--mods", mod_list.empty() ? "none" : mod_list,
        "--resolution-scale", resolution,
        "--fullscreen", settings.flag(settings::FULLSCREEN, false) ? "1" : "0",
        "--widescreen", settings.flag(settings::WIDESCREEN, false) ? "1" : "0",
        "--frame-rate", std::to_string(settings.one_of(settings::FRAME_RATE, settings::FRAME_RATES)),
        "--volume", std::to_string(settings.number(settings::VOLUME, 100, 0, 100)),
    };
    options.environment = std::move(environment);
    options.working_directory = paths::utf8(player.game);
    options.output_file = paths::utf8(player.game_log);
    return options;
}
