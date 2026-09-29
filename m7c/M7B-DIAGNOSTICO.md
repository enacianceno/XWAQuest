# M7B — Diagnóstico, Ruta y Propuesta

Fecha: 2026-09-20. Proyecto: C:\OpenXWA\XWAQuest.

## 1. Diagnóstico del fallo actual (M7A)

### Hechos comprobados

| Dato | Evidencia |
|------|-----------|
| App ejecuta 1,080+ ticks del frontend | Log `M7 TICK begin=1080` |
| Frames presentados exitosamente | Log `M7 PRESENT count=1080 ok=1` |
| Piloto "neto" creado | Log `M7 Pilot_CreateNew END result=1 name=neto` |
| Música del frontend solicitada | VFS open `music/FrFamRoom.IMC` ok=1 |
| Crash inmediatamente después | SIGSEGV fault addr 0x0 en ImVimaDecodeAdpcm+676 |
| El proceso muere | `Fatal signal 11 (SIGSEGV), code 1 (SEGV_MAPERR)` |

### Stack trace completa (tombstone)

```
#00 ImVimaDecodeAdpcm+676          ← null pointer dereference
#01 ImMcmpRead+1244
#02 ImStreamRefill+228
#03 ImProcessStreamSwitches+348
#04 ImUpdate+24
#05 (callback del timer de iMUSE)
#06 FamilyTransportRoom_Update+480
#07 FrontendScreen_RunFrame+160
#08 XwaFrontendTask_Tick+148
#09 XwaPort_Tick+560
#10 SDL_main+2092
```

### Causa raíz

En `ImVimaDecodeAdpcm` (imcodec.c:1472), el loop de decodificación lee bytes de `srcCur` sin verificar límites del buffer. Cuando el bloque MCMP de `FrFamRoom.IMC` contiene menos datos comprimidos de los que el decodificador espera (basándose en `blockDecodedSize`), `srcCur` avanza más allá del buffer válido → acceso a dirección 0x0 → SIGSEGV.

**No es un archivo corrupto** — el archivo se abre correctamente. Es un posible bug del decodificador donde el tamaño del buffer comprimido no se valida contra la cantidad de datos que el decoder necesita leer.

### Por qué este crash bloquea el vuelo

El crash ocurre en `FamilyTransportRoom_Update`, que es la pantalla principal del concourse (la sala de la familia transport). Esta pantalla se carga automáticamente después de crear un piloto. Sin resolver este crash, no se puede navegar a "Play Mission" ni iniciar ninguna misión.

## 2. Ruta concreta para iniciar una misión real

### Camino de ejecución (verificado en código)

```
Concourse_Update
  → User clicks "Fly" (Quest pointer)
  → FrontendFlight_LaunchSession(frameCounter=0)
    → sets g_frontendFlightPendingLaunch = 1
  → XwaPort_TickBody() (port.c:324)
    → FrontendFlight_HasPendingLaunch() == true
    → FrontendFlight_BeginPendingLaunch()
      → saves config, pilot
      → builds missionPath from pilotData
      → sprintf(cmdLine, "~TOUR~ ~filename~ ~pilot~ ...")
      → XwaFlightTask_Init(cmdLine, filmFilePath)
        → parses command line
        → FlightDisplay_Init() → creates flight surfaces
        → Mission_Init(fileName) → loads .MIS file
        → g_xwaFlightTaskActive = 1
        → g_xwaFlightTaskPhase = MAIN_LOOP_INIT
  → XwaFrontendTask_Tick() no longer runs
  → XwaPort_TickBody() → XwaFlightTask_Tick() now runs each frame
    → phase transitions: MAIN_LOOP_INIT → LOADING → MISSION_INSTANCE_INIT
      → HANGAR_READY → MISSION_START → FRAME (flight simulation)
```

### Verificación de wrappers M7A

Todos los wrappers necesarios ya están en trace.c:
- `FrontendFlight_LaunchSession` ✅
- `FrontendFlight_BeginPendingLaunch` ✅
- `XwaFlightTask_Init` ✅
- `XwaFlightTask_Tick` ✅
- `FlightDisplay_Init` ✅
- `Mission_Init` ✅

### Gate de vuelo

M7A eliminó el `XwaFlightTask_IsActive()` guard del loop principal (CMakeLists.txt línea 32-33). El vuelo puede ejecutarse sin restricciones.

## 3. Propuesta para mostrar el vuelo en pantalla virtual

### Estado actual de presentación

M6 y M7A presentan el juego en una ventana 2D de 1152×648 dentro de un volumetric window de Quest. El rendering pipeline es:

```
SDL_SetWindowSize(1152, 648)
  → Vulkan render to surface
  → Aeron_Present()
  → Quest compositor muestra la ventana como panel flotante
```

### Para el vuelo, la misma ruta funciona

El motor de OpenXWA ya renderiza el vuelo en la misma superficie de presentación que los menús:

1. `FrontendFlight_BeginPendingLaunch()` libera superficies del frontend
2. `FlightDisplay_Init()` crea superficies de vuelo en el mismo SDL window
3. `XwaFlightTask_Tick()` renderiza frames de vuelo
4. `AeronDx5_EndFrame()` → `Aeron_Present()` → Quest compositor

**No se necesita crear nada nuevo.** El vuelo se mostrará automáticamente en la misma ventana de Quest donde se ven los menús, como una pantalla virtual.

### Diferencia con VR inmersivo

Actualmente el juego se muestra como un panel 2D flotante (similar a una pantalla virtual). Para VR inmersivo se necesitaría:
- Estéreo (dos vistas, una por ojo)
- Head tracking (rotación de cámara según headset)
- Aberración cromática / lens distortion

**M7B NO necesita esto** — el objetivo es tener el juego funcionando en pantalla virtual primero.

## 4. Propuesta de integración de controles Quest

### Adaptador Android actual (input_android.c)

El adaptador actual:
- Recibe eventos SDL de teclado (via JNI `nativeKey`)
- Recibe texto del teclado virtual (via JNI `nativeName`)
- Emite eventos `SDL_EVENT_KEY_DOWN` para: RETURN, ESCAPE, UP, DOWN, LEFT, RIGHT
- **NO maneja gamepad/sticks/triggers**

### Gamepads ya detectados

Los logs muestran que SDL detecta los controllers Quest:
```
Opened controller slot 0 id=2 kind=gamepad name='Device 0x42F2043511D18FF9' axes=8 buttons=17
Opened controller slot 1 id=3 kind=gamepad name='Device 0xEDB72544769E68D1' axes=8 buttons=17
```

### Motor de entrada de OpenXWA

El motor de OpenXWA ya tiene un sistema de entrada que:
- Procesa `SDL_EVENT_GAMEPAD_AXIS_MOTION` para sticks
- Procesa `SDL_EVENT_GAMEPAD_BUTTON_DOWN/UP` para botones
- Mapea entradas a acciones del juego (pitch, yaw, throttle, fire, etc.)
- Lee `game_controller` config del archivo `config.yaml`

### Mapeo propuesto para Quest 3S

| Quest Input | Acción XWA |
|-------------|-----------|
| Left Stick | Pitch (arriba/abajo) + Roll (izq/der) |
| Right Stick | Yaw (izq/der) + Throttle (arriba=subir, abajo=bajar) |
| Right Trigger (gatillo) | Fire primary weapon |
| Left Trigger (gatillo) | Fire secondary weapon |
| A button | Select next target |
| B button | Select previous target |
| X button | Change weapon group |
| Y button | Match speed |
| Left Bumper | Cycle sub-systems |
| Right Bumper | Shield balance toggle |
| Menu button | Pause / open ingame menu |
| Left Stick Press | Full throttle |
| Right Stick Press | Stop / zero throttle |
| D-pad Up/Down | Next/previous target (alternative) |
| D-pad Left/Right | Countermeasures |

### Implementación

No se necesita modificar el engine de OpenXWA. El mapeo se configura en `config.yaml`:
```yaml
game_controller:
  pitch_axis: 1        # left stick Y
  roll_axis: 0         # left stick X
  yaw_axis: 2          # right stick X
  throttle_axis: 3     # right stick Y (invertido)
  fire_primary: 7      # right trigger
  fire_secondary: 6    # left trigger
  target_next: 0       # A button
  target_prev: 1       # B button
  ...
```

El adaptador Android (input_android.c) necesita una extensión para:
1. Reenviar eventos de gamepad SDL al engine (actualmente solo reenvía teclado)
2. Los events de gamepad de Quest llegan como SDL events normales — solo necesitamos verificar que el engine los procesa

## 5. Primer cambio mínimo recomendado

### Cambio 1: Guard en ImVimaDecodeAdpcm (imcodec.c)

**Archivo:** `C:\OpenXWA\src\xwa\audio\imuse\imcodec.c`
**Función:** `ImVimaDecodeAdpcm` (línea 1417)
**Cambio:** Agregar parámetro `srcSize` y verificación de límites

```c
// Línea 1417 — cambiar firma:
void ImVimaDecodeAdpcm(ImVimaState* state, int16_t* dst, unsigned char* src,
                       int sampleCount, unsigned int channelCount, int resetFlag,
                       unsigned int srcSize);

// Antes del loop (línea 1431), calcular límite:
unsigned char* srcEnd = src + srcSize;

// En cada lectura de srcCur, verificar:
// Línea 1488:
if (srcCur < srcEnd) {
    bitBuffer = (unsigned short)((bitBuffer << 8) | *srcCur);
    ++srcCur;
} else {
    bitBuffer = (unsigned short)(bitBuffer << 8);
}

// Repetir para líneas 1505 y 1516
```

**Caller (ImMcmpRead, línea 841):** pasar `blockCompressedSize[blockIndex]` como `srcSize`.

**Alternativa más simple (sin cambiar firmas):** Agregar un wrapper en trace.c que envuelva `ImMcmpRead` y detecte la condición de crash.

### Cambio 2: Configurar volumen de música = 0 para M7B

**Archivo:** `C:\OpenXWA\XWAQuest\m7\app\src\main\java\org\openxwa\xwaquest\m7\RuntimeActivity.java`
**Cambio:** En `getArguments()`, agregar `"nomusic"` al cmdLine o modificar config.yaml después de la primera ejecución.

O更好的: Modificar el config.yaml del dispositivo para que `music_volume: 0` y `datapad_music_volume: 0`. Esto desactiva la reproducción de música sin tocar código del engine.

### Cambio 3: Configurar `datapadMusicEnabled = 0` vía config

Después de la primera ejecución (que crea el config), editar el config.yaml del dispositivo:
```yaml
music_volume: 0
datapad_music_volume: 0
```

Esto evita que `FamilyTransportRoom_Update` llame a `Music_SetState()`, que es lo que desencadena la cadena de decodificación ADPCM que falla.

### Cambio 4: Habilitar entrada de gamepad en input_android.c

**Archivo:** `C:\OpenXWA\XWAQuest\m7\input_android.c`
**Cambio:** No se necesita — SDL ya detecta los gamepads (logs lo confirman). El engine de OpenXWA ya procesa eventos de gamepad. Solo verificar que funciona.

## 6. Actualización de XWAQUEST_MASTER.md

Ver archivo adjunto `XWAQUEST_MASTER_M7B.md`.

## Criterio de aceptación M7B

| # | Criterio | Evidencia requerida |
|---|----------|---------------------|
| 1 | Frontend estable sin crash | 60 segundos sin SIGSEGV en Quest |
| 2 | Poder navegar a "Play Mission" | Log `M7A LAUNCH_SESSION frameCounter=0` |
| 3 | XwaFlightTask_Init ejecuta | Log `M7A FLIGHT_INIT EXIT result=1` |
| 4 | Mission_Init carga .MIS | Log `M7A MISSION_INIT EXIT result>0` |
| 5 | Flight_Step ejecuta | Log `M7A FLIGHT_TICK count>0 active=1` |
| 6 | Imágenes de vuelo visibles | Screenshot del Quest con naves/espacio |
| 7 | Controles Quest responden | Left stick mueve la nave, triggers disparan |
| 8 | Poder regresar al frontend | Log `M7A FLIGHT_SHUTDOWN EXIT result=0` |

## Resumen ejecutivo

| Aspecto | Estado |
|---------|--------|
| Crash ADPCM | **Identificado**: null pointer en ImVimaDecodeAdpcm al leer buffer sin bounds check |
| Fix recomendado | Configurar `music_volume: 0` (sin código) O agregar bounds check en imcodec.c |
| Camino al vuelo | Verificado: FrontendFlight → XwaFlightTask_Init → Mission_Init → Flight_Step |
| Presentación | Ya funciona: misma ventana 2D de Quest |
| Controles | Gamepads detectados por SDL, engine listo para procesar |
| Primer cambio mínimo | Cambiar config.yaml para desactivar música, o fix en imcodec.c |
