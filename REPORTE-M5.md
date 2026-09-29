# REPORTE M5 — completado

Fecha: 2026-09-19. **M6 no iniciado.**

## Resultado

El runtime real ejecutó `XwaPort_Tick`, `XwaRemaster_Frame` y `Aeron_Present` después de ambas inicializaciones. Se obtuvieron imágenes auténticas tanto de las intros como del **frontend «Create a new pilot»**. No se utilizaron stubs, imágenes ficticias, sustituciones de renderer ni bypasses de validación.

La prueba final alcanzó más de **2.040 ticks y presentaciones correctas**. El siguiente límite funcional es la creación/selección de piloto y la interacción con el frontend; no se creó un piloto ni se inició una misión. No apareció un bloqueo fatal de plataforma con el visor despierto y la ventana enfocada. El cierre se produjo por el límite explícito M5 de 120 segundos, antes de avanzar a gameplay.

## Evidencia visual

Capturas PixelCopy de la superficie SDL/Vulkan real, 1280×800:

- `m5/evidence/game-frame.png`: logo original LucasArts, intro `logofinal.SNM`.
- `m5/evidence/game-frame20.png`: segunda intro.
- `m5/evidence/game-frame35.png`: STAR WARS sobre fondo estelar, `intro_final.SNM`.
- **`m5/evidence/frontend.png`: pantalla de creación de piloto, con fondo, texto y cursor originales.**

PixelCopy sólo lee la superficie existente; no genera contenido ni alimenta al renderer. Las capturas pertenecen a la ejecución final PID31926. La captura inicial `first-screen.png` del compositor mostraba passthrough/teclado, no el juego, y no se utilizó como evidencia de éxito; `screen-preview.jpg` es su vista reducida de diagnóstico.

## Ruta y subsistemas

`SDL_main -> Aeron_Init -> validación GameData -> XwaRemaster_Init -> XwaPort_Init -> bucle original -> XwaPort_Tick -> frontend bootstrap/movie task -> XwaRemaster_Frame -> Aeron_Present`.

El primer tick ejecutó la secuencia original `logofinal`, después `tgintro` y después `intro_final`. FFmpeg real abrió contenedor SMUSH, vídeo SANM 640×480/640×400 y audio ADPCM VIMA 22050 Hz estéreo. Se observaron contadores de frames decodificados/presentados y avance del reloj con foco=1.

Para alcanzar el frontend dentro de la ventana de observación se envió **un Escape** mediante ADB, utilizando la ruta original de omisión de intro. No se sustituyó la película, no se falseó su finalización y no se omitieron validaciones ni carga de recursos. El frontend cargó listas, fuentes ABP y CBM/DAT originales; el remaster registró **131 archivos, 3 grupos y 134 texturas** preparados/confirmados. La pantalla resultante solicita crear un piloto.

## Logs relevantes

Ejecución final Quest 3S, serie3487C10H9T0BKZ, PID31926, hilo SDL31961. Evidencia continua: `m5/evidence/runtime-live.txt`; filtrados: `run-focused.txt`, `final-process.txt`, `final-process-end.txt`.

```text
09:21:06.518 M5 PRESENT count=1 ok=1 error=none
09:21:06.585 M5 VIDEO state=2 decoded=9 presented=1 pos=47021 duration=7666667 focus=1
09:21:14.179 M5 VIDEO state=2 decoded=115 presented=115 pos=7640875 duration=7666667 focus=1
09:21:14.412 M5 MOVIE begin=tgintro
09:21:31.768 aeron.video: MOVIES/intro_final.SNM: smush, video=sanm 640x400, audio=adpcm_vima 22050 Hz/2 ch
09:22:29.935 xwa.remaster: 2D file 'concourse/create': source=original
09:22:29.935 xwa.remaster: frontend assets prepared: generation=2 files=131 groups=3 textures=134
09:23:01.924 M5 TICK end=2040 movie=0 quit=0
09:23:01.925 M5 PRESENT count=2040 ok=1 error=none
09:23:06.487 M5 STOP observation limit or flight boundary
09:23:06.561 M5 Aeron_Shutdown END
09:23:06.561 Finished main function
```

El límite registrado corresponde a tiempo: las muestras de estado mantienen `flight=0`. SDL cerró superficie y Activity. No se observó crash fatal en esta ejecución. No se afirma estabilidad prolongada, rendimiento definitivo ni ausencia de fugas.

## Cambios y reutilización

Todos los cambios están aislados en `C:/OpenXWA/XWAQuest/m5` y este reporte:

- `CMakeLists.txt`: genera la entrada desde el main original, elimina sólo el límite M4 anterior al bucle y añade límite de observación120000ms y guardia de estado de vuelo antes del siguiente tick. Conserva el bucle y las llamadas reales.
- `trace.c`: wrappers observacionales de ticks, frames, presentación, películas y estadísticas de vídeo. Mantiene los resultados reales.
- `app/src/main/java/org/openxwa/xwaquest/m5/RuntimeActivity.java`: SDLActivity, GameData privado y capturas diagnósticas PixelCopy.
- `Build-Target.ps1`, configuración Gradle, manifiesto, `.gitignore`, `ESTADO.md`, evidencias.

Se reutilizaron archivos estáticos y bibliotecas de M2, shaders/recursos aprobados y 7.652 archivos GameData mediante copia local **M4→M5 en el Quest**, manteniendo aislados ambos paquetes. No se volvió a copiar Steam ni se modificó su instalación. GameData M5: `getFilesDir()/GameData`, observado como `/data/user/0/org.openxwa.xwaquest.m5/files/GameData`.

No se recompilaron FFmpeg, SDL3, Aeron ni el motor completo. Sólo entrada/instrumentación, enlace y Java. No se alteraron M0–M4 ni fuentes OPT. El inventario de fuentes/APK M4 mantuvo sus hashes (`evidence/preservation.txt`).

## Corrección Android y riesgos pendientes

- La apertura automática del teclado de escritorio durante `Aeron_Init` podía quitar el foco. M5 establece **`SDL_HINT_ENABLE_SCREEN_KEYBOARD=0`** antes de inicializar Aeron. No se modificó SDL ni se fingió foco. La observación sin foco no bastaba para atribuir el estancamiento a un fallo del decodificador; con foco correcto se demostró avance normal.
- Dormir el visor pausa SDL legítimamente. Fue necesaria intervención física del usuario para mantenerlo despierto. No se eludió Guardian ni la política de suspensión.
- El teclado virtual no se abre automáticamente en esta ruta diagnóstica. La entrada de texto para crear piloto y los controles definitivos quedan sin validar; no se implementaron durante M5.
- Siguen los warnings Dx5 de estados D3DRENDERSTATE no reconocidos documentados en M4. No impidieron este frontend, pero no certifican el renderer de vuelo.
- El decodificador recortó audio más allá de la duración de las intros:156800 frames para logofinal y38400 para tgintro. Las películas avanzaron; no se certifica sincronización/calidad de audio completa.
- Las búsquedas de overrides/remaster/subtítulos ausentes y de archivos en raíz USER pueden fallar antes de encontrar la copia original en ASSET. Los logs reflejan los fallos y el fallback real; no se fabricaron archivos.
- Persisten warnings Android/AGP ya conocidos. El riesgo de limpieza de recursos de M4 no se considera resuelto por esta prueba corta.
- No se implementó estéreo XR, head tracking, controles Quest, gameplay ni optimizaciones. La presentación observada es el panel Android con backend SDL_GPU/Vulkan existente.

## Build, APK y reproducción

Build inicial exitoso1m26s; build con captura/teclado exitoso1m4s, 8 tareas ejecutadas y25 actualizadas. Instalación `Success`.

APK: `m5/app/build/outputs/apk/debug/app-debug.apk`.
SHA-256 local e instalado:
`63025A9ED4D9819A8047B156DE3D7CFE9053D3D20F9B5B5624B4808BDDCE55B7`.

Desde XWAQuest, si hace falta reconstruir: `gradlew.bat -p m5 :app:assembleDebug --console=plain`. La instalación actual ya contiene GameData; no repetir copia.

Arranque frío:
`adb shell am start -S -n org.openxwa.xwaquest.m5/.RuntimeActivity`.
Mantener visor puesto y panel enfocado. La ruta se cierra automáticamente tras120seg; Escape permite omitir la intro por el mecanismo original. No activar misiones. Capturas de la superficie se guardan en `files/m5-frame-N.png` para N=2,5,10,20,35,60,100.

Para un dispositivo limpio, después de instalar M5 y teniendo M4 con sus datos:

```sh
adb shell run-as org.openxwa.xwaquest.m5 mkdir -p files/GameData
adb shell 'run-as org.openxwa.xwaquest.m4 tar -cf - -C files/GameData . | run-as org.openxwa.xwaquest.m5 tar -xf - -C files/GameData'
```

Son datos privados separados; las capturas y GameData no forman parte de los assets del APK. El empaquetado sólo toma shaders/recursos propios aprobados de M2.

## Aceptación

**M5 cumplido, incluido el objetivo visual prioritario:** ticks reales, presentación real y frontend reconocible capturado desde la superficie del juego. Estado final: creación de piloto, esperando interacción. Cierre controlado por alcance; ninguna misión. Se detiene para revisión y autorización explícita antes de M6.
