# M8.1 — salida real del juego, monoscópica, mediante OpenXR

## Resultado local

Implementado y compilado únicamente bajo `C:\OpenXWA\XWAQuest\m8`.
Compilación/enlace nativo ARM64 correctos; APK generado y firma v2 verificada.
NO se utilizó ADB, NO se instaló y NO se ejecutó en Quest. Ningún estado visual,
de audio audible o de recorrido frontend/hangar/vuelo está confirmado en M8.

APK: `C:\OpenXWA\XWAQuest\m8\app\build\outputs\apk\debug\app-debug.apk`

- Package: `org.openxwa.xwaquest.m8`
- Activity: `org.openxwa.xwaquest.m8.M8Activity`
- VersionName: `0.8.1-mono-xr`; versionCode: `1`
- ABI: `arm64-v8a`; minSdk 29; targetSdk 34
- Tamaño: 13284401 bytes
- SHA-256: `150a6723bef6ffdaccb388ccd78598535b0af6d4ed3199f34ee4efbbe33127bb`

## Archivos nuevos

Código: `main.c`, `frame_bridge.c`, `frame_bridge.h`, `input_xr.c`,
`input_xr.h`, `xr_bridge.c`.

Build: `CMakeLists.txt`, `Build-Target.ps1`, `Build-Apk.ps1`, `build.gradle`,
`settings.gradle`, `gradle.properties`, `local.properties`, `.gitignore`.

Android: `app/build.gradle`, `app/src/main/AndroidManifest.xml`,
`app/src/main/java/org/openxwa/xwaquest/m8/M8Activity.java`.

Documentación/evidencia: este `README.md`, `evidence/*` (inventarios, hashes,
logs de build, comprobación de wrappers, dependencias, package y firma).
Los objetos, mapa, bibliotecas, assets empaquetados y APK están en
`build-android/` y `app/build/`; los archivos temporales, cachés y copia local
de la clave debug están en `tmp/`, `.gradle/`, `gradle-home/`, `tool-home/`.
Estas rutas generadas y la configuración local están excluidas en `.gitignore`.

Archivos editados fuera de M8: NINGUNO. Se compararon 1874 archivos de
m6/m7/m7c/vr-probe por existencia, tamaño y fecha, y 65 hashes de fuentes y
archivos seleccionados: cero diferencias y cero archivos nuevos en esos
baselines (`evidence/isolation-check.txt`). Los demás originales se usaron
sólo como entradas: no se editaron hello_xr, GameData, pilotos, parser,
shaders, física, IA ni renderer de vuelo.

## Arquitectura y conservación del juego

`main.c` incluye directamente el entry point actual `../../src/xwa_app/main.c`.
No se reescribe la simulación ni el loop: BeginFrame -> XwaPort_Tick (o la
rama de pausa original) -> XwaRemaster_Frame -> Aeron_Present. No hay límite
de frames ni temporizador de salida. Un guard local rechaza una segunda
llamada a XwaPort_Tick en la misma iteración lógica. El loop no se repite por ojo.

Se enlazan los mismos archivos estáticos de juego/remaster de M7B. Aeron se
obtiene del archivo ya existente `vr-probe/staging/libaeron_vrprobe.a`, que
incorpora la activación OpenXR por entorno. Los siete .so auxiliares de SDL,
FFmpeg y C++ copiados al staging M8 coinciden byte por byte con M7.

`frame_bridge.c` usa wrappers de enlace: conserva toda la composición real
(pixel layers, texturas y callbacks directos), pero redirige su adquisición
de swapchain a una textura COLOR_TARGET|SAMPLER propiedad de M8, de tamaño y
formato SDR iguales a la salida de ventana. No selecciona una textura de
objeto ni introduce geometría de VRPROBE. Cuando no hay nuevas capas se
conserva la última imagen cuya composición fue enviada correctamente.

El wrapper de BeginFrame espera OpenXR una vez; se omiten exclusivamente
la espera de swapchain de ventana y Aeron_WaitForNextFrame. Se conserva el
cálculo de delta y los límites temporales propios del juego. OpenXR es la
única fuente de pacing de presentación; los dos ojos no producen dos ticks.

Después del submit del compositor: localizar vistas válidas -> acquire/wait
de las dos imágenes XR -> VrBlit_Copy de la misma textura a ambos ojos ->
submit SDL_GPU -> release comprobado -> xrEndFrame con una capa. No se aplica
la pose XR a la cámara del juego. No se incorporan vr_stereo.c ni shaders V10.
Se reutilizan sin editar `vr_blit.c` y los shaders fullscreen existentes.
El APK contiene 86 assets SPIR-V existentes; no hubo compilación de shaders.

Se añadió `xr_bridge.c` porque las handles de sesión son privadas en
vr_openxr.c. Incluye el original sin alterarlo y engancha la creación de
sesión para adjuntar las acciones antes de beginSession. Sus helpers locales
comprueban acquire/wait/release; no liberan una imagen que haya agotado el
wait sin completarlo. Ese fallo detiene la aplicación y desmonta la sesión.

## Input provisional

- A derecho: RETURN; B derecho: ESC.
- Thumbstick izquierdo: flechas con repetición.
- Thumbstick derecho: cursor virtual; gatillo derecho: botón izquierdo.
- X izquierdo, sólo en el prompt de piloto: escribe `m8test` mediante eventos
  de texto originales; el usuario todavía debe confirmar con A.
- Mantener Y izquierdo dos segundos: salida explícita.

La posición y los botones del cursor se aplican al snapshot de Aeron después
de su muestreo de ratón Android. No depende de hover Android. El foco procede
de xrSyncActions. No se inyectan movimiento relativo de vuelo, control de
orientación VR ni head tracking de misión. No se modifica directamente un
piloto ni se confirma automáticamente su creación.

## Estados e interpretación del log

Los contadores de éxito se imprimen la primera vez y cada 120 ocurrencias;
los cambios de fase y fallos se imprimen al ocurrir.

| Marcador | Evidencia que representa en el código |
|---|---|
| M8_XR_SESSION_RUNNING | Inicialización XR y blit terminadas; beginSession tuvo éxito |
| M8_GAME_TICK | Regresó una llamada real a XwaPort_Tick; incluye delta y frame lógico |
| M8_GAME_FRAME | Regresó XwaRemaster_Frame; incluye número de capas |
| M8_PRESENT_TEXTURE_READY | Compositor capturó la textura y su submit tuvo éxito; no significa GPU finalizada |
| M8_XR_BLIT | Se grabaron ambos blits y el submit SDL_GPU fue aceptado |
| M8_XR_FRAME_PRESENTED | Release y xrEndFrame con layers=1 aceptados; visual=unconfirmed |
| M8_FRONTEND / M8_HANGAR / M8_FLIGHT | Estado real de flight task/hangar después de un tick; no prueba visual |
| M8_LAUNCH | Entrada y resultado de la inicialización real de misión |

Estos marcadores están instrumentados; NO fueron observados en ejecución
M8 en el visor. Audio: rutas originales y dependencias conservadas, pendiente
de comprobación audible. Frontend, hangar, launch, flight y simulación: código
original enlazado, recorrido completo pendiente de prueba.

## Validación y avisos

- Native build y enlace --no-undefined: correctos, sin warnings emitidos.
- Error inicial local AERON_MOUSE_LEFT corregido a AERON_MOUSE_BUTTON_LEFT.
- Desensamblado confirma las llamadas a wrappers de loop, adquisición,
  pacing y launch: evidence/wrapper-calls.txt y launch-hook.txt.
- Gradle: BUILD SUCCESSFUL, 32 tareas ejecutadas; firma APK válida.
- Avisos: AGP 8.1.4 probado hasta compileSdk 34 mientras se usa 35; tres
  warnings javac sobre source/target 8 y su aviso de supresión; nota de APIs
  deprecadas; AGP ignora la entrada riscv64 del metadata NDK. Sólo se incluye
  arm64-v8a. No se actualizaron toolchains ni se ocultaron avisos.
- Identidad y dependencias inspeccionadas dentro del APK; sin GameData,
  piloto neto ni bibliotecas de aplicaciones M6/M7/VRPROBE.

Reproducción local: ejecutar Build-Target.ps1 y, tras revisar su resultado,
Build-Apk.ps1. El segundo usa Gradle existente en modo offline, copia privada
de dependencias y clave debug; no invoca compilaciones de los baselines.

## Limitaciones y siguiente autorización

La imagen es monoscópica, llena la proyección de cada ojo y puede deformarse
por la relación de aspecto/FOV: no es un panel 3D con distancia física ni
geometría estéreo. No hay controles finales de vuelo. Rendimiento, gamma,
legibilidad, audio, navegación Touch y recorrido hasta misión siguen pendientes.
Sólo se admite salida SDR. Un error fatal o cierre de sesión solicitado por
el runtime puede terminar la aplicación; no hay auto-exit de observación y
no se implementó reanudación de una sesión XR detenida.

La aplicación espera una copia PRIVADA de los datos en
`/data/user/0/org.openxwa.xwaquest.m8/files/GameData`. No está aprovisionada
ni incluida en el APK. Preparar esos datos y un piloto M8 separado requerirá
una fase posterior; no se tocaron GameData ni neto durante este trabajo.

No hay confirmación visual M8. La instalación y prueba siguen pendientes de
la autorización separada del usuario para este APK exacto.

### Aprovisionamiento preparado (2026-09-25)

La estrategia, mecanismo seguro y pasos futuros están en [GAMEDATA.md](GAMEDATA.md).
No hubo cambios en código de aplicación ni en el APK. El aprovisionador sólo
se probó localmente con datos sintéticos; no se utilizó ADB. La copia privada
de assets M8 será escribible por M8: algunas rutas originales escriben en ASSET.
El origen M7B y su piloto permanecen separados.
