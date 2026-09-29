/* VR-PROBE observational wrappers: call the real implementation, log, return.
 * No behavior change. Lets logcat prove the real init path ran. */
#include "internal.h"
#include "host_config.h"
#include "setup.h"
#include "xwa_remaster/xwa_remaster.h"

extern Uint64 Aeron_VrProbeXrInstance(void);
extern Uint64 Aeron_VrProbeXrSystemId(void);

bool __real_SDL_Init(SDL_InitFlags flags);
bool __wrap_SDL_Init(SDL_InitFlags flags) {
    bool ok = __real_SDL_Init(flags);
    SDL_Log("VRPROBE SDL_Init ok=%d err=%s", ok, ok ? "none" : SDL_GetError());
    return ok;
}
int __real_Aeron_RenderBackendInit(void);
int __wrap_Aeron_RenderBackendInit(void) {
    int ok = __real_Aeron_RenderBackendInit();
    SDL_Log("VRPROBE Aeron_RenderBackendInit ok=%d driver=%s xr_instance=%llu xr_system=%llu", ok,
            g_aeron.gpu_device ? SDL_GetGPUDeviceDriver(g_aeron.gpu_device) : "none",
            (unsigned long long)Aeron_VrProbeXrInstance(),
            (unsigned long long)Aeron_VrProbeXrSystemId());
    return ok;
}
int __real_XwaHostConfig_Load(AeronVfs *vfs, XwaHostConfig *config, char *error, size_t capacity);
int __wrap_XwaHostConfig_Load(AeronVfs *vfs, XwaHostConfig *config, char *error, size_t capacity) {
    int ok = __real_XwaHostConfig_Load(vfs, config, error, capacity);
    SDL_Log("VRPROBE XwaHostConfig_Load ok=%d", ok);
    return ok;
}
int __real_XwaSetup_ValidateGameData(AeronVfs *vfs, const char *path, char *normalized,
                                     size_t normalized_capacity, char *error, size_t error_capacity);
int __wrap_XwaSetup_ValidateGameData(AeronVfs *vfs, const char *path, char *normalized,
                                     size_t normalized_capacity, char *error, size_t error_capacity) {
    int ok = __real_XwaSetup_ValidateGameData(vfs, path, normalized, normalized_capacity, error,
                                              error_capacity);
    SDL_Log("VRPROBE GameData validation ok=%d detail=%s", ok, ok ? normalized : error);
    return ok;
}
int __real_XwaRemaster_Init(const XwaRemasterInitOptions *options);
int __wrap_XwaRemaster_Init(const XwaRemasterInitOptions *options) {
    int ok = __real_XwaRemaster_Init(options);
    SDL_Log("VRPROBE XwaRemaster_Init ok=%d", ok);
    return ok;
}
int __real_XwaPort_Init(void);
int __wrap_XwaPort_Init(void) {
    int ok = __real_XwaPort_Init();
    SDL_Log("VRPROBE XwaPort_Init ok=%d", ok);
    return ok;
}
int __real_Aeron_Present(void);
int __wrap_Aeron_Present(void) {
    int ok = __real_Aeron_Present();
    SDL_Log("VRPROBE Aeron_Present passthrough ok=%d", ok);
    return ok;
}
char __real_ModelPreview_LoadModel(const char *modelName, int objectType);
char __wrap_ModelPreview_LoadModel(const char *modelName, int objectType) {
    char ok = __real_ModelPreview_LoadModel(modelName, objectType);
    SDL_Log("VRPROBE ModelPreview_LoadModel name=%s type=%d ok=%d", modelName, objectType, (int)ok);
    return ok;
}
