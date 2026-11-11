// The launcher's look, after the game's own: the sky, the hills and the fence of the title
// screen, a wooden sign with the name, orange buttons like the box of the date, panels like the
// dialogue boxes, and rounded lettering. Everything is drawn with shapes and the launcher's own
// fonts (fonts/README.md); nothing comes from the game.
//
// El aspecto del launcher, a partir del propio juego: el cielo, las colinas y la cerca de la
// pantalla de título, un letrero de madera con el nombre, botones naranjas como el recuadro de
// la fecha, paneles como los cuadros de diálogo y letras redondeadas. Todo se dibuja con figuras
// y las fuentes del launcher; nada sale del juego.
//
// The design is for a window of 1280x720; px() turns its sizes into the window's.
#pragma once

#include <imgui.h>

#include <string>
#include <vector>

namespace theme {

struct Fonts {
    ImFont* small = nullptr;
    ImFont* body = nullptr;
    ImFont* heading = nullptr;
    ImFont* button = nullptr;
    ImFont* subtitle = nullptr;
    ImFont* title = nullptr;
};
const Fonts& fonts();

// Builds the fonts and the style for a window `scale` times the design's (1 is 1280x720 at
// 100% of the screen's scaling). True when it changed them; the renderer's font texture must
// then be made again (aurora makes it again by itself in the game).
bool set_scale(float scale);
float px(float design_pixels);

// The landscape behind every screen. `seconds` moves the clouds and the notes.
void draw_background(ImDrawList* draw, ImVec2 size, double seconds);
// The wooden sign with the game's name, centred at `center`; `small` on the screens other than
// the main one, and with a ribbon under it that says `ribbon` when it is not small.
void draw_title_sign(ImDrawList* draw, ImVec2 center, bool small, const char* ribbon = nullptr);

enum class ButtonColor { Orange, Green, Gray };
// A button like the box of the date: a rounded orange (or green) bar with white lettering.
bool menu_button(const char* label, ImVec2 size, ButtonColor color = ButtonColor::Orange, bool enabled = true);

// A panel like a dialogue box, from `min` to `max`, with `title` where the speaker's name goes.
// What goes in it is drawn between begin_panel and end_panel, in a child window that scrolls.
void begin_panel(const char* id, ImVec2 min, ImVec2 max, const char* title);
void end_panel();

// A switch; true when it was changed.
bool toggle(const char* id, bool* value);
// A choice among `labels`, with arrows at both ends; the left and right keys (and the
// controller's) change it too. True when it was changed.
bool choice(const char* id, int* index, const std::vector<std::string>& labels, float width);
// A percentage on a bar, from `minimum` to `maximum` by `step`. True when it was changed.
bool percent(const char* id, int* value, int minimum, int maximum, int step, float width);
// The left cell of a row of the options (a table of two columns): the label, with an optional
// hint under it. The right cell holds the control, row_control_width() wide.
void row_label(const char* label, const char* hint = nullptr);
float row_control_width();

void progress_bar(float fraction, ImVec2 size, double seconds);

enum class NoteKind { Info, Warning, Error, Success };
// A box with a message, as wide as the space left.
void note(NoteKind kind, const std::string& text);

// Text with an outline of `thickness` pixels, as the game writes its titles.
void outlined_text(ImDrawList* draw, ImFont* font, float size, ImVec2 position, ImU32 fill, ImU32 outline,
                   float thickness, const char* text);
// Text of the body, wrapped at the space left, in the panel's text color (or `color`).
void paragraph(const std::string& text, ImU32 color = 0);
void heading(const std::string& text);

// Colors used outside the theme.
inline constexpr ImU32 TEXT = IM_COL32(58, 50, 46, 255);
inline constexpr ImU32 TEXT_SOFT = IM_COL32(112, 104, 98, 255);
inline constexpr ImU32 NAME_RED = IM_COL32(204, 52, 42, 255);
inline constexpr ImU32 WHITE = IM_COL32(255, 255, 255, 255);
inline constexpr ImU32 GREEN = IM_COL32(64, 168, 58, 255);
inline constexpr ImU32 ORANGE = IM_COL32(236, 134, 40, 255);
inline constexpr ImU32 LINE = IM_COL32(196, 207, 219, 255);

}  // namespace theme
