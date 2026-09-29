/* VR-PROBE OpenXR session management (isolated test code).
 * Adapted from the M1 SDL testgpu_spinning_cube_xr flow, but the GPU device
 * is Aeron's XR-enabled device (XWAQUEST_XR_ENABLE=1) instead of a private one.
 * Swapchain images are SDL_GPUTexture* ready for SDL GPU render passes. */
#include "vr_openxr.h"
#include "vr_log.h"

#include <string.h>

/* Bounded swapchain-image wait: XR_INFINITE_DURATION wedged the main thread
 * on 2026-09-19 (ANR kill). 100ms lets one slow frame pass while a stuck
 * swapchain surfaces as explicit TIMEOUT logs instead of a hang. */
#define VR_ACQUIRE_WAIT_TIMEOUT_NS (100000000LL)

static PFN_xrGetInstanceProcAddr s_getProc = NULL;
static PFN_xrEnumerateViewConfigurationViews s_xrEnumerateViewConfigurationViews = NULL;
static PFN_xrEnumerateSwapchainImages s_xrEnumerateSwapchainImages = NULL;
static PFN_xrCreateReferenceSpace s_xrCreateReferenceSpace = NULL;
static PFN_xrDestroySpace s_xrDestroySpace = NULL;
static PFN_xrDestroySession s_xrDestroySession = NULL;
static PFN_xrDestroySwapchain s_xrDestroySwapchain = NULL;
static PFN_xrPollEvent s_xrPollEvent = NULL;
static PFN_xrBeginSession s_xrBeginSession = NULL;
static PFN_xrEndSession s_xrEndSession = NULL;
static PFN_xrWaitFrame s_xrWaitFrame = NULL;
static PFN_xrBeginFrame s_xrBeginFrame = NULL;
static PFN_xrEndFrame s_xrEndFrame = NULL;
static PFN_xrLocateViews s_xrLocateViews = NULL;
static PFN_xrAcquireSwapchainImage s_xrAcquireSwapchainImage = NULL;
static PFN_xrWaitSwapchainImage s_xrWaitSwapchainImage = NULL;
static PFN_xrReleaseSwapchainImage s_xrReleaseSwapchainImage = NULL;

extern Uint64 Aeron_VrProbeXrInstance(void);
extern Uint64 Aeron_VrProbeXrSystemId(void);

static XrInstance s_instance = XR_NULL_HANDLE;
static XrSystemId s_system = XR_NULL_SYSTEM_ID;
static XrSession s_session = XR_NULL_HANDLE;
static XrSpace s_space = XR_NULL_HANDLE;
static int s_running = 0;
static int s_shouldQuit = 0;

static VrEyeSwapchain s_eyes[VR_EYE_COUNT];
static XrView s_views[VR_EYE_COUNT];
static Uint32 s_viewCount = 0;
static SDL_GPUTextureFormat s_format = SDL_GPU_TEXTUREFORMAT_INVALID;

#define XR_LOAD(fn) \
    if (s_getProc((XrInstance)s_instance, #fn, (PFN_xrVoidFunction*)&s_##fn) != XR_SUCCESS || !s_##fn) { \
        SDL_Log("VRPROBE XR load failed: " #fn); \
        return 0; \
    }

static int load_functions(void) {
    s_getProc = (PFN_xrGetInstanceProcAddr)SDL_OpenXR_GetXrGetInstanceProcAddr();
    if (!s_getProc) {
        SDL_Log("VRPROBE SDL_OpenXR_GetXrGetInstanceProcAddr failed: %s", SDL_GetError());
        return 0;
    }
    XR_LOAD(xrEnumerateViewConfigurationViews);
    XR_LOAD(xrEnumerateSwapchainImages);
    XR_LOAD(xrCreateReferenceSpace);
    XR_LOAD(xrDestroySpace);
    XR_LOAD(xrDestroySession);
    XR_LOAD(xrDestroySwapchain);
    XR_LOAD(xrPollEvent);
    XR_LOAD(xrBeginSession);
    XR_LOAD(xrEndSession);
    XR_LOAD(xrWaitFrame);
    XR_LOAD(xrBeginFrame);
    XR_LOAD(xrEndFrame);
    XR_LOAD(xrLocateViews);
    XR_LOAD(xrAcquireSwapchainImage);
    XR_LOAD(xrWaitSwapchainImage);
    XR_LOAD(xrReleaseSwapchainImage);
    return 1;
}

static int create_swapchains(SDL_GPUDevice *device);

int VrXr_Init(SDL_GPUDevice *device) {
    (void)device;
    s_instance = (XrInstance)Aeron_VrProbeXrInstance();
    s_system = (XrSystemId)Aeron_VrProbeXrSystemId();
    SDL_Log("VRPROBE XR handles instance=%llu system=%llu",
            (unsigned long long)s_instance, (unsigned long long)s_system);
    if (!s_instance || !s_system) {
        SDL_Log("VRPROBE XR device was not XR-enabled (env XWAQUEST_XR_ENABLE missing at backend init?)");
        return 0;
    }
    if (!load_functions()) {
        return 0;
    }
    {
        XrSessionCreateInfo info;
        memset(&info, 0, sizeof info);
        info.type = XR_TYPE_SESSION_CREATE_INFO;
        XrResult r = SDL_CreateGPUXRSession(device, &info, &s_session);
        if (r != XR_SUCCESS || !s_session) {
            SDL_Log("VRPROBE SDL_CreateGPUXRSession failed result=%d", (int)r);
            return 0;
        }
    }
    {
        XrReferenceSpaceCreateInfo space;
        memset(&space, 0, sizeof space);
        space.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
        space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        space.poseInReferenceSpace.orientation.w = 1.0f;
        XrResult r = s_xrCreateReferenceSpace(s_session, &space, &s_space);
        if (r != XR_SUCCESS) {
            SDL_Log("VRPROBE xrCreateReferenceSpace failed result=%d", (int)r);
            return 0;
        }
    }
    /* Create swapchains FIRST, then explicitly begin the session.
     * xrBeginSession must be called explicitly after swapchains are created.
     * The READY event is sent AFTER xrBeginSession succeeds. */
    if (!create_swapchains(device)) {
        return 0;
    }
    /* Poll events a few times to ensure session is fully initialized
     * and in IDLE state before calling xrBeginSession. */
    for (int i = 0; i < 5; ++i) {
        VrXr_PollEvents(device);
        SDL_Delay(50);
    }
        SDL_Log("VRPROBE XR swapchains created, starting session");
    {
        XrSessionBeginInfo begin;
        memset(&begin, 0, sizeof begin);
        begin.type = XR_TYPE_SESSION_BEGIN_INFO;
        begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        /* xrBeginSession can fail with XR_ERROR_SESSION_NOT_READY if the runtime
         * needs more time to process swapchain creation. Retry a few times. */
        XrResult r = XR_ERROR_SESSION_NOT_READY;
        for (int attempt = 0; attempt < 10; ++attempt) {
            r = s_xrBeginSession(s_session, &begin);
            SDL_Log("VRPROBE xrBeginSession returned %d (0x%X)", (int)r, (unsigned int)r);
            if (r != XR_SUCCESS) {
                SDL_Log("VRPROBE xrBeginSession failed with error %d (0x%X)", (int)r, (unsigned int)r);
                if (r == XR_ERROR_SESSION_NOT_READY) {
                    SDL_Log("VRPROBE Session not ready - retrying...");
                } else {
                    return 0;
                }
            } else {
                break;
            }
            SDL_Delay(200);
        }
        if (r != XR_SUCCESS) {
            SDL_Log("VRPROBE xrBeginSession failed after retries result=%d (0x%X)", (int)r, (unsigned int)r);
            return 0;
        }
        s_running = 1;
        SDL_Log("VRPROBE session begun");
    }
    return 1;
}

int VrXr_IsSessionRunning(void) { return s_running; }
int VrXr_ShouldQuit(void) { return s_shouldQuit; }
int VrXr_ViewCount(void) { return (int)s_viewCount; }
SDL_GPUTextureFormat VrXr_SwapchainFormat(void) { return s_format; }
VrEyeSwapchain *VrXr_Eye(int eye) {
    if (eye < 0 || eye >= VR_EYE_COUNT || s_viewCount != VR_EYE_COUNT) {
        return NULL;
    }
    return &s_eyes[eye];
}
int VrXr_EyeSize(int eye, int32_t *w, int32_t *h) {
    VrEyeSwapchain *e = VrXr_Eye(eye);
    if (!e) {
        return 0;
    }
    *w = e->width;
    *h = e->height;
    return 1;
}

void VrXr_PollEvents(SDL_GPUDevice *device) {
    XrEventDataBuffer ev;
    memset(&ev, 0, sizeof ev);
    ev.type = XR_TYPE_EVENT_DATA_BUFFER;
    while (s_xrPollEvent && s_xrPollEvent(s_instance, &ev) == XR_SUCCESS) {
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            XrEventDataSessionStateChanged *st = (XrEventDataSessionStateChanged *)&ev;
            VrLog("VRPROBE session state=%d", (int)st->state);
            if (st->state == XR_SESSION_STATE_READY) {
                VrLog("VRPROBE session READY (session already begun)");
                if (s_viewCount == 0 && !create_swapchains(device)) {
                    s_shouldQuit = 1;
                }
            } else if (st->state == XR_SESSION_STATE_STOPPING) {
                s_xrEndSession(s_session);
                s_running = 0;
                s_shouldQuit = 1;
            } else if (st->state == XR_SESSION_STATE_EXITING ||
                       st->state == XR_SESSION_STATE_LOSS_PENDING) {
                s_shouldQuit = 1;
            }
        } else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
            s_shouldQuit = 1;
        }
        memset(&ev, 0, sizeof ev);
        ev.type = XR_TYPE_EVENT_DATA_BUFFER;
    }
}

static int create_swapchains(SDL_GPUDevice *device) {
    XrResult r;
    XrViewConfigurationView configs[VR_EYE_COUNT];
    memset(configs, 0, sizeof configs);
    for (int i = 0; i < VR_EYE_COUNT; i++) {
        configs[i].type = XR_TYPE_VIEW_CONFIGURATION_VIEW;
    }
    r = s_xrEnumerateViewConfigurationViews(s_instance, s_system,
                                            XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
                                            VR_EYE_COUNT, &s_viewCount, configs);
    if (r != XR_SUCCESS || s_viewCount != VR_EYE_COUNT) {
        SDL_Log("VRPROBE view config failed result=%d count=%u", (int)r, s_viewCount);
        return 0;
    }
    int numFormats = 0;
    SDL_GPUTextureFormat *formats = SDL_GetGPUXRSwapchainFormats(device, s_session, &numFormats);
    if (!formats || numFormats == 0 || formats[0] == SDL_GPU_TEXTUREFORMAT_INVALID) {
        SDL_Log("VRPROBE no XR swapchain formats");
        if (formats) {
            SDL_free(formats);
        }
        return 0;
    }
    s_format = formats[0];
    SDL_Log("VRPROBE XR format=%d of %d", (int)s_format, numFormats);
    for (int f = 0; f < numFormats && formats[f] != SDL_GPU_TEXTUREFORMAT_INVALID; f++) {
        SDL_Log("VRPROBE XR format[%d]=%d", f, (int)formats[f]);
    }
    SDL_free(formats);

    memset(s_eyes, 0, sizeof s_eyes);
    memset(s_views, 0, sizeof s_views);
    for (int i = 0; i < VR_EYE_COUNT; i++) {
        s_views[i].type = XR_TYPE_VIEW;
        s_views[i].pose.orientation.w = 1.0f;
        SDL_Log("VRPROBE eye %d recommended %ux%u", i,
                (unsigned)configs[i].recommendedImageRectWidth,
                (unsigned)configs[i].recommendedImageRectHeight);
        XrSwapchainCreateInfo sci;
        memset(&sci, 0, sizeof sci);
        sci.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
        sci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
        sci.sampleCount = 1;
        sci.width = configs[i].recommendedImageRectWidth;
        sci.height = configs[i].recommendedImageRectHeight;
        sci.faceCount = 1;
        sci.arraySize = 1;
        sci.mipCount = 1;
        r = SDL_CreateGPUXRSwapchain(device, s_session, &sci, s_format,
                                     &s_eyes[i].swapchain, &s_eyes[i].images);
        if (r != XR_SUCCESS || !s_eyes[i].swapchain || !s_eyes[i].images) {
            SDL_Log("VRPROBE CreateGPUXRSwapchain eye %d failed result=%d", i, (int)r);
            return 0;
        }
        r = s_xrEnumerateSwapchainImages(s_eyes[i].swapchain, 0, &s_eyes[i].image_count, NULL);
        if (r != XR_SUCCESS) {
            SDL_Log("VRPROBE enumerate images eye %d failed", i);
            return 0;
        }
        s_eyes[i].width = (int32_t)sci.width;
        s_eyes[i].height = (int32_t)sci.height;
        SDL_GPUTextureCreateInfo depth;
        memset(&depth, 0, sizeof depth);
        depth.type = SDL_GPU_TEXTURETYPE_2D;
        depth.format = SDL_GPU_TEXTUREFORMAT_D32_FLOAT;
        depth.width = sci.width;
        depth.height = sci.height;
        depth.layer_count_or_depth = 1;
        depth.num_levels = 1;
        depth.usage = SDL_GPU_TEXTUREUSAGE_DEPTH_STENCIL_TARGET;
        s_eyes[i].depth_texture = SDL_CreateGPUTexture(device, &depth);
        if (!s_eyes[i].depth_texture) {
            SDL_Log("VRPROBE depth create eye %d failed: %s", i, SDL_GetError());
            return 0;
        }
        SDL_Log("VRPROBE swapchain %d: %dx%d images=%u", i,
                (int)s_eyes[i].width, (int)s_eyes[i].height, s_eyes[i].image_count);
    }
    return 1;
}



int VrXr_WaitBeginFrame(int *shouldRender, XrTime *displayTime) {
    XrFrameState state;
    XrFrameWaitInfo wait;
    XrFrameBeginInfo begin;
    XrResult r;
    Uint64 t;
    memset(&state, 0, sizeof state);
    state.type = XR_TYPE_FRAME_STATE;
    memset(&wait, 0, sizeof wait);
    wait.type = XR_TYPE_FRAME_WAIT_INFO;
    t = SDL_GetTicks();
    VrLog("VRPROBE xrWaitFrame begin");
    r = s_xrWaitFrame(s_session, &wait, &state);
    VrLog("VRPROBE xrWaitFrame end dt=%llums result=%d shouldRender=%d",
          (unsigned long long)(SDL_GetTicks() - t), (int)r,
          r == XR_SUCCESS ? (int)state.shouldRender : -1);
    if (r != XR_SUCCESS) {
        return 0;
    }
    memset(&begin, 0, sizeof begin);
    begin.type = XR_TYPE_FRAME_BEGIN_INFO;
    t = SDL_GetTicks();
    VrLog("VRPROBE xrBeginFrame begin");
    r = s_xrBeginFrame(s_session, &begin);
    VrLog("VRPROBE xrBeginFrame end dt=%llums result=%d", (unsigned long long)(SDL_GetTicks() - t),
          (int)r);
    if (r != XR_SUCCESS) {
        return 0;
    }
    *shouldRender = state.shouldRender != 0;
    *displayTime = state.predictedDisplayTime;
    return 1;
}

int VrXr_LocateViews(XrTime displayTime, XrView out_views[VR_EYE_COUNT]) {
    XrViewState vs;
    XrViewLocateInfo li;
    Uint32 count = 0;
    XrResult r;
    memset(&vs, 0, sizeof vs);
    vs.type = XR_TYPE_VIEW_STATE;
    memset(&li, 0, sizeof li);
    li.type = XR_TYPE_VIEW_LOCATE_INFO;
    li.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
    li.displayTime = displayTime;
    li.space = s_space;
    VrLog("VRPROBE xrLocateViews begin t=%lld", (long long)displayTime);
    r = s_xrLocateViews(s_session, &li, &vs, VR_EYE_COUNT, &count, out_views);
    VrLog("VRPROBE xrLocateViews end result=%d count=%u flags=0x%llx", (int)r, count,
          (unsigned long long)vs.viewStateFlags);
    if (r != XR_SUCCESS || count != VR_EYE_COUNT) {
        return 0;
    }
    if (!(vs.viewStateFlags & XR_VIEW_STATE_POSITION_VALID_BIT) ||
        !(vs.viewStateFlags & XR_VIEW_STATE_ORIENTATION_VALID_BIT)) {
        return 0;
    }
    return 1;
}

/* Returns: >=0 acquired image index; -1 hard failure; -2 wait timeout.
 * Timeout is NOT success: the caller must skip the eye and keep a valid
 * acquire/release pairing (release only what was acquired). */
int VrXr_AcquireEye(int eye, SDL_GPUTexture **out_image) {
    VrEyeSwapchain *e = VrXr_Eye(eye);
    Uint32 index = 0;
    XrSwapchainImageAcquireInfo ai;
    XrSwapchainImageWaitInfo wi;
    XrResult r;
    Uint64 t;
    if (!e) {
        VrLog("VRPROBE acquire eye%d no swapchain", eye);
        return -1;
    }
    memset(&ai, 0, sizeof ai);
    ai.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
    t = SDL_GetTicks();
    VrLog("VRPROBE acquire eye%d begin", eye);
    r = s_xrAcquireSwapchainImage(e->swapchain, &ai, &index);
    VrLog("VRPROBE acquire eye%d end dt=%llums result=%d index=%u/%u", eye,
          (unsigned long long)(SDL_GetTicks() - t), (int)r, index, e->image_count);
    if (r != XR_SUCCESS) {
        return -1;
    }
    if (index >= e->image_count) {
        VrLog("VRPROBE acquire eye%d BAD index, releasing", eye);
        VrXr_ReleaseEye(eye);
        return -1;
    }
    memset(&wi, 0, sizeof wi);
    wi.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
    wi.timeout = VR_ACQUIRE_WAIT_TIMEOUT_NS;
    t = SDL_GetTicks();
    VrLog("VRPROBE waitImage eye%d begin timeout=100ms", eye);
    r = s_xrWaitSwapchainImage(e->swapchain, &wi);
    VrLog("VRPROBE waitImage eye%d end dt=%llums result=%d", eye,
          (unsigned long long)(SDL_GetTicks() - t), (int)r);
    if (r == XR_TIMEOUT_EXPIRED) {
        VrLog("VRPROBE waitImage eye%d TIMEOUT, releasing image %u", eye, index);
        VrXr_ReleaseEye(eye);
        return -2;
    }
    if (r != XR_SUCCESS) {
        VrXr_ReleaseEye(eye);
        return -1;
    }
    *out_image = e->images[index];
    return (int)index;
}

void VrXr_ReleaseEye(int eye) {
    VrEyeSwapchain *e = VrXr_Eye(eye);
    XrSwapchainImageReleaseInfo ri;
    XrResult r;
    if (!e) {
        return;
    }
    memset(&ri, 0, sizeof ri);
    ri.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
    r = s_xrReleaseSwapchainImage(e->swapchain, &ri);
    VrLog("VRPROBE release eye%d result=%d", eye, (int)r);
}

int VrXr_EndFrame(XrTime displayTime, const XrPosef poses[VR_EYE_COUNT],
                  const XrFovf fovs[VR_EYE_COUNT], int rendered) {
    XrCompositionLayerProjectionView views[VR_EYE_COUNT];
    XrCompositionLayerProjection layer;
    const XrCompositionLayerBaseHeader *layers[1];
    XrFrameEndInfo end;
    memset(views, 0, sizeof views);
    memset(&layer, 0, sizeof layer);
    if (rendered) {
        for (int i = 0; i < VR_EYE_COUNT; i++) {
            views[i].type = XR_TYPE_COMPOSITION_LAYER_PROJECTION_VIEW;
            views[i].pose = poses[i];
            views[i].fov = fovs[i];
            views[i].subImage.swapchain = s_eyes[i].swapchain;
            views[i].subImage.imageRect.offset.x = 0;
            views[i].subImage.imageRect.offset.y = 0;
            views[i].subImage.imageRect.extent.width = s_eyes[i].width;
            views[i].subImage.imageRect.extent.height = s_eyes[i].height;
            views[i].subImage.imageArrayIndex = 0;
        }
        layer.type = XR_TYPE_COMPOSITION_LAYER_PROJECTION;
        layer.space = s_space;
        layer.viewCount = VR_EYE_COUNT;
        layer.views = views;
        layers[0] = (const XrCompositionLayerBaseHeader *)&layer;
    }
    memset(&end, 0, sizeof end);
    end.type = XR_TYPE_FRAME_END_INFO;
    end.displayTime = displayTime;
    end.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    end.layerCount = rendered ? 1u : 0u;
    end.layers = rendered ? layers : NULL;
    {
        Uint64 t = SDL_GetTicks();
        XrResult r;
        VrLog("VRPROBE xrEndFrame begin layers=%d", rendered ? 1 : 0);
        r = s_xrEndFrame(s_session, &end);
        VrLog("VRPROBE xrEndFrame end dt=%llums result=%d", (unsigned long long)(SDL_GetTicks() - t),
              (int)r);
        return r == XR_SUCCESS;
    }
}

void VrXr_Shutdown(SDL_GPUDevice *device) {
    if (s_space && s_xrDestroySpace) {
        s_xrDestroySpace(s_space);
        s_space = XR_NULL_HANDLE;
    }
    for (int i = 0; i < VR_EYE_COUNT; i++) {
        if (s_eyes[i].depth_texture) {
            SDL_ReleaseGPUTexture(device, s_eyes[i].depth_texture);
            s_eyes[i].depth_texture = NULL;
        }
        if (s_eyes[i].swapchain) {
            SDL_DestroyGPUXRSwapchain(device, s_eyes[i].swapchain, s_eyes[i].images);
            s_eyes[i].swapchain = XR_NULL_HANDLE;
            s_eyes[i].images = NULL;
        }
    }
    if (s_session && s_xrDestroySession) {
        s_xrDestroySession(s_session);
        s_session = XR_NULL_HANDLE;
    }
    s_running = 0;
    s_viewCount = 0;
}
