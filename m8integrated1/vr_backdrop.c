#include "vr_backdrop.h"
#include <math.h>

static void bd_place(const float eye[3], float dist, const float w[3], float out[3]) {
    float len = sqrtf(w[0] * w[0] + w[1] * w[1] + w[2] * w[2]);
    if (!(len > 1e-6f)) {
        out[0] = eye[0]; out[1] = eye[1]; out[2] = eye[2];
        return;
    }
    float k = dist / len;
    out[0] = eye[0] + w[0] * k;
    out[1] = eye[1] + w[1] * k;
    out[2] = eye[2] + w[2] * k;
}

unsigned VrBd_Derive(const XwaBackdrop *b, VrBdResolveFn resolve, void *user,
                     const float eye_m[3], float dist_m,
                     float corners[][4][3], float uvs[][4][2],
                     VrBdRef *refs, unsigned max_quads) {
    unsigned count = 0;
    float wc[4][3];
    int frame;
    VrBdRef ref;
    if (!b || b->hidden || !resolve || !eye_m || !(dist_m > 0.f) || !max_quads)
        return 0;
    if (!corners || !uvs || !refs)
        return 0;
    frame = (b->flags & 1u) ? b->frame : 1;
    if ((b->flags & 2u) != 0 || b->side <= 3) {
        /* Coordinate strip: segment quads at strip_coords[i] +/- half
         * height on world Z; U slides across each frame's segments. */
        int32_t hh;
        int prev_h;
        int seg_in_frame;
        int seg;
        if (b->strip_segment_count == 0 || b->strip_segments_per_frame == 0)
            return 0;
        if (!resolve(b->model_type, frame, &ref, user))
            return 0;
        hh = b->strip_half_height;
        prev_h = ref.classic_h;
        seg_in_frame = 1;
        for (seg = 1; seg <= b->strip_segment_count && count < max_quads; seg++) {
            float u0, u1;
            const float *c0;
            const float *c1;
            float hhf;
            float su[4], sv[4];
            float du, dv;
            int v;
            if (seg > 1) {
                seg_in_frame++;
                if (seg_in_frame > b->strip_segments_per_frame) {
                    seg_in_frame -= b->strip_segments_per_frame;
                    frame++;
                    if (!resolve(b->model_type, frame, &ref, user))
                        break;
                    if (ref.classic_h > 0 && prev_h > 0 && ref.classic_h != prev_h) {
                        hh = ref.classic_h * hh / prev_h; /* classic int math */
                        prev_h = ref.classic_h;
                    }
                    u0 = 0.0f;
                } else {
                    u0 = (float)(seg_in_frame - 1) / (float)b->strip_segments_per_frame;
                }
            } else {
                u0 = 0.0f;
            }
            u1 = (float)seg_in_frame / (float)b->strip_segments_per_frame;
            c0 = b->strip_coords[seg - 1];
            c1 = b->strip_coords[seg];
            hhf = (float)hh;
            wc[0][0] = c0[0]; wc[0][1] = c0[1]; wc[0][2] = c0[2] + hhf;
            wc[1][0] = c1[0]; wc[1][1] = c1[1]; wc[1][2] = c1[2] + hhf;
            wc[2][0] = c1[0]; wc[2][1] = c1[1]; wc[2][2] = c1[2] - hhf;
            wc[3][0] = c0[0]; wc[3][1] = c0[1]; wc[3][2] = c0[2] - hhf;
            for (v = 0; v < 4; v++)
                bd_place(eye_m, dist_m, wc[v], corners[count][v]);
            su[0] = u0; su[1] = u1; su[2] = u1; su[3] = u0;
            sv[0] = 0.0f; sv[1] = 0.0f; sv[2] = 1.0f; sv[3] = 1.0f;
            du = ref.u1 - ref.u0;
            dv = ref.v1 - ref.v0;
            for (v = 0; v < 4; v++) {
                uvs[count][v][0] = ref.u0 + su[v] * du;
                uvs[count][v][1] = ref.v0 + sv[v] * dv;
            }
            refs[count] = ref;
            count++;
        }
    } else if (b->side == 4 || b->side == 5) {
        /* Axis quad: half width along world Y, half height along world X
         * around direction / 512. Extent = angular_scale *
         * (classic_dim * 1.5) / 256. */
        int ext_w, ext_h;
        float v[3];
        float ew, eh;
        static const float def_uv[4][2] = { { 1, 1 }, { 0, 1 }, { 0, 0 }, { 1, 0 } };
        float du, dv;
        int i;
        if (!resolve(b->model_type, frame, &ref, user))
            return 0;
        if (ref.classic_w <= 0 || ref.classic_h <= 0)
            return 0;
        ext_w = ((int)b->angular_scale * (ref.classic_w + (ref.classic_w >> 1))) >> 8;
        ext_h = ((int)b->angular_scale * (ref.classic_h + (ref.classic_h >> 1))) >> 8;
        v[0] = (float)((int)b->world_dir[0] >> 9); /* classic >>9 */
        v[1] = (float)((int)b->world_dir[1] >> 9);
        v[2] = (float)((int)b->world_dir[2] >> 9);
        ew = (float)ext_w;
        eh = (float)ext_h;
        wc[0][0] = v[0] + eh; wc[0][1] = v[1] + ew; wc[0][2] = v[2];
        wc[1][0] = v[0] + eh; wc[1][1] = v[1] - ew; wc[1][2] = v[2];
        wc[2][0] = v[0] - eh; wc[2][1] = v[1] - ew; wc[2][2] = v[2];
        wc[3][0] = v[0] - eh; wc[3][1] = v[1] + ew; wc[3][2] = v[2];
        for (i = 0; i < 4; i++)
            bd_place(eye_m, dist_m, wc[i], corners[count][i]);
        du = ref.u1 - ref.u0;
        dv = ref.v1 - ref.v0;
        for (i = 0; i < 4; i++) {
            uvs[count][i][0] = ref.u0 + def_uv[i][0] * du;
            uvs[count][i][1] = ref.v0 + def_uv[i][1] * dv;
        }
        refs[count] = ref;
        count++;
    }
    return count;
}
