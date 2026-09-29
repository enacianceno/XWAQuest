# M8.2-CS1 — APK listo para prueba autorizada

Estado: **BUILD_OK_NOT_RUNTIME_TESTED**. Codex sigue siendo el único integrador.
No ADB, instalación, ejecución en Quest, cambios de GameData ni avance a M8.3.

## Estado recuperado

Se conservaron bridge, contrato, matemática de ojos, renderer estéreo, wrappers y
versionado CS1 descritos en CHECKPOINT-M8.2-CS1.md. Branch:
`codex/m1-sdl-gpu-openxr`; repositorio sin commits, fuentes untracked. No reset,
checkout, clean ni descarte de cambios. El antiguo APK M8.1 fue sustituido sólo
en el directorio de salida del build local, nunca instalado.

Copia de las fuentes anteriores a esta continuación:
`C:\OpenXWA\XWAQuest\m8\evidence\checkpoint-cs1-pre-apk.zip`
SHA256: `76E046A9E2EEF868D3C63DF735428276C6C8B346016EBB5CBABFA53B88191F00`.
Incluye manifiesto SHA256 por archivo. El checkpoint dentro del ZIP es histórico.

## Cambios de esta continuación

Todas las rutas siguientes relativas a `C:\OpenXWA\XWAQuest\m8\`:

- `vr_flight_renderer.c`: diagnósticos BeginFrame por cambio de motivo:
  state_invalid, flags_not_ready, object_index_invalid, mesh_missing,
  not_runtime_opt, invalid_mesh/index_count. RenderEye informa arguments_invalid,
  tick_mismatch, ensure_eye_failure, Compose_failure, Scene_Begin_failure,
  Scene_Render_failure y fallos de textura/present pass. Añadida validación de
  índices antes de acceder al snapshot. Retorno 2D diagnosticado; fallo de ojo
  conserva la ruta fatal existente, sin éxito simulado.
- `frame_bridge.c`: ejecuta VrConvert_SelfTest al iniciar Aeron y detiene el inicio
  si falla. VrLog permite los resultados del self-test; no los silencia.
- `tests/convert_test.c` nuevo: ejecuta VrConvert_SelfTest; stdout sólo sustituye
  el destino del log, no matemática ni subsistemas del motor.
- `tests/Run-CPU-Tests.ps1` nuevo: pruebas CPU reproducibles. Genera bajo tests
  una unidad con los cinco cuerpos exactos de funciones de cámara de scene3d.c.
  Guarda hashes de origen y extracción. Compila vr_convert.c original sin cambios.
  Esto evita enlazar funciones GPU no utilizadas del fichero monolítico en COFF;
  no introduce stubs. Intentos previos de enlazar el fichero completo fallaron por
  símbolos GPU no incluidos; la prueba final pasa con extracción exacta.
- `tests/Verify-APK.ps1` nuevo: comprueba package/version/ABI, extrae la biblioteca,
  compara SHA256 con el build actual tras strip, verifica símbolos y marcadores.
- Este reporte y addendum al checkpoint; artefactos, pruebas, ZIP y logs bajo m8.

No se modificaron fuentes fuera de m8, ni coordenadas, handedness, escala en metros,
composición vehicle × head, CULL_BACK, FOV, controles, shaders ni motor.
La lista completa de archivos CS1 anteriores sigue en el checkpoint, secciones 3–4.

## Pruebas y build

Desde `C:\OpenXWA\XWAQuest\m8`, en este orden:

1. `./tests/Run-CPU-Tests.ps1`: PASS. Eye-math: identity, IPD, yaw cabeza/nave,
   composición y rechazo inválidos/no escritura. VrConvert_SelfTest: 7/7 PASS
   (center/up/right/yaw/pitch/behind/IPD), con proyección real de Aeron.
2. `./Build-Target.ps1`: PASS, dos objetos recompilados y enlace ARM64.
   Sin warnings/errores nativos. Reutiliza archives M2/Aeron existentes.
3. `./Build-Apk.ps1`: BUILD SUCCESSFUL, 53 s, 10 tareas ejecutadas y 22 reutilizadas.
4. `./tests/Verify-APK.ps1`: APK_STATIC_AUDIT PASS.

Warnings del APK: AGP 8.1.4 probado hasta compileSdk 34, proyecto usa 35;
metadata riscv64 ignorada. ABI empaquetada verificada exclusivamente arm64-v8a.

## APK verificado

Ruta: `C:\OpenXWA\XWAQuest\m8\app\build\outputs\apk\debug\app-debug.apk`

- Package: `org.openxwa.xwaquest.m8`
- versionName: `0.8.2-cs1`; versionCode: `2`
- ABI: `arm64-v8a`; libOpenXWAM8.so es ELF AArch64.
- Tamaño: 13.294.505 bytes.
- SHA256: `C2E2C8F7AEA2E7ACFD4310522D0AA19189A613A22890FC3ADDDE997386136FBD`

Biblioteca extraída de `lib/arm64-v8a/libOpenXWAM8.so` y build actual después del
mismo strip tienen SHA256 idéntico:
`95BE552B0842CE405AD55CAE117549E51F12EC9379DB6605226AC56482DB778A`.
La comparación es byte por byte, no sólo por presencia de nombres.

Símbolos definidos encontrados en el APK: familia VrFlightBridge, familia
VrFlightRenderer, __wrap_Mission_Init y VrConvert_SelfTest.
Marcadores encontrados en esa biblioteca:

- M8_VR_FLIGHT_ENTER
- M8_VR_PLAYER_READY
- M8_VR_TARGET_READY
- M8_VR_SNAPSHOT_READY
- M8_VR_IMMERSIVE_ENTER
- M8_VR_TARGET_DRAW_RECORDED
- M8_VR_FIRST_STEREO_FRAME
- M8_XR_FRAME_PRESENTED

## Evidencias y límites

Logs bajo `C:\OpenXWA\XWAQuest\m8\evidence\`:
`eye-math-test.log`, `convert-test.log`, `convert-source-hashes.log`,
`native-build.log`, `apk-build.log` y `apk-cs1-audit\result.txt`.
La carpeta apk-cs1-audit también contiene badging.txt, elf.txt, symbols.txt,
markers.txt, native-entries.txt y las dos bibliotecas comparadas.

No hay blocker de build/empaquetado. No está demostrada ejecución GPU, captura de
una misión, visibilidad del TIE, estereoscopía ni interacción CS1 en Quest. Los
marcadores aquí son evidencia estática, no logs runtime. Siguen los límites del
checkpoint (FOV simétrico aproximado, sin recenter, coste del render plano adicional,
lifetime single-thread). Los pendientes de revisión content_revision/POSE_UPDATED
del bridge no fueron modificados en esta continuación; no condicionan el gate de
render por tick/flags. No se afirma que estén resueltos.

**M8.2-CS1 APK READY FOR QUEST TEST**
