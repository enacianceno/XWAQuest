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
# M8test3 — candidato de cockpit/mundo, no PASS físico

## Actualización de recuperación — 2026-09-26

Fuente de relevo: `C:\OpenXWA\XWAQuest\m8test3\CHECKPOINT-M8TEST3.md`.
Esta actualización sustituye el estado de instalación/provisión descrito en el
corte histórico inferior, sin cambiar sus resultados de build.

APK de SHA256 3C43C3D5D6E75CB80134CC6C4422E07914A3D13E771AC05D0158960ED6F31175
instalado con `adb install -r`: Success. Package m8test3, versión 0.8.2-test3,
versionCode4 y ABI arm64-v8a confirmados por dumpsys. No se lanzó la aplicación.

El aprovisionador generó manifiesto de origen: 7744 assets / 847224595 bytes.
Terminó con código1 sin HASHES_OK ni DATA_READY. Al recuperar, ADB no encuentra
dispositivos. Destino/staging no se pudieron inspeccionar: estado desconocido,
NO declarar copia verificada ni borrar staging. Log conservado:
`evidence/gamedata/quest-20260926-013618/device-provision.log`.
No se inició logcat test3. Última instrucción: recuperar/verificar datos y detenerse,
NO lanzar todavía. Se necesita reconexión del Quest para continuar esa verificación.

No se modificó código ni se recompiló. Hash actual del APK coincide. Inventario
de fuentes test3 sin diferencias antes de actualizar documentación; m8test2
coincide con baseline-source-hashes.csv (0 diferencias) y su APK conserva hash
FF59ABF78973D0D469D5FAC4A93BAF230F30F38D7B8BE6F4CCDD98F602B6004C.
No se escribieron archivos de m8 ni se modificó GameData origen.

Directorio: C:\OpenXWA\XWAQuest\m8test3. Esta variante es una candidata a promoción,
no un experimento desechable. No se modifica todavía m8 oficial. La implementación
completa del primer prototipo jugable sigue abierta: este APK prioriza validar
P0–P3 antes de añadir controles a un cockpit cuya posición todavía no fue vista.

## Baseline y preservación

Copiado de m8test2, incluyendo fuentes y artefactos heredados. Se usan
build-android-t3 y .gradle-t3 para evitar reutilizar el CMakeCache del directorio
original. No se borraron caches/copias existentes. Sólo se escribió en m8test3.
Manifest de fuentes originales: evidence/baseline-source-hashes.csv. Se volvieron
a comprobar contra m8test2 al terminar: sin cambios. APK m8test2 conservado:
FF59ABF78973D0D469D5FAC4A93BAF230F30F38D7B8BE6F4CCDD98F602B6004C.

Se conserva literalmente el bloque que llama a
__real_SDL_WaitAndAcquireGPUSwapchainTexture en el command buffer XR y el marcador
M8T2_CLEANUP_ACQUIRE. La llamada real aparece también en el binario final.
No se sustituyó por WaitIdle ni por otro mecanismo de fences. Riesgo heredado:
la adquisición/presentación WSI adicional puede afectar pacing y flicker; preservar
la evidencia de memoria de test2 no equivale a demostrar estabilidad de test3.

## Cambios exactos frente a m8test2

Todas las rutas relativas a C:\OpenXWA\XWAQuest\m8test3\:

| Archivo | Cambio |
|---|---|
| Build-Apk.ps1 | Cache de proyecto Gradle independiente .gradle-t3. |
| Build-Target.ps1 | Build nativo independiente build-android-t3. |
| CMakeLists.txt | Wrapper observador de Skirmish_GenerateMission. |
| diagnostics.c | Logs antes/después del generador real, sin alterar resultado. |
| frame_bridge.c | Título de ventana test3; bloque de limpieza intacto. |
| provision-gamedata.sh | SRC=m8test2, DST=m8test3; no ejecutado. Mantiene selección y hashes de assets, exclusión de pilotos y protección contra overwrite. |
| settings.gradle | Identidad del proyecto XWAQuestM8T3. |
| app/build.gradle | Package/namespace m8test3, versionCode 4, versionName 0.8.2-test3. |
| app/src/main/AndroidManifest.xml | Etiqueta XWAQuest M8test3. |
| app/src/main/java/org/openxwa/xwaquest/m8test2/M8Activity.java | Declaración package m8test3 y tag propio; conserva ruta privada files/GameData. La ruta física heredada del archivo no determina el package Java. |
| vr_flight_bridge.h | ABI privado 2: nombre OPT, matriz cockpit en metros, variante y gate; indicador de Death Star para estrellas. |
| vr_flight_bridge.c | Copia cockpit seat-0 desde snapshot real; matriz independiente del visor; log del jugador y model_index desde HUD committed cuando válido. |
| vr_flight_renderer.c | Lookup del cockpit OPT real tras asset sync, instancia por ojo, estrellas reales de OpenXWA y liberación de recursos propios. No cambia el TIE, camera conversion, FOV ni CULL_BACK. |
| tests/Run-CPU-Tests.ps1 | Ejecuta test adicional de asiento. |
| tests/Verify-APK.ps1 | Package/version test3, build actual, marcadores nuevos y de limpieza heredados. |

Nuevos: t3_features.h (flags), t3_seat_math.h (helper matemático),
tests/seat_math_test.c, este reporte y evidencias bajo evidence/.
La comparación automatizada inicial está en evidence/changed-vs-m8test2.txt;
la lista anterior incluye CMakeLists.txt añadido después de esa comparación.
Los otros archivos son copias sin cambios, no autoría nueva de esta iteración.

## Integración de cockpit y mundo

Cadena conservada: tick XWA real una vez -> snapshot committed -> copia privada
VrFlightSnapshot -> AeronScene por ojo -> SDR -> VrBlit -> OpenXR.
Modelo cockpit: snapshot.cockpit.model_name, sólo asiento de piloto y cockpit
visible. Lookup exige runtime OPT, no GLB ni geometría artificial. Si no hay modelo,
se registra M8T3_COCKPIT_UNAVAILABLE; no se sustituye por un cockpit ficticio.

El helper T3_SeatMatrix especializa fl_cockpit_model_matrix de OpenXWA para seat 0:
misma base del objeto, offset negativo hardpoint_world + camera_pan/16, convertido
a metros. La traslación se ancla al jugador, no al ojo físico. VrEye_Compose sigue
calculando vehículo × pose LOCAL de cada ojo. Ninguna pose del visor se escribe
en la simulación. No se cambia escala, handedness, FOV, clipping o culling.

Cockpit y TIE usan textura original con emisión de base_color=1 (unlit) como la
ruta CS1 existente; iluminación final, articulación interna, instrumentos y HUD
no están implementados aquí. No se dibuja el exterior del jugador desde dentro.

El entorno espacial reutiliza XwaRemasterSkyStars_Create/Prepare/Draw y los shaders
ya disponibles sky_stars.vert/frag. Base mundo->cubo idéntica al renderer OpenXWA;
parámetros experimentales acotados (grid 32, density .4, brightness/exposure 1),
reloj de simulación. No representa todavía todos los backdrops/planetas/objetos
de la misión. El TIE sigue la pose real y la IA de XWA; no se altera combate/audio.

Flags centrales: M8T3_COCKPIT=1, M8T3_STARS=1. AUTO_SETUP, FLIGHT_CONTROLS y
SPATIAL_MENU=0 indican características aún no implementadas; activarlas por sí
solo no las implementa. No se anuncian marcadores de control no existente.

## Investigación quick test y frontend: pendientes explícitos

MissionSetup_LoadSkirmishFile carga slots/loadouts del formato real SKM;
Skirmish_GenerateMission produce la misión real. Quick Start no es sinónimo de
Quick Skirmish: el botón quickstart se muestra cuando missionDirectoryId NO es
SKIRMISH y establece otra transición. No se forzó ese flag para simular setup.
AutoPilot/auto-setup/auto-flight NO implementados en este APK. La siguiente
integración debe ocurrir después de inicializar correctamente MissionSetup y
usar slots/loadouts reales, conservando validación y Begin. Por ahora la prueba
requiere reproducir manualmente el setup que funcionó en test2.

La ruta 2D heredada sigue copiando output a ambos ojos. No hay una Quad/spatial
screen distinta en el código test2 que explique de forma concluyente la pantalla
pre-flight observada. Una transición temprana a immersive/hangar puede producir
esa diferencia, pero no está demostrada. No se inventó esa correspondencia.
Distancia/head-lock y flicker de menús quedan pendientes en este corte.

## Validación local

Tests existentes eye-math PASS; VrConvert_SelfTest 7/7 PASS; test nuevo seat-math
PASS (signo/unidades/pan/16, inmutabilidad con head pose, rechazo NaN).
ARM64 build PASS, reutilizando motor/dependencias/shaders existentes.
APK BUILD SUCCESSFUL. Auditoría estática PASS: package independiente,
versionCode 4, versionName 0.8.2-test3, ABI arm64-v8a.
Shaders de estrellas dentro del APK con magic SPIR-V correcto:
sky_stars.vert.spv 9140 bytes, sky_stars.frag.spv 2664 bytes.
Biblioteca extraída y build actual stripped idénticos SHA256:
7151295A020A176FB3F5A22B4DEFA12EC990D8605260D61FD72D32FF06144429.

Warnings: AGP 8.1.4 probado hasta compileSdk34 (se usa35); metadata riscv64
ignorada; opciones Java source/target8 obsoletas y APIs deprecated en fuentes
heredadas SDL. No errores nativos ni APK.

APK: C:\OpenXWA\XWAQuest\m8test3\app\build\outputs\apk\debug\app-debug.apk
Package: org.openxwa.xwaquest.m8test3
SHA256: 3C43C3D5D6E75CB80134CC6C4422E07914A3D13E771AC05D0158960ED6F31175
Tamaño: 13.299.765 bytes.

Marcadores: M8T3_PLAYER (player_obj_idx/object_type/craft/model_index),
M8T3_MISSION_GENERATION begin/end, M8T3_COCKPIT_UNAVAILABLE,
M8T3_COCKPIT_DRAW_RECORDED por ojo y M8T3_STARS. Se conservan M8_LAUNCH,
M8_VR_TARGET_READY, M8_VR_FIRST_STEREO_FRAME y M8T2_CLEANUP_ACQUIRE.
model_index=-1 significa que el HUD committed aún no lo proporcionó; no se
inventa un índice. Todos los mensajes DRAW son recording, no PASS visual.

## Corte de prioridades y siguiente prueba

- P0: mecanismo preservado y pruebas locales; estabilidad física test3 pendiente.
- P1: gate X-Wing heredado y logs ampliados; confirmar en ejecución test3.
- P2: cockpit y campo estelar integrados; coherencia/posición/visibilidad pendientes.
- P3: TIE real heredado, pendiente comprobar junto al cockpit.
- P4: controles pendientes; se requiere validar world+cockpit primero.
- P5: pantalla espacial pendiente, ruta anterior preservada.
- P6: sin cambio de explosiones; efectos actuales heredados.

No se instaló ni usó ADB. El package independiente necesita GameData privado
antes de probarse; no comparte automáticamente el de test2. El aprovisionador
retargeteado no se ha ejecutado ni validado contra dispositivo y no copia pilotos.

Antes del visor: instalación/autorización, provisión segura de assets, verificación
y captura continua. Después: crear/seleccionar m8test mediante el flujo original;
Combat Simulator -> Single Player -> Quick Skirmish; TEAM ONE player X-Wing,
TEAM TWO AI TIE Fighter, una nave/una oleada por equipo; Begin real.
Observar cockpit reconocible y TIE, estrellas, profundidad por ojo y ausencia de
inversión. Girar cabeza y desplazarse ligeramente: cockpit no debe seguir la
cabeza y nave no debe girar por ello. Seguir al TIE y observar audio/IA/destrucción.
No hay controles de vuelo nuevos para probar todavía. Si sólo aparece TIE sin
cockpit, P2 NO PASS: recuperar diagnóstico antes de controles.

No declarar primer prototipo jugable completo ni promover a m8 hasta validación.
