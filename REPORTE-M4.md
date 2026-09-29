# REPORTE M4 — completado

Fecha: 2026-09-19. **M5 no iniciado.**

## Resultado y punto final

XWAQuest ejecutado en Quest 3S localizó y validó los datos originales de Steam. La ruta real avanzó más allá de M3: `XwaSetup_ValidateGameData` retornó **1**, `XwaRemaster_Init` retornó **1** y **`XwaPort_Init` retornó 1**.

La ejecución terminó deliberadamente inmediatamente después de `XwaPort_Init`, antes del primer tick y del bucle del juego. Se llamó a `XwaRemaster_Shutdown`, `XwaPort_Shutdown` y `Aeron_Shutdown`; SDL terminó main y destruyó la Activity. No apareció un nuevo bloqueo fatal en esta secuencia. No se ejecutó una misión ni se implementaron gameplay, controles VR o presentación OpenXR.

## Datos y estructura

Origen, utilizado sólo para lectura:
`C:/Program Files (x86)/Steam/steamapps/common/Star Wars X-Wing Alliance`.

Destino final confirmado por Android:
`/data/user/0/org.openxwa.xwaquest.m4/files/GameData`.

La aplicación calcula la ruta con `new File(getFilesDir(), "GameData")`; no contiene rutas absolutas específicas del Quest ni requiere permisos de almacenamiento. El procedimiento ADB opera sobre `files/GameData` mediante el UID de la aplicación.

Se copiaron **7.652 archivos / 757.214.582 bytes**, conservando nombres y estructura:

- Archivos raíz `.txt`, `.tab`, `.lst`, `.dat`, `.abp` existentes; esta instalación aporta archivos TXT y ABP.
- Directorios completos `FLIGHTMODELS`, `FRONTRES`, `MISSIONS`, `MOVIES`, `RESDATA`, `WAVE`, `SFX`. No existe `FONTS` separado en esta instalación.
- Se excluyeron ejecutables, DLL, instaladores y partidas; tampoco se copió el `config.cfg` de Windows. La instalación de Steam no fue modificada.

El inventario exacto está en `m4/evidence/data-inventory.csv`. El conteo remoto coincidió y se verificaron por SHA-256 diez archivos críticos, incluidos los seis del validador y los archivos de strings/fuentes: `data-hashes.csv`.

Validación original, en orden, **todos `found=1`**:

1. `RESDATA.TXT`.
2. `FLIGHTMODELS/SPACECRAFT0.LST`.
3. `MISSIONS/MISSION.LST`.
4. `MOVIES/PROLOGUE.SNM`.
5. `MOVIES/BATTLE1.SNM`.
6. `WAVE/FRONTEND/B1M1/N010101.WAV`.

El validador comprueba existencia mediante VFS; no afirma validar semánticamente todo el contenido de esos seis archivos. Después, `XwaPort_Init` abrió el catálogo `Resdata.txt`, sus **38 archivos DAT**, `strings.txt` y fuentes `times20.abp`, `times10.abp`, `times12.abp`, `times15.abp`. La inicialización real del catálogo, strings y frontend bootstrap terminó sin impedir el retorno exitoso del port.

Casos reales de compatibilidad: `Battle1.snm` y `B1m1/N010101.wav` en disco satisfacen solicitudes `BATTLE1.SNM` y `B1M1/N010101.WAV`; `RESDATA.TXT` responde a `Resdata.txt`, y `Resdata\*.dat` funciona contra `RESDATA`. Se conservó la resolución existente del VFS sin distinguir mayúsculas y su normalización de separadores.

## Archivos creados y cambios

Sólo se trabajó en `C:/OpenXWA/XWAQuest/m4` y este reporte:

- `CMakeLists.txt`, `trace.c`: entrada real generada desde `src/xwa_app/main.c`, instrumentación observacional y límite después de `XwaPort_Init`. Los wrappers llaman a las funciones reales y conservan resultados; no hay stubs ni bypass de validación.
- `Build-Target.ps1`: reutilización de los archivos estáticos, bibliotecas y shaders M2.
- `settings.gradle`, `build.gradle`, `gradle.properties`, `local.properties`, `app/build.gradle`, `app/src/main/AndroidManifest.xml`: proyecto independiente `org.openxwa.xwaquest.m4`.
- `app/src/main/java/org/openxwa/xwaquest/m4/RuntimeActivity.java`: SDLActivity real, ruta privada y argumento `--game-data`.
- `Install-GameData.ps1`: selección, inventario, transferencia con UID de la aplicación, conteo y verificación de hashes de los seis archivos del validador.
- `.gitignore`, `ESTADO.md`, `evidence/`: exclusión de staging comercial y evidencias.

No se modificaron fuentes originales OpenXWA/Aeron/SDL, las correcciones OPT, M0, hello_xr ni los proyectos M1–M3. Los inventarios de preservación M1/M2/M3/OPT conservaron todos sus hashes (`evidence/preservation.txt`).

## Build, instalación y reproducción

Build inicial: **BUILD SUCCESSFUL in 1m 8s**; sólo cuatro unidades C de la entrada y enlace `libOpenXWAM4.so`. Cambio final de ruta Java: **BUILD SUCCESSFUL in 7s**, 5 tareas ejecutadas y 28 actualizadas. No se reconstruyeron FFmpeg, SDL3, Aeron ni el motor completo.

APK instalado: `m4/app/build/outputs/apk/debug/app-debug.apk`, **13.273.235 bytes**. SHA-256 local e instalado:
`E2957925255DE91BEFCF0003F6B52ABE08124427D2483398D8AE396F6A438DFB`.

Los **87 assets del APK coinciden byte por byte con M2** (`apk-assets.txt`). Los datos comerciales sólo están en el almacenamiento de la aplicación, nunca dentro del APK.

Desde `C:/OpenXWA/XWAQuest`, con JDK/SDK de M2 y Quest conectado:

```powershell
.\gradlew.bat -p m4 :app:assembleDebug --console=plain
& "$env:LOCALAPPDATA/Android/Sdk/platform-tools/adb.exe" install -r .\m4\app\build\outputs\apk\debug\app-debug.apk
.\m4\Install-GameData.ps1 -Source 'C:\Program Files (x86)\Steam\steamapps\common\Star Wars X-Wing Alliance'
& "$env:LOCALAPPDATA/Android/Sdk/platform-tools/adb.exe" shell am start -S -n org.openxwa.xwaquest.m4/.RuntimeActivity
```

El script prepara un tar local ignorado por Git, lo transfiere a `/data/local/tmp/xwaquest-m4-import.tar` y ejecuta una tubería local `cat | run-as ... tar` hacia `files/GameData`. Comprueba conteo/hashes y retira temporales. Requiere este APK debug con `run-as`; no es todavía un importador para distribución. La técnica final se validó en Quest; durante la transferencia completa el tar se había depositado en la ruta externa del intento previo, y se utilizó la misma tubería local hacia el almacenamiento privado.

Desinstalar la aplicación o borrar sus datos elimina GameData privado. `adb install -r` conserva esos datos. No hace falta repetir la copia en la instalación actual.

## Evidencia de ejecución

Quest 3S `3487C10H9T0BKZ`, PID **25355**, hilo SDL **25391**. Log completo: `m4/evidence/final-process.txt`.

```text
04:41:28.133 M4 GameData validation END ok=1 detail=/data/user/0/org.openxwa.xwaquest.m4/files/GameData
04:41:28.141 M4 XwaRemaster_Init END ok=1
04:41:28.141 M4 XwaPort_Init BEGIN
04:41:28.142 M4 VFS open root=0 path=Resdata.txt mode=0 ok=1 error=none
04:41:28.144 M4 VFS open root=0 path=strings.txt mode=0 ok=1 error=none
04:41:28.179 M4 VFS open root=0 path=times15.abp mode=0 ok=1 error=none
04:41:28.180 M4 XwaPort_Init END ok=1 error=none
04:41:28.180 M4 STOP XwaPort_Init succeeded; no game loop authorized
04:41:28.257 M4 Aeron_Shutdown END
04:41:28.258 Finished main function
```

SDL/Aeron/Vulkan, audio básico, enumeración de controladores y VFS arrancaron en la ruta normal. Los recursos propios `remaster/opt_alpha_overrides.yaml`, `aeron/scene3d_defaults.yaml` y `remaster/config.yaml` se abrieron desde el APK. El remaster configuró HDR y la política de recursos; esto no demuestra renderización del juego ni XR. El frontend bootstrap se inicializó dentro del port, sin ejecutar su bucle.

## Problemas resueltos y riesgos

- **Almacenamiento Android:** `adb push` recursivo falló al crear subdirectorios en almacenamiento emulado; `run-as` tampoco pudo leer allí el tar. Se cambió únicamente M4 a almacenamiento privado y extracción mediante tubería local shell→UID de aplicación. Se retiraron tar y copias parciales externas.
- **Transferencia binaria host:** un intento por stdin terminó con `bad header` y otro produjo extracción parcial tras reinicio/desconexión del servidor ADB. No se aceptaron esos resultados. La extracción local final dio 7.652 archivos y hashes coincidentes. El script final comprueba contenido antes de declarar éxito.
- **Lifecycle:** el visor se durmió y SDL pausó la inicialización. La prueba final se ejecutó con el usuario manteniéndolo despierto. No se eludieron protecciones de Horizon/Guardian.
- `xwa.tab` no existe en esta instalación legítima. `Linez_LoadDict` contempla archivo ausente y el código real continuó; no se creó un archivo ficticio. El `config.cfg` privado también falta: `Config_Load` establece valores iniciales antes de intentar abrirlo. Ambas ausencias quedaron registradas sin bloquear el port.
- Aeron compat reporta `Unexpected D3DRENDERSTATE token` para 4, 31, 32, 5, 6, 33, 9, 29, 8, 26, 2, 7 y 22. Son avisos de compatibilidad del renderer heredado, no de rutas; no se ocultaron ni se amplió M4 para implementar esos estados.
- Persisten los avisos Android de icono no soportado y sondeos de formatos Gralloc observados en M3. Además aparece `A resource failed to call release` 30 segundos después del cierre, sin stack que identifique el recurso. Queda como riesgo de limpieza, sin afirmar ausencia de fugas.
- No hubo crash fatal ni excepción fatal en la ejecución final; el cierre del runtime/Activity está documentado. El proceso Android puede permanecer en caché.
- No se validó gameplay, misiones, recursos de vuelo adicionales ni renderización VR. La selección transferida es suficiente para el arranque demostrado, no una certificación de cobertura total del juego.

## Aceptación

**M4 cumplido:** datos reales localizados en Quest, seis validaciones originales superadas, catálogo/strings/fuentes abiertos por el VFS real, remaster y `XwaPort_Init` exitosos, APK sin assets comerciales, sin stubs/bypasses y milestones previos preservados. La ejecución se detuvo en el límite autorizado anterior al bucle. Se espera revisión y autorización explícita antes de M5.
