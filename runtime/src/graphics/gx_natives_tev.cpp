// The GX functions of the texture environment (TEV, the GPU's color combiner), alpha
// compare and fog, replaced by aurora's. Addresses from config/G4AEE9/symbols.txt.
//
// Las funciones de GX del entorno de texturas (TEV, el combinador de colores de la GPU),
// la comparación de alfa y la niebla, reemplazadas por las de aurora.

#include "graphics/frame_interpolation.h"
#include "graphics/guest_values.h"
#include "graphics/gx_draw.h"
#include "native_functions.h"

#include <dolphin/gx.h>

namespace {

constexpr int FOG_RANGE_ENTRIES = 10;  // GXFogAdjTable: u16 r[10]

// GXSetTevOp(GXTevStageID stage, GXTevMode mode)
void GXSetTevOpNative(uint32_t stage, uint32_t mode) {
    gx_draw::finish_primitive();
    frame_interpolation::run([=] { GXSetTevOp(static_cast<GXTevStageID>(stage), static_cast<GXTevMode>(mode)); });
}
PPC_NATIVE_OVERRIDE_VOID(801941F0, GXSetTevOpNative, (uint32_t stage, uint32_t mode), (stage, mode));

// GXSetTevColorIn(GXTevStageID stage, GXTevColorArg a, b, c, d)
void GXSetTevColorInNative(uint32_t stage, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    gx_draw::finish_primitive();
    frame_interpolation::run([=] {
        GXSetTevColorIn(static_cast<GXTevStageID>(stage), static_cast<GXTevColorArg>(a), static_cast<GXTevColorArg>(b),
                        static_cast<GXTevColorArg>(c), static_cast<GXTevColorArg>(d));
    });
}
PPC_NATIVE_OVERRIDE_VOID(8019427C, GXSetTevColorInNative,
                         (uint32_t stage, uint32_t a, uint32_t b, uint32_t c, uint32_t d), (stage, a, b, c, d));

// GXSetTevAlphaIn(GXTevStageID stage, GXTevAlphaArg a, b, c, d)
void GXSetTevAlphaInNative(uint32_t stage, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    gx_draw::finish_primitive();
    frame_interpolation::run([=] {
        GXSetTevAlphaIn(static_cast<GXTevStageID>(stage), static_cast<GXTevAlphaArg>(a), static_cast<GXTevAlphaArg>(b),
                        static_cast<GXTevAlphaArg>(c), static_cast<GXTevAlphaArg>(d));
    });
}
PPC_NATIVE_OVERRIDE_VOID(801942C0, GXSetTevAlphaInNative,
                         (uint32_t stage, uint32_t a, uint32_t b, uint32_t c, uint32_t d), (stage, a, b, c, d));

// GXSetTevColorOp(GXTevStageID stage, GXTevOp operation, GXTevBias bias, GXTevScale scale,
// GXBool clamp, GXTevRegID output)
void GXSetTevColorOpNative(uint32_t stage, uint32_t operation, uint32_t bias, uint32_t scale, uint32_t clamp,
                           uint32_t output) {
    gx_draw::finish_primitive();
    frame_interpolation::run([=] {
        GXSetTevColorOp(static_cast<GXTevStageID>(stage), static_cast<GXTevOp>(operation), static_cast<GXTevBias>(bias),
                        static_cast<GXTevScale>(scale), static_cast<GXBool>(clamp), static_cast<GXTevRegID>(output));
    });
}
PPC_NATIVE_OVERRIDE_VOID(80194304, GXSetTevColorOpNative,
                         (uint32_t stage, uint32_t operation, uint32_t bias, uint32_t scale, uint32_t clamp,
                          uint32_t output),
                         (stage, operation, bias, scale, clamp, output));

// GXSetTevAlphaOp(GXTevStageID stage, GXTevOp operation, GXTevBias bias, GXTevScale scale,
// GXBool clamp, GXTevRegID output)
void GXSetTevAlphaOpNative(uint32_t stage, uint32_t operation, uint32_t bias, uint32_t scale, uint32_t clamp,
                           uint32_t output) {
    gx_draw::finish_primitive();
    frame_interpolation::run([=] {
        GXSetTevAlphaOp(static_cast<GXTevStageID>(stage), static_cast<GXTevOp>(operation), static_cast<GXTevBias>(bias),
                        static_cast<GXTevScale>(scale), static_cast<GXBool>(clamp), static_cast<GXTevRegID>(output));
    });
}
PPC_NATIVE_OVERRIDE_VOID(8019436C, GXSetTevAlphaOpNative,
                         (uint32_t stage, uint32_t operation, uint32_t bias, uint32_t scale, uint32_t clamp,
                          uint32_t output),
                         (stage, operation, bias, scale, clamp, output));

// GXSetTevColor(GXTevRegID register, GXColor color)
void GXSetTevColorNative(uint32_t tev_register, uint32_t color_address) {
    gx_draw::finish_primitive();
    const GXColor color = guest_values::read_color(color_address);
    frame_interpolation::run([=] { GXSetTevColor(static_cast<GXTevRegID>(tev_register), color); });
}
PPC_NATIVE_OVERRIDE_VOID(801943D4, GXSetTevColorNative, (uint32_t tev_register, uint32_t color_address),
                         (tev_register, color_address));

// GXSetAlphaCompare(GXCompare first, u8 first_reference, GXAlphaOp operation,
// GXCompare second, u8 second_reference)
void GXSetAlphaCompareNative(uint32_t first, uint32_t first_reference, uint32_t operation, uint32_t second,
                             uint32_t second_reference) {
    gx_draw::finish_primitive();
    frame_interpolation::run([=] {
        GXSetAlphaCompare(static_cast<GXCompare>(first), static_cast<u8>(first_reference),
                          static_cast<GXAlphaOp>(operation), static_cast<GXCompare>(second),
                          static_cast<u8>(second_reference));
    });
}
PPC_NATIVE_OVERRIDE_VOID(801945B4, GXSetAlphaCompareNative,
                         (uint32_t first, uint32_t first_reference, uint32_t operation, uint32_t second,
                          uint32_t second_reference),
                         (first, first_reference, operation, second, second_reference));

// GXSetTevOrder(GXTevStageID stage, GXTexCoordID coordinate, GXTexMapID map,
// GXChannelID channel)
void GXSetTevOrderNative(uint32_t stage, uint32_t coordinate, uint32_t map, uint32_t channel) {
    gx_draw::finish_primitive();
    frame_interpolation::run([=] {
        GXSetTevOrder(static_cast<GXTevStageID>(stage), static_cast<GXTexCoordID>(coordinate),
                      static_cast<GXTexMapID>(map), static_cast<GXChannelID>(channel));
    });
}
PPC_NATIVE_OVERRIDE_VOID(80194684, GXSetTevOrderNative,
                         (uint32_t stage, uint32_t coordinate, uint32_t map, uint32_t channel),
                         (stage, coordinate, map, channel));

// GXSetNumTevStages(u8 count)
void GXSetNumTevStagesNative(uint32_t count) {
    gx_draw::finish_primitive();
    frame_interpolation::run([=] { GXSetNumTevStages(static_cast<u8>(count)); });
}
PPC_NATIVE_OVERRIDE_VOID(80194820, GXSetNumTevStagesNative, (uint32_t count), (count));

// GXSetFog(GXFogType type, f32 start_z, f32 end_z, f32 near_z, f32 far_z, GXColor color)
void GXSetFogNative(uint32_t type, float start_z, float end_z, float near_z, float far_z, uint32_t color_address) {
    gx_draw::finish_primitive();
    const GXColor color = guest_values::read_color(color_address);
    frame_interpolation::run([=] { GXSetFog(static_cast<GXFogType>(type), start_z, end_z, near_z, far_z, color); });
}
PPC_NATIVE_OVERRIDE_VOID(80194848, GXSetFogNative,
                         (uint32_t type, float start_z, float end_z, float near_z, float far_z, uint32_t color_address),
                         (type, start_z, end_z, near_z, far_z, color_address));

// GXSetFogRangeAdj(GXBool enable, u16 center, GXFogAdjTable* table): ten 16-bit factors.
void GXSetFogRangeAdjNative(uint32_t enable, uint32_t center, uint32_t table_address) {
    gx_draw::finish_primitive();
    GXFogAdjTable table{};
    if (table_address != 0) {
        for (int i = 0; i < FOG_RANGE_ENTRIES; ++i) {
            table.r[i] = guest_memory::read16(table_address + static_cast<uint32_t>(i) * 2u);
        }
    }
    frame_interpolation::run(
        [=]() mutable { GXSetFogRangeAdj(static_cast<GXBool>(enable), static_cast<u16>(center), &table); });
}
PPC_NATIVE_OVERRIDE_VOID(80194A5C, GXSetFogRangeAdjNative, (uint32_t enable, uint32_t center, uint32_t table_address),
                         (enable, center, table_address));

}  // namespace
