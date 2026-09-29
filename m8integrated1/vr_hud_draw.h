/* Per-eye HUD overlay for immersive VR (Phase D/E/F V1).
 * DrawList2D records composited into the eye present pass: ship-fixed
 * reticle (projected ship-forward anchor), world-space target boxes,
 * off-screen target arrow, status/target/threat text, VR-safe damage
 * overlay. Positions derive from 3D state every frame (correct under
 * head motion); pixels are head-locked overlay (readable, standard V1).
 * Build+Prepare run with NO active pass; Composite runs inside the
 * present pass after the present chain. */
#ifndef M8INTEGRATED1_VR_HUD_DRAW_H
#define M8INTEGRATED1_VR_HUD_DRAW_H

#include "vr_flight_bridge.h"
#include "aeron/render.h"

void VrHud_BuildEye(int eye, const VrFlightSnapshot *s, const float vp[16], int w, int h);
void VrHud_PrepareEye(int eye, AeronCommandBuffer *cmd);
void VrHud_CompositeEye(int eye, AeronCommandBuffer *cmd, AeronRenderPass *pass,
                        AeronRenderTarget *target);
void VrHud_Reset(void);

#endif
