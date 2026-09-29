# REPORTE M3 — completado

Fecha: 2026-09-19. M4 no iniciado.

## Resultado

El Quest 3S ejecutó realmente `SDL_main` de OpenXWA, inicializó Aeron y llegó a la validación original de GameData. El primer límite fue `RESDATA.TXT` ausente. La validación retornó fallo real, seguido de `Aeron_Shutdown`; no se simuló éxito ni se saltaron requisitos para alcanzar `XwaPort_Init`.

Ruta demostrada:

`RuntimeActivity (SDLActivity) -> SDL_main -> XwaLaunchOptions_Parse -> Aeron_Init -> XwaHostConfig_Load -> resolve_game_data -> XwaSetup_ValidateGameData -> AeronVfs_Exists -> salida controlada y Aeron_Shutdown`.

`XwaPort_Init` no fue invocado: la ruta original resuelve GameData antes de inicializar remaster y antes de esa función. La falta de datos es el único bloqueo observado en la secuencia alcanzada; no implica que las etapas posteriores ya estén verificadas.

## Archivos y aislamiento

Todos los archivos nuevos están en `C:/OpenXWA/XWAQuest/m3`:

- `CMakeLists.txt`: enlace completo de los archivos estáticos reales M2 y generación de `build-android/main-m3.c` desde el `main.c` original.
- `trace.c`: instrumentación mediante `--wrap`; cada wrapper llama a la implementación real y conserva su retorno.
- `Build-Target.ps1`: compila sólo la entrada M3 y empaqueta dependencias ya producidas en M2.
- `settings.gradle`, `build.gradle`, `gradle.properties`, `app/build.gradle`, `local.properties`: proyecto independiente, paquete `org.openxwa.xwaquest.m3`.
- `app/src/main/AndroidManifest.xml` y `app/src/main/java/org/openxwa/xwaquest/m3/RuntimeActivity.java`: Activity SDL real, sin presentación OpenXR.
- `.gitignore`, `ESTADO.md` y `evidence/`: estado y evidencias.
- Este reporte: `C:/OpenXWA/XWAQuest/REPORTE-M3.md`.

La copia generada de main añade logs, reemplaza el diálogo fatal por una salida registrada al fallar GameData y añade un límite antes del remaster incluso si apareciesen datos válidos. No sustituye validaciones ni implementaciones del motor. Los anclajes CMake fallan si el código esperado deja de encontrarse.

No se modificaron fuentes originales del motor, Aeron, SDL, OPT ni archivos de M0/M1/M2/hello_xr. Se verificaron hashes de los archivos y artefactos M2 registrados, sus archivos estáticos y bibliotecas compartidas, y el inventario preservado M1/OPT: sin cambios (`evidence/preservation.txt`). No se repitieron builds M0/M1/M2.

## Build y dispositivo

- `gradlew.bat -p m3 :app:assembleDebug --console=plain`, ejecutado desde `C:/OpenXWA/XWAQuest` con el JDK/SDK de M2.
- Resultado: **BUILD SUCCESSFUL in 32s**. Cuatro unidades C compiladas: main generado, trace, host_config y setup. Dependencias y motor M2 reutilizados, sin recompilar.
- Nuevo `libOpenXWAM3.so`: ELF64 AArch64, enlace `--no-undefined` y retención completa de archivos estáticos. NDK 28.2.13676358, API 29, ARM64-v8a; JDK 21, Gradle 8.5 y AGP 8.1.4 existentes.
- APK: `m3/app/build/outputs/apk/debug/app-debug.apk`, 13 269 587 bytes.
- SHA-256 local e instalado: `4F0258B12184F5083A910B55910C75D40318076DCA438EDA1C55B3C560548127`.
- Nueve bibliotecas: OpenXWAM3, SDL3, avcodec, avformat, avutil, swresample, swscale, libc++_shared y openxr_loader. Sólo ABI arm64-v8a. Recursos propios y shaders proceden de M2; no se añadieron assets comerciales.
- Quest 3S, serie `3487C10H9T0BKZ`; instalación `Success`. Ejecución demostrada en PID **22581**, hilo SDL **22621**, hora de logcat **04:13:40–04:13:41**.

## Subsistemas demostrados y logs

Extractos de `m3/evidence/runtime-process.txt`:

```text
04:13:40.987 M3 SDL_main ENTER: real OpenXWA application startup
04:13:41.001 M3 SDL_Init END ok=1 video=android audio=AAudio error=none
04:13:41.509 M3 Aeron_WindowInit END ok=1
04:13:41.509 M3 Aeron_RenderBackendInit BEGIN shaders=assets://shaders
04:13:41.538 aeron: Loading SPIR-V shaders for SDL GPU driver 'vulkan'
04:13:41.571 M3 Aeron_RenderBackendInit END ok=1 driver=vulkan
04:13:41.581 aeron.audio: audio device opened: 48000 Hz, 2 ch, S16 buffer=960 frames
04:13:41.581 M3 Aeron_AudioInit END ok=1 error=none
04:13:41.581 M3 Aeron_ControllersInit END
04:13:41.581 M3 Aeron_InitVfs END resource=assets://resources user=/data/data/org.openxwa.xwaquest.m3/files/
04:13:41.582 M3 Aeron_DebugUiInitInternal END
04:13:41.582 M3 Aeron_Init END ok=1 base=assets:// resource=assets://resources user=/data/data/org.openxwa.xwaquest.m3/files/
04:13:41.583 M3 XwaHostConfig_Load END ok=1 error=none
04:13:41.583 M3 GameData validation BEGIN candidate=/data/user/0/org.openxwa.xwaquest.m3/files/m3-empty-data
04:13:41.583 M3 GameData validation END ok=0 detail=selected directory is missing 'RESDATA.TXT'
04:13:41.583 M3 STOP GameData validation rejected: selected directory is missing 'RESDATA.TXT'; cancelled=0
04:13:41.635 M3 Aeron_Shutdown END
04:13:41.636 Finished main function
```

SDL creó la ventana Android. El backend Aeron real creó dispositivo Vulkan, cargó shaders integrados y completó sus pipelines iniciales; el driver es `/vendor/lib64/hw/vulkan.adreno.so`. También arrancaron VFS, configuración inicial, debug UI y enumeración de dos gamepads. Audio y controladores se inicializaron como parte normal de Aeron, sin implementar controles de juego ni audio completo. AAudio y los controladores se cerraron; SDL registró `onPause`, `surfaceDestroyed`, `onStop` y `onDestroy`.

No se ejecutaron frontend, misiones, GameData, remaster ni presentación XR. El loader OpenXR se carga por dependencia, pero no se crea sesión XR M3.

## Primer requisito de GameData

Se pasó el argumento existente `--game-data=/data/user/0/org.openxwa.xwaquest.m3/files/m3-empty-data`. Es un directorio diagnóstico privado y vacío, confirmado mediante ADB, no una política definitiva de descubrimiento/importación.

`src/xwa_app/setup.c`, función `XwaSetup_ValidateGameData`, establece la raíz ASSET y activa búsqueda sin distinguir mayúsculas/minúsculas. `setup_check_required_files` comprueba, en este orden:

1. `RESDATA.TXT` — ausencia confirmada en Quest; detiene la lista.
2. `FLIGHTMODELS/SPACECRAFT0.LST`.
3. `MISSIONS/MISSION.LST`.
4. `MOVIES/PROLOGUE.SNM`.
5. `MOVIES/BATTLE1.SNM`.
6. `WAVE/FRONTEND/B1M1/N010101.WAV`.

Los puntos 2–6 están establecidos por código; no fueron consultados tras fallar el primero. La función también comprueba `ALLIANCE/RESDATA.TXT` para reconocer una copia de CD no fusionada y producir un error específico. El candidato vacío produjo el error normal por `RESDATA.TXT`. La lista es una validación inicial, no un inventario exhaustivo de datos necesarios para el juego.

Más adelante, `XwaPort_Init` comienza con configuración de compatibilidad Dx5, proveedores de entrada, reloj y VFS, y solicita `Resdata.txt`, `xwa.tab` y `strings.txt`. Esto se documenta por lectura de código; **no se ejecutó** porque la validación previa impide llegar legítimamente allí sin datos originales.

## Problemas y riesgos

- El primer intento quedó bloqueado por visor dormido; posteriormente Horizon registró un bloqueo de Guardian. Tras despertar el visor y reintentar, la Activity abrió normalmente. No se alteró Guardian ni se eludió esa protección.
- No hicieron falta correcciones al motor Android para la secuencia alcanzada. Se utilizó SDLActivity real en vez de la Activity de carga M2.
- Advertencia no fatal de Aeron: `SDL_SetWindowIcon ... not supported`; icono de ventana de escritorio no admitido por Android.
- Gralloc/AHardwareBuffer reportó formatos 0x38/0x3b no soportados durante la creación del backend. No impidieron `Aeron_RenderBackendInit ok=1`. Se conservan como riesgo de formatos para etapas posteriores, sin afirmar compatibilidad de todos los formatos del motor.
- Avisos Android de sensores ausentes, propiedades de profiler restringidas, QSPM ausente, audio sin packageName, input connection al cerrar y swap behavior del renderer Java. Ninguno impidió la secuencia demostrada. El OpenGLRenderer Java no es el backend Aeron, confirmado Vulkan.
- No hay crash nativo ni excepción fatal en el proceso observado. El rechazo GameData conserva retorno de fallo de main; no equivale a fallo de aceptación M3. Un intento posterior de reabrir la Activity en el mismo proceso terminó con la política SDL `main() finished / System.exit status 0`; para otra prueba se debe usar lanzamiento frío con `am start -S`.
- Advertencias de build: Java source/target 8 obsoleto, compatibilidad AGP/compileSdk heredada y metadatos riscv64 ignorados; APK exclusivamente ARM64.
- Subsistemas posteriores a GameData siguen sin validarse. No hay garantía de ejecución del juego, de ausencia de fugas en ejecución prolongada ni de renderización XR.

## Aceptación

**M3 cumplido**: entrada OpenXWA real ejecutada en Quest; SDL/Aeron/Vulkan/VFS/configuración demostrados; primer límite legítimo de GameData identificado; ningún stub añadido; M0/M1/M2 preservados. Se detiene aquí, antes de importar/localizar datos y antes de M4.
