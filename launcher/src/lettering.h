// The launcher's lettering, as its design writes it (fonts/README.md): Baloo 2 ExtraBold for the
// titles, the keys and the game's codes and numbers, and Dosis Bold and SemiBold for everything else.
// The symbols that neither font has (the notes, the arrows, the gear, the flower...) come, as in a
// browser, from the computer's own fonts: Arial and Segoe UI Symbol on Windows, Liberation Sans and
// DejaVu Sans elsewhere; and from DejaVu Sans, which comes inside the launcher, when those are missing.
//
// Las letras del launcher, como las escribe su diseño: Baloo 2 ExtraBold para los títulos, las
// teclas y los códigos y números del juego, y Dosis Bold y SemiBold para todo lo demás. Los
// símbolos que no tienen esas fuentes (las notas, las flechas, el engrane, la flor...) salen, como
// en un navegador, de las fuentes de la computadora; y de DejaVu Sans, que viene dentro del
// launcher, cuando faltan.
#pragma once

#include <imgui.h>

namespace lettering {

enum class Face { Dosis600, Dosis700, Baloo800 };

// Makes every font the design uses, `scale` times its size in the design (1 at 100% of the
// screen's scaling). True when they changed: the renderer's font texture must then be made again.
bool build(float scale);

// The font of `face` at `size` pixels of the design, one of the sizes the design uses (the
// nearest one otherwise). Its glyphs are already `scale` times bigger.
ImFont* font(Face face, int size);

// How many times bigger than ImGui measures them the glyph of `c` is drawn in that font. ImGui keeps
// the size of a font in whole pixels; each font is made at the whole size under its exact one, its
// glyphs rasterized at the exact one, and the launcher draws them that much bigger.
float glyph_scale(Face face, int size, ImWchar c);

// How much farther apart `left` and `right` go in the face (its kerning), in ems.
float kerning(Face face, ImWchar left, ImWchar right);

// The face's ascent and descent, in ems: where a line of it puts the baseline.
float ascent(Face face);
float descent(Face face);

// How much a browser would thicken `c` (it comes from a symbol font that has no bold, so the browser
// makes it bold itself), in pixels of the design at `size`: 0 for most characters.
float thickening(ImWchar c, int size);

}  // namespace lettering
