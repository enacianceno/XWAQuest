# Estado M7A — Compilación exitosa, pendiente de prueba en Quest

Fecha: 2026-09-20. Proyecto: C:\OpenXWA\XWAQuest.

## Resultado de compilación

APK generado exitosamente en `m7/app/build/outputs/apk/debug/app-debug.apk`.
SHA-256: 7C10CC58942005E9DB1E74E1A1A1C7582B51EACFF13AF7DB5DD7F73F816110C1
Tamaño: 13,286,915 bytes (12.7 MB)

Build nativo: 6/6 objetos compilados, libOpenXWAM7.so enlazada (27.5 MB).
Build Gradle: 32/32 tareas completadas (53 segundos).

## Cambios respecto a M6

### Cambio principal: Eliminación de guardia de vuelo

**M6 (línea 31 de CMakeLists.txt):**
```cmake
"if (SDL_GetTicks() - m6_start > 900000 || XwaFlightTask_IsActive()) {
    SDL_Log(\"M6 STOP observation limit or flight boundary\");
    break;
}"
```

**M7A (misma línea):**
```cmake
"if (SDL_GetTicks() - m7_start > 1800000) {
    SDL_Log(\"M7 STOP observation limit reached (30min)\");
    break;
}"
```

**Efecto:** M6 detenía la aplicación al detectar que el vuelo había empezado (`XwaFlightTask_IsActive()`). M7A elimina esa condición, permitiendo que el vuelo continúe. Se conserva un límite de observación de 30 minutos (vs 15 de M6).

### Instrumentación añadida (trace.c)

Nuevos wrappers en M7A respecto a M6:

| Wrapper | Función | Propósito |
|---|---|---|
| `FrontendFlight_LaunchSession` | Detecta pulsación de "Fly" | Registra cuándo el usuario intenta lanzar misión |
| `FrontendFlight_BeginPendingLaunch` | Construye cmdLine e init | Registra si la navegación frontend funciona |
| `XwaFlightTask_Init` | Inicializa el vuelo | Registra cmdLine, resultado, errores |
| `XwaFlightTask_Tick` | Bucle de vuelo | Registra transiciones de fase y ticks |
| `XwaFlightTask_Shutdown` | Limpieza post-vuelo | Registra si el vuelo terminó correctamente |
| `FlightDisplay_Init` | Crea surfaces de vuelo | Punto crítico en Android |
| `Mission_Init` | Carga archivo .MIS | Punto crítico: ¿lee el archivo correctamente? |

### Cambios en RuntimeActivity.java

- Paquete: `org.openxwa.xwaquest.m7`
- Librería nativa: `libOpenXWAM7.so`
- Capturas: prefijo `m7-` en vez de `m6-`
- Log tags: `XWAQuestM7` en vez de `XWAQuestM6`
- Funcionalidad: idéntica a M6 (hover, touch, gamepad, IME)

## Archivos modificados

| Archivo | Cambio |
|---|---|
| `m7/CMakeLists.txt` | Nuevo: proyecto M7, wrappers de vuelo, sin guardia de vuelo |
| `m7/trace.c` | Nuevo: instrumentación M7A con 7 wrappers de vuelo |
| `m7/app/build.gradle` | Paquete m7, librería M7A |
| `m7/app/src/main/AndroidManifest.xml` | Label "XWAQuest M7 flight-test" |
| `m7/app/src/main/java/.../m7/RuntimeActivity.java` | Paquete m7, logs M7 |
| `m7/settings.gradle` | Nombre XWAQuestM7 |
| `m7/Build-Target.ps1` | Excluye .so de M6 |
| `m7/ESTADO.md` | Este archivo |

## Archivos sin cambios (copiados de M6)

- `m7/input_android.c` — idéntico a M6
- `m7/gradle.properties` — idéntico a M6
- `m7/build.gradle` (root) — idéntico a M6
- `m7/local.properties` — idéntico a M6
- `m7/.gitignore` — idéntico a M6

## Lo que M7A NO cambia

- Motor OpenXWA (mismos .a de M2)
- GameData (no se modifica)
- Configuración de vuelo (mismos valores por defecto)
- Controles (mismos mapeos que M6)
- Renderizado (sin VR, sin stereo, sin head tracking)
- Audio (sin cambios)

## Próxima prueba

1. Instalar APK en Quest 3S
2. Ejecutar aplicación
3. Navegar concourse hasta "Play Mission"
4. Observar logs `adb logcat | grep M7A`
5. Verificar si `XwaFlightTask_Init` se alcanza
6. Verificar si `FlightDisplay_Init` retorna ok
7. Verificar si `Mission_Init` carga el .MIS
8. Verificar si `Flight_Step` ejecuta sin crash
9. Verificar si hay renderizado 3D visible

## Criterios de éxito

| Criterio | Evidencia |
|---|---|
| Log `M7A FLIGHT_INIT ENTER` | La función se alcanza |
| Log `M7A FLIGHT_INIT EXIT result=1` | Inicialización exitosa |
| Log `M7A FLIGHT_DISPLAY_INIT EXIT ok=1` | Display de vuelo creado |
| Log `M7A MISSION_INIT EXIT result>0` | Misión cargada |
| Log `M7A FLIGHT_TICK count>0 active=1` | Simulación ejecutándose |
| Screenshot con naves/espacio | Renderizado 3D visible |
| Sin crash en 60 segundos | Estabilidad |

## Procedimiento de reversión

Si M7A falla:
1. No eliminar m7/ — conservar como referencia
2. Los cambios están aislados en m7/
3. M6 permanece intacto con su APK funcional
4. Para volver a M6: instalar APK de m6/
5. Para iterar: modificar m7/ y recompilar

## M6 preservado

M6 no fue modificado. Verificación:
- `m6/CMakeLists.txt` sin cambios
- `m6/trace.c` sin cambios
- `m6/input_android.c` sin cambios
- `m6/app/` sin cambios
- APK M6 disponible en `m6/app/build/outputs/`
