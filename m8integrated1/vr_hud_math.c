#include "vr_hud_math.h"
#include <math.h>

int VrHud_Project(const float vp[16], const float world[3], int vp_w, int vp_h,
                  float *out_x, float *out_y, float *out_w, int *behind) {
    float cx, cy, cw, nx, ny;
    if (!vp || !world || !out_x || !out_y || !out_w || !behind)
        return 0;
    if (vp_w <= 0 || vp_h <= 0)
        return 0;
    cx = vp[0] * world[0] + vp[1] * world[1] + vp[2] * world[2] + vp[3];
    cy = vp[4] * world[0] + vp[5] * world[1] + vp[6] * world[2] + vp[7];
    cw = vp[12] * world[0] + vp[13] * world[1] + vp[14] * world[2] + vp[15];
    *out_w = cw;
    *behind = !(cw > 0.f);
    if (cw == 0.f) {
        *out_x = 0.f;
        *out_y = 0.f;
        return 0;
    }
    /* Behind-camera pixels are mirrored; the caller un-mirrors them. */
    nx = cx / cw;
    ny = cy / cw;
    *out_x = (nx * 0.5f + 0.5f) * (float)vp_w;
    *out_y = (1.f - (ny * 0.5f + 0.5f)) * (float)vp_h;
    return !*behind;
}

int VrHud_ShipGenus(uint8_t genus) {
    switch (genus) {
    case XWA_SNAP_GENUS_FIGHTER:
    case XWA_SNAP_GENUS_TRANSPORT:
    case XWA_SNAP_GENUS_UTILITY:
    case XWA_SNAP_GENUS_FREIGHTER:
    case XWA_SNAP_GENUS_STARSHIP:
    case XWA_SNAP_GENUS_PLATFORM:
    case XWA_SNAP_GENUS_WEAPON_EMPLACEMENT:
    case XWA_SNAP_GENUS_MINE:
    case XWA_SNAP_GENUS_SATELLITE_BUOY:
    case XWA_SNAP_GENUS_CONTAINER:
    case XWA_SNAP_GENUS_ASTEROID:
        return 1;
    default:
        return 0;
    }
}

int VrHud_TargetGenus(uint8_t genus) {
    switch (genus) {
    case XWA_SNAP_GENUS_FIGHTER:
    case XWA_SNAP_GENUS_TRANSPORT:
    case XWA_SNAP_GENUS_UTILITY:
    case XWA_SNAP_GENUS_FREIGHTER:
    case XWA_SNAP_GENUS_STARSHIP:
    case XWA_SNAP_GENUS_PLATFORM:
    case XWA_SNAP_GENUS_WEAPON_EMPLACEMENT:
    case XWA_SNAP_GENUS_MINE:
        return 1;
    default:
        return 0;
    }
}

static int64_t hud_d2(const XwaFlightObject *o, const int32_t origin[3]) {
    int64_t dx = (int64_t)o->world_pos[0] - origin[0];
    int64_t dy = (int64_t)o->world_pos[1] - origin[1];
    int64_t dz = (int64_t)o->world_pos[2] - origin[2];
    return dx * dx + dy * dy + dz * dz;
}

unsigned VrHud_SelectNearest(const XwaFlightObject *objs, unsigned n,
                             const int32_t origin[3], const unsigned *idx,
                             unsigned idx_count, unsigned cap, unsigned *out_idx) {
    unsigned winners = 0, i;
    if (!objs || !origin || !idx || !out_idx || !n || !cap) return 0;
    for (i = 0; i < idx_count; ++i) {
        unsigned ci = idx[i], pos;
        int64_t d2;
        if (ci >= n) continue;
        d2 = hud_d2(&objs[ci], origin);
        if (winners >= cap) {
            if (d2 >= hud_d2(&objs[out_idx[cap - 1]], origin)) continue;
            pos = cap - 1;
        } else {
            pos = winners++;
        }
        while (pos > 0 && hud_d2(&objs[out_idx[pos - 1]], origin) > d2) { out_idx[pos] = out_idx[pos - 1]; --pos; }
        out_idx[pos] = ci;
    }
    return winners;
}

float VrHud_RangeM(const int32_t a[3], const int32_t b[3], float metres_per_unit) {
    double dx = (double)a[0] - (double)b[0];
    double dy = (double)a[1] - (double)b[1];
    double dz = (double)a[2] - (double)b[2];
    return (float)(sqrt(dx * dx + dy * dy + dz * dz) * (double)metres_per_unit);
}

float VrHud_BearingDeg(const float fwd[3], const float from[3], const float to[3]) {
    float d[3], dl, dot;
    d[0] = to[0] - from[0]; d[1] = to[1] - from[1]; d[2] = to[2] - from[2];
    dl = sqrtf(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
    if (!(dl > 1e-6f)) return -1.f;
    dot = (fwd[0] * d[0] + fwd[1] * d[1] + fwd[2] * d[2]) / dl;
    if (dot > 1.f) dot = 1.f;
    if (dot < -1.f) dot = -1.f;
    return acosf(dot) * 57.29578f;
}

void VrHud_EyeForward(const float q[4], float fwd[3]) {
    float x = q[0], y = q[1], z = q[2], w = q[3];
    float n = sqrtf(x * x + y * y + z * z + w * w);
    if (n > 1e-6f) { x /= n; y /= n; z /= n; w /= n; }
    else { x = y = z = 0.f; w = 1.f; }
    /* -Z rotated by q: negated third column of the quat rotation matrix. */
    fwd[0] = -2.f * (x * z + w * y);
    fwd[1] = -2.f * (y * z - w * x);
    fwd[2] = 2.f * (x * x + y * y) - 1.f;
}

void VrHud_LocalPos(const float m[16], float p[3]) {
    p[0] = m[3]; p[1] = m[7]; p[2] = m[11];
}

int VrHud_TargetMarker(int hud_usable, int sim_valid, uint32_t target_index,
                       uint32_t object_count, uint32_t player_index) {
    if (!hud_usable) return 0;
    if (target_index >= object_count || target_index == player_index) return 0;
    return sim_valid ? 1 : 2;
}

void VrHud_ShipForward(const float player_mat[16], float dist_m, float out[3]) {
    float fx, fy, fz, len;
    if (!player_mat || !out)
        return;
    /* Columns, OPT -Y forward (matches VrEye_Compose B-frame convention). */
    fx = -player_mat[1];
    fy = -player_mat[5];
    fz = -player_mat[9];
    len = sqrtf(fx * fx + fy * fy + fz * fz);
    if (!(len > 1e-6f)) {
        fx = 0.f; fy = 0.f; fz = -1.f; len = 1.f; /* unreachable, valid ship matrices */
    }
    out[0] = player_mat[3] + fx / len * dist_m;
    out[1] = player_mat[7] + fy / len * dist_m;
    out[2] = player_mat[11] + fz / len * dist_m;
}

float VrHud_AngularHalfPx(float radius_m, float view_depth_m, float focal_px) {
    if (!(radius_m > 0.f) || !(view_depth_m > 1e-3f) || !(focal_px > 0.f))
        return 0.f;
    return radius_m / view_depth_m * focal_px;
}

int VrHud_ClampToRect(float x, float y, int w, int h, float margin,
                      float *out_x, float *out_y) {
    float cx = x, cy = y;
    int inside = 1;
    if (!out_x || !out_y)
        return 0;
    if (cx < margin) { cx = margin; inside = 0; }
    if (cy < margin) { cy = margin; inside = 0; }
    if (cx > (float)w - margin) { cx = (float)w - margin; inside = 0; }
    if (cy > (float)h - margin) { cy = (float)h - margin; inside = 0; }
    *out_x = cx;
    *out_y = cy;
    return inside;
}
