#ifndef VR_CONVERT_H
#define VR_CONVERT_H

#include "aeron/scene/scene3d.h"

#ifdef HAVE_OPENXR_H
#include <openxr/openxr.h>
#else
#include "../src/video/khronos/openxr/openxr.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* XR (right-handed: +X right, +Y up, -Z forward) -> engine eye
 * (+X right, -Y up, +Z forward). Verified derivation:
 * - OpenXR pose quats map head-local -> world (M1's Mat4_FromXrPose inverts
 *   them for the view), so the world->eye rotation needs the conjugate.
 * - D = 180 deg about X maps (right,up,back) to (right,down,forward);
 *   det=+1 (proper rotation: no mirroring, IPD/depth safe).
 * - R_scene = D * R_xr^T, i.e. q_scene = qD (x) conj(q_xr) in Hamilton
 *   order (right applied first), matching scene_quat_to_mat3's convention.
 * Positions pass through in meters (scenes use view_space_to_meters=1). */
void VrConvert_Camera(const XrPosef *pose, float h_half, float v_half, float near_z,
                      int vp_w, int vp_h, AeronSceneCamera *out);
/* Rotate a vector by a pose orientation (head-local -> world sense). */
void VrConvert_Rotate(const XrPosef *pose, const float v[3], float out[3]);
/* Startup self-test through the REAL AeronScene_ComputeViewProj.
 * Cases: identity centering/up/right, yaw-left sign, pitch-down sign,
 * behind rejection, IPD disparity sign. Returns 1 when all pass. */
int VrConvert_SelfTest(void);

#ifdef __cplusplus
}
#endif

#endif
