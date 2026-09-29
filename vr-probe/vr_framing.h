#ifndef VR_FRAMING_H
#define VR_FRAMING_H

#include "aeron/scene/scene3d.h"
#include "aeron/scene/mesh.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CPU-side framing audit (Stage 1: logs only, never touches the scene).
 * Uses the renderer's exact conventions:
 * - eye space: forward = +Z (XwaRemasterFlight_ProjectView rejects eye[2]<=0;
 *   classic screen = center + proj_scale*xy/z), meters.
 * - NDC: ndc_x = ex/(ez*tanH)+offX, ndc_y = -ey/(ez*tanV)+offY
 *   (flight.c XwaRemasterFlight_ProjectView), clip via
 *   AeronScene_ComputeViewProj (the function Begin() itself uses).
 * Reports mesh stats (proves non-empty geometry), world bounds, per-eye
 * front/behind, frustum intersection, distance and apparent size. */
void VrFraming_Log(const AeronSceneMesh *mesh, const float model[16],
                   const AeronSceneCamera *camL, const AeronSceneCamera *camR,
                   float yaw);
/* Project a world point through a camera (engine conventions, same math as
 * the audit). Returns 0 when behind (w<=0). */
int VrFraming_ProjectPoint(const AeronSceneCamera *cam, const float world[3],
                           float *nx, float *ny, float *w);

#ifdef __cplusplus
}
#endif

#endif
