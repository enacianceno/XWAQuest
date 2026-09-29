/* M7C XR Session Management — Etapa 0: Floating Panel via OpenXR
 *
 * Creates an OpenXR session using the GPU device already initialized by Aeron
 * (via XWAQUEST_XR_ENABLE env var). Presents the game content as a floating
 * XrCompositionLayerQuad panel in 3D space.
 *
 * Architecture:
 *   - xrInit() called after Aeron_RenderBackendInit (GPU device exists)
 *   - xrBeginFrame/xrEndFrame wrap the existing Aeron_Present
 *   - The game continues to render to its normal swapchain
 *   - A second pass copies the game frame to the XR swapchain as a QUAD layer
 *   - Head tracking reads XrSpaceLocation for head pose (future Etapa 1)
 *
 * Separation principle:
 *   - Game engine (flight, input, audio) is untouched
 *   - Camera (FlightView_UpdatePlayerCamera) is untouched
 *   - Rendering (Aeron_Present) is untouched — we read its output
 *   - Only presentation is redirected through OpenXR
 */
#include "internal.h"
#include <SDL3/SDL.h>
#include <openxr/openxr.h>
#include <SDL3/SDL_openxr.h>
#include <math.h>
#include <string.h>

/* Aeron XR probe (from render_backend.c) — returns XrInstance/XrSystemId as Uint64 */
extern Uint64 Aeron_VrProbeXrInstance(void);
extern Uint64 Aeron_VrProbeXrSystemId(void);

/* ========================================================================
 * OpenXR function pointers (loaded dynamically from SDL's XR instance)
 * ======================================================================== */
static PFN_xrGetInstanceProcAddr s_getProc = NULL;

/* Session functions */
static PFN_xrCreateSession         s_xrCreateSession        = NULL;
static PFN_xrDestroySession        s_xrDestroySession       = NULL;
static PFN_xrBeginSession          s_xrBeginSession         = NULL;
static PFN_xrEndSession            s_xrEndSession           = NULL;
static PFN_xrRequestExitSession     s_xrRequestExitSession   = NULL;

/* Space functions */
static PFN_xrCreateReferenceSpace  s_xrCreateReferenceSpace = NULL;
static PFN_xrDestroySpace          s_xrDestroySpace         = NULL;
static PFN_xrLocateSpace           s_xrLocateSpace          = NULL;

/* Swapchain functions */
static PFN_xrCreateSwapchain        s_xrCreateSwapchain       = NULL;
static PFN_xrDestroySwapchain       s_xrDestroySwapchain      = NULL;
static PFN_xrEnumerateSwapchainFormats s_xrEnumerateSwapchainFormats = NULL;
static PFN_xrAcquireSwapchainImage  s_xrAcquireSwapchainImage = NULL;
static PFN_xrWaitSwapchainImage     s_xrWaitSwapchainImage    = NULL;
static PFN_xrReleaseSwapchainImage  s_xrReleaseSwapchainImage = NULL;

/* Frame functions */
static PFN_xrWaitFrame             s_xrWaitFrame            = NULL;
static PFN_xrBeginFrame            s_xrBeginFrame           = NULL;
static PFN_xrEndFrame              s_xrEndFrame             = NULL;
static PFN_xrLocateViews           s_xrLocateViews          = NULL;

/* Event functions */
static PFN_xrPollEvent             s_xrPollEvent            = NULL;

/* ========================================================================
 * XR state
 * ======================================================================== */
static XrInstance    s_instance  = XR_NULL_HANDLE;
static XrSystemId    s_system    = XR_NULL_SYSTEM_ID;
static XrSession     s_session   = XR_NULL_HANDLE;
static XrSpace       s_localSpace = XR_NULL_HANDLE;
static int           s_sessionReady = 0;
static int           s_sessionRunning = 0;
static int           s_shouldQuit = 0;

/* QUAD layer swapchain */
static XrSwapchain   s_quadSwapchain = XR_NULL_HANDLE;
static int32_t       s_quadWidth = 0;
static int32_t       s_quadHeight = 0;
static SDL_GPUTexture* s_quadTexture = NULL; /* SDL GPU texture for blit target */
static int           s_quadImageCount = 0;

/* Head pose (for future Etapa 1) */
static XrPosef       s_headPose = { .orientation = { .w = 1.0f } };

/* Frame timing */
static XrTime        s_displayTime = 0;

/* ========================================================================
 * Internal helpers
 * ======================================================================== */
#define XR_LOAD(fn) do { \
    if (s_getProc(s_instance, #fn, (PFN_xrVoidFunction*)&s_##fn) != XR_SUCCESS || !s_##fn) { \
        SDL_Log("M7C XR load failed: " #fn); \
        return 0; \
    } \
} while(0)

static int load_xr_functions(void) {
    s_getProc = (PFN_xrGetInstanceProcAddr)SDL_OpenXR_GetXrGetInstanceProcAddr();
    if (!s_getProc) {
        SDL_Log("M7C SDL_OpenXR_GetXrGetInstanceProcAddr failed: %s", SDL_GetError());
        return 0;
    }
    /* Session */
    XR_LOAD(xrCreateSession);
    XR_LOAD(xrDestroySession);
    XR_LOAD(xrBeginSession);
    XR_LOAD(xrEndSession);
    XR_LOAD(xrRequestExitSession);
    /* Space */
    XR_LOAD(xrCreateReferenceSpace);
    XR_LOAD(xrDestroySpace);
    XR_LOAD(xrLocateSpace);
    /* Swapchain */
    XR_LOAD(xrCreateSwapchain);
    XR_LOAD(xrDestroySwapchain);
    XR_LOAD(xrEnumerateSwapchainFormats);
    XR_LOAD(xrAcquireSwapchainImage);
    XR_LOAD(xrWaitSwapchainImage);
    XR_LOAD(xrReleaseSwapchainImage);
    /* Frame */
    XR_LOAD(xrWaitFrame);
    XR_LOAD(xrBeginFrame);
    XR_LOAD(xrEndFrame);
    XR_LOAD(xrLocateViews);
    /* Event */
    XR_LOAD(xrPollEvent);
    return 1;
}

/* ========================================================================
 * Public API
 * ======================================================================== */

int M7C_XrInit(SDL_GPUDevice *device) {
    SDL_Log("M7C XR INIT begin");

    /* Get XR instance and system from Aeron's probe */
    s_instance = (XrInstance)Aeron_VrProbeXrInstance();
    s_system   = (XrSystemId)Aeron_VrProbeXrSystemId();
    SDL_Log("M7C XR instance=%llu system=%llu",
            (unsigned long long)s_instance, (unsigned long long)s_system);

    if (!s_instance || !s_system) {
        SDL_Log("M7C XR: XR not available (XWAQUEST_XR_ENABLE missing at backend init?)");
        return 0;
    }

    if (!load_xr_functions()) {
        return 0;
    }

    /* Create XR session using SDL's GPU device (shares Vulkan instance/device) */
    {
        XrSessionCreateInfo info;
        memset(&info, 0, sizeof info);
        info.type = XR_TYPE_SESSION_CREATE_INFO;
        XrResult r = SDL_CreateGPUXRSession(device, &info, &s_session);
        if (r != XR_SUCCESS || !s_session) {
            SDL_Log("M7C XR CreateGPUXRSession failed result=%d: %s", (int)r, SDL_GetError());
            return 0;
        }
        SDL_Log("M7C XR session created");
    }

    /* Create LOCAL reference space (fixed in world, head moves relative to it) */
    {
        XrReferenceSpaceCreateInfo space;
        memset(&space, 0, sizeof space);
        space.type = XR_TYPE_REFERENCE_SPACE_CREATE_INFO;
        space.referenceSpaceType = XR_REFERENCE_SPACE_TYPE_LOCAL;
        space.poseInReferenceSpace.orientation.w = 1.0f;
        XrResult r = s_xrCreateReferenceSpace(s_session, &space, &s_localSpace);
        if (r != XR_SUCCESS) {
            SDL_Log("M7C XR xrCreateReferenceSpace failed result=%d", (int)r);
            return 0;
        }
        SDL_Log("M7C XR LOCAL space created");
    }

    SDL_Log("M7C XR INIT complete — waiting for SESSION_STATE_READY");
    return 1;
}

void M7C_XrPollEvents(void) {
    XrEventDataBuffer ev;
    memset(&ev, 0, sizeof ev);
    ev.type = XR_TYPE_EVENT_DATA_BUFFER;

    while (s_xrPollEvent && s_xrPollEvent(s_instance, &ev) == XR_SUCCESS) {
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            XrEventDataSessionStateChanged *st = (XrEventDataSessionStateChanged *)&ev;
            SDL_Log("M7C XR session state=%d", (int)st->state);

            if (st->state == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo begin;
                memset(&begin, 0, sizeof begin);
                begin.type = XR_TYPE_SESSION_BEGIN_INFO;
                /* Etapa 0: MONO panel (not stereo). We use PRIMARY_STEREO
                 * for the view config but render a single QUAD layer. */
                begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                if (s_xrBeginSession(s_session, &begin) == XR_SUCCESS) {
                    s_sessionReady = 1;
                    s_sessionRunning = 1;
                    SDL_Log("M7C XR session begun");
                } else {
                    SDL_Log("M7C XR xrBeginSession failed");
                }
            } else if (st->state == XR_SESSION_STATE_STOPPING) {
                s_sessionRunning = 0;
                SDL_Log("M7C XR session stopping");
            } else if (st->state == XR_SESSION_STATE_EXITING ||
                       st->state == XR_SESSION_STATE_LOSS_PENDING) {
                s_shouldQuit = 1;
                SDL_Log("M7C XR session exiting/loss");
            }
        } else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
            s_shouldQuit = 1;
            SDL_Log("M7C XR instance loss pending");
        }
        memset(&ev, 0, sizeof ev);
        ev.type = XR_TYPE_EVENT_DATA_BUFFER;
    }
}

int M7C_XrWaitFrame(void) {
    if (!s_sessionRunning) return 0;

    XrFrameState state;
    XrFrameWaitInfo wait;
    memset(&state, 0, sizeof state);
    state.type = XR_TYPE_FRAME_STATE;
    memset(&wait, 0, sizeof wait);
    wait.type = XR_TYPE_FRAME_WAIT_INFO;

    XrResult r = s_xrWaitFrame(s_session, &wait, &state);
    if (r != XR_SUCCESS) {
        SDL_Log("M7C XR xrWaitFrame failed result=%d", (int)r);
        return 0;
    }

    XrFrameBeginInfo begin;
    memset(&begin, 0, sizeof begin);
    begin.type = XR_TYPE_FRAME_BEGIN_INFO;
    r = s_xrBeginFrame(s_session, &begin);
    if (r != XR_SUCCESS) {
        SDL_Log("M7C XR xrBeginFrame failed result=%d", (int)r);
        return 0;
    }

    s_displayTime = state.predictedDisplayTime;
    return 1;
}

int M7C_XrEndFrame(int rendered) {
    if (!s_sessionRunning) return 0;

    /* Locate head pose for future use (Etapa 1 head tracking) */
    {
        XrViewState viewState;
        XrViewLocateInfo locateInfo;
        XrView views[2];
        uint32_t viewCount = 0;

        memset(&viewState, 0, sizeof viewState);
        viewState.type = XR_TYPE_VIEW_STATE;
        memset(&locateInfo, 0, sizeof locateInfo);
        locateInfo.type = XR_TYPE_VIEW_LOCATE_INFO;
        locateInfo.viewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        locateInfo.displayTime = s_displayTime;
        locateInfo.space = s_localSpace;
        views[0].type = XR_TYPE_VIEW;
        views[0].pose.orientation.w = 1.0f;
        views[1].type = XR_TYPE_VIEW;
        views[1].pose.orientation.w = 1.0f;

        s_xrLocateViews(s_session, &locateInfo, &viewState, 2, &viewCount, views);
        if (viewCount > 0) {
            s_headPose = views[0].pose;
        }
    }

    /* Etapa 0: submit a QUAD layer with the game content.
     * For now, if we haven't created a swapchain yet, just submit empty.
     * The game renders to its own window; we'll add the blit in a future step. */
    if (rendered && s_quadSwapchain != XR_NULL_HANDLE) {
        /* Acquire, blit, release — placeholder for Etapa 0 test */
        uint32_t imageIndex = 0;
        XrSwapchainImageAcquireInfo ai;
        memset(&ai, 0, sizeof ai);
        ai.type = XR_TYPE_SWAPCHAIN_IMAGE_ACQUIRE_INFO;
        XrResult r = s_xrAcquireSwapchainImage(s_quadSwapchain, &ai, &imageIndex);
        if (r == XR_SUCCESS) {
            XrSwapchainImageWaitInfo wi;
            memset(&wi, 0, sizeof wi);
            wi.type = XR_TYPE_SWAPCHAIN_IMAGE_WAIT_INFO;
            wi.timeout = 100000000LL; /* 100ms */
            s_xrWaitSwapchainImage(s_quadSwapchain, &wi);

            /* TODO: blit game frame to XR swapchain texture here */

            XrSwapchainImageReleaseInfo ri;
            memset(&ri, 0, sizeof ri);
            ri.type = XR_TYPE_SWAPCHAIN_IMAGE_RELEASE_INFO;
            s_xrReleaseSwapchainImage(s_quadSwapchain, &ri);
        }
    }

    /* Submit frame with a QUAD layer */
    XrCompositionLayerQuad quadLayer;
    memset(&quadLayer, 0, sizeof quadLayer);
    quadLayer.type = XR_TYPE_COMPOSITION_LAYER_QUAD;
    quadLayer.space = s_localSpace;
    quadLayer.pose.orientation.w = 1.0f;
    quadLayer.pose.position.x = 0.0f;
    quadLayer.pose.position.y = 1.5f;   /* Eye height */
    quadLayer.pose.position.z = -2.0f;  /* 2m in front */
    quadLayer.size.width = 2.0f;        /* 2m wide panel */
    quadLayer.size.height = 1.2f;       /* 1.2m tall (16:10) */
    if (s_quadSwapchain != XR_NULL_HANDLE) {
        quadLayer.subImage.swapchain = s_quadSwapchain;
        quadLayer.subImage.imageRect.offset.x = 0;
        quadLayer.subImage.imageRect.offset.y = 0;
        quadLayer.subImage.imageRect.extent.width = s_quadWidth;
        quadLayer.subImage.imageRect.extent.height = s_quadHeight;
    }

    const XrCompositionLayerBaseHeader *layers[1];
    layers[0] = (const XrCompositionLayerBaseHeader*)&quadLayer;

    XrFrameEndInfo end;
    memset(&end, 0, sizeof end);
    end.type = XR_TYPE_FRAME_END_INFO;
    end.displayTime = s_displayTime;
    end.layerCount = rendered ? 1 : 0;
    end.layers = layers;

    XrResult r = s_xrEndFrame(s_session, &end);
    if (r != XR_SUCCESS) {
        SDL_Log("M7C XR xrEndFrame failed result=%d", (int)r);
        return 0;
    }
    return 1;
}

/* ========================================================================
 * Accessors
 * ======================================================================== */
int M7C_XrIsRunning(void) { return s_sessionRunning; }
int M7C_XrShouldQuit(void) { return s_shouldQuit; }

void M7C_XrGetHeadPose(float *x, float *y, float *z,
                        float *qx, float *qy, float *qz, float *qw) {
    *x  = s_headPose.position.x;
    *y  = s_headPose.position.y;
    *z  = s_headPose.position.z;
    *qx = s_headPose.orientation.x;
    *qy = s_headPose.orientation.y;
    *qz = s_headPose.orientation.z;
    *qw = s_headPose.orientation.w;
}

void M7C_XrShutdown(void) {
    SDL_Log("M7C XR shutdown begin");
    if (s_localSpace != XR_NULL_HANDLE) {
        s_xrDestroySpace(s_localSpace);
        s_localSpace = XR_NULL_HANDLE;
    }
    if (s_quadSwapchain != XR_NULL_HANDLE) {
        s_xrDestroySwapchain(s_quadSwapchain);
        s_quadSwapchain = XR_NULL_HANDLE;
    }
    if (s_session != XR_NULL_HANDLE) {
        s_xrDestroySession(s_session);
        s_session = XR_NULL_HANDLE;
    }
    s_sessionRunning = 0;
    s_sessionReady = 0;
    SDL_Log("M7C XR shutdown complete");
}
