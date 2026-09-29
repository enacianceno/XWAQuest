/* M7A observational linker wrappers: always call the real M2 implementation and
 * return its actual result. No substitute implementation or success override.
 *
 * M7A adds flight-phase instrumentation to determine how far the engine
 * gets on Android ARM64 when attempting to launch a real mission. */
#include "internal.h"
#include "host_config.h"
#include "setup.h"
#include <jni.h>

bool __real_SDL_Init(SDL_InitFlags flags);
bool __wrap_SDL_Init(SDL_InitFlags flags) {
    SDL_Log("M7 SDL_Init BEGIN flags=0x%x", flags);
    bool ok = __real_SDL_Init(flags);
    SDL_Log("M7 SDL_Init END ok=%d video=%s audio=%s error=%s", ok,
        SDL_GetCurrentVideoDriver(), SDL_GetCurrentAudioDriver(), ok ? "none" : SDL_GetError());
    return ok;
}
#define TRACE_INT0(name) \
    int __real_##name(void); \
    int __wrap_##name(void) { \
        SDL_Log("M7 " #name " BEGIN"); \
        int ok = __real_##name(); \
        SDL_Log("M7 " #name " END ok=%d error=%s", ok, ok ? "none" : SDL_GetError()); \
        return ok; \
    }
#define TRACE_VOID0(name) \
    void __real_##name(void); \
    void __wrap_##name(void) { \
        SDL_Log("M7 " #name " BEGIN"); \
        __real_##name(); \
        SDL_Log("M7 " #name " END"); \
    }
TRACE_INT0(Aeron_AudioInit)
TRACE_VOID0(Aeron_ControllersInit)
TRACE_VOID0(Aeron_DebugUiInitInternal)
TRACE_VOID0(Aeron_Shutdown)
int __real_Aeron_Init(const AeronConfig* config);
int __wrap_Aeron_Init(const AeronConfig* config) {
    SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0");
    SDL_Log("M7 Aeron_Init BEGIN; Android startup keyboard disabled");
    int ok = __real_Aeron_Init(config);
    SDL_Log("M7 Aeron_Init END ok=%d base=%s resource=%s user=%s", ok,
        SDL_GetBasePath(), Aeron_ResourceRoot(), Aeron_UserPath());
    return ok;
}
int __real_Aeron_WindowInit(const AeronConfig* config);
int __wrap_Aeron_WindowInit(const AeronConfig* config) {
    SDL_Log("M7 Aeron_WindowInit BEGIN");
    int ok = __real_Aeron_WindowInit(config);
    SDL_Log("M7 Aeron_WindowInit END ok=%d", ok);
    return ok;
}
int __real_Aeron_RenderBackendInit(void);
int __wrap_Aeron_RenderBackendInit(void) {
    SDL_Log("M7 Aeron_RenderBackendInit BEGIN shaders=%s", g_aeron.shader_root);
    int ok = __real_Aeron_RenderBackendInit();
    SDL_Log("M7 Aeron_RenderBackendInit END ok=%d driver=%s", ok,
        g_aeron.gpu_device ? SDL_GetGPUDeviceDriver(g_aeron.gpu_device) : "none");
    return ok;
}
void __real_Aeron_InitVfs(const AeronConfig* config);
void __wrap_Aeron_InitVfs(const AeronConfig* config) {
    SDL_Log("M7 Aeron_InitVfs BEGIN");
    __real_Aeron_InitVfs(config);
    SDL_Log("M7 Aeron_InitVfs END resource=%s user=%s", Aeron_ResourceRoot(), Aeron_UserPath());
}
int __real_XwaHostConfig_Load(AeronVfs*, XwaHostConfig*, char*, size_t);
int __wrap_XwaHostConfig_Load(AeronVfs* vfs, XwaHostConfig* config, char* error, size_t capacity) {
    SDL_Log("M7 XwaHostConfig_Load BEGIN");
    int ok = __real_XwaHostConfig_Load(vfs, config, error, capacity);
    SDL_Log("M7 XwaHostConfig_Load END ok=%d error=%s", ok, ok ? "none" : error);
    return ok;
}
int __real_XwaSetup_ValidateGameData(AeronVfs*, const char*, char*, size_t, char*, size_t);
int __wrap_XwaSetup_ValidateGameData(AeronVfs* vfs, const char* path, char* normalized,
    size_t normalized_capacity, char* error, size_t error_capacity) {
    SDL_Log("M7 GameData validation BEGIN candidate=%s", path);
    int ok = __real_XwaSetup_ValidateGameData(vfs, path, normalized, normalized_capacity, error, error_capacity);
    SDL_Log("M7 GameData validation END ok=%d detail=%s", ok, ok ? normalized : error);
    return ok;
}


#include "xwa_remaster/xwa_remaster.h"
#include "xwa_runtime/runtime/port.h"
TRACE_INT0(XwaPort_Init)
int __real_XwaRemaster_Init(const XwaRemasterInitOptions* options);
int __wrap_XwaRemaster_Init(const XwaRemasterInitOptions* options) {
    SDL_Log("M7 XwaRemaster_Init BEGIN asset=%s resource=%s", Aeron_AssetRoot(), Aeron_ResourceRoot());
    int ok = __real_XwaRemaster_Init(options);
    SDL_Log("M7 XwaRemaster_Init END ok=%d", ok);
    return ok;
}
int __real_AeronVfs_Exists(AeronVfs*, AeronVfsRoot, const char*);
int __wrap_AeronVfs_Exists(AeronVfs* vfs, AeronVfsRoot root, const char* path) {
    int exists = __real_AeronVfs_Exists(vfs, root, path);
    SDL_Log("M7 VFS exists root=%d path=%s found=%d", root, path, exists);
    return exists;
}
int __real_AeronVfs_Open(AeronVfs*, AeronVfsRoot, const char*, AeronVfsOpenMode, AeronFile**);
int __wrap_AeronVfs_Open(AeronVfs* vfs, AeronVfsRoot root, const char* path,
    AeronVfsOpenMode mode, AeronFile** out_file) {
    int ok = __real_AeronVfs_Open(vfs, root, path, mode, out_file);
    SDL_Log("M7 VFS open root=%d path=%s mode=%d ok=%d error=%s", root, path, mode,
        ok, ok ? "none" : SDL_GetError());
    return ok;
}


#include "xwa_runtime/runtime/movie_task.h"
#include "xwa_runtime/runtime/flight_task.h"
#include "xwa/frontend/frontend_flight.h"
#include "xwa/flight/flight_display.h"
#include "xwa/flight/mission/mission.h"

static unsigned m7_ticks, m7_frames, m7_presents;
void __real_XwaPort_Tick(int32_t delta_us);
void __wrap_XwaPort_Tick(int32_t delta_us) {
    unsigned tick = ++m7_ticks;
    if(tick <= 3 || tick % 120 == 0) SDL_Log("M7 TICK begin=%u delta=%d movie=%d flight=%d", tick, delta_us, XwaMovieTask_IsActive(), XwaFlightTask_IsActive());
    __real_XwaPort_Tick(delta_us);
    if(tick <= 3 || tick % 120 == 0) SDL_Log("M7 TICK end=%u movie=%d quit=%d", tick, XwaMovieTask_IsActive(), XwaPort_ShouldQuit());
}
void __real_XwaRemaster_Frame(int32_t delta_us);
void __wrap_XwaRemaster_Frame(int32_t delta_us) {
    __real_XwaRemaster_Frame(delta_us);
    if(++m7_frames <= 3 || m7_frames % 120 == 0) SDL_Log("M7 REMASTER frame=%u", m7_frames);
}
int __real_Aeron_Present(void);
int __wrap_Aeron_Present(void) {
    int ok=__real_Aeron_Present();
    if(++m7_presents <= 3 || m7_presents % 120 == 0 || !ok) SDL_Log("M7 PRESENT count=%u ok=%d error=%s", m7_presents, ok, ok ? "none" : Aeron_RenderLastError());
    return ok;
}
int __real_Movie_Play(const char* name, int noFade);
int __wrap_Movie_Play(const char* name, int noFade) {
    SDL_Log("M7 MOVIE begin=%s", name);
    int ok=__real_Movie_Play(name, noFade);
    SDL_Log("M7 MOVIE result=%d active=%d", ok, XwaMovieTask_IsActive());
    return ok;
}

#include "aeron/video.h"
void __real_Aeron_VideoUpdate(AeronVideoPlayer* player);
void __wrap_Aeron_VideoUpdate(AeronVideoPlayer* player) {
    static unsigned updates;
    __real_Aeron_VideoUpdate(player);
    if(++updates <= 3 || updates % 120 == 0) {
        AeronVideoStats s;
        if(Aeron_VideoGetStats(player,&s)) SDL_Log("M7 VIDEO state=%d decoded=%llu presented=%llu pos=%lld duration=%lld focus=%d", Aeron_VideoGetState(player), (unsigned long long)s.video_frames_decoded, (unsigned long long)s.video_frames_presented, (long long)s.position_us, (long long)s.duration_us, Aeron_InputSnapshot()->has_focus);
    }
}

/* ========================================================================
 * M7B QUEST FLIGHT INPUT INJECTION
 * ======================================================================== */

/* Quest virtual axes (set from Java via JNI). */
static float s_questAxisX   = 0;  /* pitch: +1 = up,    -1 = down  */
static float s_questAxisY   = 0;  /* roll:  +1 = right, -1 = left  */
static float s_questAxisR   = 0;  /* yaw:   +1 = right, -1 = left  */
static float s_questThrottle = 0; /* throttle: +1 = faster, -1 = slower */

/* JNI entry points called from Java RuntimeActivity. */
JNIEXPORT void JNICALL
Java_org_openxwa_xwaquest_m7_RuntimeActivity_nativeSetQuestAxes(
    JNIEnv *env, jobject thiz,
    jfloat pitch, jfloat roll, jfloat yaw, jfloat throttle)
{
    s_questAxisX    = pitch;
    s_questAxisY    = roll;
    s_questAxisR    = yaw;
    s_questThrottle = throttle;
}

/* Forward-declare engine globals we write to. */
extern int16_t g_ctrlAxisX;
extern int16_t g_ctrlAxisY;
extern int16_t g_ctrlAxisR;

uint16_t __real_FlightInput_Read(int playerIdxOrSentinel);
uint16_t __wrap_FlightInput_Read(int playerIdxOrSentinel) {
    uint16_t key = __real_FlightInput_Read(playerIdxOrSentinel);

    /* Only override axes in flight mode, not hangar. */
    extern int g_inHangarReady;
    if (g_inHangarReady == 0) {
        g_ctrlAxisX += (int16_t)(s_questAxisX * 128.0f);
        g_ctrlAxisY += (int16_t)(s_questAxisY * 128.0f);
        g_ctrlAxisR += (int16_t)(s_questAxisR * 128.0f);

        /* Throttle: nudge g_throttleSmoothed directly.
         * Range is 0-255; joystick raw is -128..+127 mapped to 0..255. */
        if (s_questThrottle != 0.0f) {
            extern int g_throttleSmoothed;
            if (g_throttleSmoothed < 0) g_throttleSmoothed = 128;
            g_throttleSmoothed += (int)(s_questThrottle * 8.0f);
            if (g_throttleSmoothed < 0)   g_throttleSmoothed = 0;
            if (g_throttleSmoothed > 255) g_throttleSmoothed = 255;
        }

        static int m7b_axis_log = 0;
        if (++m7b_axis_log % 60 == 0) {
            extern int g_throttleSmoothed;
            SDL_Log("M7B FLIGHT_AXES X=%d Y=%d R=%d thr=%d key=0x%04x",
                g_ctrlAxisX, g_ctrlAxisY, g_ctrlAxisR,
                g_throttleSmoothed, key);
        }
    }

    return key;
}

/* ========================================================================
 * M7A FLIGHT-PHASE INSTRUMENTATION
 * ======================================================================== */

/* FrontendFlight_LaunchSession: called when "Fly" is pressed.
 * Signature: int FrontendFlight_LaunchSession(int frameCounter) */
int __real_FrontendFlight_LaunchSession(int frameCounter);
int __wrap_FrontendFlight_LaunchSession(int frameCounter) {
    SDL_Log("M7A LAUNCH_SESSION frameCounter=%d", frameCounter);
    int ok = __real_FrontendFlight_LaunchSession(frameCounter);
    SDL_Log("M7A LAUNCH_SESSION result=%d pending=%d", ok, FrontendFlight_HasPendingLaunch());
    return ok;
}

/* FrontendFlight_BeginPendingLaunch: builds cmdLine and calls XwaFlightTask_Init.
 * Signature: int FrontendFlight_BeginPendingLaunch(void) */
int __real_FrontendFlight_BeginPendingLaunch(void);
int __wrap_FrontendFlight_BeginPendingLaunch(void) {
    SDL_Log("M7A BEGIN_PENDING_LAUNCH ENTER");
    int ok = __real_FrontendFlight_BeginPendingLaunch();
    SDL_Log("M7A BEGIN_PENDING_LAUNCH EXIT result=%d", ok);
    return ok;
}

/* XwaFlightTask_Init: parses command line, inits display/sound/network, sets phase.
 * Signature: int XwaFlightTask_Init(char* missionCmdLine, const char* filmFilePath) */
int __real_XwaFlightTask_Init(char* missionCmdLine, const char* filmFilePath);
int __wrap_XwaFlightTask_Init(char* missionCmdLine, const char* filmFilePath) {
    SDL_Log("M7A FLIGHT_INIT ENTER cmd=%s film=%s", missionCmdLine ? missionCmdLine : "NULL",
        filmFilePath && filmFilePath[0] ? filmFilePath : "none");
    int ok = __real_XwaFlightTask_Init(missionCmdLine, filmFilePath);
    SDL_Log("M7A FLIGHT_INIT EXIT result=%d", ok);
    return ok;
}

/* XwaFlightTask_Tick: the flight loop dispatcher.
 * Signature: void XwaFlightTask_Tick(void)
 * We log phase transitions only (not every tick). */
void __real_XwaFlightTask_Tick(void);
static int m7_last_phase = -1;
static unsigned m7_flight_tick_count;
void __wrap_XwaFlightTask_Tick(void) {
    __real_XwaFlightTask_Tick();
    ++m7_flight_tick_count;
    /* Log phase transitions and periodic progress. */
    if (XwaFlightTask_IsActive()) {
        /* Phase is internal; use IsComplete + IsActive to infer. */
        if (m7_flight_tick_count <= 5 || m7_flight_tick_count % 500 == 0) {
            SDL_Log("M7A FLIGHT_TICK count=%u active=%d complete=%d",
                m7_flight_tick_count, XwaFlightTask_IsActive(), XwaFlightTask_IsComplete());
        }
    } else if (m7_flight_tick_count <= 10 || m7_flight_tick_count % 100 == 0) {
        SDL_Log("M7A FLIGHT_TICK count=%u active=%d (not active)", m7_flight_tick_count, XwaFlightTask_IsActive());
    }
}

/* XwaFlightTask_Shutdown: cleanup after flight ends.
 * Signature: int XwaFlightTask_Shutdown(void) */
int __real_XwaFlightTask_Shutdown(void);
int __wrap_XwaFlightTask_Shutdown(void) {
    SDL_Log("M7A FLIGHT_SHUTDOWN ENTER");
    int result = __real_XwaFlightTask_Shutdown();
    SDL_Log("M7A FLIGHT_SHUTDOWN EXIT result=%d", result);
    return result;
}

/* FlightDisplay_Init: creates flight surfaces, sets up D3D for flight.
 * Signature: char FlightDisplay_Init(void) — returns char (int8_t) */
char __real_FlightDisplay_Init(void);
char __wrap_FlightDisplay_Init(void) {
    SDL_Log("M7A FLIGHT_DISPLAY_INIT ENTER");
    char ok = __real_FlightDisplay_Init();
    SDL_Log("M7A FLIGHT_DISPLAY_INIT EXIT ok=%d", (int)ok);
    return ok;
}

/* Mission_Init: loads .MIS file, creates flight groups, objects.
 * Signature: uint16_t Mission_Init(char* fileName) */
uint16_t __real_Mission_Init(char* fileName);
uint16_t __wrap_Mission_Init(char* fileName) {
    SDL_Log("M7A MISSION_INIT ENTER file=%s", fileName ? fileName : "NULL");
    uint16_t ok = __real_Mission_Init(fileName);
    SDL_Log("M7A MISSION_INIT EXIT result=%u", (unsigned)ok);
    return ok;
}
