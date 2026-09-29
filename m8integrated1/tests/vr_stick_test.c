/* CPU tests for physical-stick logic (stick V1). */
#include <math.h>
#include <stdio.h>
#include <string.h>
#include "vr_stick_logic.h"

static int failures;
#define CHECK(cond, name) do { \
    if (!(cond)) { ++failures; printf("FAIL %s (line %d)\n", name, __LINE__); } \
} while (0)
static int near(float a, float b) { return fabsf(a - b) < 1e-4f; }

int main(void) {
    float right[3], fwd[3], up[3], pos[3];
    float ident[16] = { 1, 0, 0, 10, 0, 1, 0, 20, 0, 0, 1, 30, 0, 0, 0, 1 };
    VrStick_SeatFrame(ident, right, fwd, up, pos);
    CHECK(near(right[0], 1.f) && near(right[1], 0.f) && near(right[2], 0.f), "seat-right");
    CHECK(near(fwd[0], 0.f) && near(fwd[1], -1.f) && near(fwd[2], 0.f), "seat-fwd");
    CHECK(near(up[0], 0.f) && near(up[1], 0.f) && near(up[2], 1.f), "seat-up");
    CHECK(near(pos[0], 10.f) && near(pos[1], 20.f) && near(pos[2], 30.f), "seat-pos");
    /* Scaled matrix normalizes identically. */
    float scaled[16] = { 2.5f, 0, 0, 1, 0, 2.5f, 0, 2, 0, 0, 2.5f, 3, 0, 0, 0, 1 };
    VrStick_SeatFrame(scaled, right, fwd, up, pos);
    CHECK(near(right[0], 1.f) && near(fwd[1], -1.f) && near(up[2], 1.f), "seat-norm");
    /* -90 deg about Z: right -> -Y, forward -> -X. */
    float yaw[16] = { 0, 1, 0, 5, -1, 0, 0, 6, 0, 0, 1, 7, 0, 0, 0, 1 };
    VrStick_SeatFrame(yaw, right, fwd, up, pos);
    CHECK(near(right[1], -1.f) && near(fwd[0], -1.f) && near(up[2], 1.f), "seat-yaw");

    /* Rest pose from identity seat at (1,2,3). */
    float sp[3] = { 1.f, 2.f, 3.f };
    float sr[3] = { 1.f, 0.f, 0.f }, sf[3] = { 0.f, -1.f, 0.f }, su[3] = { 0.f, 0.f, 1.f };
    float base[3], rest[3];
    VrStick_RestPose(sp, sr, sf, su, base, rest);
    CHECK(near(base[0], 1.02f) && near(base[1], 1.62f) && near(base[2], 2.88f), "rest-base");
    CHECK(near(rest[0], 1.02f) && near(rest[1], 1.62f) && near(rest[2], 3.16f), "rest-top");

    /* State machine. */
    VrStickState st;
    VrStickInput in;
    float pitch, roll;
    int changed = -1;
    VrStick_Init(&st);
    memset(&in, 0, sizeof in);
    CHECK(st.grip_valid == 0, "grip-init");
    CHECK(!VrStick_Update(&st, &in, sp, sr, sf, su, &pitch, &roll, &changed), "idle-ret");
    CHECK(!st.grabbed && changed == 0, "idle-state");
    /* Hover near the handle. */
    in.focused = 1;
    in.active = 1;
    in.pose_valid = 1;
    in.grip_pos[0] = rest[0] + 0.05f;
    in.grip_pos[1] = rest[1];
    in.grip_pos[2] = rest[2];
    in.squeeze = 0.f;
    CHECK(!VrStick_Update(&st, &in, sp, sr, sf, su, &pitch, &roll, &changed), "hover-ret");
    CHECK(st.hover && !st.grabbed && changed == 0, "hover-state");
    /* Wide hover knob: 0.16m still hovers and grabs. */
    in.grip_pos[0] = rest[0] + 0.16f;
    in.grip_pos[1] = rest[1];
    in.grip_pos[2] = rest[2];
    CHECK(!VrStick_Update(&st, &in, sp, sr, sf, su, &pitch, &roll, &changed), "widehover-ret");
    CHECK(st.hover && !st.grabbed, "widehover-state");
    in.squeeze = 0.8f;
    CHECK(VrStick_Update(&st, &in, sp, sr, sf, su, &pitch, &roll, &changed), "widegrab-ret");
    CHECK(st.grabbed && changed == 1, "widegrab-state");
    in.squeeze = 0.1f;
    VrStick_Update(&st, &in, sp, sr, sf, su, &pitch, &roll, &changed);
    /* Grab with squeeze at the rest point. */
    in.grip_pos[0] = rest[0];
    in.grip_pos[1] = rest[1];
    in.grip_pos[2] = rest[2];
    in.squeeze = 0.8f;
    CHECK(VrStick_Update(&st, &in, sp, sr, sf, su, &pitch, &roll, &changed), "grab-ret");
    CHECK(st.grabbed && changed == 1, "grab-state");
    CHECK(near(pitch, 0.f) && near(roll, 0.f), "grab-neutral");
    /* Full forward push: +1 smoothed one step -> 0.4. */
    in.grip_pos[0] = rest[0];
    in.grip_pos[1] = rest[1] - 0.15f;
    in.grip_pos[2] = rest[2];
    CHECK(VrStick_Update(&st, &in, sp, sr, sf, su, &pitch, &roll, &changed), "push-ret");
    CHECK(near(pitch, 0.4f) && near(roll, 0.f), "push-smooth1");
    CHECK(VrStick_Update(&st, &in, sp, sr, sf, su, &pitch, &roll, &changed), "push2-ret");
    CHECK(near(pitch, 0.64f), "push-smooth2");
    /* Release with low squeeze. */
    in.squeeze = 0.1f;
    CHECK(!VrStick_Update(&st, &in, sp, sr, sf, su, &pitch, &roll, &changed), "rel-ret");
    CHECK(!st.grabbed && changed == 1 && near(pitch, 0.f), "rel-state");
    /* Squeeze hysteresis: mid value while hovering does not grab. */
    in.grip_pos[0] = rest[0];
    in.grip_pos[1] = rest[1];
    in.grip_pos[2] = rest[2];
    in.squeeze = 0.5f;
    CHECK(!VrStick_Update(&st, &in, sp, sr, sf, su, &pitch, &roll, &changed), "hyst-nograb");
    CHECK(st.hover && !st.grabbed, "hyst-hover");
    /* Focus loss while grabbed releases cleanly. */
    in.squeeze = 0.8f;
    VrStick_Update(&st, &in, sp, sr, sf, su, &pitch, &roll, &changed);
    CHECK(st.grabbed, "regrab");
    in.focused = 0;
    CHECK(!VrStick_Update(&st, &in, sp, sr, sf, su, &pitch, &roll, &changed), "focus-ret");
    CHECK(!st.grabbed && changed == 1 && near(pitch, 0.f) && near(roll, 0.f), "focus-rel");
    /* Grip carry for the hand-marker visual. */
    CHECK(st.grip_valid == 0, "grip-invalid-focus");
    CHECK(near(st.grip_pos[0], rest[0]) && near(st.grip_pos[1], rest[1]) && near(st.grip_pos[2], rest[2]), "grip-last");
    in.focused = 1;
    VrStick_Update(&st, &in, sp, sr, sf, su, &pitch, &roll, &changed);
    CHECK(st.grip_valid == 1, "grip-valid");
    CHECK(near(st.grip_pos[0], rest[0]) && near(st.grip_pos[1], rest[1]) && near(st.grip_pos[2], rest[2]), "grip-pos");

    /* Handle geometry. */
    float top[3];
    VrStick_HandleTop(base, sr, sf, su, 0.f, 0.f, top);
    CHECK(near(top[0], rest[0]) && near(top[1], rest[1]) && near(top[2], rest[2]), "handle-rest");
    VrStick_HandleTop(base, sr, sf, su, 1.f, 0.f, top);
    CHECK(top[0] > base[0] && near(top[1], base[1]), "handle-tilt");
    float q[4][4][3];
    VrStick_Quads(base, rest, sr, sf, su, q);
    CHECK(near((q[0][0][0] + q[0][1][0] + q[0][2][0] + q[0][3][0]) * 0.25f, base[0]), "quad-center");
    {
        float dx = q[2][0][0] - q[2][1][0], dy = q[2][0][1] - q[2][1][1], dz = q[2][0][2] - q[2][1][2];
        CHECK(near(sqrtf(dx * dx + dy * dy + dz * dz), 0.032f), "quad-width");
    }
    {
        float dx = q[2][0][0] - q[2][3][0], dy = q[2][0][1] - q[2][3][1], dz = q[2][0][2] - q[2][3][2];
        CHECK(near(sqrtf(dx * dx + dy * dy + dz * dz), 0.28f), "quad-length");
    }

    /* Hand marker + connector. */
    float hq[3][4][3];
    VrStick_HandQuads(rest, sr, sf, su, hq);
    CHECK(near((hq[0][0][0] + hq[0][1][0] + hq[0][2][0] + hq[0][3][0]) * 0.25f, rest[0]), "hand-center");
    { float ex = hq[0][1][0] - hq[0][0][0], ey = hq[0][1][1] - hq[0][0][1], ez = hq[0][1][2] - hq[0][0][2];
      CHECK(near(sqrtf(ex * ex + ey * ey + ez * ez), 0.05f), "hand-width"); }
    float lq[2][4][3];
    float la[3] = { 0.f, 0.f, 0.f }, lb[3] = { 0.f, 0.f, 1.f };
    VrStick_LineQuads(la, lb, lq);
    { float sx = lq[0][0][0] - lq[0][3][0], sy = lq[0][0][1] - lq[0][3][1], sz = lq[0][0][2] - lq[0][3][2];
      CHECK(near(sqrtf(sx * sx + sy * sy + sz * sz), 1.0f), "line-length"); }
    { float wx = lq[0][0][0] - lq[0][1][0], wy = lq[0][0][1] - lq[0][1][1], wz = lq[0][0][2] - lq[0][1][2];
      CHECK(near(sqrtf(wx * wx + wy * wy + wz * wz), 0.012f), "line-width"); }
    VrStick_LineQuads(la, la, lq);
    CHECK(lq[0][0][0] == 0.f && lq[1][3][2] == 0.f, "line-degen");
    float gq[2][4][3];
    VrStick_GripQuads(rest, sr, sf, su, gq);
    CHECK(near((gq[0][0][0] + gq[0][1][0] + gq[0][2][0] + gq[0][3][0]) * 0.25f, rest[0]), "gripbar-center");
    { float wx = gq[0][0][0] - gq[0][1][0], wy = gq[0][0][1] - gq[0][1][1], wz = gq[0][0][2] - gq[0][1][2];
      CHECK(near(sqrtf(wx * wx + wy * wy + wz * wz), 0.10f), "gripbar-width"); }
    { float hx = gq[0][1][0] - gq[0][2][0], hy = gq[0][1][1] - gq[0][2][1], hz = gq[0][1][2] - gq[0][2][2];
      CHECK(near(sqrtf(hx * hx + hy * hy + hz * hz), 0.025f), "gripbar-height"); }

    if (failures) { printf("VR_STICK_TEST FAILURES=%d\n", failures); return 1; }
    printf("VR_STICK_TEST PASS: seat, grab, deflect, geom, hand, grip, widehover, gripbar\n");
    return 0;
}
