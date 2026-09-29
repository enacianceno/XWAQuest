#ifndef VR_OPENXR_H
#define VR_OPENXR_H

#include <SDL3/SDL.h>

#ifdef HAVE_OPENXR_H
#include <openxr/openxr.h>
#else
#include "../src/video/khronos/openxr/openxr.h"
#endif
#include <SDL3/SDL_openxr.h>

#ifdef __cplusplus
extern "C" {
#endif

#define VR_EYE_COUNT 2

typedef struct VrEyeSwapchain {
    XrSwapchain swapchain;
    SDL_GPUTexture **images;
    SDL_GPUTexture *depth_texture;
    Uint32 image_count;
    int32_t width;
    int32_t height;
} VrEyeSwapchain;

/* Lifecycle. device is Aeron's GPU device (g_aeron.gpu_device). */
int VrXr_Init(SDL_GPUDevice *device);
int VrXr_IsSessionRunning(void);
int VrXr_ShouldQuit(void);
int VrXr_ViewCount(void);
SDL_GPUTextureFormat VrXr_SwapchainFormat(void);
int VrXr_EyeSize(int eye, int32_t *w, int32_t *h);
VrEyeSwapchain *VrXr_Eye(int eye);
/* Pump OpenXR events; begins session and creates swapchains on READY. */
void VrXr_PollEvents(SDL_GPUDevice *device);
/* xrWaitFrame + xrBeginFrame. Returns 1 with *shouldRender set. */
int VrXr_WaitBeginFrame(int *shouldRender, XrTime *displayTime);
/* Locate views at displayTime into out_views (count 2). Returns 1 when both valid. */
int VrXr_LocateViews(XrTime displayTime, XrView out_views[VR_EYE_COUNT]);
/* Acquire+wait swapchain image for eye. Returns image index or -1. */
int VrXr_AcquireEye(int eye, SDL_GPUTexture **out_image);
/* Release swapchain image for eye. */
void VrXr_ReleaseEye(int eye);
/* Submit projection layers built from poses/fovs and end frame. */
int VrXr_EndFrame(XrTime displayTime, const XrPosef poses[VR_EYE_COUNT],
                  const XrFovf fovs[VR_EYE_COUNT], int rendered);
void VrXr_Shutdown(SDL_GPUDevice *device);

#ifdef __cplusplus
}
#endif

#endif
