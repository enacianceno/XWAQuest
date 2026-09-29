/* VR-PROBE async GPU readback (isolated test code).
 * Uses a small staging buffer pool to read back selected pixels from a
 * render target without blocking the GPU pipeline. */
#include "vr_readback.h"
#include "vr_log.h"
#include "internal.h"

#include <string.h>

VrReadback *VrReadback_Create(int num_samples, int num_eyes) {
    if (num_samples > VR_READBACK_MAX_SAMPLES || num_eyes > 2) {
        return NULL;
    }
    VrReadback *rb = (VrReadback *)SDL_calloc(1, sizeof(VrReadback));
    if (!rb) return NULL;
    rb->num_samples = num_samples;
    rb->num_eyes = num_eyes;
    return rb;
}

VrReadback *VrReadback_Init(SDL_GPUDevice *device, int num_samples, int num_eyes) {
    VrReadback *rb = VrReadback_Create(num_samples, num_eyes);
    if (!rb) return NULL;
    rb->device = device;
    /* RGBA16_FLOAT = 4 channels × 2 bytes = 8 bytes per pixel.
     * Each 1×1 pixel download writes 8 bytes at the sample offset. */
    const Uint32 BYTES_PER_PIXEL = 8;
    for (int i = 0; i < num_eyes; i++) {
        SDL_GPUTransferBufferCreateInfo ci = {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
            .size = (Uint32)(num_samples * BYTES_PER_PIXEL),
            .props = 0,
        };
        rb->staging_buffer[i] = SDL_CreateGPUTransferBuffer(device, &ci);
        if (!rb->staging_buffer[i]) {
            VrLog("VRPROBE readback: staging buffer eye%d creation failed: %s",
                  i, SDL_GetError());
            VrReadback_Destroy(rb);
            return NULL;
        }
    }
    /* v9.5: Allocate per-eye row-scan staging buffer.
     * One row at a time; max texture width is 1680 (XR swapchain).
     * Buffer size: 1680 pixels × 8 bytes = 13,440 bytes per eye. */
    const Uint32 ROW_MAX_W = 1680;
    for (int i = 0; i < num_eyes; i++) {
        SDL_GPUTransferBufferCreateInfo rci = {
            .usage = SDL_GPU_TRANSFERBUFFERUSAGE_DOWNLOAD,
            .size = ROW_MAX_W * BYTES_PER_PIXEL,
            .props = 0,
        };
        rb->row_buffer[i] = SDL_CreateGPUTransferBuffer(device, &rci);
        if (!rb->row_buffer[i]) {
            VrLog("VRPROBE readback: row buffer eye%d creation failed: %s",
                  i, SDL_GetError());
            VrReadback_Destroy(rb);
            return NULL;
        }
    }
    rb->row_pending_eye_count = 0;
    rb->row_pending_eyes[0] = rb->row_pending_eyes[1] = -1;
    rb->row_ready = 0;
    VrLog("VRPROBE readback: init ok samples=%d eyes=%d buf=%u bytes row_buf=%u bytes",
          num_samples, num_eyes, (unsigned)(num_samples * BYTES_PER_PIXEL),
          (unsigned)(ROW_MAX_W * BYTES_PER_PIXEL));
    return rb;
}

int VrReadback_Schedule(VrReadback *rb, int eye, AeronTexture *src_texture,
                        const float ndc_points[][2], int num_samples,
                        AeronCommandBuffer *cmd) {
    if (!rb || !rb->staging_buffer[eye] || !src_texture || !src_texture->texture ||
        num_samples > rb->num_samples || !cmd || !cmd->command_buffer) {
        return 0;
    }

    int w = src_texture->width;
    int h = src_texture->height;

    // Prepare copy pass
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(cmd->command_buffer);
    if (!copy_pass) {
        VrLog("VRPROBE readback: failed to begin copy pass: %s", SDL_GetError());
        return 0;
    }

    for (int i = 0; i < num_samples; i++) {
        // Convert NDC to texture coordinates
        float ndc_x = ndc_points[i][0];
        float ndc_y = ndc_points[i][1];
        float tx_f = (ndc_points[i][0] + 1.0f) * 0.5f * (w - 1);
        float ty_f = (1.0f - ndc_points[i][1]) * 0.5f * (h - 1); // flip Y: NDC +Y up, texture +Y down
        int tx = (int)(tx_f + 0.5f);
        int ty = (int)(ty_f + 0.5f);
        if (tx < 0) tx = 0;
        if (tx >= w) tx = w - 1;
        if (ty < 0) ty = 0;
        if (ty >= h) ty = h - 1;

        // Source region in the texture (SDL_GPUTextureRegion)
        SDL_GPUTextureRegion src_region = {
            .texture = src_texture->texture,
            .mip_level = 0,
            .layer = 0,
            .x = (Uint32)tx,
            .y = (Uint32)ty,
            .z = 0,
            .w = 1,
            .h = 1,
            .d = 1
        };

        // Destination transfer info (SDL_GPUTextureTransferInfo)
        // RGBA16_FLOAT = 8 bytes per pixel; offset = sample_index × 8
        SDL_GPUTextureTransferInfo dst_info = {
            .transfer_buffer = rb->staging_buffer[eye],
            .offset = (Uint32)(i * 8),
            .pixels_per_row = 1,
            .rows_per_layer = 1
        };

        SDL_DownloadFromGPUTexture(copy_pass, &src_region, &dst_info);
    }

    SDL_EndGPUCopyPass(copy_pass);

    // Mark samples as pending
    for (int i = 0; i < num_samples; i++) {
        rb->samples[eye][i].ndc_x = ndc_points[i][0];
        rb->samples[eye][i].ndc_y = ndc_points[i][1];
        rb->samples[eye][i].ready = 0;
    }
    rb->pending_count[eye] = num_samples;
    rb->completed_count[eye] = 0;

    VrLog("VRPROBE readback: scheduled eye=%d samples=%d", eye, num_samples);
    return 1;
}

int VrReadback_Collect(VrReadback *rb, int eye, uint8_t *out_rgba) {
    if (!rb || eye < 0 || eye >= rb->num_eyes || !rb->staging_buffer[eye]) {
        return 0;
    }

    // Map the staging buffer
    void *mapped = SDL_MapGPUTransferBuffer(rb->device, rb->staging_buffer[eye], false);
    if (!mapped) {
        return 0;
    }

    /* RGBA16_FLOAT: 8 bytes per pixel (4 channels × 2 bytes half-float, LE).
     * Byte layout: [R_lo R_hi G_lo G_hi B_lo B_hi A_lo A_hi]
     * We store the high byte for quick nonzero detection AND log the raw
     * 16-bit value per channel for accurate diagnostics. */
    int collected = 0;
    for (int i = 0; i < rb->pending_count[eye]; i++) {
        if (!rb->samples[eye][i].ready) {
            uint8_t *src = (uint8_t *)mapped + i * 8;
            uint16_t r16 = (uint16_t)(src[0] | ((uint16_t)src[1] << 8));
            uint16_t g16 = (uint16_t)(src[2] | ((uint16_t)src[3] << 8));
            uint16_t b16 = (uint16_t)(src[4] | ((uint16_t)src[5] << 8));
            uint16_t a16 = (uint16_t)(src[6] | ((uint16_t)src[7] << 8));
            /* Store high byte for legacy compat */
            rb->samples[eye][i].rgba[0] = src[1];
            rb->samples[eye][i].rgba[1] = src[3];
            rb->samples[eye][i].rgba[2] = src[5];
            rb->samples[eye][i].rgba[3] = src[7];
            /* v9.7: Store full float values via half_to_float for correct comparison */
            rb->samples[eye][i].rgba_f[0] = half_to_float(r16);
            rb->samples[eye][i].rgba_f[1] = half_to_float(g16);
            rb->samples[eye][i].rgba_f[2] = half_to_float(b16);
            rb->samples[eye][i].rgba_f[3] = half_to_float(a16);
            rb->samples[eye][i].ready = 1;
            VrLog("VRPROBE readback-sample eye=%d s=%d rgba16=(0x%04X,0x%04X,0x%04X,0x%04X) "
                  "float=(%.6f,%.6f,%.6f,%.6f)",
                  eye, i, r16, g16, b16, a16,
                  rb->samples[eye][i].rgba_f[0], rb->samples[eye][i].rgba_f[1],
                  rb->samples[eye][i].rgba_f[2], rb->samples[eye][i].rgba_f[3]);
            collected++;
        }
        if (out_rgba) {
            out_rgba[i * 4 + 0] = rb->samples[eye][i].rgba[0];
            out_rgba[i * 4 + 1] = rb->samples[eye][i].rgba[1];
            out_rgba[i * 4 + 2] = rb->samples[eye][i].rgba[2];
            out_rgba[i * 4 + 3] = rb->samples[eye][i].rgba[3];
        }
    }

    rb->completed_count[eye] = collected;
    rb->pending_count[eye] = 0;

    SDL_UnmapGPUTransferBuffer(rb->device, rb->staging_buffer[eye]);
    return collected;
}

/* ── v9.5 row-scan ───────────────────────────────────────────────────────── */

int VrReadback_ScheduleRow(VrReadback *rb, int eye,
                           AeronTexture *src_texture, int row_y,
                           AeronCommandBuffer *cmd) {
    if (!rb || !rb->row_buffer[eye] || !src_texture || !src_texture->texture ||
        !cmd || !cmd->command_buffer || rb->row_pending_eye_count >= 2) {
        return 0;
    }
    int w = src_texture->width;
    int h = src_texture->height;
    if (row_y < 0 || row_y >= h || w <= 0) {
        VrLog("VRPROBE row-scan: invalid row_y=%d (h=%d)", row_y, h);
        return 0;
    }

    SDL_GPUCopyPass *cp = SDL_BeginGPUCopyPass(cmd->command_buffer);
    if (!cp) {
        VrLog("VRPROBE row-scan: copy pass failed: %s", SDL_GetError());
        return 0;
    }

    SDL_GPUTextureRegion src_region = {
        .texture  = src_texture->texture,
        .mip_level = 0,
        .layer    = 0,
        .x = 0, .y = (Uint32)row_y, .z = 0,
        .w = (Uint32)w, .h = 1, .d = 1
    };
    SDL_GPUTextureTransferInfo dst_info = {
        .transfer_buffer = rb->row_buffer[eye],
        .offset          = 0,
        .pixels_per_row  = (Uint32)w,
        .rows_per_layer  = 1
    };
    SDL_DownloadFromGPUTexture(cp, &src_region, &dst_info);
    SDL_EndGPUCopyPass(cp);

    rb->row_pending_eyes[rb->row_pending_eye_count++] = eye;
    rb->row_tex_w       = w;
    rb->row_tex_h       = h;
    rb->row_y           = row_y;
    rb->row_ready       = 0;
    VrLog("VRPROBE row-scan: scheduled eye=%d row=%d w=%d", eye, row_y, w);
    return 1;
}

/* Convert a 16-bit IEEE 754 half-float (little-endian) to float.
 * v9.7: Made non-static so vr_main.c can use it for readback comparison. */
float half_to_float(uint16_t h) {
    uint32_t sign = (uint32_t)(h & 0x8000) << 16;
    uint32_t expo = (h >> 10) & 0x1F;
    uint32_t mant = h & 0x03FF;
    uint32_t f;
    if (expo == 0) {
        if (mant == 0) {
            f = sign;
        } else {
            expo = 1;
            while (!(mant & 0x0400)) { mant <<= 1; expo--; }
            mant &= 0x03FF;
            f = sign | ((127 - 15 + expo) << 23) | (mant << 13);
        }
    } else if (expo == 31) {
        f = sign | 0x7F800000 | (mant << 13);
    } else {
        f = sign | ((expo + 127 - 15) << 23) | (mant << 13);
    }
    float val;
    memcpy(&val, &f, 4);
    return val;
}

int VrReadback_CollectRow(VrReadback *rb, int eye,
                          const uint16_t bg_rgba16[4]) {
    if (!rb || !rb->row_buffer[eye]) {
        return 0;
    }
    /* Check if this eye has a pending row download */
    {
        int found = 0;
        for (int i = 0; i < rb->row_pending_eye_count; i++) {
            if (rb->row_pending_eyes[i] == eye) { found = 1; break; }
        }
        if (!found) return 0;
    }

    void *mapped = SDL_MapGPUTransferBuffer(rb->device, rb->row_buffer[eye], false);
    if (!mapped) {
        VrLog("VRPROBE row-scan: map failed eye=%d", eye);
        rb->row_pending_eye_count = 0;
        rb->row_pending_eyes[0] = rb->row_pending_eyes[1] = -1;
        return 0;
    }

    int w = rb->row_tex_w;
    int ty = rb->row_y;
    uint8_t *raw = (uint8_t *)mapped;

    /* Reference background as float for comparison */
    float bg_f[4];
    for (int c = 0; c < 4; c++) bg_f[c] = half_to_float(bg_rgba16[c]);

    /* Threshold: a pixel is "non-background" if any channel differs from
     * the reference by more than 5% of the reference value, or by more
     * than an absolute minimum of 0.001 (to handle near-zero channels). */
    int nonbg = 0;
    int first_x = -1, last_x = -1;
    float min_f[4], max_f[4];
    for (int c = 0; c < 4; c++) { min_f[c] = 1e9f; max_f[c] = -1e9f; }
    float rep_f[4] = {0};

    for (int x = 0; x < w; x++) {
        uint8_t *px = raw + x * 8;
        uint16_t r16 = (uint16_t)(px[0] | ((uint16_t)px[1] << 8));
        uint16_t g16 = (uint16_t)(px[2] | ((uint16_t)px[3] << 8));
        uint16_t b16 = (uint16_t)(px[4] | ((uint16_t)px[5] << 8));
        uint16_t a16 = (uint16_t)(px[6] | ((uint16_t)px[7] << 8));

        float rf = half_to_float(r16);
        float gf = half_to_float(g16);
        float bf = half_to_float(b16);
        float af = half_to_float(a16);

        int differs = 0;
        for (int c = 0; c < 4; c++) {
            float v = (c == 0) ? rf : (c == 1) ? gf : (c == 2) ? bf : af;
            float b = bg_f[c];
            float abs_diff = v > b ? v - b : b - v;
            float rel_thresh = (b > 0.01f) ? b * 0.05f : 0.001f;
            if (abs_diff > rel_thresh && abs_diff > 0.0005f) {
                differs = 1;
            }
            if (v < min_f[c]) min_f[c] = v;
            if (v > max_f[c]) max_f[c] = v;
        }
        if (differs) {
            nonbg++;
            if (first_x < 0) first_x = x;
            last_x = x;
            rep_f[0] = rf; rep_f[1] = gf; rep_f[2] = bf; rep_f[3] = af;
        }
    }

    VrLog("VRPROBE row-scan eye=%d ty=%d w=%d nonbg=%d/%d range=[%d..%d] "
          "bg16=(0x%04X,0x%04X,0x%04X,0x%04X)",
          eye, ty, w, nonbg, w, first_x, last_x,
          bg_rgba16[0], bg_rgba16[1], bg_rgba16[2], bg_rgba16[3]);
    if (nonbg > 0) {
        VrLog("VRPROBE row-scan eye=%d ty=%d min=(%.4f,%.4f,%.4f,%.4f) "
              "max=(%.4f,%.4f,%.4f,%.4f) rep=(%.4f,%.4f,%.4f,%.4f)",
              eye, ty,
              min_f[0], min_f[1], min_f[2], min_f[3],
              max_f[0], max_f[1], max_f[2], max_f[3],
              rep_f[0], rep_f[1], rep_f[2], rep_f[3]);
    }

    /* Remove this eye from the pending list */
    for (int i = 0; i < rb->row_pending_eye_count; i++) {
        if (rb->row_pending_eyes[i] == eye) {
            rb->row_pending_eyes[i] = rb->row_pending_eyes[--rb->row_pending_eye_count];
            break;
        }
    }
    rb->row_ready = 1;
    SDL_UnmapGPUTransferBuffer(rb->device, rb->row_buffer[eye]);
    return nonbg;
}

void VrReadback_Destroy(VrReadback *rb) {
    if (!rb) return;
    for (int e = 0; e < 2; e++) {
        if (rb->staging_buffer[e]) {
            SDL_ReleaseGPUTransferBuffer(rb->device, rb->staging_buffer[e]);
        }
        if (rb->row_buffer[e]) {
            SDL_ReleaseGPUTransferBuffer(rb->device, rb->row_buffer[e]);
        }
    }
    SDL_free(rb);
}