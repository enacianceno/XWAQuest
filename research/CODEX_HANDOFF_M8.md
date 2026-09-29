# XWAQuest Codex Handoff — M8

## READ THIS FIRST

- **WHERE WE ARE**: Milestone "FIRST REAL X-WING VISUALLY CONFIRMED ON QUEST 3S" cerrado 2026-09-24. La sonda VR (`vr-probe`) dibuja geometría real XWING.OPT en el Quest 3S vía OpenXR + SDL_GPU (pipeline V10).
- **WHAT WORKS**: cooking OPT→malla real; pipeline V10 ShaderCross; render indexed por ojo; OpenXR swapchains (30/30 `xrEndFrame result=0`); M7B (juego original en panel 2D) funcional aparte.
- **WHAT JUST WORKED**: APK `D41B7AAAE0F2EAA6F149AE64927C15661E187BB0125C270F021F6824C55E8937` → usuario vio **X-Wing magenta reconocible**; log `vr-probe/evidence/xwing-v10-run-20260924-222207.log` (10380 verts / 852 idx / 284 tris; auto-exit 30 frames, exit 0).
- **WHAT NOT TO TOUCH**: M6 (`m6/`), M7B (`m7/`), GameData, piloto, shaders V10, parser OPT, fuentes de `vr-probe` sin autorización. Regla ADB obligatoria (§14).
- **WHAT M8 ES**: "Primera mini-experiencia XWA jugable dentro del Quest" (§9) — unir el loop real de M7B con el ciclo OpenXR de VRPROBE, sin reescritura (§10-11).
- **WHAT TO READ NEXT** (8 rutas): `research/CODEX_HANDOFF_M8.md` → `research/mimo-xwing-v10/REPORT.md` → `XWAQUEST_STATUS.md` → `.opencode/skills/xwaquest-development/SKILL.md` → `vr-probe/vr_main.c` → `vr-probe/vr_stereo.c` → `C:\OpenXWA\src\xwa_app\main.c` → `C:\OpenXWA\src\xwa_runtime\runtime\port.c`

---

## 1. Objetivo general

Port nativo standalone de Star Wars X-Wing Alliance a Meta Quest 3/3S
Android ARM64 mediante OpenXR.

## 2. Estado demostrado

**DEMONSTRADO** (evidencia física o log citable):
- Parser/cooking `XWING.OPT` funcional (log `[flight_gltf] … 10380 verts, 852 indices, 90 prims`; `XWING_MESH_CPU_READY` en device).
- Juego/misión original funcionando en M7B 2D (panel; confirmaciones de usuario en `XWAQUEST_STATUS.md`).
- OpenXR funcional en Quest 3S (30/30 `xrEndFrame result=0`, swapchains 2 ojos).
- SDL_GPU/ShaderCross V10 funcional (pipeline create OK, draws registrados).
- Triángulo V10 físicamente visible (validación física previa, `VISUAL_CONFIRMED_BY_USER`).
- **X-Wing real físicamente visible** — 2026-09-24, `VISUAL_CONFIRMED_BY_USER = DEMOSTRADO` (X-Wing magenta reconocible).
- Indexed rendering demostrado (`XWING_DRAW_RECORDED: tris=284`, sin fallback al triángulo).

**NO DEMOSTRADO todavía**:
- Misión/juego real ejecutándose dentro de la sesión VR (el loop VRPROBE no llama `XwaPort_Tick`).
- Estéreo visual de la escena Aeron (código aplica poses por ojo; sin confirmación visual).
- Input, controles y audio de juego en sesión VR.
- Head tracking controlando la cámara del juego (solo convención de cámara convertida; sin prueba visual declarada).

**PENDIENTE**:
- Orientación exacta percibida del X-Wing (solo "reconocible" confirmado; por código es vista cenital sin rotación).
- Coexistencia X-Wing V10 vs escena Aeron (posible en código; el usuario no reportó problema).
- Todo el alcance M8 (§9).

## 3. Baselines que NO deben romperse

| Baseline | Carpeta | Package | Estado |
|---|---|---|---|
| M6 | `C:\OpenXWA\XWAQuest\m6` | `org.openxwa.xwaquest.m6` | Funcional 2D. APK histórica `6FFCE008…0FED` (claim de STATUS; **el .apk no está en disco actualmente**) |
| M7B | `C:\OpenXWA\XWAQuest\m7` | `org.openxwa.xwaquest.m7` (`versionName 0.7-M7B-adpcm-fix`, `m7/app/build.gradle:8,12`) | Funcional 2D — **protegido por el skill** |
| M7C (folder) | `C:\OpenXWA\XWAQuest\m7c` | `org.openxwa.xwaquest.m7c` (`0.8-M7C-xr-panel`) | Proyecto XR-panel con seam `Aeron_Present`→XR previo; intacto |
| **VRPROBE V10 actual** | `C:\OpenXWA\XWAQuest\vr-probe` | `org.openxwa.xwaquest.vrprobe` (`0.0-VRPROBE`) | **APK SHA-256 `D41B7AAAE0F2EAA6F149AE64927C15661E187BB0125C270F021F6824C55E8937`** — baseline física validada 2026-09-24 |

Protegido además: GameData (7660 archivos, `FLIGHTMODELS/XWING.OPT`), datos del piloto, `m0–m6/`.

## 4. Archivos importantes (solo los que M8 probablemente necesita)

| Ruta | Función | Por qué importa |
|---|---|---|
| `research/CODEX_HANDOFF_M8.md` | Este handoff | Primer documento a leer |
| `research/mimo-xwing-v10/REPORT.md` | Auditoría V10 + cierre milestone + Runtime Readiness Audit | Evidencia y límites del estado actual |
| `XWAQUEST_STATUS.md` | Estado y reglas de seguridad/ADB | Baselines y protecciones |
| `.opencode/skills/xwaquest-development/SKILL.md` | Reglas del proyecto | Prohibiciones y proceso ADB |
| `vr-probe/vr_main.c` | Init + loop XR (524-983), warmup (313-344), auto-exit (63, 547-550) | Loop que M8 debe ampliar |
| `vr-probe/vr_stereo.c` | `VrStereo_RenderEye` (186-283), `CreateMeshBuffers` (386+), `DrawMesh` (604) | Render por ojo actual |
| `vr-probe/vr_openxr.c` | Ciclo de vida OpenXR (`VrXr_*`, 81-458) | Reutilizable tal cual |
| `C:\OpenXWA\src\xwa_app\main.c` | Loop real del juego M7B (400-468) | El loop que hay que unir |
| `C:\OpenXWA\src\xwa_runtime\runtime\port.c` | `XwaPort_Tick`/`TickBody` (189-340) | Update del juego por frame |
| `C:\OpenXWA\src\xwa\flight\flight_task.c` | Máquina de fases de misión/vuelo (1189-1650) | Misión real |
| `C:\OpenXWA\src\xwa_remaster\flight.c` | `XwaRemasterFlight_BuildView` (1547), `AeronScene_Begin` (3909), render (4324) | Cámara y escena del juego |
| `C:\OpenXWA\aeron\src\render_backend.c` | XR opt-in (1416-1475), `Aeron_Present` (1615), `Aeron_WaitForPresentationSlot` (1860) | Seam de presentación/timing |
| `C:\OpenXWA\XWAQuest\m7c\trace.c` (149-155) + `m7c\input_android.c` (83-108) | Prior art: wraps `Aeron_Present`→XR y `Aeron_BeginFrame`→XR wait | Patrón de unión ya codificado |
| `C:\OpenXWA\XWAQuest\m7\CMakeLists.txt` (18-61) | Wraps y archive de Aeron de M7B | Seam de build/link |

## 5. Ruta M7B funcional (compacta)

```
RuntimeActivity (SDLActivity) [m7/app/src/main/java/.../RuntimeActivity.java:3]
  → SDL trampoline → main() [src/xwa_app/main.c:262]   (parcheado a main-m7.c por m7/CMakeLists.txt:18-36)
init: Aeron_Init (main.c:297) → XwaHostConfig_Load (300) → XwaSetup_ValidateGameData
      → XwaRemaster_Init (359) → XwaPort_Init (384) [port.c:111 → XwaFrontendTask_Init frontend_task.c:53]
loop [main.c:400-468] por frame:
  Aeron_BeginFrame (401; delta_us; pump eventos + espera vsync events.c:119)
  → input snapshot (402) → XwaRemaster_BeginFrame (447)
  → XwaPort_Tick(delta_us) (454) [port.c:330→TickBody port.c:189]
       frontend (port.c:303-327): menús/concurso → "Fly" → FrontendFlight_LaunchSession
         [frontend_flight.c:102] → BeginPendingLaunch (:112) → MissionSetup_LoadMissionList
         → XwaFlightTask_Init (:196) → fase LOADING → Mission_Init [mission.c:7294 por flight_task.c:1437]
         → MISSION_INSTANCE_INIT (cockpit/exterior/hangar flight_task.c:1514-1583) → MISSION_START (:1604)
       vuelo (port.c:229-301): AeronCompat_Update (234) → XwaFlightTask_Tick (271)
         → RunSinglePlayerFrame [flight_task.c:891] → Flight_StepSimToTime [flight.c:5497]
         → Flight_AdvanceOneStep (5813: steering 5133, movimiento object.c:1378, Flight_UpdateEntity 10606
            con FlightInput_Read flight.c:2440)
         → render+audio: XwaFlightTask_RenderFrameAndAudio [flight_task.c:670/980]
            FlightView_RenderFrame [flight_view.c:2276] + Sound_FlushQueuedEffects [sound.c:1221]
  → XwaRemaster_Frame(delta_us) (459) [xwa_remaster.c:541]  ← render HD:
       SyncAssets (ship.c:157 vía :646) → XwaRemasterFlight_Render [flight.c:3699]
       → BuildView (flight.c:1547) → AeronScene_Begin(…,camera) (3909) → AddMeshInstance (4114+)
       → AeronScene_Render (4324) → Aeron_SubmitTextureLayer (xwa_remaster.c:857)
  → Aeron_Present() (463) [render_backend.c:1615]  ← compone capas al swapchain SDL
  → Aeron_WaitForNextFrame(XwaPort_NextWakeDelayUs()) (467)
input: Java RuntimeActivity (dispatchGeneric/MotionEvent:66,105) → JNI → wraps
  [m7/trace.c:183 __wrap_FlightInput_Read inyecta axes Quest; m7/input_android.c:81 teclado]
audio: Aeron_AudioInit [aeron.c:103→audio.c:549] + DSound shim [compat/dsound.c] + iMUSE [imuse.c:128]
  + Sound engine en vuelo [sound.c:69 vía flight_task.c:1297]
```

## 6. Ruta VRPROBE funcional (compacta)

```
vr_main.c main (182): Aeron_Init (258) → host config/validate (262,272) → XwaRemaster_Init (291)
  → XwaPort_Init (299) → warmup 30× XwaPort_Tick (313-323) → load_xwing_model (326; ModelPreview_LoadModel 162/172)
  → warmup 30× (334-344) → XwaSnapshot_Current (351) → XwaRemasterShip_SyncAssets (363)
  → CommitSyncBatch (373) → MeshForName("xwing") (374-376)
  → VrXr_Init (394) → VrStereo_Init (433) → VrStereo_CreateMeshBuffers (437) → VrBlit_Init (444) → VrReadback_Init (466)
loop (524-983):
  VrXr_PollEvents (561) → VrXr_WaitBeginFrame (569) → VrXr_LocateViews (581)
  → cams[2] desde poses XR (611-638 → camera_from_xr 144-152 → VrConvert_Camera [vr_convert.c:46])
  → VrXr_AcquireEye (717-728) → cmd (735)
  → VrStereo_RenderEye(cmd, i, &cams[i], …) (738-741)   [vr_stereo.c:186-283:
       AeronScene_Begin(scene, cam) 219 → instancia única 235-247 → AeronScene_Render 249
       → tonemap/present en s_present[eye] 267-279]
  → VrStereo_DrawMesh(cmd, present_tex[i]) (751)  [vr_stereo.c:604: V10 clip-space, post-tonemap]
     fallback VrStereo_DrawTriangle (757)
  → VrBlit_Copy → xrImage (764) → submit (843) → VrXr_ReleaseEye (854) → VrXr_EndFrame (859)
  → auto-exit: cap 30 frames (63, 547-550)
cocción malla: XWING.OPT → Aeron_OptModelBuildMemory [opt_model.c:277] → OptGltf_BuildMemory (solo LOD0)
  → Aeron_GltfMeshBuildData [gltf_mesh.c:383] → AeronScene_MeshCreate [mesh.c:266] → cpu_vertices/cpu_indices
  → VrStereo_CreateMeshBuffers [vr_stereo.c:386: bounds solo referenciados, clip=(pos−center)·scale]
  → VBO/IBO → VrStereo_DrawMesh [SDL_DrawGPUIndexedPrimitives vr_stereo.c:640]
NOTA: el loop NO llama XwaPort_Tick/XwaRemaster_Frame/Aeron_Present (solo warmups 313-344).
```

## 7. Qué está probado en Quest (solo evidencia fuerte)

- Triángulo V10 visible físicamente (validación previa con usuario, `VISUAL_CONFIRMED_BY_USER`).
- **X-Wing magenta visible físicamente** (2026-09-24, usuario: "X-Wing magenta reconocible").
- 30 frames (`stereo=30 submitted=30`), cierre `reason=auto-exit submitted cap`, `exit code=0` — **no crash**.
- OpenXR success: 30/30 `VRPROBE xrEndFrame end … result=0`.
- Indexed draw success: `XWING_DRAW_RECORDED: tris=284` + `VBO_CREATED` (124 560 B) + `IBO_CREATED` (1704 B) + `UPLOAD_SUBMITTED`; sin fallback (`triangle-draw ok`=0).
- Log: `C:\OpenXWA\XWAQuest\vr-probe\evidence\xwing-v10-run-20260924-222207.log`
- APK: `C:\OpenXWA\XWAQuest\vr-probe\app\build\outputs\apk\debug\app-debug.apk` SHA-256 `D41B7AAAE0F2…C55E8937`.

## 8. Limitaciones actuales VRPROBE (verificadas en código)

1. **Auto-exit a 30 frames** (`vr_main.c:63`, `547-550`) — sesión de ~5 s por diseño diagnóstico.
2. **X-Wing en magenta sólido** (`shaders_v9/minimal.frag.hlsl:1-3` → `float4(1,0,1,1)`).
3. **Shader V10 mínimo**: vert passthrough `float4(pos,1)` (`minimal.vert.hlsl`), sin texturas/iluminación.
4. **X-Wing V10 sin matrices VR**: transformación CPU `clip=(pos−center)·scale` (`vr_stereo.c:456-463`), sin view/proj → **head-locked y plano** (mismo NDC en ambos ojos, disparidad 0). La escena Aeron SÍ usa poses XR por ojo (`vr_main.c:611-638` → `AeronScene_Begin vr_stereo.c:219`), pero su estéreo **no está visualmente confirmado**.
5. **Orientación actual**: sin rotación → vista cenital del X-Wing (por código); el usuario confirmó "reconocible" sin describir orientación — PENDIENTE.
6. **Posible coexistencia** con la escena Aeron (X-Wing V10 centrado en pantalla + barco Aeron con su model matrix) — posible en código, sin incidencia reportada.
7. **Sin controles VR**: `vr-probe` no tiene action sets/controllers/haptics OpenXR (grep negativo). Sin input de juego en el loop.
8. **Sin game loop real**: `XwaPort_Tick` solo en warmup (`vr_main.c:313-344`); no hay misión, HUD ni vuelo en sesión.
9. **Sin audio de juego** en la sesión (dispositivo Aeron se inicializa; `Sound_Init_Sound_Engine` no se ejecuta).

## 9. M8 — siguiente milestone

**"Primera mini-experiencia XWA jugable dentro del Quest."**

Target mínimo (texturas perfectas y controles VR finales NO son requisito inicial):
1. iniciar una pequeña parte real de XWA;
2. permanecer en VR sin auto-exit;
3. usar una escena/misión real;
4. render estereoscópico por ojo;
5. head tracking independiente;
6. cockpit o punto de vuelo reconocible;
7. al menos otra geometría/entorno real;
8. conservar lógica real de XWA;
9. input provisional suficiente para controlar la nave;
10. salida limpia.

## 10. M7B + VRPROBE: puntos de integración (lo más importante)

**¿Qué tiene M7B que conservar?** Todo el juego: frontend→misión (`frontend_flight.c:102` → `flight_task.c:1189` → `mission.c:7294`), flight loop (`flight.c:5497/5813`), cámara de vuelo (`flight_view.c:871` → snapshot `snapshot.c:1343` → `XwaRemasterFlight_BuildView flight.c:1547`), render HD (`XwaRemaster_Frame xwa_remaster.c:541` → `AeronScene_Render flight.c:4324`), input Java/JNI (`RuntimeActivity.java:66-124` → `m7/trace.c:183`), audio (Aeron + DSound + iMUSE + Sound), timing (`Aeron_BeginFrame` delta + `WaitForNextFrame`).

**¿Qué tiene VRPROBE que incorporar?** Ciclo OpenXR completo (`VrXr_Init/WaitBeginFrame/LocateViews/Acquire/EndFrame/Shutdown`, `vr_openxr.c:81-458`), swapchains por ojo + `VrBlit_Copy` (`vr_blit.c:150`), conversión de cámara XR→Aeron (`vr_convert.c:46`, autotest `:89`), render por ojo con tonemap (`VrStereo_RenderEye`), diagnósticos (readback/framing/audit), pipeline V10.

**Puntos mínimos de unión observados (solo hechos de código):**
1. **Seam de game tick**: meter `XwaPort_Tick` (`port.c:330`) dentro del loop XR entre `VrXr_WaitBeginFrame` (`vr_main.c:569`) y `VrStereo_RenderEye` (`:738`). El probe ya corre `XwaPort_Tick(16667)` sin swapchain de panel (313-344; comentario 306-311). **Restricción de timing**: la frescura de input depende de `Aeron_BeginFrame` → `Aeron_WaitForPresentationSlot` (`events.c:119` → `render_backend.c:1860-1874`), que se bloquea en actividad IMMERSIVE_HMD; M7B en input vía JNI/wraps sin pasar por ese wait.
2. **Seam de cámara**: sustituir `cams[i]` (`vr_main.c:739`) por la cámara de vuelo de M7B (`XwaRemasterFlight_BuildView flight.c:1547` → `XwaRemasterFlightView.camera`, tipo `flight.h:188`). Ambos lados son `AeronSceneCamera` consumido por `AeronScene_Begin` (`scene3d.c:380`) — **mismo tipo, unión directa**. (Composición head-pose + cockpit = decisión de diseño posterior.)
3. **Seam de misión/estado**: reemplazar el cook one-shot del probe (`vr_main.c:351-376`) por la ruta misión: `XwaRemaster_Frame` → `SyncAssets` (`xwa_remaster.c:646`) → `XwaRemasterFlight_Render` (`:743`). OJO: esa ruta emite presentation layers (`Aeron_SubmitTextureLayer xwa_remaster.c:857`) que solo consume `Aeron_Present` (`render_backend.c:1615`), que el probe **nunca** llama.
4. **Seam de presentación con prior art en M7C**: `__wrap_Aeron_Present → M7C_XrEndFrame` (`m7c/trace.c:149-155`) y `__wrap_Aeron_BeginFrame → XR poll/wait` (`m7c/input_android.c:83-108`) — patrón "mantener loop M7B, envolver present/begin" ya codificado una vez. Variante textura: `VrBlit_Copy` con textura Aeron (`vr_main.c:764`).
5. **Seam de build/link**: wraps difieren (`m7/CMakeLists.txt:46-53` vs `vr-probe/CMakeLists.txt:27-30`) y archives de Aeron difieren (`m7` usa M2 congelado vs `vr-probe/staging/libaeron_vrprobe.a` con bloque XR opt-in `render_backend.c:1416-1475`). El opt-in se activa con env `XWAQUEST_XR_ENABLE`: probe lo setea (`vr_main.c:191`), M7B no. Unificar archives/env es prerequisito de integración.

**No proponer reescritura**: reutilizar los cuatro bloques anteriores; el juego sigue siendo M7B, XR se acopla por envoltorio/inyección.

## 11. Estrategia M8 recomendada (5 pasos pequeños y verificables)

**M8.1 — Misión real corriendo con OpenXR activo.**
Objetivo: `XwaPort_Tick` por frame dentro del loop XR sin swapchain de panel y sin auto-exit (flag sobre `vr_main.c:63,547-550`).
Archivos: `vr-probe/vr_main.c`, `src/xwa_app/main.c`, `src/xwa_runtime/runtime/port.c`, `aeron/src/events.c`.
Éxito: log de ticks de misión por frame + `xrEndFrame result=0` sostenido ≥60 s + sin wedge del wait de presentación.

**M8.2 — Cámara OpenXR controla la view matrix por ojo.**
Objetivo: componer pose de cabeza XR con la cámara de vuelo de M7B y pasarla a `AeronScene_Begin`.
Archivos: `vr-probe/vr_main.c` (611-638), `vr-probe/vr_convert.c`, `src/xwa_remaster/flight.c` (1547), `aeron/src/scene/scene3d.c` (380).
Éxito: al mover la cabeza cambia la vista de la escena en ambos ojos; autotest `VrConvert_SelfTest` sigue pasando.

**M8.3 — Render estereoscópico por ojo de la escena del juego.**
Objetivo: que `VrStereo_RenderEye` renderice la escena real (varias instancias/mallas de misión, no una sola malla+model matrix).
Archivos: `vr-probe/vr_stereo.c` (186-283), `src/xwa_remaster/flight.c` (3909-4324), `aeron/src/scene/scene3d.c`.
Éxito: geometría de misión (nave + entorno) visible por ojo con disparidad correcta en dispositivo.

**M8.4 — Input provisional de vuelo.**
Objetivo: reutilizar el camino Java/JNI de M7B (`RuntimeActivity` → `nativeSetQuestAxes` → wrap `FlightInput_Read`) en la sesión VR, desacoplando el pump de input del wait de presentación.
Archivos: `m7/RuntimeActivity.java`, `m7/trace.c` (183-213), `m7/input_android.c`, `vr-probe/vr_main.c`, `aeron/src/events.c` (119-149).
Éxito: stick/trigger del Quest mueven la nave durante la sesión VR (evidencia por log de ejes).

**M8.5 — Sesión persistente + salida limpia.**
Objetivo: cap de 30 frames solo bajo flag de diagnóstico; mantener sesión minutos; salida con `exit code=0` y teardown OpenXR correcto.
Archivos: `vr-probe/vr_main.c` (63-65, 547-558), `vr-probe/vr_openxr.c` (460-482).
Éxito: sesión ≥5 min + salida normal + VRPROBE baseline intacta cuando el flag está en su valor por defecto.

## 12. Primer trabajo recomendado para Codex (UNA tarea, read-only)

**Tarea: validar el "M7B + VRPROBE integration seam" sin modificar nada.** Entregar un informe breve que responda, con file:line, los 9 ejes de §10 (game loop, misión, scene/model ownership, renderer, OpenXR lifecycle, frame timing, input, cámara, audio) y señale el punto mínimo de unión entre M7B y VRPROBE reutilizando código existente.

Archivos a leer exactamente:
1. `C:\OpenXWA\XWAQuest\vr-probe\vr_main.c` (init 193-520, loop 524-983)
2. `C:\OpenXWA\src\xwa_app\main.c` (262-468)
3. `C:\OpenXWA\src\xwa_runtime\runtime\port.c` (189-340)
4. `C:\OpenXWA\XWAQuest\vr-probe\vr_stereo.c` (186-283)
5. `C:\OpenXWA\src\xwa_remaster\flight.c` (1547-1600, 3699-4330)
6. `C:\OpenXWA\aeron\src\render_backend.c` (1416-1475, 1615-1874)
7. `C:\OpenXWA\XWAQuest\m7c\trace.c` (149-155) y `C:\OpenXWA\XWAQuest\m7c\input_android.c` (83-108)
8. `C:\OpenXWA\XWAQuest\m7\CMakeLists.txt` (18-61) y `C:\OpenXWA\XWAQuest\vr-probe\CMakeLists.txt` (27-39)

## 13. No hacer todavía

- No texturas perfectas; no joystick VR final; no UI VR completa.
- No optimización; no refactor grande; no reescritura del loop.
- No reescribir el parser OPT; no tocar el cooking demostrado.
- No cambiar shaders/pipeline V10 sin necesidad demostrada.
- No romper M7B (`m7/`) ni M6 (`m6/`); no romper el baseline VRPROBE (APK `D41B7AA…`).
- No ejecutar `fix_*.py`; no ADB sin la regla del §14.

## 14. Safety / ADB

**ANTES DE CUALQUIER ADB, incluso `adb devices`, preguntar EXACTAMENTE:**

> ¿Ya encendiste el Quest 3S, lo conectaste a la PC y autorizaste la depuración USB? Confírmame cuando esté listo para continuar.

Esperar la confirmación del usuario. Esa confirmación **solo** autoriza comprobar la conexión.

**Instalar APK requiere autorización explícita separada**, indicando: ruta exacta, package y SHA-256 de la APK. No desinstalar otras apps, no borrar datos, no tocar M6/M7B/GameData/piloto sin autorización específica.

---

## Apéndice — Contradicciones documentadas (NO resueltas; para Codex)

1. `XWAQUEST_STATUS.md:11-12` dice M7A=`m7a`/`.m7a` y M7B=`m7b`/`.m7b`; **no existen esos folders ni packages**. M7B real = `m7/`, `org.openxwa.xwaquest.m7`. `m7/ESTADO.md:1` se autotitula "M7A" (doc viejo).
2. STATUS llama "M7C" a `vr-probe`/`.vrprobe`, pero existe carpeta `m7c/` con `.m7c`/`0.8-M7C-xr-panel`; `REPORTE-VR-PROBE.md:3-4` dice que vr-probe "No es M7".
3. STATUS afirma APK de M6 y M7B idénticas (`6FFCE008…0FED`), pero sus CMakeLists difieren (M6 aborta en frontera de vuelo, M7B solo cap de 30 min) → no pueden ser byte-idénticas; además ningún `.apk` de milestone está en disco actualmente.
4. Secciones "Problema activo de M7C / Hipótesis H1-H5" de STATUS (era v9.x, pipeline plano) quedan **históricas**: el pipeline V10 actual está demostrado operativo (§2/§7).
5. Desconocido: si el `libaeron.a` congelado de M7B contiene el bloque XR opt-in (`render_backend.c:1416-1475`); el probe staged su propio archive con él (`vr-probe/CMakeLists.txt:32-39`), lo que sugiere que no.
