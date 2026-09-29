#ifndef VR_BLIT_H
#define VR_BLIT_H

#include <SDL3/SDL.h>
#include "aeron/aeron.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Copies a tonemapped present texture onto an XR swapchain image with a
 * fullscreen textured quad (same fullscreen.vert/frag pair Aeron ships).
 * No shared engine code is duplicated: the pass is recorded on the caller's
 * Aeron command buffer while no other pass is open. */
int VrBlit_Init(SDL_GPUDevice *device, SDL_GPUTextureFormat xr_format);
void VrBlit_Shutdown(SDL_GPUDevice *device);
/* Draw src (sampler-capable Aeron texture) covering the whole dst image. */
int VrBlit_Copy(AeronCommandBuffer *cmd, SDL_GPUDevice *device,
                AeronTexture *src, SDL_GPUTexture *dst, int w, int h);

#ifdef __cplusplus
}
#endif

#endif
