// See in_game_menu.h.

#include "in_game_menu.h"
#include "input/touch_buttons.h"

#include "crop_grid.h"
#include "controls_editor.h"
#include "graphics/aurora_host.h"
#include "graphics/second_screen.h"
#include "mods/native_mods.h"
#include "player_options.h"
#include "runtime_log.h"
#include "settings.h"
#include "texts.h"
#include "theme.h"

#include <aurora/aurora.h>
#include <backends/imgui_impl_sdl3.h>
#include <imgui.h>

#include <SDL3/SDL_gamepad.h>
#include <SDL3/SDL_video.h>

#include <algorithm>
#include <cfloat>
#include <cmath>
#include <cstdlib>
#include <iterator>
#include <string>
#include <utility>
#include <vector>

namespace in_game_menu {
namespace {

using texts::Text;
using texts::text;

// The sizes are the launcher's, made for a window of 1280x720 (shared/theme.h); in a smaller
// window the menu shrinks with it, down to where its lettering is still easy to read.
constexpr float DESIGN_WIDTH = 1280.f;
constexpr float DESIGN_HEIGHT = 720.f;
constexpr float SMALLEST_SCALE = 0.75f;

// A stick moved less than this (of 127), or a trigger pressed less than this (of 255), is let go.
constexpr int STICK_AT_REST = 24;
constexpr int TRIGGER_AT_REST = 40;

bool g_open = false;
bool g_controls_window = false;     // the window for the keys and buttons is showing
bool g_asking_to_quit = false;      // the question before quitting is showing
float g_bar_height = 0.f;           // of the bar at the top, as last drawn
bool g_quit = false;
bool g_waiting_for_release = false;  // the menu closed; the buttons that closed it are still held
bool g_select_held = false;         // the controller's Select button, as last seen
bool g_b_held = false;              // its button that is the game's B, as last seen
bool g_b_pressed = false;           // pressed since the last frame
bool g_ready = false;               // the language and ImGui's settings are set
// The lettering was made before this frame started: ImGui builds it as a frame starts, so a menu
// opened between frames (by an event read while the game loads) waits for the next one.
bool g_lettering_ready = false;

// The menu is in Spanish when that is the language of the settings, and in English for the others.
void apply_language() {
    const std::string language = settings::GAME_LANGUAGES[player_options::current().language()];
    texts::set_language(language == "es" ? texts::Language::Spanish : texts::Language::English);
}

// The language of the settings, and nothing of ImGui kept between runs.
void get_ready() {
    if (g_ready) {
        return;
    }
    g_ready = true;
    apply_language();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.LogFilename = nullptr;
}

// The controller that plays as the game's port 1, the one aurora chose.
SDL_Gamepad* first_port_gamepad() {
    const s32 index = PADGetIndexForPort(PAD_CHAN0);
    return index >= 0 ? PADGetSDLGamepadForIndex(static_cast<u32>(index)) : nullptr;
}

// Whether the game plays something with that controller's Select button (the Nintendo Switch
// Online GameCube controller has Z there): then the button does not open the menu.
bool game_uses_select() {
    u32 count = 0;
    const PADButtonMapping* mappings = PADGetButtonMappings(PAD_CHAN0, &count);
    for (u32 i = 0; mappings != nullptr && i < count; ++i) {
        if (mappings[i].nativeButton == SDL_GAMEPAD_BUTTON_BACK) {
            return true;
        }
    }
    return false;
}

// The controller's button that is the game's B (aurora's mapping): it goes back in the menu, as
// B does in the game's own menus. (The game's A, the one at the bottom of the face, is the one
// that ImGui takes for choosing.)
SDL_GamepadButton game_b_button() {
    u32 count = 0;
    const PADButtonMapping* mappings = PADGetButtonMappings(PAD_CHAN0, &count);
    for (u32 i = 0; mappings != nullptr && i < count; ++i) {
        if (mappings[i].padButton == PAD_BUTTON_B && mappings[i].nativeButton < SDL_GAMEPAD_BUTTON_COUNT) {
            return static_cast<SDL_GamepadButton>(mappings[i].nativeButton);
        }
    }
    return SDL_GAMEPAD_BUTTON_EAST;
}

void open() {
    get_ready();
    g_open = true;
    g_controls_window = false;
    g_asking_to_quit = false;
    player_options::note_fullscreen(aurora_host::is_fullscreen());
    // The keyboard and the controller move between the menu's controls; ImGui reads the same
    // controller as the game.
    ImGuiIO& io = ImGui::GetIO();
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    SDL_Gamepad* gamepad = first_port_gamepad();
    ImGui_ImplSDL3_SetGamepadMode(ImGui_ImplSDL3_GamepadMode_Manual, &gamepad, gamepad != nullptr ? 1 : 0);
    runtime_log::info("menu", "the options menu is open");
}

void close() {
    g_open = false;
    g_asking_to_quit = false;
    g_controls_window = false;
    g_waiting_for_release = true;
    controls_editor::stop();
    ImGui::GetIO().ConfigFlags &= ~(ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad);
    player_options::save_if_changed();
    runtime_log::info("menu", "the options menu is closed");
}

void toggle() {
    if (g_open) {
        close();
    } else {
        open();
    }
}

// The lettering for the window's size: the fonts are built again only when it changed.
void fit_to_window() {
    int width = 0;
    int height = 0;
    SDL_GetWindowSize(aurora_get_window(), &width, &height);
    if (width <= 0 || height <= 0) {
        return;
    }
    theme::set_scale(std::max(SMALLEST_SCALE, std::min(static_cast<float>(width) / DESIGN_WIDTH,
                                                       static_cast<float>(height) / DESIGN_HEIGHT)));
}

bool is_held(const PADStatus& status) {
    return status.err == PAD_ERR_NONE &&
           (status.button != 0 || std::abs(status.stickX) > STICK_AT_REST || std::abs(status.stickY) > STICK_AT_REST ||
            std::abs(status.substickX) > STICK_AT_REST || std::abs(status.substickY) > STICK_AT_REST ||
            status.triggerLeft > TRIGGER_AT_REST || status.triggerRight > TRIGGER_AT_REST);
}

// Text in a dark label, centred under the bar at the top, like the notices of an emulator.
void draw_note(const char* note, float top) {
    ImFont* font = theme::fonts().small;
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const ImVec2 extent = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.f, note);
    const ImVec2 padding(theme::px(10), theme::px(5));
    const ImVec2 min((ImGui::GetIO().DisplaySize.x - extent.x) * 0.5f - padding.x, top + theme::px(8));
    const ImVec2 max = min + extent + padding * 2;
    draw->AddRectFilled(min, max, IM_COL32(20, 20, 20, 200), theme::px(3));
    draw->AddText(font, font->FontSize, min + padding, IM_COL32(235, 235, 235, 255), note);
}

void draw_frame_rate(float top) {
    const std::string label =
        texts::format(Text::FrameRateCounter, static_cast<int>(std::lround(aurora_host::frames_per_second())));
    ImFont* font = theme::fonts().body;
    const ImVec2 extent = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.f, label.c_str());
    const ImVec2 position(ImGui::GetIO().DisplaySize.x - extent.x - theme::px(16), top + theme::px(10));
    theme::outlined_text(ImGui::GetForegroundDrawList(), font, font->FontSize, position, theme::WHITE,
                         IM_COL32(40, 36, 32, 230), theme::px(2.5f), label.c_str());
}

// The menu is plain, as the menus of an emulator are, and not like the launcher: ImGui's own dark
// colours and a bar at the top with drop-down menus, over the game. It is put on for the time the
// menu draws, and the style the game's other windows use is put back after.
class PlainLook {
public:
    PlainLook() : saved_(ImGui::GetStyle()) {
        ImGuiStyle& style = ImGui::GetStyle();
        ImGui::StyleColorsDark(&style);
        style.WindowRounding = 0.f;
        style.PopupRounding = theme::px(3);
        style.FrameRounding = theme::px(3);
        style.WindowBorderSize = 1.f;
        // Room between the edge of a window or a menu and what is in it (the launcher's style, which
        // the game's windows start from, has none: its panels bring their own).
        style.WindowPadding = ImVec2(theme::px(14), theme::px(12));
        style.FramePadding = ImVec2(theme::px(9), theme::px(5));
        style.ItemSpacing = ImVec2(theme::px(9), theme::px(7));
        style.ItemInnerSpacing = ImVec2(theme::px(8), theme::px(4));
        style.CellPadding = ImVec2(theme::px(8), theme::px(4));
        style.WindowTitleAlign = ImVec2(0.f, 0.5f);
        style.ScrollbarSize = theme::px(12);
        style.Colors[ImGuiCol_MenuBarBg] = ImVec4(0.07f, 0.08f, 0.10f, 0.94f);
        ImGui::PushFont(theme::fonts().small);
    }
    ~PlainLook() {
        ImGui::PopFont();
        ImGui::GetStyle() = saved_;
    }
    PlainLook(const PlainLook&) = delete;
    PlainLook& operator=(const PlainLook&) = delete;

private:
    ImGuiStyle saved_;
};

std::string resolution_label(int lines) { return lines == 0 ? text(Text::ResolutionWindow) : std::to_string(lines) + "p"; }

std::string frame_rate_label(int rate) {
    return rate == 30 ? text(Text::FrameRateGame) : texts::format(Text::FrameRateMade, rate);
}

// A switch of the settings, in a menu that stays open.
bool flag_item(settings::Settings& settings, const char* name, Text label) {
    bool value = settings.flag(name, false);
    if (!ImGui::Checkbox(text(label), &value)) {
        return false;
    }
    settings.set(name, value);
    return true;
}

// Whether the bar has room for the values beside the names of its menus ("Volume: 100%").
bool g_bar_is_wide = true;

bool draw_resolution_menu(settings::Settings& settings) {
    const int lines = settings.one_of(settings::RESOLUTION, settings::RESOLUTIONS);
    const std::string title = std::string(text(Text::Resolution)) +
                              (g_bar_is_wide ? ": " + resolution_label(lines) : std::string()) + "###resolution";
    bool changed = false;
    if (ImGui::BeginMenu(title.c_str())) {
        for (const int value : settings::RESOLUTIONS) {
            if (ImGui::MenuItem(resolution_label(value).c_str(), nullptr, value == lines)) {
                settings.set(settings::RESOLUTION, value);
                changed = true;
            }
        }
        ImGui::EndMenu();
    }
    return changed;
}

bool draw_picture_menu(settings::Settings& settings) {
    bool changed = false;
    if (ImGui::BeginMenu(text(Text::PictureSection))) {
        changed |= flag_item(settings, settings::FULLSCREEN, Text::Fullscreen);
        changed |= flag_item(settings, settings::WIDESCREEN, Text::Widescreen);
        const int rate = settings.one_of(settings::FRAME_RATE, settings::FRAME_RATES);
        const std::string rate_title = std::string(text(Text::FrameRate)) + ": " + std::to_string(rate) + "###rate";
        if (ImGui::BeginMenu(rate_title.c_str())) {
            for (const int value : settings::FRAME_RATES) {
                if (ImGui::MenuItem(frame_rate_label(value).c_str(), nullptr, value == rate)) {
                    settings.set(settings::FRAME_RATE, value);
                    changed = true;
                }
            }
            ImGui::EndMenu();
        }
        changed |= flag_item(settings, settings::SHOW_FRAME_RATE, Text::ShowFrameRate);
#if defined(__ANDROID__)
        // The touch buttons' options, only where the buttons are part of playing: a computer shows
        // them only on a touch screen, and its launcher has these options too.
        changed |= flag_item(settings, settings::HIDE_TOUCH_BUTTONS, Text::HideTouchButtons);
        changed |= flag_item(settings, settings::HIDE_TOUCH_TURBO, Text::HideTouchTurbo);
#endif
        ImGui::EndMenu();
    }
    return changed;
}

bool draw_audio_menu(settings::Settings& settings) {
    int volume = settings.number(settings::VOLUME, 100, 0, 100);
    const std::string title = std::string(text(Text::Volume)) +
                              (g_bar_is_wide ? ": " + std::to_string(volume) + "%" : std::string()) + "###audio";
    bool changed = false;
    if (ImGui::BeginMenu(title.c_str())) {
        ImGui::SetNextItemWidth(theme::px(240));
        if (ImGui::SliderInt("##volume", &volume, 0, 100, "%d%%")) {
            settings.set(settings::VOLUME, volume);
            changed = true;
        }
        ImGui::EndMenu();
    }
    return changed;
}

// The colour of what tells the player that the game has to be restarted.
constexpr ImVec4 RESTART_COLOR(1.f, 0.82f, 0.35f, 1.f);

// The language of the game's dialogues: the game reads it when it starts (game_language.h).
void draw_language_menu() {
    settings::Settings& settings = player_options::current();
    const int chosen = settings.language();
    if (!ImGui::BeginMenu(text(Text::GameLanguageShort))) {
        return;
    }
    for (int i = 0; i < static_cast<int>(std::size(settings::GAME_LANGUAGES)); ++i) {
        if (ImGui::MenuItem(settings::GAME_LANGUAGE_NAMES[i], nullptr, i == chosen)) {
            player_options::set_game_language(settings::GAME_LANGUAGES[i]);
            apply_language();  // the menu changes at once; the game's dialogues, at the next start
        }
    }
    ImGui::Separator();
    ImGui::PushTextWrapPos(theme::px(340));
    ImGui::TextColored(RESTART_COLOR, "%s", text(Text::GameLanguageRestart));
    ImGui::PopTextWrapPos();
    ImGui::EndMenu();
}

// The name of a mod in the language of the menu; the ids this game's mods have are known here.
std::string mod_name(const std::string& id) {
    const std::pair<const char*, Text> names[] = {
        {"jamie_marriage", Text::ModNameJamieMarriage},
        {"player_speed", Text::ModNamePlayerSpeed},
        {"carry_speed", Text::ModNameCarrySpeed},
        {"far_camera", Text::ModNameFarCamera},
        {"marry_anyone", Text::ModNameMarryAnyone},
        {"longer_days", Text::ModNameLongerDays},
        {"gentle_stamina", Text::ModNameGentleStamina},
        {"faster_tools", Text::ModNameFasterTools},
    };
    for (const auto& [known, name] : names) {
        if (id == known) {
            return text(name);
        }
    }
    return id;
}

// How fast the player_speed mod makes the character, under its switch: the same number as in the
// launcher, in steps of 5.
void draw_player_speed(bool mod_on) {
    int percent = player_options::player_speed_percent();
    const std::string format = std::string(text(Text::PlayerSpeed)) + ": %d%%";
    ImGui::BeginDisabled(!mod_on);
    ImGui::Indent();
    ImGui::SetNextItemWidth(theme::px(260));
    if (ImGui::SliderInt("##player speed", &percent, settings::PLAYER_SPEED_MINIMUM, settings::PLAYER_SPEED_MAXIMUM,
                         format.c_str())) {
        const int step = settings::PLAYER_SPEED_STEP;
        const int rounded = (percent + step / 2) / step * step;
        if (rounded != player_options::player_speed_percent()) {
            player_options::set_player_speed_percent(rounded);  // the character has it from the next frame
        }
    }
    ImGui::Unindent();
    ImGui::EndDisabled();
}

// The mods that only make the game more comfortable ("category": "quality_of_life" in their
// mod.json), shown after the others under their own heading, as the launcher shows them.
bool is_quality_of_life(const std::string& id) {
    for (const char* comfort :
         {"player_speed", "carry_speed", "far_camera", "longer_days", "gentle_stamina", "faster_tools"}) {
        if (id == comfort) {
            return true;
        }
    }
    return false;
}

// One mod's switch, with when a change to it counts.
void draw_mod_switch(const settings::Settings& settings, const std::string& id) {
    bool on = settings.mod_enabled(id);
    const std::string label = mod_name(id) + "###" + id;
    if (ImGui::Checkbox(label.c_str(), &on)) {
        player_options::set_mod_enabled(id, on);
    }
    // Each mod says when a change to it counts: at once, or when the game is restarted.
    ImGui::SameLine();
    if (native_mods::switches_while_running(id)) {
        ImGui::TextDisabled("(%s)", text(Text::ModAppliesAtOnce));
    } else {
        ImGui::TextColored(RESTART_COLOR, "(%s)", text(Text::NeedsRestart));
    }
    if (id == "player_speed") {
        draw_player_speed(on);
    }
}

// The mods that are compiled in, with a switch each. The game keeps the mods it started with, so
// a change waits for the next start.
void draw_mods_menu() {
    if (!ImGui::BeginMenu(text(Text::Mods))) {
        return;
    }
    const settings::Settings& settings = player_options::current();
    std::vector<std::string> comfort;
    for (const std::string& id : native_mods::compiled_in()) {
        if (id == settings::CHEATS_MOD) {
            continue;  // on when a cheat is, in the launcher's options
        }
        if (is_quality_of_life(id)) {
            comfort.push_back(id);
        } else {
            draw_mod_switch(settings, id);
        }
    }
    if (!comfort.empty()) {
        ImGui::SeparatorText(text(Text::QualityOfLife));
        for (const std::string& id : comfort) {
            draw_mod_switch(settings, id);
        }
    }
    ImGui::EndMenu();
}

// The bar at the top: the picture, the sound, the mods and the keys, and at the right the way back
// to the game and the way out.
void draw_menu_bar(ImVec2 window) {
    if (!ImGui::BeginMainMenuBar()) {
        return;
    }
    settings::Settings& settings = player_options::current();
    bool changed = false;
    // The game's name and the values of the menus only where the bar has room for them.
    g_bar_is_wide = window.x >= theme::px(1100);
    if (g_bar_is_wide) {
        ImGui::TextDisabled("Harvest Moon MM");
    }
    changed |= draw_resolution_menu(settings);
    changed |= draw_picture_menu(settings);
    if (ImGui::MenuItem(text(Text::ControllerSettings))) {
        g_controls_window = true;
    }
    changed |= draw_audio_menu(settings);
    draw_language_menu();
    draw_mods_menu();
    if (changed) {
        player_options::apply_changes();
    }
    if (player_options::restart_pending()) {
        ImGui::TextColored(RESTART_COLOR, "%s", text(Text::RestartPending));
    }
    // At the right, in the order they are read from the end.
    const ImGuiStyle& style = ImGui::GetStyle();
    const float quit_width = ImGui::CalcTextSize(text(Text::QuitGame)).x + 2 * style.FramePadding.x;
    const float hide_width = ImGui::CalcTextSize(text(Text::HideMenu)).x + 2 * style.FramePadding.x;
    ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX() + style.ItemSpacing.x,
                                  window.x - quit_width - hide_width - 4 * style.ItemSpacing.x - theme::px(8)));
    if (ImGui::MenuItem(text(Text::QuitGame))) {
        g_asking_to_quit = true;
    }
    if (ImGui::MenuItem(text(Text::HideMenu))) {
        close();
    }
    g_bar_height = ImGui::GetWindowSize().y;
    ImGui::EndMainMenuBar();
}

// "Quit the game?", over the menu, with "No" first.
void draw_quit_question() {
    ImGui::SetNextWindowPos(ImGui::GetMainViewport()->GetCenter(), ImGuiCond_Always, ImVec2(0.5f, 0.5f));
    if (g_asking_to_quit && !ImGui::IsPopupOpen("###quit")) {
        ImGui::OpenPopup("###quit");
    }
    const std::string title = std::string(text(Text::QuitGame)) + "###quit";
    if (ImGui::BeginPopupModal(title.c_str(), &g_asking_to_quit,
                               ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoSavedSettings)) {
        ImGui::TextUnformatted(text(Text::QuitQuestion));
        ImGui::Spacing();
        ImGui::SetItemDefaultFocus();
        if (ImGui::Button(text(Text::QuitNo), ImVec2(theme::px(120), 0))) {
            g_asking_to_quit = false;
        }
        ImGui::SameLine();
        if (ImGui::Button(text(Text::QuitYes), ImVec2(theme::px(120), 0))) {
            runtime_log::info("menu", "the player quits the game");
            g_quit = true;
            close();
        }
        ImGui::EndPopup();
    }
}

void draw_menu(ImVec2 window) {
    const PlainLook look;
    draw_menu_bar(window);
    if (g_controls_window && !controls_editor::draw_window(&g_controls_window)) {
        controls_editor::stop();
    }
    draw_quit_question();
}

// Escape and the controller's B go back: from the question to the menu, from the menu to the
// game. Returns whether the menu closed.
bool go_back_if_asked() {
    const bool back = (ImGui::IsKeyPressed(ImGuiKey_Escape, false) || std::exchange(g_b_pressed, false)) &&
                      !controls_editor::busy();
    if (!back) {
        return false;
    }
    if (g_asking_to_quit) {
        g_asking_to_quit = false;
        return false;
    }
    close();
    return true;
}

// The same menus as the bar's, for the second screen (graphics/second_screen.h), where the menu is
// always there to be touched: one under the other in a column at the left, each opening to its
// right, in bigger letters, with what the controller does underneath.
void draw_on_second_screen(ImVec2 screen) {
    if (g_open) {
        go_back_if_asked();  // closed, it stays on the screen without the controller
    }
    const PlainLook look;
    ImGui::SetNextWindowPos(ImVec2(0.f, 0.f));
    ImGui::SetNextWindowSize(screen);
    constexpr ImGuiWindowFlags SCREEN = ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoMove |
                                        ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus;
    if (ImGui::Begin("###second screen", nullptr, SCREEN)) {
        ImGui::TextDisabled("Harvest Moon MM");
        ImGui::Separator();
        settings::Settings& settings = player_options::current();
        bool changed = false;
        g_bar_is_wide = true;  // the menus' names have room for their values
        ImGui::PushFont(theme::fonts().body);
        ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(theme::px(10), theme::px(18)));
        if (ImGui::BeginChild("##menus", ImVec2(screen.x * 0.48f, 0.f), ImGuiChildFlags_AutoResizeY)) {
            changed |= draw_resolution_menu(settings);
            changed |= draw_picture_menu(settings);
            if (ImGui::MenuItem(text(Text::ControllerSettings))) {
                g_controls_window = true;
            }
            changed |= draw_audio_menu(settings);
            draw_language_menu();
            draw_mods_menu();
            ImGui::Separator();
            if (g_open && ImGui::MenuItem(text(Text::BackToGame))) {
                close();
            }
            if (ImGui::MenuItem(text(Text::QuitGame))) {
                g_asking_to_quit = true;
            }
        }
        ImGui::EndChild();
        ImGui::PopStyleVar();
        ImGui::PopFont();
        if (changed) {
            player_options::apply_changes();
        }
        if (player_options::restart_pending()) {
            ImGui::TextColored(RESTART_COLOR, "%s", text(Text::RestartPending));
        }
        // What the controller does, at the bottom of the screen.
        const char* note = text(g_open ? Text::SecondScreenControllerNote : Text::SecondScreenTouchNote);
        ImGui::SetCursorPosY(std::max(ImGui::GetCursorPosY(), screen.y - ImGui::GetTextLineHeightWithSpacing() * 2.f -
                                                                  ImGui::GetStyle().WindowPadding.y));
        ImGui::Separator();
        ImGui::PushTextWrapPos(0.f);
        ImGui::TextDisabled("%s", note);
        ImGui::PopTextWrapPos();
    }
    ImGui::End();
    if (g_controls_window && !controls_editor::draw_window(&g_controls_window)) {
        controls_editor::stop();
    }
    draw_quit_question();
}

}  // namespace

void handle_event(const SDL_Event& event) {
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.scancode == SDL_SCANCODE_F10) {
        toggle();
    } else if (g_open) {
        controls_editor::handle_event(event);
    }
}


void before_frame() {
    SDL_Gamepad* gamepad = first_port_gamepad();
    const bool select =
        gamepad != nullptr && SDL_GetGamepadButton(gamepad, SDL_GAMEPAD_BUTTON_BACK) && !game_uses_select();
    if (select && !g_select_held) {
        toggle();
    }
    g_select_held = select;
    const bool b = gamepad != nullptr && SDL_GetGamepadButton(gamepad, game_b_button());
    g_b_pressed = b && !g_b_held;
    g_b_held = b;
    second_screen::update(true);
    if (g_open || player_options::show_frame_rate() || touch_buttons::visible() || second_screen::available()) {
        get_ready();
        fit_to_window();
        g_lettering_ready = true;
    }
}

void draw() {
    if (!g_lettering_ready) {
        return;
    }
    // With a second screen the menu is there, always, and the game's screen keeps only a note while
    // the menu has the controller.
    if (second_screen::available()) {
        second_screen::frame(g_open ? first_port_gamepad() : nullptr, draw_on_second_screen);
        if (player_options::show_frame_rate()) {
            draw_frame_rate(0.f);
        }
        if (g_open) {
            draw_note(text(Text::MenuOnSecondScreenNote), 0.f);
        }
        return;
    }
    if (player_options::show_frame_rate()) {
        draw_frame_rate(g_open ? g_bar_height : 0.f);
    }
    if (!g_open || go_back_if_asked()) {
        return;
    }
    const ImVec2 window = ImGui::GetIO().DisplaySize;
    draw_menu(window);
    if (g_open) {
        draw_note(text(Text::MenuOpenNote), g_bar_height);
    }
}

bool is_open() { return g_open; }

void open_from_touch() {
    if (!g_open) {
        open();
    }
}

bool quit_requested() { return g_quit; }

void hold_back_controllers(PADStatus statuses[PAD_CHANMAX]) {
    if (!g_open && !g_waiting_for_release) {
        return;
    }
    if (!g_open && std::none_of(statuses, statuses + PAD_CHANMAX, is_held)) {
        g_waiting_for_release = false;
        return;
    }
    for (uint32_t port = 0; port < PAD_CHANMAX; ++port) {
        const s8 error = statuses[port].err;
        statuses[port] = PADStatus{};
        statuses[port].err = error;
    }
}

}  // namespace in_game_menu
