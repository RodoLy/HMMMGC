// See lettering.h.

#include "lettering.h"

#include "kerning.h"

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <map>
#include <optional>
#include <string_view>
#include <utility>
#include <vector>

// The fonts inside the program (CMakeLists.txt turns launcher/fonts into these arrays).
extern const unsigned char FONT_BALOO2_EXTRABOLD[];
extern const size_t FONT_BALOO2_EXTRABOLD_size;
extern const unsigned char FONT_DOSIS_BOLD[];
extern const size_t FONT_DOSIS_BOLD_size;
extern const unsigned char FONT_DOSIS_SEMIBOLD[];
extern const size_t FONT_DOSIS_SEMIBOLD_size;
extern const unsigned char FONT_DEJAVU_SYMBOLS[];
extern const size_t FONT_DEJAVU_SYMBOLS_size;

namespace lettering {
namespace {

// Which characters a size of the design needs, so that the big ones take little room.
enum class Glyphs {
    Text,     // Latin, the punctuation and the symbols
    Ascii,    // the keys and the numbers, and the symbols
    Letters,  // the names of the tabs
    Code,     // the disc's code
    Symbols,  // the symbols alone
};

struct Size {
    Face face;
    int size;
    Glyphs glyphs;
};

// Every size the design writes with.
constexpr Size SIZES[] = {
    {Face::Dosis700, 13, Glyphs::Symbols},  // the arrows of the scroll bar
    {Face::Dosis700, 15, Glyphs::Text},     // the window's title
    {Face::Dosis700, 16, Glyphs::Text},     // a mod's version and warning
    {Face::Dosis600, 17, Glyphs::Text},     // a mod's description
    {Face::Dosis700, 18, Glyphs::Text},     // a mod's setting
    {Face::Dosis700, 19, Glyphs::Text},     // the state of the game and of a mod
    {Face::Dosis700, 20, Glyphs::Text},     // the languages, the values of the rows
    {Face::Dosis700, 21, Glyphs::Text},     // "Disc", "version"
    {Face::Dosis700, 23, Glyphs::Text},     // "Exit", the names of the mods
    {Face::Dosis700, 26, Glyphs::Text},     // the rows, the panel's title
    {Face::Dosis700, 28, Glyphs::Text},     // the message at the bottom
    {Face::Dosis700, 34, Glyphs::Text},     // "Play"
    {Face::Baloo800, 18, Glyphs::Ascii},    // "L", "R" and the arrows of the values
    {Face::Baloo800, 19, Glyphs::Ascii},    // "Esc"
    {Face::Baloo800, 22, Glyphs::Symbols},  // the symbols of the rows
    {Face::Baloo800, 24, Glyphs::Symbols},  // "→" and the smallest bubbles
    {Face::Baloo800, 26, Glyphs::Text},     // the name of the chosen mod
    {Face::Baloo800, 28, Glyphs::Ascii},    // the version
    {Face::Baloo800, 30, Glyphs::Symbols},  // the symbols of the tabs
    {Face::Baloo800, 31, Glyphs::Symbols},  // the bubbles
    {Face::Baloo800, 37, Glyphs::Symbols},
    {Face::Baloo800, 38, Glyphs::Code},     // the disc's code
    {Face::Baloo800, 44, Glyphs::Symbols},  // the cursor and the biggest bubbles
    {Face::Baloo800, 58, Glyphs::Letters},  // the name of the tab
    {Face::Baloo800, 62, Glyphs::Symbols},  // the symbol of the tab in the medallion
};

// Latin with the accents of Spanish and of other European languages, and the punctuation.
constexpr ImWchar TEXT_RANGES[] = {0x0020, 0x007E, 0x00A0, 0x017F, 0x2010, 0x2027, 0x2030, 0x205E, 0x20AC, 0x20AC, 0};
constexpr ImWchar ASCII_RANGES[] = {0x0020, 0x007E, 0};
constexpr ImWchar LETTER_RANGES[] = {0x0020, 0x0020, 0x0041, 0x005A, 0x0061, 0x007A, 0x00C0, 0x017F, 0};
constexpr ImWchar CODE_RANGES[] = {0x0020, 0x0020, 0x002D, 0x002E, 0x0030, 0x0039, 0x0041, 0x005A, 0x2014, 0x2014, 0};
constexpr ImWchar SPACE_RANGES[] = {0x0020, 0x0020, 0};

// The symbols of the design that Baloo 2 and Dosis do not have, and those of the buttons of the
// controllers (controls.h).
constexpr std::u32string_view SYMBOLS = U"♪♫♩♬◀▶▲▼→≡✕⚙✿✚◎☞←↑↓○□△";

// The computer's fonts a browser takes those symbols from (after Baloo 2 or Dosis comes the
// generic sans-serif, and then the system's fallback), each with the symbols it gives.
struct SystemFont {
    const char* file;
    std::u32string_view symbols;
    bool bold;  // false: a browser thickens what it takes from it
};
#ifdef _WIN32
constexpr SystemFont SYSTEM_FONTS[] = {
    {"arialbd.ttf", U"♪♫→≡▲▼←↑↓○□", true},
    {"seguisym.ttf", U"♩♬◀▶✕⚙✿✚◎☞△", false},
};
#else
constexpr SystemFont SYSTEM_FONTS[] = {
    {"/usr/share/fonts/truetype/liberation/LiberationSans-Bold.ttf", U"♪♫♩♬→≡▲▼←↑↓○□", true},
    {"/usr/share/fonts/truetype/dejavu/DejaVuSans-Bold.ttf", U"◀▶✕⚙✿✚◎☞△", true},
};
#endif

// A font's vertical metrics (its 'hhea' table), in ems; the descent is positive.
struct Metrics {
    float ascent = 1.f;
    float descent = 0.f;
};

std::optional<Metrics> read_metrics(const unsigned char* data, size_t size) {
    const auto u16 = [&](size_t at) -> uint32_t { return at + 2 <= size ? (data[at] << 8) | data[at + 1] : 0; };
    const auto u32 = [&](size_t at) -> uint32_t { return (u16(at) << 16) | u16(at + 2); };
    if (size < 12) {
        return std::nullopt;
    }
    size_t head = 0;
    size_t hhea = 0;
    for (uint32_t table = 0; table < u16(4); ++table) {
        const size_t record = 12 + table * 16;
        if (record + 16 > size) {
            return std::nullopt;
        }
        if (std::memcmp(data + record, "head", 4) == 0) {
            head = u32(record + 8);
        } else if (std::memcmp(data + record, "hhea", 4) == 0) {
            hhea = u32(record + 8);
        }
    }
    const float units_per_em = static_cast<float>(u16(head + 18));
    if (head == 0 || hhea == 0 || units_per_em <= 0.f) {
        return std::nullopt;
    }
    return Metrics{static_cast<int16_t>(u16(hhea + 4)) / units_per_em,
                   -static_cast<int16_t>(u16(hhea + 6)) / units_per_em};
}

// A font to make the sizes from: one inside the program, or a file of the computer read once.
struct Source {
    std::vector<unsigned char> file;
    const unsigned char* data = nullptr;
    size_t size = 0;
    Metrics metrics;
    std::vector<ImWchar> symbols;  // for a font of symbols, the ones it gives (ImGui's ranges)
    bool bold = true;

    // ImGui sizes a font by its whole height, the ascent and the descent together, in whole pixels:
    // it is made at the whole size under the exact one and drawn bigger (see glyph_scale()).
    float exact(float em) const { return em * (metrics.ascent + metrics.descent); }
    float pixels(float em) const { return std::max(1.f, std::floor(exact(em))); }
    float density(float em) const { return exact(em) / pixels(em); }
};

Source embedded(const unsigned char* data, size_t size) {
    Source source;
    source.data = data;
    source.size = size;
    source.metrics = read_metrics(data, size).value_or(Metrics{});
    return source;
}

std::vector<ImWchar> ranges_of(std::u32string_view characters) {
    std::vector<ImWchar> ranges;
    for (const char32_t character : characters) {
        ranges.push_back(static_cast<ImWchar>(character));
        ranges.push_back(static_cast<ImWchar>(character));
    }
    ranges.push_back(0);
    return ranges;
}

std::optional<Source> system_font(const SystemFont& font) {
    std::filesystem::path path = font.file;
#ifdef _WIN32
    const char* windows = std::getenv("WINDIR");
    path = std::filesystem::path(windows != nullptr ? windows : "C:\\Windows") / "Fonts" / font.file;
#endif
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        return std::nullopt;
    }
    Source source;
    source.file.assign(std::istreambuf_iterator<char>(stream), std::istreambuf_iterator<char>());
    const std::optional<Metrics> metrics = read_metrics(source.file.data(), source.file.size());
    if (!metrics) {
        return std::nullopt;
    }
    source.data = source.file.data();
    source.size = source.file.size();
    source.metrics = *metrics;
    source.symbols = ranges_of(font.symbols);
    source.bold = font.bold;
    return source;
}

struct Fonts {
    Source dosis600 = embedded(FONT_DOSIS_SEMIBOLD, FONT_DOSIS_SEMIBOLD_size);
    Source dosis700 = embedded(FONT_DOSIS_BOLD, FONT_DOSIS_BOLD_size);
    Source baloo800 = embedded(FONT_BALOO2_EXTRABOLD, FONT_BALOO2_EXTRABOLD_size);
    Kerning dosis600_kerning{dosis600.data, dosis600.size};
    Kerning dosis700_kerning{dosis700.data, dosis700.size};
    Kerning baloo800_kerning{baloo800.data, baloo800.size};
    // The computer's symbol fonts that were found, then the launcher's own, for whatever they lack
    // (ImGui keeps the first font that has a character).
    std::vector<Source> symbols;
    std::map<std::pair<Face, int>, ImFont*> made;
    float scale = 0.f;

    Fonts() {
        for (const SystemFont& font : SYSTEM_FONTS) {
            if (std::optional<Source> source = system_font(font)) {
                symbols.push_back(std::move(*source));
            }
        }
        Source own = embedded(FONT_DEJAVU_SYMBOLS, FONT_DEJAVU_SYMBOLS_size);
        own.symbols = ranges_of(SYMBOLS);
        symbols.push_back(std::move(own));
    }

    const Source& of(Face face) const {
        switch (face) {
        case Face::Dosis600: return dosis600;
        case Face::Dosis700: return dosis700;
        case Face::Baloo800: break;
        }
        return baloo800;
    }
};

Fonts& fonts() {
    static Fonts all;
    return all;
}

const ImWchar* ranges_for(Glyphs glyphs) {
    switch (glyphs) {
    case Glyphs::Text: return TEXT_RANGES;
    case Glyphs::Ascii: return ASCII_RANGES;
    case Glyphs::Letters: return LETTER_RANGES;
    case Glyphs::Code: return CODE_RANGES;
    case Glyphs::Symbols: break;
    }
    return SPACE_RANGES;
}

// How much bolder Skia, the browser's drawing library, makes a font that has no bold: a twenty-fourth
// of the size at 9 pixels, down to a thirty-second from 36 pixels up. Half of it goes to each side.
float fake_bold_share(float size) {
    const float position = std::clamp((size - 9.f) / (36.f - 9.f), 0.f, 1.f);
    return 1.f / 24.f + (1.f / 32.f - 1.f / 24.f) * position;
}

}  // namespace

bool build(float scale) {
    Fonts& all = fonts();
    scale = std::max(0.25f, std::round(scale * 100.f) / 100.f);
    if (!all.made.empty() && scale == all.scale) {
        return false;
    }
    ImFontAtlas* atlas = ImGui::GetIO().Fonts;
    atlas->Clear();
    all.made.clear();
    for (const Size& entry : SIZES) {
        const float em = static_cast<float>(entry.size) * scale;
        ImFontConfig config;
        config.FontDataOwnedByAtlas = false;
        // Small letters are drawn at fractions of a pixel, so they are made twice as wide first.
        config.OversampleH = em > 34.f ? 1 : 2;
        config.OversampleV = 1;
        const Source& main = all.of(entry.face);
        // The glyphs are drawn at their exact size; ImGui measures them at the whole size.
        config.RasterizerDensity = main.density(em);
        ImFont* font = atlas->AddFontFromMemoryTTF(const_cast<unsigned char*>(main.data), static_cast<int>(main.size),
                                                   main.pixels(em), &config, ranges_for(entry.glyphs));
        if (entry.glyphs != Glyphs::Letters && entry.glyphs != Glyphs::Code) {
            for (const Source& source : all.symbols) {
                ImFontConfig merged = config;
                merged.MergeMode = true;
                merged.RasterizerDensity = source.density(em);
                atlas->AddFontFromMemoryTTF(const_cast<unsigned char*>(source.data), static_cast<int>(source.size),
                                            source.pixels(em), &merged, source.symbols.data());
            }
        }
        all.made[{entry.face, entry.size}] = font;
    }
    all.scale = scale;
    return true;
}

ImFont* font(Face face, int size) {
    const Fonts& all = fonts();
    ImFont* nearest = nullptr;
    int distance = 0;
    for (const auto& [key, made] : all.made) {
        if (key.first == face && (nearest == nullptr || std::abs(key.second - size) < distance)) {
            nearest = made;
            distance = std::abs(key.second - size);
        }
    }
    return nearest != nullptr ? nearest : ImGui::GetFont();
}

float glyph_scale(Face face, int size, ImWchar c) {
    const Fonts& all = fonts();
    const float em = static_cast<float>(size) * all.scale;
    if (SYMBOLS.find(static_cast<char32_t>(c)) != std::u32string_view::npos) {
        for (const Source& source : all.symbols) {
            for (size_t i = 0; i + 1 < source.symbols.size(); i += 2) {
                if (source.symbols[i] == c) {
                    return source.density(em);
                }
            }
        }
    }
    return all.of(face).density(em);
}

float kerning(Face face, ImWchar left, ImWchar right) {
    Fonts& all = fonts();
    Kerning& of_face = face == Face::Dosis600 ? all.dosis600_kerning
                       : face == Face::Dosis700 ? all.dosis700_kerning
                                                : all.baloo800_kerning;
    return of_face.between(left, right);
}

float ascent(Face face) { return fonts().of(face).metrics.ascent; }
float descent(Face face) { return fonts().of(face).metrics.descent; }

float thickening(ImWchar c, int size) {
    for (const Source& source : fonts().symbols) {
        for (size_t i = 0; i + 1 < source.symbols.size(); i += 2) {
            if (source.symbols[i] == c) {
                return source.bold ? 0.f : static_cast<float>(size) * fake_bold_share(static_cast<float>(size)) / 2.f;
            }
        }
    }
    return 0.f;
}

}  // namespace lettering
