// The kerning of a font: how much closer or farther apart two letters go, as its GPOS table says
// (the pair adjustments of its 'kern' feature), the way a browser spaces them.
//
// El interletraje de una fuente: cuánto se acercan o se alejan dos letras, como lo dice su tabla
// GPOS, como las espacia un navegador.
#pragma once

#include <cstddef>
#include <cstdint>
#include <unordered_map>
#include <vector>

class Kerning {
public:
    // `data` (a TrueType or OpenType font) must live as long as this.
    Kerning(const unsigned char* data, size_t size);

    // How much the space after `left` grows when `right` follows it, in ems (negative: it shrinks).
    float between(char32_t left, char32_t right);

private:
    struct Table {
        size_t offset = 0;
        bool found = false;
    };
    uint32_t u16(size_t at) const;
    uint32_t u32(size_t at) const;
    Table table(const char* tag) const;
    uint16_t glyph(char32_t c) const;
    int pair(uint16_t first, uint16_t second) const;
    int pair_in_subtable(size_t subtable, uint16_t first, uint16_t second, bool& applied) const;
    int coverage_index(size_t coverage, uint16_t glyph) const;
    int glyph_class(size_t class_definition, uint16_t glyph) const;

    const unsigned char* data_;
    size_t size_;
    float units_per_em_ = 1000.f;
    size_t cmap_ = 0;  // the subtable that maps characters to glyphs (format 4 or 12), 0 when none
    std::vector<size_t> pair_subtables_;  // in order; those of one lookup together
    std::vector<size_t> lookup_ends_;     // where each lookup's subtables end in pair_subtables_
    std::unordered_map<uint64_t, float> known_;
};
