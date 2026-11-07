// The GX functions that copy the picture out of the GPU (to the screen, or to a texture),
// replaced by aurora's. Copying to the screen (GXCopyDisp) is where a frame ends, so it
// also hands the frame to aurora_host. Addresses from config/G4AEE9/symbols.txt.
//
// Las funciones de GX que copian la imagen fuera de la GPU (a la pantalla o a una
// textura), reemplazadas por las de aurora. Copiar a la pantalla (GXCopyDisp) es donde
// termina un cuadro, así que también le pasa el cuadro a aurora_host.

#include "graphics/aurora_host.h"
#include "graphics/guest_values.h"
#include "graphics/gx_draw.h"
#include "native_functions.h"

#include <dolphin/gx.h>

namespace {

constexpr int SAMPLE_POINTS = 12;  // GXSetCopyFilter: u8 sample_pattern[12][2]
constexpr int VERTICAL_TAPS = 7;   // GXSetCopyFilter: u8 vfilter[7]

// GXSetDispCopySrc(u16 left, u16 top, u16 width, u16 height)
void GXSetDispCopySrcNative(uint32_t left, uint32_t top, uint32_t width, uint32_t height) {
    gx_draw::finish_primitive();
    GXSetDispCopySrc(static_cast<u16>(left), static_cast<u16>(top), static_cast<u16>(width), static_cast<u16>(height));
}
PPC_NATIVE_OVERRIDE_VOID(801920A4, GXSetDispCopySrcNative,
                         (uint32_t left, uint32_t top, uint32_t width, uint32_t height), (left, top, width, height));

// GXSetTexCopySrc(u16 left, u16 top, u16 width, u16 height)
void GXSetTexCopySrcNative(uint32_t left, uint32_t top, uint32_t width, uint32_t height) {
    gx_draw::finish_primitive();
    GXSetTexCopySrc(static_cast<u16>(left), static_cast<u16>(top), static_cast<u16>(width), static_cast<u16>(height));
}
PPC_NATIVE_OVERRIDE_VOID(80192120, GXSetTexCopySrcNative,
                         (uint32_t left, uint32_t top, uint32_t width, uint32_t height), (left, top, width, height));

// GXSetDispCopyDst(u16 width, u16 height)
void GXSetDispCopyDstNative(uint32_t width, uint32_t height) {
    gx_draw::finish_primitive();
    GXSetDispCopyDst(static_cast<u16>(width), static_cast<u16>(height));
}
PPC_NATIVE_OVERRIDE_VOID(8019219C, GXSetDispCopyDstNative, (uint32_t width, uint32_t height), (width, height));

// GXSetTexCopyDst(u16 width, u16 height, GXTexFmt format, GXBool mipmap)
void GXSetTexCopyDstNative(uint32_t width, uint32_t height, uint32_t format, uint32_t mipmap) {
    gx_draw::finish_primitive();
    GXSetTexCopyDst(static_cast<u16>(width), static_cast<u16>(height), static_cast<GXTexFmt>(format),
                    static_cast<GXBool>(mipmap));
}
PPC_NATIVE_OVERRIDE_VOID(801921D0, GXSetTexCopyDstNative,
                         (uint32_t width, uint32_t height, uint32_t format, uint32_t mipmap),
                         (width, height, format, mipmap));

// GXSetDispCopyYScale(f32 scale): returns the number of lines of the copy.
uint32_t GXSetDispCopyYScaleNative(float scale) {
    gx_draw::finish_primitive();
    return GXSetDispCopyYScale(scale);
}
PPC_NATIVE_OVERRIDE(801925B4, GXSetDispCopyYScaleNative, uint32_t, (float scale), (scale));

// GXSetCopyClear(GXColor color, u32 depth)
void GXSetCopyClearNative(uint32_t color_address, uint32_t depth) {
    gx_draw::finish_primitive();
    GXSetCopyClear(guest_values::read_color(color_address), depth);
}
PPC_NATIVE_OVERRIDE_VOID(80192680, GXSetCopyClearNative, (uint32_t color_address, uint32_t depth),
                         (color_address, depth));

// GXSetCopyFilter(GXBool antialias, u8 sample_pattern[12][2], GXBool vertical_filter,
// u8 vertical_taps[7])
void GXSetCopyFilterNative(uint32_t antialias, uint32_t pattern_address, uint32_t vertical_filter,
                           uint32_t taps_address) {
    gx_draw::finish_primitive();
    u8 pattern[SAMPLE_POINTS][2]{};
    u8 taps[VERTICAL_TAPS]{};
    for (int point = 0; point < SAMPLE_POINTS && pattern_address != 0; ++point) {
        pattern[point][0] = guest_memory::read8(pattern_address + static_cast<uint32_t>(point) * 2u);
        pattern[point][1] = guest_memory::read8(pattern_address + static_cast<uint32_t>(point) * 2u + 1u);
    }
    for (int tap = 0; tap < VERTICAL_TAPS && taps_address != 0; ++tap) {
        taps[tap] = guest_memory::read8(taps_address + static_cast<uint32_t>(tap));
    }
    GXSetCopyFilter(static_cast<GXBool>(antialias), pattern, static_cast<GXBool>(vertical_filter), taps);
}
PPC_NATIVE_OVERRIDE_VOID(801926F8, GXSetCopyFilterNative,
                         (uint32_t antialias, uint32_t pattern_address, uint32_t vertical_filter,
                          uint32_t taps_address),
                         (antialias, pattern_address, vertical_filter, taps_address));

// GXSetDispCopyGamma(GXGamma gamma)
void GXSetDispCopyGammaNative(uint32_t gamma) {
    gx_draw::finish_primitive();
    GXSetDispCopyGamma(static_cast<GXGamma>(gamma));
}
PPC_NATIVE_OVERRIDE_VOID(80192900, GXSetDispCopyGammaNative, (uint32_t gamma), (gamma));

// GXCopyDisp(void* destination, GXBool clear): the finished picture goes to the external
// frame buffer that the video interface shows. On the PC aurora keeps it as a texture
// and shows it when the frame ends, which is now.
void GXCopyDispNative(uint32_t destination_address, uint32_t clear) {
    gx_draw::finish_primitive();
    GXCopyDisp(const_cast<void*>(guest_values::host_data(destination_address, 1)), static_cast<GXBool>(clear));
    aurora_host::present_frame();
}
PPC_NATIVE_OVERRIDE_VOID(80192914, GXCopyDispNative, (uint32_t destination_address, uint32_t clear),
                         (destination_address, clear));

// GXCopyTex(void* destination, GXBool clear): the picture becomes a texture at that
// address, which the game later reads with GXInitTexObj; aurora matches the two by the
// address.
void GXCopyTexNative(uint32_t destination_address, uint32_t clear) {
    gx_draw::finish_primitive();
    GXCopyTex(const_cast<void*>(guest_values::host_data(destination_address, 1)), static_cast<GXBool>(clear));
}
PPC_NATIVE_OVERRIDE_VOID(80192A7C, GXCopyTexNative, (uint32_t destination_address, uint32_t clear),
                         (destination_address, clear));

}  // namespace
