// See far_camera.h.

#include "graphics/far_camera.h"

#include "graphics/camera_view.h"

#ifdef HMMM_FAR_CAMERA_MOD
// How far past the game's limit the camera is, from 0 to 1 (mods/far_camera/mod.c).
extern "C" float hm_mm_far_camera_amount(void);
#endif

namespace far_camera {

float zoom() {
#ifdef HMMM_FAR_CAMERA_MOD
    const float amount = hm_mm_far_camera_amount();
    return camera_view::MAXIMUM_ZOOM - amount * (camera_view::MAXIMUM_ZOOM - camera_view::MINIMUM_ZOOM);
#else
    return camera_view::MAXIMUM_ZOOM;
#endif
}

}  // namespace far_camera
