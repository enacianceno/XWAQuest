> PRUEBA FÍSICA COMPLETADA 2026-09-26, PID14829. Esta actualización prevalece sobre el estado pendiente inferior. Captura detenida correctamente (host adb PID1140); archivo completo quest-test3-20260926-142159-logcat.log preservado. No se cerró app por ADB, no hubo cambios de código/build/GameData.
>
> Resultado físico: COCKPIT PASS; HEAD TRACKING PASS para mirar alrededor; TIE PASS; COMBAT/DAMAGE PASS; AUDIO PASS; STARFIELD FAIL (no visible); FLIGHT CONTROLS FAIL (sin implementación de vuelo); FRONTEND FAIL (cerca/head-locked/flicker); PREFLIGHT SCREEN PASS visual, ruta exacta UNKNOWN; FLICKER FAIL también en Flight; MEMORY PARTIAL (sin fuga catastrófica, no prueba prolongada completa).
>
> Player confirmado slot0/object_type1/model_index0/craft Xwing; cockpit XWINGCockpit, indices1203 por ojo. TIE slot1/signature3/indices540; object_type5 sustentado por selector de código, model_index no registrado. Stereo empezó14:27:36.066. Starfield Prepare registrado, visibilidad NO demostrada. Flight FPS medio34.39 frente72Hz, rango28–37, fuerte Stale/Early. No alternancia inmersiva repetida ni fallos XR registrados. Cleanup dummy_null0 mantenido; WSI adicional existe pero causalidad con flicker no probada. M8_SHUTDOWN14:28:36.519, presented18318, sin LOW_MEMORY observado.
>
> Análisis completo y propuestas NO implementadas: `C:\OpenXWA\XWAQuest\m8test3\evidence\ANALISIS-PRUEBA-PID14829.md`. Siguiente acción: esperar autorización para diagnóstico focalizado de timing/WSI/PSS y starfield; no promover m8test3 ni declarar completamente jugable. Pantalla pre-flight world-fixed reportada por usuario, no se encontró ruta UI espacial distinta: retención/reproyección durante carga es hipótesis, no conclusión.
> ACTUALIZACIÓN 2026-09-26 14:22: GameData VERIFIED. Esta nota sustituye el estado operativo pendiente/desconectado del corte histórico inferior. Package instalado confirmado: org.openxwa.xwaquest.m8test3, 0.8.2-test3, code4, arm64-v8a. La copia remota anterior había terminado: final GameData existe y manifest/source-after/destination/final coinciden. Verificación NUEVA SHA-256 de los 7744 assets seleccionados en origen y destino PASS; destino total7744 archivos/847224595bytes, igual manifiesto fuente. RESDATA.TXT, XWING.OPT, XWINGCOCKPIT.OPT y TIEFIGHTER.OPT presentes y hashes iguales. Sin .plt/.bak ni archivos especiales en destino. No fue necesario copiar, borrar, reinstalar ni modificar código. m8test2 sólo leído. Evidencia: evidence/recovery-20260926-142120/verification.log y verify-existing.sh.
>
> LOGCAT READY: captura persistente PID1140, evidence/quest-test3-20260926-142159-logcat.log; metadata quest-test3-20260926-142159-session.json. Buffers anteriores preservados en -preclear.log. NO se lanzó la aplicación en esta tarea. Próximo paso: esperar autorización explícita para lanzamiento. Se encontró previamente m8-cs1-diagnostics.log con APP_START: no atribuir ese arranque al integrador ni afirmar que el package nunca fue abierto; esta tarea no lo abrió.
# CHECKPOINT M8TEST3 — fuente de relevo

Fecha de recuperación: 2026-09-26. Integrador: Codex. No se cambió código ni se recompiló durante instalación/recuperación. Este documento describe el estado, no autoriza nuevas implementaciones.

## 1. Estado operativo exacto y límites

**APK instalado, GameData NO verificado, aplicación NO lanzada.** El intento de aprovisionamiento terminó con código 1. Su único marcador persistido es `M8_DATA_SOURCE_VALIDATED assets=7744 bytes=847224595 manifest=manifest.tsv`. No emitió `M8_DATA_HASHES_OK` ni `M8_DATA_READY`. Al recuperar, `adb devices -l` devuelve lista vacía. No es posible afirmar cuántos archivos quedaron en staging, si el worker remoto continuó o si alcanzó la promoción. No borrar ni sobrescribir nada para averiguarlo.

El checkpoint no existía al recuperar y se creó en esta sesión. La última instrucción del usuario **prohíbe lanzar la aplicación**, incluso después de verificar GameData. Al concluir la recuperación, detenerse y esperar nuevas instrucciones. La autorización anterior de LOGCAT→LAUNCH no prevalece sobre esta restricción posterior.

Objetivo: validar cockpit original, estrellas, TIE real y seguimiento de cabeza independiente dentro del mundo VR. Arquitectura fija Opción C/híbrida: XWA/OpenXWA mantiene simulación, misión, IA, assets y audio reales; el adaptador copia estado committed; el renderer por ojo consume esa copia y las mallas OPT reales. No introducir simulación ficticia.

- `C:\OpenXWA\XWAQuest\m8`: oficial, NO modificar hasta promoción autorizada.
- `C:\OpenXWA\XWAQuest\m8test2`: baseline física estable; NO modificar.
- `C:\OpenXWA\XWAQuest\m8test3`: variante experimental activa, candidata a siguiente base oficial, NO desechable.
- Preservar M0–M7, M7B/M7C, `vr-probe`, `hello_xr`, GameData origen, pilotos y correcciones OPT. Se reutilizan bibliotecas M2 y vr-probe sin reconstruirlas.
- No implementar controles, cambios de cockpit/escala/FOV, sincronización, memoria, frontend, auto-setup ni explosiones durante esta prueba.

## 2. Identidad exacta

| Campo | Valor |
|---|---|
| Package | org.openxwa.xwaquest.m8test3 |
| versionName | 0.8.2-test3 |
| versionCode | 4 |
| ABI instalada | arm64-v8a |
| APK | C:\OpenXWA\XWAQuest\m8test3\app\build\outputs\apk\debug\app-debug.apk |
| SHA-256 esperado y real comprobado antes de instalar | 3C43C3D5D6E75CB80134CC6C4422E07914A3D13E771AC05D0158960ED6F31175 |
| Tamaño auditado | 13.299.765 bytes |
| libOpenXWAM8.so empaquetada/stripped | 7151295A020A176FB3F5A22B4DEFA12EC990D8605260D61FD72D32FF06144429 |

`adb install -r` devolvió `Success`. `dumpsys package` confirmó versión/código/ABI anteriores y codePath `/data/app/~~G4Orqi-td4qC1wmmw3FmvQ==/org.openxwa.xwaquest.m8test3-TJ-Fo3ptFyUzfLReqgykgQ==`. No asumir esta ruta si Android reinstala/reubica posteriormente.

## 3. Archivos de implementación diferentes de m8test2

Todas las rutas de esta tabla están bajo **C:\OpenXWA\XWAQuest\m8test3\**. Lista histórica comprobable: `evidence/changed-vs-m8test2.txt`; hashes: `evidence/t3-changed-files-sha256.csv`. No confundir archivos copiados sin cambios con implementación nueva.

| Ruta relativa | Funciones/helpers/cambio, propósito y riesgo |
|---|---|
| Build-Apk.ps1 | Cache `.gradle-t3`; build independiente. No ejecutar ahora. |
| Build-Target.ps1 | Directorio `build-android-t3`, no reutiliza CMakeCache de test2. |
| CMakeLists.txt | Añade `--wrap=Skirmish_GenerateMission`; reutiliza motor/dependencias completos. |
| diagnostics.c | `__wrap_Skirmish_GenerateMission`: logs begin/end alrededor de función real; no modifica resultado. |
| frame_bridge.c | Título de ventana test3; workaround de memoria test2 intacto. |
| provision-gamedata.sh | SRC=m8test2, DST=m8test3; staging/manifest/selección/hashes originales. Mensajes antiguos M7/M8 no cambian los packages reales. No admite reintento ciego con staging existente. |
| settings.gradle | Nombre proyecto XWAQuestM8T3. |
| app/build.gradle | Namespace/applicationId test3, versión 4/0.8.2-test3. |
| app/src/main/AndroidManifest.xml | Label XWAQuest M8test3. |
| app/src/main/java/org/openxwa/xwaquest/m8test2/M8Activity.java | Package Java m8test3 y tag propio; mantiene GameData privado. Directorio físico heredado no determina package. |
| vr_flight_bridge.h | ABI privada 2; `cockpit_valid`, `cockpit_variant`, `death_star_mode`, `cockpit_opt[40]`, `cockpit_to_local_m[16]`. |
| vr_flight_bridge.c | `VrFlightBridge_CaptureAfterTick`: copia cockpit seat0 desde snapshot; `M8T3_PLAYER`; no escribe pose del visor en simulación. |
| vr_flight_renderer.c | `BeginFrame` lookup runtime OPT cockpit; `RenderEye` cockpit+estrellas; `Reset` libera recursos propios. TIE, FOV, conversión y CULL_BACK preservados. Visual/coherencia todavía pendientes. |
| t3_features.h (nuevo) | `M8T3_COCKPIT=1`, `M8T3_STARS=1`; `M8T3_FLIGHT_CONTROLS=0`, `M8T3_SPATIAL_MENU=0`, `M8T3_AUTO_SETUP=0`. Los flags apagados NO implementan esas funciones al ponerlos en 1. |
| t3_seat_math.h (nuevo) | `T3_SeatMatrix`, fórmula seat0 sin pose física. Riesgo visual de anclaje/hardpoint por validar. |
| tests/seat_math_test.c (nuevo) | Signo, unidades, pan/16, independencia de head e invalidez NaN. |
| tests/Run-CPU-Tests.ps1 | Añade seat math a tests existentes. |
| tests/Verify-APK.ps1 | Identidad test3, biblioteca actual y marcadores nuevos/heredados. |

Documentación/evidencia añadida: `REPORTE-M8TEST3.md`, este checkpoint, logs de build/tests, auditoría APK, inventarios hash; `evidence/gamedata/quest-20260926-013618/selected-manifest-worker.sh` es un artefacto concatenado de los helpers existentes, NO un cambio de código. En la recuperación sólo se escribe documentación/evidencia.

## 4. Pipeline, contratos y lifetime

`frame_bridge.c::__wrap_XwaPort_Tick` impide más de un tick por `logical_frames`, llama `__real_XwaPort_Tick(delta)` y después `VrFlightBridge_CaptureAfterTick(logical_frames)`. Ruta interna establecida: `XwaFlightTask_Tick → XwaSnapshot_CaptureFlight → XwaSnapshot_Commit → XwaSnapshot_Current`.

`__wrap_Mission_Init` invalida el bridge antes de cargar, delega la función real y publica nueva epoch si retorna éxito. `__wrap_XwaRemaster_Frame` llama el renderer real y sincroniza los assets a través de su ruta existente. En `__wrap_Aeron_Present`, `VrFlightRenderer_BeginFrame` resuelve meshes; `RenderEye` compone una cámara por ojo, dibuja y produce SDR; `VrBlit_Copy` copia a swapchains y OpenXR presenta.

`VrFlightSnapshot` contiene ABI/state/flags, host_frame_id/mission_epoch/tick_index, game_time_ms, opt_asset_generation/content_revision/source_dropped_records, origin_xwa, región, índices de player/target, cockpit y objetos. `VrFlightObject`: slot+signature, object_type/genus/render_region, world_xwa int32[3], matrix float[16], identidad/resolución OPT. Capacidad contrato 1664; política CS1 copia **máximo dos**, player y un TIE. No añadir multi-object ahora.

Sólo hilo host: `GetLatest` devuelve almacenamiento estático prestado hasta siguiente mutación. Copia por valor de simulación; meshes prestados entre BeginFrame/EndFrame, sin asset sync o tick intermedio. Reset libera escenas, targets, chain, sampler y estrellas; no destruye meshes prestados. No usar estos punteros desde otro hilo.

## 5. Player, TIE y assets

Player = `flight_objects[i].slot == flight_camera.player_obj_idx`; no elegir primer objeto por intuición. Origen local = posición int32 del player cada tick (traslada, no rota).

TIE = distinto del player, `has_craft`, `object_type==OBJ_TIEFighter`, misma render_region, slot_class distinto de OTHER. Conserva slot+signature anterior si existe; si no, menor slot válido. Posición/orientación son las del snapshot real. Ready CS1 exige player OBJ_XWing, dos objetos, assets LOADED_OPT y nombres XWING/TIEFIGHTER. Esto no confirma por sí solo el setup histórico del usuario.

`asset_identity`: `g_loadedModels.byObjectType[type]` → snapshot.opt_assets public_handle/name; fallback `XwaSnapshotExport_ModelName(type)` está marcado como fallback y no satisface gate CS1. Nombres sin rutas/extensión. `XwaRemasterShip_MeshForNameWithSource` exige runtime OPT (no GLB) y vbo/ibo/index_count válidos.

Cockpit: `snapshot.cockpit.model_name`, `cockpit_valid`, seat==0, `flight_camera.cockpit_visible`; no nombre falso. Expected asset XWINGCOCKPIT en player X-Wing, pero se usa el nombre real. Copia hardpoint_world/camera_pan mediante T3_SeatMatrix y variante player.node_switch. Lookup después del sync real de assets. Si falta: `M8T3_COCKPIT_UNAVAILABLE`; no sustituye por cubos. Por ojo `AeronScene_AddMeshInstance`, `AeronScene_Render`; base_color_emissive_strength=1, no_local_lights=1, zero_velocity=1, `AERON_CULL_BACK`. Texturas originales; instrumentos/HUD/iluminación final no implementados.

## 6. Matemáticas exactas y coordenadas

Fuentes: `C:\OpenXWA\src\xwa_remaster\flight.c`, `C:\OpenXWA\aeron\include\aeron\asset\opt_model.h`, test3 `vr_eye_math.h`, `t3_seat_math.h`, y `C:\OpenXWA\XWAQuest\vr-probe\vr_convert.c` (reutilizado sin editar).

- World XWA int32; Euler Q16 vuelta completa 65536, radianes `(int16_t)angle * 2π/65536`; filas móviles Q15 /32768.
- `AERON_OPT_METERS_PER_UNIT = 1600/65536 = 0.0244140625`; recíproco UNITS_PER_METER=40.96.
- `fl_curmat_from_cached`: R0=rows[0..2]/32768 (side); R1=rows[6..8]/32768 (up); R2=-rows[3..5]/32768 (-forward).
- Si no has_mobj o orient_dirty: `fl_curmat_from_euler`: A=(int16)(0xc000-pitch)*2π/65536, B=(int16)(-yaw)*2π/65536; R0=(cosB,sinB,0); R2=(-sinB*cosA,cosB*cosA,sinA); R1=(-sinB*sinA,cosB*sinA,-cosA); fl_transformaxes aplica angle_d alrededor R1, roll alrededor R2 y spin cuando existe. Rodrigues se consume transpuesto como el motor original. No reimplementar como Euler XYZ genérico.
- `fl_object_world` filas (R0,R2,R1); `fl_model_matrix` las consume transpuestas como columnas. `XwaRemasterFlight_ObjectModelMatrixAtOrigin` calcula delta mediante AeronWorld_LocalI32(origin,world_pos).
- Matriz row-major, vectores columna; traslación índices **3,7,11**, última fila 0,0,0,1. El helper OpenXWA multiplica base por 40.96; bridge multiplica elementos 0..11 por 0.0244140625, cancelando esa escala de base y convirtiendo traslación a metros. No multiplicar sólo traslación otra vez.
- Sistema resultante: OPT model metres → local XWA-axis metres. No existe un simple swizzle global separado XWA→XR. La base de modelo contiene reflexión OPT (-Y forward); la base cámara corrige a rotación propia.

En `VrEye_Compose(m,eye,pos,q)`: r=normalize(col0(m)); u=normalize(col2(m)-r*dot(r,col2(m))); b=cross(r,u); exige dot(b,col1(m))>=.9. Gram-Schmidt elimina cuantización Q15. B=[r u b], rotación propia equivalente a model.right,model.up,-model.forward. qB se obtiene de B y:

```
q_world_eye = normalize(qB ⊗ q_local_eye)   // XYZW Hamilton
p_world_eye[i] = m[4*i+3] + r[i]*eye.x + u[i]*eye.y + b[i]*eye.z
```

OpenXR entrega pose LOCAL por ojo, que **ya contiene head pose × eye offset**. No multiplicar head dos veces. Conceptualmente T_world_eye=T_ship_basis*T_LOCAL_head*T_head_eye. La implementación usa T_ship_basis*T_LOCAL_eye directamente. Posiciones físicas en metros, orientación XR XYZW, +X derecha,+Y arriba,-Z adelante. No se escribe q/p física a la nave/snapshot/input de vuelo.

`T3_SeatMatrix`: copia ship matrix y para cada eje `out[4*i+3] -= (hardpoint_world[i] + camera_pan[i]/16)*METERS_PER_UNIT`. Base del ship intacta; NO entra eye/head. Cockpit queda anclado a ship, cámara física cambia aparte.

Target utiliza su propia model_to_local_m con delta respecto player. `VrConvert_Camera`: convierte XYZW→WXYZ, `q_scene = qD ⊗ conjugate(q_world_eye)`, qD=(0,1,0,0) en WXYZ (180° X); pos se conserva. Aeron calcula view/projection. No introducir otra inversión de handedness.

ReadEye conserva cuatro ángulos runtime; RenderEye mantiene aproximación simétrica existente: hhalf=atan((tan(right)-tan(left))/2), vhalf=atan((tan(up)-tan(down))/2). Near=.05 metros, viewport tamaño swapchain. Aeron usa reversed-Z; no se añade far explícito en este renderer/camera. No inventar un far numérico ni modificar clipping para esta prueba.

Estrellas: `XwaRemasterSkyStars_Create/Prepare/Draw` y shaders originales sky_stars.vert/frag SPIR-V. Por ojo hook BEFORE_OPAQUE, world_to_cube row-major `{1,0,0,0,0,1,0,-1,0}`. Params exposure=1,brightness=1,density=.4,grid_n=32, upscale=height/480, core/feather=.5*upscale,pixel_pitch=upscale,flare=0,game_time_ms=snapshot. Skip death_star_mode. Cámara por ojo anterior controla la dirección; no plano pegado a cabeza. No representa todos los planetas/backdrops.

## 7. Workaround memoria heredado de test2

`frame_bridge.c::__wrap_Aeron_Present`, después de adquirir imágenes XR, obtiene `AeronCommandBuffer *cmd=Aeron_AcquireCommandBuffer()`. En **ese mismo CB** que grabará las escenas y blits llama:

```
__real_SDL_WaitAndAcquireGPUSwapchainTexture(
    cmd->command_buffer, g_aeron.window, &dummy, &dw, &dh)
```

Dummy nunca se enlaza como render target ni se dibuja. Su presencia sólo alimenta telemetría; error no fatal por diseño. Luego se usa/submite cmd mediante Aeron. No sustituir con WaitIdle.

SDL Vulkan: `third_party/SDL3/src/gpu/vulkan/SDL_gpu_vulkan.c`, gate `performCleanups=(claimedWindowCount>0 && commandBuffer->swapchainRequested)||claimedWindowCount==0`. Acquire real marca swapchainRequested; el wrapper original omitía esa llamada manteniendo una ventana claimed. Cleanup recicla command/uniform/descriptor/fence y recursos pendientes asociados a submissions completadas. m8 crecía aproximadamente 90–106 MB/s hasta LOW_MEMORY. La corrección experimental vuelve a recorrer el gate, y test2 confirmó dummy_null=0 y memoria alrededor de 68–70 MB después del arranque durante sesión prolongada (evidencia histórica, no medida nueva test3). Posibles efectos WSI/pacing/flicker siguen pendientes. No promover automáticamente a m8.

## 8. Evidencia física versus local

**CONFIRMED ON QUEST — m8test2, reportado por usuario:** sesión prolongada estable; frontend; Combat Simulator→Single Player→Quick Skirmish; misión real; TIE 3D visible, moviéndose, aproximándose y disparando; jugador destruido; audio/música/nave/disparos; flujo posterior a destrucción; explosión vista como efecto 2D; memoria estable con workaround.

**STATIC/CPU ONLY en test3:** cockpit, estrellas, composición matemática independiente head/ship, APK/build. No declaración PASS visual. No confirmados: cockpit test3, estrellas test3, independencia correcta física, controles vuelo Touch, throttle, disparo player, HUD, explosiones world-space, frontend spatial screen.

Observación usuario: frontend principal cerca/head-locked/flicker y opciones inferiores difíciles de ver. Pantalla inmediatamente previa a Flight parecía fija espacialmente, a distancia cómoda, permitía mirar alrededor y sin flicker visible. Código test3 hereda copia 2D output a ambos ojos; no se encontró una Quad distinta que explique concluyentemente esa pantalla. Investigar después, no afirmar causa ni implementar ahora.

## 9. Input actual (código, no garantía física por acción)

`input_xr.c::M8_InputInit/Poll/ApplyPointer`: perfil oculus/touch_controller. A/right=a/click→SDL RETURN pulso; B→ESC pulso; X/left→12 backspaces y texto `m8test` sólo durante prompt; Y mantenido 2s→request exit y Aeron_RequestQuit. Trigger derecho >.55→click izquierdo press/release. Stick izquierdo→flechas con umbral .55, repetición inicial350ms/siguiente130ms. Stick derecho→cursor absoluto normalizado, deadzone .15, velocidad .55/s, dt cap .1s. Relative mouse=0; no pose física inyectada al vuelo.

Usuario confirmó navegación con stick/botones; A/B en Flight parecían activar funciones/menús, no controles de nave demostrados. No trasladar automáticamente los mappings de M6 a este package. No hay mapping vuelo/lasers/throttle confirmado.

## 10. Quick Skirmish real y duda player

Flujo manual: Combat Simulator→Single Player→Quick Skirmish→setup→generación misión→Flight. `MissionSetup_LoadSkirmishFile` carga SKM/slots/loadouts; `Skirmish_GenerateMission` construye misión real. No auto-setup en test3; Quick Start y Quick Skirmish no son sinónimos.

`C:\OpenXWA\src\xwa\frontend\skirmish.c::Skirmish_GenerateMission`: recorre16 slots con craftType!=0; asigna slot.fgIndex, FG.team=slotIndex/(16/g_teamCount), FG.craftType=slot.craftType; ownerPlayerId!=0 asigna playerNumber secuencial y globalUnit=slotIndex+1. Copia numberOfCraft/groupAI/loadout y waves (goalType puede imponer99 a flyable). Team One/Two corresponden buckets reales. AI TIE debe ser slot no-player configurado como TIE, no un objeto generado por renderer.

Configuración deseada: Team One player X-Wing, Team Two un AI TIE, una nave/oleada si las opciones reales lo permiten. **La primera prueba física no confirmó completamente asignación X-Wing del player**, aunque el TIE funcionó. Verificar `M8T3_PLAYER player_obj_idx/object_type/craft/confirmed_xwing/model_index`; model_index proviene del HUD committed sólo si valid y player_slot coincide, si no es -1 (desconocido). `M8_VR_SNAPSHOT_READY` requiere X-Wing/TIE originales pero no sustituye confirmación física.

## 11. Marcadores y procedencia

Rutas bajo test3 salvo indicación. Logs DRAW/PRESENT son grabación/aceptación, NO resultado visual.

| Marker | Archivo::función | Significado/esperado |
|---|---|---|
| M8_VR_FLIGHT_ENTER, M8_VR_PLAYER_READY, M8T3_PLAYER | vr_flight_bridge.c::CaptureAfterTick | Snapshot válido, identidad real player, ideal confirmed_xwing=1/craft=XWING. |
| M8_VR_TARGET_READY/LOST | mismo | TIE slot+signature y OPT; invalidación si desaparece. |
| M8_VR_SNAPSHOT_READY | mismo | cs1=1 objects=2 units=metres. |
| M8_VR_POSE_UPDATED | mismo | Cambio real de pose/revisión, no animación ficticia. |
| M8_VR_FLIGHT_EXIT | CaptureAfterTick/Reset | Invalidación de vuelo. |
| M8_VR_IMMERSIVE_ENTER | vr_flight_renderer.c::BeginFrame | Runtime OPT listo con índices. |
| M8_VR_BEGIN_FRAME_BLOCKED | begin_failure | state_invalid/flags_not_ready/object_index_invalid/mesh_missing/not_runtime_opt/invalid_mesh/index_count. |
| M8T3_COCKPIT_UNAVAILABLE | BeginFrame | Gate/asset faltante; conservar diagnóstico. |
| M8T3_COCKPIT_DRAW_RECORDED | RenderEye | eye0 y eye1 con OPT/indices; visual=unconfirmed. No hay marker separado que por sí solo pruebe carga visible. |
| M8T3_STARS | RenderEye | Prepare estrellas originales y reloj real; no prueba visibilidad. |
| M8_VR_EYE_STATE_READY | RenderEye | LOCAL, vehicle_times_eye, symmetric_approx. No log continuo de poses numéricas implementado. |
| M8_VR_TARGET_DRAW_RECORDED | RenderEye | índices TIE grabados por ojo. |
| M8_VR_RENDER_EYE_FAILED | eye_failure | tick mismatch, ensure_eye, Compose, Scene_Begin/Render, stars/present failures. |
| M8_VR_FIRST_STEREO_FRAME | EndFrame | draw_mask=3, submit éxito; presentation=unconfirmed. |
| M8_VR_IMMERSIVE_EXIT | Reset | Liberación escena test3. |
| M8T2_CLEANUP_ACQUIRE | frame_bridge.c::__wrap_Aeron_Present | attempted/ok/dummy_null/blits; esperado dummy_null=0. |
| M8_PRESENT_TEXTURE_READY, M8_XR_BLIT, M8_XR_FRAME_PRESENTED | mismo | salida SDR, source cs1_scene en Flight, frame aceptado. |
| M8_OUTPUT_POLICY | __wrap_Aeron_SetOutputHdr | forced_hdr=0; compositor SDR heredado. |
| M8_XR_SESSION_RUNNING | __wrap_Aeron_Init | sesión2eyes inicializada. |
| M8_GAME_TICK, M8_FRONTEND/HANGAR/FLIGHT | __wrap_XwaPort_Tick | tick real y fase. |
| M8_GAME_FRAME | __wrap_XwaRemaster_Frame | frame real/layers. |
| M8_LAUNCH | __wrap_XwaFlightTask_Init | begin y resultado real. |
| M8T3_MISSION_GENERATION | diagnostics.c::__wrap_Skirmish_GenerateMission | begin(path)/end(result) reales. |
| M8_UI_FRAME_STATE, M8_UI_END_FRAME_FAILED | frame_bridge.c::__wrap_Aeron_Present | shouldRender,ready,views,bridge,immersive,layers; transición/periodic. |
| M8_UI_LOCATE_VIEWS | xr_bridge.c::M8_XrLocateViews | xrLocateViews resultado/count/flags; valid exige posición+orientación. |
| M8_XR_ACQUIRE_FAILED/RELEASE_FAILED | xr_bridge.c::M8_XrAcquire/Release | errores swapchain. |
| M8_UI_XR_END_RESULT | frame_bridge.c::VrLog | Error xrEndFrame reenviado. |
| M8_MEM_APP_START | frame_bridge.c::__wrap_Aeron_Init | before Aeron/after XR. |
| M8_MEM_PILOT | input_xr.c::__wrap_FrontendDialog_PromptForPilotName | prompt/retorno nombre, no prueba creación. |
| M8_MEM_COMBAT_SIM | diagnostics.c wrappers CombatSimMenu_Update/Exit | antes/después transición. |
| M8_MEM_SINGLE_PLAYER_ENTER/READY, M8_MEM_SKIRMISH_ENTER, M8_MEM_SETUP_EXIT | diagnostics.c MissionSetup wrappers/directory check | transiciones reales; READY no confirma imagen. |
| M8_MEM_SAMPLE | diagnostics.c::M8_MemoryPoll | cada5s; RSS/PSS/VM/heap/resources/assets/objects/uploads/handles. |
| M8_MEM_RESOURCE/_LIST/_MOVIE/_LARGE_ALLOC, M8_RESOURCE, M8_LARGE_ALLOC | diagnostics.c wrappers FrontImage/Movie/Mem_Alloc | carga/liberación y asignaciones>=8MiB. |
| M8_INPUT_READY/ERROR, M8_PILOT_NAME, M8_EXPLICIT_EXIT | input_xr.c | bindings, texto temporal, salida Y. |
| M8_FATAL, M8_SHUTDOWN | frame_bridge.c::M8_Fail/__wrap_Aeron_Shutdown | error/cierre; confirmar exit-info si proceso muere. |

Archivo privado diagnósticos: `/data/user/0/org.openxwa.xwaquest.m8test3/files/m8-cs1-diagnostics.log`. No existe evidencia de runtime test3 recogida todavía.

## 12. GameData y recuperación segura pendiente

Packages tienen almacenamiento privado separado. SRC `/data/user/0/org.openxwa.xwaquest.m8test2/files/GameData`; DST `/data/user/0/org.openxwa.xwaquest.m8test3/files/GameData`; staging DST `files/.m8-gamedata-import-v3/data`.

ADB `C:\Users\enaci\AppData\Local\Android\Sdk\platform-tools\adb.exe`; serial autorizado `3487C10H9T0BKZ`. Comandos ejecutados en este intento:

```
adb devices -l                                      # Quest_3S device inicialmente
adb -s 3487C10H9T0BKZ install -r <APK de sección2>   # Success
adb -s 3487C10H9T0BKZ shell dumpsys package org.openxwa.xwaquest.m8test3
adb -s 3487C10H9T0BKZ shell pidof org.openxwa.xwaquest.m8test2  # sin PID
adb -s 3487C10H9T0BKZ shell run-as org.openxwa.xwaquest.m8test3 ls -la files # inexistente antes de copiar
adb -s 3487C10H9T0BKZ push <test3/provision-gamedata.sh> <remote>
adb -s 3487C10H9T0BKZ push <helper concatenado> <remote>.manifest.sh
adb -s 3487C10H9T0BKZ shell -T sh <remote> --approved-m8-copy <remote>.manifest.sh
```

Remote exacto `/data/local/tmp/xwaquest-m8test3-gamedata-c36725eb276643ff93d0112c3a416d20.sh`. Helper local `C:\OpenXWA\XWAQuest\m8test3\evidence\gamedata\quest-20260926-013618\selected-manifest-worker.sh` = asset-selection.sh + asset-manifest.sh, LF UTF8 sin BOM. Log `...\device-provision.log`. Proceso host/session49278 terminó código1; no hay captura logcat test3 iniciada.

Worker selecciona assets explícitos (base7660 más datos adicionales admitidos; no conteo rígido árbol), excluye pilotos/config personal/binarios no-assets. Genera TSV SHA256/Tamaño/Ruta; tar entre run-as SRC→DST privado, compara source-after/destination/final con manifiesto; chmod sólo destino; promoción no-overwrite. Fuente sólo leída. No se tocó piloto neto ni pilotos test2.

**Resultado disponible:** fuente seleccionada7744/847224595bytes. Conteo/bytes destino DESCONOCIDOS. RESDATA.TXT/críticos/hashes destino PENDING. No afirmar HASHES_OK/DATA_READY. Verificaciones de recuperación `adb devices -l` vacío; `run-as ... ls files` y staging fallaron device not found. Causa observada actual: dispositivo no conectado; no afirmar exactamente en qué instrucción remota se interrumpió.

Siguiente recuperación (ya autorizada por usuario, sin cambiar código): reconectar Quest/USB, revisar procesos remotos antes de duplicar worker; inspeccionar GameData/staging y TSV existentes en sólo lectura. Si final existe, verificar contra manifiesto y fuente actual sin overwrite. Si parcial, conservar evidencia y completar únicamente assets seleccionados ausentes/incompletos en staging privado, usando mismo manifiesto, sin tocar origen. Recalcular SHA256/tamaño/ruta origen y destino completos; parar ante discrepancias de fuente o archivos inesperados, no borrar staging. Sólo promover cuando todo coincida y final no exista. El script original rechaza staging existente: **no ejecutarlo ciegamente otra vez ni desactivar su protección**. Ningún procedimiento de reanudación fue ejecutado aún.

Verificar explícitamente RESDATA.TXT, FLIGHTMODELS/XWING.OPT, XWINGCOCKPIT.OPT, TIEFIGHTER.OPT y muestras MUSIC/RESOURCE/SKIRMISH seleccionadas. Mantener manifiesto y salida local; demostrar fuente unchanged. Después detenerse: la instrucción vigente NO permite lanzar todavía.

## 13. Validación local y preservación

Histórica, sin repetir builds: eye math PASS (`evidence/eye-math-test.log`); VrConvert real 7/7 PASS (`convert-test.log`); seat math PASS (`seat-math-test.log`). ARM64 PASS (`native-build.log`, `configure.log`); APK BUILD SUCCESSFUL (`apk-build.log`); auditoría `evidence/apk-cs1-audit/` versión/package/ABI/lib vigente. Shaders sky_stars.vert.spv9140bytes, frag2664bytes, magic SPIR-V comprobado. No evidencia visual derivada de CPU.

Scripts anteriores: `tests/Run-CPU-Tests.ps1`, `Build-Target.ps1`, `Build-Apk.ps1`, `tests/Verify-APK.ps1`; NDK28.2.13676358, CMake3.22.1, JBR21.0.11, Gradle8.5. NO ejecutar ahora. Warnings AGP8.1.4 probado compileSDK34 (usa35), metadata riscv64 ignorada, Java8/deprecated SDL. No errores de compilación anteriores.

Recuperación comprobó `evidence/baseline-source-hashes.csv` contra m8test2: **0 diferencias**, APK test2 SHA256 `FF59ABF78973D0D469D5FAC4A93BAF230F30F38D7B8BE6F4CCDD98F602B6004C` conservado. No se realizaron escrituras en m8 ni acciones sobre su package. No existe en este checkpoint una comparación hash histórica integral de m8 que permita atribuir cambios ajenos; no inventarla. Ninguna acción autorizada aquí cambia esos baselines.

Logs test2 conservados/copias: `evidence/m8t2-quest-20260925-223339-logcat.log`, `m8t2-quest-valid-logcat.log`, `m8t2-capture-path.txt`. No confundir esos archivos con un runtime de test3.

## 14. Próxima prueba física y roadmap

Tras verificar GameData, **esperar autorización de lanzamiento**. Después iniciar captura limpia persistente antes de único lanzamiento; guardar PID/rutas/serial/package/SHA en session.json. Capturar buffers completos para OOM/exit-info además de markers del package. No comandos de aprobación mientras usuario usa visor. No cierre automático, navegación automática ni correcciones tras prueba.

Ruta manual: Launch→crear/seleccionar piloto real (X rellena m8test, A confirma)→Combat Simulator→Single Player→Quick Skirmish→setup X-Wing player/TIE AI→Flight. Observar A cockpit, B estrellas, C TIE y movimiento real, D cabeza frente/izquierda/derecha/arriba; ideal seguir TIE. Head no debe girar nave simulada. Escala/posición/clipping/texturas imperfectas se anotan aparte, no descartar automáticamente toda viabilidad por ellas. No evaluar Touch vuelo/disparo/throttle/HUD como requisitos de esta prueba.

Tras reporte físico: preservar logcat completo, exit-info si cierre y archivo privado diagnóstico. Comparar EXPECTED/OBSERVED y PASS/PARTIAL/FAIL/UNKNOWN por A–D. Observación del usuario tiene prioridad sobre logs. No corregir ni promover automáticamente.

Roadmap condicional NO autorizado para implementar ahora: P1 controles básicos, P2 pitch/yaw, P3 throttle, P4 lasers, P5 damage/destruction, P6 frontend spatial screen, P7 explosiones billboard world-space, P8 HUD/targeting, P9 mejoras. Promoción de test3 a m8 requiere decisión explícita después del test.

## 15. INSTRUCTIONS FOR NEXT CODEX SESSION

1. Leer este checkpoint y `REPORTE-M8TEST3.md`; seguir última instrucción usuario: recuperar datos, NO lanzar todavía.
2. No repetir investigación ni builds. No modificar código, m8, m8test2, M0–M7, OPT, GameData origen o pilotos. No borrar staging.
3. APK exacto sección2 ya instalado/verificado; no reinstalar por costumbre. Evidencia física válida pertenece a test2, cockpit/estrellas/head test3 siguen pendientes.
4. **Siguiente acción concreta: comprobar conexión del Quest y leer GameData/staging/manifest del package test3 sin modificarlo; el último intento quedó sin verificación final y ADB estaba desconectado.** No asumir copia terminada ni empezar otra en paralelo. Recuperar según sección12.
5. Archivos clave bridge/renderer/eye_math/seat_math/frame_bridge/input_xr/diagnostics; inventarios y logs en evidence. Riesgos: configuración player real, hardpoint/escala visual, proyección simétrica, lifetime meshes, cleanup WSI experimental, flicker/frontend aún sin solución. No resolverlos sin evidencia y autorización.
6. Cuando datos verificados, actualizar checkpoint/inventario documental y devolver sólo estado requerido por usuario; esperar autorización para captura/lanzamiento. Al lanzar posteriormente, dejar app y logcat abiertos para prueba manual, sin nuevos comandos que obliguen a quitarse visor.
