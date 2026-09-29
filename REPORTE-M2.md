# Reporte M2 — compilación y enlace Android ARM64

**Estado: COMPLETADO. M3 no iniciado.** Fecha: 18 de septiembre de 2026.

OpenXWA, Aeron y las dependencias que utiliza el motor compilan y enlazan para
Android ARM64-v8a. El APK M2 se instaló en Quest 3S y cargó las bibliotecas reales.
La Activity de validación no llama a `SDL_main`, `Aeron_Init` ni `XwaPort_Init`;
no inicializa el motor, busca GameData, abre frontend o ejecuta misiones.

## Preservación y organización

- M0 y `hello_xr`: no modificados; no se repitió M0.
- M1: fuentes, configuración y APK funcional conservados. Los hashes registrados
  antes de M2 coinciden al terminar la validación.
- `src/xwa/assets/opt_model.c` y `.h`: intactos, incluidos LOD0, NODEREF y sus
  diagnósticos preexistentes. No se sustituyeron por copias limpias.
- Proyecto: `C:\OpenXWA\XWAQuest`.
- Build y aplicación M2 separados en `C:\OpenXWA\XWAQuest\m2`.
- Paquete M2: `org.openxwa.xwaquest.m2`. No reemplaza el paquete M1
  `org.openxwa.xwaquest`.
- No se hicieron commits indiscriminados ni se alteraron los datos originales.

## Archivos modificados y creados

Cambios en archivos existentes:

| Archivo | Cambio |
|---|---|
| `C:\OpenXWA\CMakeLists.txt` | En Android, genera `xwa` como DSO `libOpenXWA.so` con las mismas fuentes de aplicación, y exige enlace sin símbolos indefinidos. Otros sistemas conservan el ejecutable. |
| `C:\OpenXWA\aeron\CMakeLists.txt` | Acepta targets SDL3, zstd y FFmpeg proporcionados por el proyecto padre. Conserva las búsquedas originales cuando esos targets no existen. |

Archivos nuevos bajo `XWAQuest/m2`:

- `CMakeLists.txt`: contenedor Android, dependencias explícitas, revisión SDL
  fijada, SPIR-V y enlace completo de objetos del motor.
- `host-tools/CMakeLists.txt`: build Windows x64 de SDL, shadercross y generador FSR.
- `Prepare.ps1`: descargas fijadas por commit/SHA-256 y aplicación del parche FSR
  de shadercross que ya existía en OpenXWA.
- `Build-Host.ps1`, `Build-FFmpeg.sh`, `Build-Target.ps1`: etapas reproducibles.
- `Verify.ps1`: ABI, dependencias, proveedores de símbolos, shaders y preservación.
- `settings.gradle`, `build.gradle`, `gradle.properties`, `app/build.gradle`:
  APK M2 independiente, compilación nativa previa al empaquetado, bibliotecas y
  recursos generados.
- `app/src/main/AndroidManifest.xml` y
  `app/src/main/java/org/openxwa/xwaquest/m2/LoadCheckActivity.java`: prueba de
  carga únicamente; usa las clases Java reales de SDL sin heredar de SDLActivity.
- `.gitignore`, `BLOCKERS.md`, este reporte y evidencias locales.

Fuentes descargadas y builds quedan en directorios ignorados `deps/`, `host/`,
`downloads/`, `prefix/`, `build-*` y `evidence/`. No se modificó SDL3 ni las fuentes
de FFmpeg/zstd. Sólo el checkout host de shadercross recibe el parche existente
`packaging/common/shadercross-fsr3.patch`. Los diffs M2 están guardados en
`evidence/openxwa-m2.patch` y `evidence/aeron-m2.patch`.

## HOST y TARGET

**HOST = Windows x64. TARGET = Android ARM64-v8a, API mínima 29.**

| Componente | Versión / revisión |
|---|---|
| OpenXWA | Base `f063965a4647ed6955d45e73095006788e0830e2`, cambios OPT preservados y cambio CMake M2 |
| Aeron | Base `572b368446dc75987dd281b6c0fbe782fc85b3d3`, cambio CMake M2 |
| SDL3 host y Android | `8f8ed757bc94d2e097aab8c4c0f58b57dcee9871`, 3.5.0, misma revisión que M1 |
| JDK | Temurin 21.0.12.1, directorio `jdk-21.0.12.101-hotspot` |
| Gradle / AGP | 8.5 / 8.1.4 |
| SDK | compile 35, target 34, min 29; Build Tools 33.0.1 |
| NDK / compilador Android | 28.2.13676358 / Clang 19.0.1 |
| CMake | 3.22.1, Ninja del SDK |
| zstd | 1.5.7, `f8745da6ff1ad1e7bab384bd1f9d742439278e99` |
| FFmpeg | 7.1.3, `f46e514491172d15bd74b4abb1814cd2f05a763e` |
| OpenXR loader empaquetado | AAR Khronos 1.1.43, el mismo de M1 |
| Compilador host | LLVM-MinGW 20260908 UCRT x86_64, Clang 23.1.1 |
| shadercross host | `1ff05bec573988a98ef9e0260b4da44f512b8367` + parche FSR de OpenXWA |
| SPIRV-Cross host | `1a6169566c73d3da552748fc372fe2bbb856e46e` |
| DXC host | v1.9.2602, archivo oficial `dxc_2026_02_20.zip`, SHA-256 verificado |
| Otros host | Git for Windows/bash, GNU make 4.4.1 de MSYS2 |

`shadercross.exe` y `aeron_fsr3_shadergen.exe` se verificaron como PE/COFF AMD64.
FFmpeg declara por separado clang NDK para target y clang LLVM-MinGW para sus
herramientas host. Los generadores se ejecutan en Windows y producen recursos,
no bibliotecas Android. Sus DLL permanecen fuera del APK.

CMake restringe búsquedas de bibliotecas, headers y paquetes al target; sólo las
búsquedas de programas corresponden al host. FFmpeg se importa mediante rutas
absolutas de `m2/prefix/android-arm64`, sin pkg-config de Windows. SDL y zstd
se construyen desde sus fuentes. El `CMAKE_INSTALL_PREFIX` por defecto que aparece
en logs no se utiliza: el staging es explícito y no se ejecuta `cmake --install`.

## Dependencias y cobertura del motor

Se construyen `xwa_core`, `xwa_remaster`, `xwa_2d_formats`, Aeron, `aeron_compat`,
vídeo, decodificación de audio, VFS FFmpeg, escena, assets, FSR3, ImGui, libyaml,
cgltf, cJSON, parser/conversor OPT, cocinado glTF e imgbake, además de SDL y zstd.
Se conservan los codecs BC5/BC6H/BC7 y la implementación DirectXMath portátil.
Aunque parte de ese código esté en `tools/`, se necesita desde el motor y
permanece compilado para Android. Los ejecutables de herramientas de assets no
se construyen para Android.

`aeron_fsr3_host` es el nombre upstream de la implementación CPU de FSR: en este
grafo se compila **para ARM64**, no para Windows. El ejecutable generador FSR es
una herramienta diferente, compilada en `build-host`.

FFmpeg proporciona realmente `avformat`, `avcodec`, `avutil`, `swresample` y
`swscale`, con decodificadores/demultiplexores internos, NEON y zlib de Android.
Se desactiva autodetección de bibliotecas externas para evitar contaminación.
No se construyen sus programas, encoders, muxers, avdevice, avfilter ni postproc:
Aeron utiliza demultiplexado, decodificación, remuestreo y conversión de imagen,
no esas APIs. No se sustituyó ninguna API utilizada por una implementación ficticia.

**Stubs:** M2 no usa `android_probe`, `xwa_probe_stubs.c` ni `XWA_OPT_PROBE`.
El probe antiguo queda preservado, no borrado. `aeron_compat` es la implementación
real existente, no un reemplazo vacío. Los helpers diagnósticos OPT preexistentes
también quedan en el enlace completo; su nombre “Probe” no implica que M2 use
las exclusiones o stubs del antiguo build.

El enlace usa `--whole-archive` para los componentes del motor,
`--no-gc-sections` y `--no-undefined`. Así no se ocultan subsistemas pendientes
eliminando archivos de un archivo estático porque todavía no se llaman.
`build-android/OpenXWA.map` registra los objetos enlazados. Entre los símbolos
reales exportados están `SDL_main`, `Aeron_Init`, `XwaPort_Init`, funciones de
vídeo/compatibilidad y `ZSTD_decompress`.

## Bloqueos encontrados y resueltos

| Bloqueo | Causa y cambio mínimo |
|---|---|
| FFmpeg: “Host compiler lacks C11 support” | Buscaba `gcc` host, ausente. Se especificó `--host-cc` Windows, manteniendo el clang Android para target. |
| Descarga DXC intentaba NMake | El script upstream asumía un generador no instalado. Se descargó el mismo ZIP oficial con su hash, y se usó Ninja para el build host. |
| Referencias CMake a DLL DXC fuera de alcance | Los targets importados eran locales al subdirectorio shadercross. Se usan las rutas fijadas de sus DLL x64 para el staging host. |
| zlib no habilitada con autodetección desactivada | Se habilitó explícitamente la zlib real del NDK y se completó el build FFmpeg con ella. |
| FSR3: `--spirv-vulkan1.1` desconocido | Faltaba el parche que OpenXWA ya utiliza. Se aplicó ese parche y se recompiló shadercross, sin quitar FSR3 ni variantes FP16/wave. |
| Enlace: `__android_log_print` sin proveedor | Diagnósticos OPT preexistentes necesitan liblog. Se añadió `log` al enlace Android, sin modificar OPT. |
| Primer APK: aborto en `SDL3 JNI_OnLoad` | Faltaban las clases `org.libsdl.app`. Se empaquetaron las fuentes Java reales de la revisión SDL usada. Se recompiló el APK y la prueba de carga pasó. |

No se reescribieron subsistemas OpenXWA/Aeron. No fue necesario cambiar el alcance
o la arquitectura acordada. El ensamblador x86 antiguo está protegido por
`XWA_MODERN`; las ramas C existentes compilan. Las referencias Windows encontradas
en selección de archivos y utilidades FSR están condicionadas por plataforma.
Los tipos fundamentales de compatibilidad usan enteros de tamaño fijo.

## Resultado, APK y comprobaciones

Build nativo completo: **éxito**, `libOpenXWA.so` enlazada sin símbolos faltantes.
Build APK final: **BUILD SUCCESSFUL in 16s**, 5 tareas ejecutadas y 29 actualizadas.
La corrección final sólo añadió clases Java; no repitió la compilación del motor.

APK: `C:\OpenXWA\XWAQuest\m2\app\build\outputs\apk\debug\app-debug.apk`.
Tamaño: **13 269 445 bytes**. Versión `0.2-M2-linkcheck`.
SHA-256 local e instalado, idénticos:

```text
028CC41DA8AF9347D471FE0E983AE1A797E8DBE02D9F26AA3962798EC388B27E
```

Todas las bibliotecas del APK están bajo `lib/arm64-v8a/`:

| Biblioteca | Bytes en APK |
|---|---:|
| libOpenXWA.so | 6 269 160 |
| libSDL3.so | 2 168 008 |
| libavcodec.so | 11 158 296 |
| libavformat.so | 1 731 800 |
| libavutil.so | 718 280 |
| libc++_shared.so | 1 253 544 |
| libopenxr_loader.so | 1 583 992 |
| libswresample.so | 97 504 |
| libswscale.so | 757 784 |

zstd y las bibliotecas estáticas del motor están integradas en `libOpenXWA.so`.
Las copias sin stripping y el mapa de enlace permanecen en `m2/build-android`.

Validación completada:

- **9 DSO ELF64/AArch64**, también verificadas dentro del APK final.
- **22 archivos estáticos ARM64**; sin objetos Windows en ellos.
- **594 unidades de traducción del grafo CMake** usan el target Android API 29;
  FFmpeg se compila adicionalmente mediante su propio build.
- Imports fuertes de las DSO resueltos por bibliotecas empaquetadas o stubs
  oficiales del sistema Android API 29. Cero símbolos fuertes sin proveedor.
- `DT_NEEDED` de OpenXWA: SDL3, las cinco bibliotecas FFmpeg, libc++ y bibliotecas
  del sistema Android (`liblog`, `libm`, `libdl`, `libc`). Inventario completo en CSV.
- **84 shaders SPIR-V**, incluidos **20 FSR**. Se comprobó estructura binaria,
  entrypoints y reflexión mediante shadercross. El generador FSR además valida
  sus bindings y tamaños de grupos contra el manifest. Los shaders del APK
  coinciden por hash con los verificados. No se generaron DXIL/MSL para Android.
- Sin DLL, EXE, archivos `.lib`/`.a` Windows ni GameData en el APK.
- Recursos de aplicación, configuración Aeron y shaders empaquetados; su futura
  localización por el motor no se inicializa ni se prueba en M2.

## Prueba física de carga

Dispositivo ADB `3487C10H9T0BKZ`, Meta Quest 3S. Instalación final: `Success`.
Activity: `org.openxwa.xwaquest.m2/.LoadCheckActivity`.
Arranque frío: `Status: ok`, 365 ms. PID validado: **14003**.

Log del dispositivo:

```text
09-18 23:18:58.481 14003 14003 I XWAQuestM2:
LOAD_OK Android arm64 OpenXWA/Aeron/FFmpeg; SDL_main NOT invoked
```

Se conservaron los mapas del proceso, que muestran las bibliotecas cargadas,
y el hash del APK instalado. La captura del proceso corregido no presenta
`UnsatisfiedLinkError`, `FATAL EXCEPTION` ni `Fatal signal`. El fallo JNI del
primer intento se conserva como evidencia y no se confunde con el resultado final.
No se necesita confirmación visual del usuario para esta prueba de carga.

## Reproducción

Desde PowerShell en `C:\OpenXWA`, con Git for Windows, JDK 21 y el SDK/NDK indicados:

```powershell
# Preparación desde cero; descargas y fuentes fijadas en Prepare.ps1.
.\XWAQuest\m2\Prepare.ps1
.\XWAQuest\m2\Build-Host.ps1
& 'C:\Program Files\Git\bin\bash.exe' XWAQuest/m2/Build-FFmpeg.sh

$env:JAVA_HOME = 'C:\Program Files\Eclipse Adoptium\jdk-21.0.12.101-hotspot'
$env:ANDROID_HOME = "$env:LOCALAPPDATA\Android\Sdk"
.\XWAQuest\gradlew.bat -p .\XWAQuest\m2 :app:assembleDebug --console=plain
.\XWAQuest\m2\Verify.ps1
```

Gradle llama a `Build-Target.ps1`; no es necesario ejecutar ambos por separado.
Si las dependencias ya están construidas, el ciclo incremental comienza con
Gradle. `Verify.ps1` usa el manifiesto local de hashes preservados de esta sesión.
Para otro checkout se necesita conservar la misma revisión del motor y los
cambios OPT previos, además de aplicar los cambios M2. Reproducible significa
fuentes/configuración fijadas y scripts repetibles; no se afirma determinismo
bit a bit entre equipos ni se repitió un segundo build limpio completo.

Referencias primarias de dependencias: [FFmpeg](https://ffmpeg.org/platform.html),
[SDL_shadercross](https://github.com/libsdl-org/SDL_shadercross),
[zstd 1.5.7](https://github.com/facebook/zstd/releases/tag/v1.5.7).

## Evidencia y riesgos pendientes

Directorio: `C:\OpenXWA\XWAQuest\m2\evidence`.

- Builds: `host-build.log`, `host-patched-run.log`, `ffmpeg-configure.log`,
  `ffmpeg-build.log`, `ffmpeg-install.log`, `target-attempt2-liblog.log`
  (compilación completa y fallo de enlace antes de corregir liblog),
  `target-link-success.log`, `gradle-build.log`.
- Validación: `native-libraries.csv`, `*-elf.txt`, `*-imports.txt`,
  `symbol-validation.txt`, `engine-exports.txt`, `host-abi.txt`, `shaders.csv`,
  `shader-reflection/`, `validation-summary.txt`, `apk-native.csv`,
  `apk-entries.txt`, `apk-validation.txt`, hashes local/instalado.
- Quest: `install.txt`, `launch.txt`, `quest-load-logcat.txt`,
  `quest-success-process.txt`, `quest-loaded-maps.txt`, `installed-package.txt`.
- Preservación y revisión: `preserved-before.csv`, parches, `BLOCKERS.md`,
  `compiler-warning-summary.txt`, `compiler-warnings-review.txt`.

Advertencias técnicas, sin ocultarlas ni ampliar M2:

1. Existen warnings del código recuperado: conversiones/punteros incompatibles,
   miembros packed potencialmente desalineados, variable `pixels` posiblemente
   sin inicializar en `frontend_screen.c`, retorno y comparaciones de signo/rango.
   Compilan, pero la prueba de carga no demuestra seguridad de esas rutas al jugar.
   No se añadieron supresiones de warnings para conseguir el enlace.
2. FFmpeg emite warnings de conversiones numéricas; FSR incluye pragmas MSVC
   ignorados e inicializadores parciales. Los logs completos quedan disponibles.
3. AGP 8.1.4 advierte sobre compile SDK 35 y metadata `riscv64` del NDK; sólo ARM64
   se compila y empaqueta. El loader AAR usa la extracción prefab comprobada en M1.
4. No se validó ejecución de frontend, audio, misiones, datos ni pipelines del
   motor. Cargar una DSO no equivale a probar el juego. Las capacidades runtime
   FP16/wave/formatos de textura y sus costes siguen siendo cuestiones posteriores.
5. El APK es de debug y de validación. No implementa el acceso futuro a recursos
   o GameData ni adapta el bucle de presentación Aeron a XR. El loader XR se
   empaqueta/carga, pero M2 no crea una sesión XR nueva ni repite M1.

**Criterio de aceptación M2 cumplido:** motor y dependencias necesarias construidos
y enlazados para ARM64 sin stubs de sustitución, shaders y dependencias verificados,
APK construido e instalación/carga real demostradas en Quest. No quedan criterios
de compilación/enlace/carga M2 pendientes.

**Trabajo detenido en M2. Se espera revisión y autorización explícita antes de M3.**
