#include "frame_bridge.h"
#include "vr_flight_bridge.h"
#include "vr_flight_renderer.h"
#include "input_xr.h"
#include "vr_blit.h"
#include "vr_log.h"
#include "vr_convert.h"
#include "diagnostics.h"
#include "internal.h"
#include "xwa_runtime/runtime/flight_task.h"
#include "xwa/flight/hangar.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

FILE *g_vrLog;
volatile int g_vrPhase;
volatile unsigned g_vrPhaseTick, g_vrStereo;
static int active, begun, render, captured, ready;
static XrTime display_time;
static AeronTexture output;
static SDL_GPUTextureFormat output_format;
static unsigned long long logical_frames, ticks, game_frames, textures, blits, presents;
static int periodic(unsigned long long n) { return n == 1 || n % 120 == 0; }
void VrLog(const char *format, ...) {
    /* The reused diagnostic emits many lines per eye. M8 has its own success
     * counters; retain diagnostic failures only, without claiming success. */
    if (!strstr(format, "fail") && !strstr(format, "missing") &&
        !strstr(format, "TIMEOUT") && !strstr(format, "selftest") &&
        !strstr(format,"xrEndFrame end")) return;
    char text[1024];
    va_list args;
    va_start(args, format);
    SDL_vsnprintf(text, sizeof text, format, args);
    va_end(args);
    if(strstr(format,"xrEndFrame end")) {
        if(!strstr(text,"result=0")) M8_Diag("M8_UI_XR_END_RESULT %s",text);
        return;
    }
    SDL_Log("M8_DIAGNOSTIC %s", text);
}
void M8_Fail(const char *operation) {
    SDL_Log("M8_FATAL operation=%s SDL=%s", operation, SDL_GetError());
    Aeron_RequestFatalError("XWAQuest M8", operation);
}
void __wrap_Aeron_Shutdown(void);
int __real_Aeron_Init(const AeronConfig *config);
int __wrap_Aeron_Init(const AeronConfig *config) {
    AeronConfig local = *config;
    local.org_name = "XWAQuest";
    local.app_name = "M8";
    local.window_title = "XWAQuest M8.2-CS1";
    VrFlightBridge_Reset();
    M8_Memory("M8_MEM_APP_START","before_Aeron_Init",1);
    if (setenv("XWAQUEST_XR_ENABLE", "1", 1) != 0) return 0;
    if (!__real_Aeron_Init(&local)) return 0;
    active = 1; /* ensure failed XR init also has a complete teardown */
    if (!VrConvert_SelfTest()) {
        M8_Fail("VrConvert_SelfTest");
        __wrap_Aeron_Shutdown();
        return 0;
    }
    if (!VrXr_Init(g_aeron.gpu_device) ||
        !VrBlit_Init(g_aeron.gpu_device, VrXr_SwapchainFormat())) {
        M8_Fail("XR/blit initialization");
        __wrap_Aeron_Shutdown();
        return 0;
    }
    SDL_Log("M8_XR_SESSION_RUNNING beginSession=success eyes=%d", VrXr_ViewCount());
    M8_Memory("M8_MEM_APP_START","after_XR_init",1);
    return 1;
}

int __real_Aeron_SetOutputHdr(int enabled);
int __wrap_Aeron_SetOutputHdr(int enabled) {
    static int policy_logged;
    if (!policy_logged) {
        SDL_Log("M8_OUTPUT_POLICY requested_hdr=%d forced_hdr=0 reason=XR_SDR_CAPTURE", enabled);
        policy_logged = 1;
    }
    return __real_Aeron_SetOutputHdr(0);
}

bool __real_SDL_WaitForGPUSwapchain(SDL_GPUDevice *, SDL_Window *);
bool __wrap_SDL_WaitForGPUSwapchain(SDL_GPUDevice *device, SDL_Window *window) {
    return active ? true : __real_SDL_WaitForGPUSwapchain(device, window);
}
bool __real_SDL_WaitAndAcquireGPUSwapchainTexture(SDL_GPUCommandBuffer *, SDL_Window *,
                                               SDL_GPUTexture **, Uint32 *, Uint32 *);
bool __wrap_SDL_WaitAndAcquireGPUSwapchainTexture(SDL_GPUCommandBuffer *cmd,
        SDL_Window *window, SDL_GPUTexture **texture, Uint32 *width, Uint32 *height) {
    if (!active) return __real_SDL_WaitAndAcquireGPUSwapchainTexture(cmd, window, texture, width, height);
    int w = 0, h = 0;
    if (!SDL_GetWindowSizeInPixels(window, &w, &h) || w <= 0 || h <= 0) return false;
    SDL_GPUTextureFormat format = SDL_GetGPUSwapchainTextureFormat(g_aeron.gpu_device, window);
    /* Match the existing compositor's SDR pipeline. Reject HDR until it is
     * explicitly in scope, rather than silently applying a wrong conversion. */
    if (format != SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM && format != SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB &&
        format != SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM && format != SDL_GPU_TEXTUREFORMAT_B8G8R8A8_UNORM_SRGB)
        return SDL_SetError("M8.1 requires an SDR game output texture");
    if (!output.texture || output.width != w || output.height != h || output_format != format) {
        SDL_GPUTextureCreateInfo info = {0};
        info.type = SDL_GPU_TEXTURETYPE_2D;
        info.format = format;
        info.usage = SDL_GPU_TEXTUREUSAGE_COLOR_TARGET | SDL_GPU_TEXTUREUSAGE_SAMPLER;
        info.width = w; info.height = h; info.layer_count_or_depth = 1; info.num_levels = 1;
        info.sample_count = SDL_GPU_SAMPLECOUNT_1;
        SDL_GPUTexture *next = SDL_CreateGPUTexture(g_aeron.gpu_device, &info);
        if (!next) return false;
        if (output.texture) SDL_ReleaseGPUTexture(g_aeron.gpu_device, output.texture);
        output.texture = next; output.width = w; output.height = h;
        output.format = AERON_TEXTURE_FORMAT_RGBA8_UNORM;
        output_format = format; ready = 0;
    }
    *texture = output.texture; *width = w; *height = h;
    captured = 1;
    return true;
}

int32_t __real_Aeron_BeginFrame(void);
int32_t __wrap_Aeron_BeginFrame(void) {
    VrXr_PollEvents(g_aeron.gpu_device);
    if (VrXr_ShouldQuit() || !VrXr_IsSessionRunning()) {
        Aeron_RequestQuit();
        return 0;
    }
    if (begun || !VrXr_WaitBeginFrame(&render, &display_time)) {
        M8_Fail("xrWaitFrame/xrBeginFrame");
        return 0;
    }
    begun = 1;
    ++logical_frames;
    M8_MemoryPoll();
    int focused = M8_InputPoll();
    int32_t delta = __real_Aeron_BeginFrame();
    /* Focus comes from xrSyncActions, not Android's hidden 2D window. */
    g_aeron.input.has_focus = focused;
    M8_InputApplyPointer(focused);
    return delta;
}
void __wrap_Aeron_WaitForNextFrame(uint64_t delay) {
    (void)delay; /* xrWaitFrame is the sole display pacing source. */
}
void __real_XwaPort_Tick(int32_t delta);
void __wrap_XwaPort_Tick(int32_t delta) {
    static unsigned long long last_tick_frame;
    static int last_phase = -1;
    if (last_tick_frame == logical_frames) {
        M8_Fail("more than one game tick in logical frame");
        return;
    }
    last_tick_frame = logical_frames;
    __real_XwaPort_Tick(delta);
    VrFlightBridge_CaptureAfterTick(logical_frames);
    if (periodic(++ticks)) SDL_Log("M8_GAME_TICK completed=%llu logical=%llu delta_us=%d", ticks, logical_frames, delta);
    int phase = XwaFlightTask_IsActive() ? (g_inHangarReady ? 1 : 2) : 0;
    if (phase != last_phase) {
        SDL_Log("%s tick=%llu active=%d hangar=%d", phase == 0 ? "M8_FRONTEND" : phase == 1 ? "M8_HANGAR" : "M8_FLIGHT",
                ticks, XwaFlightTask_IsActive(), g_inHangarReady);
        last_phase = phase;
    }
}
void __real_XwaRemaster_Frame(int32_t delta);
void __wrap_XwaRemaster_Frame(int32_t delta) {
    __real_XwaRemaster_Frame(delta);
    if (periodic(++game_frames)) SDL_Log("M8_GAME_FRAME completed=%llu layers=%d", game_frames, g_aeron.render_layer_count);
}
int __real_XwaFlightTask_Init(char *command_line, const char *film_path);
int __wrap_XwaFlightTask_Init(char *command_line, const char *film_path) {
    SDL_Log("M8_LAUNCH mission_initialization=begin");
    int r = __real_XwaFlightTask_Init(command_line, film_path);
    SDL_Log("M8_LAUNCH returned result=%d active=%d", r, XwaFlightTask_IsActive());
    return r;
}

uint16_t __real_Mission_Init(char *file);
uint16_t __wrap_Mission_Init(char *file) {
    VrFlightBridge_BeginMissionLoad();
    uint16_t result = __real_Mission_Init(file);
    VrFlightBridge_EndMissionLoad(result != 0);
    return result;
}
int M8_XrLocateViews(XrTime time, XrView views[VR_EYE_COUNT]);
int __real_Aeron_Present(void);
int __wrap_Aeron_Present(void) {
    captured = 0;
    int ok = __real_Aeron_Present();
    if (ok && captured) {
        ready = 1;
        if (periodic(++textures)) SDL_Log("M8_PRESENT_TEXTURE_READY submitted=%llu size=%dx%d format=%d", textures, output.width, output.height, output_format);
    }
    int rendered = 0;
    const VrFlightSnapshot *flight = VrFlightBridge_GetLatest();
    int immersive_frame = VrFlightRenderer_BeginFrame(flight);
    XrView views[VR_EYE_COUNT] = {{.type = XR_TYPE_VIEW}, {.type = XR_TYPE_VIEW}};
    XrPosef poses[VR_EYE_COUNT] = {0};
    XrFovf fovs[VR_EYE_COUNT] = {0};
    int views_valid=-1;
    if(ok && begun && render && ready) views_valid=M8_XrLocateViews(display_time, views);
    if (ok && begun && render && ready && views_valid==1) {
        SDL_GPUTexture *eye_images[VR_EYE_COUNT] = {0};
        for (int eye = 0; eye < VR_EYE_COUNT; ++eye) {
            poses[eye] = views[eye].pose; fovs[eye] = views[eye].fov;
            if (!M8_XrAcquire(eye, &eye_images[eye])) { ok = 0; break; }
        }
        AeronCommandBuffer *cmd = NULL;
        if (ok) {
            cmd = Aeron_AcquireCommandBuffer();
            if (!cmd) ok = 0;
        }
        /* M8test2 experiment: invoke the REAL window-swapchain acquire on the
         * submitted blit command buffer. SDL3 Vulkan sets swapchainRequested on
         * that CB, which is the sole gate for its per-submit cleanup
         * (command/uniform/descriptor/fence recycling). Never bound: the dummy
         * texture is only observed for telemetry. Non-fatal by design. */
        static unsigned long long t2_acq, t2_acq_ok, t2_acq_null;
        if (ok && cmd && cmd->command_buffer) {
            SDL_GPUTexture *dummy = NULL;
            Uint32 dw = 0, dh = 0;
            ++t2_acq;
            if (__real_SDL_WaitAndAcquireGPUSwapchainTexture(cmd->command_buffer,
                    g_aeron.window, &dummy, &dw, &dh)) {
                ++t2_acq_ok;
                if (!dummy) ++t2_acq_null;
            }
        }
        if (periodic(logical_frames) && t2_acq)
            SDL_Log("M8T2_CLEANUP_ACQUIRE attempted=%llu ok=%llu dummy_null=%llu blits=%llu",
                t2_acq, t2_acq_ok, t2_acq_null, blits);
        for (int eye = 0; ok && eye < VR_EYE_COUNT; ++eye) {
            VrEyeSwapchain *e = VrXr_Eye(eye);
            AeronTexture *source = &output;
            if (immersive_frame) {
                VrEyeState state;
                VrFlightRenderer_ReadEye(&views[eye], e->width, e->height, &state);
                source = VrFlightRenderer_RenderEye(eye, &state, flight, cmd);
                if (!source) { ok = 0; break; }
            }
            ok = VrBlit_Copy(cmd, g_aeron.gpu_device, source, eye_images[eye], e->width, e->height);
        }
        if (cmd) {
            if (ok) ok = Aeron_SubmitCommandBuffer(cmd);
            else Aeron_CancelCommandBuffer(cmd);
        }
        VrFlightRenderer_EndFrame(ok && immersive_frame);
        if (ok && periodic(++blits)) SDL_Log("M8_XR_BLIT submitted=%llu eyes=2 source=%s", blits, immersive_frame ? "cs1_scene" : "game_compositor");
        for (int eye = 0; eye < VR_EYE_COUNT; ++eye) if (!M8_XrRelease(eye)) ok = 0;
        rendered = ok;
    }
    VrFlightRenderer_EndFrame(0);
    static unsigned last_status=~0u;
    unsigned status=(ok?1u:0u)|(begun?2u:0u)|(render?4u:0u)|(ready?8u:0u)|
        (views_valid==1?16u:0u)|(rendered?32u:0u)|((flight->flags&VR_FLIGHT_VALID)?64u:0u)|(immersive_frame?128u:0u);
    if(status!=last_status) {
        M8_Diag("M8_UI_FRAME_STATE logical=%llu ok=%d begun=%d shouldRender=%d frame_ready=%d views_valid=%d bridge_valid=%d immersive=%d layers=%d",
            logical_frames,ok,begun,render,ready,views_valid,!!(flight->flags&VR_FLIGHT_VALID),immersive_frame,rendered?1:0);
        last_status=status;
    }
    if (begun) {
        int ended = VrXr_EndFrame(display_time, poses, fovs, rendered);
        if(!ended) M8_Diag("M8_UI_END_FRAME_FAILED logical=%llu layers=%d",logical_frames,rendered?1:0);
        begun = 0;
        if (!ended) ok = 0;
        if (ended && rendered && periodic(++presents))
            SDL_Log("M8_XR_FRAME_PRESENTED accepted=%llu layers=1 result=0 visual=unconfirmed", presents);
    }
    if (!ok) M8_Fail("game compositor/XR presentation");
    return ok;
}
void __real_Aeron_Shutdown(void);
void __wrap_Aeron_Shutdown(void) {
    if (active) {
        if (begun) {
            if (!VrXr_EndFrame(display_time, NULL, NULL, 0)) SDL_Log("M8_XR_END_ON_EXIT_FAILED");
            begun = 0;
        }
        if (!SDL_WaitForGPUIdle(g_aeron.gpu_device)) SDL_Log("M8_GPU_IDLE_FAILED %s", SDL_GetError());
        VrFlightRenderer_Reset();
        VrFlightBridge_Reset();
        if (output.texture) SDL_ReleaseGPUTexture(g_aeron.gpu_device, output.texture);
        output.texture = NULL;
        VrBlit_Shutdown(g_aeron.gpu_device);
        M8_InputShutdown();
        VrXr_Shutdown(g_aeron.gpu_device);
        active = 0;
        SDL_Log("M8_SHUTDOWN ticks=%llu game_frames=%llu presented=%llu", ticks, game_frames, presents);
    }
    __real_Aeron_Shutdown();
}
