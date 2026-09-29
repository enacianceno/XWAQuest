/* Finite world-locked frontend screen: pure anchor/size math.
 * No XR calls, no SDL, no simulation. CPU-testable on host.
 * Layout-compatible-by-construction with XrVector3f/XrQuaternionf
 * (same float order); call sites copy field-by-field into XrPosef. */
#ifndef M8INTEGRATED1_VR_FRONTEND_SCREEN_H
#define M8INTEGRATED1_VR_FRONTEND_SCREEN_H

#define VR_UI_SCREEN_DISTANCE_M 2.0f
#define VR_UI_SCREEN_WIDTH_M 2.3f
/* UI TEXT-SHIMMER A/B: backing-store scale. 1.0 = control (full-res UI
 * swapchain); 0.5 = experiment (half-res prefiltered UI, same quad
 * pose/size/distance). Revert by setting back to 1.0. */
#define VR_UI_BACKING_SCALE 0.5f

typedef struct VrUiVec3 { float x, y, z; } VrUiVec3;
typedef struct VrUiQuat { float x, y, z, w; } VrUiQuat;

typedef struct VrUiAnchor {
    int valid;
    VrUiVec3 position;    /* screen center in LOCAL space */
    VrUiQuat orientation; /* yaw-only rotation */
    float width_m, height_m;
    unsigned long long anchored_frame;
} VrUiAnchor;

/* Yaw about +Y from a head orientation. Pitch/roll do not contribute. */
float VrUi_YawFromQuat(VrUiQuat q);
/* Normalized lerp of two orientations (hemisphere-safe). */
VrUiQuat VrUi_AverageQuat(VrUiQuat a, VrUiQuat b);
VrUiVec3 VrUi_Midpoint(VrUiVec3 a, VrUiVec3 b);
VrUiQuat VrUi_YawQuat(float yaw);
/* R_y(yaw) * (0,0,-1): direction from head to screen center. */
VrUiVec3 VrUi_ForwardFromYaw(float yaw);
/* Screen center = head_pos + forward * distance; orientation = head yaw. */
void VrUi_AnchorPose(VrUiVec3 head_pos, VrUiQuat head_ori, float distance_m,
                     VrUiVec3 *out_pos, VrUiQuat *out_ori);
/* Quad size preserving content aspect. Returns 0 on invalid input. */
int VrUi_QuadSize(int content_w, int content_h, float width_m,
                  float *out_w, float *out_h);
/* Scaled UI backing-store dims (even, >=2). Anchor/quad geometry is
 * unaffected: callers keep sizing the quad from full content dims. */
int VrUi_BackingSize(int content_w, int content_h, float scale,
                     int *out_w, int *out_h);

#endif
