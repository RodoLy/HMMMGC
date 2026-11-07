// The GX functions that describe and send geometry, replaced by aurora's. The vertex
// layout is also kept in vertex_layout.h, which gx_draw.h needs to read the game's
// vertices. Addresses from config/G4AEE9/symbols.txt.
//
// Las funciones de GX que describen y envían la geometría, reemplazadas por las de aurora.

#include "graphics/gx_draw.h"
#include "graphics/vertex_layout.h"
#include "native_functions.h"

#include <dolphin/gx.h>

namespace {

// GXSetVtxDesc(GXAttr attribute, GXAttrType presence)
void GXSetVtxDescNative(uint32_t attribute, uint32_t presence) {
    gx_draw::finish_primitive();
    vertex_layout::set_presence(attribute, static_cast<vertex_layout::Presence>(presence));
    GXSetVtxDesc(static_cast<GXAttr>(attribute), static_cast<GXAttrType>(presence));
}
PPC_NATIVE_OVERRIDE_VOID(801908C8, GXSetVtxDescNative, (uint32_t attribute, uint32_t presence), (attribute, presence));

// GXClearVtxDesc(void)
void GXClearVtxDescNative() {
    gx_draw::finish_primitive();
    vertex_layout::clear_presences();
    GXClearVtxDesc();
}
PPC_NATIVE_OVERRIDE_VOID(80190D14, GXClearVtxDescNative, (), ());

// GXSetVtxAttrFmt(GXVtxFmt format, GXAttr attribute, GXCompCnt count, GXCompType type,
// u8 fraction_bits)
void GXSetVtxAttrFmtNative(uint32_t vertex_format, uint32_t attribute, uint32_t component_count,
                           uint32_t component_type, uint32_t fraction_bits) {
    gx_draw::finish_primitive();
    vertex_layout::set_format(vertex_format, attribute, component_count, component_type);
    GXSetVtxAttrFmt(static_cast<GXVtxFmt>(vertex_format), static_cast<GXAttr>(attribute),
                    static_cast<GXCompCnt>(component_count), static_cast<GXCompType>(component_type),
                    static_cast<u8>(fraction_bits));
}
PPC_NATIVE_OVERRIDE_VOID(80190D4C, GXSetVtxAttrFmtNative,
                         (uint32_t vertex_format, uint32_t attribute, uint32_t component_count, uint32_t component_type,
                          uint32_t fraction_bits),
                         (vertex_format, attribute, component_count, component_type, fraction_bits));

// GXSetArray(GXAttr attribute, void* base, u8 stride). aurora also wants the array's size,
// which the game never gives: gx_draw.h tells aurora at each draw.
void GXSetArrayNative(uint32_t attribute, uint32_t base_address, uint32_t stride) {
    gx_draw::finish_primitive();
    vertex_layout::set_array(attribute, base_address, stride & 0xFFu);
}
PPC_NATIVE_OVERRIDE_VOID(801912B0, GXSetArrayNative, (uint32_t attribute, uint32_t base_address, uint32_t stride),
                         (attribute, base_address, stride));

// GXSetTexCoordGen2(GXTexCoordID coordinate, GXTexGenType function, GXTexGenSrc source,
// u32 matrix, GXBool normalize, u32 post_matrix)
void GXSetTexCoordGen2Native(uint32_t coordinate, uint32_t function, uint32_t source, uint32_t matrix,
                             uint32_t normalize, uint32_t post_matrix) {
    gx_draw::finish_primitive();
    GXSetTexCoordGen2(static_cast<GXTexCoordID>(coordinate), static_cast<GXTexGenType>(function),
                      static_cast<GXTexGenSrc>(source), matrix, static_cast<GXBool>(normalize), post_matrix);
}
PPC_NATIVE_OVERRIDE_VOID(80191304, GXSetTexCoordGen2Native,
                         (uint32_t coordinate, uint32_t function, uint32_t source, uint32_t matrix, uint32_t normalize,
                          uint32_t post_matrix),
                         (coordinate, function, source, matrix, normalize, post_matrix));

// GXSetNumTexGens(u8 count)
void GXSetNumTexGensNative(uint32_t count) {
    gx_draw::finish_primitive();
    GXSetNumTexGens(static_cast<u8>(count));
}
PPC_NATIVE_OVERRIDE_VOID(80191584, GXSetNumTexGensNative, (uint32_t count), (count));

// GXBegin(GXPrimitive primitive, GXVtxFmt format, u16 vertex_count): the vertices follow
// through the write-gather pipe (gx_draw.h).
void GXBeginNative(uint32_t primitive, uint32_t vertex_format, uint32_t vertex_count) {
    gx_draw::begin_primitive(primitive, vertex_format, vertex_count & 0xFFFFu);
}
PPC_NATIVE_OVERRIDE_VOID(80191CC0, GXBeginNative, (uint32_t primitive, uint32_t vertex_format, uint32_t vertex_count),
                         (primitive, vertex_format, vertex_count));

// GXSetPointSize(u8 size, GXTexOffset texture_offsets)
void GXSetPointSizeNative(uint32_t size, uint32_t texture_offsets) {
    gx_draw::finish_primitive();
    GXSetPointSize(static_cast<u8>(size), static_cast<GXTexOffset>(texture_offsets));
}
PPC_NATIVE_OVERRIDE_VOID(80191E58, GXSetPointSizeNative, (uint32_t size, uint32_t texture_offsets),
                         (size, texture_offsets));

// GXSetCullMode(GXCullMode mode)
void GXSetCullModeNative(uint32_t mode) {
    gx_draw::finish_primitive();
    GXSetCullMode(static_cast<GXCullMode>(mode));
}
PPC_NATIVE_OVERRIDE_VOID(80191EE0, GXSetCullModeNative, (uint32_t mode), (mode));

// GXSetClipMode(GXClipMode mode)
void GXSetClipModeNative(uint32_t mode) {
    gx_draw::finish_primitive();
    GXSetClipMode(static_cast<GXClipMode>(mode));
}
PPC_NATIVE_OVERRIDE_VOID(80195694, GXSetClipModeNative, (uint32_t mode), (mode));

// GXBeginDisplayList(void* list, u32 size), GXEndDisplayList(void) and
// GXCallDisplayList(void* list, u32 size).
void GXBeginDisplayListNative(uint32_t list_address, uint32_t size) { gx_draw::begin_display_list(list_address, size); }
PPC_NATIVE_OVERRIDE_VOID(80194E80, GXBeginDisplayListNative, (uint32_t list_address, uint32_t size),
                         (list_address, size));

uint32_t GXEndDisplayListNative() { return gx_draw::end_display_list(); }
PPC_NATIVE_OVERRIDE(80194F4C, GXEndDisplayListNative, uint32_t, (), ());

void GXCallDisplayListNative(uint32_t list_address, uint32_t size) { gx_draw::call_display_list(list_address, size); }
PPC_NATIVE_OVERRIDE_VOID(80195010, GXCallDisplayListNative, (uint32_t list_address, uint32_t size),
                         (list_address, size));

}  // namespace
