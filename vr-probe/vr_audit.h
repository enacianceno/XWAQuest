#ifndef VR_AUDIT_H
#define VR_AUDIT_H

#include "aeron/scene/scene3d.h"
#include "aeron/scene/mesh.h"

#ifdef __cplusplus
extern "C" {
#endif

/* CPU audit of the geometry the GPU will actually draw (Objective 1).
 * Walks the mesh's RETAINED index ranges (opaque/mask/blend) against the
 * retained cpu_vertices/cpu_indices — the same buffers uploaded to the GPU.
 * Per range logs: triangle count, index-range validity, degenerate (zero
 * area) count, NDC winding sign distribution under the audit-time cameras,
 * and world bounds of the REFERENCED vertices (vs the mesh bounds used by
 * the v7 framing audit). Read-only: no scene, resource or render change. */
void VrAudit_MeshTriangles(const AeronSceneMesh *mesh, const float model[16],
                           const AeronSceneCamera *camL, const AeronSceneCamera *camR);

#ifdef __cplusplus
}
#endif

#endif
