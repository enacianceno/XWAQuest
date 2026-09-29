#ifndef VR_READBACK_H
#define VR_READBACK_H

#include "aeron/aeron.h"
#include <SDL3/SDL.h>
#include <SDL3/SDL_gpu.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VR_READBACK_MAX_SAMPLES 16

/* Async GPU readback for diagnostic sampling.
 * Uses a small staging buffer pool to read back selected pixels from a
 * render target without blocking the GPU pipeline. */
typedef struct VrReadbackSample {
    float ndc_x, ndc_y;
    uint8_t rgba[4];      /* high bytes — legacy, kept for row-scan compat */
    float rgba_f[4];      /* v9.7: full float values via half_to_float */
    int ready;
} VrReadbackSample;

typedef struct VrReadback {
    SDL_GPUDevice *device;
    int num_samples;
    int num_eyes;

    // Per-eye staging (point samples)
    SDL_GPUTransferBuffer *staging_buffer[2];
    uint8_t *staging_mapped[2];

    // Per-eye per-sample tracking
    VrReadbackSample samples[2][VR_READBACK_MAX_SAMPLES];
    int pending_count[2];
    int completed_count[2];

    // v9.5: Row-scan staging (one row at a time, per-eye)
    SDL_GPUTransferBuffer *row_buffer[2];
    int row_pending_eye_count;   // how many eyes have pending row downloads (0/1/2)
    int row_pending_eyes[2];     // which eyes have pending downloads
    int row_tex_w, row_tex_h;   // texture dimensions at schedule time
    int row_y;                   // texture row index of the pending download
    int row_ready;               // 1 = row data available for analysis
} VrReadback;

/* Async GPU readback for diagnostic sampling.
 * Uses a small staging buffer pool to read back selected pixels from a
 * render target without blocking the GPU pipeline. */
VrReadback *VrReadback_Create(int num_samples, int num_eyes);
VrReadback *VrReadback_Init(SDL_GPUDevice *device, int num_samples, int num_eyes);
void VrReadback_Destroy(VrReadback *rb);

/* Schedule a readback of `num_samples` points from `src_texture`.
 * `ndc_points` is an array of (x,y) in NDC [-1,1].
 * The readback is asynchronous; results are available on the next call
 * to VrReadback_Collect. */
int VrReadback_Schedule(VrReadback *rb, int eye, AeronTexture *src_texture,
                        const float ndc_points[][2], int num_samples,
                        AeronCommandBuffer *cmd);

/* Collect completed readbacks. Fills `out_rgba` with 4 uint8 per sample.
 * Returns number of samples collected (0 if none ready). */
int VrReadback_Collect(VrReadback *rb, int eye, uint8_t *out_rgba);

/* v9.5: Schedule a full-row download from src_texture.
 * row_y is the texture-space row index (0 = top of texture).
 * The download is recorded into `cmd`; results are available after
 * SDL_WaitForGPUIdle + VrReadback_CollectRow. */
int VrReadback_ScheduleRow(VrReadback *rb, int eye,
                           AeronTexture *src_texture, int row_y,
                           AeronCommandBuffer *cmd);

/* v9.5: Collect and analyse a previously-scheduled row download.
 * Analyses every pixel in the row against `bg_rgba16` (the known
 * background / clear colour in 16-bit-per-channel LE format).
 * Logs per-row statistics and returns the number of non-background pixels. */
int VrReadback_CollectRow(VrReadback *rb, int eye,
                          const uint16_t bg_rgba16[4]);

/* v9.7: Convert a 16-bit IEEE 754 half-float to float. */
float half_to_float(uint16_t h);

#ifdef __cplusplus
}
#endif

#endif