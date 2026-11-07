// The GX functions of textures and color tables (TLUTs), replaced by aurora's. Addresses
// from config/G4AEE9/symbols.txt.
//
// Las funciones de GX de texturas y tablas de colores (TLUT), reemplazadas por las de
// aurora.
//
// The game keeps a GXTexObj of 32 bytes and a GXTlutObj of 12 in its memory; aurora's are
// larger on the PC (64 and 40 bytes), so each game object gets an aurora object kept
// here, found by the game object's address. The images and tables stay in the game's
// memory: aurora reads them from there, in the console's formats.

#include "graphics/guest_values.h"
#include "graphics/gx_draw.h"
#include "native_functions.h"

#include <dolphin/gx.h>

#include <unordered_map>

namespace {

std::unordered_map<uint32_t, GXTexObj> g_textures;
std::unordered_map<uint32_t, GXTlutObj> g_color_tables;

// aurora works out the size of an image from its format; the address only has to be
// ordinary memory.
const void* image_data(uint32_t image_address) { return guest_values::host_data(image_address, 1); }

// GXInitTexObj(GXTexObj* texture, void* image, u16 width, u16 height, GXTexFmt format,
// GXTexWrapMode wrap_s, GXTexWrapMode wrap_t, GXBool mipmap)
void GXInitTexObjNative(uint32_t texture_address, uint32_t image_address, uint32_t width, uint32_t height,
                        uint32_t format, uint32_t wrap_s, uint32_t wrap_t, uint32_t mipmap) {
    GXInitTexObj(&g_textures[texture_address], image_data(image_address), static_cast<u16>(width),
                 static_cast<u16>(height), static_cast<GXTexFmt>(format), static_cast<GXTexWrapMode>(wrap_s),
                 static_cast<GXTexWrapMode>(wrap_t), static_cast<GXBool>(mipmap));
}
PPC_NATIVE_OVERRIDE_VOID(801931B8, GXInitTexObjNative,
                         (uint32_t texture_address, uint32_t image_address, uint32_t width, uint32_t height,
                          uint32_t format, uint32_t wrap_s, uint32_t wrap_t, uint32_t mipmap),
                         (texture_address, image_address, width, height, format, wrap_s, wrap_t, mipmap));

// GXInitTexObjCI(GXTexObj* texture, void* image, u16 width, u16 height, GXCITexFmt format,
// GXTexWrapMode wrap_s, GXTexWrapMode wrap_t, GXBool mipmap, u32 color_table): the ninth
// argument comes from the stack.
void GXInitTexObjCINative(uint32_t texture_address, uint32_t image_address, uint32_t width, uint32_t height,
                          uint32_t format, uint32_t wrap_s, uint32_t wrap_t, uint32_t mipmap, uint32_t color_table) {
    GXInitTexObjCI(&g_textures[texture_address], image_data(image_address), static_cast<u16>(width),
                   static_cast<u16>(height), static_cast<GXCITexFmt>(format), static_cast<GXTexWrapMode>(wrap_s),
                   static_cast<GXTexWrapMode>(wrap_t), static_cast<GXBool>(mipmap), color_table);
}
PPC_NATIVE_OVERRIDE_VOID(80193404, GXInitTexObjCINative,
                         (uint32_t texture_address, uint32_t image_address, uint32_t width, uint32_t height,
                          uint32_t format, uint32_t wrap_s, uint32_t wrap_t, uint32_t mipmap, uint32_t color_table),
                         (texture_address, image_address, width, height, format, wrap_s, wrap_t, mipmap, color_table));

// GXInitTexObjLOD(GXTexObj* texture, GXTexFilter minification, GXTexFilter magnification,
// f32 min_lod, f32 max_lod, f32 lod_bias, GXBool bias_clamp, GXBool edge_lod,
// GXAnisotropy max_anisotropy)
void GXInitTexObjLODNative(uint32_t texture_address, uint32_t minification, uint32_t magnification, float min_lod,
                           float max_lod, float lod_bias, uint32_t bias_clamp, uint32_t edge_lod,
                           uint32_t max_anisotropy) {
    GXInitTexObjLOD(&g_textures[texture_address], static_cast<GXTexFilter>(minification),
                    static_cast<GXTexFilter>(magnification), min_lod, max_lod, lod_bias,
                    static_cast<GXBool>(bias_clamp), static_cast<GXBool>(edge_lod),
                    static_cast<GXAnisotropy>(max_anisotropy));
}
PPC_NATIVE_OVERRIDE_VOID(8019344C, GXInitTexObjLODNative,
                         (uint32_t texture_address, uint32_t minification, uint32_t magnification, float min_lod,
                          float max_lod, float lod_bias, uint32_t bias_clamp, uint32_t edge_lod,
                          uint32_t max_anisotropy),
                         (texture_address, minification, magnification, min_lod, max_lod, lod_bias, bias_clamp,
                          edge_lod, max_anisotropy));

// GXLoadTexObj(GXTexObj* texture, GXTexMapID map)
void GXLoadTexObjNative(uint32_t texture_address, uint32_t map) {
    gx_draw::finish_primitive();
    GXLoadTexObj(&g_textures[texture_address], static_cast<GXTexMapID>(map));
}
PPC_NATIVE_OVERRIDE_VOID(8019374C, GXLoadTexObjNative, (uint32_t texture_address, uint32_t map),
                         (texture_address, map));

// GXInitTlutObj(GXTlutObj* table, void* colors, GXTlutFmt format, u16 entries)
void GXInitTlutObjNative(uint32_t table_address, uint32_t colors_address, uint32_t format, uint32_t entries) {
    GXInitTlutObj(&g_color_tables[table_address], image_data(colors_address), static_cast<GXTlutFmt>(format),
                  static_cast<u16>(entries));
}
PPC_NATIVE_OVERRIDE_VOID(801937A0, GXInitTlutObjNative,
                         (uint32_t table_address, uint32_t colors_address, uint32_t format, uint32_t entries),
                         (table_address, colors_address, format, entries));

// GXLoadTlut(GXTlutObj* table, u32 slot)
void GXLoadTlutNative(uint32_t table_address, uint32_t slot) {
    gx_draw::finish_primitive();
    GXLoadTlut(&g_color_tables[table_address], slot);
}
PPC_NATIVE_OVERRIDE_VOID(801937D8, GXLoadTlutNative, (uint32_t table_address, uint32_t slot), (table_address, slot));

// GXInvalidateTexAll(void): the game tells the GPU that images in memory changed.
void GXInvalidateTexAllNative() {
    gx_draw::finish_primitive();
    GXInvalidateTexAll();
}
PPC_NATIVE_OVERRIDE_VOID(8019399C, GXInvalidateTexAllNative, (), ());

}  // namespace
