// The GX functions of lighting and color channels, replaced by aurora's. Addresses from
// config/G4AEE9/symbols.txt.
//
// Las funciones de GX de iluminación y canales de color, reemplazadas por las de aurora.
//
// A light is a GXLightObj that the game keeps in its memory; aurora's GXLightObj has
// another size and layout on the PC, so each game light gets an aurora light kept here,
// found by the game light's address.

#include "graphics/guest_values.h"
#include "graphics/gx_draw.h"
#include "native_functions.h"

#include <dolphin/gx.h>

#include <unordered_map>

namespace {

std::unordered_map<uint32_t, GXLightObj> g_lights;

// GXInitLightPos(GXLightObj* light, f32 x, f32 y, f32 z)
void GXInitLightPosNative(uint32_t light_address, float x, float y, float z) {
    GXInitLightPos(&g_lights[light_address], x, y, z);
}
PPC_NATIVE_OVERRIDE_VOID(80192C40, GXInitLightPosNative, (uint32_t light_address, float x, float y, float z),
                         (light_address, x, y, z));

// GXInitLightColor(GXLightObj* light, GXColor color)
void GXInitLightColorNative(uint32_t light_address, uint32_t color_address) {
    GXInitLightColor(&g_lights[light_address], guest_values::read_color(color_address));
}
PPC_NATIVE_OVERRIDE_VOID(80192C50, GXInitLightColorNative, (uint32_t light_address, uint32_t color_address),
                         (light_address, color_address));

// GXLoadLightObjImm(GXLightObj* light, GXLightID slot)
void GXLoadLightObjImmNative(uint32_t light_address, uint32_t slot) {
    gx_draw::finish_primitive();
    GXLoadLightObjImm(&g_lights[light_address], static_cast<GXLightID>(slot));
}
PPC_NATIVE_OVERRIDE_VOID(80192C5C, GXLoadLightObjImmNative, (uint32_t light_address, uint32_t slot),
                         (light_address, slot));

// GXSetChanAmbColor(GXChannelID channel, GXColor color)
void GXSetChanAmbColorNative(uint32_t channel, uint32_t color_address) {
    gx_draw::finish_primitive();
    GXSetChanAmbColor(static_cast<GXChannelID>(channel), guest_values::read_color(color_address));
}
PPC_NATIVE_OVERRIDE_VOID(80192CD8, GXSetChanAmbColorNative, (uint32_t channel, uint32_t color_address),
                         (channel, color_address));

// GXSetChanMatColor(GXChannelID channel, GXColor color)
void GXSetChanMatColorNative(uint32_t channel, uint32_t color_address) {
    gx_draw::finish_primitive();
    GXSetChanMatColor(static_cast<GXChannelID>(channel), guest_values::read_color(color_address));
}
PPC_NATIVE_OVERRIDE_VOID(80192DC0, GXSetChanMatColorNative, (uint32_t channel, uint32_t color_address),
                         (channel, color_address));

// GXSetNumChans(u8 count)
void GXSetNumChansNative(uint32_t count) {
    gx_draw::finish_primitive();
    GXSetNumChans(static_cast<u8>(count));
}
PPC_NATIVE_OVERRIDE_VOID(80192EA8, GXSetNumChansNative, (uint32_t count), (count));

// GXSetChanCtrl(GXChannelID channel, GXBool enable, GXColorSrc ambient_source,
// GXColorSrc material_source, u32 lights, GXDiffuseFn diffuse, GXAttnFn attenuation)
void GXSetChanCtrlNative(uint32_t channel, uint32_t enable, uint32_t ambient_source, uint32_t material_source,
                         uint32_t lights, uint32_t diffuse, uint32_t attenuation) {
    gx_draw::finish_primitive();
    GXSetChanCtrl(static_cast<GXChannelID>(channel), static_cast<GXBool>(enable),
                  static_cast<GXColorSrc>(ambient_source), static_cast<GXColorSrc>(material_source), lights,
                  static_cast<GXDiffuseFn>(diffuse), static_cast<GXAttnFn>(attenuation));
}
PPC_NATIVE_OVERRIDE_VOID(80192EE4, GXSetChanCtrlNative,
                         (uint32_t channel, uint32_t enable, uint32_t ambient_source, uint32_t material_source,
                          uint32_t lights, uint32_t diffuse, uint32_t attenuation),
                         (channel, enable, ambient_source, material_source, lights, diffuse, attenuation));

}  // namespace
