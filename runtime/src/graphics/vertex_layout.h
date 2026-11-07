// The layout of the game's vertices: which attributes a vertex has, in which format, and
// the arrays that indexed attributes point into.
//
// La forma de los vértices del juego: qué atributos tiene un vértice, en qué formato, y
// los arreglos a los que apuntan los atributos indexados.
//
// The GameCube's GPU reads vertices as a stream of bytes whose layout the program
// describes with GXSetVtxDesc (which attributes, direct or by index) and GXSetVtxAttrFmt
// (their formats, eight sets of them). An indexed attribute is an index into an array
// that GXSetArray placed in main memory. aurora needs to know how much of each array a
// draw uses before the draw (it copies that much to the GPU), and the game never says
// it: this module keeps the same description the GPU would have, and walks the vertex
// bytes of a draw to find the highest index of each array.
//
// Formats and sizes as YAGCD (section 5.10, "Vertex attribute formats") and the Dolphin
// SDK's gx/GXEnum.h describe them.
#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace vertex_layout {

// Attributes (GXAttr), in the order in which they follow one another in a vertex.
enum Attribute : uint32_t {
    POSITION_NORMAL_MATRIX_INDEX = 0,  // GX_VA_PNMTXIDX
    TEXTURE0_MATRIX_INDEX = 1,         // GX_VA_TEX0MTXIDX, ... up to TEX7MTXIDX = 8
    POSITION = 9,                      // GX_VA_POS
    NORMAL = 10,                       // GX_VA_NRM
    COLOR0 = 11,                       // GX_VA_CLR0
    COLOR1 = 12,                       // GX_VA_CLR1
    TEXTURE_COORDINATE0 = 13,          // GX_VA_TEX0, ... up to TEX7 = 20
    POSITION_MATRIX_ARRAY = 21,        // GX_POS_MTX_ARRAY: the arrays of the indexed matrix loads
    NORMAL_MATRIX_ARRAY = 22,          // GX_NRM_MTX_ARRAY
    TEXTURE_MATRIX_ARRAY = 23,         // GX_TEX_MTX_ARRAY
    LIGHT_ARRAY = 24,                  // GX_LIGHT_ARRAY
    NORMAL_BINORMAL_TANGENT = 25,      // GX_VA_NBT: the normal attribute with binormal and tangent
};

constexpr uint32_t VERTEX_ATTRIBUTE_COUNT = 21;  // GX_VA_PNMTXIDX to GX_VA_TEX7
constexpr uint32_t ARRAY_COUNT = 25;             // every attribute up to GX_LIGHT_ARRAY
constexpr uint32_t VERTEX_FORMAT_COUNT = 8;      // GX_VTXFMT0 to GX_VTXFMT7

// How a vertex carries an attribute (GXAttrType).
enum class Presence : uint32_t {
    NONE = 0,     // GX_NONE
    DIRECT = 1,   // GX_DIRECT: the value itself
    INDEX8 = 2,   // GX_INDEX8: an 8-bit index into the attribute's array
    INDEX16 = 3,  // GX_INDEX16: a 16-bit index
};

// The format of one attribute in one of the eight vertex formats (GXSetVtxAttrFmt).
struct AttributeFormat {
    uint32_t component_count = 0;  // GXCompCnt: XY/XYZ, NRM/NBT/NBT3, RGB/RGBA, S/ST
    uint32_t component_type = 0;   // GXCompType: U8, S8, U16, S16, F32, or a color format
};

// The highest index that a draw uses in each array, or -1 for an array it does not use.
using HighestIndices = std::array<int32_t, ARRAY_COUNT>;

void set_presence(uint32_t attribute, Presence presence);
void clear_presences();
void set_format(uint32_t vertex_format, uint32_t attribute, uint32_t component_count, uint32_t component_type);
void set_array(uint32_t attribute, uint32_t guest_address, uint32_t stride);

uint32_t array_address(uint32_t attribute);
uint32_t array_stride(uint32_t attribute);

// The size in bytes of one element of an attribute's array in a vertex format: what an
// index selects.
uint32_t array_element_size(uint32_t vertex_format, uint32_t attribute);

// The size in bytes of one vertex in a vertex format, as the GPU reads it.
uint32_t vertex_size(uint32_t vertex_format);

// Walks `vertex_count` vertices of a vertex format starting at `vertices` (big-endian, as
// the game wrote them) and raises `highest` to the highest index each array is read at.
void find_highest_indices(uint32_t vertex_format, const uint8_t* vertices, uint32_t vertex_count,
                          HighestIndices& highest);

}  // namespace vertex_layout
