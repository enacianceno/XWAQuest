#ifndef M8_VR_FLIGHT_RENDERER_H
#define M8_VR_FLIGHT_RENDERER_H
#include "vr_flight_bridge.h"
#include "vr_eye_math.h"
#include "aeron/render.h"
#include "vr_openxr.h"
int VrFlightRenderer_BeginFrame(const VrFlightSnapshot *snapshot);
AeronTexture *VrFlightRenderer_RenderEye(int eye,const VrEyeState *state,
    const VrFlightSnapshot *snapshot,AeronCommandBuffer *cmd);
void VrFlightRenderer_EndFrame(int submitted);
void VrFlightRenderer_Reset(void);
void VrFlightRenderer_ReadEye(const XrView *view,int width,int height,VrEyeState *out);
#endif
