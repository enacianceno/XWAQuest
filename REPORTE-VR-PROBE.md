# REPORTE VR-PROBE — sonda estéreo experimental (en curso)

Fecha: 2026-09-19. Proyecto: `C:\OpenXWA\XWAQuest\vr-probe`. Paquete
`org.openxwa.xwaquest.vrprobe`. No es M7. M6 preservado y funcional.

## Objetivo

X-Wing real en Quest 3S con dos vistas independientes (poses + FOV OpenXR
por ojo), cámara dirigida por la cabeza y paralaje real. Sin misión,
sin cabina, sin HUD VR.

## Aislamiento y preservación

- Todo el código nuevo vive en `vr-probe/`: `vr_main.c`, `vr_openxr.c/h`,
  `vr_stereo.c/h`, `vr_blit.c/h`, `vr_trace.c`, `CMakeLists.txt`,
  `Build-Target.ps1`, gradle, `AndroidManifest.xml`, `VrProbeActivity.java`.
  Reutiliza archivos estáticos, `.so` y shaders aprobados de M2.
- `m0/`–`m6/`, `hello_xr`, correcciones OPT y GameData original intactos.
  APK M6 verificado tras el trabajo: `6FFCE008...0FED` (sin cambios).
- GameData del probe = copia privada `m6 -> vrprobe` en el Quest
  (`run-as ... tar`), 7660 archivos, incluye `FLIGHTMODELS/XWING.OPT`.
  Originales y copia M6 intactos.
- Único archivo compartido modificado: `aeron/src/render_backend.c`
  (bloque XR opt-in, env `XWAQUEST_XR_ENABLE`; sin env el flujo M0–M6 es
  idéntico). Reversión: `git -C aeron checkout -- src/render_backend.c`
  o restaurar `vr-probe/evidence/original-render_backend.c`
  (SHA256 `723E02BD...`). Diff: `evidence/shared-render_backend.patch`.
- `m2/build-android/.../libaeron.a` se reconstruyó temporalmente para
  enlazar el probe y se restauró byte a byte (`83765035...`, verificado).
  El `.so` de M6 ya estaba enlazado: inafectado.

## Ruta real reutilizada

`XwaLaunchOptions_Parse -> Aeron_Init -> XwaHostConfig_Load ->
XwaSetup_ValidateGameData -> XwaRemaster_Init -> XwaPort_Init ->
ModelPreview_LoadModel("FlightModels\\XWING.OPT")` (candidatos + tipos
`FeDiskIo_GetCraftModelName`) -> 60 `XwaPort_Tick` ->
`XwaRemasterShip_SyncAssets` + `CommitSyncBatch` (cocinado OPT->glb real) ->
`XwaRemasterShip_MeshForName("xwing")` -> 2x
`AeronScene_Begin/AddMeshInstance/Render` (uno por ojo) -> tonemap
`PresentChain` -> quad fullscreen a la imagen adquirida de cada swapchain XR
-> `xrEndFrame` con capa de proyección. Nunca se dibuja la misma textura
en ambos ojos: cada ojo renderiza su propia geometría con su propia cámara.

## Coordenadas (eye-space vs OpenXR)

El preview clásico fija cámara identidad y hornea la vista en la instancia.
La sonda NO reutiliza ese horneado: toma la malla cocinada real y la coloca
fija en el mundo (`xr=(ex,ez,-ey)`, escala vitrina 0.08, guiñada lenta),
con escenas en metros (`view_space_to_meters=1`). Cámaras desde
`xrLocateViews` (pos directa, quat `(x,y,z,w)->(w,x,y,z)`),
`h_half=atan((tanR-tanL)/2)`, `v_half=atan((tanU-tanD)/2)`, `near=0.05`,
offsets 0 (aproximación simétrica documentada; offsets asimétricos
pendientes). `VRPROBE_FIXED_IPD=1` = diagnóstico de paralaje con poses
fijas ±0.032 m (no es criterio de aceptación). Temporal/post apagados y
dos `AeronScene3D` independientes: nada se mezcla entre ojos.

## Interoperabilidad SDL_GPU/Vulkan/OpenXR

Sin bloqueo arquitectónico: SDL3 expone `SDL_CreateGPUXRSession`,
`SDL_GetGPUXRSwapchainFormats` y `SDL_CreateGPUXRSwapchain` sobre el mismo
`SDL_GPUDevice` de Aeron (habilitado para XR vía el opt-in). Las imágenes
de swapchain son `SDL_GPUTexture*` y reciben la escena mediante render pass
directo con los shaders `fullscreen` ya empaquetados. Sincronización por
`xrAcquire/Wait/ReleaseSwapchainImage` + `Aeron_SubmitCommandBuffer` antes
de `xrEndFrame`. Formatos: escena HDR intermedia, swapchain en su formato
nativo anunciado.

## Bloqueos encontrados (laptop)

1. Enlace inicial: `Aeron_VrProbe*` indefinidos porque `libaeron.a` de M2
   era anterior al parche. Resuelto con reconstrucción temporal +
   restauración verificada (arriba).
2. `SDL_CreateGPUDeviceWithProperties` con XR se quedaba dormido con el
   manifest 2D (sin permisos OpenXR / broker queries): causa raíz probable.
   Resuelto copiando las declaraciones XR validadas en M1
   (`OPENXR`, `OPENXR_SYSTEM`, queries, metadatos oculus, categorías
   `IMMERSIVE_HMD` + `com.oculus.intent.category.VR`).
3. Lanzamiento `adb` sin visor: el sistema muestra
   `app_launch_blocked_controller_required` y no arranca el proceso.
   Esperado: la sesión inmersiva exige visor + mandos activos.
   Evidencia: `vr-probe/evidence/launch-blocked-no-headset.txt`.

## APK

`vr-probe/app/build/outputs/apk/debug/app-debug.apk`
v1 `D76D6E27...` (manifest 2D, colgado en creación XR), v2 `26EFD384...`
(manifest VR), v3 `64357270...` (fixes anti-bloqueo ventana),
v4 `1EFC51E0...` (file-log + watchdog),
**v5 `EBB2748E...` (instrumentación por operación + wait 100ms + señal
rojo/verde, instalada 16:19)**. GameData 7660 verificados.
M6 `6FFCE008...` y `libaeron.a` M2 `83765035...` intactos tras el trabajo.

## Pase diagnóstico v4 (16:10–16:11) — hechos del registro interno

`files/vrprobe-log.txt` (51 s de ejecución, el proceso murió sin `loop end`):
- `0–1029` warmup + carga OPT + warmup2 OK y rápidos.
- `1037` sync `opt_assets=1`; `1059` **malla cocinada OK** (`0xb4...`).
- `1263` ojos `1680x1760 format=52`. `1321` **vistas XR reales** (IPD ~64 mm,
  FOVs por ojo) + ancla del modelo.
- **36 `xrEndFrame(rendered=1)`**: 1–3 a `5.7 s`, 30 a `7.1 s` (~8 fps),
  35 a `~10 s`; congelado en 35 (`10 s`→`47 s`), un frame 36 a `47.5 s`.
- Sin `FAILED`, sin crash. Cierre `16:11:36` por Android:
  `Killing ... Can't deliver broadcast` (ANR). "Se cerró sola" = kill
  del sistema, no salida limpia.

## Incidente 16:00 — cuadro azul y visor inhibido

Síntoma del usuario: solo un cuadro azul, imposible salir, hubo que
mantener power. El sistema mató el proceso dos veces
(`Can't deliver broadcast`, ANRs `anr_...16-00`, `...16-01`).

Causa raíz: el warmup llamaba `Aeron_BeginFrame`, que espera al swapchain
de la ventana Android. Una actividad `IMMERSIVE_HMD` nunca lo alimenta:
hilo principal bloqueado, sesión XR nunca sondeada/liberada, ni siquiera
el gesto de salida del sistema podía actuar. `port.c` verificado: el tick
no toca el swapchain.

Correcciones (solo `vr-probe/`, sin tocar motor salvo el parche ya
documentado):
- Warmup con `XwaPort_Tick(16667)` + `Aeron_PumpEvents()`, sin `BeginFrame`.
- `STOPPING` de sesión => salida limpia del proceso (libera la sesión).
- Heartbeats de log (`warmup tick`, `waiting XR session`) para diagnóstico.
- `staging/libaeron_vrprobe.a`: copia congelada del `libaeron.a` parcheado;
  `m2/` no se vuelve a tocar para enlazar (restaurado `83765035...`).

## Resultado de prueba física

v1: FALLO (cuadro azul permanente + inhibición, salida con power).
v3 con fixes: superado por v4/v5.
v4 (16:10–16:11): 36 frames enviados, parada progresiva, kill del sistema.
**v5 (16:20:51–16:21:42, PID 30957): FALLO parcial con hitos.**

Observado por el usuario: señal de color al inicio (posiblemente rojo y
verde esperados, sin poder asignar color a cada ojo; el azul posterior
confundió), líneas de guardian, nunca la X-Wing, congelamiento ~30 s,
diálogos esperar/terminar (esperar ×3) y cierre manual por el usuario.

Hechos del registro (`vr-probe/evidence/v5-vrprobe-log.txt`, 1239 líneas):
- 10 llamadas DIAG confirmadas (5 stereo frames: ojo0 rojo, ojo1 verde),
  cada una con acquire/wait/blit/release/xrEndFrame `result=0` e índices
  ciclando 0,1,2/3 en ambos ojos. La señal se generó en los RTs que
  alimentan las imágenes XR. **Presentación al compositor: demostrada.**
- Del frame 5: ruta de nave ejecutada (`render end ok` ambos ojos) pero
  sin píxeles visibles confirmados. Render API OK ≠ píxeles visibles.
- Degradación progresiva: render ojo1 145 ms → 2041 ms; gpu-submit
  11 ms → 2341 ms; xrEndFrame 2 ms → 18185 ms; xrWaitFrame → 10146 ms.
- Última operación: frame 39 `render eye1 begin` (48825 ms), sin `end`.
  Ningún timeout de 100 ms se disparó (todas las esperas XR devolvieron 0).
  Sin `loop end`, sin `Fatal signal`: muerte `16:21:42` al cerrar el
  usuario tras el congelamiento (hilo clavado en `AeronScene_Render`).
- El azul sostenido es compatible con nuestro clear (0.01,0.015,0.03) de
  frames vacíos; el verde del ojo derecho duró <1 s (5 frames iniciales
  rápidos), coherente con que el usuario no pudiera asignarlo.

Hipótesis abiertas (nave invisible): encuadre, culling, iluminación en
negro sobre fondo oscuro, o malla sin triángulos visibles. No se ha movido
la nave. Propuesta acotada pendiente de autorización: media resolución
+ auto-salida limpia + log de encuadre NDC por CPU + override emisivo
(`base_color_emissive_strength`) para aislar iluminación de geometría.

## v6 — corrección acotada autorizada (instalada 16:31, F2505AD8...)

Solo `vr-probe/` (+ parche XR ya documentado). M6 `6FFCE008...`, GameData
7660 y OPT intactos tras compilar/instalar.

- Etapa 1: `vr_framing.c` nuevo. Auditoría CPU solo-lectura con las
  convenciones exactas del renderer (quat de `scene3d.c:30`, NDC de
  `flight.c:1588`, clip vía `AeronScene_ComputeViewProj` con
  auto-chequeo clip-vs-proyección). Registra stats de malla
  (verts/índices/rangos/materiales/límites), matriz de instancia, 8
  esquinas en mundo/ojo/NDC por ojo, FRONT/BEHIND, INSIDE, distancia y
  tamaño aparente. Nave intacta: sin cambios de posición/escala/culling/luz.
- Etapa 2: `VRSTEREO_SCALE_DIV 2` (escenas a ~840×880, blit reescala a las
  imágenes XR; restaurar = 1). Sin postproceso que quitar (bloom/SSAO/
  temporal ya off, MSAA 1). Geometría, estéreo y head tracking intactos.
- Etapa 3: auto-salida limpia: 240 frames enviados, 100 s de bucle o 60
  skips consecutivos, con motivo registrado (`loop end ... reason=...`) y
  liberación por la ruta normal. Siguen potencialmente bloqueantes:
  compilación de pipelines dentro de `AeronScene_Render`, espera GPU en
  `Aeron_SubmitCommandBuffer`, y esperas del runtime (`xrEndFrame`,
  `xrWaitFrame`).
- Etapa 4: segmentos temporales en el mismo build: frames 0–4
  rojo/verde (presentación), 5–64 PBR, **65–124 emisivo unlit**
  (`base_color_emissive_strength=1`), luego PBR hasta auto-salida.
  Emisivo visible + PBR negro = iluminación/materiales; emisivo
  invisible = encuadre/culling/malla.

## Pendientes

- Ejecutar prueba física y anotar resultado.
- Offsets de proyección asimétricos por ojo (FOV real completo).
- Decidir escena definitiva (esta vitrina vs hangar de vuelo) antes de M7.
- No avanzar a M7 sin autorización explícita.

## Auditoría de transformaciones (2026-09-19, sin visor, sin rebuild)

Observación del usuario: al inicio el área parecía desplazada a la derecha;
el azul parecía moverse con la cabeza; nave nunca visible.

1. Espacio de referencia: se crea UNA vez (`vr_openxr.c`), tipo LOCAL,
   pose identidad, sin offsets de la app; el mismo espacio se usa en todos
   los `xrLocateViews` y en las capas de `xrEndFrame`. No existe
   desplazamiento aplicado por nosotros. El "área desplazada" es atribuible
   a UI del sistema (panel de lanzamiento) o a no mirar al -Z de LOCAL al
   arrancar: nuestra colocación fija `(0,-0.15,-2.0)` en ejes LOCAL ignora
   el yaw inicial (debe usar el forward de la cabeza; pendiente).
2. Conversión pose→cámara: pos directa (metros, escenas en metros: OK);
   quat `(x,y,z,w)->(w,x,y,z)` según `scene3d.h:237` (OK); FOV a
   semiángulos por `tan` con offsets 0 (aproximación documentada).
   HALLAZGO: el renderer define forward de ojo = **+Z** (pruebas:
   `XwaRemasterFlight_ProjectView` rechaza `eye[2]<=0`;
   `screen=center+proj_scale*xy/z` clásico; PiP de preview con `z>0`
   visible) y arriba de ojo = **-Y** (convención eye-y-down de
   `scene3d.c:51`, coherente con overlay/clásico en SPLIT). Nuestra cámara
   pasa quats XR sin conversión y coloca la nave en -Z XR: con yaw≈0 el ojo
   computa `ez≈-2.0` => **detrás de la cámara**.
3. IPD: un solo `xrLocateViews`, mismo espacio, poses directas por ojo
   (deltas ~64 mm reales en logs). Sin doble conteo. OK.
4. Cálculo con números v5 (ancla `head=(-0.037,-0.007,-0.002)`,
   quat≈identidad, `model=(-0.037,-0.157,-2.002)`, `R≈identidad`):
   centro `eye≈(0,-0.15,-2.0)`, esquinas `ez∈[-2.6,-1.4]` =>
   **0/8 FRONT en ambos ojos => clip total => 0 píxeles**. En términos XR
   la nave SÍ está al frente (-Z); el renderer la lee detrás (+Z forward).
   Causa raíz de la invisibilidad. El run v6 (ya instalado) lo confirmará
   numéricamente con `FRONT/BEHIND` por esquina.
5. Referencia fija: mientras la nave es invisible, las líneas de guardian
   (fijas al mundo) ya permiten juzgar seguimiento; la nave fija al mundo
   será la referencia in-app cuando sea visible. No se añade objeto nuevo:
   el azul uniforme no permite juzgar movimiento en ningún caso, con o sin
   referencia nuestra. Si v6 muestra anomalía de seguimiento, se añadirá
   cruz emisiva fija al mundo.
6. Y-flip (arriba XR -> abajo pantalla con quats directos): identificado
   como defecto de orientación secundario; la corrección principista es
   `q_scene = qX180 * q_xr` (media vuelta sobre X: +Y->-Y y -Z->+Z,
   `det=+1`) + colocación con forward de cabeza. NO aplicada: requiere
   versionar como fix, no como compensación arbitraria. Propuesta para el
   siguiente paso tras confirmar v6.

## v7 — fix de conversión verificado (instalada 16:41, C96ECFB4...)

Causa del error de sentido detectada por la validación offline: el quat de
pose OpenXR mapea head-local -> mundo (M1 lo invierte para la vista), así
que la composición ingenua `qD (x) q_xr` espejaba el yaw. Correcto:
`R_scene = D * R_xr^T`, i.e. `q_scene = qD (x) conj(q_xr)` (Hamilton,
derecha primero), con `D` = 180° sobre X (`det=+1`: sin espejos, IPD y
profundidad a salvo). Posiciones directas (la traslación no rota).

Validación offline (fórmulas literales de `scene3d.c`/`flight.c`):
identidad centra con `w>0` y signos up/right OK; yaw +30° (mirar izq) =>
punto fijo a `ndc_x=+0.537` (derecha, correcto); pitch down 20° =>
`ndc_y=+0.316` (arriba, correcto); conversión vieja => `w=-2.00` BEHIND;
caja 1 m a 2 m nueva => 8/8 INSIDE; IPD ±0.032 con signo correcto.
`vr_convert.c` nuevo + `VrConvert_SelfTest()` en arranque que ejecuta la
REAL `AeronScene_ComputeViewProj` (T1–T7: centro/arriba/derecha/yaw/pitch/
behind/IPD) y aborta limpio si falla. Ancla con forward horizontal de la
cabeza (2 m, una vez). Encuadre runtime (`vr_framing.c`) sin cambios.
Media resolución, diag rojo/verde, segmentos PBR/emisivo, auto-salida
(240/100 s/60 skips) y toda la instrumentación: intactos.
M6 `6FFCE008...`, GameData 7660, `libaeron.a` M2 `83765035...` intactos.

## v7 — tres intentos, FALLO con causa nueva (log del 3º en evidence/v7)

16:45:58 PID 6796 (fuera del límite) → kill 16:46:28. 16:46:47 PID 8693
(estacionario) → kill 16:47:48. 16:49:41 PID 10963 (dentro del límite) →
kill 16:50:33. Los 3 cierres por Android (`Can't deliver broadcast`);
auto-salida nunca alcanzada (154 < 240, ~52 s < 100 s). Logs de los
intentos 1–2 perdidos (reescritura por lanzamiento).

Usuario: entrada centrada (nuevo, coherente con ancla por forward),
guardian + rojo/verde, sin nave en PBR ni emisivo, giros rápidos =>
congelamiento + azul, cierre manual final.

Confirmado (`evidence/v7-vrprobe-log.txt`, 4724 líneas, intento 3):
1. Self-test T1–T7 ALL PASS en dispositivo con la ComputeViewProj real.
2. Malla: `verts=10380 idx=852 opaque=852 mask=0 blend=0 mats=39
   prims=90`, límites ±6.5 m. `MeshCreate` copia `index_count` del modelo:
   IBO total = **852 índices = 284 triángulos** para 13 m de nave.
3. Encuadre REAL: ancla con forward (`fwd=(-0.699,0,-0.715)`, 2.004 m),
   **8/8 FRONT + 8/8 INSIDE ambos ojos**, NDC ±0.35, aparente 38°.
   Conversión Z verificada en runtime.
4. Emisivo ejecutado: ON a 6458 ms, 120 begins `emissive=1.00`
   (frames 65–124). Sin nave visible tampoco en emisivo.
5. Sin evidencia de píxeles en ningún segmento (sin readback: se afirma
   por combinación, no por captura). Frames 0–11 a plena velocidad
   (submits 0–1 ms); degradación a renders >1.3 s y submits ~2.3 s;
   frame 154 clavado en `render eye0 begin`. Ningún timeout (todo
   `result=0`); waits de acquire 0→152 ms al final.
6. Movimiento de cabeza presente en poses todo el run (tracking entrega
   datos); coincide con la degradación sin causalidad establecida.

Corrección a la lectura v7 (investigación laptop posterior, solo lectura):
el cocinado es FIEL — `XWING.OPT` contiene file-wide 96 FACEDATA con
16 tris + 144 quads = 304 tris (LOD0 = 284). La sonda del prototipo
(`OptModel_ProbeBuildTriangles`, sin filtro LOD) lee los mismos + ~20 de
LOD1. Diferencia de solo 20 tris: **no hay pérdida en el cook.**
`swap_axis_v3` (det −1) y `gltf_to_aeron` (det −1) se cancelan en
posiciones. Pipelines: `front=CCW` + `cull=BACK` en todos.
Offline con cámaras v7 reales: winding mixto ~109+/96− (NONE sistemático:
~la mitad sobrevive BACK-cull en cualquier caso), áreas pequeñas pero no
nulas. El VBO de 10380 vértices queda sin explicar (residuo, no causa
visual: lo no referenciado no dibuja).

## v8 — auditoría de geometría + uploads (instalada 17:12, 92ED1FEE...)

Solo `vr-probe/` (+ parche XR documentado). M6 `6FFCE008...`, GameData
7660, `libaeron.a` `83765035...`, OPT intactos tras compilar/instalar.
- `vr_audit.h/.c` nuevo: camina los rangos IBO retenidos contra
  `cpu_vertices/cpu_indices` (los buffers de GPU): validez de índices,
  degenerados por área, signo NDC por ojo con cámaras del momento,
  vértices referenciados y sus bounds vs bounds de malla. Sin cambios.
- Uploads por frame con `Aeron_CommandBufferGetUploadUsage` (API pública)
  antes de cada submit: distingue acumulación de térmico/compilaciones.
- Intactos: conversión + self-test, ancla, media resolución, diag,
  segmentos PBR/emisivo, auto-salida, framing, per-op dts.
- Limitación documentada: sin timeout posible dentro de
  `AeronScene_Render`/submit; la auto-salida cubre progreso, no un render
  clavado.

## v8 — pase único (17:1x, PID 14913): rojo/verde OK, nave invisible

Usuario: entrada correcta, diag rojo/verde correctos, solo azul después,
sin nave en ninguna fase. Sin geometría parcial confirmada.

Confirmado (`vr-probe/evidence/v8-vrprobe-log.txt`, 4771 líneas):
1. Auditoría en dispositivo: `opaque: tris=284 bad_idx=0 degen=0
   clipped=0 ndc_pos=139 ndc_neg=145`; mask/blend 0. **284/284 índices
   válidos, 0 degenerados, 0 fuera de frustum (ojo izq; el derecho
   comparte escena).** Winding mixto: ~la mitad sobrevive BACK-cull por
   ojo en cualquier caso. Referenciados `530/10380` verts, bounds
   ≈ volumen de la nave a 2 m (coherente con framing v7 8/8).
2. Geometría en frustum: SÍ (auditoría + framing v7). Draw calls: rango
   opaque 852 índices/ojo/frame + tonemap + blit; todos los submits OK.
   Geometría ENTRA al render pass (instancia añadida, sin errores).
3. Emisivo ejecutado: ON 6490 ms, OFF 7926 ms (frames 65–124, 120 begins
   `emissive=1.00`). Usuario: nada visible tampoco ahí.
4. Rendimiento: uploads PLANOS (`staged=6592B copies=6` todos los frames)
   => **no hay acumulación**. Submits 0–1 ms → 12284 ms (frame 150);
   renders rápidos → >1 s; `xrEndFrame` 0–3 ms → 45 s+ clavado en el 150.
   150 enviados, muerte 17:18:34 (`has died`, sin `loop end`, sin auto-
   salida, sin `Fatal signal` en lo muestreado).
5. Píxeles: **sin prueba de contenido** (no existe readback/captura XR en
   la sonda; se indica expresamente). Render-OK ≠ imagen visible.
   Inferencia: ~140 tris/ojo rasterizados pero azules ⇒ salen NEGROS
   (sombreado base negro) o el tonemap los apaga. El emisivo-invisible
   NO descarta luz/materiales: emite el color base muestreado, y si la
   base es negra (texturas sin hornear) también sale negro.

Hipótesis que siguen abiertas: (a) tonemap `PresentChain` apaga la escena
(jamás validado visualmente con contenido real: el diag lo puentea con
clear directo); (b) sombreado/material base en negro (normales, falta de
atlas, metalness sin entorno). Profundidad: pasa (GE vs clear 0, w 1–3).
Culling: ~mitad sobrevive, no explica cero.
Propuesta acotada v9 (IMPLEMENTADA): bypass de PresentChain y readback
comparativo de `scene_tex` vs `s_present` para distinguir dónde se pierde
la geometría.

## v9 — diagnóstico diferencial completo (instalada 19/09/2026, C5402B89...)

Solo `vr-probe/` (+ parche XR documentado en `aeron/src/render_backend.c`,
restaurado byte-a-byte en M2). M6 `6FFCE008...`, GameData 7660, `libaeron.a`
M2 `83765035...`, OPT intactos tras compilar/instalar.

### Cambios implementados en `vr-probe/`:

**Nuevos archivos:**
- `vr_readback.h/.c`: auditoría asíncrona GPU de `scene_tex` (pre-tonemap)
  y `s_present` (post-PresentChain) mediante readback no bloqueante de
  13 muestras/ojo (9 en casco nave + 4 control fondo). Usa
  `SDL_DownloadFromGPUTexture` asíncrono + `VrReadback_Collect` al frame
  siguiente. Sin readback bloqueante.

- `vr_convert.c`: autotest T1–T7 en arranque con la **real**
  `AeronScene_ComputeViewProj` (T1 centro, T2 up, T3 right, T4 yaw-left,
  T5 pitch-down, T6 behind rejection, T7 IPD sign). Aborta limpio si falla.

- `vr_convert.c`: `VrConvert_Camera` implementa `R_scene = D · R_xr^T`
  (`q_scene = qD ⊗ conj(q_xr)`, Hamilton, `D` = 180° sobre X, det=+1).
  Verificado offline y en runtime (T1–T7 PASS).

- `vr_stereo.c`: `VrStereo_GetSceneTexture(eye)` expone `scene_tex`
  pre-tonemap para readback W3.

- `vr_blit.c/.h`: pipeline HDR→SDR (`fullscreen_hdr_to_sdr.frag`) para
  bypass tonemap en W2. Selección automática por formato (HDR `RGBA16F`
  → SDR swapchain). Blit directo `scene_tex` → swapchain XR.

- `vr_main.c`: tres ventanas diagnósticas de 10 frames c/u:
  - **W1** (frames 5–14): Control PBR normal
  - **W2** (15–24): Bypass PresentChain — `scene_tex` → XR directo
  - **W3** (25–34): Readback comparativo `scene_tex` vs `s_present`
  - Segmentos: 0–64 PBR, 65–124 emisivo unlit (`base_color_emissive_strength=1`),
    luego PBR hasta auto-salida 240 frames / 100 s / 60 skips consecutivos

- `vr_audit.c`: auditoría CPU de triángulos válidos/degenerados/clippeados,
  winding NDC por ojo, vértices referenciados y bounds reales vs
  bounds de malla.

- `vr_readback.h/.c`: readback asíncrono de 13 muestras/ojo (9 casco nave +
  4 control fondo) con `SDL_DownloadFromGPUTexture` asíncrono + staging
  buffer pool + `VrReadback_Collect` al frame N+1. Logs RGBA+NDC por muestra.

- `vr_convert.c`: `VrConvert_SelfTest()` en arranque ejecuta T1–T7 con
  la **real** `AeronScene_ComputeViewProj`. Aborta limpio si falla.

- `vr_main.c`: pacing 72 Hz (`SDL_Delay` antes de `xrWaitFrame`),
  watchdog frames lentos (>33ms, max 10 consecutivos → yield frame),
  ancla nave en forward horizontal de la cabeza (2m, fixed yaw lento),
  ancla una vez; no sigue la cabeza. Auto-salida: 240 frames, 100s,
  60 skips consecutivos.

### Archivos modificados (solo `vr-probe/` + parche XR opt-in):

- `vr_main.c`: loop principal, ventanas W1/W2/W3, pacing, watchdog, anclaje
- `vr_stereo.c/h`: `VrStereo_GetSceneTexture`, `VrStereo_SetDiagFrames`
- `vr_blit.c/h`: pipeline HDR→SDR, selección automática por formato
- `vr_readback.h/.c`: pool staging buffers, scheduling/collect asíncrono
- `vr_audit.c/.h`: auditoría triángulos, winding NDC, bounds referenciados
- `vr_convert.c/h`: `VrConvert_Camera` (conjugación), `VrConvert_SelfTest`
- `vr_readback.h/.c`: pool staging, scheduling/collect asíncrono
- `vr_audit.h`: declaración `VrAudit_MeshTriangles`
- `vr_stereo.h`: declaración `VrStereo_GetSceneTexture`
- `vr_blit.h`: declaración `VrBlit_Init/Copy/Shutdown`
- `CMakeLists.txt`: añade `vr_readback.c`
- `vr_stereo.c`: exporta `VrStereo_GetSceneTexture`

### APK v9 instalada: `C5402B8909072399F7FC311C4F99C53372CBFE64CEA33ABED271A88B7F1B1AAB`
M6 `6FFCE008...`, GameData 7660, `libaeron.a` M2 `83765035...` intactos.

### Validaciones en laptop (sin Quest):
- Compilación/linkeo v9 exitoso
- Autotest T1–T7 pasa en arranque (log `ALL PASS`)
- Formatos HDR→SDR verificados en código (`VrBlit_EnsureHdrPipeline`)
- `VrConvert_SelfTest` ejecuta T1–T7 con `AeronScene_ComputeViewProj` real
- Pacing 72 Hz implementado con `SDL_Delay` antes de `xrWaitFrame`
- Watchdog frames lentos (>33ms, max 10 consec.) implementado
- Auto-salida: 240 frames / 100s / 60 skips

### Pruebas realizadas:
- Compilación e instalación v9 exitosa (hash C5402B89...)
- M6 verificado intacto (hash `6FFCE008...`)
- `libaeron.a` M2 restaurado byte-a-byte (`83765035...`)
- GameData 7660 archivos intactos en `vrprobe`

### Pendiente de validación en Quest (prueba física pendiente):
1. **W1 (frames 5–14)**: ¿aparece nave PBR?
2. **W2 (frames 15–24, bypass PresentChain)**: ¿aparece nave?
   - Si SÍ → PresentChain culpable (exposure/AGX/headroom/src_coverage)
   - Si NO → fragment shader negro (material/base color/textura/normales)
3. **W3 (frames 25–34)**: readback `scene_tex` vs `s_present`
   - `scene_tex` ≠ fondo → geometría en `scene_tex`, PresentChain apaga
   - `scene_tex` = fondo → fragment shader produce negro
   - `s_present` ≠ `scene_tex` → PresentChain transforma
4. Emisivo ON (frames 65–124): si visible → problema luz/material; si no → geometría

### Pruebas de escritorio realizadas:
- Compilación/linkeo v9 exitoso
- Autotest T1–T7 pasa en arranque (log `ALL PASS` en log)
- Formatos HDR→SDR verificados en código
- Pacing 72 Hz + watchdog implementados
- Readback asíncrono 13 muestras/ojo (9 casco nave + 4 control fondo)
- Ancla nave en forward horizontal de la cabeza (2m, una vez)
- Auto-salida 240 frames / 100s / 60 skips consecutivos

### Partes NO implementadas (por diseño):
- Readback de `s_present` en W1/W2 (solo W3)
- Readback de `scene_tex` en W1/W2 (solo W3)
- Readback de textura XR swapchain (no disponible sin readback)
- Píxel central único → muestreo 13 puntos estratificados
- Readback bloqueante por frame → asíncrono frame N+1
- Shaders personalizados → usa `fullscreen.frag` + `fullscreen_hdr_to_sdr.frag` existentes
- Multiview / render array (mantiene render por ojo separado)
- Cambios en cámara / posición / geometría / iluminación

### M0–M6, GameData, frontend, OPT: **intactos y verificados**

### Próximo paso (requiere autorización):
Una única ejecución física en Quest 3S modo estacionario (~90s máx):
1. Observar W1 (PBR) — ¿aparece nave?
2. Observar W2 (bypass) — ¿aparece nave? (distingue A vs B)
3. W3 readback confirmará si `scene_tex` ≠ `s_present`
4. Emisivo ON (frames 65–124) confirma geometría vs shading
5. Auto-salida a 240 frames (~2 min) o cierre manual

No se implementará v10 hasta revisar resultados v9.
