# XWAQuest - Estado del proyecto

## Objetivo
Portar Star Wars X-Wing Alliance a Meta Quest 3/3S como aplicación standalone Android ARM64, utilizando los archivos originales del juego y evolucionando progresivamente hacia una experiencia VR estereoscópica.

## Milestone: FIRST REAL X-WING VISUALLY CONFIRMED ON QUEST 3S (2026-09-24) ✅

**Hecho demostrado**: X-Wing real visible físicamente en Quest 3S (pipeline V10 ShaderCross + SDL_GPU + OpenXR).

- **APK**: `C:\OpenXWA\XWAQuest\vr-probe\app\build\outputs\apk\debug\app-debug.apk`
- **SHA-256**: `D41B7AAAE0F2EAA6F149AE64927C15661E187BB0125C270F021F6824C55E8937`
- **Package**: `org.openxwa.xwaquest.vrprobe`
- **Log**: `C:\OpenXWA\XWAQuest\vr-probe\evidence\xwing-v10-run-20260924-222207.log`
- **Runtime real**: `vertex_count=10380`, `index_count=852`, `triangle_count=284`
- **Bounds**: `min=(-5.467,-6.480,-1.695)`, `max=(5.508,6.480,1.503)`, `center=(0.021,0.000,-0.096)`, `scale=0.123456`
- **DEMONSTRADO**: `MESH_CPU_READY`, `BOUNDS_OK`, `VBO_CREATED`, `IBO_CREATED`, `UPLOAD_SUBMITTED`, `DRAW_RECORDED`, `30/30 xrEndFrame result=0`, `stereo=30`, `submitted=30`
- **Sin fallback al triángulo** (`triangle-draw ok=0`, `mesh draw inactive=0`)
- **Terminación NORMAL**: `reason=auto-exit submitted cap`, `exit code=0` (auto-exit diagnóstico de 30 frames — **no es un crash**)
- **VISUAL_CONFIRMED_BY_USER = DEMOSTRADO**: el usuario confirmó físicamente en el Quest 3S: **UN X-WING MAGENTA RECONOCIBLE**

**Handoff M8**: `research/CODEX_HANDOFF_M8.md` (documento de entrada para continuar el proyecto).

> Nota: las secciones siguientes (era v9.7.2, "FALLO EN INICIALIZACIÓN" de VrStereo_Init, hipótesis H1–H5) son **históricas**: el pipeline V10 actual está demostrado operativo (milestone arriba). Tabla de versiones: M7B vive en `C:\OpenXWA\XWAQuest\m7` (paquete `org.openxwa.xwaquest.m7`); no existen carpetas `m7a`/`m7b` (ver `research/CODEX_HANDOFF_M8.md`, Apéndice — contradicciones documentadas).

## Versiones y paquetes Android

| Versión | Paquete | Estado | Carpeta |
|---------|---------|--------|---------|
| M6 | `org.openxwa.xwaquest.m6` | **Funcional** - Juego original en panel 2D | `C:\OpenXWA\XWAQuest\m6` |
| M7A | `org.openxwa.xwaquest.m7a` | Prueba de integración OpenXR | `C:\OpenXWA\XWAQuest\m7a` |
| M7B | `org.openxwa.xwaquest.m7b` | **Funcional** - Juego original en panel 2D | `C:\OpenXWA\XWAQuest\m7b` |
| M7C | `org.openxwa.xwaquest.vrprobe` | **En desarrollo** - Integración OpenXR estereoscópica | `C:\OpenXWA\XWAQuest\vr-probe` |

## Funcionalidad comprobada

### M6 (Base preservada)
- ✅ Juego original ejecutándose en panel 2D en Quest 3S
- APK: `6FFCE008...0FED` (SHA-256 verificado)
- GameData: 7660 archivos, incluye `FLIGHTMODELS/XWING.OPT`

### M7B (Panel 2D funcional)
- ✅ Juego original en panel 2D en Quest 3S
- APK: `6FFCE008...0FED` (idéntico a M6)
- Confirmación visual: "veo el cubo de colores correctamente sobre un fondo azul oscuro"

### M7C (vr-probe) - **Estado actual: FALLO EN INICIALIZACIÓN**
- **Paquete**: `org.openxwa.xwaquest.vrprobe`
- **Carpeta**: `C:\OpenXWA\XWAQuest\vr-probe`
- **Librería**: `libOpenXWAVRP.so` (compilada desde `vr-probe/`)
- **Build**: `BUILD SUCCESSFUL` (libOpenXWAVRP.so generado)
- **Instalación**: Exitosa en Quest 3S
- **Sesión OpenXR**: Llega a `XR_SESSION_STATE_FOCUSED` ✅
- **Swapchains**: 2 ojos, 1680x1760, depth buffer OK ✅
- **VrBlit_Init**: OK (`ok=1`)
- **VrStereo_Init**: **FALLA** (`ok=0`) - pipeline plano falla
- **Resultado**: `FATAL stereo/blit init failed stereo=0 blit=1` → watchdog phase=6

## Arquitectura actual (M7C - vr-probe)

### Flujo de inicialización (vr_main.c)
1. `Aeron_Init()` → `XwaHostConfig_Load()` → `XwaSetup_ValidateGameData()` → `XwaRemaster_Init()` → `XwaPort_Init()`
2. `ModelPreview_LoadModel("FlightModels\\XWING.OPT")` → 60 `XwaPort_Tick`
3. `XwaRemasterShip_SyncAssets` + `CommitSyncBatch` (cocinado OPT→glTF real)
4. `XwaRemasterShip_MeshForName("xwing")` → 2x `AeronScene_Begin/AddMeshInstance/Render` (uno por ojo)
4. `VrXr_Init()` → `VrStereo_Init()` → `VrBlit_Init()`
5. Loop estereoscópico: 2x `AeronScene_Render` → tonemap → `PresentChain` → blit a swapchain XR

### Dependencias clave (v9.7.2-fixed)
- **Aeron** (motor gráfico) - `libOpenXWAVRP.so` enlaza Aeron estático
- **SDL3** - `libSDL3.so` (3.5.0, rev `8f8ed75`)
- **OpenXR** - `libopenxr_loader.so` (AAR `openxr_loader_for_android:1.1.43`)
- **OpenXR Runtime** - Quest 3S runtime desde `/odm/etc/openxr/1/active_runtime.aarch64.json`
- **GameData** - Copia privada `m6 → vrprobe` (7660 archivos, `FLIGHTMODELS/XWING.OPT`)

## Problema activo de M7C

### Fallo: `SDL_ClaimWindowForGPUDevice` / Pipeline plano

**Síntoma**: `VrStereo_Init` falla en la creación del pipeline plano de diagnóstico (`vkCreateGraphicsPipelines Unhandled VkResult!`)

**Logs clave (v9.7.2-fixed-device-log.txt)**:
```
1154 VRPROBE init flat-pipe gpu_device=0xb400006fa4d41900
1154 VRPROBE init flat VS create begin spv_size=916
1154 VRPROBE init flat VS create ok vs=0xb400006f15baf750
1154 VRPROBE init flat FS create begin spv_size=712
1154 VRPROBE init flat FS create ok fs=0xb400006f1ae78bd0
1154 VRPROBE init flat pipeline create begin fmt=29
1154 VRPROBE init FAIL: flat pipeline create err=vkCreateGraphicsPipelines Unhandled VkResult!
1154 VRPROBE init VrStereo_Init end ok=0
1154 VRPROBE init VrBlit_Init begin fmt=52
...
1167 VRPROBE init VrBlit_Init end ok=1
1167 VRPROBE FATAL stereo/blit init failed stereo=0 blit=1
```

**Formato del pipeline plano**: `fmt=29` = `SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT` (RGBA16_FLOAT)
**Formato swapchain/scene RT**: `fmt=18` = `AERON_TEXTURE_FORMAT_RGBA16_FLOAT` → SDL `SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT` (29)

## Hipótesis pendientes de verificar

| # | Hipótesis | Evidencia | Estado |
|-----|-----------|-----------|--------|
| H1 | **Formato 29 (RGBA16_FLOAT) no soportado como color attachment** en Adreno 740 | VrBlit usa formato 52 (SRGB) y funciona; pipeline plano usa 29 y falla | **Pendiente** - Verificar `SDL_GPUTextureSupportsFormat` |
| H2 | **Falta `vertex_input_state`** en pipeline plano | VrBlit funciona sin vertex_input_state; pipeline plano lo tiene pero falla | **Pendiente** |
| H3 | **Orden de inicialización**: `SDL_ClaimWindowForGPUDevice` después de `Aeron_Init` | `Aeron_Init` crea device; `VrXr_Init` usa device; ¿window claim antes? | **Pendiente** - Revisar `vr_main.c:258` vs `Aeron_Init()` |
| H4 | **Formato depth mismatch** | Scene RT tiene depth; pipeline plano no declara depth target | **Pendiente** |
| H5 | **Falta `enable_depth_clip`** en rasterizer | Vulkan lo requiere explícito | **Pendiente** |

## Próxima prueba autorizada

**Prueba mínima reversibles**: Verificar `SDL_GPUTextureSupportsFormat` para formato 29 como `COLOR_TARGET` antes de crear pipeline.

**Cambio propuesto** (en `vr_stereo.c`, antes de `SDL_CreateGPUGraphicsPipeline`):
```c
bool fmt_ok = SDL_GPUTextureSupportsFormat(dev, s_ct.format,
    SDL_GPU_TEXTURETYPE_2D, SDL_GPU_TEXTUREUSAGE_COLOR_TARGET);
VrLog("VRPROBE format %d supported as COLOR_TARGET: %d", (int)s_ct.format, fmt_ok);
```

**Criterio de éxito**: Log muestra `supported as COLOR_TARGET: 1` o `0`.

**Rollback**: Revertir cambio (git checkout o revert manual).

## Reglas de seguridad

### ADB
> **Antes de CUALQUIER comando ADB, incluso `adb devices`, preguntar:**
> "¿Ya encendiste el Quest 3S, lo conectaste a la PC y autorizaste la depuración USB? Confírmame cuando esté listo para continuar."

### Versiones protegidas
- **No modificar**: M6, M7A, M7B (especialmente M7B)
- **No modificar**: GameData, archivos originales del juego, datos del piloto
- **No modificar**: M6, M7A, M7B sin autorización explícita

### ADB
- No ejecutar `adb devices` sin confirmación del usuario
- No desinstalar versiones funcionales sin autorización
- No comandos ADB destructivos sin autorización específica

### Arquitectura VR
- Motor y lógica del juego: separados
- Entrada de controles: separada
- Cámara del jugador: separada
- Renderizado del juego: separado
- Presentación OpenXR: separada

## Registro de cambios y pruebas (M7C)

| Fecha | Versión | Cambio | Resultado |
|-------|---------|--------|-----------|
| 2026-09-19 | v9.0 | Corrección loop estereoscópico | Loop avanza W1-W5 |
| 2026-09-19 | v9.1 | Pipeline plano con vertex_input | Fallo `vkCreateGraphicsPipelines` |
| 2026-09-19 | v9.2 | Loop fix + pipeline plano | Fallo `vkCreateGraphicsPipelines` |
| 2026-09-20 | v9.3 | Pipeline con vertex_input + rasterizer | Fallo `vkCreateGraphicsPipelines` |
| 2026-09-20 | v9.4 | Pipeline sin vertex_input | Fallo `vkCreateGraphicsPipelines` |
| 2026-09-20 | v9.5 | Pipeline mínimo (como VrBlit) | Fallo `vkCreateGraphicsPipelines` |
| 2026-09-20 | v9.5 | Formato 52 (SRGB) | Fallo `vkCreateGraphicsPipelines` |
| 2026-09-20 | v9.6 | Formato 29 (RGBA16_FLOAT) + vertex_input + rasterizer | Fallo `vkCreateGraphicsPipelines` |
| 2026-09-20 | v9.7 | Fix tipo AeronShader/SDL_GPUShader | Fallo `vkCreateGraphicsPipelines` |
| 2026-09-20 | v9.7.1 | Diagnóstico granular | Fallo `vkCreateGraphicsPipelines` |
| 2026-09-20 | v9.7.2 | Formato 29 + vertex_input + rasterizer + depth_clip | **Fallo persistente** |

### Archivos clave M7C (vr-probe)
| Archivo | Función |
|---------|---------|
| `vr_main.c` | Entry point, init sequence, main loop |
| `vr_openxr.c` | `VrXr_Init`, `create_swapchains`, `VrXr_Init` |
| `vr_stereo.c` | `VrStereo_Init` (pipeline plano falla aquí) |
| `vr_blit.c` | `VrBlit_Init` (funciona - referencia) |
| `vr_stereo.h` | Declaraciones |
| `vr_blit.h` | Declaraciones |
| `vr_framing.c` | Framing, auto-exit |
| `vr_convert.c` | Conversión coordenadas OpenXR → Aeron |
| `vr_flat_shaders.h` | SPIR-V shaders embebidos (VS 916B, FS 712B) |

## Datos no verificados (pendientes)

| Dato | Estado | Próxima acción |
|------|--------|----------------|
| `SDL_GPUTextureSupportsFormat(dev, 29, 2D, COLOR_TARGET)` | **Desconocido** | Ejecutar prueba propuesta |
| `SDL_ClaimWindowForGPUDevice` orden vs `Aeron_Init` | **No verificado** | Revisar `vr_main.c` vs `Aeron_Init` |
| `SDL_GPUTextureSupportsFormat(dev, 29, 2D, SAMPLER)` | **Desconocido** | Verificar si sampler requiere soporte |
| Pipeline plano con `vertex_input_state` conectado | **No verificado en device** | Probar con vertex_input conectado |

---

*Última actualización: 2026-09-22*
*Próxima revisión: tras ejecutar prueba de `SDL_GPUTextureSupportsFormat`*