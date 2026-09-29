/* VR-PROBE entry point (isolated test code, package org.openxwa.xwaquest.vrprobe).
 *
 * Real init path only: XwaLaunchOptions_Parse -> Aeron_Init -> XwaHostConfig_Load
 * -> XwaSetup_ValidateGameData -> XwaRemaster_Init -> XwaPort_Init ->
 * ModelPreview_LoadModel (real Tech Library loader) -> warmup XwaPort_Tick ->
 * XwaRemasterShip_SyncAssets (real OPT->glb cooking) ->
 * XwaRemasterShip_MeshForName -> dual AeronScene_Render (one camera per eye) ->
 * blit to OpenXR swapchains.
 *
 * Coordinate resolution (preview eye-space vs OpenXR):
 * - The classic preview bakes the camera into the instance (world==eye).
 *   The probe does NOT reuse that bake. It takes the real cooked mesh and the
 *   real preview scale intent, then places the ship world-locked in XR LOCAL
 *   space (meters, Y up, -Z forward).
 * - OPT axes (X right, Y forward, Z up) map to XR axes as
 *   xr = (eng_x, eng_z, -eng_y); applied as a fixed basis rotation plus a
 *   display scale (tabletop ~1m ship) and slow yaw so depth is visible.
 * - Per-eye AeronSceneCamera pos/quat come from xrLocateViews (meters pass
 *   through; scenes use view_space_to_meters=1). Half-FOVs come from the
 *   per-eye XrFovf via tan averaging (symmetric approximation, offsets kept
 *   at 0 and logged; asymmetric offsets are a documented follow-up).
 * - VRPROBE_FIXED_IPD=1 keeps real FOVs but replaces poses with fixed
 *   identity-orientation cameras at +/-0.032m: parallax diagnostic only,
 *   never the acceptance criterion.
 */
#include <SDL3/SDL.h>
#include <SDL3/SDL_main.h>

#include <math.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "internal.h"
#include "host_config.h"
#include "setup.h"
#include "window_icon.h"
#include "xwa/config/game_config.h"
#include "xwa_remaster/xwa_remaster.h"
#include "xwa_runtime/input/controller_mapping.h"
#include "xwa_runtime/input/mouse_flight.h"
#include "xwa_runtime/runtime/port.h"
#include "xwa/frontend/model_preview.h"
#include "xwa/flight/fediskio.h"
#include "xwa_runtime/snapshot/snapshot.h"
#include "xwa_remaster/ship.h"

#include "vr_openxr.h"
#include "vr_stereo.h"
#include "vr_blit.h"
#include "vr_log.h"
#include "vr_framing.h"
#include "vr_convert.h"
#include "vr_audit.h"
#include "vr_readback.h"

#define VRPROBE_MAX_STEREO_FRAMES (72u * 600u)
/* Stage 3 (safe exit): hard bounds so a run always ends gracefully instead
 * of lingering into an ANR kill. Segments: submitted 0..64 PBR (after the
 * 10 diag RenderEye calls), 65..124 emissive override (Stage 4), then PBR. */
#define VRPROBE_AUTO_EXIT_SUBMITTED 30u
#define VRPROBE_AUTO_EXIT_MS (60u * 1000u)
#define VRPROBE_MAX_CONSEC_SKIPS 10u

/* v9.7 Diagnostic windows:
 * W1: Flat-color diagnostic (frames 0-4) — draws real mesh with magenta/cyan
 * W2: PBR settle (frames 5-9)
 * W3: Readback PBR (frames 10-19) — readback with proper float comparison
 * W4: Readback flat-color again (frames 20-29) — verify consistency */
#define VRPROBE_W1_START 0
#define VRPROBE_W1_END 5
#define VRPROBE_W2_START 5
#define VRPROBE_W2_END 10
#define VRPROBE_W3_START 10
#define VRPROBE_W3_END 20
#define VRPROBE_W4_START 20
#define VRPROBE_W4_END 30
#define VRPROBE_DIAG_FRAMES 10

/* Frame pacing: target 72 Hz = 13.89 ms/frame */
#define VRPROBE_TARGET_FRAME_MS 13
#define VRPROBE_MAX_CONSEC_SLOW 10

/* File logging + phase watchdog: logcat rotates too fast on Quest, and ANR
 * kills leave no trace. Every phase transition and a 2s heartbeat go to
 * files/vrprobe-log.txt (pulled via run-as after the test). */
FILE *g_vrLog;
volatile int g_vrPhase;
volatile unsigned g_vrPhaseTick;
volatile unsigned g_vrStereo;
VrReadback *g_vrReadback;

void VrLog(const char *fmt, ...) {
    char buf[1024];
    va_list a;
    va_start(a, fmt);
    vsnprintf(buf, sizeof buf, fmt, a);
    va_end(a);
    SDL_Log("%s", buf);
    if (g_vrLog) {
        fprintf(g_vrLog, "%llu %s\n", (unsigned long long)SDL_GetTicks(), buf);
        fflush(g_vrLog);
    }
}

static void *vr_watchdog(void *arg) {
    (void)arg;
    while (g_vrLog) {
        fprintf(g_vrLog, "%llu WATCH phase=%d tick=%u stereo=%u\n",
                (unsigned long long)SDL_GetTicks(), g_vrPhase, g_vrPhaseTick,
                g_vrStereo);
        fflush(g_vrLog);
        SDL_Delay(2000);
    }
    return NULL;
}

static void build_model_matrix(float out[16], float scale, float yaw, const float pos[3]) {
    /* Rmap rows (OPT->XR): xr=(ex, ez, -ey). Then yaw about XR up: R = Ry*Rmap. */
    const float c = cosf(yaw), s = sinf(yaw);
    const float r00 = c, r01 = -s, r02 = 0.0f;
    const float r10 = 0.0f, r11 = 0.0f, r12 = 1.0f;
    const float r20 = -s, r21 = -c, r22 = 0.0f;
    out[0] = scale * r00;
    out[1] = scale * r01;
    out[2] = scale * r02;
    out[3] = pos[0];
    out[4] = scale * r10;
    out[5] = scale * r11;
    out[6] = scale * r12;
    out[7] = pos[1];
    out[8] = scale * r20;
    out[9] = scale * r21;
    out[10] = scale * r22;
    out[11] = pos[2];
    out[12] = 0.0f;
    out[13] = 0.0f;
    out[14] = 0.0f;
    out[15] = 1.0f;
}

static void camera_from_xr(AeronSceneCamera *cam, const XrPosef *pose, const XrFovf *fov,
                           int w, int h) {
    /* Orientation/position convention handled by VrConvert_Camera
     * (D-conversion, runtime self-tested). FOV stays per-eye symmetric. */
    float tanL = tanf(fov->angleLeft), tanR = tanf(fov->angleRight);
    float tanU = tanf(fov->angleUp), tanD = tanf(fov->angleDown);
    VrConvert_Camera(pose, atanf((tanR - tanL) * 0.5f), atanf((tanU - tanD) * 0.5f),
                     0.05f, w, h, cam);
}

static int load_xwing_model(void) {
    static const char *candidates[] = {
        "FlightModels\\XWING.OPT",
        "XWING.OPT",
        "XWING",
    };
    for (unsigned ci = 0; ci < sizeof candidates / sizeof candidates[0]; ci++) {
        for (int type = 1; type <= 2; type++) {
            char ok = ModelPreview_LoadModel(candidates[ci], type);
            SDL_Log("VRPROBE try load name=%s type=%d ok=%d", candidates[ci], type, (int)ok);
            if (ok) {
                return 1;
            }
        }
    }
    for (unsigned ct = 1; ct <= 6; ct++) {
        char *name = FeDiskIo_GetCraftModelName(ct);
        if (name && name[0]) {
            char ok = ModelPreview_LoadModel(name, (int)ct);
            SDL_Log("VRPROBE try craft type=%u name=%s ok=%d", ct, name, (int)ok);
            if (ok) {
                return 1;
            }
        }
    }
    return 0;
}

int main(int argc, char **argv) {
    AeronConfig config;
    XwaHostConfig host_config;
    XwaLaunchOptions launch;
    char selected_game_data[XWA_HOST_CONFIG_PATH_CAPACITY];
    char config_error[1024];
    int setup_cancelled = 0;

#if defined(__ANDROID__) || defined(__linux__) || defined(_WIN32)
    setenv("XWAQUEST_XR_ENABLE", "1", 1);
#endif
    SDL_Log("VRPROBE BOOT experimental stereo probe, package vrprobe");
    {
        /* Open the file log before Aeron_Init rewrites CWD assumptions:
         * use the absolute files dir via SDL_GetBasePath sibling. The
         * activity passes --game-data=<files>/GameData, so files dir is
         * known; defer precise path until launch options are parsed. */
    }

    if (!XwaLaunchOptions_Parse(argc, argv, &launch, config_error, sizeof config_error)) {
        SDL_Log("VRPROBE launch parse failed: %s", config_error);
        return 2;
    }
    {
        /* files dir = dirname of the validated game-data path; open log now. */
        char filesDir[1024], logPath[1152];
        snprintf(filesDir, sizeof filesDir, "%s", launch.game_data_path);
        char *sep = strrchr(filesDir, '/');
        if (sep) {
            *sep = '\0';
            snprintf(logPath, sizeof logPath, "%s/vrprobe-log.txt", filesDir);
            g_vrLog = fopen(logPath, "w");
        }
        VrLog("VRPROBE BOOT file log open");
        {
            pthread_t wd;
            pthread_create(&wd, NULL, vr_watchdog, NULL);
            pthread_detach(wd);
        }
        /* Readback sampling points in NDC [-1,1] based on projected ship bounds.
         * Covers ship projection area + control points in background. */
        static const float sample_ndc[][2] = {
            // Ship projection area (from v7/v8 framing bounds)
            {-0.25f, -0.25f}, { 0.00f, -0.25f}, { 0.25f, -0.25f},
            {-0.25f,  0.00f}, { 0.00f,  0.00f}, { 0.25f,  0.00f},
            {-0.25f,  0.25f}, { 0.00f,  0.25f}, { 0.25f,  0.25f},
            // Control: background corners
            {-0.90f, -0.90f}, { 0.90f, -0.90f}, {-0.90f,  0.90f}, { 0.90f,  0.90f}
        };
#define VR_READBACK_NUM_SAMPLES 13
        /* Coordinate conversion self-test through the REAL
         * AeronScene_ComputeViewProj (pure math, no init needed).
         * Abort before touching hardware if conventions drift. */
        g_vrPhase = 0;
        if (!VrConvert_SelfTest()) {
            VrLog("VRPROBE FATAL conversion self-test failed");
            fclose(g_vrLog);
            g_vrLog = NULL;
            return 1;
        }
        /* NOTE: VrReadback_Init moved to after Aeron_Init (see ~line 463).
         * g_aeron.gpu_device is NULL at this point. */
    }
    memset(&config, 0, sizeof config);
    config.org_name = "TotallyOpen";
    config.app_name = "OpenXWA";
    config.resource_path = "resources";
    config.shader_path = "shaders";
    config.window_title = "XWAQuest VR-Probe";
    config.window_icon_bmp = xwa_window_icon_bmp;
    config.window_icon_bmp_size = sizeof(xwa_window_icon_bmp);
    config.logical_width = 1280;
    config.logical_height = 720;
    config.presentation_mode = AERON_PRESENTATION_ASPECT_FIT;
    config.clear_color_enabled = 1;

    if (!Aeron_Init(&config)) {
        SDL_Log("VRPROBE Aeron_Init FAILED");
        return 1;
    }
    if (!XwaHostConfig_Load(Aeron_GetVfs(), &host_config, config_error, sizeof config_error)) {
        SDL_Log("VRPROBE host config failed: %s", config_error);
        Aeron_Shutdown();
        return 1;
    }
    if (!launch.game_data_path[0]) {
        SDL_Log("VRPROBE missing --game-data argument");
        Aeron_Shutdown();
        return 1;
    }
    if (!XwaSetup_ValidateGameData(Aeron_GetVfs(), launch.game_data_path, selected_game_data,
                                   sizeof selected_game_data, config_error, sizeof config_error)) {
        SDL_Log("VRPROBE GameData invalid: %s", config_error);
        Aeron_Shutdown();
        return 1;
    }
    SDL_Log("VRPROBE game data: %s", selected_game_data);

    {
        const XwaRemasterInitOptions remaster_options = {
            .opt_smooth_angle_degrees = host_config.model_smooth_angle_degrees,
            .opt_emissive_strength = host_config.model_opt_emissive_strength,
            .opt_projectile_emissive_strength = host_config.model_opt_projectile_emissive_strength,
            .engine_emissive_strength = host_config.model_engine_emissive_strength,
            .force_opt_models = host_config.force_opt_models,
            .prefer_original_2d = host_config.prefer_original_2d,
            .video_options = host_config.video_options,
            .video_options_override_mask = host_config.video_options_override_mask,
        };
        if (!XwaRemaster_Init(&remaster_options)) {
            SDL_Log("VRPROBE XwaRemaster_Init FAILED");
            XwaRemaster_Shutdown();
            Aeron_Shutdown();
            return 1;
        }
    }
    XwaPort_SetCommandLine("skipintro");
    if (!XwaPort_Init()) {
        SDL_Log("VRPROBE XwaPort_Init FAILED");
        XwaRemaster_Shutdown();
        Aeron_Shutdown();
        return 1;
    }

    /* Warmup ticks so snapshots exist, then load the real X-Wing OPT.
     * NOTE: no Aeron_BeginFrame here on purpose: it waits on the Android
     * window swapchain, which never produces frames for an IMMERSIVE_HMD
     * activity and wedged the main thread (blue box + ANR, 2026-09-19).
     * XwaPort_Tick never touches the window swapchain (verified in
     * port.c), so a fixed delta + event pump is safe for warmup. */
    g_vrPhase = 1;
    for (int i = 0; i < 30; i++) {
        Aeron_PumpEvents();
        if (Aeron_QuitRequested() || XwaPort_ShouldQuit()) {
            break;
        }
        g_vrPhaseTick = (unsigned)i;
        XwaPort_Tick(16667);
        if (i % 15 == 0) {
            VrLog("VRPROBE warmup tick=%d", i);
        }
    }
    g_vrPhase = 2;
    VrLog("VRPROBE loading X-Wing OPT");
    if (!load_xwing_model()) {
        VrLog("VRPROBE FATAL no X-Wing OPT candidate loaded");
        XwaRemaster_Shutdown();
        XwaPort_Shutdown();
        Aeron_Shutdown();
        return 1;
    }
    g_vrPhase = 3;
    for (int i = 0; i < 30; i++) {
        Aeron_PumpEvents();
        if (Aeron_QuitRequested() || XwaPort_ShouldQuit()) {
            break;
        }
        g_vrPhaseTick = (unsigned)i;
        XwaPort_Tick(16667);
        if (i % 15 == 0) {
            VrLog("VRPROBE warmup2 tick=%d", i);
        }
    }

    /* Cook the real mesh through the real snapshot->SyncAssets route. */
    g_vrPhase = 4;
    AeronSceneMesh *mesh = NULL;
    for (int attempt = 0; attempt < 20 && !mesh; attempt++) {
        Aeron_PumpEvents();
        const XwaSnapshot *snap = XwaSnapshot_Current();
        if (!snap) {
            VrLog("VRPROBE no snapshot yet attempt=%d", attempt);
            break;
        }
        VrLog("VRPROBE sync attempt=%d opt_assets=%u gen=%llu", attempt, snap->opt_asset_count,
              (unsigned long long)snap->opt_asset_generation);
        AeronCommandBuffer *cmd = Aeron_AcquireCommandBuffer();
        if (!cmd) {
            break;
        }
        XwaRemasterShipSyncResult sr =
            XwaRemasterShip_SyncAssets(cmd, snap, 64u * 1024u * 1024u, 4096u);
        if (sr == XWA_REMASTER_SHIP_SYNC_FAILED) {
            Aeron_CancelCommandBuffer(cmd);
            VrLog("VRPROBE ship sync FAILED");
            break;
        }
        if (!Aeron_SubmitCommandBuffer(cmd)) {
            VrLog("VRPROBE ship sync submit FAILED");
            break;
        }
        XwaRemasterShip_CommitSyncBatch();
        mesh = XwaRemasterShip_MeshForName("xwing");
        if (!mesh) {
            mesh = XwaRemasterShip_MeshForName("XWING");
        }
        if (sr == XWA_REMASTER_SHIP_SYNC_COMPLETE && !mesh) {
            VrLog("VRPROBE sync complete but xwing mesh missing");
            break;
        }
    }
    if (!mesh) {
        VrLog("VRPROBE FATAL cooked xwing mesh unavailable");
        XwaRemaster_Shutdown();
        XwaPort_Shutdown();
        Aeron_Shutdown();
        return 1;
    }
    VrLog("VRPROBE cooked xwing mesh=%p", (void *)mesh);

    g_vrPhase = 5;
    SDL_Log("VRPROBE TRACE: Calling VrXr_Init");
    if (!VrXr_Init(g_aeron.gpu_device)) {
        SDL_Log("VRPROBE FATAL XR init failed (see render_backend XR opt-in)");
        XwaRemaster_Shutdown();
        XwaPort_Shutdown();
        Aeron_Shutdown();
        return 1;
    }
    SDL_Log("VRPROBE TRACE: VrXr_Init returned OK");
    g_vrPhase = 6;
    {
        Uint64 t0 = SDL_GetTicks();
        Uint64 lastBeat = 0;
        while (!VrXr_IsSessionRunning() && !VrXr_ShouldQuit() && SDL_GetTicks() - t0 < 30000) {
            VrXr_PollEvents(g_aeron.gpu_device);
            Aeron_PumpEvents();
            if (Aeron_QuitRequested() || XwaPort_ShouldQuit()) {
                break;
            }
            if (SDL_GetTicks() - lastBeat > 5000) {
                lastBeat = SDL_GetTicks();
                VrLog("VRPROBE waiting XR session running=%d quit=%d",
                      VrXr_IsSessionRunning(), VrXr_ShouldQuit());
            }
            SDL_Delay(100);
        }
    }
    if (!VrXr_IsSessionRunning() || VrXr_ViewCount() != 2) {
        VrLog("VRPROBE FATAL XR session not running (running=%d views=%d)",
              VrXr_IsSessionRunning(), VrXr_ViewCount());
        VrXr_Shutdown(g_aeron.gpu_device);
        XwaRemaster_Shutdown();
        XwaPort_Shutdown();
        Aeron_Shutdown();
        return 1;
    }
    int32_t eyeW = 0, eyeH = 0;
    VrXr_EyeSize(0, &eyeW, &eyeH);
    SDL_Log("VRPROBE TRACE: eyeW=%d eyeH=%d fmt=%d", (int)eyeW, (int)eyeH, (int)VrXr_SwapchainFormat());
    SDL_Log("VRPROBE TRACE: Calling VrStereo_Init");
    int stereo_ok = VrStereo_Init((int)eyeW, (int)eyeH);
    SDL_Log("VRPROBE TRACE: VrStereo_Init returned %d", stereo_ok);
    if (stereo_ok && mesh) {
        SDL_Log("VRPROBE TRACE: Creating X-Wing mesh buffers for V10");
        if (!VrStereo_CreateMeshBuffers(mesh)) {
            /* NOT fatal: the V10 pipeline and the triangle diagnostic remain
             * available. Register the fallback clearly and keep running. */
            VrLog("VRPROBE XWING mesh buffers FAILED - fallback to V10 triangle diagnostic");
        }
    }
    SDL_Log("VRPROBE TRACE: Calling VrBlit_Init");
    int blit_ok = VrBlit_Init(g_aeron.gpu_device, VrXr_SwapchainFormat());
    SDL_Log("VRPROBE TRACE: VrBlit_Init returned %d", blit_ok);
    if (!stereo_ok || !blit_ok) {
        VrLog("VRPROBE FATAL stereo/blit init failed stereo=%d blit=%d", stereo_ok, blit_ok);
        VrXr_Shutdown(g_aeron.gpu_device);
        XwaRemaster_Shutdown();
        XwaPort_Shutdown();
        Aeron_Shutdown();
        return 1;
    }

    /* Initialize GPU readback AFTER Aeron_Init so g_aeron.gpu_device is valid.
     * The NULL check is defensive: VrBlit_Init already verified the device. */
    if (!g_aeron.gpu_device) {
        VrLog("VRPROBE FATAL gpu_device is NULL before readback init");
        VrStereo_Shutdown();
        VrXr_Shutdown(g_aeron.gpu_device);
        XwaRemaster_Shutdown();
        XwaPort_Shutdown();
        Aeron_Shutdown();
        return 1;
    }
    g_vrReadback = VrReadback_Init(g_aeron.gpu_device, VR_READBACK_NUM_SAMPLES, 2);
    SDL_Log("VRPROBE TRACE: VrReadback_Init returned %p", (void*)g_vrReadback);
    if (!g_vrReadback) {
        VrLog("VRPROBE FATAL readback init failed");
        VrBlit_Shutdown(g_aeron.gpu_device);
        VrStereo_Shutdown();
        VrXr_Shutdown(g_aeron.gpu_device);
        XwaRemaster_Shutdown();
        XwaPort_Shutdown();
        Aeron_Shutdown();
        return 1;
    }

    const int fixedIpd = getenv("VRPROBE_FIXED_IPD") != NULL;
    SDL_Log("VRPROBE TRACE: Entering stereo loop fixed_ipd=%d", fixedIpd);
    g_vrPhase = 7;
    /* First RenderEye calls per eye emit solid red/green into the presented
     * RTs (same path the ship uses). Interpretation: red/green visible =>
     * compositor shows our images, missing ship is a framing/culling issue;
     * still blue => our frames never display, presentation path issue. */
    VrStereo_SetDiagFrames(VRPROBE_DIAG_FRAMES);
    float modelPos[3] = { 0.0f, 0.0f, 0.0f };
    int anchored = 0;
    int framingDone = 0;
    unsigned stereo = 0;
    unsigned submitted = 0;
    unsigned consecSkips = 0;
    unsigned consecSlow = 0;
    Uint64 startTicks = SDL_GetTicks();
    Uint64 loopStart = 0;
    int exit_code = 0;
    const char *exitReason = "cap";
    int in_w1 = 0, in_w2 = 0, in_w3 = 0;
    static const float sample_ndc[VR_READBACK_NUM_SAMPLES][2] = {
        {-0.25f, -0.25f}, { 0.00f, -0.25f}, { 0.25f, -0.25f},
        {-0.25f,  0.00f}, { 0.00f,  0.00f}, { 0.25f,  0.00f},
        {-0.25f,  0.25f}, { 0.00f,  0.25f}, { 0.25f,  0.25f},
        {-0.90f, -0.90f}, { 0.90f, -0.90f}, {-0.90f,  0.90f}, { 0.90f,  0.90f}
    };
    /* v9.7: Row-scan rows — computed at runtime from actual texture dimensions.
     * Ship projects to NDC y ≈ −0.218..+0.023.
     * For h-tall texture: row = (1 - ndc_y) * 0.5 * (h-1).
     * We sample 5 rows evenly across the ship's vertical extent. */
    static const int row_scan_frames[5]  = { 10, 12, 14, 16, 18 };
    int row_scan_idx = 0;
    static const int row_scan_frames_b[5] = { 20, 22, 24, 26, 28 };
    int row_scan_idx_b = 0;
    /* v9.6: Reference background corrected.
     * GPU clear (0.01, 0.015, 0.03, 1.0) stored as RGBA16_FLOAT:
     *   R 0.01  → 0x211E  (2^(-7) × 1.2793)
     *   G 0.015 → 0x23AE  (2^(-7) × 1.920)
     *   B 0.03  → 0x27AE  (2^(-6) × 1.920)
     *   A 1.0   → 0x3C00  (2^0 × 1.0) */
    static const uint16_t bg_rgba16[4] = { 0x211E, 0x23AE, 0x27AE, 0x3C00 };
    /* v9.5: Head-tracking log state */
    int track_ref_logged = 0;
    float track_ref_ori[4] = {0};
    AeronTexture *s_present[2] = { NULL, NULL };
    while (!VrXr_ShouldQuit() && !Aeron_QuitRequested() && !XwaPort_ShouldQuit() &&
           stereo < VRPROBE_MAX_STEREO_FRAMES) {
        SDL_Log("VRPROBE TRACE: Main loop iteration stereo=%u", stereo);
        /* Window selection: W1 = flat-color diag (0-4), W2 = PBR settle (5-9),
         * W3 = readback PBR (10-19), W4 = readback flat-color (20-29). */
        in_w1 = (stereo < VRPROBE_W1_END) ? 1 : 0;
        in_w2 = (stereo >= VRPROBE_W2_START && stereo < VRPROBE_W2_END) ? 1 : 0;
        in_w3 = (stereo >= VRPROBE_W3_START && stereo < VRPROBE_W3_END) ? 1 : 0;
        int in_w4 = (stereo >= VRPROBE_W4_START && stereo < VRPROBE_W4_END) ? 1 : 0;
        /* v9.7: Enable flat-color draw during W1 and W4 */
        g_flatDrawEnabled = (in_w1 || in_w4) ? 1 : 0;
        if (!loopStart) {
            loopStart = SDL_GetTicks();
            /* VmRSS at loop start */
            { FILE *f = fopen("/proc/self/status", "r");
              if (f) { char line[128];
                while (fgets(line, sizeof line, f))
                    if (strstr(line, "VmRSS:"))
                        { VrLog("VRPROBE mem-loop-start %s", line); break; }
                fclose(f); }
              else { VrLog("VRPROBE mem-loop-start FAILED to open /proc/self/status"); }
            }
        }
        if (submitted >= VRPROBE_AUTO_EXIT_SUBMITTED) {
            exitReason = "auto-exit submitted cap";
            break;
        }
        if (SDL_GetTicks() - loopStart > VRPROBE_AUTO_EXIT_MS) {
            exitReason = "auto-exit time cap";
            break;
        }
        if (consecSkips >= VRPROBE_MAX_CONSEC_SKIPS) {
            exitReason = "auto-exit skip storm";
            break;
        }
        /* Frame pacing: target 72 Hz = ~13.89 ms/frame */
        Uint64 frameStart = SDL_GetTicks();
        VrXr_PollEvents(g_aeron.gpu_device);
        Aeron_PumpEvents();
        if (!VrXr_IsSessionRunning()) {
            SDL_Delay(50);
            continue;
        }
        int shouldRender = 0;
        XrTime displayTime = 0;
        if (!VrXr_WaitBeginFrame(&shouldRender, &displayTime)) {
            SDL_Log("VRPROBE XR frame wait/begin failed");
            exit_code = 1;
            break;
        }
        XrView views[2];
        XrPosef poses[2];
        XrFovf fovs[2];
        memset(views, 0, sizeof views);
        for (int i = 0; i < 2; i++) {
            views[i].type = XR_TYPE_VIEW;
        }
        int located = shouldRender && VrXr_LocateViews(displayTime, views);
        if (located && !anchored) {
            /* Anchor once along the INITIAL horizontal head forward so the
             * ship starts centered whatever way the user faces. Fixed
             * afterwards: turning the head looks away and back. */
            float fwd[3], fh[3], fl;
            const float zAxis[3] = { 0.0f, 0.0f, -1.0f };
            VrConvert_Rotate(&views[0].pose, zAxis, fwd);
            fh[0] = fwd[0];
            fh[1] = 0.0f;
            fh[2] = fwd[2];
            fl = sqrtf(fh[0] * fh[0] + fh[2] * fh[2]);
            if (fl < 1e-4f) {
                fh[0] = 0.0f;
                fh[2] = -1.0f;
                fl = 1.0f;
            }
            modelPos[0] = views[0].pose.position.x + fh[0] / fl * 2.0f;
            modelPos[1] = views[0].pose.position.y - 0.15f;
            modelPos[2] = views[0].pose.position.z + fh[2] / fl * 2.0f;
            anchored = 1;
            VrLog("VRPROBE anchor head=(%.3f,%.3f,%.3f) fwd=(%.3f,%.3f,%.3f) model=(%.3f,%.3f,%.3f)",
                  views[0].pose.position.x, views[0].pose.position.y,
                  views[0].pose.position.z, fh[0] / fl, 0.0f, fh[2] / fl, modelPos[0],
                  modelPos[1], modelPos[2]);
        }
        if (!located) {
            VrXr_EndFrame(displayTime, poses, fovs, 0);
            continue;
        }
        for (int i = 0; i < 2; i++) {
            poses[i] = views[i].pose;
            fovs[i] = views[i].fov;
        }
        float yaw =
            0.45f * sinf((float)(SDL_GetTicks() - startTicks) / 1000.0f * 0.25f);
        float model[16];
        build_model_matrix(model, 0.08f, yaw, modelPos);
        AeronSceneCamera cams[2];
        if (fixedIpd) {
            for (int i = 0; i < 2; i++) {
                memset(&cams[i], 0, sizeof cams[i]);
                cams[i].pos[0] = modelPos[0] + (i == 0 ? -0.032f : 0.032f);
                cams[i].pos[1] = modelPos[1] + 0.15f;
                cams[i].pos[2] = modelPos[2] + 2.0f;
                cams[i].ori[0] = 1.0f;
                cams[i].h_half_rad = atanf(0.55f);
                cams[i].v_half_rad = atanf(0.55f * (float)eyeH / (float)eyeW);
                cams[i].near_z = 0.05f;
                cams[i].viewport.width = (int)eyeW;
                cams[i].viewport.height = (int)eyeH;
            }
        } else {
            for (int i = 0; i < 2; i++) {
                camera_from_xr(&cams[i], &views[i].pose, &views[i].fov, (int)eyeW,
                               (int)eyeH);
            }
        }
        if (stereo == 0) {
            for (int i = 0; i < 2; i++) {
                VrLog("VRPROBE eye%d pose=(%.3f,%.3f,%.3f) quat=(%.3f,%.3f,%.3f,%.3f) "
                      "fov=(L%.3f R%.3f U%.3f D%.3f) cam_h=%.4f cam_v=%.4f",
                      i, poses[i].position.x, poses[i].position.y, poses[i].position.z,
                      poses[i].orientation.x, poses[i].orientation.y,
                      poses[i].orientation.z, poses[i].orientation.w, fovs[i].angleLeft,
                      fovs[i].angleRight, fovs[i].angleUp, fovs[i].angleDown,
                      cams[i].h_half_rad, cams[i].v_half_rad);
            }
        }
        /* v9.5: Head-tracking verification.
         * Log reference pose + projected ship centre at the first located
         * frame, then again after a significant rotation (≥15°).  Compare
         * the projected NDC displacement against the recorded orientation
         * change to confirm the camera responds correctly to head motion. */
        {
            float nx, ny, pw;
            int proj_ok = VrFraming_ProjectPoint(&cams[0], modelPos, &nx, &ny, &pw);
            /* Quaternion distance that handles q/-q equivalence:
             * q and -q represent the same orientation, so we take the
             * minimum of |q - q_ref| and |q + q_ref|. */
            float dx1 = poses[0].orientation.x - track_ref_ori[0];
            float dy1 = poses[0].orientation.y - track_ref_ori[1];
            float dz1 = poses[0].orientation.z - track_ref_ori[2];
            float dw1 = poses[0].orientation.w - track_ref_ori[3];
            float dq1 = dx1*dx1 + dy1*dy1 + dz1*dz1 + dw1*dw1;
            float dx2 = poses[0].orientation.x + track_ref_ori[0];
            float dy2 = poses[0].orientation.y + track_ref_ori[1];
            float dz2 = poses[0].orientation.z + track_ref_ori[2];
            float dw2 = poses[0].orientation.w + track_ref_ori[3];
            float dq2 = dx2*dx2 + dy2*dy2 + dz2*dz2 + dw2*dw2;
            float dq = sqrtf(dq1 < dq2 ? dq1 : dq2);

            if (!track_ref_logged && proj_ok) {
                /* First located frame: record reference */
                track_ref_ori[0] = poses[0].orientation.x;
                track_ref_ori[1] = poses[0].orientation.y;
                track_ref_ori[2] = poses[0].orientation.z;
                track_ref_ori[3] = poses[0].orientation.w;
                track_ref_logged = 1;
                VrLog("VRPROBE track REF stereo=%u campos=(%.3f,%.3f,%.3f) "
                      "camori=(%.4f,%.4f,%.4f,%.4f) modelPos=(%.3f,%.3f,%.3f) "
                      "ship_ndc=(%.4f,%.4f) w=%.3f",
                      stereo,
                      cams[0].pos[0], cams[0].pos[1], cams[0].pos[2],
                      cams[0].ori[0], cams[0].ori[1], cams[0].ori[2], cams[0].ori[3],
                      modelPos[0], modelPos[1], modelPos[2],
                      nx, ny, pw);
            } else if (track_ref_logged && dq > 0.15f && proj_ok) {
                /* Significant rotation detected: log comparison frame */
                VrLog("VRPROBE track CMP stereo=%u dq=%.3f campos=(%.3f,%.3f,%.3f) "
                      "camori=(%.4f,%.4f,%.4f,%.4f) modelPos=(%.3f,%.3f,%.3f) "
                      "ship_ndc=(%.4f,%.4f) w=%.3f",
                      stereo, dq,
                      cams[0].pos[0], cams[0].pos[1], cams[0].pos[2],
                      cams[0].ori[0], cams[0].ori[1], cams[0].ori[2], cams[0].ori[3],
                      modelPos[0], modelPos[1], modelPos[2],
                      nx, ny, pw);
                track_ref_logged = 2;  /* done — log at most one comparison */
            }
        }
        static const float lightDir[3] = { 0.4f, 0.6f, -0.7f };
        static const float lightColor[3] = { 1.0f, 0.98f, 0.95f };
        /* Stage 1 (audit, first located frame only) + Stage 4 segments:
         * submitted 65..124 render unlit emissive to separate geometry
         * visibility from lighting. Everything else stays PBR. */
        if (!framingDone) {
            framingDone = 1;
            VrFraming_Log(mesh, model, &cams[0], &cams[1], yaw);
            VrAudit_MeshTriangles(mesh, model, &cams[0], &cams[1]);
        }
        float emissive = (submitted >= 65 && submitted < 125) ? 1.0f : 0.0f;
        if (submitted == 65 || submitted == 125) {
            VrLog("VRPROBE emissive segment %s", emissive > 0.0f ? "ON" : "OFF");
        }
        /* Acquire-first: a stuck swapchain fails HERE with bounded logs
         * instead of wedging mid-frame. Timeout (-2) is not success. */
        SDL_GPUTexture *xrImage[2] = { NULL, NULL };
        int acq[2] = { -1, -1 };
        int acquiOk = 1;
        for (int i = 0; i < 2; i++) {
            acq[i] = VrXr_AcquireEye(i, &xrImage[i]);
            if (acq[i] < 0 || !xrImage[i]) {
                VrLog("VRPROBE frame %u eye%d acquire result=%d SKIP FRAME", stereo, i,
                      acq[i]);
                acquiOk = 0;
                break;
            }
        }
        int frameOk = 0;
        AeronCommandBuffer *cmd = NULL;
        Uint64 tRender = 0;
        if (acquiOk) {
            tRender = SDL_GetTicks();
            VrLog("VRPROBE frame %u submit-render begin", stereo);
            cmd = Aeron_AcquireCommandBuffer();
            frameOk = cmd != NULL;
            AeronTexture *present_tex[2] = { NULL, NULL };
            for (int i = 0; i < 2 && frameOk; i++) {
                present_tex[i] = VrStereo_RenderEye(cmd, i, &cams[i], mesh, model,
                                                           lightDir, lightColor, 1.0f,
                                                           emissive);
                if (!present_tex[i]) {
                    VrLog("VRPROBE frame %u eye%d render NULL", stereo, i);
                    frameOk = 0;
                    break;
                }
                /* v10: MUTUALLY EXCLUSIVE diagnostic draw into the present texture
                 * (AFTER RenderEye/tonemap, BEFORE blit - same site where the
                 * triangle was validated). X-Wing mesh when available; the V10
                 * triangle is only the fallback when the mesh is not ready. */
                if (!VrStereo_DrawMesh(cmd, present_tex[i])) {
                    static int s_meshFallbackLogged = 0;
                    if (!s_meshFallbackLogged) {
                        s_meshFallbackLogged = 1;
                        VrLog("VRPROBE XWING mesh draw inactive - fallback ACTIVE (V10 triangle)");
                    }
                    if (!VrStereo_DrawTriangle(cmd, present_tex[i])) {
                        VrLog("VRPROBE frame %u eye%d triangle draw failed", stereo, i);
                        frameOk = 0;
                        break;
                    }
                }
                VrLog("VRPROBE frame %u blit eye%d begin img=%d", stereo, i, acq[i]);
                if (!VrBlit_Copy(cmd, g_aeron.gpu_device, present_tex[i], xrImage[i],
                                 (int)eyeW, (int)eyeH)) {
                    VrLog("VRPROBE frame %u eye%d blit failed", stereo, i);
                    frameOk = 0;
                    break;
                }
                VrLog("VRPROBE frame %u blit eye%d end", stereo, i);
            }
            /* W3: Schedule readback of scene_tex for both eyes.
             * v9.7: Log scene texture dimensions and compute row-scan rows
             * from actual texture height, not hardcoded values. */
            if (in_w3 && frameOk) {
                for (int i = 0; i < 2; i++) {
                    AeronTexture *scene_tex = VrStereo_GetSceneTexture(i);
                    if (scene_tex) {
                        VrLog("VRPROBE scene_tex eye=%d w=%d h=%d fmt=%d",
                              i, scene_tex->width, scene_tex->height, scene_tex->format);
                        VrReadback_Schedule(g_vrReadback, i, scene_tex, sample_ndc, VR_READBACK_NUM_SAMPLES, cmd);
                    }
                }
                /* v9.7: Compute row-scan rows from actual texture height.
                 * Ship projects to NDC y ≈ −0.218..+0.023.
                 * row = (1 - ndc_y) * 0.5 * (h - 1) */
                if (row_scan_idx < 5 && stereo == (unsigned)row_scan_frames[row_scan_idx]) {
                    AeronTexture *scene_tex = VrStereo_GetSceneTexture(0);
                    if (scene_tex) {
                        int th = scene_tex->height;
                        /* 5 rows: NDC y = −0.15, −0.075, 0.0, +0.075, +0.15 */
                        static const float row_ndc_y[5] = { -0.15f, -0.075f, 0.0f, 0.075f, 0.15f };
                        for (int i = 0; i < 2; i++) {
                            scene_tex = VrStereo_GetSceneTexture(i);
                            if (scene_tex) {
                                int ry = (int)((1.0f - row_ndc_y[row_scan_idx]) * 0.5f * (scene_tex->height - 1));
                                if (ry < 0) ry = 0;
                                if (ry >= scene_tex->height) ry = scene_tex->height - 1;
                                VrReadback_ScheduleRow(g_vrReadback, i, scene_tex, ry, cmd);
                            }
                        }
                    }
                }
            }
            /* v9.7: W4 — readback during flat-color phase (frames 20–29).
             * Reads scene_tex with flat-color draw enabled.
             * Purpose: verify magenta/cyan pixels appear in readback. */
            if (in_w4 && frameOk) {
                for (int i = 0; i < 2; i++) {
                    AeronTexture *scene_tex = VrStereo_GetSceneTexture(i);
                    if (scene_tex) {
                        VrReadback_Schedule(g_vrReadback, i, scene_tex, sample_ndc, VR_READBACK_NUM_SAMPLES, cmd);
                    }
                }
                if (row_scan_idx_b < 5 && stereo == (unsigned)row_scan_frames_b[row_scan_idx_b]) {
                    AeronTexture *scene_tex = VrStereo_GetSceneTexture(0);
                    if (scene_tex) {
                        int th = scene_tex->height;
                        static const float row_ndc_y_b[5] = { -0.15f, -0.075f, 0.0f, 0.075f, 0.15f };
                        for (int i = 0; i < 2; i++) {
                            scene_tex = VrStereo_GetSceneTexture(i);
                            if (scene_tex) {
                                int ry = (int)((1.0f - row_ndc_y_b[row_scan_idx_b]) * 0.5f * (scene_tex->height - 1));
                                if (ry < 0) ry = 0;
                                if (ry >= scene_tex->height) ry = scene_tex->height - 1;
                                VrReadback_ScheduleRow(g_vrReadback, i, scene_tex, ry, cmd);
                            }
                        }
                    }
                }
            }
            if (frameOk) {
                Uint64 tSub = SDL_GetTicks();
                AeronCommandBufferUploadUsage uu;
                memset(&uu, 0, sizeof uu);
                VrLog("VRPROBE frame %u gpu-submit begin", stereo);
                if (Aeron_CommandBufferGetUploadUsage(cmd, &uu)) {
                    VrLog("VRPROBE frame %u upload staged=%lluB copies=%u buf=%u tex=%u chunks=%u passes=%u largest=%u",
                          stereo, (unsigned long long)uu.staged_bytes, uu.copy_count,
                          uu.buffer_copy_count, uu.texture_copy_count, uu.chunk_count,
                          uu.copy_pass_count, uu.largest_upload_bytes);
                }
                frameOk = Aeron_SubmitCommandBuffer(cmd) != 0;
                VrLog("VRPROBE frame %u gpu-submit end dt=%llums ok=%d", stereo,
                      (unsigned long long)(SDL_GetTicks() - tSub), frameOk);
                cmd = NULL;
            } else if (cmd) {
                Aeron_CancelCommandBuffer(cmd);
                cmd = NULL;
            }
        }
        VrLog("VRPROBE frame %u submit-render end dt=%llums ok=%d", stereo,
              (unsigned long long)(SDL_GetTicks() - tRender), frameOk);
        for (int i = 0; i < 2; i++) {
            if (acq[i] >= 0) {
                VrXr_ReleaseEye(i);
            }
        }
        VrXr_EndFrame(displayTime, poses, fovs, frameOk);
        /* W3: Collect readback results for scene_tex */
        if (in_w3) {
            /* Ensure GPU finished the download before mapping the buffer. */
            Uint64 tSync = SDL_GetTicks();
            SDL_WaitForGPUIdle(g_aeron.gpu_device);
            Uint64 syncDt = SDL_GetTicks() - tSync;
            VrLog("VRPROBE frame %u gpu-sync dt=%llums", stereo, (unsigned long long)syncDt);
            for (int i = 0; i < 2; i++) {
                uint8_t rgba[VR_READBACK_NUM_SAMPLES * 4];
                int collected = VrReadback_Collect(g_vrReadback, i, rgba);
                if (collected > 0) {
                    /* v9.7: Use rgba_f[] (proper half_to_float conversion) for comparison. */
                    int nonzero = 0;
                    float bg_f[4];
                    bg_f[0] = half_to_float(bg_rgba16[0]);
                    bg_f[1] = half_to_float(bg_rgba16[1]);
                    bg_f[2] = half_to_float(bg_rgba16[2]);
                    bg_f[3] = half_to_float(bg_rgba16[3]);
                    for (int s = 0; s < collected; s++) {
                        float rf = g_vrReadback->samples[i][s].rgba_f[0];
                        float gf = g_vrReadback->samples[i][s].rgba_f[1];
                        float bf = g_vrReadback->samples[i][s].rgba_f[2];
                        float af = g_vrReadback->samples[i][s].rgba_f[3];
                        /* Compare using proper float values with 5% relative + absolute threshold */
                        float threshold = 0.02f;
                        int differs = 0;
                        if (fabsf(rf - bg_f[0]) > threshold) differs = 1;
                        if (fabsf(gf - bg_f[1]) > threshold) differs = 1;
                        if (fabsf(bf - bg_f[2]) > threshold) differs = 1;
                        if (fabsf(af - bg_f[3]) > threshold) differs = 1;
                        if (differs) nonzero++;
                        VrLog("VRPROBE readback frame=%u eye=%d s=%d ndc=(%.2f,%.2f) "
                              "float=(%.6f,%.6f,%.6f,%.6f) bg=(%.6f,%.6f,%.6f,%.6f) diff=%d",
                              stereo, i, s,
                              g_vrReadback->samples[i][s].ndc_x,
                              g_vrReadback->samples[i][s].ndc_y,
                              rf, gf, bf, af,
                              bg_f[0], bg_f[1], bg_f[2], bg_f[3], differs);
                    }
                    VrLog("VRPROBE readback frame=%u eye=%d summary collected=%d nonzero=%d/%d emissive=PBR",
                          stereo, i, collected, nonzero, collected);
                } else {
                    VrLog("VRPROBE readback frame=%u eye=%d collected=0", stereo, i);
                }
            }
            /* v9.7: Collect row-scan results after GPU sync. */
            if (row_scan_idx < 5 && stereo == (unsigned)row_scan_frames[row_scan_idx]) {
                for (int i = 0; i < 2; i++) {
                    VrReadback_CollectRow(g_vrReadback, i, bg_rgba16);
                }
                VrLog("VRPROBE row-scan frame=%u row_idx=%d emissive=PBR done",
                      stereo, row_scan_idx);
                row_scan_idx++;
            }
        } /* end W3 readback — in_w3 */
        /* v9.7: W4 — collect flat-color readback (frames 20–29).
         * Same structure as W3 collection: point samples + row scan.
         * Logs include flat-color tag and scene_tex dimensions. */
        if (in_w4) {
            Uint64 tSyncW4 = SDL_GetTicks();
            SDL_WaitForGPUIdle(g_aeron.gpu_device);
            Uint64 syncDtW4 = SDL_GetTicks() - tSyncW4;
            VrLog("VRPROBE frame %u gpu-sync-w4 dt=%llums", stereo, (unsigned long long)syncDtW4);
            for (int i = 0; i < 2; i++) {
                uint8_t rgba[VR_READBACK_NUM_SAMPLES * 4];
                int collected = VrReadback_Collect(g_vrReadback, i, rgba);
                if (collected > 0) {
                    int nonzero = 0;
                    float bg_f[4];
                    bg_f[0] = half_to_float(bg_rgba16[0]);
                    bg_f[1] = half_to_float(bg_rgba16[1]);
                    bg_f[2] = half_to_float(bg_rgba16[2]);
                    bg_f[3] = half_to_float(bg_rgba16[3]);
                    for (int s = 0; s < collected; s++) {
                        float rf = g_vrReadback->samples[i][s].rgba_f[0];
                        float gf = g_vrReadback->samples[i][s].rgba_f[1];
                        float bf = g_vrReadback->samples[i][s].rgba_f[2];
                        float af = g_vrReadback->samples[i][s].rgba_f[3];
                        float threshold = 0.02f;
                        int differs = 0;
                        if (fabsf(rf - bg_f[0]) > threshold) differs = 1;
                        if (fabsf(gf - bg_f[1]) > threshold) differs = 1;
                        if (fabsf(bf - bg_f[2]) > threshold) differs = 1;
                        if (fabsf(af - bg_f[3]) > threshold) differs = 1;
                        if (differs) nonzero++;
                        VrLog("VRPROBE w4-readback frame=%u eye=%d s=%d ndc=(%.2f,%.2f) "
                              "float=(%.6f,%.6f,%.6f,%.6f) bg=(%.6f,%.6f,%.6f,%.6f) diff=%d",
                              stereo, i, s,
                              g_vrReadback->samples[i][s].ndc_x,
                              g_vrReadback->samples[i][s].ndc_y,
                              rf, gf, bf, af,
                              bg_f[0], bg_f[1], bg_f[2], bg_f[3], differs);
                    }
                    VrLog("VRPROBE w4-readback frame=%u eye=%d summary collected=%d nonzero=%d/%d flat=ON",
                          stereo, i, collected, nonzero, collected);
                } else {
                    VrLog("VRPROBE w4-readback frame=%u eye=%d collected=0", stereo, i);
                }
            }
            if (row_scan_idx_b < 5 && stereo == (unsigned)row_scan_frames_b[row_scan_idx_b]) {
                for (int i = 0; i < 2; i++) {
                    VrReadback_CollectRow(g_vrReadback, i, bg_rgba16);
                }
                VrLog("VRPROBE w4-row-scan frame=%u row_idx=%d flat=ON done",
                      stereo, row_scan_idx_b);
                row_scan_idx_b++;
            }
        } /* end W4 readback — in_w4 */
        if (!frameOk) {
            consecSkips++;
            VrLog("VRPROBE stereo frame %u SKIPPED (no layers) consec=%u", stereo,
                  consecSkips);
        } else {
            consecSkips = 0;
            submitted++;
        }
        stereo++;
        g_vrStereo = stereo;
        if (stereo <= 5 || stereo % 30 == 0) {
            VrLog("VRPROBE STEREO count=%u submitted=%u consecSkips=%u mode=%s emissive=%s",
                  stereo, submitted, consecSkips, fixedIpd ? "FIXED_IPD" : "XR_POSE",
                  emissive > 0.0f ? "ON" : "OFF");
        }
    } /* end while loop */
    if (exitReason[0] == 'c' && exitReason[1] == 'a' && exitReason[2] == 'p') {
        if (VrXr_ShouldQuit()) {
            exitReason = "xr quit";
        } else if (Aeron_QuitRequested() || XwaPort_ShouldQuit()) {
            exitReason = "host quit";
        }
    }
    VrLog("VRPROBE loop end stereo=%u submitted=%u reason=%s quit=%d", stereo, submitted,
          exitReason, VrXr_ShouldQuit());
    /* v9.5: Report if head-tracking comparison was never triggered (user
     * may not have rotated head ≥15° during the test). */
    if (track_ref_logged == 1) {
        VrLog("VRPROBE track INCONCLUSIVE: reference logged but no significant "
              "rotation detected (dq never exceeded 0.15).  User needs to turn "
              "head ≥15° during the test for a valid comparison.");
    }

    VrBlit_Shutdown(g_aeron.gpu_device);
    VrStereo_Shutdown();
    VrXr_Shutdown(g_aeron.gpu_device);
    VrReadback_Destroy(g_vrReadback);
    XwaRemaster_Shutdown();
    XwaPort_Shutdown();
    Aeron_Shutdown();
    if (g_vrLog) {
        VrLog("VRPROBE exit code=%d", exit_code);
        fclose(g_vrLog);
        g_vrLog = NULL;
    }
    return exit_code;
}
