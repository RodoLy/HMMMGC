// See player_options.h.

#include "player_options.h"

#include "audio/audio_output.h"
#include "game_clock.h"
#include "graphics/aurora_host.h"
#include "graphics/camera_view.h"
#include "graphics/frame_interpolation.h"
#include "mods/native_mods.h"
#include "runtime_log.h"

#include <algorithm>
#include <cmath>
#include <iterator>

namespace player_options {
namespace {

// The options as the game runs with them.
struct Options {
    float resolution_scale = 0.f;
    bool fullscreen = false;
    bool widescreen = false;
    float zoom = 1.f;
    unsigned frame_rate = 30;
    unsigned volume_percent = 100;
};

constexpr float PERCENT = 100.f;

std::filesystem::path g_file;
settings::Settings g_settings;
// The options as the game runs with them, and as g_settings said them when they were last given
// to the game (they differ only in what the command line gave that the file cannot say, such as
// a resolution scale of 2, which is no resolution of the menu): the menu's changes are what differs
// from the latter.
Options g_options;
Options g_from_settings;
std::optional<std::vector<std::string>> g_given_mods;
std::vector<std::string> g_running_mods;
int g_running_player_speed = settings::PLAYER_SPEED_DEFAULT;  // what the mod was given at the start
std::string g_running_language;  // the language of the dialogues this run started with
bool g_unsaved = false;

// The options that `saved` says, with the launcher's limits: a value out of them is the default.
Options read(const settings::Settings& saved) {
    Options options;
    options.resolution_scale = settings::resolution_scale(saved.one_of(settings::RESOLUTION, settings::RESOLUTIONS));
    options.fullscreen = saved.flag(settings::FULLSCREEN, false);
    options.widescreen = saved.flag(settings::WIDESCREEN, false);
    options.zoom = settings::zoom_for(saved.number(settings::ZOOM_OUT, 0, 0, 100));
    options.frame_rate = static_cast<unsigned>(saved.one_of(settings::FRAME_RATE, settings::FRAME_RATES));
    options.volume_percent = static_cast<unsigned>(saved.number(settings::VOLUME, 100, 0, 100));
    return options;
}

// Writes into the settings what the command line gave, where the file can say it.
void write_given(settings::Settings& settings, const Given& given) {
    if (given.resolution_scale) {
        // The file keeps the lines the game draws (720 for 1.5); a scale that is none of them stays
        // out of it, as it is.
        const auto lines = static_cast<int>(std::lround(*given.resolution_scale * 480.f));
        if (settings::resolution_scale(lines) == *given.resolution_scale &&
            std::ranges::find(settings::RESOLUTIONS, lines) != std::end(settings::RESOLUTIONS)) {
            settings.set(settings::RESOLUTION, lines);
        }
    }
    if (given.fullscreen) {
        settings.set(settings::FULLSCREEN, *given.fullscreen);
    }
    if (given.widescreen) {
        settings.set(settings::WIDESCREEN, *given.widescreen);
    }
    if (given.zoom) {
        settings.set(settings::ZOOM_OUT, settings::zoom_out_percent_for(*given.zoom));
    }
    if (given.frame_rate) {
        settings.set(settings::FRAME_RATE, static_cast<int>(*given.frame_rate));
    }
    if (given.volume_percent) {
        settings.set(settings::VOLUME, static_cast<int>(*given.volume_percent));
    }
}

void log_options(const char* when, const Options& options) {
    runtime_log::info("player", "options %s: resolution %s, %s, %s, zoom %.2f, %u frames a second, "
                      "volume %u%%",
                      when, options.resolution_scale > 0.f ? "fixed" : "the window's",
                      options.fullscreen ? "full screen" : "windowed", options.widescreen ? "widescreen" : "4:3",
                      options.zoom, options.frame_rate, options.volume_percent);
}

}  // namespace

void load(const std::filesystem::path& file, const Given& given) {
    g_file = file;
    g_settings = settings::Settings::load(file);
    Options options = read(g_settings);
    write_given(g_settings, given);
    g_from_settings = read(g_settings);
    // What the command line gives goes first.
    options.resolution_scale = given.resolution_scale.value_or(options.resolution_scale);
    options.fullscreen = given.fullscreen.value_or(options.fullscreen);
    options.widescreen = given.widescreen.value_or(options.widescreen);
    options.zoom = given.zoom.value_or(options.zoom);
    options.frame_rate = given.frame_rate.value_or(options.frame_rate);
    options.volume_percent = std::min(given.volume_percent.value_or(options.volume_percent), 100u);
    g_options = options;
    g_given_mods = given.mods;
    runtime_log::info("player", "settings: %s", file.string().c_str());
    log_options("(from the settings and the command line)", options);
}

float resolution_scale() { return g_options.resolution_scale; }
bool fullscreen() { return g_options.fullscreen; }
bool widescreen() { return g_options.widescreen; }
float zoom() { return g_options.zoom; }
unsigned frame_rate() { return g_options.frame_rate; }
unsigned volume_percent() { return g_options.volume_percent; }
bool show_frame_rate() { return g_settings.flag(settings::SHOW_FRAME_RATE, false); }

bool mod_enabled(const std::string& id) {
    if (g_given_mods) {
        return std::ranges::find(*g_given_mods, id) != g_given_mods->end();
    }
    return g_settings.mod_enabled(id);
}

void note_running_mods(const std::vector<std::string>& ids) {
    g_running_mods = ids;
    g_running_language = g_settings.text(settings::GAME_LANGUAGE, settings::GAME_LANGUAGES[0]);
    if (const char* given = std::getenv(settings::PLAYER_SPEED_VARIABLE)) {
        g_running_player_speed = std::atoi(given);
    }
}

int player_speed_percent() {
    return g_settings.number(settings::PLAYER_SPEED, settings::PLAYER_SPEED_DEFAULT, settings::PLAYER_SPEED_MINIMUM,
                             settings::PLAYER_SPEED_MAXIMUM);
}

void set_player_speed_percent(int percent) {
    g_settings.set(settings::PLAYER_SPEED, percent);
    g_unsaved = true;
}

bool restart_pending() {
    if (g_settings.text(settings::GAME_LANGUAGE, settings::GAME_LANGUAGES[0]) != g_running_language) {
        return true;
    }
    if (mod_is_running("player_speed") && g_settings.mod_enabled("player_speed") &&
        player_speed_percent() != g_running_player_speed) {
        return true;
    }
    return std::ranges::any_of(native_mods::compiled_in(), [](const std::string& id) {
        return g_settings.mod_enabled(id) != mod_is_running(id);
    });
}

bool mod_is_running(const std::string& id) { return std::ranges::find(g_running_mods, id) != g_running_mods.end(); }

void set_mod_enabled(const std::string& id, bool enabled) {
    g_settings.set_mod_enabled(id, enabled);
    g_unsaved = true;
}

void set_game_language(const std::string& language) {
    g_settings.set(settings::GAME_LANGUAGE, language);
    g_unsaved = true;
}

settings::Settings& current() { return g_settings; }

void apply_changes() {
    const Options wanted = read(g_settings);
    const Options& before = g_from_settings;
    if (wanted.resolution_scale != before.resolution_scale) {
        aurora_host::set_resolution_scale(wanted.resolution_scale);
        g_options.resolution_scale = wanted.resolution_scale;
    }
    if (wanted.fullscreen != before.fullscreen) {
        aurora_host::set_fullscreen(wanted.fullscreen);
        g_options.fullscreen = wanted.fullscreen;
    }
    if (wanted.widescreen != before.widescreen) {
        aurora_host::set_widescreen(wanted.widescreen);
        g_options.widescreen = wanted.widescreen;
    }
    if (wanted.zoom != before.zoom) {
        camera_view::set_zoom(wanted.zoom);
        g_options.zoom = wanted.zoom;
    }
    if (wanted.frame_rate != before.frame_rate) {
        frame_interpolation::set_frame_rate(wanted.frame_rate);
        g_options.frame_rate = wanted.frame_rate;
    }
    if (wanted.volume_percent != before.volume_percent) {
        audio_output::set_volume(static_cast<float>(wanted.volume_percent) / PERCENT);
        g_options.volume_percent = wanted.volume_percent;
    }
    g_from_settings = wanted;
    g_unsaved = true;
}

void note_fullscreen(bool fullscreen) {
    if (g_settings.flag(settings::FULLSCREEN, false) != fullscreen) {
        g_settings.set(settings::FULLSCREEN, fullscreen);
    }
    g_from_settings.fullscreen = fullscreen;
    g_options.fullscreen = fullscreen;
}

void save_if_changed() {
    if (!g_unsaved) {
        return;
    }
    g_unsaved = false;
    log_options("changed in the menu", g_options);
    if (g_settings.save(g_file)) {
        runtime_log::info("player", "the options are saved in %s", g_file.string().c_str());
    } else {
        runtime_log::warning("player", "the options could not be saved in %s", g_file.string().c_str());
    }
}

}  // namespace player_options
