/* Observational linker wrappers: always call the real M2 implementation and
 * return its actual result. No substitute implementation or success override. */
#include "internal.h"
#include "host_config.h"
#include "setup.h"

bool __real_SDL_Init(SDL_InitFlags flags);
bool __wrap_SDL_Init(SDL_InitFlags flags) {
    SDL_Log("M3 SDL_Init BEGIN flags=0x%x", flags);
    bool ok = __real_SDL_Init(flags);
    SDL_Log("M3 SDL_Init END ok=%d video=%s audio=%s error=%s", ok,
        SDL_GetCurrentVideoDriver(), SDL_GetCurrentAudioDriver(), ok ? "none" : SDL_GetError());
    return ok;
}
#define TRACE_INT0(name) \
    int __real_##name(void); \
    int __wrap_##name(void) { \
        SDL_Log("M3 " #name " BEGIN"); \
        int ok = __real_##name(); \
        SDL_Log("M3 " #name " END ok=%d error=%s", ok, ok ? "none" : SDL_GetError()); \
        return ok; \
    }
#define TRACE_VOID0(name) \
    void __real_##name(void); \
    void __wrap_##name(void) { \
        SDL_Log("M3 " #name " BEGIN"); \
        __real_##name(); \
        SDL_Log("M3 " #name " END"); \
    }
TRACE_INT0(Aeron_AudioInit)
TRACE_VOID0(Aeron_ControllersInit)
TRACE_VOID0(Aeron_DebugUiInitInternal)
TRACE_VOID0(Aeron_Shutdown)
int __real_Aeron_Init(const AeronConfig* config);
int __wrap_Aeron_Init(const AeronConfig* config) {
    SDL_Log("M3 Aeron_Init BEGIN");
    int ok = __real_Aeron_Init(config);
    SDL_Log("M3 Aeron_Init END ok=%d base=%s resource=%s user=%s", ok,
        SDL_GetBasePath(), Aeron_ResourceRoot(), Aeron_UserPath());
    return ok;
}
int __real_Aeron_WindowInit(const AeronConfig* config);
int __wrap_Aeron_WindowInit(const AeronConfig* config) {
    SDL_Log("M3 Aeron_WindowInit BEGIN");
    int ok = __real_Aeron_WindowInit(config);
    SDL_Log("M3 Aeron_WindowInit END ok=%d", ok);
    return ok;
}
int __real_Aeron_RenderBackendInit(void);
int __wrap_Aeron_RenderBackendInit(void) {
    SDL_Log("M3 Aeron_RenderBackendInit BEGIN shaders=%s", g_aeron.shader_root);
    int ok = __real_Aeron_RenderBackendInit();
    SDL_Log("M3 Aeron_RenderBackendInit END ok=%d driver=%s", ok,
        g_aeron.gpu_device ? SDL_GetGPUDeviceDriver(g_aeron.gpu_device) : "none");
    return ok;
}
void __real_Aeron_InitVfs(const AeronConfig* config);
void __wrap_Aeron_InitVfs(const AeronConfig* config) {
    SDL_Log("M3 Aeron_InitVfs BEGIN");
    __real_Aeron_InitVfs(config);
    SDL_Log("M3 Aeron_InitVfs END resource=%s user=%s", Aeron_ResourceRoot(), Aeron_UserPath());
}
int __real_XwaHostConfig_Load(AeronVfs*, XwaHostConfig*, char*, size_t);
int __wrap_XwaHostConfig_Load(AeronVfs* vfs, XwaHostConfig* config, char* error, size_t capacity) {
    SDL_Log("M3 XwaHostConfig_Load BEGIN");
    int ok = __real_XwaHostConfig_Load(vfs, config, error, capacity);
    SDL_Log("M3 XwaHostConfig_Load END ok=%d error=%s", ok, ok ? "none" : error);
    return ok;
}
int __real_XwaSetup_ValidateGameData(AeronVfs*, const char*, char*, size_t, char*, size_t);
int __wrap_XwaSetup_ValidateGameData(AeronVfs* vfs, const char* path, char* normalized,
    size_t normalized_capacity, char* error, size_t error_capacity) {
    SDL_Log("M3 GameData validation BEGIN candidate=%s", path);
    int ok = __real_XwaSetup_ValidateGameData(vfs, path, normalized, normalized_capacity, error, error_capacity);
    SDL_Log("M3 GameData validation END ok=%d detail=%s", ok, ok ? normalized : error);
    return ok;
}
