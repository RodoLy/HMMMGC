// The GX functions of transformation: matrices, projection, viewport and scissor,
// replaced by aurora's, and GXProject, worked out here. The game passes its matrices by
// address, in big-endian floats. Addresses from config/G4AEE9/symbols.txt.
//
// Las funciones de GX de transformación (matrices, proyección, viewport y tijera),
// reemplazadas por las de aurora, y GXProject, calculada aquí.

#include "graphics/camera_view.h"
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
constexpr size_t PROJECTION_NUMBERS = 7;  // GXGetProjectionv: the type and six entries
constexpr size_t VIEWPORT_NUMBERS = 6;    // GXGetViewportv: left, top, width, height, near, far

// GXSetProjection(Mtx44 matrix, GXProjectionType type)
void GXSetProjectionNative(uint32_t matrix_address, uint32_t type) {
    gx_draw::finish_primitive();
    auto matrix = guest_values::read_floats<MATRIX_4X4_FLOATS>(matrix_address);
    camera_view::adjust_projection(matrix.data(), static_cast<int>(type));  // widescreen and zoom
    GXSetProjection(matrix.data(), static_cast<GXProjectionType>(type));
}
PPC_NATIVE_OVERRIDE_VOID(801951F4, GXSetProjectionNative, (uint32_t matrix_address, uint32_t type),
                         (matrix_address, type));

// GXProject(f32 x, f32 y, f32 z, const Mtx view, const f32* projection, const f32* viewport,
// f32* screen_x, f32* screen_y, f32* screen_z): where a point of the scene lands on the
// screen, worked out as the game's own GXProject does it, step by step in single precision.
// The game places its 2D marks over the scene with it (the bubble over a character's head),
// so the projection gets the widescreen and zoom change of the screen's (camera_view.h).
void GXProjectNative(float x, float y, float z, uint32_t view_address, uint32_t projection_address,
                     uint32_t viewport_address, uint32_t screen_x_address, uint32_t screen_y_address,
                     uint32_t screen_z_address) {
    const auto view = guest_values::read_floats<MATRIX_3X4_FLOATS>(view_address);
    auto projection = guest_values::read_floats<PROJECTION_NUMBERS>(projection_address);
    const auto viewport = guest_values::read_floats<VIEWPORT_NUMBERS>(viewport_address);
    camera_view::adjust_projection_numbers(projection.data(), viewport[2], viewport[3]);
    // The point in the camera's space.
    const float eye_x = view[3] + (view[2] * z + (view[0] * x + view[1] * y));
    const float eye_y = view[7] + (view[6] * z + (view[4] * x + view[5] * y));
    const float eye_z = view[11] + (view[10] * z + (view[8] * x + view[9] * y));
    // Where the projection takes it, and the divide by w (by -z in a perspective one).
    float clip_x = 0.f;
    float clip_y = 0.f;
    float clip_z = 0.f;
    float inverse_w = 1.f;
    if (projection[0] == static_cast<float>(GX_PERSPECTIVE)) {
        clip_x = eye_x * projection[1] + eye_z * projection[2];
        clip_y = eye_y * projection[3] + eye_z * projection[4];
        clip_z = projection[6] + eye_z * projection[5];
        inverse_w = 1.f / -eye_z;
    } else {
        clip_x = projection[2] + eye_x * projection[1];
        clip_y = projection[4] + eye_y * projection[3];
        clip_z = projection[6] + eye_z * projection[5];
    }
    // The viewport: x to the right and y down from its top left corner, z between near and far.
    const float screen_x = viewport[2] * 0.5f + (viewport[0] + inverse_w * (clip_x * viewport[2] * 0.5f));
    const float screen_y = viewport[3] * 0.5f + (viewport[1] + inverse_w * (-clip_y * viewport[3] * 0.5f));
    const float screen_z = viewport[5] + inverse_w * (clip_z * (viewport[5] - viewport[4]));
    guest_values::write_float(screen_x_address, screen_x);
    guest_values::write_float(screen_y_address, screen_y);
    guest_values::write_float(screen_z_address, screen_z);
}
PPC_NATIVE_OVERRIDE_VOID(80195080, GXProjectNative,
                         (float x, float y, float z, uint32_t view_address, uint32_t projection_address,
                          uint32_t viewport_address, uint32_t screen_x_address, uint32_t screen_y_address,
                          uint32_t screen_z_address),
                         (x, y, z, view_address, projection_address, viewport_address, screen_x_address,
                          screen_y_address, screen_z_address));

// GXLoadPosMtxImm(Mtx matrix, u32 slot)
void GXLoadPosMtxImmNative(uint32_t matrix_address, uint32_t slot) {
    gx_draw::finish_primitive();
    const auto matrix = guest_values::read_floats<MATRIX_3X4_FLOATS>(matrix_address);
    camera_view::set_position_matrix(matrix.data(), slot);
    GXLoadPosMtxImm(matrix.data(), slot);
}
PPC_NATIVE_OVERRIDE_VOID(80195324, GXLoadPosMtxImmNative, (uint32_t matrix_address, uint32_t slot),
                         (matrix_address, slot));

// GXLoadNrmMtxImm(Mtx matrix, u32 slot): the 3x3 part of a 3x4 matrix.
void GXLoadNrmMtxImmNative(uint32_t matrix_address, uint32_t slot) {
    gx_draw::finish_primitive();
    const auto matrix = guest_values::read_floats<MATRIX_3X4_FLOATS>(matrix_address);
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
        camera_view::adjust_texture_matrix(matrix.data(), static_cast<int>(type));  // screen lookups
    }
    GXLoadTexMtxImm(matrix.data(), slot, static_cast<GXTexMtxType>(type));
}
PPC_NATIVE_OVERRIDE_VOID(801953F8, GXLoadTexMtxImmNative, (uint32_t matrix_address, uint32_t slot, uint32_t type),
                         (matrix_address, slot, type));

// GXSetViewportJitter(f32 left, f32 top, f32 width, f32 height, f32 near_z, f32 far_z,
// u32 field)
void GXSetViewportJitterNative(float left, float top, float width, float height, float near_z, float far_z,
                               uint32_t field) {
    gx_draw::finish_primitive();
    camera_view::set_viewport(width, height);
    GXSetViewportJitter(left, top, width, height, near_z, far_z, field);
}
PPC_NATIVE_OVERRIDE_VOID(8019553C, GXSetViewportJitterNative,
                         (float left, float top, float width, float height, float near_z, float far_z, uint32_t field),
                         (left, top, width, height, near_z, far_z, field));

// GXSetViewport(f32 left, f32 top, f32 width, f32 height, f32 near_z, f32 far_z)
void GXSetViewportNative(float left, float top, float width, float height, float near_z, float far_z) {
    gx_draw::finish_primitive();
    camera_view::set_viewport(width, height);
    GXSetViewport(left, top, width, height, near_z, far_z);
}
PPC_NATIVE_OVERRIDE_VOID(80195594, GXSetViewportNative,
                         (float left, float top, float width, float height, float near_z, float far_z),
                         (left, top, width, height, near_z, far_z));

// GXSetScissor(u32 left, u32 top, u32 width, u32 height)
void GXSetScissorNative(uint32_t left, uint32_t top, uint32_t width, uint32_t height) {
    gx_draw::finish_primitive();
    GXSetScissor(left, top, width, height);
}
PPC_NATIVE_OVERRIDE_VOID(801955DC, GXSetScissorNative, (uint32_t left, uint32_t top, uint32_t width, uint32_t height),
                         (left, top, width, height));

}  // namespace
