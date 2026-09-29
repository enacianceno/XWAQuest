/* Mission-backdrop quad derivation for immersive VR (Phase C).
 * Pure math over snapshot STATE (XwaBackdrop): no XR calls, no SDL, no GPU.
 * CPU-testable on host. Corner math mirrors desktop fl_derive_backdrops
 * (xwa_remaster/flight.c): the camera-rotation round-trip cancels exactly,
 * so strips need no camera rotation at all, and axis quads quantize in
 * world space (sub-degree vs desktop view-space quantization).
 * Output corners are placed at dist_m along each corner direction from
 * the eye (directions at infinity; reversed-Z infinite far plane).
 * V1 has no side-4/5 visibility gate: GPU clipping handles behind-camera
 * quads. Revisit if a pole backdrop bleeds across the view. */
#ifndef M8INTEGRATED1_VR_BACKDROP_H
#define M8INTEGRATED1_VR_BACKDROP_H

#include "xwa_runtime/snapshot/snapshot.h"

/* Resolver output: borrowed texture handle + atlas sub-rect + classic dims.
 * texture is opaque here (AeronTexture* in the renderer). */
typedef struct VrBdRef {
    void *texture;
    float u0, v0, u1, v1;
    int classic_w, classic_h;
} VrBdRef;

/* Resolve (model_type, frame) to a resident texture ref. Returns 0 when
 * unavailable (renderer wraps XwaRemasterAssets_FlightModelFrame). */
typedef int (*VrBdResolveFn)(int model_type, int frame, VrBdRef *out, void *user);

/* Sky distance in metres (must be inside the scene far plane; the scene
 * uses reversed-Z infinite far, so any battle-exceeding distance works). */
#define VR_BD_SKY_DIST_M 100000.0f

/* Derive up to max_quads for one backdrop record. corners/uvs/refs are
 * caller buffers (refs[i] parallels quad i). Returns quad count. */
unsigned VrBd_Derive(const XwaBackdrop *b, VrBdResolveFn resolve, void *user,
                     const float eye_m[3], float dist_m,
                     float corners[][4][3], float uvs[][4][2],
                     VrBdRef *refs, unsigned max_quads);

#endif
