# XWAQUEST — PORT STANDALONE DE X-WING ALLIANCE A META QUEST

## Objetivo Maestro
Port de Star Wars: X-Wing Alliance a Meta Quest 3 y 3S,
utilizando archivos originales del juego, menús funcionales,
misiones completas, jugabilidad VR estereoscópica con
seguimiento de cabeza y controles Meta Quest.

## Estado de Hitos

### M0-M1: Snapshot y SDL3+Vulkan+OpenXR
- OpenXWA f063965a, Aeron 572b3684, SDL3 8f8ed757
- Swapchains 1680×1760, renderizado mono
- ✅ Completado, preservado intacto

### M2: Motor compilado para Android ARM64
- Static libs, shaders, dependencias enlazadas
- ✅ Completado, preservado intacto

### M3: Inicialización del motor
- SDL_main → Aeron_Init → VFS/shaders/audio
- ✅ Completado, preservado intacto

### M4: Validación de GameData
- 7,652 archivos / 757MB de Steam
- ✅ Completado, preservado intacto

### M5: Frontend renderizado
- 2,040+ ticks, intros SMUSH, concourse visible
- ✅ Completado, preservado intacto

### M6: Creación de piloto
- Piloto "neto" creado (152KB .plt)
- Puntero Quest funcionando
- ✅ Completado, preservado intacto

### M7A: Primera misión — compilado y ejecutado
- Guardia de vuelo eliminada
- 7 wrappers de instrumentación de vuelo añadidos
- Ejecutado en Quest 3S: 1,080+ ticks, frontend estable, piloto creado
- **Crash identificado:** SIGSEGV en ImVimaDecodeAdpcm (null pointer en decodificación ADPCM de música del frontend)
- Causa: decodificador lee buffer sin bounds check; no es archivo corrupto
- El crash bloquea navegación a "Play Mission"
- ✅ M7A completado como hito de observación. Preservado intacto.

### M7B: PRIMERA MISIÓN JUGABLE — ACTIVO
- **Objetivo:** Misión original jugable en pantalla virtual del Quest
- **Fix mínimo:** Desactivar música vía config (music_volume: 0) O bounds check en imcodec.c
- **Código:** Nuevo directorio m7/ (separado de M7A)
- **Controles:** Gamepads Quest detectados por SDL, engine listo
- **Presentación:** Misma ventana 2D de Quest (pantalla virtual)
- ⏳ Pendiente: implementar fix, compilar, probar en Quest

## Arquitectura Vigente
```
Quest 3S (ARM64-v8a)
├── RuntimeActivity.java (IME + puntero Quest + gamepad)
├── input_android.c (adaptador SDL + teclado virtual)
├── trace.c (wrappers: init + vuelo + present + audio)
├── main.c (entry point, sin guardia de vuelo)
├── Aeron (Vulkan + SDL_GPU + Audio)
├── OpenXWA (Frontend → Flight → Render)
└── GameData (7,652 archivos de Steam)
```

## Hallazgos de XWAU VR
- XWAU usa ddraw.dll hook sobre ejecutable original (no aplica)
- Conceptos transportables: separación IPD, pose correction, dynamic cockpit
- Implementación VR completa: MIT licencia
- Quest usa OpenXR nativo (no OpenVR/SteamVR)

## Errores Resueltos
- M7A crash ADPCM: diagnosticado, fix vía config o imcodec.c bounds check

## Errores Pendientes
- M7B: Aplicar fix de audio y probar misión en Quest

## Próxima Tarea Concreta
Aplicar fix de M7B (desactivar música o bounds check), compilar nueva APK,
instalar en Quest 3S, navegar a "Play Mission", verificar que
XwaFlightTask_Init → Mission_Init → Flight_Step ejecutan correctamente.
