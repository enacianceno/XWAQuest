#include "vr_stick_logic.h"
#include <math.h>
#include <string.h>

static void st_norm(float v[3], float fx, float fy, float fz) {
    float len = sqrtf(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
    if (len > 1e-6f) {
        v[0] /= len; v[1] /= len; v[2] /= len;
    } else {
        v[0] = fx; v[1] = fy; v[2] = fz;
    }
}

void VrStick_SeatFrame(const float mat[16], float right[3], float fwd[3],
                       float up[3], float pos[3]) {
    right[0] = mat[0]; right[1] = mat[4]; right[2] = mat[8];
    fwd[0] = -mat[1]; fwd[1] = -mat[5]; fwd[2] = -mat[9];
    up[0] = mat[2]; up[1] = mat[6]; up[2] = mat[10];
    pos[0] = mat[3]; pos[1] = mat[7]; pos[2] = mat[11];
    st_norm(right, 1.f, 0.f, 0.f);
    st_norm(fwd, 0.f, -1.f, 0.f);
    st_norm(up, 0.f, 0.f, 1.f);
}

void VrStick_Init(VrStickState *st) {
    memset(st, 0, sizeof *st);
}

void VrStick_RestPose(const float seat_pos[3], const float seat_right[3],
                      const float seat_fwd[3], const float seat_up[3],
                      float base[3], float top[3]) {
    int i;
    for (i = 0; i < 3; i++)
        base[i] = seat_pos[i] + seat_right[i] * VR_STICK_BASE_RIGHT_M +
                  seat_fwd[i] * VR_STICK_BASE_FWD_M + seat_up[i] * VR_STICK_BASE_UP_M;
    for (i = 0; i < 3; i++)
        top[i] = base[i] + seat_up[i] * VR_STICK_HANDLE_LEN_M;
}

void VrStick_HandleTop(const float base[3], const float right[3],
                       const float fwd[3], const float up[3],
                       float dx, float dy, float top[3]) {
    float dir[3];
    int i;
    for (i = 0; i < 3; i++)
        dir[i] = up[i] + right[i] * dx * VR_STICK_TILT + fwd[i] * dy * VR_STICK_TILT;
    st_norm(dir, 0.f, 0.f, 1.f);
    for (i = 0; i < 3; i++)
        top[i] = base[i] + dir[i] * VR_STICK_HANDLE_LEN_M;
}

static float st_deadzone(float v) {
    float a = fabsf(v), s = v < 0.f ? -1.f : 1.f, m;
    if (a < VR_STICK_DEADZONE)
        return 0.f;
    m = (a - VR_STICK_DEADZONE) / (1.f - VR_STICK_DEADZONE);
    if (m > 1.f)
        m = 1.f;
    return s * m;
}

int VrStick_Update(VrStickState *st, const VrStickInput *in,
                   const float seat_pos[3], const float seat_right[3],
                   const float seat_fwd[3], const float seat_up[3],
                   float *out_pitch, float *out_roll, int *changed) {
    float base[3], rest[3], d[3];
    float dist, tx, ty;
    int was;
    *out_pitch = 0.f;
    *out_roll = 0.f;
    *changed = 0;
    st->grip_pos[0] = in->grip_pos[0];
    st->grip_pos[1] = in->grip_pos[1];
    st->grip_pos[2] = in->grip_pos[2];
    VrStick_RestPose(seat_pos, seat_right, seat_fwd, seat_up, base, rest);
    was = st->grabbed;
    if (!in->focused || !in->active || !in->pose_valid) {
        st->grabbed = 0;
        st->hover = 0;
        st->grip_valid = 0;
        st->dx = 0.f;
        st->dy = 0.f;
        if (was)
            *changed = 1;
        return 0;
    }
    st->grip_valid = 1;
    d[0] = in->grip_pos[0] - rest[0];
    d[1] = in->grip_pos[1] - rest[1];
    d[2] = in->grip_pos[2] - rest[2];
    dist = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    st->hover = !st->grabbed && dist < VR_STICK_HOVER_R_M;
    if (!st->grabbed && st->hover && in->squeeze > VR_STICK_GRAB_SQUEEZE)
        st->grabbed = 1;
    else if (st->grabbed && in->squeeze < VR_STICK_RELEASE_SQUEEZE)
        st->grabbed = 0;
    if (st->grabbed != was)
        *changed = 1;
    if (!st->grabbed) {
        st->dx = 0.f;
        st->dy = 0.f;
        return 0;
    }
    tx = (d[0] * seat_right[0] + d[1] * seat_right[1] + d[2] * seat_right[2]) / VR_STICK_RANGE_M;
    ty = (d[0] * seat_fwd[0] + d[1] * seat_fwd[1] + d[2] * seat_fwd[2]) / VR_STICK_RANGE_M;
    tx = st_deadzone(tx);
    ty = st_deadzone(ty);
    st->dx += (tx - st->dx) * VR_STICK_SMOOTH;
    st->dy += (ty - st->dy) * VR_STICK_SMOOTH;
    *out_pitch = st->dy;
    *out_roll = st->dx;
    return 1;
}

void VrStick_Quads(const float base[3], const float top[3],
                   const float right[3], const float fwd[3],
                   const float up[3], float corners[4][4][3]) {
    const float hb = 0.035f, hw = 0.016f;
    int i;
    float rb[3], fb[3], ub[3], rh[3], fh[3];
    for (i = 0; i < 3; i++) {
        rb[i] = right[i] * hb;
        fb[i] = fwd[i] * hb;
        ub[i] = up[i] * hb;
        rh[i] = right[i] * hw;
        fh[i] = fwd[i] * hw;
    }
    /* Base quad A (right-up plane) and B (fwd-up plane). */
    for (i = 0; i < 3; i++) {
        corners[0][0][i] = base[i] - rb[i] + ub[i];
        corners[0][1][i] = base[i] + rb[i] + ub[i];
        corners[0][2][i] = base[i] + rb[i] - ub[i];
        corners[0][3][i] = base[i] - rb[i] - ub[i];
        corners[1][0][i] = base[i] - fb[i] + ub[i];
        corners[1][1][i] = base[i] + fb[i] + ub[i];
        corners[1][2][i] = base[i] + fb[i] - ub[i];
        corners[1][3][i] = base[i] - fb[i] - ub[i];
        corners[2][0][i] = base[i] - rh[i];
        corners[2][1][i] = base[i] + rh[i];
        corners[2][2][i] = top[i] + rh[i];
        corners[2][3][i] = top[i] - rh[i];
        corners[3][0][i] = base[i] - fh[i];
        corners[3][1][i] = base[i] + fh[i];
        corners[3][2][i] = top[i] + fh[i];
        corners[3][3][i] = top[i] - fh[i];
    }
}

void VrStick_HandQuads(const float grip[3], const float right[3],
                       const float fwd[3], const float up[3],
                       float corners[3][4][3]) {
    const float hh = 0.025f;
    int i;
    float rh[3], fh[3], uh[3];
    for (i = 0; i < 3; i++) { rh[i] = right[i] * hh; fh[i] = fwd[i] * hh; uh[i] = up[i] * hh; }
    for (i = 0; i < 3; i++) {
        corners[0][0][i] = grip[i] - rh[i] + uh[i];
        corners[0][1][i] = grip[i] + rh[i] + uh[i];
        corners[0][2][i] = grip[i] + rh[i] - uh[i];
        corners[0][3][i] = grip[i] - rh[i] - uh[i];
        corners[1][0][i] = grip[i] - fh[i] + uh[i];
        corners[1][1][i] = grip[i] + fh[i] + uh[i];
        corners[1][2][i] = grip[i] + fh[i] - uh[i];
        corners[1][3][i] = grip[i] - fh[i] - uh[i];
        corners[2][0][i] = grip[i] - rh[i] + fh[i];
        corners[2][1][i] = grip[i] + rh[i] + fh[i];
        corners[2][2][i] = grip[i] + rh[i] - fh[i];
        corners[2][3][i] = grip[i] - rh[i] - fh[i];
    }
}

void VrStick_LineQuads(const float a[3], const float b[3],
                       float corners[2][4][3]) {
    float d[3], p1[3], p2[3], ref[3] = { 0.f, 0.f, 1.f };
    float len, n;
    const float hw = 0.006f;
    int i, q, v;
    d[0] = b[0] - a[0]; d[1] = b[1] - a[1]; d[2] = b[2] - a[2];
    len = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (!(len > 1e-6f)) {
        for (q = 0; q < 2; q++) for (v = 0; v < 4; v++)
            for (i = 0; i < 3; i++) corners[q][v][i] = a[i];
        return;
    }
    d[0] /= len; d[1] /= len; d[2] /= len;
    if (fabsf(d[2]) > 0.94f) { ref[0] = 1.f; ref[1] = 0.f; ref[2] = 0.f; }
    p1[0] = d[1] * ref[2] - d[2] * ref[1];
    p1[1] = d[2] * ref[0] - d[0] * ref[2];
    p1[2] = d[0] * ref[1] - d[1] * ref[0];
    n = sqrtf(p1[0] * p1[0] + p1[1] * p1[1] + p1[2] * p1[2]);
    p1[0] /= n; p1[1] /= n; p1[2] /= n;
    p2[0] = d[1] * p1[2] - d[2] * p1[1];
    p2[1] = d[2] * p1[0] - d[0] * p1[2];
    p2[2] = d[0] * p1[1] - d[1] * p1[0];
    for (i = 0; i < 3; i++) {
        corners[0][0][i] = a[i] - p1[i] * hw;
        corners[0][1][i] = a[i] + p1[i] * hw;
        corners[0][2][i] = b[i] + p1[i] * hw;
        corners[0][3][i] = b[i] - p1[i] * hw;
        corners[1][0][i] = a[i] - p2[i] * hw;
        corners[1][1][i] = a[i] + p2[i] * hw;
        corners[1][2][i] = b[i] + p2[i] * hw;
        corners[1][3][i] = b[i] - p2[i] * hw;
    }
}

void VrStick_GripQuads(const float top[3], const float right[3],
                       const float fwd[3], const float up[3],
                       float corners[2][4][3]) {
    const float hw = 0.05f, hh = 0.0125f;
    float rw[3], fw[3], uh[3];
    int i;
    for (i = 0; i < 3; i++) {
        rw[i] = right[i] * hw;
        fw[i] = fwd[i] * hw;
        uh[i] = up[i] * hh;
    }
    for (i = 0; i < 3; i++) {
        corners[0][0][i] = top[i] - rw[i] + uh[i];
        corners[0][1][i] = top[i] + rw[i] + uh[i];
        corners[0][2][i] = top[i] + rw[i] - uh[i];
        corners[0][3][i] = top[i] - rw[i] - uh[i];
        corners[1][0][i] = top[i] - fw[i] + uh[i];
        corners[1][1][i] = top[i] + fw[i] + uh[i];
        corners[1][2][i] = top[i] + fw[i] - uh[i];
        corners[1][3][i] = top[i] - fw[i] - uh[i];
    }
}
