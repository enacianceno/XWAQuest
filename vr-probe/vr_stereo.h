#ifndef VR_STEREO_H
#define VR_STEREO_H

#include <SDL3/SDL.h>
#include "aeron/aeron.h"
#include "aeron/scene/scene3d.h"
#include "aeron/scene/present.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Two fully independent AeronScene3D (one per eye): separate cameras,
 * separate history, temporal/post disabled so no information leaks
 * between eyes. World units are METERS (view_space_to_meters=1). */
int VrStereo_Init(int w, int h);
void VrStereo_Shutdown(void);
/* Render the real cooked mesh with a world-space model->world matrix
 * (row-major 16 floats, meters) under camera cam. Returns the tonemapped
 * present texture for blitting to the XR swapchain image. */
AeronTexture *VrStereo_RenderEye(AeronCommandBuffer *cmd, int eye,
                                 const AeronSceneCamera *cam,
                                 const AeronSceneMesh *mesh,
                                 const float model_matrix[16],
                                 const float light_dir[3],
                                 const float light_color[3],
                                 float light_intensity,
                                 float emissive);
/* Next n RenderEye calls per eye emit a solid diagnostic color
 * (eye0 red, eye1 green) into the presented RTs instead of the ship. */
void VrStereo_SetDiagFrames(int n);

/* Get the scene texture for a given eye (valid after VrStereo_RenderEye).
 * Returns the HDR scene texture before tonemapping. */
AeronTexture *VrStereo_GetSceneTexture(int eye);

/* v9.7: Draw the real X-Wing mesh with a flat color (magenta=eye0, cyan=eye1)
 * into the scene render target. Proves geometry produces visible fragments
 * independent of PBR/materials. Depth test OFF, depth write OFF. */
void VrStereo_DrawFlatDiagnostic(AeronCommandBuffer *cmd, int eye,
                                 const AeronSceneCamera *cam,
                                 const AeronSceneMesh *mesh,
                                 const float model_matrix[16]);

/* v9.7: Enable/disable flat-color diagnostic draw in VrStereo_RenderEye.
 * When enabled, after AeronScene_Render completes, the flat-color pipeline
 * draws the mesh on top of the scene RT before the present pass. */
extern int g_flatDrawEnabled;

/* v10: Draw a triangle using the V9 pipeline (shadercross VS+FS with vertex input)
 * into the given render target. Returns 1 on success, 0 on failure. */
int VrStereo_DrawTriangle(AeronCommandBuffer *cmd, AeronTexture *color_target);

/* v10: Initialize the triangle pipeline (shadercross VS+FS with vertex input).
 * Returns 1 on success, 0 on failure. */
int VrStereo_InitV10Pipeline(SDL_GPUDevice *dev);

/* X-Wing mesh: create independent V10 vertex/index buffers from AeronSceneMesh
 * and draw using the existing V10 pipeline (shadercross VS+FS, clip space).
 * DrawMesh mirrors VrStereo_DrawTriangle: same ownership, same render-pass
 * pattern, but bound mesh VBO + IBO with an indexed draw. Returns 1 on
 * success, 0 on failure (caller falls back to the triangle diagnostic). */
int VrStereo_CreateMeshBuffers(AeronSceneMesh *mesh);
int VrStereo_DrawMesh(AeronCommandBuffer *cmd, AeronTexture *color_target);

#ifdef __cplusplus
}
#endif

#endif
