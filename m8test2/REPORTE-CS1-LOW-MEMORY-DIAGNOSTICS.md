# M8.2-CS1 — instrumentación LOW_MEMORY

Estado: BUILD_OK_NOT_RUNTIME_TESTED. Sólo diagnóstico. No ADB, instalación ni
ejecución Quest en esta etapa. No cambios al renderer CS1, FOV, coordenadas,
escala, clipping, transforms, sincronización, GameData o pilotos.

## Archivos de esta etapa

Raíz de todas las rutas: `C:\OpenXWA\XWAQuest\m8\`.

- Nuevos `diagnostics.c`, `diagnostics.h`: lectura de memoria, log privado,
  wrappers de transiciones, imágenes, listas, película y asignaciones grandes.
- `CMakeLists.txt`: unidad diagnostics.c y --wrap de CombatSimMenu_Update/Exit,
  MissionSetup_Update/Exit, Movie_Play, FrontImage_RegisterResourceDefault,
  FrontImage_FreeResourceByName, FrontImage_LoadResourceList y Mem_Alloc.
- `frame_bridge.c`: checkpoints inicio; muestra cada 5 segundos desde el hilo
  host; estados de presentación al cambiar y resultado real de xrEndFrame si
  no es XR_SUCCESS. Conserva llamadas, condiciones y orden de presentación.
- `input_xr.c`: checkpoints de entrada y retorno con nombre del prompt de piloto.
- `xr_bridge.c`: resultado/count/flags de xrLocateViews cuando cambian
  validez o resultado. Conserva exactamente el criterio de aceptación anterior.
- `tests/Verify-APK.ps1`: exige marcadores diagnósticos además de CS1.
- Este reporte. Artefactos normales de build/tests y evidencias dentro de m8.

No cambios de fuentes fuera de m8. Hashes de los siete archivos de código/script:
`evidence/diagnostic-source-hashes.csv`.

## Instrumentación

Cada registro incluye tiempo monotónico ms. RSS, VmSize y VmHWM proceden de
/proc/self/status; PSS de /proc/self/smaps_rollup sólo en checkpoints completos
y muestras periódicas. -1 significa no disponible o no muestreado; no cero.
mallinfo informa heap usado, arena y mmap en bytes: no equivale a memoria GPU
ni a toda la memoria del proceso. Datos existentes leídos sin modificarlos:
g_resourceCount, conteo/bytes de handles, opt_asset_count, flight_object_count,
AeronRenderDataStats (upload reservado/staged, chunks y copias de textura).
Los últimos son contadores del frame, NO inventario de recursos GPU vivos.

- M8_MEM_APP_START: antes de Aeron_Init real y después del inicio XR.
- M8_MEM_PILOT: entrada al prompt y retorno con nombre; no afirma que el archivo
  del piloto ya esté creado.
- M8_MEM_COMBAT_SIM: antes/después del primer Update y de Exit.
- M8_MEM_SINGLE_PLAYER_ENTER: antes del primer MissionSetup_Update.
- M8_MEM_SINGLE_PLAYER_READY: retorno de Update sin iniciar película en esa
  invocación; la primera llamada que inicia pod no se anuncia lista.
  Es evidencia de retorno del código, no confirmación visual de menú.
- M8_MEM_SKIRMISH_ENTER: observación del cambio a missionDirectoryId=SKIRMISH
  antes/después de Update; no equivale a inicio de misión. Si la asignación y
  carga suceden dentro del mismo Update, se observa al retornar.
- M8_MEM_SETUP_EXIT: antes/después de liberaciones de MissionSetup.
- M8_RESOURCE y M8_MEM_RESOURCE: nombre/ruta y memoria antes/después de carga y
  liberación. Listas de recursos llevan checkpoints completos antes/después.
- M8_MEM_MOVIE: alrededor de Movie_Play, incluido pod. El retorno puede significar
  inicio de una tarea de película, no fin de decodificación.
- M8_LARGE_ALLOC / M8_MEM_LARGE_ALLOC: Mem_Alloc >=8 MiB, tamaño solicitado,
  dirección de retorno del llamador y memoria antes/después. No intercepta malloc
  de FFmpeg/SDL ni cambia asignaciones. No pretende cubrir todo el heap.

Wrappers sólo interceptan referencias enlazables externas; no se reconstruyó el
motor para interceptar llamadas internas de la misma unidad. Las listas llevan
su propia medición aunque sus registros internos no atraviesen cada wrapper.
Los conteos antes/después de Exit y de entrada siguiente permiten comprobar si
se liberaron recursos anteriores, sin inferirlo sólo por nombres de checkpoints.

Logs a SDL/logcat y, mediante write append sin buffering stdio, a:
`/data/user/0/org.openxwa.xwaquest.m8/files/m8-cs1-diagnostics.log`.
No se escribe dentro de GameData. Archivo acumulativo; el marcador APP_START
delimita ejecuciones. No fsync por evento: se busca sobrevivir muerte del proceso,
no garantizar persistencia ante corte eléctrico. Si no abre, se emite
M8_DIAG_FILE_UNAVAILABLE y sigue logcat. Memoria/IO diagnóstico tiene coste;
no se considera una optimización ni una prueba de rendimiento.

Telemetría UI:
M8_UI_FRAME_STATE registra shouldRender, ready de output, validez de bridge,
views_valid (-1=no consultadas), immersive y layers solicitadas sólo al cambiar
el conjunto de estados. Frontend con bridge_valid=0 es normal.
M8_UI_LOCATE_VIEWS registra resultado/flags/count por cambio.
M8_UI_XR_END_RESULT conserva resultado real anómalo; M8_UI_END_FRAME_FAILED
señala el fallo del helper. Cero layers se informa, no se corrige ni oculta.

## Validación local

`./tests/Run-CPU-Tests.ps1`: eye-math PASS, VrConvert_SelfTest 7/7 PASS.
`./Build-Target.ps1`: PASS, cuatro objetos nuevos/recompilados y enlace ARM64;
sin warnings/errores nativos. Dependencias/motor reutilizados.
`./Build-Apk.ps1`: BUILD SUCCESSFUL, 27 s.
Warnings conocidos: AGP 8.1.4/compileSdk 35 y metadata riscv64 ignorada.
`./tests/Verify-APK.ps1`: APK_STATIC_AUDIT PASS.

El disassembly confirma llamadas a wrappers de recursos, Movie_Play y Mem_Alloc.
Las relocaciones GLOB_DAT confirman callbacks __wrap_CombatSimMenu_Update/Exit
y __wrap_MissionSetup_Update/Exit. Evidencia:
`evidence/diagnostic-call-sites.log`, `evidence/diagnostic-callback-relocations.log`.

APK: `C:\OpenXWA\XWAQuest\m8\app\build\outputs\apk\debug\app-debug.apk`
Package org.openxwa.xwaquest.m8, versionName 0.8.2-cs1, versionCode 2,
ABI arm64-v8a; 16.376.311 bytes. La versión se conserva; el hash distingue este
APK diagnóstico del anterior.

SHA256 APK:
`DBAD92295137854EAC72E0603C6D1953E33C03FFF6CE40E88C25EEC8E6325814`

Biblioteca extraída idéntica al build actual después de strip:
`F160AF1A0F2D5EFB7BFAF280C62CA319536A0F7033DF4B08BCC69B8F885A1481`.
Auditoría: `evidence/apk-cs1-audit/`, incluyendo diagnostic-markers.txt.

## Siguiente prueba — pendiente de autorización

1. En laptop, verificar el hash anterior e instalar este APK con adb install -r;
   verificar package/version. No borrar datos ni pilotos. Esta etapa no lo ejecutó.
2. Guardar logcat anterior y preparar captura continua de todos los buffers con
   timestamps antes del lanzamiento. Preparar en laptop cualquier permiso necesario.
3. Lanzar org.openxwa.xwaquest.m8 una sola vez. Entregar entonces la prueba al usuario.
4. En Quest: abrir frontend; si m8test ya existe, conservarlo y seleccionarlo.
   Para repetir específicamente creación, usar un nombre nuevo como m8diag vía
   X → nombre → A, sin sobrescribir m8test. Anotar qué ruta se realizó.
5. Entrar Combat Simulator → Single Player; esperar transición y seleccionar
   Skirmish si está disponible. No lanzar una misión. Anotar último menú, flicker
   y hora aproximada; si se cierra, no relanzar.
6. Tras confirmar que el usuario puede quitarse el visor: guardar captura y
   consultar exit-info. Recuperar el log privado mediante
   `adb exec-out run-as org.openxwa.xwaquest.m8 cat files/m8-cs1-diagnostics.log`.
   Correlacionar último checkpoint, carga pendiente y variación RSS/PSS/heap.
   No corregir nada automáticamente como parte de esa prueba.

La causa concreta del LOW_MEMORY sigue pendiente de mediciones reales.
