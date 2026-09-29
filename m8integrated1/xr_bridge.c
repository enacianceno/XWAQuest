/* Reuse the validated session/swapchain implementation verbatim. The local
 * session hook attaches actions before xrBeginSession; no original is edited. */
#include "frame_bridge.h"
#include "input_xr.h"
#include "diagnostics.h"
extern Uint64 Aeron_VrProbeXrInstance(void);
static XrResult M8_CreateSession(SDL_GPUDevice *device,
        const XrSessionCreateInfo *info, XrSession *session) {
    XrResult r = SDL_CreateGPUXRSession(device, info, session);
    if (r == XR_SUCCESS && !M8_InputInit((XrInstance)Aeron_VrProbeXrInstance(), *session))
        return XR_ERROR_INITIALIZATION_FAILED;
    return r;
}
#define SDL_CreateGPUXRSession M8_CreateSession
#include "../vr-probe/vr_openxr.c"
#undef SDL_CreateGPUXRSession

XrSpace M8_XrLocalSpace(void) { return s_space; }

/* Require both components of physical LOCAL eye poses for CS1; preserve the
 * baseline implementation unchanged in vr-probe. No tracking spaces created. */
int M8_XrLocateViews(XrTime time, XrView views[VR_EYE_COUNT]) {
    XrViewState state = { .type = XR_TYPE_VIEW_STATE };
    XrViewLocateInfo info = { .type = XR_TYPE_VIEW_LOCATE_INFO,
        .viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO,
        .displayTime = time, .space = s_space };
    uint32_t count = 0;
    XrResult r = s_xrLocateViews(s_session, &info, &state, VR_EYE_COUNT, &count, views);
    XrViewStateFlags required = XR_VIEW_STATE_ORIENTATION_VALID_BIT | XR_VIEW_STATE_POSITION_VALID_BIT;
    int valid=XR_SUCCEEDED(r) && count == VR_EYE_COUNT && (state.viewStateFlags & required) == required;
    static int previous=-1;
    static XrResult previous_result=XR_SUCCESS;
    if(valid!=previous || r!=previous_result) {
        M8_Diag("M8_UI_LOCATE_VIEWS result=%d count=%u flags=%llu valid=%d",(int)r,count,(unsigned long long)state.viewStateFlags,valid);
        previous=valid; previous_result=r;
    }
    return valid;
}

/* Unlike the diagnostic helper, never release an image whose wait timed out.
 * The application fails closed on acquire/wait failure and destroys its session.
 * Waited images are always released, including failed GPU recording/submission. */
static int waited[VR_EYE_COUNT];
int M8_XrAcquire(int eye, SDL_GPUTexture **texture) {
    VrEyeSwapchain *e = VrXr_Eye(eye);
    uint32_t index = 0;
    XrSwapchainImageAcquireInfo ai = { .type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO };
    XrSwapchainImageWaitInfo wi = { .type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO,
                                  .timeout = VR_ACQUIRE_WAIT_TIMEOUT_NS };
    if (!e) return 0;
    XrResult r = s_xrAcquireSwapchainImage(e->swapchain, &ai, &index);
    if (r == XR_SUCCESS) r = s_xrWaitSwapchainImage(e->swapchain, &wi);
    if (r != XR_SUCCESS) {
        SDL_Log("M8_XR_ACQUIRE_FAILED eye=%d result=%d", eye, (int)r);
        return 0;
    }
    waited[eye] = 1;
    if (index >= e->image_count) {
        M8_XrRelease(eye);
        return 0;
    }
    *texture = e->images[index];
    return *texture != NULL;
}
int M8_XrRelease(int eye) {
    if (!waited[eye]) return 1;
    XrSwapchainImageReleaseInfo ri = { .type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO };
    XrResult r = s_xrReleaseSwapchainImage(s_eyes[eye].swapchain, &ri);
    waited[eye] = 0;
    if (r != XR_SUCCESS) SDL_Log("M8_XR_RELEASE_FAILED eye=%d result=%d", eye, (int)r);
    return r == XR_SUCCESS;
}

/* ---- M8integrated1 finite frontend screen: UI swapchain + quad layer ----
 * Same translation unit as vr_openxr.c: s_session/s_space/s_format and the
 * loaded xr* entry points are visible here. vr-probe itself is untouched. */
static XrSwapchain s_uiSwapchain = XR_NULL_HANDLE;
static SDL_GPUTexture **s_uiImages = NULL;
static Uint32 s_uiImageCount = 0;
static int s_uiW = 0, s_uiH = 0;
static int s_uiWaited = 0;
static unsigned s_uiLastIndex = 0;

int VrUi_Width(void) { return s_uiW; }
int VrUi_Height(void) { return s_uiH; }
unsigned VrUi_LastIndex(void) { return s_uiLastIndex; }

void VrUi_Destroy(SDL_GPUDevice *device) {
    if (s_uiSwapchain) {
        SDL_DestroyGPUXRSwapchain(device, s_uiSwapchain, s_uiImages);
        s_uiSwapchain = XR_NULL_HANDLE;
    }
    s_uiImages = NULL; s_uiImageCount = 0; s_uiW = s_uiH = 0;
    s_uiWaited = 0; s_uiLastIndex = 0;
}

int VrUi_Ensure(SDL_GPUDevice *device, int w, int h) {
    XrSwapchainCreateInfo sci;
    XrResult r;
    if (w <= 0 || h <= 0 || !device) return 0;
    if (s_uiSwapchain && s_uiW == w && s_uiH == h) return 1;
    VrUi_Destroy(device);
    memset(&sci, 0, sizeof sci);
    sci.type = XR_TYPE_SWAPCHAIN_CREATE_INFO;
    sci.usageFlags = XR_SWAPCHAIN_USAGE_COLOR_ATTACHMENT_BIT | XR_SWAPCHAIN_USAGE_SAMPLED_BIT;
    sci.sampleCount = 1;
    sci.width = (uint32_t)w; sci.height = (uint32_t)h;
    sci.faceCount = 1; sci.arraySize = 1; sci.mipCount = 1;
    r = SDL_CreateGPUXRSwapchain(device, s_session, &sci, s_format,
                                 &s_uiSwapchain, &s_uiImages);
    if (r != XR_SUCCESS || !s_uiSwapchain || !s_uiImages) {
        SDL_Log("M8I_UI_SWAPCHAIN_FAILED result=%d size=%dx%d", (int)r, w, h);
        s_uiSwapchain = XR_NULL_HANDLE; s_uiImages = NULL;
        return 0;
    }
    r = s_xrEnumerateSwapchainImages(s_uiSwapchain, 0, &s_uiImageCount, NULL);
    if (r != XR_SUCCESS || !s_uiImageCount) {
        SDL_Log("M8I_UI_SWAPCHAIN_FAILED enumerate=%d", (int)r);
        VrUi_Destroy(device);
        return 0;
    }
    s_uiW = w; s_uiH = h;
    M8_Diag("M8I_UI_SWAPCHAIN created=%dx%d images=%u format=%d",
            w, h, s_uiImageCount, (int)s_format);
    return 1;
}

int VrUi_Release(void);
int VrUi_Acquire(SDL_GPUTexture **texture) {
    XrSwapchainImageAcquireInfo ai;
    XrSwapchainImageWaitInfo wi;
    XrResult r;
    uint32_t index = 0;
    if (!s_uiSwapchain || !texture) return 0;
    memset(&ai, 0, sizeof ai);
    ai.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
    memset(&wi, 0, sizeof wi);
    wi.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
    wi.timeout = VR_ACQUIRE_WAIT_TIMEOUT_NS;
    r = s_xrAcquireSwapchainImage(s_uiSwapchain, &ai, &index);
    if (r == XR_SUCCESS) r = s_xrWaitSwapchainImage(s_uiSwapchain, &wi);
    if (r != XR_SUCCESS) {
        SDL_Log("M8I_UI_ACQUIRE_FAILED result=%d", (int)r);
        return 0;
    }
    s_uiWaited = 1;
    if (index >= s_uiImageCount) { VrUi_Release(); return 0; }
    s_uiLastIndex = index;
    *texture = s_uiImages[index];
    return *texture != NULL;
}

int VrUi_Release(void) {
    XrSwapchainImageReleaseInfo ri;
    XrResult r;
    if (!s_uiWaited) return 1;
    memset(&ri, 0, sizeof ri);
    ri.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
    r = s_xrReleaseSwapchainImage(s_uiSwapchain, &ri);
    s_uiWaited = 0;
    if (r != XR_SUCCESS) SDL_Log("M8I_UI_RELEASE_FAILED result=%d", (int)r);
    return r == XR_SUCCESS;
}

int M8_XrEndFrameUI(XrTime display_time, const XrPosef *pose, float w, float h) {
    XrCompositionLayerQuad quad;
    const XrCompositionLayerBaseHeader *layers[1];
    XrFrameEndInfo end;
    XrResult r;
    if (!pose || !(w > 0.f) || !(h > 0.f) || !s_uiSwapchain) return 0;
    memset(&quad, 0, sizeof quad);
    quad.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
    quad.space = s_space;
    quad.eyeVisibility = XR_EYE_VISIBILITY_BOTH;
    quad.subImage.swapchain = s_uiSwapchain;
    quad.subImage.imageRect.offset.x = 0;
    quad.subImage.imageRect.offset.y = 0;
    quad.subImage.imageRect.extent.width = s_uiW;
    quad.subImage.imageRect.extent.height = s_uiH;
    quad.subImage.imageArrayIndex = 0;
    quad.pose = *pose;
    quad.size.width = w;
    quad.size.height = h;
    layers[0] = (const XrCompositionLayerBaseHeader *)&quad;
    memset(&end, 0, sizeof end);
    end.type = XR_TYPE_FRAME_END_INFO;
    end.displayTime = display_time;
    end.environmentBlendMode = XR_ENVIRONMENT_BLEND_MODE_OPAQUE;
    end.layerCount = 1u;
    end.layers = layers;
    r = s_xrEndFrame(s_session, &end);
    if (r != XR_SUCCESS) SDL_Log("M8I_UI_END_FAILED result=%d", (int)r);
    return r == XR_SUCCESS;
}
