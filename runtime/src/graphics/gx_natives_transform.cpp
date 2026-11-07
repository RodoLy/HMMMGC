// The GX functions of transformation: matrices, projection, viewport and scissor,
// replaced by aurora's. The game passes its matrices by address, in big-endian floats.
// Addresses from config/G4AEE9/symbols.txt.
//
// Las funciones de GX de transformación (matrices, proyección, viewport y tijera),
// reemplazadas por las de aurora.

#include "graphics/camera_view.h"
#include "graphics/frame_interpolation.h"
#include "graphics/guest_values.h"
#include "graphics/gx_draw.h"
#include "native_functions.h"

#include <dolphin/gx.h>

#include <algorithm>
#include <array>

namespace {

constexpr size_t MATRIX_2X4_FLOATS = 8;   // a texture matrix of two rows
constexpr size_t MATRIX_3X4_FLOATS = 12;  // Mtx
constexpr size_t MATRIX_4X4_FLOATS = 16;  // Mtx44

// GXSetProjection(Mtx44 matrix, GXProjectionType type)
void GXSetProjectionNative(uint32_t matrix_address, uint32_t type) {
    gx_draw::finish_primitive();
    auto matrix = guest_values::read_floats<MATRIX_4X4_FLOATS>(matrix_address);
    camera_view::adjust_projection(matrix.data(), static_cast<int>(type));  // widescreen and zoom
    if (frame_interpolation::is_recording()) {
        frame_interpolation::record_projection(matrix, type);
        return;
    }
    GXSetProjection(matrix.data(), static_cast<GXProjectionType>(type));
}
PPC_NATIVE_OVERRIDE_VOID(801951F4, GXSetProjectionNative, (uint32_t matrix_address, uint32_t type),
                         (matrix_address, type));

// GXLoadPosMtxImm(Mtx matrix, u32 slot)
void GXLoadPosMtxImmNative(uint32_t matrix_address, uint32_t slot) {
    gx_draw::finish_primitive();
    const auto matrix = guest_values::read_floats<MATRIX_3X4_FLOATS>(matrix_address);
    if (frame_interpolation::is_recording()) {
        frame_interpolation::record_position_matrix(matrix, slot);
        return;
    }
    GXLoadPosMtxImm(matrix.data(), slot);
}
PPC_NATIVE_OVERRIDE_VOID(80195324, GXLoadPosMtxImmNative, (uint32_t matrix_address, uint32_t slot),
                         (matrix_address, slot));

// GXLoadNrmMtxImm(Mtx matrix, u32 slot): the 3x3 part of a 3x4 matrix.
void GXLoadNrmMtxImmNative(uint32_t matrix_address, uint32_t slot) {
    gx_draw::finish_primitive();
    const auto matrix = guest_values::read_floats<MATRIX_3X4_FLOATS>(matrix_address);
    if (frame_interpolation::is_recording()) {
        frame_interpolation::record_normal_matrix(matrix, slot);
        return;
    }
    GXLoadNrmMtxImm(matrix.data(), slot);
}
PPC_NATIVE_OVERRIDE_VOID(80195374, GXLoadNrmMtxImmNative, (uint32_t matrix_address, uint32_t slot),
                         (matrix_address, slot));

// GXLoadTexMtxImm(Mtx matrix, u32 slot, GXTexMtxType type): 3x4, or 2x4 (two rows).
void GXLoadTexMtxImmNative(uint32_t matrix_address, uint32_t slot, uint32_t type) {
    gx_draw::finish_primitive();
    std::array<float, MATRIX_3X4_FLOATS> matrix{};
    if (type == GX_MTX2x4) {
        const auto rows = guest_values::read_floats<MATRIX_2X4_FLOATS>(matrix_address);
        std::copy(rows.begin(), rows.end(), matrix.begin());
    } else {
        matrix = guest_values::read_floats<MATRIX_3X4_FLOATS>(matrix_address);
    }
    frame_interpolation::run([=] { GXLoadTexMtxImm(matrix.data(), slot, static_cast<GXTexMtxType>(type)); });
}
PPC_NATIVE_OVERRIDE_VOID(801953F8, GXLoadTexMtxImmNative, (uint32_t matrix_address, uint32_t slot, uint32_t type),
                         (matrix_address, slot, type));

// GXSetViewportJitter(f32 left, f32 top, f32 width, f32 height, f32 near_z, f32 far_z,
// u32 field)
void GXSetViewportJitterNative(float left, float top, float width, float height, float near_z, float far_z,
                               uint32_t field) {
    gx_draw::finish_primitive();
    frame_interpolation::run([=] { GXSetViewportJitter(left, top, width, height, near_z, far_z, field); });
}
PPC_NATIVE_OVERRIDE_VOID(8019553C, GXSetViewportJitterNative,
                         (float left, float top, float width, float height, float near_z, float far_z, uint32_t field),
                         (left, top, width, height, near_z, far_z, field));

// GXSetViewport(f32 left, f32 top, f32 width, f32 height, f32 near_z, f32 far_z)
void GXSetViewportNative(float left, float top, float width, float height, float near_z, float far_z) {
    gx_draw::finish_primitive();
    frame_interpolation::run([=] { GXSetViewport(left, top, width, height, near_z, far_z); });
}
PPC_NATIVE_OVERRIDE_VOID(80195594, GXSetViewportNative,
                         (float left, float top, float width, float height, float near_z, float far_z),
                         (left, top, width, height, near_z, far_z));

// GXSetScissor(u32 left, u32 top, u32 width, u32 height)
void GXSetScissorNative(uint32_t left, uint32_t top, uint32_t width, uint32_t height) {
    gx_draw::finish_primitive();
    frame_interpolation::run([=] { GXSetScissor(left, top, width, height); });
}
PPC_NATIVE_OVERRIDE_VOID(801955DC, GXSetScissorNative, (uint32_t left, uint32_t top, uint32_t width, uint32_t height),
                         (left, top, width, height));

}  // namespace
