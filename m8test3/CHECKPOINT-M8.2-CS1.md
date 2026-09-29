# CHECKPOINT / HANDOFF M8.2-CS1 — 2026-09-25, antes de empaquetar

> ADDENDUM posterior: APK CS1 ya construido y auditado. Ver
> REPORTE-M8.2-CS1-APK.md para estado actual, cambios, hashes y evidencias.
> Tests CPU y build ARM64/APK PASS; NO instalado ni ejecutado en Quest.
> El texto siguiente conserva la fotografía histórica anterior al empaquetado.

Codex continúa como ÚNICO integrador. MiMo/Nemotron sólo análisis. Esta fotografía se
conserva en evidence/checkpoint-cs1-pre-apk.zip con SHA-256 por archivo. No restaurar
automáticamente: puede haber trabajo posterior. Arquitectura FIJA C / híbrida.
NO ADB, instalación, Quest ni GameData autorizados en esta etapa. No CS2.

## 1. Estado actual

A–D implementadas inicialmente: bridge propietario, composición de ojos, AeronScene
por ojo y selección 2D/immersive. E parcialmente completada: build nativo ARM64 OK.
CPU self-test Windows ejecutado y PASS. Ninguna prueba GPU/runtime nueva.
Falta revisar bordes/diagnósticos, ampliar comprobación CPU de orientación,
verificar enlaces, ejecutar Build-Apk.ps1 y verificar APK/package/hash. No llamar
PASS visual a un build. Entrega de esta tarea: BUILD_OK_NOT_RUNTIME_TESTED.
PASS de runtime/visual requiere autorización y prueba Quest futura separada.

## 2. Arquitectura implementada

m8/frame_bridge.c __wrap_XwaPort_Tick llama al tick real UNA vez y justo después a
VrFlightBridge_CaptureAfterTick(logical_frames). Lee XwaSnapshot_Current ya committed.
m8/vr_flight_bridge.c copia registros y nombres al buffer latest estático. No entrega
punteros de XWA ni OPT handles. Capacidad 1664, política CS1 limita a player+TIE.
__wrap_Mission_Init invalida antes de cargar y notifica resultado para mission_epoch.
__wrap_XwaRemaster_Frame sigue real; carga/sincroniza los meshes normales.
__wrap_Aeron_Present sigue componiendo el output 2D real antes del paso XR.
VrFlightRenderer_BeginFrame consulta mesh por basename DESPUÉS de remaster; exige
runtime_opt=1. Lo toma prestado sólo hasta EndFrame, sin sim/asset sync entre ojos.
Por ojo: XrView LOCAL -> VrEyeState -> VrEye_Compose(player MODEL, raw eye) ->
VrConvert_Camera -> AeronScene_Begin/AddMeshInstance/Render -> present chain SDR
RGBA8_SRGB -> VrBlit_Copy -> submit Aeron -> release XR -> xrEndFrame existente.
No imports V10 ni cambios a vr-probe. 2D sigue cuando CS1 no está listo/mesh no listo.

## 3. Archivos fuente nuevos (rutas exactas)

- C:\OpenXWA\XWAQuest\m8\vr_flight_bridge.h: VrObjectId, VrAssetIdentity,
  VrFlightObject, VrFlightSnapshot, flags/estado/API. Contrato sin recursos GPU.
- C:\OpenXWA\XWAQuest\m8\vr_flight_bridge.c: Reset, Begin/EndMissionLoad,
  CaptureAfterTick, GetLatest; copia/filtro/assets/matrices/revisión/logs.
- C:\OpenXWA\XWAQuest\m8\vr_eye_math.h: VrEyeState y funciones CPU inline
  VrEye_QMul, VrEye_Compose; ninguna escritura a simulación.
- C:\OpenXWA\XWAQuest\m8\vr_flight_renderer.h: API BeginFrame/RenderEye/
  EndFrame/Reset/ReadEye; frontera renderer separada del bridge.
- C:\OpenXWA\XWAQuest\m8\vr_flight_renderer.c: dos AeronScene, RTs SDR,
  préstamo temporal de mesh, draw del target con textura original sin iluminación
  PBR (base_color_emissive_strength=1), present chain y logs limitados.
- C:\OpenXWA\XWAQuest\m8\tests\eye_math_test.c: test real del header C usado
  en producción; identity/IPD/yaw vehículo+yaw cabeza/orden/no escrituras/invalid.

## 4. Archivos fuente modificados (rutas exactas)

- C:\OpenXWA\XWAQuest\m8\frame_bridge.c: includes nuevos; Reset al init;
  título M8.2-CS1; captura inmediata tras tick; wrapper Mission_Init; presentación
  por AeronCommandBuffer administrado (necesario para staging AeronScene), selección
  textura 2D/CS1 por ojo; reset en shutdown. Wrapper SDR anterior se conserva.
- C:\OpenXWA\XWAQuest\m8\xr_bridge.c: M8_XrLocateViews valida ambas flags de
  posición/orientación y count=2 sobre s_space LOCAL, mismo predictedDisplayTime.
- C:\OpenXWA\XWAQuest\m8\CMakeLists.txt: nuevas unidades bridge/renderer,
  reutiliza vr-probe/vr_convert.c sin modificarlo; --wrap=Mission_Init.
- C:\OpenXWA\XWAQuest\m8\app\build.gradle: versionCode 2,
  versionName 0.8.2-cs1; package preservado org.openxwa.xwaquest.m8.

No se han editado fuentes fuera de M8. No M6/M7/M7B/M7C/VRPROBE/hello_xr,
GameData, piloto ni motor. También se generaron/actualizaron artefactos normales
en m8/build-android, app/build/generated/engineJni, evidence/configure.log,
evidence/native-build.log y tests/eye_math_test.exe. Este checkpoint y su paquete
son documentación/evidencia nueva, no cambios al motor.

## 5. Build exacto

Directorio C:\OpenXWA\XWAQuest\m8; PowerShell: & ./Build-Target.ps1
Necesitó require_escalated para ejecutar CMake/NDK instalados fuera del workspace.
Resultado exit 0: 5 objetos C (incluyendo vr_convert) + link libOpenXWAM8.so.
Sin warnings/errores emitidos. Reutiliza todos los archives M2 y libaeron_vrprobe.a.
libOpenXWAM8.so actual: 27753384 bytes al momento de este checkpoint.

Self-test:
& ../m2/host/llvm-mingw-20260908-ucrt-x86_64/bin/clang.exe -std=c99 -Wall -Wextra -Werror tests/eye_math_test.c -o tests/eye_math_test.exe
& ./tests/eye_math_test.exe
PASS, exit 0; compilador host real, no mocks del motor.

No APK CS1 todavía. app/build/outputs/apk/debug/app-debug.apk sigue siendo M8.1 SDR:
16352569 bytes; SHA256 DF754EAC79D374575B92E4DA29A65FB322EE8481B170ED6715609AB0EA47393C.
Ese APK viejo NO contiene el nuevo bridge aunque Gradle source ya diga 0.8.2.
No instalación ni prueba Quest de CS1. No comandos ADB en esta tarea.

## 6. TIE IA y transformaciones

Fuente: snapshot.flight_objects. Player: buscar slot == flight_camera.player_obj_idx,
nunca indexar array por slot. Target: has_craft, OBJ_TIEFighter, misma render_region,
slot_class != OTHER, distinto player; conservar slot+signature anterior si válido,
en otro caso menor slot. Sólo CS1_READY con OBJ_XWing y ambos loaded OPT names
XWING/TIEFIGHTER. No fallback silencioso de otro tipo para anunciar CS1.
Pose: copia world_pos int32; orientación del helper ya existente
XwaRemasterFlight_ObjectModelMatrixAtOrigin. Origen: world_pos entero del player.
Helper resta en int64 antes de float. Multiplica las primeras 12 entradas por
1600/65536 para salida en metros; row-major, traslación [3,7,11].
OPT identity: lookup g_loadedModels.byObjectType local al bridge, búsqueda del
public_handle en snapshot.opt_assets; nombre copiado. Fallback del export existente
marcado fallback, no habilita CS1_READY. Nada de handles hacia renderer.

Cámara: el mundo/modelos permanecen en ejes XWA. B (vehicle-to-world) usa columnas
MODEL.col0, MODEL.col2, MODEL.col1, es decir derecha/up/back de la nave. La permutación
compensa la reflexión de MODEL OPT. Q15 se ortonormaliza sólo en la cámara; MODEL del
objeto no se altera. Pose final eye-world = B * pose-LOCAL; VrConvert aplica su D
al inverso del quaternion para world-to-eye Aeron. FOV simétrico de tangentes como
baseline; guarda cuatro ángulos raw. No recenter ni offset cockpit; posición LOCAL
absoluta del visor se toma tal cual. IPD en metros; MODEL no usa normalización V10.

## 7. Pendiente, en orden

1. vr_flight_renderer.c BeginFrame: diagnóstico de bajo ruido cuando la malla no está
   lista/no es runtime OPT; hoy retorna 2D silenciosamente. Revisar índices/flags.
2. vr_flight_bridge.c CaptureAfterTick: comprobar que content_revision cubra cambios
   de generación de assets/región; mejorar que POSE_UPDATED refleje pose y no sólo
   cambio de flags. Revisar rigor de marcadores de snapshot/player/target.
3. tests/eye_math_test.c: añadir pitch/roll y proyección/signos si es posible sin
   mocks del motor, reusar matemática real. Verificar handedness conforme helper.
4. Revisar lifecycle mesh/scene y gates de draw; AeronScenePbr sí registra draws
   de sus instancias sin culling CPU general. No afirmar fragmentos visibles.
5. Build-Target.ps1 incremental si hubo cambios; inspección símbolos --wrap y ELF.
6. Build-Apk.ps1 offline; aapt dump badging, SHA256/tamaño; reporte final CS1.

## 8. Riesgos conocidos (documentados, no resueltos por este checkpoint)

- Sin prueba GPU: AeronScene PBR/shaders/RTs en Quest aún no están demostrados por
  esta etapa. El éxito histórico V10 no demuestra AeronScene.
- FOV simétrico aproximado; cuatro ángulos raw conservados. LOCAL puede incluir
  altura/desplazamiento inicial del usuario, no hay recenter solicitado.
- Player MODEL tiene convención OPT/reflexión; B la transforma a rotación propia.
  Pruebas actuales pasan para matrices sintéticas, falta confirmación visual real.
- Mesh sólo prestado durante el frame; dueño XwaRemasterShip puede reemplazarlo
  en futuros CommitSyncBatch. BeginFrame vuelve a resolver cada frame; EndFrame
  suelta referencia. No usar el valor estático frame_mesh tras EndFrame.
- Single-thread; GetLatest expira con próxima captura/reset. No async permitido.
- Snapshot truncation se expone pero no se ha probado el límite. Sólo 2 objetos
  por política CS1 actual. No daños/articulación/efectos; intención del alcance.
- Bridge no vuelve a capturar si host usa PausedFrame; último snapshot se mantiene.
- Logs DRAW_RECORDED y FIRST_STEREO son recording/submission, no presentación visual.
- Renderer mantiene ejecución del remaster plano para conservar asset sync y
  retorno 2D. Doble coste de render, no doble simulación. Sin optimizaciones.
- El m8 package conserva datos; no se tocaron datos ni aprovisionador.

## 9. Git y preservación

Git root: C:/OpenXWA/XWAQuest. Branch: codex/m1-sdl-gpu-openxr.
NO HAY COMMITS: git log -1 devuelve "does not have any commits yet".
git status -uno vacío NO significa limpio: todo el árbol está untracked.
git status --short normal muestra ?? m8/ y los baselines/carpetas del proyecto.
git status --short --untracked-files=all -- m8 lista como ?? todos los sources M8,
scripts preexistentes, los nuevos archivos y el ejecutable de self-test.
No se hizo git add/commit/reset/checkout/clean/revert. No hay commit restaurable.
La lista de cambios de esta etapa es la de secciones 3/4, no todo el untracked.
El ZIP de checkpoint guarda los sources afectados, scripts build y logs junto
con manifiesto SHA-256; es la copia recuperable independiente de Git.

## 10. Siguiente acción exacta

Si otro integrador tuviera que continuar desde este punto, el siguiente cambio
concreto que debería realizar es: en m8/vr_flight_renderer.c, dentro de
VrFlightRenderer_BeginFrame, agregar un diagnóstico con memoria del motivo
mesh_missing / not_runtime_opt / invalid_mesh que se emita sólo al cambiar motivo
o identidad; mantener retorno a 2D, sin cargar un parser alternativo ni modificar
OpenXWA/Aeron. Luego revisar las dos condiciones de revision/pose del bridge
indicadas arriba, correr CPU self-test y empaquetar el APK, siempre sin ADB.
