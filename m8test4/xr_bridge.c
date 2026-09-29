/* Reuse the validated session/swapchain implementation verbatim. The local
 * session hook attaches actions before xrBeginSession; no original is edited. */
#include "frame_bridge.h"
#include "input_xr.h"
#include "diagnostics.h"
#include "t4_timing.h"
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

/* Install after VrXr_Init has resolved pointers; no change to call order/results. */
#define T4_XR_HOOK(name,type,args,call,stage) \
static PFN_##name t4_real_##name; \
static XrResult XRAPI_PTR t4_##name args { Uint64 t=SDL_GetTicksNS(); XrResult r=t4_real_##name call; T4_Add(stage,t); return r; }
T4_XR_HOOK(xrWaitFrame,unused,(XrSession a,const XrFrameWaitInfo*b,XrFrameState*c),(a,b,c),T4_XR_WAIT)
T4_XR_HOOK(xrBeginFrame,unused,(XrSession a,const XrFrameBeginInfo*b),(a,b),T4_XR_BEGIN)
T4_XR_HOOK(xrEndFrame,unused,(XrSession a,const XrFrameEndInfo*b),(a,b),T4_XR_END)
T4_XR_HOOK(xrLocateViews,unused,(XrSession a,const XrViewLocateInfo*b,XrViewState*c,uint32_t d,uint32_t*e,XrView*f),(a,b,c,d,e,f),T4_LOCATE)
T4_XR_HOOK(xrAcquireSwapchainImage,unused,(XrSwapchain a,const XrSwapchainImageAcquireInfo*b,uint32_t*c),(a,b,c),T4_ACQUIRE)
T4_XR_HOOK(xrWaitSwapchainImage,unused,(XrSwapchain a,const XrSwapchainImageWaitInfo*b),(a,b),T4_WAIT_IMAGE)
T4_XR_HOOK(xrReleaseSwapchainImage,unused,(XrSwapchain a,const XrSwapchainImageReleaseInfo*b),(a,b),T4_RELEASE)
#undef T4_XR_HOOK
void T4_InstallXrTiming(void) {
#define HOOK(name) do { t4_real_##name=s_##name; s_##name=t4_##name; } while(0)
HOOK(xrWaitFrame);HOOK(xrBeginFrame);HOOK(xrEndFrame);HOOK(xrLocateViews);
HOOK(xrAcquireSwapchainImage);HOOK(xrWaitSwapchainImage);HOOK(xrReleaseSwapchainImage);
#undef HOOK
}