# REPORTE M7A

Fecha: 2026-09-20. Proyecto: C:\OpenXWA\XWAQuest.

## Resultado

M7A compilado exitosamente. APK generado: `m7/app/build/outputs/apk/debug/app-debug.apk`.
SHA-256: 7C10CC58942005E9DB1E74E1A1A1C7582B51EACFF13AF7DB5DD7F73F816110C1.
Tamaño: 13,286,915 bytes. Paquete: `org.openxwa.xwaquest.m7`. Librería: `libOpenXWAM7.so`.

M7A NO está declarado completado. La ejecución en Quest 3S deberá verificarse por separado.

## Cambio principal

M6 contenía una guardia que detenía la aplicación al inicio del vuelo:
```c
if (SDL_GetTicks() - m6_start > 900000 || XwaFlightTask_IsActive()) {
    SDL_Log("M6 STOP observation limit or flight boundary");
    break;
}
```

M7A reemplaza esa condición por un simple límite de tiempo:
```c
if (SDL_GetTicks() - m7_start > 1800000) {
    SDL_Log("M7 STOP observation limit reached (30min)");
    break;
}
```

La condición `XwaFlightTask_IsActive()` ha sido eliminada. El vuelo puede ahora ejecutarse sin que la aplicación se detenga. Se conserva un límite de observación de 30 minutos (frente a los 15 de M6) para evitar ejecuciones indefinidas durante pruebas.

## Instrumentación añadida

Siete wrappers nuevos en `m7/trace.c`:

| Wrapper | Firma verificada | Evidencia de la llamada |
|---|---|---|
| `FrontendFlight_LaunchSession` | `int FrontendFlight_LaunchSession(int)` | Detecta pulsación "Fly" |
| `FrontendFlight_BeginPendingLaunch` | `int FrontendFlight_BeginPendingLaunch(void)` | Construye cmdLine |
| `XwaFlightTask_Init` | `int XwaFlightTask_Init(char*, const char*)` | Inicializa vuelo |
| `XwaFlightTask_Tick` | `void XwaFlightTask_Tick(void)` | Bucle de simulación |
| `XwaFlightTask_Shutdown` | `int XwaFlightTask_Shutdown(void)` | Limpieza post-vuelo |
| `FlightDisplay_Init` | `char FlightDisplay_Init(void)` | Crea surfaces de vuelo |
| `Mission_Init` | `uint16_t Mission_Init(char*)` | Carga archivo .MIS |

Todos los wrappers siguen el mismo patrón de M6: llaman a `__real_*` y registran BEGIN/END con resultado. Los wrappers de vuelo usan prefijo `M7A` para distinguirlos en logcat. Los wrappers de infraestructura mantienen prefijo `M7`.

## Compilación

- Build nativo: CMake + Ninja, Clang 19.0.1, 6 objetos, 1 shared library
- Build Gradle: 32 tareas, 53 segundos, warnings idénticos a M6 (Java8 obsoleto, riscv64)
- No se encontraron errores ni warnings nuevos

## Preservación

- M6 no fue modificado. Hashes de CMakeLists.txt, trace.c, input_android.c verificados.
- GameData no fue modificado.
- M2 (archivos estáticos y dependencias) no fue modificado.
- El APK de M6 permanece disponible en `m7/app/build/outputs/apk/debug/app-debug.apk`

## Próxima acción

Antes de cualquier comando ADB:
**¿Ya encendiste el Quest 3S, lo conectaste a la PC y autorizaste la depuración USB?**
**Confírmame cuando esté listo para continuar.**

Una vez confirmado, procederé a:
1. Instalar APK M7A en Quest 3S
2. Ejecutar `adb logcat -s XWAQuestM7,M7,M7A`
3. Navegar concourse hasta "Play Mission"
4. Analizar logs para determinar hasta qué fase llega el motor
5. Generar REPORTE-M7A-ejecucion.md con hallazgos

## Procedimiento de reversión

Si M7A falla en Quest:
1. No eliminar m7/ — conservar como referencia
2. Los cambios están aislados en m7/
3. M6 permanece intacto con su APK funcional
4. Para volver a M6: instalar APK de `m6/app/build/outputs/`
5. Para iterar: modificar m7/ y recompilar con Build-Target.ps1
