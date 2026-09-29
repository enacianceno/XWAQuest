# DIAGNÓSTICO PUNTUAL: Cierre de VR-Probe v9 — VrBlit_Init

## 1. Operación que falla: NO DEMOSTRADA

El registro proporcido contiene únicamente dos líneas:
```
1028 VRPROBE stereo render 840x880
1079 VRPROBE FATAL stereo/blit init failed
```

**No hay evidencia suficiente para identificar con certeza cuál operación específica de `VrBlit_Init` devuelve 0.** El fallo ocurre dentro de `VrBlit_Init` (confirmado por el contexto: `VrStereo_Init` termina correctamente), pero las 4 rutas que pueden devolver 0 son todas indistinguibles con el log disponible.

---

## 2. Todas las rutas que devuelven 0 en VrBlit_Init

Ubicación: `vr-probe/vr_blit.c`, función `VrBlit_Init` (línea 88).

### Ruta A — Shader load_spv falla (`!s_vs || !s_fs`)
**Línea 93-97:**
```c
s_vs = load_spv(device, "fullscreen.vert", SDL_GPU_SHADERSTAGE_VERTEX, 0);
s_fs = load_spv(device, "fullscreen.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 1);
if (!s_vs || !s_fs) {
    return 0;   // ← Ruta A
}
```
- `load_spv` (línea 20-42) **solo loguea cuando el ARCHIVO no existe** (línea 27).
- Si `SDL_LoadFile` encuentra el `.spv` pero `SDL_CreateGPUShader` falla, `load_spv` **devuelve NULL SIN log**.
- `VrBlit_Init` tampoco loguea cuál shader falló (línea 95-96, sin log).
- **Esta ruta es SILENCIOSA cuando el SPIR-V existe pero falla la creación del shader.**

### Ruta B — Pipeline creation falla (`!s_pipe`)
**Línea 111-115:**
```c
s_pipe = SDL_CreateGPUGraphicsPipeline(device, &pi);
if (!s_pipe) {
    SDL_Log("VRPROBE blit pipeline failed: %s", SDL_GetError());
    return 0;   // ← Ruta B
}
```
- **Esta ruta sí produce un log** (`VRPROBE blit pipeline failed: ...`).
- Ese log **NO aparece** entre las líneas 1028 y 1079 del registro proporcionado.

### Ruta C — VrBlit_EnsureHdrPipeline devuelve 0
**Línea 116-118:**
```c
if (!VrBlit_EnsureHdrPipeline(device, xr_format)) {
    return 0;   // ← Ruta C
}
```
- `VrBlit_EnsureHdrPipeline` (línea 49-86) **siempre devuelve 1** en la práctica:
  - Si `fullscreen_hdr_to_sdr.frag` falta → log + `return 1` (línea 61-62).
  - Si `s_fs_hdr` es NULL → `return 1` (línea 63).
  - Si `!s_vs` → `return 0` (línea 64), pero `s_vs` fue verificado en la línea 96 de `VrBlit_Init`. **Esto es teóricamente inalcanzable.**
  - Si `SDL_CreateGPUGraphicsPipeline` para el pipeline HDR falla → log + `return 1` (línea 81-83).
- **Ruta C es efectivamente imposible** bajo condiciones normales.

### Ruta D — Sampler creation falla (`!s_sampler`)
**Línea 127-130:**
```c
s_sampler = SDL_CreateGPUSampler(device, &si);
if (!s_sampler) {
    return 0;   // ← Ruta D — COMPLETAMENTE SILENCIOSA
}
```
- **Sin log, sin SDL_Log, sin ningún rastro.** Es la ruta más difícil de diagnosticar.

---

## 3. Evidencia existente vs. capacidad de identificación

| Ruta | ¿Produce log? | ¿Aparece en registro 1028-1079? | ¿Identificable? |
|------|---------------|----------------------------------|-------------------|
| A (shader) | Solo si archivo falta; SILENCIOSA si compilación falla | No | **NO** |
| B (pipeline) | `VRPROBE blit pipeline failed` | No | **SÍ si apareciera** |
| C (HDR pipeline) | Siempre retorna 1; imposible | N/A | **NO aplica** |
| D (sampler) | Ningún log | No | **NO** |

**Conclusión:** El registro actual **NO permite** distinguir entre las rutas A, B y D. La ausencia del log de pipeline (Ruta B) sugiere que NO es pipeline creation, pero la ausencia de log de shader (Ruta A) o sampler (Ruta D) es igualmente consistente con el registro.

---

## 4. Correspondencia pipeline ↔ render target

### Flujo de formato
- `VrBlit_Init(g_aeron.gpu_device, VrXr_SwapchainFormat())` recibe el formato XR.
- Desde `vr_openxr.c` línea 161: `s_format = formats[0]` (primer formato devuelto por `SDL_GetGPUXRSwapchainFormats`).
- Log v5 (evidencia previa): `format=52`.
- **Formato 52 = `SDL_GPU_TEXTUREFORMAT_R8G8B8A8_UNORM_SRGB`** (SDR, verificado por enum en `SDL_gpu.h`).

### Pipeline creation
- `ct.format = xr_format` = 52 = `R8G8B8A8_UNORM_SRGB`.
- `fullscreen.frag` (shader Aeron estándar) outputea a `SV_Target0`.
- El pipeline se crea con un target de formato sRGB. **Esto debería funcionar** con el shader `fullscreen.frag`, que es el mismo shader usado exitosamente por `AeronScenePresentChain_Draw` en todas las versiones previas.

### Problema potencial no diagnosticado
El pipeline se crea contra el formato XR directamente. Si `SDL_CreateGPUGraphicsPipeline` rechaza el formato sRGB como color target (por una restricción del backend Vulkan en Quest 3S con el swapchain específico), el pipeline creation fallaría silenciosamente en la creación del shader o pipeline.

---

## 5. Shader HDR: no existe en el APK

### Hallazgo
- `fullscreen_hdr_to_sdr.frag.spv` **NO existe** en `m2/app/build/generated/engineAssets/shaders/`.
- No existe fuente `fullscreen_hdr_to_sdr.frag.hlsl` en `C:\OpenXWA\aeron\shaders\` ni en ningún otro directorio del proyecto.
- Es un shader **nuevo creado para v9** (`vr_blit.c` línea 59: `load_spv(device, "fullscreen_hdr_to_sdr.frag", ...)`) pero **nunca fue compilado ni empaquetado**.

### Impacto
- `VrBlit_EnsureHdrPipeline` (línea 59) llama a `load_spv` para este archivo.
- `load_spv` detecta que el archivo no existe (SDL_LoadFile devuelve NULL).
- Log: `VRPROBE blit: HDR→SDR shader missing, HDR pipeline disabled` (línea 61).
- Retorna 1 (continúa sin HDR pipeline).
- **Este camino NO causa el fallo de VrBlit_Init.** El HDR pipeline se deshabilita correctamente.

### Variable muerta
- `s_fs_hdr_to_sdr` (línea 46) se declara como `static SDL_GPUShader *s_fs_hdr_to_sdr = NULL` pero **nunca se usa**. El shader se carga en `s_fs_hdr` (línea 59). Es código muerto/confusión.
- `s_pipe_hdr` se declara dos veces (línea 15 como tentative, línea 47 como real). En C99/C11 esto es válido (la tentative se completa con la real), pero es confuso.

---

## 6. Hipótesis sin comprobar

### Hipótesis 1: Fallo en `load_spv` — shader SPIR-V incompatible
- Los `.spv` fueron compilados por `shadercross` para el backend SPIR-V del entorno M2.
- Si el dispositivo Quest 3S usa un backend diferente (o una versión de SDL_GPU que genera un compilador de shaders incompatible con estos SPIR-V específicos), `SDL_CreateGPUShader` podría fallar silenciosamente.
- **Evidencia a favor:** El formato SPIR-V fue aprobado en M2/M6, pero M6 no usa `VrBlit_Init` con estos shaders de forma directa (usa `AeronScenePresentChain`). Los shaders `fullscreen.vert.spv`/`fullscreen.frag.spv` nunca se usaron directamente con `SDL_CreateGPUShader` en la ruta actual.
- **Evidencia contra:** Los mismos archivos `.spv` están en `m2/build-android/shaders/` y `m2/app/build/generated/engineAssets/shaders/`, y M2/M6 funcionan.

### Hipótesis 2: Fallo en `SDL_CreateGPUGraphicsPipeline`
- El formato XR (52 = R8G8B8A8_UNORM_SRGB) puede tener restricciones especiales en el backend Vulkan de Quest que impidan la creación de un pipeline con color target sRGB y blend mode implicito.
- **Evidencia a favor:** Los logs de v7/v8 muestran degradación progresiva y el sistema mata el proceso, sugiriendo problemas de GPU.
- **Evidencia contra:** `VrStereo_Init` crea render targets con formatos HDR sin problema, y los shaders `fullscreen` son los mismos que Aeron usa exitosamente.

### Hipótesis 3: Fallo en `SDL_CreateGPUSampler`
- En Vulkan, la creación de sampler puede fallar si los parámetros de filtrado no son soportados por el formato del swapchain.
- **Evidencia a favor:** Es un caso conocido en Vulkan: ciertos formatos de swapchain no soportan todos los modos de mipmapping.
- **Evidencia contra:** Los parámetros del sampler (LINEAR min/mag, NEAREST mipmap) son estándar y deberían funcionar con cualquier formato de textura Vulkan.

---

## 7. Instrumentación mínima necesaria

Para distinguir las rutas en una **única ejecución**, se necesitan cambios mínimos en `vr_blit.c`:

### Cambio A: Loguear cada paso en `load_spv` (para distinguir Ruta A)
```c
static SDL_GPUShader *load_spv(SDL_GPUDevice *device, const char *name,
                               SDL_GPUShaderStage stage, Uint32 samplers) {
    char path[1024];
    size_t size = 0;
    SDL_snprintf(path, sizeof path, "%s/%s.spv", g_aeron.shader_root, name);
    Uint8 *code = (Uint8 *)SDL_LoadFile(path, &size);
    if (!code) {
        SDL_Log("VRPROBE blit shader missing %s: %s", path, SDL_GetError());
        return NULL;
    }
    SDL_GPUShaderCreateInfo info;
    memset(&info, 0, sizeof info);
    info.code = code;
    info.code_size = size;
    info.entrypoint = "main";
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.stage = stage;
    info.num_samplers = samplers;
    info.num_uniform_buffers = stage == SDL_GPU_SHADERSTAGE_FRAGMENT ? 1u : 0u;
    SDL_GPUShader *sh = SDL_CreateGPUShader(device, &info);
    if (!sh) {
        SDL_Log("VRPROBE blit shader create failed %s: %s", name, SDL_GetError());
    }
    SDL_free(code);
    return sh;
}
```

### Cambio B: Loguear sampler failure (para distinguir Ruta D)
```c
    s_sampler = SDL_CreateGPUSampler(device, &si);
    if (!s_sampler) {
        SDL_Log("VRPROBE blit sampler failed: %s", SDL_GetError());
        return 0;
    }
```

### Cambio C: Loguear pipeline formato antes de crear (para contexto)
```c
    VrLog("VRPROBE blit creating pipeline format=%d vs=%p fs=%p", (int)xr_format, (void*)s_vs, (void*)s_fs);
```

### Total: 3 líneas de log adicionales, sin cambios de lógica.

Con estos cambios, en una sola ejecución se puede leer exactamente qué operación falla:
- Si `VRPROBE blit shader create failed` aparece → Ruta A
- Si `VRPROBE blit pipeline failed` aparece → Ruta B
- Si `VRPROBE blit sampler failed` aparece → Ruta D
- Si ninguno aparece → Ruta C (inhalcanzable) o `VrBlit_Shutdown` causa efecto secundario

---

## 8. Resumen

| Aspecto | Estado |
|---------|--------|
| Operación que falla | **No demostrada** — el registro es insuficiente |
| `fullscreen.vert.spv` | Existe en assets |
| `fullscreen.frag.spv` | Existe en assets |
| `fullscreen_hdr_to_sdr.frag.spv` | **No existe** — HDR pipeline deshabilitado, no causa fallo |
| `is_sdr_dst` bug (formato 20-23) | Bug de enum, solo afecta `VrBlit_Copy`, no `VrBlit_Init` |
| Rutas de fallo distinguibles | **No** con el registro actual |
| Instrumentación necesaria | 3 líneas de log adicionales en `vr_blit.c` |
| Cambios en M0-M6 / GameData / frontend / OPT | Ninguno requerido |
