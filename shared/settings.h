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
// How far the camera is pulled back outdoors: 0 is the game's camera, 100 the farthest (zoom_for).
inline constexpr const char* ZOOM_OUT = "cameraZoomOutPercent";
inline constexpr const char* RESOLUTION = "internalResolution";  // the lines the game draws, 0: the window's
inline constexpr const char* FULLSCREEN = "fullscreen";
inline constexpr const char* WIDESCREEN = "widescreen";
inline constexpr const char* FRAME_RATE = "frameRate";
inline constexpr const char* VOLUME = "volumePercent";  // 0 (silent) to 100 (the game's own)
inline constexpr const char* SHOW_FRAME_RATE = "showFrameRate";  // over the game, at the top right
inline constexpr const char* LANGUAGE = "language";      // "auto", "es" or "en"
inline constexpr const char* DISC_IMAGE = "discImage";   // the player's disc image, as UTF-8

inline constexpr int RESOLUTIONS[] = {0, 480, 720, 1080, 1440};
// How many times bigger than the console's 640x480 the game draws for a resolution (0 follows
// the window): 720 lines is 1.5 times.
constexpr float resolution_scale(int lines) { return static_cast<float>(lines) / 480.f; }
// The camera's zoom for a "zoom out" percent: 0 is the game's (1), 100 is the farthest (0.5).
constexpr float zoom_for(int zoom_out_percent) { return 1.f - static_cast<float>(zoom_out_percent) / 200.f; }
// The "zoom out" percent for a zoom, the other way round (the command line gives a zoom).
constexpr int zoom_out_percent_for(float zoom) { return static_cast<int>((1.f - zoom) * 200.f + 0.5f); }
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
    void set(const char* name, const nlohmann::json& value);

    // A mod is off unless the file says it is on: the game is the original one until the player chooses.
    bool mod_enabled(const std::string& id) const;
    void set_mod_enabled(const std::string& id, bool enabled);

private:
    nlohmann::json document_ = nlohmann::json::object();
};

}  // namespace settings
