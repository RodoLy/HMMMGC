// See kerning.h. The OpenType tables are read where they are, in the font's bytes (big-endian).

#include "kerning.h"

#include <algorithm>
#include <bit>
#include <cstring>

namespace {

constexpr uint32_t PAIR_ADJUSTMENT = 2;  // the lookup type of kerning
constexpr uint32_t EXTENSION = 9;        // a lookup that holds one of another type further away

// A value record holds two bytes for each field its format names, in the order of their bits:
// the X placement, the Y placement, the X advance (the kerning), and then others.
constexpr uint32_t X_PLACEMENT = 0x0001;
constexpr uint32_t Y_PLACEMENT = 0x0002;
constexpr uint32_t X_ADVANCE = 0x0004;

size_t record_size(uint32_t format) { return 2u * static_cast<size_t>(std::popcount(format & 0xFFu)); }

// Where the X advance is in a value record of `format`, or -1 when it has none.
int x_advance_at(uint32_t format) {
    return (format & X_ADVANCE) != 0 ? 2 * std::popcount(format & (X_PLACEMENT | Y_PLACEMENT)) : -1;
}

}  // namespace

Kerning::Kerning(const unsigned char* data, size_t size) : data_(data), size_(size) {
    if (const Table head = table("head"); head.found && u16(head.offset + 18) != 0) {
        units_per_em_ = static_cast<float>(u16(head.offset + 18));
    }

    // The map from characters to glyphs: the full Unicode one (format 12) before the basic one (4).
    if (const Table cmap = table("cmap"); cmap.found) {
        int best = 0;
        for (uint32_t i = 0; i < u16(cmap.offset + 2); ++i) {
            const size_t record = cmap.offset + 4 + i * 8;
            const uint32_t platform = u16(record);
            const uint32_t encoding = u16(record + 2);
            const size_t subtable = cmap.offset + u32(record + 4);
            const uint32_t format = u16(subtable);
            const bool unicode = platform == 0 || (platform == 3 && (encoding == 1 || encoding == 10));
            const int rank = !unicode ? 0 : format == 12 ? 2 : format == 4 ? 1 : 0;
            if (rank > best) {
                best = rank;
                cmap_ = subtable;
            }
        }
    }

    // The lookups of the 'kern' feature, in the order of the lookup list, and their subtables.
    const Table gpos = table("GPOS");
    if (!gpos.found) {
        return;
    }
    const size_t features = gpos.offset + u16(gpos.offset + 6);
    const size_t lookups = gpos.offset + u16(gpos.offset + 8);
    std::vector<uint32_t> indices;
    for (uint32_t i = 0; i < u16(features); ++i) {
        const size_t record = features + 2 + i * 6;
        if (record + 6 > size_ || std::memcmp(data_ + record, "kern", 4) != 0) {
            continue;
        }
        const size_t feature = features + u16(record + 4);
        for (uint32_t j = 0; j < u16(feature + 2); ++j) {
            const uint32_t index = u16(feature + 4 + j * 2);
            if (std::ranges::find(indices, index) == indices.end()) {
                indices.push_back(index);
            }
        }
    }
    std::ranges::sort(indices);
    for (const uint32_t index : indices) {
        if (index >= u16(lookups)) {
            continue;
        }
        const size_t lookup = lookups + u16(lookups + 2 + index * 2);
        const uint32_t type = u16(lookup);
        for (uint32_t k = 0; k < u16(lookup + 4); ++k) {
            size_t subtable = lookup + u16(lookup + 6 + k * 2);
            uint32_t subtable_type = type;
            if (type == EXTENSION) {
                subtable_type = u16(subtable + 2);
                subtable += u32(subtable + 4);
            }
            if (subtable_type == PAIR_ADJUSTMENT) {
                pair_subtables_.push_back(subtable);
            }
        }
        lookup_ends_.push_back(pair_subtables_.size());
    }
}

float Kerning::between(char32_t left, char32_t right) {
    if (pair_subtables_.empty() || cmap_ == 0) {
        return 0.f;
    }
    const uint64_t key = static_cast<uint64_t>(left) << 32 | right;
    if (const auto found = known_.find(key); found != known_.end()) {
        return found->second;
    }
    const uint16_t first = glyph(left);
    const uint16_t second = glyph(right);
    const float value = first != 0 && second != 0 ? static_cast<float>(pair(first, second)) / units_per_em_ : 0.f;
    known_.emplace(key, value);
    return value;
}

uint32_t Kerning::u16(size_t at) const {
    return at + 2 <= size_ ? static_cast<uint32_t>(data_[at] << 8 | data_[at + 1]) : 0;
}

uint32_t Kerning::u32(size_t at) const { return u16(at) << 16 | u16(at + 2); }

Kerning::Table Kerning::table(const char* tag) const {
    for (uint32_t i = 0; i < u16(4); ++i) {
        const size_t record = 12 + i * 16;
        if (record + 16 <= size_ && std::memcmp(data_ + record, tag, 4) == 0) {
            return {u32(record + 8), true};
        }
    }
    return {};
}

uint16_t Kerning::glyph(char32_t c) const {
    if (u16(cmap_) == 12) {
        // Groups of characters that go to consecutive glyphs.
        const uint32_t groups = u32(cmap_ + 12);
        for (uint32_t i = 0; i < groups; ++i) {
            const size_t group = cmap_ + 16 + i * 12;
            if (c >= u32(group) && c <= u32(group + 4)) {
                return static_cast<uint16_t>(u32(group + 8) + (c - u32(group)));
            }
        }
        return 0;
    }
    if (c > 0xFFFF) {
        return 0;
    }
    // Format 4: segments of characters, each with a delta or with its own array of glyphs.
    const uint32_t segments = u16(cmap_ + 6) / 2;
    const size_t ends = cmap_ + 14;
    const size_t starts = ends + segments * 2 + 2;
    const size_t deltas = starts + segments * 2;
    const size_t range_offsets = deltas + segments * 2;
    for (uint32_t i = 0; i < segments; ++i) {
        if (c > u16(ends + i * 2)) {
            continue;
        }
        const uint32_t start = u16(starts + i * 2);
        if (c < start) {
            return 0;
        }
        const uint32_t delta = u16(deltas + i * 2);
        const uint32_t range_offset = u16(range_offsets + i * 2);
        if (range_offset == 0) {
            return static_cast<uint16_t>((c + delta) & 0xFFFF);
        }
        const uint32_t found = u16(range_offsets + i * 2 + range_offset + (c - start) * 2);
        return found == 0 ? 0 : static_cast<uint16_t>((found + delta) & 0xFFFF);
    }
    return 0;
}

// The kerning of every lookup added up; in each lookup, the first subtable that has the pair.
int Kerning::pair(uint16_t first, uint16_t second) const {
    int total = 0;
    size_t start = 0;
    for (const size_t end : lookup_ends_) {
        for (size_t i = start; i < end; ++i) {
            bool applied = false;
            const int value = pair_in_subtable(pair_subtables_[i], first, second, applied);
            if (applied) {
                total += value;
                break;
            }
        }
        start = end;
    }
    return total;
}

int Kerning::pair_in_subtable(size_t subtable, uint16_t first, uint16_t second, bool& applied) const {
    const int covered = coverage_index(subtable + u16(subtable + 2), first);
    if (covered < 0) {
        return 0;
    }
    const uint32_t format1 = u16(subtable + 4);
    const uint32_t format2 = u16(subtable + 6);
    const int x_advance = x_advance_at(format1);
    if (u16(subtable) == 1) {
        // The pairs of the first glyph, sorted by the second.
        if (covered >= static_cast<int>(u16(subtable + 8))) {
            return 0;
        }
        const size_t set = subtable + u16(subtable + 10 + static_cast<size_t>(covered) * 2);
        const size_t stride = 2 + record_size(format1) + record_size(format2);
        size_t low = 0;
        size_t high = u16(set);
        while (low < high) {
            const size_t middle = (low + high) / 2;
            const size_t record = set + 2 + middle * stride;
            const uint32_t glyph = u16(record);
            if (glyph == second) {
                applied = true;
                return x_advance < 0 ? 0 : static_cast<int16_t>(u16(record + 2 + static_cast<size_t>(x_advance)));
            }
            if (glyph < second) {
                low = middle + 1;
            } else {
                high = middle;
            }
        }
        return 0;
    }
    if (u16(subtable) == 2) {
        // A table of the classes of the first glyph by those of the second.
        const auto class1 = static_cast<uint32_t>(glyph_class(subtable + u16(subtable + 8), first));
        const auto class2 = static_cast<uint32_t>(glyph_class(subtable + u16(subtable + 10), second));
        const uint32_t class2_count = u16(subtable + 14);
        if (class1 >= u16(subtable + 12) || class2 >= class2_count) {
            return 0;
        }
        applied = true;
        const size_t pair_size = record_size(format1) + record_size(format2);
        const size_t record = subtable + 16 + (class1 * class2_count + class2) * pair_size;
        return x_advance < 0 ? 0 : static_cast<int16_t>(u16(record + static_cast<size_t>(x_advance)));
    }
    return 0;
}

int Kerning::coverage_index(size_t coverage, uint16_t glyph) const {
    const uint32_t count = u16(coverage + 2);
    if (u16(coverage) == 1) {
        // The glyphs, sorted.
        size_t low = 0;
        size_t high = count;
        while (low < high) {
            const size_t middle = (low + high) / 2;
            const uint32_t found = u16(coverage + 4 + middle * 2);
            if (found == glyph) {
                return static_cast<int>(middle);
            }
            if (found < glyph) {
                low = middle + 1;
            } else {
                high = middle;
            }
        }
    } else if (u16(coverage) == 2) {
        // Ranges of glyphs, each with the index of its first one.
        for (uint32_t i = 0; i < count; ++i) {
            const size_t range = coverage + 4 + i * 6;
            if (glyph >= u16(range) && glyph <= u16(range + 2)) {
                return static_cast<int>(u16(range + 4) + (glyph - u16(range)));
            }
        }
    }
    return -1;
}

int Kerning::glyph_class(size_t class_definition, uint16_t glyph) const {
    if (u16(class_definition) == 1) {
        const uint32_t start = u16(class_definition + 2);
        if (glyph >= start && glyph < start + u16(class_definition + 4)) {
            return static_cast<int>(u16(class_definition + 6 + (glyph - start) * 2));
        }
    } else if (u16(class_definition) == 2) {
        for (uint32_t i = 0; i < u16(class_definition + 2); ++i) {
            const size_t range = class_definition + 4 + i * 6;
            if (glyph >= u16(range) && glyph <= u16(range + 2)) {
                return static_cast<int>(u16(range + 4));
            }
        }
    }
    return 0;  // the glyphs no class names are of class 0
}
