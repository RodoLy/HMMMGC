// The player's choices, kept in settings.json in the player's folder (paths.h).
//
// Las elecciones del jugador, guardadas en settings.json en la carpeta del jugador.
//
// It is the same file that the settings windows of the earlier packages write (Settings.ps1,
// Launcher.ps1), with the same names, so the choices carry over; values the launcher does not
// know are kept as they are. Each value read is checked against what the game accepts, and one
// that is missing or out of range reads as its default.
#pragma once

#include <nlohmann/json.hpp>

#include <filesystem>
#include <span>
#include <string>

namespace settings {

// The names in the file, and the limits of each value (the same as the runtime's options).
inline constexpr const char* MODS = "mods";
inline constexpr const char* PLAYER_SPEED = "playerSpeedPercent";
// The limits of the player_speed mod's setting (mods/player_speed/mod.json), and the variable the
// launcher gives it to the game in.
inline constexpr int PLAYER_SPEED_MINIMUM = 25;
inline constexpr int PLAYER_SPEED_MAXIMUM = 300;
inline constexpr int PLAYER_SPEED_DEFAULT = 100;
inline constexpr int PLAYER_SPEED_STEP = 5;
inline constexpr const char* PLAYER_SPEED_VARIABLE = "HM_MM_PLAYER_SPEED_PERCENT";
inline constexpr const char* RESOLUTION = "internalResolution";  // the lines the game draws, 0: the window's
inline constexpr const char* FULLSCREEN = "fullscreen";
inline constexpr const char* WIDESCREEN = "widescreen";
inline constexpr const char* FRAME_RATE = "frameRate";
inline constexpr const char* VOLUME = "volumePercent";  // 0 (silent) to 100 (the game's own)
// The languages the one setting above can say: the five of the game's text on the disc. The
// launcher and the in-game menu are in Spanish for "es" and in English for the others.
inline constexpr const char* GAME_LANGUAGES[] = {"en", "es", "fr", "de", "it"};
// Their names, each in its own language, in the same order.
inline constexpr const char* GAME_LANGUAGE_NAMES[] = {"English", "Espa\xC3\xB1ol", ("Fran\xC3\xA7" "ais"), "Deutsch",
                                                        "Italiano"};
inline constexpr const char* SHOW_FRAME_RATE = "showFrameRate";  // over the game, at the top right
// No touch buttons while a controller plays as controller 1 (runtime/src/input/touch_buttons.h).
inline constexpr const char* HIDE_TOUCH_BUTTONS = "hideTouchButtons";
// No TURBO among the touch buttons (the turbo stays on its key, Tab, and its gamepad button).
inline constexpr const char* HIDE_TOUCH_TURBO = "hideTouchTurbo";
inline constexpr const char* LANGUAGE = "language";  // of the game's dialogues, the launcher and the menu
inline constexpr const char* DISC_IMAGE = "discImage";   // the player's disc image, as UTF-8

// The cheats, each a switch in the object "cheats" of the file ({"cheats": {"stopTime": true}}). The
// cheats mod (mods/cheats) does them: the runtime loads it when one is on and gives it the names of
// those that are on, separated by commas, in CHEATS_VARIABLE.
inline constexpr const char* CHEATS = "cheats";
inline constexpr const char* CHEATS_MOD = "cheats";
inline constexpr const char* CHEATS_VARIABLE = "HM_MM_CHEATS";
inline constexpr const char* CHEAT_NAMES[] = {"infiniteStamina", "infiniteMoney", "stopTime", "maxFriendship"};

inline constexpr int RESOLUTIONS[] = {0, 480, 720, 1080, 1440};
// How many times bigger than the console's 640x480 the game draws for a resolution (0 follows
// the window): 720 lines is 1.5 times.
constexpr float resolution_scale(int lines) { return static_cast<float>(lines) / 480.f; }
inline constexpr int FRAME_RATES[] = {30, 60, 120};

class Settings {
public:
    static Settings load(const std::filesystem::path& file);
    // Writes the file (and its folder); false when it could not.
    bool save(const std::filesystem::path& file) const;

    int number(const char* name, int fallback, int minimum, int maximum) const;
    // A number that must be one of `allowed` (the first is the default).
    int one_of(const char* name, std::span<const int> allowed) const;
    bool flag(const char* name, bool fallback) const;
    std::string text(const char* name, const std::string& fallback) const;
    // Which of GAME_LANGUAGES the file says (0, English, when it says none of them).
    int language() const;
    void set(const char* name, const nlohmann::json& value);

    // A mod is off unless the file says it is on: the game is the original one until the player chooses.
    // The cheats mod is on when a cheat is.
    bool mod_enabled(const std::string& id) const;
    void set_mod_enabled(const std::string& id, bool enabled);

    // A cheat of CHEAT_NAMES; all are off unless the file says otherwise.
    bool cheat(const std::string& name) const;
    void set_cheat(const std::string& name, bool on);
    // The names of the cheats that are on, separated by commas (empty when none is).
    std::string cheats_on() const;

private:
    nlohmann::json document_ = nlohmann::json::object();
};

}  // namespace settings
