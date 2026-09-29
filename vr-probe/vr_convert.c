/* VR-PROBE XR->engine conversion (isolated test code).
 * Verified two ways: offline Python reimplementation of these exact formulas
 * (identity/yaw/pitch/IPD/behind cases) and the runtime SelfTest below,
 * which runs the REAL AeronScene_ComputeViewProj. */
#include "vr_convert.h"
#include "vr_log.h"

#include <math.h>
#include <string.h>

/* Hamilton product, (w,x,y,z) order, right operand applied first. */
static void qmul(const float a[4], const float b[4], float out[4]) {
    out[0] = a[0] * b[0] - a[1] * b[1] - a[2] * b[2] - a[3] * b[3];
    out[1] = a[0] * b[1] + a[1] * b[0] + a[2] * b[3] - a[3] * b[2];
    out[2] = a[0] * b[2] - a[1] * b[3] + a[2] * b[0] + a[3] * b[1];
    out[3] = a[0] * b[3] + a[1] * b[2] - a[2] * b[1] + a[3] * b[0];
}

static void qconj(const float q[4], float out[4]) {
    out[0] = q[0];
    out[1] = -q[1];
    out[2] = -q[2];
    out[3] = -q[3];
}

/* Verbatim copy of scene_quat_to_mat3 (aeron scene3d.c / util.c). */
static void qmat(const float q[4], float m[9]) {
    const float w = q[0], x = q[1], y = q[2], z = q[3];
    const float xx = x * x, yy = y * y, zz = z * z;
    const float xy = x * y, xz = x * z, yz = y * z;
    const float wx = w * x, wy = w * y, wz = w * z;
    m[0] = 1.0f - 2.0f * (yy + zz);
    m[1] = 2.0f * (xy - wz);
    m[2] = 2.0f * (xz + wy);
    m[3] = 2.0f * (xy + wz);
    m[4] = 1.0f - 2.0f * (xx + zz);
    m[5] = 2.0f * (yz - wx);
    m[6] = 2.0f * (xz - wy);
    m[7] = 2.0f * (yz + wx);
    m[8] = 1.0f - 2.0f * (xx + yy);
}

/* D = 180 deg about X, (w,x,y,z) = (0,1,0,0). */
static const float kQD[4] = { 0.0f, 1.0f, 0.0f, 0.0f };

void VrConvert_Camera(const XrPosef *pose, float h_half, float v_half, float near_z,
                      int vp_w, int vp_h, AeronSceneCamera *out) {
    float qx[4], qc[4], qs[4];
    memset(out, 0, sizeof *out);
    out->pos[0] = pose->position.x;
    out->pos[1] = pose->position.y;
    out->pos[2] = pose->position.z;
    qx[0] = pose->orientation.w;
    qx[1] = pose->orientation.x;
    qx[2] = pose->orientation.y;
    qx[3] = pose->orientation.z;
    qconj(qx, qc);
    qmul(kQD, qc, qs);
    out->ori[0] = qs[0];
    out->ori[1] = qs[1];
    out->ori[2] = qs[2];
    out->ori[3] = qs[3];
    out->h_half_rad = h_half;
    out->v_half_rad = v_half;
    out->near_z = near_z;
    out->viewport.x = 0;
    out->viewport.y = 0;
    out->viewport.width = vp_w;
    out->viewport.height = vp_h;
}

void VrConvert_Rotate(const XrPosef *pose, const float v[3], float out[3]) {
    float qx[4], m[9];
    qx[0] = pose->orientation.w;
    qx[1] = pose->orientation.x;
    qx[2] = pose->orientation.y;
    qx[3] = pose->orientation.z;
    qmat(qx, m);
    out[0] = m[0] * v[0] + m[1] * v[1] + m[2] * v[2];
    out[1] = m[3] * v[0] + m[4] * v[1] + m[5] * v[2];
    out[2] = m[6] * v[0] + m[7] * v[1] + m[8] * v[2];
}

static int check(const char *name, int cond, float a, float b, float w) {
    VrLog("VRPROBE selftest %s %s (%.4f,%.4f,w=%.2f)", name, cond ? "PASS" : "FAIL", a, b, w);
    return cond ? 1 : 0;
}

static void proj_pt(const AeronSceneCamera *cam, const float p[3], float *nx, float *ny,
                    float *w) {
    float vp[16];
    float cx, cy, cw;
    AeronScene_ComputeViewProj(cam, vp);
    cx = vp[0] * p[0] + vp[1] * p[1] + vp[2] * p[2] + vp[3];
    cy = vp[4] * p[0] + vp[5] * p[1] + vp[6] * p[2] + vp[7];
    cw = vp[12] * p[0] + vp[13] * p[1] + vp[14] * p[2] + vp[15];
    *w = cw;
    *nx = cw != 0.0f ? cx / cw : 0.0f;
    *ny = cw != 0.0f ? cy / cw : 0.0f;
}

int VrConvert_SelfTest(void) {
    int ok = 1;
    XrPosef ident;
    AeronSceneCamera cam;
    float nx, ny, w;
    memset(&ident, 0, sizeof ident);
    ident.orientation.w = 1.0f;

    /* T1: identity head, point ahead centers with w>0. */
    VrConvert_Camera(&ident, 0.8216f, 0.8556f, 0.05f, 1680, 1760, &cam);
    {
        const float p[3] = { 0.0f, 0.0f, -5.0f };
        proj_pt(&cam, p, &nx, &ny, &w);
        ok &= check("T1-center", w > 0.0f && fabsf(nx) < 0.01f && fabsf(ny) < 0.01f, nx, ny, w);
    }
    /* T2: up point maps NDC_y>0 (screen-up through the engine chain). */
    {
        const float p[3] = { 0.0f, 1.0f, -5.0f };
        proj_pt(&cam, p, &nx, &ny, &w);
        ok &= check("T2-up", w > 0.0f && ny > 0.1f, nx, ny, w);
    }
    /* T3: right point maps NDC_x>0. */
    {
        const float p[3] = { 1.0f, 0.0f, -5.0f };
        proj_pt(&cam, p, &nx, &ny, &w);
        ok &= check("T3-right", w > 0.0f && nx > 0.1f, nx, ny, w);
    }
    /* T4: yaw +30deg about Y (look left); fixed ahead point appears right. */
    {
        XrPosef yaw = ident;
        const float h = 15.0f * 3.14159265f / 180.0f;
        const float p[3] = { 0.0f, 0.0f, -5.0f };
        yaw.orientation.w = cosf(h);
        yaw.orientation.y = sinf(h);
        VrConvert_Camera(&yaw, 0.8216f, 0.8556f, 0.05f, 1680, 1760, &cam);
        proj_pt(&cam, p, &nx, &ny, &w);
        ok &= check("T4-yaw-sign", w > 0.0f && nx > 0.4f && nx < 0.7f, nx, ny, w);
    }
    /* T5: pitch down 20deg about X; fixed ahead point appears up. */
    {
        XrPosef pitch = ident;
        const float h = -10.0f * 3.14159265f / 180.0f;
        const float p[3] = { 0.0f, 0.0f, -5.0f };
        pitch.orientation.w = cosf(h);
        pitch.orientation.x = sinf(h);
        VrConvert_Camera(&pitch, 0.8216f, 0.8556f, 0.05f, 1680, 1760, &cam);
        proj_pt(&cam, p, &nx, &ny, &w);
        ok &= check("T5-pitch-sign", w > 0.0f && ny > 0.2f && ny < 0.6f, nx, ny, w);
    }
    /* T6: behind point rejected (w<0). */
    {
        const float p[3] = { 0.0f, 0.0f, 5.0f };
        VrConvert_Camera(&ident, 0.8216f, 0.8556f, 0.05f, 1680, 1760, &cam);
        proj_pt(&cam, p, &nx, &ny, &w);
        ok &= check("T6-behind", w < 0.0f, nx, ny, w);
    }
    /* T7: IPD disparity sign (eyes +-0.032, identity). */
    {
        AeronSceneCamera cl, cr;
        const float p[3] = { 0.0f, -0.15f, -2.0f };
        float nlx, nly, wl, nrx, nry, wr;
        VrConvert_Camera(&ident, 0.8216f, 0.8556f, 0.05f, 1680, 1760, &cl);
        cr = cl;
        cl.pos[0] = -0.032f;
        cr.pos[0] = 0.032f;
        proj_pt(&cl, p, &nlx, &nly, &wl);
        proj_pt(&cr, p, &nrx, &nry, &wr);
        ok &= check("T7-ipd", wl > 0.0f && wr > 0.0f && nlx > nrx && (nlx - nrx) < 0.1f,
                    nlx, nrx, wl);
    }
    VrLog("VRPROBE selftest %s", ok ? "ALL PASS" : "FAILURES PRESENT");
    return ok;
}
