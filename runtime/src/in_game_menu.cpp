// See in_game_menu.h.

#include "in_game_menu.h"

#include "controls.h"
#include "crop_grid.h"
#include "graphics/aurora_host.h"
#include "mods/native_mods.h"
#include "option_rows.h"
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
#include <string>
#include <utility>

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

// The bar at the top has one button for each page, in this order.
enum class Page { Picture, Sound, Mods, Controls };
constexpr int PAGE_COUNT = 4;

bool g_open = false;
Page g_page = Page::Picture;
bool g_focus_first = false;         // the next frame puts the focus on the page's first control
bool g_asking_to_quit = false;      // the question before quitting is showing
bool g_focus_question = false;      // the next frame puts the focus on its "No"
bool g_quit = false;
bool g_waiting_for_release = false;  // the menu closed; the buttons that closed it are still held
bool g_select_held = false;         // the controller's Select button, as last seen
bool g_b_held = false;              // its button that is the game's B, as last seen
bool g_b_pressed = false;           // pressed since the last frame
bool g_ready = false;               // the language and ImGui's settings are set
// The lettering was made before this frame started: ImGui builds it as a frame starts, so a menu
// opened between frames (by an event read while the game loads) waits for the next one.
bool g_lettering_ready = false;
controls::Gamepads g_gamepads;
int g_selected_device = 0;

// The language of the launcher's choice, and nothing of ImGui kept between runs.
void get_ready() {
    if (g_ready) {
        return;
    }
    g_ready = true;
    const std::string language = player_options::current().text(settings::LANGUAGE, "en");
    texts::set_language(language == "es" ? texts::Language::Spanish : texts::Language::English);
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
    g_page = Page::Picture;
    g_focus_first = true;
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
    g_waiting_for_release = true;
    g_gamepads.close();
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

void show_page(Page page) {
    if (page != g_page) {
        g_page = page;
        g_focus_first = true;
        if (page != Page::Controls) {
            g_gamepads.close();
        }
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

// Text in a rounded label, centred at the bottom of the window, like a message of the game.
void draw_note(const char* note, float bottom_margin) {
    ImFont* font = theme::fonts().small;
    ImDrawList* draw = ImGui::GetForegroundDrawList();
    const ImVec2 extent = font->CalcTextSizeA(font->FontSize, FLT_MAX, 0.f, note);
    const ImVec2 padding(theme::px(16), theme::px(7));
    const ImVec2 min((ImGui::GetIO().DisplaySize.x - extent.x) * 0.5f - padding.x,
                     ImGui::GetIO().DisplaySize.y - bottom_margin - extent.y - 2 * padding.y);
    const ImVec2 max = min + extent + padding * 2;
    draw->AddRectFilled(min, max, IM_COL32(255, 255, 255, 235), theme::px(14));
    draw->AddRect(min, max, theme::ORANGE, theme::px(14), 0, theme::px(2));
    draw->AddText(font, font->FontSize, min + padding, theme::TEXT, note);
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

void draw_picture_page() {
    settings::Settings& settings = player_options::current();
    const bool focus = std::exchange(g_focus_first, false);
    bool changed = false;
    if (option_rows::begin("##picture")) {
        changed |= option_rows::resolution(settings, focus);
        changed |= option_rows::fullscreen(settings);
        changed |= option_rows::widescreen(settings);
        changed |= option_rows::frame_rate(settings);
        changed |= option_rows::show_frame_rate(settings);
        changed |= option_rows::zoom(settings);
        option_rows::end();
    }
    // The farm's grid of crops was sized for the view the game started with (crop_grid.h).
    const float zoom = settings::zoom_for(settings.number(settings::ZOOM_OUT, 0, 0, 100));
    if (!crop_grid::covers_view(zoom, settings.flag(settings::WIDESCREEN, false))) {
        theme::note(theme::NoteKind::Info, text(Text::ZoomBeyondStart));
    }
    if (changed) {
        player_options::apply_changes();
    }
}

void draw_sound_page() {
    settings::Settings& settings = player_options::current();
    const bool focus = std::exchange(g_focus_first, false);
    if (option_rows::begin("##sound")) {
        if (option_rows::volume(settings, focus)) {
            player_options::apply_changes();
        }
        option_rows::end();
    }
}

// The name of a mod in the language of the menu; the ids this game's mods have are known here.
std::string mod_name(const std::string& id) {
    const std::pair<const char*, Text> names[] = {
        {"jamie_marriage", Text::ModNameJamieMarriage},
        {"player_speed", Text::ModNamePlayerSpeed},
        {"carry_speed", Text::ModNameCarrySpeed},
    };
    for (const auto& [known, name] : names) {
        if (id == known) {
            return text(name);
        }
    }
    return id;
}

// The mods that are compiled in, with a switch each. The game keeps the mods it started with, so
// a change waits for the next start.
void draw_mods_page() {
    const settings::Settings& settings = player_options::current();
    const bool focus = std::exchange(g_focus_first, false);
    bool first = true;
    if (option_rows::begin("##mods")) {
        for (const std::string& id : native_mods::compiled_in()) {
            bool on = settings.mod_enabled(id);
            const bool waiting = on != player_options::mod_is_running(id);
            option_rows::next_row(mod_name(id).c_str(), waiting ? text(Text::ModAppliesOnRestart) : nullptr);
            if (std::exchange(first, false) && focus) {
                ImGui::SetKeyboardFocusHere();
            }
            if (theme::toggle(id.c_str(), &on)) {
                player_options::set_mod_enabled(id, on);
            }
        }
        option_rows::end();
    }
    theme::note(theme::NoteKind::Info, text(Text::ModsRestartNote));
}

float page_height(Page page) {
    switch (page) {
        case Page::Picture: return 520.f;
        case Page::Sound: return 190.f;
        case Page::Mods: return 370.f;
        case Page::Controls: return 540.f;
    }
    return 400.f;
}

const char* page_title(Page page) {
    switch (page) {
        case Page::Picture: return text(Text::PictureSection);
        case Page::Sound: return text(Text::SoundSection);
        case Page::Mods: return text(Text::Mods);
        case Page::Controls: return text(Text::Controls);
    }
    return "";
}

// The bar at the top of the window: a button for each page, and at the right the way back to the game
// and the way out. Its height, in pixels of the window.
float bar_height() { return theme::px(46); }

void draw_bar(ImVec2 window) {
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0, 0), ImVec2(window.x, bar_height()), IM_COL32(20, 24, 40, 235));
    const float height = theme::px(34);
    const float spacing = theme::px(8);
    const float top = (bar_height() - height) * 0.5f;
    const float page_width = theme::px(120);
    const float quit_width = theme::px(190);
    const float back_width = theme::px(190);
    // Narrower when the window is.
    const float needed = PAGE_COUNT * (page_width + spacing) + quit_width + back_width + 3 * spacing;
    const float fit = std::min(1.f, window.x / needed);
    ImGui::SetCursorPos(ImVec2(spacing, top));
    for (int i = 0; i < PAGE_COUNT; ++i) {
        const Page page = static_cast<Page>(i);
        if (i > 0) {
            ImGui::SameLine(0, spacing);
        }
        const auto color = page == g_page ? theme::ButtonColor::Orange : theme::ButtonColor::Gray;
        ImGui::PushID(i);
        if (theme::menu_button(page_title(page), ImVec2(page_width * fit, height), color)) {
            show_page(page);
        }
        ImGui::PopID();
    }
    // The last two at the right.
    ImGui::SameLine(0, spacing);
    const float right = (quit_width + back_width) * fit + spacing;
    if (const float room = window.x - spacing - right - ImGui::GetCursorPosX(); room > 0) {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + room);
    }
    if (theme::menu_button(text(Text::BackToGame), ImVec2(back_width * fit, height), theme::ButtonColor::Green)) {
        close();
        return;
    }
    ImGui::SameLine(0, spacing);
    if (theme::menu_button(text(Text::QuitGame), ImVec2(quit_width * fit, height), theme::ButtonColor::Gray)) {
        g_asking_to_quit = true;
        g_focus_question = true;
    }
}

void draw_menu(ImVec2 window) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(window);
    ImGui::Begin("##options menu", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoBringToFrontOnFocus);
    ImGui::BeginDisabled(g_asking_to_quit);
    draw_bar(window);
    if (g_open) {
        // The page hangs under the bar, at the left; the game stays visible around it.
        const float margin = theme::px(16);
        const float width = g_page == Page::Controls ? theme::px(940) : theme::px(640);
        const ImVec2 min(margin, bar_height() + margin);
        const ImVec2 size(std::min(window.x - 2 * margin, width),
                          std::min(window.y - min.y - margin - theme::px(50), theme::px(page_height(g_page))));
        theme::begin_panel("##panel", min, min + size, page_title(g_page));
        switch (g_page) {
            case Page::Picture: draw_picture_page(); break;
            case Page::Sound: draw_sound_page(); break;
            case Page::Mods: draw_mods_page(); break;
            case Page::Controls:
                controls::draw_screen(g_gamepads, g_selected_device, std::exchange(g_focus_first, false));
                break;
        }
        theme::end_panel();
    }
    ImGui::EndDisabled();
    ImGui::End();
    draw_note(text(Text::MenuOpenNote), theme::px(16));
}

// "Quit the game?", over the menu, with "No" first.
void draw_quit_question(ImVec2 window) {
    ImGui::SetNextWindowPos(ImVec2(0, 0));
    ImGui::SetNextWindowSize(window);
    ImGui::Begin("##quit question", nullptr,
                 ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoMove |
                     ImGuiWindowFlags_NoSavedSettings);
    ImGui::GetWindowDrawList()->AddRectFilled(ImVec2(0, 0), window, IM_COL32(14, 22, 40, 90));
    const ImVec2 size(std::min(window.x - theme::px(48), theme::px(700)), theme::px(260));
    const ImVec2 min = (window - size) * 0.5f;
    theme::begin_panel("##question", min, min + size, text(Text::QuitGame));
    theme::paragraph(text(Text::QuitQuestion));
    ImGui::Dummy(ImVec2(0, theme::px(6)));
    const ImVec2 button((ImGui::GetContentRegionAvail().x - theme::px(16)) * 0.5f, theme::px(52));
    if (std::exchange(g_focus_question, false)) {
        ImGui::SetKeyboardFocusHere();
    }
    if (theme::menu_button(text(Text::QuitNo), button, theme::ButtonColor::Green)) {
        g_asking_to_quit = false;
    }
    ImGui::SameLine(0, theme::px(16));
    if (theme::menu_button(text(Text::QuitYes), button, theme::ButtonColor::Gray)) {
        runtime_log::info("menu", "the player quits the game");
        g_quit = true;
        close();
    }
    theme::end_panel();
    ImGui::End();
}

}  // namespace

void handle_event(const SDL_Event& event) {
    if (event.type == SDL_EVENT_KEY_DOWN && !event.key.repeat && event.key.scancode == SDL_SCANCODE_F10) {
        toggle();
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
    if (g_open || player_options::show_frame_rate()) {
        get_ready();
        fit_to_window();
        g_lettering_ready = true;
    }
}

void draw() {
    if (!g_lettering_ready) {
        return;
    }
    if (player_options::show_frame_rate()) {
        draw_frame_rate(g_open ? bar_height() : 0.f);
    }
    if (!g_open) {
        return;
    }
    // Escape and the controller's B go back: from the question to the menu, from the menu to the
    // game. The controller's shoulder buttons change the page.
    const bool back = ImGui::IsKeyPressed(ImGuiKey_Escape, false) || std::exchange(g_b_pressed, false);
    if (g_asking_to_quit) {
        if (back) {
            g_asking_to_quit = false;
        }
    } else if (back) {
        close();
        return;
    } else if (ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false) || ImGui::IsKeyPressed(ImGuiKey_GamepadR1, false)) {
        const int step = ImGui::IsKeyPressed(ImGuiKey_GamepadL1, false) ? PAGE_COUNT - 1 : 1;
        show_page(static_cast<Page>((static_cast<int>(g_page) + step) % PAGE_COUNT));
    }
    const ImVec2 window = ImGui::GetIO().DisplaySize;
    draw_menu(window);
    if (g_open && g_asking_to_quit) {
        draw_quit_question(window);
    }
}

bool is_open() { return g_open; }

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
