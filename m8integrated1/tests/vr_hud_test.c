/* CPU tests for VR HUD math (Phase D/E). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "vr_hud_math.h"

static int failures;
#define CHECK(cond, name) do { \
    if (!(cond)) { ++failures; printf("FAIL %s (line %d)\n", name, __LINE__); } \
} while (0)
static int near(float a, float b) { return fabsf(a - b) < 1e-3f; }

int main(void) {
    /* Identity-ish VP: NDC = world, w = 1. */
    float vp[16] = { 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1, 0, 0, 0, 0, 1 };
    float x, y, w;
    int behind = -1;
    float p0[3] = { 0.f, 0.f, 0.f };
    CHECK(VrHud_Project(vp, p0, 1000, 800, &x, &y, &w, &behind), "proj-center-ok");
    CHECK(near(x, 500.f) && near(y, 400.f) && near(w, 1.f) && behind == 0, "proj-center");
    float p1[3] = { 1.f, -1.f, 0.f };
    CHECK(VrHud_Project(vp, p1, 1000, 800, &x, &y, &w, &behind), "proj-edge-ok");
    CHECK(near(x, 1000.f) && near(y, 800.f), "proj-edge");
    /* Depth-row VP: w = 5 - z. */
    float vp2[16] = { 2, 0, 0, 0, 0, 2, 0, 0, 0, 0, 1, 0, 0, 0, -1, 5 };
    float p2[3] = { 0.f, 0.f, 3.f };
    CHECK(VrHud_Project(vp2, p2, 1000, 800, &x, &y, &w, &behind), "proj-front-ok");
    CHECK(near(w, 2.f) && behind == 0 && near(x, 500.f), "proj-front");
    float p3[3] = { 0.f, 0.f, 7.f };
    CHECK(!VrHud_Project(vp2, p3, 1000, 800, &x, &y, &w, &behind), "proj-behind-ret");
    CHECK(behind == 1 && near(w, -2.f), "proj-behind");
    CHECK(!VrHud_Project(NULL, p0, 1000, 800, &x, &y, &w, &behind), "proj-null");
    CHECK(!VrHud_Project(vp, p0, 0, 800, &x, &y, &w, &behind), "proj-badw");

    /* Ship forward: identity -> model -Y. */
    float ident[16] = { 1, 0, 0, 10, 0, 1, 0, 20, 0, 0, 1, 30, 0, 0, 0, 1 };
    float fwd[3];
    VrHud_ShipForward(ident, 100.f, fwd);
    CHECK(near(fwd[0], 10.f) && near(fwd[1], -80.f) && near(fwd[2], 30.f), "fwd-ident");
    /* -90 deg about Z; forward = -column1 = -X. */
    float yaw[16] = { 0, 1, 0, 5, -1, 0, 0, 6, 0, 0, 1, 7, 0, 0, 0, 1 };
    VrHud_ShipForward(yaw, 50.f, fwd);
    CHECK(near(fwd[0], -45.f) && near(fwd[1], 6.f) && near(fwd[2], 7.f), "fwd-yaw");

    /* Angular size. */
    CHECK(near(VrHud_AngularHalfPx(2.f, 10.f, 1000.f), 200.f), "ang-basic");
    CHECK(VrHud_AngularHalfPx(2.f, 0.f, 1000.f) == 0.f, "ang-nodepth");
    CHECK(VrHud_AngularHalfPx(-1.f, 10.f, 1000.f) == 0.f, "ang-negrad");

    /* Clamp. */
    float ox, oy;
    CHECK(VrHud_ClampToRect(500.f, 400.f, 1000, 800, 40.f, &ox, &oy) == 1, "clamp-in-ret");
    CHECK(near(ox, 500.f) && near(oy, 400.f), "clamp-in");
    CHECK(VrHud_ClampToRect(5000.f, -100.f, 1000, 800, 40.f, &ox, &oy) == 0, "clamp-out-ret");
    CHECK(near(ox, 960.f) && near(oy, 40.f), "clamp-out");

    /* Behind-camera writes mirrored pixels for caller un-mirroring. */
    CHECK(near(x, 500.f) && near(y, 400.f), "behind-mirror");

    /* Ship genus allowlist. */
    CHECK(VrHud_ShipGenus(XWA_SNAP_GENUS_FIGHTER), "genus-fighter");
    CHECK(VrHud_ShipGenus(XWA_SNAP_GENUS_STARSHIP), "genus-ship");
    CHECK(VrHud_ShipGenus(XWA_SNAP_GENUS_ASTEROID), "genus-rock");
    CHECK(!VrHud_ShipGenus(XWA_SNAP_GENUS_PLAYER_PROJECTILE), "genus-bolt");
    CHECK(!VrHud_ShipGenus(XWA_SNAP_GENUS_EXPLOSION), "genus-boom");
    CHECK(!VrHud_ShipGenus(XWA_SNAP_GENUS_DEBRIS), "genus-debris");

    /* Targetable genera. */
    CHECK(VrHud_TargetGenus(XWA_SNAP_GENUS_FIGHTER), "tgen-fighter");
    CHECK(VrHud_TargetGenus(XWA_SNAP_GENUS_MINE), "tgen-mine");
    CHECK(VrHud_TargetGenus(XWA_SNAP_GENUS_STARSHIP), "tgen-ship");
    CHECK(!VrHud_TargetGenus(XWA_SNAP_GENUS_ASTEROID), "tgen-rock");
    CHECK(!VrHud_TargetGenus(XWA_SNAP_GENUS_CONTAINER), "tgen-box");
    CHECK(!VrHud_TargetGenus(XWA_SNAP_GENUS_PLAYER_PROJECTILE), "tgen-bolt");
    /* Nearest-cap selection. */
    { XwaFlightObject so[4]; int32_t org[3] = { 0, 0, 0 };
      unsigned ci[4] = { 0, 1, 2, 3 }, win[4];
      memset(so, 0, sizeof so);
      so[0].world_pos[0] = 10; so[1].world_pos[0] = 5;
      so[2].world_pos[0] = 20; so[3].world_pos[0] = 3;
      CHECK(VrHud_SelectNearest(so, 4, org, ci, 4, 2, win) == 2, "sel-count");
      CHECK(win[0] == 3 && win[1] == 1, "sel-order");
      CHECK(VrHud_SelectNearest(so, 4, org, ci, 4, 9, win) == 4, "sel-all");
      CHECK(win[3] == 2, "sel-far");
      CHECK(VrHud_SelectNearest(so, 4, org, ci, 4, 0, win) == 0, "sel-zero");
      CHECK(VrHud_SelectNearest(NULL, 4, org, ci, 4, 2, win) == 0, "sel-null"); }
    /* Range and bearing. */
    { int32_t ra[3] = { 0, 0, 0 }, rb[3] = { 3000, 4000, 0 };
      CHECK(near(VrHud_RangeM(ra, rb, 0.01f), 50.0f), "range-345"); }
    { float bf[3] = { 1.f, 0.f, 0.f }, bp[3] = { 0.f, 0.f, 0.f };
      float t0[3] = { 10.f, 0.f, 0.f }, t1[3] = { -10.f, 0.f, 0.f };
      float t2[3] = { 0.f, 10.f, 0.f }, t3[3] = { 0.f, 0.f, 0.f };
      CHECK(near(VrHud_BearingDeg(bf, bp, t0), 0.f), "brg-ahead");
      CHECK(near(VrHud_BearingDeg(bf, bp, t1), 180.f), "brg-astern");
      CHECK(near(VrHud_BearingDeg(bf, bp, t2), 90.f), "brg-beam");
      CHECK(VrHud_BearingDeg(bf, bp, t3) < 0.f, "brg-degen"); }

    /* Eye forward from quaternion (TARGET_VIEW gaze math). */
    { float q0[4] = { 0.f, 0.f, 0.f, 1.f }, f[3];
      VrHud_EyeForward(q0, f);
      CHECK(near(f[0], 0.f) && near(f[1], 0.f) && near(f[2], -1.f), "eyefwd-id"); }
    { float h = 0.70710678f, f[3]; float qy[4] = { 0.f, 0.f, 0.f, 0.f };
      qy[1] = h; qy[3] = h; /* +90 deg yaw: -Z -> -X */
      VrHud_EyeForward(qy, f);
      CHECK(near(f[0], -1.f) && near(f[1], 0.f) && near(f[2], 0.f), "eyefwd-yaw"); }
    { float h = 0.70710678f, f[3]; float qx[4] = { 0.f, 0.f, 0.f, 0.f };
      qx[0] = h; qx[3] = h; /* +90 deg pitch about X: -Z -> +Y */
      VrHud_EyeForward(qx, f);
      CHECK(near(f[0], 0.f) && near(f[1], 1.f) && near(f[2], 0.f), "eyefwd-pitch"); }
    { float q2[4] = { 0.f, 0.f, 0.f, 2.f }, f[3]; /* non-unit: normalize */
      VrHud_EyeForward(q2, f);
      CHECK(near(f[0], 0.f) && near(f[1], 0.f) && near(f[2], -1.f), "eyefwd-nonunit"); }
    { float qz[4] = { 0.f, 0.f, 0.f, 0.f }, f[3]; /* zero: identity fallback */
      VrHud_EyeForward(qz, f);
      CHECK(near(f[0], 0.f) && near(f[1], 0.f) && near(f[2], -1.f), "eyefwd-zero"); }

    /* Target-marker mode (bridge aid when sim selected nothing). */
    CHECK(VrHud_TargetMarker(0, 0, 1, 2, 0) == 0, "mark-nohud");
    CHECK(VrHud_TargetMarker(1, 1, 1, 2, 0) == 1, "mark-sim");
    CHECK(VrHud_TargetMarker(1, 0, 1, 2, 0) == 2, "mark-bridge");
    CHECK(VrHud_TargetMarker(1, 0, 9, 2, 0) == 0, "mark-badidx");
    CHECK(VrHud_TargetMarker(1, 0, 0, 2, 0) == 0, "mark-player");
    CHECK(VrHud_TargetMarker(1, 1, 9, 2, 0) == 0, "mark-simbad");

    /* Local position convention (TARGET_VIEW regression: [3,7,11]). */
    { float mm[16] = { 1, 0, 0, 10, 0, 1, 0, 20, 0, 0, 1, 30, 0, 0, 0, 1 }, pp[3];
      VrHud_LocalPos(mm, pp);
      CHECK(near(pp[0], 10.f) && near(pp[1], 20.f) && near(pp[2], 30.f), "localpos-off"); }
    { float mm[16] = { 0 }; float pp[3] = { 9.f, 9.f, 9.f };
      VrHud_LocalPos(mm, pp);
      CHECK(near(pp[0], 0.f) && near(pp[1], 0.f) && near(pp[2], 0.f), "localpos-zero"); }

    if (failures) { printf("VR_HUD_TEST FAILURES=%d\n", failures); return 1; }
    printf("VR_HUD_TEST PASS: project, anchor, size, clamp, target, range, eyefwd, marker, localpos\n");
    return 0;
}
