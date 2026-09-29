# M7C — OpenXR Floating Panel (Etapa 0)

## Objetivo
Presentar el juego XWA en un panel flotante dentro de la sesión OpenXR del Quest 3S.

## Cambios vs M7B

### Archivos nuevos
- `xr_session.c` — Gestión de sesión OpenXR: init, frame loop, QUAD layer, head pose
- `xr_session.h` — API pública para wrappers

### Archivos modificados
- `trace.c` — Añadido `xr_session.h`, `stdlib.h`; wrappers `Aeron_Init` (putenv XR), `Aeron_RenderBackendInit` (M7C_XrInit), `Aeron_Present` (M7C_XrEndFrame), `Aeron_Shutdown` (M7C_XrShutdown); JNI m7→m7c
- `input_android.c` — Añadido `xr_session.h`; wrappers `Aeron_BeginFrame` (M7C_XrPollEvents + M7C_XrWaitFrame); JNI m7→m7c
- `CMakeLists.txt` — Proyecto renombrado a XWAQuestM7C, añadido `xr_session.c`
- `app/build.gradle` — package m7c, versionName 0.8-M7C-xr-panel
- `app/src/main/AndroidManifest.xml` — Label M7C, adds OpenXR feature (optional)
- `settings.gradle` — rootProject M7C
- `RuntimeActivity.java` — Package m7c, log tags M7C, libraries M7C

### Preservado
- M7B (directorio m7/) intacto
- M6 (directorio m6/) intacto
- GameData intacto
- Piloto intacto
- Todas las funciones de M7B (trigger→RETURN, nativeInFlight, axes)

## Arquitectura de separación

```
Game Engine (flight, input, audio) → SIN CAMBIOS
Camera (FlightView_UpdatePlayerCamera) → SIN CAMBIOS
Rendering (Aeron_Present) → SIN CAMBIOS
Presentation → REDIRECTED through OpenXR
```

## Flujo del frame

```
Aeron_BeginFrame()
  → M7C_XrPollEvents()          ← detecta estado de sesión
  → M7C_XrWaitFrame()           ← sincroniza con VR display
  → __real_Aeron_BeginFrame()   ← game tick normal
XwaPort_Tick()
  → FlightInput_Read()          ← controles normales
  → FlightView_UpdatePlayerCamera() ← cámara normal
Aeron_Present()
  → __real_Aeron_Present()      ← renderiza al swapchain de la ventana
  → M7C_XrEndFrame()            ← envía QUAD layer al headset
```

## Estado de Etapa 0

| Componente | Estado | Notas |
|-----------|--------|-------|
| XR session | ✅ Implementado | Crea session + LOCAL space |
| QUAD layer | ✅ Implementado | Panel 2m×1.2m a 2m frente |
| Head pose | ✅ Implementado | xrLocateViews en cada frame |
| Blit a XR | ⏳ Pendiente | El juego renderiza a su ventana; falta copiar al swapchain XR |
| Control | ✅ Conservado | Trigger→RETURN funciona en menús |
| Throttle | ✅ Conservado | Stick Y → g_throttleSmoothed |

## Pendiente para completar Etapa 0

1. **Blit del frame del juego al swapchain XR** — Copiar la textura del swapchain de Aeron al swapchain QUAD de OpenXR. Esto requiere acceder a la textura del swapchain de Aeron después de `Aeron_Present` y copiarla al swapchain de XR.

2. **Prueba en Quest 3S** — Verificar que la sesión XR se inicia y el panel aparece.

## Build

```bash
cd m7c
& "$env:LOCALAPPDATA/Android/Sdk/cmake/3.22.1/bin/cmake.exe" -B build-android -DCMAKE_TOOLCHAIN_FILE="$env:LOCALAPPDATA/Android/Sdk/ndk/28.2.13676358/build/cmake/android.toolchain.cmake" -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-29 -DCMAKE_BUILD_TYPE=Release
& "$env:LOCALAPPDATA/Android/Sdk/cmake/3.22.1/bin/cmake.exe" --build build-android --target OpenXWAM7C -j8
Copy-Item build-android\libOpenXWAM7C.so ..\app\build\generated\engineJni\arm64-v8a\
& ..\gradlew.bat -p . assembleDebug
```
