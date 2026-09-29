/* VR-PROBE framing audit (isolated test code, Stage 1).
 * Read-only math. Conventions verified against:
 * - aeron/src/scene/scene3d.c scene_quat_to_mat3 (world->eye rotation)
 * - src/xwa_remaster/flight.c XwaRemasterFlight_ProjectView (NDC formulas,
 *   eye[2]<=0 rejected => forward is +Z)
 * - AeronScene_ComputeViewProj (exact clip math Begin() uses). */
#include "vr_framing.h"
#include "vr_log.h"

#include <math.h>
#include <string.h>
/* Verbatim copy of scene_quat_to_mat3 (scene3d.c:30). Any drift is a bug. */
static void quat_to_mat3(const float q[4], float m[9]) {
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

static void analyze_eye(const char *tag, const AeronSceneCamera *cam,
                          const float corners[8][3]);

int VrFraming_ProjectPoint(const AeronSceneCamera *cam, const float world[3],
                           float *nx, float *ny, float *w) {
    float r[9], vp[16], cx, cy, cw;
    float dx = world[0] - cam->pos[0];
    float dy = world[1] - cam->pos[1];
    float dz = world[2] - cam->pos[2];
    float ex, ey, ez;
    quat_to_mat3(cam->ori, r);
    ex = r[0] * dx + r[1] * dy + r[2] * dz;
    ey = r[3] * dx + r[4] * dy + r[5] * dz;
    ez = r[6] * dx + r[7] * dy + r[8] * dz;
    if (ez <= 0.0f) {
        return 0;
    }
    AeronScene_ComputeViewProj(cam, vp);
    cx = vp[0] * world[0] + vp[1] * world[1] + vp[2] * world[2] + vp[3];
    cy = vp[4] * world[0] + vp[5] * world[1] + vp[6] * world[2] + vp[7];
    cw = vp[12] * world[0] + vp[13] * world[1] + vp[14] * world[2] + vp[15];
    if (cw == 0.0f) {
        return 0;
    }
    *nx = cx / cw;
    *ny = cy / cw;
    *w = cw;
    return 1;
}

static void analyze_eye(const char *tag, const AeronSceneCamera *cam,
                        const float corners[8][3]) {    float r[9];
    float vp[16];
    int front = 0, inside = 0;
    quat_to_mat3(cam->ori, r);
    AeronScene_ComputeViewProj(cam, vp);
    VrLog("VRPROBE framing %s campos=(%.3f,%.3f,%.3f) ori=(%.4f,%.4f,%.4f,%.4f) "
          "h_half=%.4f v_half=%.4f off=(%.4f,%.4f)",
          tag, cam->pos[0], cam->pos[1], cam->pos[2], cam->ori[0], cam->ori[1],
          cam->ori[2], cam->ori[3], cam->h_half_rad, cam->v_half_rad,
          cam->proj_x_offset, cam->proj_y_offset);
    for (int i = 0; i < 8; i++) {
        float dx = corners[i][0] - cam->pos[0];
        float dy = corners[i][1] - cam->pos[1];
        float dz = corners[i][2] - cam->pos[2];
        float ex = r[0] * dx + r[1] * dy + r[2] * dz;
        float ey = r[3] * dx + r[4] * dy + r[5] * dz;
        float ez = r[6] * dx + r[7] * dy + r[8] * dz;
        /* Exact clip path Begin() uses (row-major VP, column point). */
        float cx = vp[0] * corners[i][0] + vp[1] * corners[i][1] + vp[2] * corners[i][2] + vp[3];
        float cy = vp[4] * corners[i][0] + vp[5] * corners[i][1] + vp[6] * corners[i][2] + vp[7];
        float cw = vp[12] * corners[i][0] + vp[13] * corners[i][1] + vp[14] * corners[i][2] + vp[15];
        float nx = 0.0f, ny = 0.0f;
        int isFront = ez > 0.0f ? 1 : 0;
        int isIn = 0;
        if (isFront && cw != 0.0f) {
            nx = cx / cw;
            ny = cy / cw;
            /* Cross-check: clip-derived NDC must match the ProjectView form. */
            float px = ex / (ez * tanf(cam->h_half_rad)) + cam->proj_x_offset;
            float py = -ey / (ez * tanf(cam->v_half_rad)) + cam->proj_y_offset;
            if (fabsf(px - nx) > 0.001f || fabsf(py - ny) > 0.001f) {
                VrLog("VRPROBE framing %s corner%d MATH MISMATCH clip=(%.4f,%.4f) proj=(%.4f,%.4f)",
                      tag, i, nx, ny, px, py);
            }
            isIn = (nx >= -1.0f && nx <= 1.0f && ny >= -1.0f && ny <= 1.0f) ? 1 : 0;
        }
        front += isFront;
        inside += isIn;
        VrLog("VRPROBE framing %s corner%d world=(%.3f,%.3f,%.3f) eye=(%.3f,%.3f,%.3f) "
              "w=%.3f ndc=(%.3f,%.3f) %s %s",
              tag, i, corners[i][0], corners[i][1], corners[i][2], ex, ey, ez, cw, nx,
              ny, isFront ? "FRONT" : "BEHIND", isIn ? "INSIDE" : "outside");
    }
    VrLog("VRPROBE framing %s summary front=%d/8 inside=%d/8", tag, front, inside);
}

void VrFraming_Log(const AeronSceneMesh *mesh, const float model[16],
                   const AeronSceneCamera *camL, const AeronSceneCamera *camR, float yaw) {
    float corners[8][3];
    if (!mesh || !camL || !camR) {
        VrLog("VRPROBE framing missing inputs");
        return;
    }
    VrLog("VRPROBE framing mesh=%p verts=%u idx=%u opaque=%u mask=%u blend=%u mats=%u prims=%u "
          "localMin=(%.3f,%.3f,%.3f) localMax=(%.3f,%.3f,%.3f) radius=%.3f yaw=%.3f",
          (const void *)mesh, mesh->vertex_count, mesh->index_count,
          mesh->opaque_index_count, mesh->mask_index_count, mesh->blend_index_count,
          mesh->material_count, mesh->total_prim_count, mesh->bound_min[0],
          mesh->bound_min[1], mesh->bound_min[2], mesh->bound_max[0], mesh->bound_max[1],
          mesh->bound_max[2], mesh->bound_radius, yaw);
    if (mesh->vertex_count == 0 || mesh->index_count == 0) {
        VrLog("VRPROBE framing EMPTY MESH (no geometry to draw)");
    }
    VrLog("VRPROBE framing model rows r0=(%.4f,%.4f,%.4f,%.3f) r1=(%.4f,%.4f,%.4f,%.3f) "
          "r2=(%.4f,%.4f,%.4f,%.3f)",
          model[0], model[1], model[2], model[3], model[4], model[5], model[6], model[7],
          model[8], model[9], model[10], model[11]);
    for (int i = 0; i < 8; i++) {
        float lx = (i & 1) ? mesh->bound_max[0] : mesh->bound_min[0];
        float ly = (i & 2) ? mesh->bound_max[1] : mesh->bound_min[1];
        float lz = (i & 4) ? mesh->bound_max[2] : mesh->bound_min[2];
        corners[i][0] = model[0] * lx + model[1] * ly + model[2] * lz + model[3];
        corners[i][1] = model[4] * lx + model[5] * ly + model[6] * lz + model[7];
        corners[i][2] = model[8] * lx + model[9] * ly + model[10] * lz + model[11];
    }
    {
        float cx = (corners[0][0] + corners[7][0]) * 0.5f;
        float cy = (corners[0][1] + corners[7][1]) * 0.5f;
        float cz = (corners[0][2] + corners[7][2]) * 0.5f;
        float dx = cx - camL->pos[0], dy = cy - camL->pos[1], dz = cz - camL->pos[2];
        float dist = sqrtf(dx * dx + dy * dy + dz * dz);
        float extent = 0.0f;
        for (int i = 0; i < 8; i++) {
            float ex = corners[i][0] - cx, ey = corners[i][1] - cy, ez = corners[i][2] - cz;
            float d = sqrtf(ex * ex + ey * ey + ez * ez);
            if (d > extent) {
                extent = d;
            }
        }
        VrLog("VRPROBE framing center=(%.3f,%.3f,%.3f) dist=%.3fm extent=%.3fm apparent=%.2fdeg",
              cx, cy, cz, dist, extent, dist > 0.0f ? 2.0f * atanf(extent / dist) * 57.2958f : -1.0f);
    }
    analyze_eye("eye0", camL, corners);
    analyze_eye("eye1", camR, corners);
}
