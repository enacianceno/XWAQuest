/* Observational linker wrappers: always call the real M2 implementation and
 * return its actual result. No substitute implementation or success override. */
#include "internal.h"
#include "host_config.h"
#include "setup.h"

bool __real_SDL_Init(SDL_InitFlags flags);
bool __wrap_SDL_Init(SDL_InitFlags flags) {
    SDL_Log("M6 SDL_Init BEGIN flags=0x%x", flags);
    bool ok = __real_SDL_Init(flags);
    SDL_Log("M6 SDL_Init END ok=%d video=%s audio=%s error=%s", ok,
        SDL_GetCurrentVideoDriver(), SDL_GetCurrentAudioDriver(), ok ? "none" : SDL_GetError());
    return ok;
}
#define TRACE_INT0(name) \
    int __real_##name(void); \
    int __wrap_##name(void) { \
        SDL_Log("M6 " #name " BEGIN"); \
        int ok = __real_##name(); \
        SDL_Log("M6 " #name " END ok=%d error=%s", ok, ok ? "none" : SDL_GetError()); \
        return ok; \
    }
#define TRACE_VOID0(name) \
    void __real_##name(void); \
    void __wrap_##name(void) { \
        SDL_Log("M6 " #name " BEGIN"); \
        __real_##name(); \
        SDL_Log("M6 " #name " END"); \
    }
TRACE_INT0(Aeron_AudioInit)
TRACE_VOID0(Aeron_ControllersInit)
TRACE_VOID0(Aeron_DebugUiInitInternal)
TRACE_VOID0(Aeron_Shutdown)
int __real_Aeron_Init(const AeronConfig* config);
int __wrap_Aeron_Init(const AeronConfig* config) {
    SDL_SetHint(SDL_HINT_ENABLE_SCREEN_KEYBOARD, "0");
    SDL_Log("M6 Aeron_Init BEGIN; Android startup keyboard disabled");
    int ok = __real_Aeron_Init(config);
    SDL_Log("M6 Aeron_Init END ok=%d base=%s resource=%s user=%s", ok,
        SDL_GetBasePath(), Aeron_ResourceRoot(), Aeron_UserPath());
    return ok;
}
int __real_Aeron_WindowInit(const AeronConfig* config);
int __wrap_Aeron_WindowInit(const AeronConfig* config) {
    SDL_Log("M6 Aeron_WindowInit BEGIN");
    int ok = __real_Aeron_WindowInit(config);
    SDL_Log("M6 Aeron_WindowInit END ok=%d", ok);
    return ok;
}
int __real_Aeron_RenderBackendInit(void);
int __wrap_Aeron_RenderBackendInit(void) {
    SDL_Log("M6 Aeron_RenderBackendInit BEGIN shaders=%s", g_aeron.shader_root);
    int ok = __real_Aeron_RenderBackendInit();
    SDL_Log("M6 Aeron_RenderBackendInit END ok=%d driver=%s", ok,
        g_aeron.gpu_device ? SDL_GetGPUDeviceDriver(g_aeron.gpu_device) : "none");
    return ok;
}
void __real_Aeron_InitVfs(const AeronConfig* config);
void __wrap_Aeron_InitVfs(const AeronConfig* config) {
    SDL_Log("M6 Aeron_InitVfs BEGIN");
    __real_Aeron_InitVfs(config);
    SDL_Log("M6 Aeron_InitVfs END resource=%s user=%s", Aeron_ResourceRoot(), Aeron_UserPath());
}
int __real_XwaHostConfig_Load(AeronVfs*, XwaHostConfig*, char*, size_t);
int __wrap_XwaHostConfig_Load(AeronVfs* vfs, XwaHostConfig* config, char* error, size_t capacity) {
    SDL_Log("M6 XwaHostConfig_Load BEGIN");
    int ok = __real_XwaHostConfig_Load(vfs, config, error, capacity);
    SDL_Log("M6 XwaHostConfig_Load END ok=%d error=%s", ok, ok ? "none" : error);
    return ok;
}
int __real_XwaSetup_ValidateGameData(AeronVfs*, const char*, char*, size_t, char*, size_t);
int __wrap_XwaSetup_ValidateGameData(AeronVfs* vfs, const char* path, char* normalized,
    size_t normalized_capacity, char* error, size_t error_capacity) {
    SDL_Log("M6 GameData validation BEGIN candidate=%s", path);
    int ok = __real_XwaSetup_ValidateGameData(vfs, path, normalized, normalized_capacity, error, error_capacity);
    SDL_Log("M6 GameData validation END ok=%d detail=%s", ok, ok ? normalized : error);
    return ok;
}


#include "xwa_remaster/xwa_remaster.h"
#include "xwa_runtime/runtime/port.h"
TRACE_INT0(XwaPort_Init)
int __real_XwaRemaster_Init(const XwaRemasterInitOptions* options);
int __wrap_XwaRemaster_Init(const XwaRemasterInitOptions* options) {
    SDL_Log("M6 XwaRemaster_Init BEGIN asset=%s resource=%s", Aeron_AssetRoot(), Aeron_ResourceRoot());
    int ok = __real_XwaRemaster_Init(options);
    SDL_Log("M6 XwaRemaster_Init END ok=%d", ok);
    return ok;
}
int __real_AeronVfs_Exists(AeronVfs*, AeronVfsRoot, const char*);
int __wrap_AeronVfs_Exists(AeronVfs* vfs, AeronVfsRoot root, const char* path) {
    int exists = __real_AeronVfs_Exists(vfs, root, path);
    SDL_Log("M6 VFS exists root=%d path=%s found=%d", root, path, exists);
    return exists;
}
int __real_AeronVfs_Open(AeronVfs*, AeronVfsRoot, const char*, AeronVfsOpenMode, AeronFile**);
int __wrap_AeronVfs_Open(AeronVfs* vfs, AeronVfsRoot root, const char* path,
    AeronVfsOpenMode mode, AeronFile** out_file) {
    int ok = __real_AeronVfs_Open(vfs, root, path, mode, out_file);
    SDL_Log("M6 VFS open root=%d path=%s mode=%d ok=%d error=%s", root, path, mode,
        ok, ok ? "none" : SDL_GetError());
    return ok;
}


#include "xwa_runtime/runtime/movie_task.h"
#include "xwa_runtime/runtime/flight_task.h"
static unsigned m6_ticks, m6_frames, m6_presents;
void __real_XwaPort_Tick(int32_t delta_us);
void __wrap_XwaPort_Tick(int32_t delta_us) {
    unsigned tick = ++m6_ticks;
    if(tick <= 3 || tick % 120 == 0) SDL_Log("M6 TICK begin=%u delta=%d movie=%d flight=%d", tick, delta_us, XwaMovieTask_IsActive(), XwaFlightTask_IsActive());
    __real_XwaPort_Tick(delta_us);
    if(tick <= 3 || tick % 120 == 0) SDL_Log("M6 TICK end=%u movie=%d quit=%d", tick, XwaMovieTask_IsActive(), XwaPort_ShouldQuit());
}
void __real_XwaRemaster_Frame(int32_t delta_us);
void __wrap_XwaRemaster_Frame(int32_t delta_us) {
    __real_XwaRemaster_Frame(delta_us);
    if(++m6_frames <= 3 || m6_frames % 120 == 0) SDL_Log("M6 REMASTER frame=%u", m6_frames);
}
int __real_Aeron_Present(void);
int __wrap_Aeron_Present(void) {
    int ok=__real_Aeron_Present();
    if(++m6_presents <= 3 || m6_presents % 120 == 0 || !ok) SDL_Log("M6 PRESENT count=%u ok=%d error=%s", m6_presents, ok, ok ? "none" : Aeron_RenderLastError());
    return ok;
}
int __real_Movie_Play(const char* name, int noFade);
int __wrap_Movie_Play(const char* name, int noFade) {
    SDL_Log("M6 MOVIE begin=%s", name);
    int ok=__real_Movie_Play(name, noFade);
    SDL_Log("M6 MOVIE result=%d active=%d", ok, XwaMovieTask_IsActive());
    return ok;
}

#include "aeron/video.h"
void __real_Aeron_VideoUpdate(AeronVideoPlayer* player);
void __wrap_Aeron_VideoUpdate(AeronVideoPlayer* player) {
    static unsigned updates;
    __real_Aeron_VideoUpdate(player);
    if(++updates <= 3 || updates % 120 == 0) {
        AeronVideoStats s;
        if(Aeron_VideoGetStats(player,&s)) SDL_Log("M6 VIDEO state=%d decoded=%llu presented=%llu pos=%lld duration=%lld focus=%d", Aeron_VideoGetState(player), (unsigned long long)s.video_frames_decoded, (unsigned long long)s.video_frames_presented, (long long)s.position_us, (long long)s.duration_us, Aeron_InputSnapshot()->has_focus);
    }
}

