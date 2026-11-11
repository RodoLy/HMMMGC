// See vertex_layout.h.

#include "graphics/vertex_layout.h"

#include <algorithm>

namespace vertex_layout {
namespace {

// GXCompCnt values that change a size.
constexpr uint32_t POSITION_XY = 0;  // GX_POS_XY (GX_POS_XYZ = 1)
constexpr uint32_t NORMAL_XYZ = 0;   // GX_NRM_XYZ (GX_NRM_NBT = 1: normal, binormal and tangent at one index)
constexpr uint32_t NORMAL_NBT3 = 2;  // GX_NRM_NBT3: the three of them at three indices
constexpr uint32_t TEXTURE_S = 0;    // GX_TEX_S (GX_TEX_ST = 1)

constexpr uint32_t LAST_MATRIX_INDEX = 8;         // GX_VA_TEX7MTXIDX
constexpr uint32_t LAST_TEXTURE_COORDINATE = 20;  // GX_VA_TEX7

struct State {
    std::array<Presence, VERTEX_ATTRIBUTE_COUNT> presences{};
    std::array<std::array<AttributeFormat, VERTEX_ATTRIBUTE_COUNT>, VERTEX_FORMAT_COUNT> formats{};
    std::array<uint32_t, ARRAY_COUNT> array_addresses{};
    std::array<uint32_t, ARRAY_COUNT> array_strides{};
};
State g_state;

bool is_color(uint32_t attribute) { return attribute == COLOR0 || attribute == COLOR1; }

// GX_VA_NBT names the normal attribute.
uint32_t attribute_of(uint32_t attribute) { return attribute == NORMAL_BINORMAL_TANGENT ? NORMAL : attribute; }

// Bytes of one component (GXCompType: U8, S8, U16, S16, F32).
uint32_t component_size(uint32_t component_type) {
    static constexpr uint32_t SIZES[] = {1, 1, 2, 2, 4};
    return component_type < std::size(SIZES) ? SIZES[component_type] : 4;
}

// Bytes of one color (GXCompType for colors: RGB565, RGB8, RGBX8, RGBA4, RGBA6, RGBA8).
uint32_t color_size(uint32_t color_type) {
    static constexpr uint32_t SIZES[] = {2, 3, 4, 2, 3, 4};
    return color_type < std::size(SIZES) ? SIZES[color_type] : 4;
}

// Components of one value of an attribute as it is stored, directly or in its array.
uint32_t component_count(uint32_t attribute, const AttributeFormat& format) {
    if (attribute == POSITION) {
        return format.component_count == POSITION_XY ? 2 : 3;
    }
    if (attribute == NORMAL) {
        return format.component_count == NORMAL_XYZ ? 3 : 9;
    }
    return format.component_count == TEXTURE_S ? 1 : 2;  // texture coordinates
}

// Bytes of an attribute written directly in a vertex.
uint32_t direct_size(uint32_t attribute, const AttributeFormat& format) {
    if (attribute <= LAST_MATRIX_INDEX) {
        return 1;
    }
    if (is_color(attribute)) {
        return color_size(format.component_type);
    }
    return component_count(attribute, format) * component_size(format.component_type);
}

// How many indices a vertex holds for an indexed attribute: three for a normal with its
// binormal and tangent at separate indices, one otherwise.
uint32_t index_count(uint32_t attribute, const AttributeFormat& format) {
    return attribute == NORMAL && format.component_count == NORMAL_NBT3 ? 3 : 1;
}

uint32_t read_big_endian16(const uint8_t* bytes) { return (uint32_t{bytes[0]} << 8) | bytes[1]; }

}  // namespace

void set_presence(uint32_t attribute, Presence presence) {
    attribute = attribute_of(attribute);
    if (attribute < VERTEX_ATTRIBUTE_COUNT) {
        g_state.presences[attribute] = presence;
    }
}

void clear_presences() { g_state.presences.fill(Presence::NONE); }

void set_format(uint32_t vertex_format, uint32_t attribute, uint32_t component_count, uint32_t component_type) {
    attribute = attribute_of(attribute);
    if (vertex_format < VERTEX_FORMAT_COUNT && attribute < VERTEX_ATTRIBUTE_COUNT) {
        g_state.formats[vertex_format][attribute] = {component_count, component_type};
    }
}

void set_array(uint32_t attribute, uint32_t guest_address, uint32_t stride) {
    attribute = attribute_of(attribute);
    if (attribute < ARRAY_COUNT) {
        g_state.array_addresses[attribute] = guest_address;
        g_state.array_strides[attribute] = stride;
    }
}

uint32_t array_address(uint32_t attribute) { return g_state.array_addresses[attribute_of(attribute)]; }

uint32_t array_stride(uint32_t attribute) { return g_state.array_strides[attribute_of(attribute)]; }

uint32_t array_element_size(uint32_t vertex_format, uint32_t attribute) {
    attribute = attribute_of(attribute);
    const AttributeFormat& format = g_state.formats[vertex_format % VERTEX_FORMAT_COUNT][attribute];
    if (is_color(attribute)) {
        return color_size(format.component_type);
    }
    if (attribute == NORMAL && format.component_count == NORMAL_NBT3) {
        return 3 * component_size(format.component_type);  // each index selects one vector
    }
    return component_count(attribute, format) * component_size(format.component_type);
}

uint32_t vertex_size(uint32_t vertex_format) {
    const auto& formats = g_state.formats[vertex_format % VERTEX_FORMAT_COUNT];
    uint32_t size = 0;
    for (uint32_t attribute = 0; attribute <= LAST_TEXTURE_COORDINATE; ++attribute) {
        switch (g_state.presences[attribute]) {
            case Presence::NONE:
                break;
            case Presence::DIRECT:
                size += direct_size(attribute, formats[attribute]);
                break;
            case Presence::INDEX8:
                size += index_count(attribute, formats[attribute]);
                break;
            case Presence::INDEX16:
                size += 2 * index_count(attribute, formats[attribute]);
                break;
        }
    }
    return size;
}

void find_highest_indices(uint32_t vertex_format, const uint8_t* vertices, uint32_t vertex_count,
                          HighestIndices& highest) {
    const auto& formats = g_state.formats[vertex_format % VERTEX_FORMAT_COUNT];
    const uint8_t* cursor = vertices;
    for (uint32_t vertex = 0; vertex < vertex_count; ++vertex) {
        for (uint32_t attribute = 0; attribute <= LAST_TEXTURE_COORDINATE; ++attribute) {
            const Presence presence = g_state.presences[attribute];
            if (presence == Presence::NONE) {
                continue;
            }
            if (presence == Presence::DIRECT) {
                cursor += direct_size(attribute, formats[attribute]);
                continue;
            }
            const bool wide = presence == Presence::INDEX16;
            for (uint32_t index_number = 0; index_number < index_count(attribute, formats[attribute]); ++index_number) {
                const uint32_t index = wide ? read_big_endian16(cursor) : *cursor;
                cursor += wide ? 2 : 1;
                // An index with every bit set skips the vertex instead of naming an element
                // (Dolphin's vertex loader treats positions that way); it does not belong to
                // the array.
                const uint32_t skip_index = wide ? 0xFFFFu : 0xFFu;
                if (attribute == POSITION && index == skip_index) {
                    continue;
                }
                highest[attribute] = std::max(highest[attribute], static_cast<int32_t>(index));
            }
        }
    }
}

}  // namespace vertex_layout
