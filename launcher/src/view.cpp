// See app.h. How the launcher looks and how it is used, after its design ("Launcher v4", made in
// Claude Design): every piece with the place, the size, the colors and the movement its CSS gives it.
// The design is 1280x752: the window's title bar (32 pixels) and the launcher under it; TOP moves
// the launcher's coordinates under the title bar.
//
// Cómo se ve el launcher y cómo se usa, según su diseño: cada pieza con el lugar, el tamaño, los
// colores y el movimiento que le da su CSS.

#include "app.h"

#include "look.h"
#include "texts.h"

#include <SDL3/SDL_video.h>

#include <algorithm>
#include <cmath>
#include <iterator>

namespace {

using lettering::Face;
using look::Box;
using look::Corners;
using look::Rect;
using look::rgb;
using look::Sides;
using look::TextStyle;
using texts::Text;
using texts::text;

constexpr const char* PACKAGE_VERSION = HMMM_PACKAGE_VERSION;

constexpr float TOP = 32.f;               // the launcher, under the window's title bar
constexpr float LAUNCHER_HEIGHT = 720.f;

// The design's colors.
constexpr ImU32 NIGHT = rgb(0x17115e);        // the title bar, the panel, the keys
constexpr ImU32 PINK = rgb(0xf2718f);         // the band at the top
constexpr ImU32 PINK_DARK = rgb(0xd4496b);
constexpr ImU32 ORANGE = rgb(0xf08a2c);       // the tabs
constexpr ImU32 TEAL = rgb(0x8fdcd8);         // the chosen tab and the panel's border
constexpr ImU32 ROYAL = rgb(0x1c2a8c);        // the boxes of the header
constexpr ImU32 YELLOW = rgb(0xffe36b);
constexpr ImU32 ROW_TEXT = rgb(0xf2f9ff);
constexpr ImU32 ROW_SHADOW = rgb(0x0a0a3c, 0.45f);
constexpr ImU32 BAR_TOP = rgb(0xd2ffe0);      // the green of the bars and the scroll bar
constexpr ImU32 BAR_BOTTOM = rgb(0x6fd89a);

// The symbol of each tab, in the medallion and on the tab, and the color of its rows' squares.
// ♪ ⚙ ✿ ✚
constexpr const char* TAB_SYMBOLS[] = {"\xE2\x99\xAA", "\xE2\x9A\x99", "\xE2\x9C\xBF", "\xE2\x9C\x9A"};
constexpr ImU32 TAB_COLORS[] = {rgb(0xf08a2c), rgb(0x4aa0d8), rgb(0x5ab04a), rgb(0x9a6ad0)};

constexpr const char* NOTE = "\xE2\x99\xAA";        // ♪
constexpr const char* LEFT_ARROW = "\xE2\x97\x80";  // ◀
constexpr const char* RIGHT_ARROW = "\xE2\x96\xB6"; // ▶

// A rectangle of the launcher, in its own coordinates (under the title bar).
Rect at(float x, float y, float w, float h) { return {x, y + TOP, w, h}; }

TextStyle style(Face face, int size, float spacing, ImU32 color) {
    return {.face = face, .size = size, .spacing = spacing, .color = color};
}

struct ChipColors {
    ImU32 background;
    ImU32 text;
};

ChipColors chip_colors(App::Tone tone) {
    switch (tone) {
    case App::Tone::On: return {rgb(0xc9f2b8), rgb(0x1f6a1a)};
    case App::Tone::Off: return {rgb(0xc5cede), rgb(0x3d4960)};
    case App::Tone::Value: return {rgb(0xcfe2ff), rgb(0x173f94)};
    case App::Tone::Warning: return {rgb(0xffe9a0), rgb(0x6e4d00)};
    case App::Tone::Key: return {NIGHT, rgb(0xffffff)};
    case App::Tone::None: break;
    }
    return {0, 0};
}

// A chip with `value` (padding 3px 14px), ending at `right` and centered on `center_y`.
// Returns where it starts; an empty one takes its room without being drawn.
float draw_chip(ImDrawList* draw, const std::string& value, App::Tone tone, int size, float right, float center_y) {
    const TextStyle lettering = style(Face::Dosis700, size, 0.05f, chip_colors(tone).text);
    const float width = look::text_width(lettering, value) + 28.f;
    const float height = look::line_height(lettering) + 6.f;
    const float x = right - width;
    if (tone != App::Tone::None && !value.empty()) {
        look::fill(draw, {x, center_y - height * 0.5f, width, height}, 10.f, chip_colors(tone).background);
        look::draw_text(draw, lettering, x + 14.f, look::baseline_centered(lettering, center_y), value);
    }
    return x;
}

// A text centered on `center_x` and `center_y`, as a grid's place-items: center puts it.
void draw_centered(ImDrawList* draw, const TextStyle& lettering, float center_x, float center_y,
                   std::string_view text) {
    look::draw_text(draw, lettering, center_x - look::text_width(lettering, text) * 0.5f,
                    look::baseline_centered(lettering, center_y), text);
}

// The height of the list of rows, and how far a notch of the wheel or an arrow of the scroll bar
// moves it, in the design's pixels.
constexpr float LIST_HEIGHT = 310.f;
constexpr float WHEEL_STEP = 60.f;

// The animation of a row or a tab coming in from the left (hmSlideL).
struct Slide {
    float dx;
    float opacity;
};
Slide slide_in(double seconds, float delay, float duration, bool still) {
    const float done = still ? 1.f : look::ease_out(look::progress(seconds, delay, duration));
    return {-40.f * (1.f - done), done};
}

}  // namespace

bool App::moves_window(ImVec2 window_point) {
    const ImVec2 point = look::to_design(window_point);
    return point.y >= 0.f && point.y < 30.f && point.x >= 0.f && point.x < 1192.f;
}

bool App::hovered(float x, float y, float w, float h) {
    const bool inside = Rect{x, y, w, h}.contains(look::to_design(ImGui::GetIO().MousePos));
    if (inside) {
        ImGui::SetMouseCursor(ImGuiMouseCursor_Hand);
    }
    return inside;
}

bool App::clicked(float x, float y, float w, float h) {
    return hovered(x, y, w, h) && ImGui::IsMouseClicked(ImGuiMouseButton_Left);
}

// -- The keys, the controller and the wheel -------------------------------------------------------

void App::handle_keys(const std::vector<Row>& rows) {
    const auto pressed = [](std::initializer_list<ImGuiKey> keys, bool repeat) {
        return std::ranges::any_of(keys, [&](ImGuiKey key) { return ImGui::IsKeyPressed(key, repeat); });
    };
    const bool preparing = preparation_.state() == Preparation::State::Running;
    if (pressed({ImGuiKey_Escape}, false)) {
        if (preparing) {
            preparation_.cancel();
        } else {
            quit_ = true;
        }
        return;
    }
    if (pressed({ImGuiKey_Q, ImGuiKey_GamepadL1}, false)) {
        choose_tab(static_cast<Tab>((static_cast<int>(tab_) + 3) % 4));
        return;
    }
    if (pressed({ImGuiKey_E, ImGuiKey_GamepadR1}, false)) {
        choose_tab(static_cast<Tab>((static_cast<int>(tab_) + 1) % 4));
        return;
    }
    if (pressed({ImGuiKey_UpArrow, ImGuiKey_GamepadDpadUp, ImGuiKey_GamepadLStickUp}, true)) {
        move_selection(-1);
    }
    if (pressed({ImGuiKey_DownArrow, ImGuiKey_GamepadDpadDown, ImGuiKey_GamepadLStickDown}, true)) {
        move_selection(1);
    }
    const bool right = pressed({ImGuiKey_RightArrow, ImGuiKey_GamepadDpadRight, ImGuiKey_GamepadLStickRight}, true);
    const bool left = pressed({ImGuiKey_LeftArrow, ImGuiKey_GamepadDpadLeft, ImGuiKey_GamepadLStickLeft}, true);
    const int step = (right ? 1 : 0) - (left ? 1 : 0);
    const bool enter = pressed({ImGuiKey_Enter, ImGuiKey_KeypadEnter, ImGuiKey_GamepadFaceDown}, false);

    if (tab_ == Tab::Mods) {
        if (mods_.empty()) {
            return;
        }
        selected_ = std::clamp(selected_, 0, static_cast<int>(mods_.size()) - 1);
        const mods::Mod& mod = mods_[static_cast<size_t>(selected_)];
        if (enter) {
            settings_.set_mod_enabled(mod.id, !settings_.mod_enabled(mod.id));
            save_settings();
        }
        // Left and right change the mod's setting, the first one if it has several.
        if (step != 0 && !mod.settings.empty()) {
            const mods::Setting& setting = mod.settings.front();
            const int value = settings_.number(setting.key.c_str(), setting.fallback, setting.minimum, setting.maximum);
            settings_.set(setting.key.c_str(),
                          std::clamp(value + step * setting.step, setting.minimum, setting.maximum));
            save_settings();
        }
    } else if (selected_ >= 0 && selected_ < static_cast<int>(rows.size())) {
        const Row& row = rows[static_cast<size_t>(selected_)];
        if (step != 0 && row.change) {
            row.change(step);
        }
        if (enter && row.activate) {
            row.activate();
        }
    }

    // The wheel scrolls the list, or a mod's description when it is over it.
    const float wheel = ImGui::GetIO().MouseWheel;
    if (wheel != 0.f) {
        const bool over_description =
            tab_ == Tab::Mods && at(634, 234, 550, 310).contains(look::to_design(ImGui::GetIO().MousePos));
        if (over_description) {
            description_scroll_ = std::max(0.f, description_scroll_ - wheel * 30.f);
        } else {
            scroll_ -= wheel * WHEEL_STEP;
            follow_selection_ = false;
        }
    }
}

// -- The frame ------------------------------------------------------------------------------------

void App::draw(double seconds, const std::vector<Row>& rows) {
    ImDrawList* draw = ImGui::GetBackgroundDrawList();
    const ImVec2 mouse = ImGui::GetIO().MousePos;
    mouse_moved_ = last_mouse_.x >= 0.f && (mouse.x != last_mouse_.x || mouse.y != last_mouse_.y);
    last_mouse_ = mouse;

    draw_title_bar(draw);
    draw->PushClipRect(look::to_window(ImVec2(0.f, TOP)), look::to_window(ImVec2(look::WIDTH, look::HEIGHT)), true);
    draw_background(draw, seconds);
    if (!still_) {
        draw_bubbles(draw, seconds);
    }
    draw_header(draw, seconds);
    draw_tabs(draw, seconds);
    draw_panel(draw, seconds, rows);
    std::string message = toast_;
    if (message.empty() && tab_ == Tab::Mods && selected_ < static_cast<int>(mods_.size())) {
        const mods::Mod& mod = mods_[static_cast<size_t>(selected_)];
        message = mod.version.empty() && mod.description.empty() ? text(Text::NoManifestHint) : mod.summary;
    } else if (message.empty() && selected_ < static_cast<int>(rows.size())) {
        message = rows[static_cast<size_t>(selected_)].hint;
    }
    draw_message(draw, seconds, message);
    draw->PopClipRect();
}

void App::draw_title_bar(ImDrawList* draw) {
    look::fill(draw, {0.f, 0.f, look::WIDTH, 30.f}, 0.f, NIGHT);
    look::fill(draw, {0.f, 30.f, look::WIDTH, 2.f}, 0.f, rgb(0x4a5fd0));
    TextStyle title = style(Face::Dosis700, 15, 0.06f, rgb(0xdfe6ff));
    TextStyle note = title;
    note.color = YELLOW;
    const float baseline = look::baseline_centered(title, 15.f);
    look::draw_text(draw, note, 14.f, baseline, NOTE);
    look::draw_text(draw, title, 14.f + look::text_width(note, NOTE) + 8.f, baseline,
                    std::string("Harvest Moon: Magical Melody \xE2\x80\x94 ") + text(Text::Subtitle));

    // Minimize and close, which light up under the mouse.
    struct Button {
        float x;
        const char* symbol;
        ImU32 lit;
        bool close;
    };
    const Button buttons[] = {{1192.f, "\xE2\x80\x93", rgb(0xffffff, 0.12f), false},  // –
                              {1234.f, "\xE2\x9C\x95", rgb(0xc0392b), true}};          // ✕
    for (const Button& button : buttons) {
        if (hovered(button.x, 2.f, 40.f, 26.f)) {
            look::fill(draw, {button.x, 2.f, 40.f, 26.f}, 4.f, button.lit);
        }
        draw_centered(draw, title, button.x + 20.f, 15.f, button.symbol);
        if (clicked(button.x, 2.f, 40.f, 26.f)) {
            if (button.close) {
                later([this] {
                    preparation_.cancel();
                    quit_ = true;
                });
            } else {
                later([this] { SDL_MinimizeWindow(window_); });
            }
        }
    }
}

void App::draw_background(ImDrawList* draw, double seconds) {
    // A checkerboard of two blues that moves down and to the right, 112 pixels every 14 seconds.
    const Rect area = at(0.f, 0.f, look::WIDTH, LAUNCHER_HEIGHT);
    look::fill(draw, area, 0.f, rgb(0x1380dc));
    const float shift = still_ ? 0.f : static_cast<float>(std::fmod(seconds, 14.0) / 14.0 * 112.0);
    look::checkerboard(draw, area, 56.f, ImVec2(shift, shift), rgb(0x1b8eea));
}

void App::draw_bubbles(ImDrawList* draw, double seconds) {
    // Seven bubbles with notes that rise from below the bottom, each at its pace (hmBubble).
    constexpr float LEFT[] = {3.f, 12.f, 88.f, 96.f, 45.f, 70.f, 22.f};
    // ♪ ♫ ♩ ♬
    constexpr const char* NOTES[] = {"\xE2\x99\xAA", "\xE2\x99\xAB", "\xE2\x99\xA9", "\xE2\x99\xAC"};
    for (int i = 0; i < static_cast<int>(std::size(LEFT)); ++i) {
        const float size = 44.f + static_cast<float>((i * 37) % 4) * 12.f;
        const double duration = 10.0 + (i * 13) % 6;
        const double delay = std::round((1.5 + std::fmod(i * 1.7, 8.0)) * 10.0) / 10.0;
        if (seconds < delay) {
            continue;
        }
        const float phase = static_cast<float>(std::fmod(seconds - delay, duration) / duration);
        float dx = 0.f;
        float dy = 0.f;
        float grow = 1.f;
        if (phase < 0.5f) {
            const float done = look::ease_in_out(phase / 0.5f);
            dx = 18.f * done;
            dy = -400.f * done;
            grow = 0.6f + 0.4f * done;
        } else {
            const float done = look::ease_in_out((phase - 0.5f) / 0.5f);
            dx = 18.f - 28.f * done;
            dy = -400.f - 420.f * done;
        }
        const float opacity = phase < 0.1f   ? look::ease_in_out(phase / 0.1f)
                              : phase > 0.9f ? 1.f - look::ease_in_out((phase - 0.9f) / 0.1f)
                                             : 1.f;
        const float outside = size + 4.f;
        // Its bottom starts 80 pixels below the launcher's.
        const ImVec2 center(LEFT[i] / 100.f * look::WIDTH + outside * 0.5f,
                            TOP + LAUNCHER_HEIGHT + 80.f - outside * 0.5f);
        const look::Moved moved(draw, dx, dy, opacity, grow, center);
        look::bubble(draw, center, size, NOTES[i % 4], static_cast<int>(std::lround(size * 0.55f)));
    }
}

void App::draw_header(ImDrawList* draw, double seconds) {
    const float done = still_ ? 1.f : look::ease_out(look::progress(seconds, 0.1f, 0.5f));
    const look::Moved moved(draw, 0.f, -160.f * (1.f - done), done);
    const ImU32 ring = rgb(0xffffff, 0.09f);
    const ImVec2 rings_center(110.f, 100.f + TOP);

    // The pink medallion, half under the band, and the band with its rings, as a record's grooves.
    look::draw_box(draw, Box{.rect = at(-30.f, -40.f, 280.f, 280.f), .radius = 140.f, .background = PINK,
                             .border = 6.f, .border_color = PINK_DARK});
    look::rings(draw, rings_center, 134.f, 29.f, 26.f, 29.f, ring);
    const Rect band = at(0.f, 0.f, look::WIDTH, 150.f);
    look::fill(draw, band, 0.f, PINK);
    draw->PushClipRect(look::to_window(ImVec2(band.x, band.y)), look::to_window(ImVec2(band.right(), band.bottom())),
                       true);
    look::rings(draw, rings_center, 1180.f, 29.f, 26.f, 29.f, ring);
    draw->PopClipRect();
    look::fill(draw, at(239.f, 150.f, 1041.f, 6.f), 0.f, PINK_DARK);

    // The shoulder buttons that change the tab.
    const TextStyle shoulder = style(Face::Baloo800, 18, 0.f, rgb(0x7a3fa8));
    const std::string labels[] = {std::string(LEFT_ARROW) + " L", std::string("R ") + RIGHT_ARROW};
    const float lefts[] = {12.f, 168.f};
    for (int i = 0; i < 2; ++i) {
        const float width = look::text_width(shoulder, labels[i]) + 20.f;
        const Rect button = at(lefts[i], 10.f, width, 30.f);
        look::draw_box(draw, Box{.rect = button, .radius = 8.f, .background = rgb(0xffffff),
                                 .shadows = {{{.y = 3.f, .color = rgb(0xb03a5c)}}}});
        look::draw_text(draw, shoulder, button.x + 10.f, look::baseline_in_line(shoulder, button.y, 30.f), labels[i]);
        if (clicked(button.x, button.y, button.w, button.h)) {
            later([this, i] { choose_tab(static_cast<Tab>((static_cast<int>(tab_) + (i == 0 ? 3 : 1)) % 4)); });
        }
    }

    // The medallion with the tab's symbol, and the tab's name.
    const auto tab = static_cast<size_t>(tab_);
    look::draw_box(draw, Box{.rect = at(48.f, 44.f, 124.f, 124.f), .radius = 62.f, .background = rgb(0xfff3d6),
                             .border = 6.f, .border_color = rgb(0xffffff),
                             .shadows = {{{.y = 5.f, .color = PINK_DARK}}}});
    draw_centered(draw, style(Face::Baloo800, 62, 0.f, ORANGE), 110.f, 106.f + TOP, TAB_SYMBOLS[tab]);
    const char* names[] = {text(Text::GameTab), text(Text::Options), text(Text::Mods), text(Text::Controls)};
    TextStyle section = style(Face::Baloo800, 58, 0.02f, rgb(0xeaf45c));
    section.stroke = 9.f;
    section.stroke_color = rgb(0x43206f);
    section.shadow_y = 5.f;
    section.shadow_color = rgb(0x43206f);
    look::draw_text(draw, section, 262.f, look::baseline_in_line(section, 46.f + TOP, 58.f), names[tab]);

    // The disc.
    look::draw_box(draw, Box{.rect = at(570.f, 16.f, 400.f, 62.f), .radius = 18.f, .background = ROYAL,
                             .border = 4.f, .border_color = rgb(0x3348c0)});
    const ImVec2 disc(588.f + 17.f, 47.f + TOP);
    look::circle(draw, disc, 17.f, rgb(0xffffff));
    look::circle(draw, disc, 14.f, rgb(0xffd84a));
    look::circle(draw, disc, 4.85f, rgb(0xffffff));
    look::circle(draw, disc, 3.27f, ROYAL);
    const TextStyle disc_label = style(Face::Dosis700, 21, 0.08f, rgb(0xc8d4ff));
    look::draw_text(draw, disc_label, 634.f, look::baseline_centered(disc_label, 47.f + TOP), text(Text::DiscBox));
    TextStyle code = style(Face::Baloo800, 38, 0.05f, YELLOW);
    code.stroke = 5.f;
    code.stroke_color = rgb(0x6a3d00);
    const std::string& known = disc_code_.empty() ? prepared_.game : disc_code_;
    const std::string game = known.empty() ? "\xE2\x80\x94" : known;  // —
    look::draw_text(draw, code, 946.f - look::text_width(code, game), look::baseline_in_line(code, 28.f + TOP, 38.f),
                    game);

    // The state of the game, and the languages.
    look::draw_box(draw, Box{.rect = at(570.f, 86.f, 400.f, 56.f), .radius = 16.f, .background = rgb(0x4a8fe0),
                             .border = 4.f, .border_color = rgb(0x9ad2fa)});
    const TextStyle language = style(Face::Dosis700, 20, 0.05f, 0);
    const bool spanish = texts::language() == texts::Language::Spanish;
    const struct {
        const char* name;
        const char* code;
        bool chosen;
    } languages[] = {{"Espa\xC3\xB1ol", "es", spanish}, {"English", "en", !spanish}};
    float right = 958.f;
    for (int i = 1; i >= 0; --i) {
        TextStyle lettering = language;
        lettering.color = languages[i].chosen ? ROYAL : rgb(0xc8d4ff);
        const float width = look::text_width(lettering, languages[i].name) + 28.f;
        const Rect button = at(right - width, 96.f, width, 36.f);
        look::draw_box(draw, Box{.rect = button, .radius = 10.f,
                                 .background = languages[i].chosen ? rgb(0xdff1ff) : rgb(0x1b2f8f),
                                 .shadows = {{{.y = 3.f, .color = rgb(0x16257a)}}}});
        look::draw_text(draw, lettering, button.x + 14.f, look::baseline_in_line(lettering, button.y, 36.f),
                        languages[i].name);
        if (clicked(button.x, button.y, button.w, button.h)) {
            later([this, code = languages[i].code] {
                settings_.set(settings::LANGUAGE, code);
                save_settings();
                apply_language();
                mods_ = mods::load(package_.mods, package_mods());
            });
        }
        right = button.x - 8.f;
    }
    const Preparation::State state = preparation_.state();
    const char* status = text(Text::StatusNotPrepared);
    ImU32 dot = rgb(0xffd84a);
    if (state == Preparation::State::Running) {
        status = text(Text::StatusPreparing);
        const float blink = still_ ? 0.f : 0.5f - 0.5f * std::cos(static_cast<float>(seconds) * 5.f);
        dot = look::mix(rgb(0xffd84a), rgb(0x2a5fb0), blink * 0.7f);
    } else if (prepared_is_current()) {
        status = text(Text::StatusReady);
        dot = rgb(0x5fd84a);
    } else if (state == Preparation::State::Failed) {
        status = text(Text::StatusFailed);
        dot = rgb(0xff6b6b);
    }
    const Rect pill = at(582.f, 96.f, right - 582.f, 36.f);
    look::fill(draw, pill, 10.f, rgb(0x2a5fb0));
    const TextStyle state_text = style(Face::Dosis700, 19, 0.06f, rgb(0xffffff));
    const float group = 16.f + 8.f + look::text_width(state_text, status);
    const float start = pill.center_x() - group * 0.5f;
    look::circle(draw, ImVec2(start + 8.f, pill.center_y()), 8.f, rgb(0xffffff));
    look::circle(draw, ImVec2(start + 8.f, pill.center_y()), 6.f, dot);
    look::draw_text(draw, state_text, start + 24.f, look::baseline_centered(state_text, pill.center_y()), status);

    // Esc to leave (or to stop the preparation).
    look::draw_box(draw, Box{.rect = at(992.f, 16.f, 272.f, 54.f), .radius = 27.f, .background = ROYAL,
                             .border = 4.f, .border_color = rgb(0x7fe0d0)});
    const TextStyle key = style(Face::Baloo800, 19, 0.f, rgb(0xffffff));
    const float key_width = look::text_width(key, "Esc") + 28.f;
    look::draw_box(draw, Box{.rect = at(1010.f, 28.f, key_width, 30.f), .radius = 14.f, .background = rgb(0x4f6fe0),
                             .shadows = {{{.y = 3.f, .color = rgb(0x101a5c)}}}});
    look::draw_text(draw, key, 1024.f, look::baseline_in_line(key, 28.f + TOP, 30.f), "Esc");
    const TextStyle arrow = style(Face::Baloo800, 24, 0.f, rgb(0xffffff));
    const float arrow_x = 1010.f + key_width + 12.f;
    look::draw_text(draw, arrow, arrow_x, look::baseline_centered(arrow, 43.f + TOP), "\xE2\x86\x92");  // →
    const TextStyle leave = style(Face::Dosis700, 23, 0.08f, rgb(0xffffff));
    const bool preparing = state == Preparation::State::Running;
    look::draw_text(draw, leave, arrow_x + look::text_width(arrow, "\xE2\x86\x92") + 12.f,
                    look::baseline_centered(leave, 43.f + TOP), preparing ? text(Text::Cancel) : text(Text::Exit));
    if (clicked(992.f, 16.f + TOP, 272.f, 54.f)) {
        later([this, preparing] {
            if (preparing) {
                preparation_.cancel();
            } else {
                quit_ = true;
            }
        });
    }

    // The version, in two halves.
    const Rect version = at(1016.f, 86.f, 248.f, 50.f);
    look::fill(draw, version, 14.f, rgb(0xffffff));
    look::fill(draw, {1019.f, 89.f + TOP, 121.f, 44.f}, Corners(11.f, 0.f, 0.f, 11.f), rgb(0x2f62c4));
    look::fill(draw, {1140.f, 89.f + TOP, 121.f, 44.f}, Corners(0.f, 11.f, 11.f, 0.f), rgb(0x7a4fc8));
    draw_centered(draw, style(Face::Dosis700, 21, 0.08f, rgb(0xffffff)), 1079.5f, 111.f + TOP, text(Text::VersionBox));
    TextStyle number = style(Face::Baloo800, 28, 0.f, YELLOW);
    number.stroke = 4.f;
    number.stroke_color = rgb(0x3a1f6e);
    if (look::text_width(number, PACKAGE_VERSION) > 110.f) {
        number.size = 19;  // a long version, in smaller numbers
    }
    look::draw_text(draw, number, 1200.5f - look::text_width(number, PACKAGE_VERSION) * 0.5f,
                    look::baseline_in_line(number, 111.f + TOP - number.size * 0.5f, static_cast<float>(number.size)),
                    PACKAGE_VERSION);
}

void App::draw_tabs(ImDrawList* draw, double seconds) {
    // At the left of the panel, one under the other; the chosen one wider and joined to it.
    for (int i = 0; i < 4; ++i) {
        const bool chosen = i == static_cast<int>(tab_);
        const float width = chosen ? 104.f : 86.f;
        const Rect tab = at(244.f - (chosen ? 0.f : 6.f) - width, 258.f + static_cast<float>(i) * 76.f, width, 68.f);
        const Slide slide = slide_in(seconds, 0.7f + static_cast<float>(i) * 0.08f, 0.4f, still_);
        const look::Moved moved(draw, slide.dx, 0.f, slide.opacity);
        const Corners corners(16.f, 0.f, 0.f, 16.f);
        look::fill(draw, tab, corners, chosen ? TEAL : ORANGE);
        look::inset_bottom_shadow(draw, tab, corners, 4.f, rgb(0x000028, 0.12f));
        const Rect square{tab.x + 10.f, tab.y + 8.f, 52.f, 52.f};
        look::fill(draw, square, 12.f, chosen ? rgb(0x4fa8b4) : rgb(0xcf6712));
        draw_centered(draw, style(Face::Baloo800, 30, 0.f, rgb(0xffffff)), square.center_x(), square.center_y(),
                      TAB_SYMBOLS[i]);
        if (clicked(tab.x, tab.y, tab.w, tab.h)) {
            later([this, i] { choose_tab(static_cast<Tab>(i)); });
        }
    }
}

void App::draw_panel(ImDrawList* draw, double seconds, const std::vector<Row>& rows) {
    // It pops in (hmPop): from 85% of its size, a little past it, and back.
    const Rect panel = at(236.f, 196.f, 1004.f, 370.f);
    float opacity = 1.f;
    float grow = 1.f;
    if (!still_) {
        const float part = look::progress(seconds, 0.6f, 0.45f);
        if (part < 0.7f) {
            const float done = look::ease_out(part / 0.7f);
            grow = 0.85f + 0.17f * done;
            opacity = done;
        } else {
            grow = 1.02f - 0.02f * look::ease_out((part - 0.7f) / 0.3f);
        }
    }
    const look::Moved moved(draw, 0.f, 0.f, opacity, grow, ImVec2(panel.center_x(), panel.center_y()));
    look::draw_box(draw, Box{.rect = panel, .radius = 22.f, .background = NIGHT, .border = 8.f, .border_color = TEAL,
                             .shadows = {{{.spread = 2.f, .color = rgb(0x3f9fa8)},
                                          {.y = 6.f, .color = rgb(0x000028, 0.3f)}}}});

    // The panel's title, on a tab over its top border.
    std::string title = "Magical Melody";
    if (tab_ == Tab::Options) {
        title = text(Text::PanelPicture);
    } else if (tab_ == Tab::Mods) {
        const auto on =
            std::ranges::count_if(mods_, [&](const mods::Mod& mod) { return settings_.mod_enabled(mod.id); });
        title = texts::format(Text::PanelModsOn, static_cast<int>(on), static_cast<int>(mods_.size()));
    } else if (tab_ == Tab::Controls) {
        title = text(Text::PanelDevices);
    }
    const TextStyle title_text = style(Face::Dosis700, 26, 0.14f, ROYAL);
    const float title_width = std::max(260.f, look::text_width(title_text, title) + 56.f + 6.f);
    const Rect title_tab = at(280.f, 170.f, title_width, 44.f);
    look::draw_box(draw, Box{.rect = title_tab, .radius = Corners(16.f, 16.f, 6.f, 6.f), .background = rgb(0xb4e2fb),
                             .background_bottom = rgb(0x74b8ec), .border = 3.f, .border_color = rgb(0xd6f0ff)});
    look::draw_text(draw, title_text, title_tab.x + 31.f, look::baseline_centered(title_text, title_tab.center_y()),
                    title);

    if (tab_ == Tab::Mods) {
        draw_mods(draw, seconds);
        draw_scroll_bar(draw);
    } else {
        draw_rows(draw, seconds, rows, tab_ == Tab::Game ? 640.f : 922.f);
        if (tab_ == Tab::Game) {
            draw_art(draw);
        }
        draw_scroll_bar(draw);
    }
}

void App::draw_cursor(ImDrawList* draw, double seconds, float row_x, float row_center) {
    // A note at the left of the chosen row, that sways (hmPoint).
    TextStyle cursor = style(Face::Baloo800, 44, 0.f, YELLOW);
    cursor.stroke = 5.f;
    cursor.stroke_color = NIGHT;
    float sway = 0.f;
    if (!still_) {
        const float phase = static_cast<float>(std::fmod(seconds, 0.7) / 0.7);
        sway = 7.f * (phase < 0.5f ? look::ease_in_out(phase / 0.5f) : 1.f - look::ease_in_out((phase - 0.5f) / 0.5f));
    }
    look::draw_text(draw, cursor, row_x + 3.f - 54.f + sway, look::baseline_in_line(cursor, row_center - 26.f, 52.f),
                    NOTE);
}

void App::draw_rows(ImDrawList* draw, double seconds, const std::vector<Row>& rows, float width) {
    const float left = 262.f;
    const float top = 234.f + TOP;
    const float height = 310.f;
    // Where each row is in the list; the list scrolls so that the chosen one shows.
    std::vector<float> places;
    float y = 0.f;
    for (const Row& row : rows) {
        places.push_back(y);
        y += (row.play ? 72.f : 52.f) + 8.f;
    }
    const float total = std::max(0.f, y - 8.f);
    if (follow_selection_ && selected_ >= 0 && selected_ < static_cast<int>(rows.size())) {
        const float row_top = places[static_cast<size_t>(selected_)];
        const float row_bottom = row_top + (rows[static_cast<size_t>(selected_)].play ? 72.f : 52.f);
        scroll_ = std::clamp(scroll_, row_bottom - height, row_top);
    }
    scroll_ = std::clamp(scroll_, 0.f, std::max(0.f, total - height));
    list_total_ = total;

    const TextStyle arrows = style(Face::Baloo800, 18, 0.f, rgb(0xffffff));
    int hovered_row = -1;
    draw->PushClipRect(look::to_window(ImVec2(150.f, top)), look::to_window(ImVec2(1218.f, top + height)), true);
    for (size_t i = 0; i < rows.size(); ++i) {
        const Row& row = rows[i];
        const bool chosen = static_cast<int>(i) == selected_;
        const Rect box = {left, top + places[i] - scroll_, width, row.play ? 72.f : 52.f};
        const Slide slide = slide_in(seconds, 1.1f + static_cast<float>(i) * 0.09f, 0.35f, still_);
        const look::Moved moved(draw, slide.dx, 0.f, slide.opacity);

        ImU32 background = chosen ? rgb(0x3e9199) : rgb(0x2d5f95);
        ImU32 border = chosen ? rgb(0xa6ece4) : rgb(0x5c8fc2);
        if (row.play) {
            background = rgb(0xffb850);
            border = chosen ? rgb(0xffffff) : rgb(0xffd89a);
        }
        look::draw_box(draw, Box{.rect = box, .radius = 14.f, .background = background,
                                 .background_bottom = row.play ? rgb(0xf07f1e) : 0, .border = 3.f,
                                 .border_color = border});
        if (row.progress >= 0.f) {
            const Rect inside = box.inset(3.f);
            look::fill(draw, {inside.x, inside.y, inside.w * std::clamp(row.progress, 0.f, 1.f), inside.h},
                       Corners(11.f, 0.f, 0.f, 11.f), rgb(0xffffff, 0.22f));
        }
        const float center = box.center_y();
        const Rect square{box.x + 13.f, center - 19.f, 38.f, 38.f};
        look::fill(draw, square, 10.f, row.play ? rgb(0xc95f10) : TAB_COLORS[static_cast<size_t>(tab_)]);
        draw_centered(draw, style(Face::Baloo800, 22, 0.f, rgb(0xffffff)), square.center_x(), square.center_y(),
                      row.symbol);

        TextStyle label = style(Face::Dosis700, row.play ? 34 : 26, 0.07f, ROW_TEXT);
        label.shadow_y = 2.f;
        label.shadow_color = ROW_SHADOW;
        if (row.play) {
            label.stroke = 6.f;
            label.stroke_color = rgb(0x8a3c00);
            // The shadow of the outlined letters, as it looks over the orange.
            label.shadow_color = look::over(ROW_SHADOW, rgb(0xf89c37));
        }
        look::draw_text(draw, label, box.x + 65.f, look::baseline_centered(label, center), row.label);

        // At the right: the value in its chip, between arrows when it can change.
        const float right_arrow = box.right() - 17.f - look::text_width(arrows, RIGHT_ARROW);
        const float chip = draw_chip(draw, row.value, row.tone, 20, right_arrow - 8.f, center);
        if (chosen && row.arrows) {
            const float baseline = look::baseline_centered(arrows, center);
            look::draw_text(draw, arrows, right_arrow, baseline, RIGHT_ARROW);
            look::draw_text(draw, arrows, chip - 8.f - look::text_width(arrows, LEFT_ARROW), baseline, LEFT_ARROW);
        }
        if (chosen) {
            draw_cursor(draw, seconds, box.x, center);
        }
        if (mouse_moved_ && hovered(box.x, box.y, box.w, box.h)) {
            hovered_row = static_cast<int>(i);
        }
        if (clicked(box.x, box.y, box.w, box.h)) {
            later([this, i, activate = row.activate] {
                selected_ = static_cast<int>(i);
                if (activate) {
                    activate();
                }
            });
        }
    }
    draw->PopClipRect();
    if (hovered_row >= 0) {
        selected_ = hovered_row;
        follow_selection_ = false;  // a row half out of view stays where the mouse found it
    }
}

void App::draw_mods(ImDrawList* draw, double seconds) {
    const float top = 234.f + TOP;
    const float height = 310.f;
    if (mods_.empty()) {
        return;
    }
    selected_ = std::clamp(selected_, 0, static_cast<int>(mods_.size()) - 1);
    const float total = static_cast<float>(mods_.size()) * 76.f - 8.f;
    const float row_top = static_cast<float>(selected_) * 76.f;
    if (follow_selection_) {
        scroll_ = std::clamp(scroll_, row_top + 68.f - height, row_top);
    }
    scroll_ = std::clamp(scroll_, 0.f, std::max(0.f, total - height));
    list_total_ = total;

    const TextStyle name = [] {
        TextStyle lettering = style(Face::Dosis700, 23, 0.06f, ROW_TEXT);
        lettering.shadow_y = 2.f;
        lettering.shadow_color = ROW_SHADOW;
        return lettering;
    }();
    const TextStyle about = style(Face::Dosis700, 16, 0.06f, rgb(0xd6e4ff));
    int hovered_row = -1;
    draw->PushClipRect(look::to_window(ImVec2(150.f, top)), look::to_window(ImVec2(1218.f, top + height)), true);
    for (size_t i = 0; i < mods_.size(); ++i) {
        const mods::Mod& mod = mods_[i];
        const bool chosen = static_cast<int>(i) == selected_;
        const bool on = settings_.mod_enabled(mod.id);
        const Rect box = {262.f, top + static_cast<float>(i) * 76.f - scroll_, 360.f, 68.f};
        look::draw_box(draw, Box{.rect = box, .radius = 14.f, .background = chosen ? rgb(0x2a767f) : rgb(0x2d5f95),
                                 .border = 3.f, .border_color = chosen ? rgb(0xa6ece4) : rgb(0x5c8fc2)});
        // The switch.
        const Rect knob_track{box.x + 15.f, box.y + 19.f, 52.f, 30.f};
        look::draw_box(draw, Box{.rect = knob_track, .radius = 15.f, .background = on ? rgb(0x3fae3a) : rgb(0x6b7690),
                                 .border = 2.f, .border_color = rgb(0xffffff)});
        const ImVec2 knob(knob_track.x + 2.f + (on ? 24.f : 2.f) + 11.f, knob_track.y + 15.f);
        look::circle(draw, knob, 11.f, rgb(0xffffff));
        // The name and, under it, the version and authors.
        const float column = 26.45f + 19.2f;
        const float text_top = box.center_y() - column * 0.5f;
        const bool manifest = !(mod.version.empty() && mod.description.empty());
        look::draw_text(draw, name, box.x + 79.f, look::baseline_in_line(name, text_top, 26.45f),
                        look::fit(name, mod.name, 266.f));
        const std::string version = manifest ? "v" + mod.version + " \xC2\xB7 " + mod.authors : text(Text::NoManifest);
        look::draw_text(draw, about, box.x + 79.f, look::baseline_in_line(about, text_top + 26.45f, 19.2f),
                        look::fit(about, version, 266.f));
        if (chosen) {
            draw_cursor(draw, seconds, box.x, box.center_y());
        }
        if (mouse_moved_ && hovered(box.x, box.y, box.w, box.h)) {
            hovered_row = static_cast<int>(i);
        }
        if (clicked(knob_track.x, knob_track.y, knob_track.w, knob_track.h)) {
            later([this, i, id = mod.id] {
                selected_ = static_cast<int>(i);
                settings_.set_mod_enabled(id, !settings_.mod_enabled(id));
                save_settings();
            });
        } else if (clicked(box.x, box.y, box.w, box.h)) {
            later([this, i] { selected_ = static_cast<int>(i); });
        }
    }
    draw->PopClipRect();
    if (hovered_row >= 0 && hovered_row != selected_) {
        selected_ = hovered_row;
        follow_selection_ = false;
        description_scroll_ = 0.f;
    }
    draw_mod_details(draw, mods_[static_cast<size_t>(selected_)]);
}

void App::draw_mod_details(ImDrawList* draw, const mods::Mod& mod) {
    const Rect box = at(634.f, 234.f, 550.f, 310.f);
    look::draw_box(draw, Box{.rect = box, .radius = 14.f, .background = rgb(0x26328f), .border = 3.f,
                             .border_color = rgb(0x4a5fd0)});
    const Rect inside = box.inset(15.f, 19.f, 15.f, 19.f);
    const bool on = settings_.mod_enabled(mod.id);
    const bool manifest = !(mod.version.empty() && mod.description.empty());

    // The name and whether it is on, in a row as tall as the taller of the two.
    const float header = std::max(28.6f, look::line_height(style(Face::Dosis700, 19, 0.05f, 0)) + 6.f);
    const float chip = draw_chip(draw, on ? text(Text::ModOn) : text(Text::ModOff), on ? Tone::On : Tone::Off, 19,
                                 inside.right(), inside.y + header * 0.5f);
    const TextStyle title = style(Face::Baloo800, 26, 0.f, rgb(0xffffff));
    look::draw_text(draw, title, inside.x, look::baseline_in_line(title, inside.y + (header - 28.6f) * 0.5f, 28.6f),
                    look::fit(title, mod.name, chip - inside.x - 10.f));

    // From the bottom up: its settings, its warning, and its description in the room left.
    float bottom = inside.bottom();
    const TextStyle setting_text = style(Face::Dosis700, 18, 0.05f, rgb(0xffffff));
    const TextStyle arrows = style(Face::Baloo800, 18, 0.f, rgb(0xffffff));
    const TextStyle value_text = style(Face::Dosis700, 19, 0.05f, rgb(0x173f94));
    if (mod.settings.empty()) {
        const TextStyle none = style(Face::Dosis700, 16, 0.05f, rgb(0xc8d4ff));
        bottom -= look::line_height(none);
        look::draw_text(draw, none, inside.x, look::baseline_in_line(none, bottom, look::line_height(none)),
                        text(Text::ModNoSettings));
    }
    for (auto setting = mod.settings.rbegin(); setting != mod.settings.rend(); ++setting) {
        bottom -= 34.f;
        const float center = bottom + 17.f;
        const int value = settings_.number(setting->key.c_str(), setting->fallback, setting->minimum, setting->maximum);
        const std::string shown = std::to_string(value) + "%";
        const float value_width = std::max(52.f, look::text_width(value_text, shown) + 20.f);
        const float value_height = look::line_height(value_text) + 6.f;
        const Rect value_box{inside.right() - value_width, center - value_height * 0.5f, value_width, value_height};
        look::fill(draw, value_box, 10.f, rgb(0xcfe2ff));
        draw_centered(draw, value_text, value_box.center_x(), center, shown);
        const float arrow_width = look::text_width(arrows, RIGHT_ARROW) + 8.f;
        const Rect more{value_box.x - 10.f - arrow_width, center - 15.f, arrow_width, 30.f};
        look::draw_text(draw, arrows, more.x + 4.f, look::baseline_centered(arrows, center), RIGHT_ARROW);
        look::draw_text(draw, setting_text, inside.x, look::baseline_centered(setting_text, center), setting->label);
        const Rect less{inside.x + look::text_width(setting_text, setting->label) + 10.f, center - 15.f,
                        look::text_width(arrows, LEFT_ARROW) + 8.f, 30.f};
        look::draw_text(draw, arrows, less.x + 4.f, look::baseline_centered(arrows, center), LEFT_ARROW);
        // The bar, filled from the left as far as the value goes.
        const Rect bar{less.right() + 10.f, center - 7.f, more.x - 10.f - less.right() - 10.f, 14.f};
        look::draw_box(draw, Box{.rect = bar, .radius = 7.f, .background = rgb(0x0e0a44), .border = 2.f,
                                 .border_color = rgb(0xd6f0ff)});
        const float share = static_cast<float>(value - setting->minimum) /
                            static_cast<float>(std::max(1, setting->maximum - setting->minimum));
        const Rect fill_bar = bar.inset(2.f);
        look::fill(draw, {fill_bar.x, fill_bar.y, fill_bar.w * share, fill_bar.h},
                   share >= 0.99f ? Corners(5.f) : Corners(5.f, 0.f, 0.f, 5.f), BAR_TOP, BAR_BOTTOM);
        const auto change = [this, key = setting->key, step = setting->step, minimum = setting->minimum,
                             maximum = setting->maximum, value](int direction) {
            later([this, key, step, minimum, maximum, value, direction] {
                settings_.set(key.c_str(), std::clamp(value + step * direction, minimum, maximum));
                save_settings();
            });
        };
        if (clicked(less.x, less.y, less.w, less.h)) {
            change(-1);
        }
        if (clicked(more.x, more.y, more.w, more.h)) {
            change(1);
        }
        bottom -= 8.f;
    }
    if (mod.settings.empty()) {
        bottom -= 8.f;
    }
    if (!mod.warning.empty() && on) {
        const TextStyle warning = style(Face::Dosis700, 16, 0.02f, rgb(0x5c4000));
        const std::vector<std::string> lines = look::wrap(warning, mod.warning, inside.w - 24.f);
        const float line = 20.8f;
        const float warning_height = static_cast<float>(lines.size()) * line + 12.f;
        bottom -= warning_height;
        look::fill(draw, {inside.x, bottom, inside.w, warning_height}, 10.f, rgb(0xffe9a0));
        for (size_t i = 0; i < lines.size(); ++i) {
            const float line_top = bottom + 6.f + static_cast<float>(i) * line;
            look::draw_text(draw, warning, inside.x + 12.f, look::baseline_in_line(warning, line_top, line), lines[i]);
        }
        bottom -= 8.f;
    }
    const TextStyle description = style(Face::Dosis600, 17, 0.03f, rgb(0xe8eeff));
    const float line = 17.f * 1.35f;
    const float description_top = inside.y + header + 8.f;
    const std::vector<std::string> lines =
        look::wrap(description, manifest && !mod.description.empty() ? mod.description : text(Text::NoDescription),
                   inside.w);
    const float room = std::max(0.f, bottom - description_top);
    description_scroll_ = std::clamp(description_scroll_, 0.f, std::max(0.f, lines.size() * line - room));
    draw->PushClipRect(look::to_window(ImVec2(inside.x - 2.f, description_top)),
                       look::to_window(ImVec2(inside.right() + 2.f, description_top + room)), true);
    for (size_t i = 0; i < lines.size(); ++i) {
        const float line_top = description_top + static_cast<float>(i) * line - description_scroll_;
        look::draw_text(draw, description, inside.x, look::baseline_in_line(description, line_top, line), lines[i]);
    }
    draw->PopClipRect();
}

void App::draw_art(ImDrawList* draw) {
    // The design's box for the art: diagonal stripes in a dashed border. It stays empty; the game's
    // own logo is in the game, and the launcher, the project's face, shows nothing of the game.
    const Rect box = at(914.f, 234.f, 270.f, 310.f);
    look::stripes(draw, box, 14.f, rgb(0x221c78), rgb(0x1c1668), 12.f, 6.f);
    look::dashed_border(draw, box, 14.f, 3.f, rgb(0x6a7ce0));
}

void App::draw_scroll_bar(ImDrawList* draw) {
    // Its handle shows which part of the list is in view; it is dragged, its track is clicked, and
    // its arrows move the list a step.
    const float top_arrow = 240.5f + TOP;
    const float bottom_arrow = 537.5f + TOP;
    const TextStyle arrows = style(Face::Dosis700, 13, 0.f, rgb(0xd6f0ff));
    draw_centered(draw, arrows, 1207.f, top_arrow, "\xE2\x96\xB2");     // ▲
    draw_centered(draw, arrows, 1207.f, bottom_arrow, "\xE2\x96\xBC");  // ▼
    const Rect track = at(1199.f, 251.f, 16.f, 276.f);
    look::draw_box(draw, Box{.rect = track, .radius = 8.f, .background = rgb(0x0e0a44), .border = 2.f,
                             .border_color = rgb(0xd6f0ff)});

    const float hidden = std::max(0.f, list_total_ - LIST_HEIGHT);
    const float run = track.h - 4.f;
    const float handle = hidden > 0.f ? std::max(34.f, run * LIST_HEIGHT / list_total_) : run;
    if (hidden > 0.f) {
        if (clicked(1195.f, top_arrow - 10.f, 24.f, 20.f)) {
            scroll_ -= WHEEL_STEP;
            follow_selection_ = false;
        }
        if (clicked(1195.f, bottom_arrow - 10.f, 24.f, 20.f)) {
            scroll_ += WHEEL_STEP;
            follow_selection_ = false;
        }
        if (clicked(track.x - 4.f, track.y, track.w + 8.f, track.h)) {
            dragging_bar_ = true;
        }
        if (!ImGui::IsMouseDown(ImGuiMouseButton_Left)) {
            dragging_bar_ = false;
        }
        if (dragging_bar_) {
            // The middle of the handle goes where the mouse is.
            const float mouse = look::to_design(ImGui::GetIO().MousePos).y;
            const float place = (mouse - track.y - 2.f - handle * 0.5f) / std::max(1.f, run - handle);
            scroll_ = std::clamp(place, 0.f, 1.f) * hidden;
            follow_selection_ = false;
        }
        scroll_ = std::clamp(scroll_, 0.f, hidden);
    } else {
        dragging_bar_ = false;
    }
    const float place = hidden > 0.f ? scroll_ / hidden : 0.f;
    look::fill(draw, {track.x + 3.f, track.y + 2.f + (run - handle) * place, 10.f, handle}, 6.f, BAR_TOP, BAR_BOTTOM);
}

void App::draw_message(ImDrawList* draw, double seconds, const std::string& message) {
    // The box at the bottom, like the game's dialogue boxes, that comes in from the left (hmMsg).
    const float done = still_ ? 1.f : look::ease_out(look::progress(seconds, 1.f, 0.5f));
    const look::Moved moved(draw, -1236.f * 1.02f * (1.f - done), 0.f);
    look::draw_box(draw, Box{.rect = at(0.f, 584.f, 1236.f, 120.f), .radius = Corners(0.f, 60.f, 60.f, 0.f),
                             .background = rgb(0xffe594), .border = Sides(6.f, 6.f, 6.f, 0.f),
                             .border_color = rgb(0xf29a1f), .shadows = {{{.y = 5.f, .color = rgb(0x000028, 0.25f)}}},
                             .inner_ring = 3.f, .inner_ring_color = rgb(0xfff3c4)});
    TextStyle words = style(Face::Dosis700, 28, 0.1f, rgb(0x2a2560));
    std::vector<std::string> lines = look::wrap(words, message, 1104.f, true);
    if (lines.size() > 2) {
        // A long message (an error), in smaller letters so that it fits.
        words.size = 23;
        words.spacing = 0.06f;
        lines = look::wrap(words, message, 1104.f, true);
    }
    const float line = static_cast<float>(words.size) * 1.35f;
    const float top = 644.f + TOP - static_cast<float>(lines.size()) * line * 0.5f;
    for (size_t i = 0; i < lines.size(); ++i) {
        look::draw_text(draw, words, 56.f, look::baseline_in_line(words, top + static_cast<float>(i) * line, line),
                        lines[i]);
    }
}
