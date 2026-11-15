// See headless.h.

#include "headless.h"

#include "game_start.h"
#include "mods.h"
#include "paths.h"
#include "preparation.h"
#include "settings.h"
#include "texts.h"

#include <SDL3/SDL_timer.h>

#include <cmath>
#include <cstdio>
#include <iterator>
#include <memory>
#include <optional>

namespace headless {

namespace {

using texts::Text;
using texts::text;

// How often the progress is looked at, and the least change of it that is printed.
constexpr Uint32 LOOK_EVERY_MILLISECONDS = 200;
constexpr float SMALLEST_CHANGE = 0.005f;

// The texts in the player's language, as the window shows them.
void use_the_players_language(const settings::Settings& settings) {
    const std::string chosen = settings::GAME_LANGUAGES[settings.language()];
    texts::set_language(chosen == "es" ? texts::Language::Spanish : texts::Language::English);
}

std::string stage_text(int stage) {
    constexpr Text STAGES[] = {Text::StageDisc,    Text::StageTranslate, Text::StageAdapt,
                               Text::StageCompile, Text::StageLink,      Text::StageFinish};
    return stage >= 0 && stage < static_cast<int>(std::size(STAGES)) ? text(STAGES[stage]) : text(Text::PreparingRow);
}

}  // namespace

int prepare(const std::string& disc) {
    const paths::Package package = paths::find_package();
    const paths::Player player = paths::find_player();
    settings::Settings settings = settings::Settings::load(player.settings);
    use_the_players_language(settings);
    if (const std::vector<std::string> missing = package.missing(); !missing.empty()) {
        std::printf("error setup the package is missing %s\n", missing.front().c_str());
        return PREPARE_FAILED;
    }
    // The window remembers the disc the player chose: so does this.
    settings.set(settings::DISC_IMAGE, disc);
    settings.save(player.settings);

    Preparation preparation;
    if (!preparation.start(package, player, disc, HMMM_PACKAGE_VERSION)) {
        std::printf("error %s %s\n", preparation.error_reason().c_str(), preparation.error_text().c_str());
        return PREPARE_FAILED;
    }
    float said = -1.f;
    int said_stage = -2;
    while (preparation.state() == Preparation::State::Running) {
        SDL_Delay(LOOK_EVERY_MILLISECONDS);
        preparation.update();
        const float overall = preparation.overall();
        if (preparation.stage() != said_stage || std::fabs(overall - said) >= SMALLEST_CHANGE) {
            said = overall;
            said_stage = preparation.stage();
            std::printf("progress %.3f %s\n", static_cast<double>(overall), stage_text(said_stage).c_str());
            std::fflush(stdout);
        }
    }
    if (preparation.state() != Preparation::State::Succeeded) {
        std::printf("error %s %s\n", preparation.error_reason().c_str(), preparation.error_text().c_str());
        return PREPARE_FAILED;
    }
    std::printf("progress 1 %s\n", text(Text::Prepared));
    return 0;
}

int play() {
    const paths::Package package = paths::find_package();
    const paths::Player player = paths::find_player();
    const settings::Settings settings = settings::Settings::load(player.settings);
    use_the_players_language(settings);
    const PreparedGame prepared = PreparedGame::read(player);
    if (!prepared.is_current(player)) {
        std::printf("the game has not been prepared by this package (%s)\n", HMMM_PACKAGE_VERSION);
        return NOT_PREPARED;
    }
    const std::vector<mods::Mod> mods = mods::load(package.mods, kit_mods(package));
    std::string error;
    const std::unique_ptr<ChildProcess> game =
        ChildProcess::start(game_start_options(package, player, settings, mods, prepared), error);
    if (game == nullptr) {
        std::printf("the game could not be started: %s\n", error.c_str());
        return NOT_STARTED;
    }
    std::optional<int> code = game->exit_code();
    while (!code) {
        SDL_Delay(LOOK_EVERY_MILLISECONDS);
        code = game->exit_code();
    }
    return *code;
}

}  // namespace headless
