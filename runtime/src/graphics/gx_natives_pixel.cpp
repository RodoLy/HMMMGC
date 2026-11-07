// The GX functions of the pixel stage (blending, depth, the frame buffer's format),
// replaced by aurora's. Addresses from config/G4AEE9/symbols.txt.
//
// Las funciones de GX de la etapa de píxeles (mezcla, profundidad, formato del frame
// buffer), reemplazadas por las de aurora.

#include "graphics/frame_interpolation.h"
#include "graphics/gx_draw.h"
#include "native_functions.h"

#include <dolphin/gx.h>

namespace {

// GXSetBlendMode(GXBlendMode type, GXBlendFactor source, GXBlendFactor destination,
// GXLogicOp operation)
void GXSetBlendModeNative(uint32_t type, uint32_t source, uint32_t destination, uint32_t operation) {
    gx_draw::finish_primitive();
    frame_interpolation::run([=] {
        GXSetBlendMode(static_cast<GXBlendMode>(type), static_cast<GXBlendFactor>(source),
                       static_cast<GXBlendFactor>(destination), static_cast<GXLogicOp>(operation));
    });
}
PPC_NATIVE_OVERRIDE_VOID(80194B80, GXSetBlendModeNative,
                         (uint32_t type, uint32_t source, uint32_t destination, uint32_t operation),
                         (type, source, destination, operation));

// GXSetColorUpdate(GXBool enable)
void GXSetColorUpdateNative(uint32_t enable) {
    gx_draw::finish_primitive();
    frame_interpolation::run([=] { GXSetColorUpdate(static_cast<GXBool>(enable)); });
}
PPC_NATIVE_OVERRIDE_VOID(80194BD4, GXSetColorUpdateNative, (uint32_t enable), (enable));

// GXSetAlphaUpdate(GXBool enable)
void GXSetAlphaUpdateNative(uint32_t enable) {
    gx_draw::finish_primitive();
    frame_interpolation::run([=] { GXSetAlphaUpdate(static_cast<GXBool>(enable)); });
}
PPC_NATIVE_OVERRIDE_VOID(80194C00, GXSetAlphaUpdateNative, (uint32_t enable), (enable));

// GXSetZMode(GXBool compare, GXCompare function, GXBool update)
void GXSetZModeNative(uint32_t compare, uint32_t function, uint32_t update) {
    gx_draw::finish_primitive();
    frame_interpolation::run([=] {
        GXSetZMode(static_cast<GXBool>(compare), static_cast<GXCompare>(function), static_cast<GXBool>(update));
    });
}
PPC_NATIVE_OVERRIDE_VOID(80194C2C, GXSetZModeNative, (uint32_t compare, uint32_t function, uint32_t update),
                         (compare, function, update));

// GXSetZCompLoc(GXBool before_texture)
void GXSetZCompLocNative(uint32_t before_texture) {
    gx_draw::finish_primitive();
    frame_interpolation::run([=] { GXSetZCompLoc(static_cast<GXBool>(before_texture)); });
}
PPC_NATIVE_OVERRIDE_VOID(80194C60, GXSetZCompLocNative, (uint32_t before_texture), (before_texture));

// GXSetPixelFmt(GXPixelFmt pixel_format, GXZFmt16 depth_format)
void GXSetPixelFmtNative(uint32_t pixel_format, uint32_t depth_format) {
    gx_draw::finish_primitive();
    frame_interpolation::run(
        [=] { GXSetPixelFmt(static_cast<GXPixelFmt>(pixel_format), static_cast<GXZFmt16>(depth_format)); });
}
PPC_NATIVE_OVERRIDE_VOID(80194C94, GXSetPixelFmtNative, (uint32_t pixel_format, uint32_t depth_format),
                         (pixel_format, depth_format));

}  // namespace
