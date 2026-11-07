// The GX functions that start the graphics library and synchronize with it, replaced by
// aurora's (see aurora_host.h). Addresses from config/G4AEE9/symbols.txt.
//
// Las funciones de GX que arrancan la biblioteca de gráficos y se sincronizan con ella,
// reemplazadas por las de aurora.

#include "graphics/aurora_host.h"
#include "graphics/frame_interpolation.h"
#include "graphics/guest_values.h"
#include "graphics/gx_draw.h"
#include "native_functions.h"

#include <dolphin/gx.h>

namespace {

// The game's draw-sync callback. aurora runs no game code, and the game never sends the
// draw-sync token that would call it, so it is only kept for GXSetDrawSyncCallback to
// return.
uint32_t g_draw_sync_callback = 0;

// GXInit(void* base, u32 size): the game's command FIFO. The graphics start here, so this
// is where the window opens. The game keeps the returned FIFO object and never uses it;
// the guest address of its FIFO buffer stands in for it.
uint32_t GXInitNative(uint32_t fifo_address, uint32_t fifo_size) {
    aurora_host::initialize();
    GXInit(const_cast<void*>(guest_values::host_data(fifo_address, fifo_size)), fifo_size);
    return fifo_address;
}
PPC_NATIVE_OVERRIDE(8018F234, GXInitNative, uint32_t, (uint32_t fifo_address, uint32_t fifo_size),
                    (fifo_address, fifo_size));

// GXSetMisc(GXMiscToken token, u32 value)
void GXSetMiscNative(uint32_t token, uint32_t value) {
    gx_draw::finish_primitive();
    frame_interpolation::run([=] { GXSetMisc(static_cast<GXMiscToken>(token), value); });
}
PPC_NATIVE_OVERRIDE_VOID(801915C0, GXSetMiscNative, (uint32_t token, uint32_t value), (token, value));

// GXDrawDone(void): waits until the GPU has drawn everything sent so far.
void GXDrawDoneNative() {
    gx_draw::finish_primitive();
    frame_interpolation::run([] { GXDrawDone(); });
}
PPC_NATIVE_OVERRIDE_VOID(80191850, GXDrawDoneNative, (), ());

// GXPixModeSync(void) and GXTexModeSync(void) order the GPU's own work; aurora keeps that
// order by itself.
void GXPixModeSyncNative() { gx_draw::finish_primitive(); }
PPC_NATIVE_OVERRIDE_VOID(801918D0, GXPixModeSyncNative, (), ());

void GXTexModeSyncNative() { gx_draw::finish_primitive(); }
PPC_NATIVE_OVERRIDE_VOID(801918F4, GXTexModeSyncNative, (), ());

// GXSetDrawSyncCallback(GXDrawSyncCallback callback): returns the previous callback.
uint32_t GXSetDrawSyncCallbackNative(uint32_t callback) {
    const uint32_t previous = g_draw_sync_callback;
    g_draw_sync_callback = callback;
    return previous;
}
PPC_NATIVE_OVERRIDE(80191A3C, GXSetDrawSyncCallbackNative, uint32_t, (uint32_t callback), (callback));

// GXInvalidateVtxCache(void): the GPU's vertex cache; aurora has none.
void GXInvalidateVtxCacheNative() { gx_draw::finish_primitive(); }
PPC_NATIVE_OVERRIDE_VOID(801912F4, GXInvalidateVtxCacheNative, (), ());

// GXSetGPMetric(GXPerf0, GXPerf1) and GXClearGPMetric(void): the GPU's performance
// counters, which nothing on the PC reads.
void GXSetGPMetricNative(uint32_t first_counter, uint32_t second_counter) { gx_draw::finish_primitive(); }
PPC_NATIVE_OVERRIDE_VOID(80195740, GXSetGPMetricNative, (uint32_t first_counter, uint32_t second_counter),
                         (first_counter, second_counter));

void GXClearGPMetricNative() {}
PPC_NATIVE_OVERRIDE_VOID(80195F88, GXClearGPMetricNative, (), ());

}  // namespace
