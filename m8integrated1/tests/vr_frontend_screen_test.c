/* CPU tests for finite frontend-screen anchor/size math. */
#include <math.h>
#include <stdio.h>
#include "vr_frontend_screen.h"

static int failures;
#define CHECK(cond, name) do { \
    if (!(cond)) { ++failures; printf("FAIL %s (line %d)\n", name, __LINE__); } \
} while (0)
static int near(float a, float b) { return fabsf(a - b) < 1e-4f; }
static VrUiQuat qmul(VrUiQuat a, VrUiQuat b) {
    VrUiQuat r;
    r.x = a.w*b.x + a.x*b.w + a.y*b.z - a.z*b.y;
    r.y = a.w*b.y - a.x*b.z + a.y*b.w + a.z*b.x;
    r.z = a.w*b.z + a.x*b.y - a.y*b.x + a.z*b.w;
    r.w = a.w*b.w - a.x*b.x - a.y*b.y - a.z*b.z;
    return r;
}

int main(void) {
    const float PI = 3.14159265f;
    VrUiQuat id = { 0.f, 0.f, 0.f, 1.f };
    CHECK(near(VrUi_YawFromQuat(id), 0.f), "identity-yaw");
    /* Yaw extraction roundtrip, incl. negatives. */
    const float angles[] = { -PI + .01f, -PI / 2.f, -.3f, 0.f, .3f, PI / 2.f, PI - .01f };
    for (unsigned i = 0; i < sizeof angles / sizeof angles[0]; ++i) {
        float back = VrUi_YawFromQuat(VrUi_YawQuat(angles[i]));
        CHECK(near(back, angles[i]), "yaw-roundtrip");
    }
    /* Pitch about X and roll about Z must not leak into yaw. */
    VrUiQuat pitch = { sinf(.2618f), 0.f, 0.f, cosf(.2618f) }; /* 30 deg */
    VrUiQuat roll = { 0.f, 0.f, sinf(.1745f), cosf(.1745f) };   /* 20 deg */
    CHECK(near(VrUi_YawFromQuat(pitch), 0.f), "pitch-no-yaw");
    CHECK(near(VrUi_YawFromQuat(roll), 0.f), "roll-no-yaw");
    VrUiQuat combo = qmul(VrUi_YawQuat(.5236f), pitch); /* yaw 30 + pitch 30 */
    CHECK(fabsf(VrUi_YawFromQuat(combo) - .5236f) < 1e-3f, "combo-yaw");
    /* Averaging. */
    VrUiQuat avg = VrUi_AverageQuat(id, id);
    CHECK(near(avg.w, 1.f) && near(avg.x + avg.y + avg.z, 0.f), "avg-identity");
    VrUiQuat q90 = VrUi_YawQuat(PI / 2.f);
    avg = VrUi_AverageQuat(q90, q90);
    CHECK(near(avg.y, q90.y) && near(avg.w, q90.w), "avg-self");
    VrUiQuat neg = { -q90.x, -q90.y, -q90.z, -q90.w };
    avg = VrUi_AverageQuat(q90, neg);
    float len = sqrtf(avg.x*avg.x + avg.y*avg.y + avg.z*avg.z + avg.w*avg.w);
    CHECK(near(len, 1.f), "avg-hemisphere");
    /* Midpoint across IPD. */
    VrUiVec3 l = { -.032f, 1.6f, .1f }, r = { .032f, 1.6f, .1f };
    VrUiVec3 m = VrUi_Midpoint(l, r);
    CHECK(near(m.x, 0.f) && near(m.y, 1.6f) && near(m.z, .1f), "midpoint-ipd");
    /* Forward vectors. */
    VrUiVec3 f0 = VrUi_ForwardFromYaw(0.f);
    CHECK(near(f0.x, 0.f) && near(f0.y, 0.f) && near(f0.z, -1.f), "fwd-zero");
    VrUiVec3 f90 = VrUi_ForwardFromYaw(PI / 2.f);
    CHECK(near(f90.x, -1.f) && near(f90.y, 0.f) && near(f90.z, 0.f), "fwd-90");
    /* Anchor pose: 2 m in front, head height, yaw-only orientation. */
    VrUiVec3 hp = { .5f, 1.62f, -.3f }, ap;
    VrUiQuat ao;
    VrUi_AnchorPose(hp, id, VR_UI_SCREEN_DISTANCE_M, &ap, &ao);
    CHECK(near(ap.x, .5f) && near(ap.y, 1.62f) && near(ap.z, -2.3f), "anchor-pos");
    CHECK(near(ao.w, 1.f) && near(VrUi_YawFromQuat(ao), 0.f), "anchor-ori");
    VrUi_AnchorPose(hp, qmul(VrUi_YawQuat(-PI / 2.f), pitch), VR_UI_SCREEN_DISTANCE_M, &ap, &ao);
    CHECK(near(ap.x, 2.5f) && near(ap.y, 1.62f) && near(ap.z, -.3f), "anchor-yaw-pos");
    CHECK(near(VrUi_YawFromQuat(ao), -PI / 2.f), "anchor-yaw-ori");
    /* Quad sizes preserve aspect. */
    float w, h;
    CHECK(VrUi_QuadSize(2048, 1536, VR_UI_SCREEN_WIDTH_M, &w, &h) && near(w, 2.3f) && near(h, 1.725f), "quad-4x3");
    CHECK(VrUi_QuadSize(1920, 1080, VR_UI_SCREEN_WIDTH_M, &w, &h) && near(h, 1.29375f), "quad-16x9");
    CHECK(!VrUi_QuadSize(0, 1080, VR_UI_SCREEN_WIDTH_M, &w, &h), "quad-zero-w");
    CHECK(!VrUi_QuadSize(1920, -1, VR_UI_SCREEN_WIDTH_M, &w, &h), "quad-neg-h");
    CHECK(!VrUi_QuadSize(1920, 1080, 0.f, &w, &h), "quad-zero-width");
    /* Backing size: scaled UI swapchain for the text-shimmer A/B. */
    int bw, bh;
    CHECK(VrUi_BackingSize(3664, 1920, 1.0f, &bw, &bh) && bw == 3664 && bh == 1920, "back-control");
    CHECK(VrUi_BackingSize(3664, 1920, 0.5f, &bw, &bh) && bw == 1832 && bh == 960, "back-half");
    CHECK(VrUi_BackingSize(1921, 1081, 0.5f, &bw, &bh) && bw == 960 && bh == 540, "back-even");
    CHECK(!VrUi_BackingSize(0, 1080, 0.5f, &bw, &bh), "back-zero-w");
    CHECK(!VrUi_BackingSize(1920, 1080, 0.f, &bw, &bh), "back-zero-scale");
    if (failures) { printf("VR_FRONTEND_SCREEN_TEST FAILURES=%d\n", failures); return 1; }
    printf("VR_FRONTEND_SCREEN_TEST PASS: yaw, average, anchor, size, backing\n");
    return 0;
}
