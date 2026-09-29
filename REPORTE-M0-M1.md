# Reporte final M0 y M1 — XWAQuest

Fecha: 18 de septiembre de 2026. **M1 completado como prueba básica en Meta Quest 3S. M2 no iniciado.**

Confirmación visual aportada por el usuario, aceptada sin repetirla: **«Dentro del Quest 3S veo el cubo de colores correctamente sobre un fondo azul oscuro.»** No se cambió el ejecutable después de esa confirmación.

## M0 preservado

Se conserva `C:\OpenXWA\milestones\M0-20260918` sin modificaciones en esta continuación. No se repitió M0 ni su verificación completa de hashes. Su README y registros documentan copias verificadas mediante SHA-256, `manifest.csv`, `checksums.csv`, el parche de trabajo, inventario original y bundles Git verificados.

- OpenXWA original: `f063965a4647ed6955d45e73095006788e0830e2`.
- Aeron preservado: `572b368446dc75987dd281b6c0fbe782fc85b3d3`.
- SDL3 preservado: `8f8ed757bc94d2e097aab8c4c0f58b57dcee9871`.
- Los cambios preexistentes en `src/xwa/assets/opt_model.c` y `.h` y sus backups permanecen intactos.
- Se conservan `OpenXWA/`, `hello_xr/`, `OpenXR-parent-reference/`, `SDL3.bundle`, `aeron.bundle` y los registros de restauración. No se copiaron datos comerciales ni archivos que pudieran contenerlos, según las exclusiones originales de M0.

## Proyecto y archivos

Proyecto independiente: `C:\OpenXWA\XWAQuest`. Paquete `org.openxwa.xwaquest`, versión `0.1-M1`, código 1. Repositorio local en rama `codex/m1-sdl-gpu-openxr`, todavía sin commits; los archivos del proyecto están sin seguimiento. SDL permanece sin modificaciones locales.

Archivos creados durante M1, ya existentes al retomar:

- `.gitignore`, `README.md`, `Build.ps1`, `build.gradle`, `settings.gradle`, `gradle.properties`.
- `gradlew`, `gradlew.bat`, sus licencias y `gradle/wrapper/gradle-wrapper.jar`, `.properties` y licencias.
- `SDL-LICENSE.txt`, `OpenXR-LICENSE.txt`.
- `app/build.gradle`, `app/src/main/AndroidManifest.xml`.
- `app/src/main/cpp/CMakeLists.txt`, `app/src/main/cpp/main.c`.
- `app/src/main/java/org/openxwa/xwaquest/MainActivity.java`.
- Configuración local `local.properties`, productos de build y evidencias, excluidos de Git.

La adaptación de `main.c` parte del ejemplo SDL de cubos XR: un cubo, fondo azul oscuro, Vulkan explícito, dos ojos, comprobación de resultados y logs. El empaquetado Gradle extrae el loader desde `prefab` del AAR y MainActivity carga `openxr_loader`, `SDL3` y `main`.

En esta continuación **no se modificó código, configuración, dependencias ni APK**. Se creó este reporte y se añadieron capturas bajo `evidence/`. El inventario de archivos del proyecto está en `evidence/project-files.txt`. Algunas capturas de logcat se actualizaron durante la prueba; `process-first-complete.txt` conserva el arranque y cierre completos del primer proceso, y `verification-xr-logcat.txt` conserva la reactivación final.

## Versiones y build

| Componente | Versión/configuración verificada |
|---|---|
| JDK | Eclipse Temurin `21.0.12.1+1`, `JAVA_VERSION=21.0.12.1`; directorio `jdk-21.0.12.101-hotspot` |
| Gradle / Android Gradle Plugin | 8.5 / 8.1.4 |
| Android SDK | compile 35, target 34, mínimo 29; Build Tools configuradas 33.0.1 |
| NDK / CMake | 28.2.13676358 / 3.22.1, registrados en configuración y build previo |
| SDL3 | 3.5.0 (`SDL_version=3005000`), revisión `8f8ed757bc94d2e097aab8c4c0f58b57dcee9871` |
| OpenXR loader | AAR Khronos `openxr_loader_for_android:1.1.43` |
| Headers OpenXR incluidos por SDL | 1.1.62; SDL solicita por defecto API OpenXR 1.0 |
| Runtime OpenXR | Runtime del Quest cargado desde `/odm/etc/openxr/1/active_runtime.aarch64.json`; loader registra interfaz 1/API 1.0 |
| Quest | Quest 3S (`panther`), Android 14, ABI principal `arm64-v8a` |
| GPU | Adreno 740; runtime informa API Vulkan del dispositivo 1.3.295, driver 512.837.9 |
| ADB | 1.0.41, platform-tools 37.0.1-15733141 |

Resultado final conservado: **BUILD SUCCESSFUL in 8s**, 35 tareas, 13 ejecutadas y 22 actualizadas, en `evidence/build-M1.log`. No se recompiló porque el APK válido coincide con el instalado y no hubo correcciones necesarias. El archivo `build-M1.log` de la raíz es un intento antiguo fallido con Java 25; no representa el resultado final. `Build.ps1` exige JDK 21.

## APK, instalación e identidad

APK: `C:\OpenXWA\XWAQuest\app\build\outputs\apk\debug\app-debug.apk`, 2 316 950 bytes, generado a las 13:30:45.

SHA-256 del APK local y del `base.apk` instalado, comprobados por separado:

```text
6B80E1C6621845C94112FCEFA97439002707B194E964D6EC0F10B86A5A0A113D
```

Contiene manifest, recursos, DEX y solamente estas bibliotecas nativas ARM64:

| Entrada | Bytes | ELF |
|---|---:|---|
| `lib/arm64-v8a/libSDL3.so` | 3 632 208 | 64 bits, AArch64 (machine 183) |
| `lib/arm64-v8a/libc++_shared.so` | 1 253 544 | 64 bits, AArch64 |
| `lib/arm64-v8a/libmain.so` | 36 416 | 64 bits, AArch64 |
| `lib/arm64-v8a/libopenxr_loader.so` | 1 583 992 | 64 bits, AArch64 |

El loader del APK es idéntico al miembro `prefab/modules/openxr_loader/libs/android.arm64-v8a/libopenxr_loader.so` del AAR 1.1.43. SHA-256 de ambos: `713E3BB8D955254C670ACC1C4899A65CB8C930E97DD9958BF37EA922D72B7A06`. Evidencia: `apk-elf-verification.txt`, `loader-origin.txt`, `installed-apk-sha256.txt`.

Instalación previa: `Success`, 13:31:10, confirmada por Package Manager. En esta continuación `am start -W` devolvió `Status: ok`, arranque frío en 327 ms. Se reutilizó esa instalación sin reinstalar. No se empaqueta el motor OpenXWA, Aeron completo ni game data.

## Ejecución y aceptación M1

Los siguientes registros proceden del Quest, no de una simulación:

```text
17:58:49.954 BUILD backend=vulkan SDL_version=3005000
17:58:50.046 XR Session begun!
17:58:50.046 View count: 2
17:58:50.058 Created swapchain 0: 1680x1760, 3 images, with depth buffer
17:58:50.067 Created swapchain 1: 1680x1760, 3 images, with depth buffer
17:58:50.102 FRAME submitted=1 views=2 cubes=1 shouldRender=1 xrEndFrame=0
17:59:10.932 FRAME submitted=1500 views=2 cubes=1 shouldRender=1 xrEndFrame=0
18:00:53.353 FRAME submitted=2400 views=2 cubes=1 shouldRender=1 xrEndFrame=0
```

| Criterio | Resultado y fundamento |
|---|---|
| Android ARM64-v8a | Demostrado por ELF de las cuatro bibliotecas, Package Manager y ejecución real. |
| SDL3 | Inicialización correcta y versión registrada en ejecución. |
| SDL_GPU/Vulkan | `SDL_CreateGPUDeviceWithProperties`, backend real `vulkan`, driver Adreno y `Engine Name: SDLGPU`. Los mensajes OpenGLRenderer corresponden también a infraestructura Android; no cambian el backend registrado de SDL_GPU. |
| OpenXR y sesión | Loader e instancia correctos, `xrCreateSession` y `xrBeginSession` registrados, estados IDLE→READY→SYNCHRONIZED→VISIBLE→FOCUSED. |
| Swapchains | Dos swapchains independientes, tres imágenes por ojo, 1680×1760 y depth local. |
| Adquisición/liberación | El código ejecuta acquire→wait→render→submit GPU→release por ojo, comprueba resultados e índice, y sólo registra FRAME después de ambos ojos y `xrEndFrame` exitoso. Miles de frames sin errores demuestran esa ruta; no se añadió traza individual de cada acquire/release. |
| Envío/presentación | Submit GPU comprobado y `xrEndFrame=0` con una capa de proyección estéreo; la confirmación visual del usuario completa la evidencia de presentación en el visor. |
| Una vista por ojo | Dos poses/FOV obtenidos con `xrLocateViews`, matrices y render pass independientes, cada subImage asociada a su swapchain, `viewCount=2`. Confirmación visual aceptada; no se afirma haber realizado una nueva prueba monocular humana. |
| Lifecycle básico | Arranque frío, Home/cierre, reapertura, suspensión y reactivación comprobados. Detalle abajo. |
| Recursos y estabilidad básica | Cierre de ambos swapchains, sesión e instancia registrado; sin crash/ANR o error de frame identificado en las capturas. No equivale a una prueba prolongada de fugas. |

Lifecycle: el proceso 25577 recibió `onPause`, pasó 5→4→3→6→1, ejecutó `Cleaning up`, destruyó las seis imágenes de los dos swapchains, la sesión y la instancia. Android registra `EXIT_SELF`, status 0, a las 17:59:12.406. Home cerró la actividad, por lo que el retorno creó el proceso 25847. La suspensión/reactivación posterior conservó ese PID: `onResume`, READY y nuevo `xrBeginSession`, hasta FOCUSED y frames 2100/2400. No fue necesario recrear swapchains en ese retorno. Se dejó el APK instalado y el proceso reactivado; el visor puede volver a suspenderse normalmente.

## Problemas resueltos y advertencias

Problemas previos ya resueltos al retomar:

1. Gradle con Java 25 fallaba con `Unsupported class file major version 69`. Se usó JDK 21 y quedó la comprobación en `Build.ps1`.
2. Loader ausente del empaquetado inicial: el AAR lo distribuye en `prefab`, no `jni`. La tarea `extractOpenXRLoader`, dependiente de `preBuild`, lo coloca en `generated/openxrJni`. Esta continuación verifica tanto empaquetado como identidad y carga real.

No se detectó un nuevo defecto bloqueante de M1 que justificara cambiar el APK confirmado visualmente.

Advertencias preservadas, sin ocultarlas:

- Build: AGP 8.1.4 probado hasta compile SDK 34 frente a compile SDK 35; metadata `riscv64` del NDK ignorada; resolución de `openxrRuntime` durante configuración. No impidieron el build ARM64.
- Loader: brokers OpenXR sin cursor y mensaje de error de acceso; inmediatamente usa el manifiesto global del Quest y carga el runtime correctamente. Es un fallback observado, no un fallo de sesión.
- Runtime/driver: funciones Vulkan opcionales ausentes, telemetría/servicios opcionales no disponibles y acceso denegado a algunos atributos `sysfs`. No hubo `VK_ERROR_DEVICE_LOST`, `XR_ERROR`, `FATAL EXCEPTION`, `Fatal signal` ni ANR de XWAQuest en las capturas examinadas.
- Android registra `SensorManager: sensor or listener is null` y `OpenGLRenderer: Unable to match the desired swap behavior` al arrancar; la ejecución XR posterior permanece correcta.
- HzOS advierte que falta versión mínima de su SDK en el manifest para acceso futuro a `VolumetricWindowManager`. Riesgo de compatibilidad futura del runtime; no bloquea M1 actual. No se inventó una versión ni se cambió el manifest para silenciarlo.
- Al destruir el runtime aparece `SoundPool-JNI ... mObjectCount: 1 should be zero on destruction`. XWAQuest no inicializa audio; se observa durante cierre de componentes del runtime y termina con status 0. Se conserva como advertencia de recursos, sin atribuir de forma concluyente la causa ni afirmar que se corrigió.
- Memoria del segundo proceso: PSS 204 296→208 116 KB, heap nativo 9 912→10 188 KB, gráficos estable en 136 536 KB. Son dos muestras cortas que abarcan lifecycle; no prueban ausencia absoluta de fugas. Las seis imágenes del primer proceso sí tienen registros explícitos de destrucción.
- No se habilitaron capas de validación Vulkan/OpenXR ni se hizo prueba de estrés prolongada. SDL3 3.5.0 queda fijado por revisión; no se declara como una versión estable publicada. APK de debug, no de distribución.

## Evidencias y cierre

Evidencias locales en `C:\OpenXWA\XWAQuest\evidence`: build e instalación originales; APK/ELF/hash/loader; `process-first-complete.txt` para arranque, rendering y teardown; `process-second.txt` para pausa; `verification-xr-logcat.txt` para reactivación y frames posteriores; `final-logcat-full.txt` y `resume-logcat-full.txt` para contexto; `resume-package.txt`, `exit-info.txt` y muestras `memory-*.txt`. La consulta al buffer de crashes devolvió salida vacía; no se generó archivo con contenido. El buffer normal rota rápidamente, por eso se conservaron capturas separadas.

**Todos los criterios funcionales básicos solicitados de M1 están demostrados**, combinando código, ejecución, logs y confirmación visual. No queda pendiente repetir la confirmación visual. Las advertencias y los límites de validación anteriores permanecen documentados y no se presentan como pruebas realizadas.

**Trabajo detenido al terminar M1. Se espera revisión y autorización explícita del usuario antes de M2.** No se inició integración de OpenXWA/Aeron completos, game data, frontend, misiones, audio ni controles.
