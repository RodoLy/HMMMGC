// See player_options.h.

#include "player_options.h"

#include "runtime_log.h"

#include <algorithm>

namespace player_options {
namespace {

// The options as the game runs with them.
struct Options {
    unsigned game_speed_percent = 100;
    float resolution_scale = 0.f;
    bool fullscreen = false;
    bool widescreen = false;
    float zoom = 1.f;
    unsigned frame_rate = 30;
    unsigned volume_percent = 100;
};

Options g_options;
settings::Settings g_settings;
std::optional<std::vector<std::string>> g_given_mods;

constexpr float PERCENT = 100.f;

}  // namespace

void load(const std::filesystem::path& file, const Given& given) {
    g_settings = settings::Settings::load(file);
    const settings::Settings& saved = g_settings;
    // What the file says, with the launcher's limits; a value out of them reads as the default.
    Options options;
    options.game_speed_percent = static_cast<unsigned>(saved.number(settings::GAME_SPEED, 100, 100, 400));
    options.resolution_scale = static_cast<float>(saved.one_of(settings::RESOLUTION_SCALE, settings::RESOLUTION_SCALES));
    options.fullscreen = saved.flag(settings::FULLSCREEN, false);
    options.widescreen = saved.flag(settings::WIDESCREEN, false);
    options.zoom = static_cast<float>(saved.number(settings::ZOOM, 100, 50, 200)) / PERCENT;
    options.frame_rate = static_cast<unsigned>(saved.one_of(settings::FRAME_RATE, settings::FRAME_RATES));
    options.volume_percent = static_cast<unsigned>(saved.number(settings::VOLUME, 100, 0, 100));
    // What the command line gives goes first.
    options.game_speed_percent = given.game_speed_percent.value_or(options.game_speed_percent);
    options.resolution_scale = given.resolution_scale.value_or(options.resolution_scale);
    options.fullscreen = given.fullscreen.value_or(options.fullscreen);
    options.widescreen = given.widescreen.value_or(options.widescreen);
    options.zoom = given.zoom.value_or(options.zoom);
    options.frame_rate = given.frame_rate.value_or(options.frame_rate);
    options.volume_percent = std::min(given.volume_percent.value_or(options.volume_percent), 100u);
    g_options = options;
    g_given_mods = given.mods;

    runtime_log::info("player", "options (from %s and the command line): game speed %u%%, resolution %s, %s, %s, "
                      "zoom %.2f, %u frames a second, volume %u%%",
                      file.string().c_str(), options.game_speed_percent,
                      options.resolution_scale > 0.f ? "fixed" : "the window's",
                      options.fullscreen ? "full screen" : "windowed", options.widescreen ? "widescreen" : "4:3",
                      options.zoom, options.frame_rate, options.volume_percent);
}

unsigned game_speed_percent() { return g_options.game_speed_percent; }
float resolution_scale() { return g_options.resolution_scale; }
bool fullscreen() { return g_options.fullscreen; }
bool widescreen() { return g_options.widescreen; }
float zoom() { return g_options.zoom; }
unsigned frame_rate() { return g_options.frame_rate; }
unsigned volume_percent() { return g_options.volume_percent; }

bool mod_enabled(const std::string& id) {
    if (g_given_mods) {
        return std::ranges::find(*g_given_mods, id) != g_given_mods->end();
    }
    return g_settings.mod_enabled(id);
}

}  // namespace player_options
