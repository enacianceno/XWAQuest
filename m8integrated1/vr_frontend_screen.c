#include "vr_frontend_screen.h"
#include <math.h>

float VrUi_YawFromQuat(VrUiQuat q) {
    /* yaw = atan2(2(w*y + x*z), 1 - 2(x*x + y*y)).
     * Identity -> 0; pure X pitch -> 0; pure Z roll -> 0;
     * (0,sin(t/2),0,cos(t/2)) -> t. */
    return atan2f(2.f * (q.w * q.y + q.x * q.z),
                  1.f - 2.f * (q.x * q.x + q.y * q.y));
}

VrUiQuat VrUi_AverageQuat(VrUiQuat a, VrUiQuat b) {
    float dot = a.x * b.x + a.y * b.y + a.z * b.z + a.w * b.w;
    VrUiQuat r;
    if (dot < 0.f) { b.x = -b.x; b.y = -b.y; b.z = -b.z; b.w = -b.w; }
    r.x = a.x + b.x; r.y = a.y + b.y; r.z = a.z + b.z; r.w = a.w + b.w;
    float len2 = r.x * r.x + r.y * r.y + r.z * r.z + r.w * r.w;
    if (!(len2 > 1e-12f)) return a;
    float inv = 1.f / sqrtf(len2);
    r.x *= inv; r.y *= inv; r.z *= inv; r.w *= inv;
    return r;
}

VrUiVec3 VrUi_Midpoint(VrUiVec3 a, VrUiVec3 b) {
    VrUiVec3 r = { (a.x + b.x) * .5f, (a.y + b.y) * .5f, (a.z + b.z) * .5f };
    return r;
}

VrUiQuat VrUi_YawQuat(float yaw) {
    VrUiQuat r = { 0.f, sinf(yaw * .5f), 0.f, cosf(yaw * .5f) };
    return r;
}

VrUiVec3 VrUi_ForwardFromYaw(float yaw) {
    VrUiVec3 r = { -sinf(yaw), 0.f, -cosf(yaw) };
    return r;
}

void VrUi_AnchorPose(VrUiVec3 head_pos, VrUiQuat head_ori, float distance_m,
                     VrUiVec3 *out_pos, VrUiQuat *out_ori) {
    float yaw = VrUi_YawFromQuat(head_ori);
    VrUiVec3 fwd = VrUi_ForwardFromYaw(yaw);
    out_pos->x = head_pos.x + fwd.x * distance_m;
    out_pos->y = head_pos.y;
    out_pos->z = head_pos.z + fwd.z * distance_m;
    *out_ori = VrUi_YawQuat(yaw);
}

int VrUi_QuadSize(int content_w, int content_h, float width_m,
                  float *out_w, float *out_h) {
    if (content_w <= 0 || content_h <= 0 || !(width_m > 0.f)) return 0;
    *out_w = width_m;
    *out_h = width_m * (float)content_h / (float)content_w;
    return *out_h > 0.f;
}

int VrUi_BackingSize(int content_w, int content_h, float scale,
                     int *out_w, int *out_h) {
    int w, h;
    if (content_w <= 0 || content_h <= 0 || !(scale > 0.f) || !out_w || !out_h) return 0;
    w = (int)(content_w * scale + 0.5f);
    h = (int)(content_h * scale + 0.5f);
    if (w < 2) w = 2;
    if (h < 2) h = 2;
    *out_w = w & ~1;
    *out_h = h & ~1;
    if (*out_w < 2) *out_w = 2;
    if (*out_h < 2) *out_h = 2;
    return 1;
}
