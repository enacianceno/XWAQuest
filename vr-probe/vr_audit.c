/* VR-PROBE mesh triangle audit (isolated test code, Objective 1).
 * Evidence over assumption: walks what the GPU draws. */
#include "vr_audit.h"
#include "vr_framing.h"
#include "vr_log.h"

#include <math.h>
#include <stdlib.h>
#include <string.h>

static void xform(const float m[16], const float p[3], float out[3]) {
    out[0] = m[0] * p[0] + m[1] * p[1] + m[2] * p[2] + m[3];
    out[1] = m[4] * p[0] + m[5] * p[1] + m[6] * p[2] + m[7];
    out[2] = m[8] * p[0] + m[9] * p[1] + m[10] * p[2] + m[11];
}

static void audit_range(const char *tag, const AeronSceneMesh *mesh, const float model[16],
                        const AeronSceneCamera *camL, const AeronSceneCamera *camR,
                        uint32_t start, uint32_t count, uint32_t *refMark, uint32_t *refCount,
                        float refMin[3], float refMax[3]) {
    uint32_t tris = 0, bad = 0, degen = 0, pos = 0, neg = 0, clipped = 0;
    double areaSum = 0.0;
    if (!mesh->cpu_indices) {
        VrLog("VRPROBE audit %s: no retained cpu_indices", tag);
        return;
    }
    if (start + count > mesh->index_count) {
        VrLog("VRPROBE audit %s: range [%u,%u) outside ibo %u, clamping", tag, start,
              start + count, mesh->index_count);
        if (start >= mesh->index_count) {
            return;
        }
        count = mesh->index_count - start;
    }
    for (uint32_t t = 0; t + 3 <= count; t += 3) {
        uint32_t i0 = mesh->cpu_indices[start + t];
        uint32_t i1 = mesh->cpu_indices[start + t + 1];
        uint32_t i2 = mesh->cpu_indices[start + t + 2];
        float A[3], B[3], C[3], WA[3], WB[3], WC[3];
        float ab[3], ac[3], cr[3], len;
        tris++;
        if (i0 >= mesh->vertex_count || i1 >= mesh->vertex_count ||
            i2 >= mesh->vertex_count) {
            bad++;
            continue;
        }
        if (!mesh->cpu_vertices) {
            bad++;
            continue;
        }
        A[0] = mesh->cpu_vertices[i0].pos[0];
        A[1] = mesh->cpu_vertices[i0].pos[1];
        A[2] = mesh->cpu_vertices[i0].pos[2];
        B[0] = mesh->cpu_vertices[i1].pos[0];
        B[1] = mesh->cpu_vertices[i1].pos[1];
        B[2] = mesh->cpu_vertices[i1].pos[2];
        C[0] = mesh->cpu_vertices[i2].pos[0];
        C[1] = mesh->cpu_vertices[i2].pos[1];
        C[2] = mesh->cpu_vertices[i2].pos[2];
        ab[0] = B[0] - A[0];
        ab[1] = B[1] - A[1];
        ab[2] = B[2] - A[2];
        ac[0] = C[0] - A[0];
        ac[1] = C[1] - A[1];
        ac[2] = C[2] - A[2];
        cr[0] = ab[1] * ac[2] - ab[2] * ac[1];
        cr[1] = ab[2] * ac[0] - ab[0] * ac[2];
        cr[2] = ab[0] * ac[1] - ab[1] * ac[0];
        len = sqrtf(cr[0] * cr[0] + cr[1] * cr[1] + cr[2] * cr[2]);
        if (!(len > 0.0f) || !isfinite(len)) {
            degen++;
            continue;
        }
        xform(model, A, WA);
        xform(model, B, WB);
        xform(model, C, WC);
        {
            const float *P[3] = { WA, WB, WC };
            const uint32_t I[3] = { i0, i1, i2 };
            for (int k = 0; k < 3; k++) {
                int seen = 0;
                for (uint32_t j = 0; j < *refCount; j++) {
                    if (refMark[j] == I[k]) {
                        seen = 1;
                        break;
                    }
                }
                if (!seen && *refCount < mesh->vertex_count) {
                    refMark[(*refCount)++] = I[k];
                    for (int a = 0; a < 3; a++) {
                        if (P[k][a] < refMin[a]) {
                            refMin[a] = P[k][a];
                        }
                        if (P[k][a] > refMax[a]) {
                            refMax[a] = P[k][a];
                        }
                    }
                }
            }
        }
        {
            float ax, ay, aw, bx, by, bw, cx, cy, cw, s;
            int pa = VrFraming_ProjectPoint(camL, WA, &ax, &ay, &aw);
            int pb = VrFraming_ProjectPoint(camL, WB, &bx, &by, &bw);
            int pc = VrFraming_ProjectPoint(camL, WC, &cx, &cy, &cw);
            if (!pa || !pb || !pc) {
                clipped++;
                continue;
            }
            s = (bx - ax) * (by + ay) + (cx - bx) * (cy + by) + (ax - cx) * (ay + cy);
            areaSum += fabs(s) * 0.5;
            if (s > 0.0f) {
                pos++;
            } else {
                neg++;
            }
        }
    }
    VrLog("VRPROBE audit %s: tris=%u bad_idx=%u degen=%u clipped=%u ndc_pos=%u ndc_neg=%u",
          tag, tris, bad, degen, clipped, pos, neg);
    (void)camR;
}

void VrAudit_MeshTriangles(const AeronSceneMesh *mesh, const float model[16],
                           const AeronSceneCamera *camL, const AeronSceneCamera *camR) {
    uint32_t *refMark;
    uint32_t refCount = 0;
    float refMin[3] = { 1e30f, 1e30f, 1e30f };
    float refMax[3] = { -1e30f, -1e30f, -1e30f };
    if (!mesh) {
        VrLog("VRPROBE audit: null mesh");
        return;
    }
    VrLog("VRPROBE audit mesh=%p verts=%u idx=%u opaque=%u mask_off=%u mask=%u blend_off=%u blend=%u "
          "cpu_v=%p cpu_i=%p",
          (const void *)mesh, mesh->vertex_count, mesh->index_count,
          mesh->opaque_index_count, mesh->mask_index_offset, mesh->mask_index_count,
          mesh->blend_index_offset, mesh->blend_index_count,
          (const void *)mesh->cpu_vertices, (const void *)mesh->cpu_indices);
    refMark = (uint32_t *)calloc(mesh->vertex_count ? mesh->vertex_count : 1, sizeof *refMark);
    if (!refMark) {
        VrLog("VRPROBE audit: no memory for usage map");
        return;
    }
    audit_range("opaque", mesh, model, camL, camR, 0, mesh->opaque_index_count, refMark,
                &refCount, refMin, refMax);
    audit_range("mask", mesh, model, camL, camR, mesh->mask_index_offset,
                mesh->mask_index_count, refMark, &refCount, refMin, refMax);
    audit_range("blend", mesh, model, camL, camR, mesh->blend_index_offset,
                mesh->blend_index_count, refMark, &refCount, refMin, refMax);
    VrLog("VRPROBE audit referenced verts=%u/%u bounds min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f)",
          refCount, mesh->vertex_count, refMin[0], refMin[1], refMin[2], refMax[0], refMax[1],
          refMax[2]);
    free(refMark);
}
