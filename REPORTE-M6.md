# REPORTE M6

Fecha: 2026-09-19. Proyecto: C:\OpenXWA\XWAQuest.

## Resultado

Objetivo principal M6 demostrado: entrada de nombre, creación real de piloto, confirmación y avance al frontend posterior. Navegación mediante puntero Quest y clic validada físicamente. B/Escape y stick/flechas permanecen NO funcionales en esta ruta; no se consideran mappings validados. No se ha iniciado M7.

El usuario introdujo `neto` con el diálogo Android y confirmó en el frontend. El motor creó `files/neto0.plt` (152076 bytes), lo guardó y lo volvió a leer en arranques posteriores. La captura `m6/evidence/pilot-created.png` muestra la sala familiar real, con droide, correo y opciones originales. No se escribió un piloto artificial ni se omitió lógica de creación.

## Archivos y aislamiento

Todo el código nuevo está en `m6/`: CMakeLists.txt, Build-Target.ps1, trace.c, input_android.c, settings.gradle, build.gradle, gradle.properties, app/build.gradle, app/src/main/AndroidManifest.xml y app/src/main/java/org/openxwa/xwaquest/m6/RuntimeActivity.java. Configuración local en local.properties; exclusiones en .gitignore; notas en ESTADO.md. Este reporte se añade en la raíz XWAQuest.

Paquete independiente `org.openxwa.xwaquest.m6`, biblioteca libOpenXWAM6.so. Se reutilizan archivos estáticos, dependencias y shaders de M2; sólo se compila el adaptador/entrada de M6 y se enlaza. CMake genera main-m6.c desde el original con instrumentación y límite de observación de15min/guardia de vuelo. `skipintro` es una opción real preexistente.

M0–M5, hello_xr y las correcciones OPT no fueron editados. Hashes de fuentes/configuración y APK M5 comprobados contra m6/evidence/preserved-m5.csv, sin diferencias. GameData original Steam y las copias anteriores permanecen intactos. M6 usa copia privada; se añadieron los8 archivos originales de MELEE que Pilot_CreateNew necesita (7660 archivos totales). Ningún asset comercial empaquetado en APK.

## Entrada real

Texto: EditText/IME Android → validación temporal de1–12 caracteres alfanuméricos ASCII → JNI → eventos SDL_TEXT_INPUT → Aeron → XwaInputBridge → FrontendText_DrawEditableField. Se envía un carácter por frame. El adaptador no modifica buffers internos del juego ni archivos de piloto.

Confirmación: ruta real FrontendDialog_PromptForPilotName → Pilot_CreateNew → MissionSetup_LoadMissionList (missions y melee) → File_WriteCount/Pilot_Save. Los wrappers invocan siempre las funciones reales.

Puntero: Quest entrega hover como source4098/tool1 (touchscreen/finger). La ruta genérica SDL esperaba mouse para ese hover. M6 traduce ACTION_HOVER_ENTER/MOVE a movimiento SDL sin botones. ACTION_DOWN/UP generan press/release izquierdo y ACTION_MOVE sólo movimiento. Se consume la ruta táctil duplicada para evitar doble conversión. No se obtiene pose OpenXR ni se simula un rayo.

La barra inferior se eliminó completamente. El usuario confirmó interfaz completa, cursor siguiendo el rayo sin gatillo y selección al pulsar. A también funciona como clic según la prueba física; no hay evidencia de que sea Enter del adaptador.

Se evaluó A/B y stick por dispatchKeyEvent/dispatchGenericMotionEvent Android y por SDL_GetGamepadButton/Axis de los dispositivos reales de Aeron. Los mandos se conectan, pero la prueba no produjo eventos BUTTON/STICK Java ni cambios M6 PAD ni M6 SDL key para estos mappings. B y sticks no respondieron físicamente. Esto localiza el problema antes de la traducción a teclado del motor, pero no demuestra que sea imposible obtener otros eventos/raw inputs de esos dispositivos. No atribuir una causa definitiva al sistema sin investigación adicional.

## Evidencia

`m6/evidence/runtime-pilot-created.txt`:

- 09:42:41: REAL_EDIT progresa n → ne → net → neto.
- 09:42:45.226: real pilot prompt accepted name=neto; Pilot_CreateNew BEGIN.
- VFS USER neto0.plt mode=1 ok=1; ASSET missions\\mission.lst y melee\\mission.lst ok=1.
- 09:42:45.230: Pilot_CreateNew END result=1 name=neto.
- Captura automática del frontend posterior: pilot-created.png.

`m6/evidence/runtime-pointer-live.txt`:

- HOVER source=4098 tool=1 con movimiento continuo y POINTER action=0/1 separados.
- M6 PAD attached en ambos slots; sin transiciones de botones/ejes mapeados durante prueba.
- neto0.plt leído correctamente en varios reinicios.
- Última sesión PID13922: TICK/REMASTER/PRESENT count=7320 ok=1; lifecycle onPause/surfaceDestroyed al finalizar prueba.
- Sin FATAL EXCEPTION ni Fatal signal encontrados en este archivo de prueba. No se ha realizado una auditoría general de memoria.

Pruebas físicas: usuario confirmó nombre/creación e interacción inicial; posteriormente interfaz libre, hover sin gatillo, clic, A como clic; B y stick fallaron en ambas pruebas. En la última prueba no seleccionó Play Mission. Una sesión previa alcanzó la guardia de vuelo y salió; M6 no valida gameplay ni controles de vuelo.

## Build

Build incremental final: BUILD SUCCESSFUL en12s,5 tareas ejecutadas/28 actualizadas. Instalación `adb install -r` exitosa conservando piloto. APK: m6/app/build/outputs/apk/debug/app-debug.apk.

SHA256: 6FFCE008253CD01BFD3FA900C392D094FCAE0A151A39654C51987023639B0FED.

Reproducción desde XWAQuest, con M2 aprobado disponible: definir JAVA_HOME al JDK21 instalado y ejecutar `./gradlew.bat -p m6 :app:assembleDebug --console=plain`. Logs build.log/native-build.log en m6/evidence. No recompilar M2 para este cambio.

## Pendientes y riesgos

- B/Escape y stick/flechas NO validados ni funcionales. El frontend puede navegarse con puntero y botones originales; estos atajos físicos siguen pendientes. Evitar prometer que A es Enter.
- Adaptador temporal de texto ASCII y mappings de frontend; no son controles Quest definitivos. IME se abre automáticamente una vez al detectar creación inicial; acceso X depende de eventos no demostrados. Creación confirmada con versión anterior que tenía barra; no se repitió creación tras quitarla porque se conserva el piloto real.
- La rama SDL gamepad añadida es experimental: sólo conexión demostrada, sin acciones emitidas. No afecta al puntero validado.
- Persisten warnings heredados de estados D3D no soportados, recursos opcionales y música FrFamRoom.IMC ausente; audio completo fuera de alcance. Build registra metadatos riscv64 ignorados y Java8/deprecaciones, sin impedir ARM64.
- Sin stereo VR, tracking OpenXR, mappings de vuelo ni optimizaciones de milestones posteriores.

Criterios principales M6: frontend real, nombre introducido, piloto creado/confirmado, avance de estado, ruta de navegación identificada, ausencia de stubs y preservación previa: demostrados. Cobertura adicional de B/stick: pendiente explícita. Se detiene para revisión, sin avanzar a M7.
