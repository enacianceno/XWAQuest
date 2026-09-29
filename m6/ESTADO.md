# Estado M6 — pendiente de prueba física

M6 separado en `m6/`, paquete org.openxwa.xwaquest.m6. Motor, dependencias y shaders reutilizados de M2. Fuentes originales, OPT y milestones previos sin modificar; hashes de M5 verificados contra evidence/preserved-m5.csv.

Implementación: RuntimeActivity.java incorpora EditText/IME Android y barra temporal Nombre/Confirmar/Volver/flechas/Captura. input_android.c envía SDL_TEXT_INPUT y pulsaciones reales, recibidas por Aeron y XwaInputBridge. No escribe datos de piloto. Los wrappers observan y llaman las funciones reales del diálogo, campo editable y Pilot_CreateNew. Nombre temporalmente limitado a ASCII alfanumérico de 1–12 caracteres. No hay mappings de vuelo/Touch definitivos.

Ruta: FrontendDialog_PromptForPilotName → FrontendText_DrawEditableField → confirmación Return o botón original → Pilot_CreateNew → MissionSetup_LoadMissionList TOUR/MELEE → File_WriteCount y Pilot_Save reales. Se añadió MELEE original Steam (8 archivos) solamente a la copia privada M6. GameData M6: 7660 archivos. Originales y copia M5 preservados.

Primer arranque instalado comprobado: PID3109, INPUT_STATUS=1, frontend 134 texturas/131 archivos, PRESENT count=120 ok=1, captura automática m6-1-state1.png. Todavía NO se ha demostrado creación del piloto ni teclado físico del visor.

Prueba pendiente: Nombre → introducir nombre → Enviar al juego → comprobar campo real → Confirmar → observar siguiente pantalla y Captura. Logs automáticos en evidence/runtime-live.txt; PID del logger host en evidence/logger-pid.txt. Capturas PixelCopy en files/ de la aplicación. Tras prueba, recuperar logs, capturas y evidencia de .plt real; generar REPORTE-M6.md.

CMake/Gradle derivados aislados de M5; main real generado con skipintro (opción original), límite15min y guardia de vuelo; no M7. SDL_ENABLE_SCREEN_KEYBOARD=0 heredado evita apertura automática SDL: EditText abre IME Android explícitamente. Flechas mantienen key_down durante un snapshot real. Puntero/touch de la ventana sigue ruta SDL original, pendiente de prueba física.

Warnings build: Java8 obsoleto y metadatos riscv64 ignorados por CMake3.22/NDK28; no impiden ARM64. Riesgos heredados M5: estados D3D no soportados y recursos opcionales ausentes. No declarar M6 completado hasta creación real y estado posterior confirmados.

## Actualización después de prueba física
Usuario confirma interacción. Log real: nombre neto recibido por SDL_TEXT_INPUT, Pilot_CreateNew END result=1 a las09:42:45.230; VFS escribe neto0.plt en USER, 152076bytes verificados. Captura evidence/pilot-created.png muestra sala familiar real (concourse, droid y correo), más allá del diálogo. Lecturas originales missions/mission.lst y melee/mission.lst correctas. No piloto artificial.

Usuario pide eliminar barra y separar ray/hover de clic. RuntimeActivity ahora sin overlay; ACTION_HOVER_ENTER/MOVE (incluyendo FINGER/UNKNOWN que SDL no trata como ratón) envía sólo movimiento SDL, DOWN/UP envían botones reales, MOVE no emite clic. Eventos táctiles originales se consumen evitando duplicación touch-to-mouse. A/B mapeados a Return/Escape y stick/hat a flechas en flanco, X abre diálogo si el juego solicita nombre. IME abre automáticamente una vez al entrar al diálogo inicial. Pendiente demostrar que el sistema Quest entrega hover y botones físicos a esta ventana Android; no se ha incorporado OpenXR tracking.

No debe afirmarse que el apuntado continuo está validado antes de la próxima prueba. Si Android no entrega hover, evaluar evidencia y alcance antes de rediseñar hacia tracking OpenXR. No avanzar a M7. El piloto existente se conserva; próxima prueba es navegación sin barra y puntero independiente del gatillo. Evitar Play Mission, guardia de vuelo ya detuvo la sesión previa.
APK nuevo instalado SHA256 81BA361C42F3CBDAA8D745694EACD93D97C8CFD29445141787162ACB04FD996F. Logger anterior ya terminado; nuevo logcat activo mediante exec session33842 hacia evidence/runtime-pointer.txt. Antes de comandos tras prueba indicar al usuario que puede quitarse visor.
Logger definitivo exec session24341: adb logcat -T1 hacia evidence/runtime-pointer-live.txt con escritura inmediata por línea; session33842 detenida por incluir histórico antiguo. Próxima acción depende de prueba física del usuario.

## Segunda prueba física y ajuste SDL
Usuario confirma interfaz completa, ray hover sin gatillo y clic independiente. A/B y stick no respondieron. No se seleccionó misión. Logs muestran HOVER source4098 tool1 y DOWN/UP separados; ningún BUTTON/STICK Java. Se añade controller_key() en input_android.c: sondea SDL_GetGamepadButton/Axis de los mandos reales de Aeron, mapea SOUTH/EAST a Return/Escape y stick izquierdo/dpad a flechas por flanco. Logs M6 PAD registran conexión y cambios. No se modifica motor ni puntero validado. Build incremental exitoso12s,5tareas ejecutadas/28up-to-date; APK instalado preservando neto0.plt152076bytes. Pendiente prueba física A/B y sticks; logger session24341/runtime-pointer-live.txt.
Cierre: REPORTE-M6.md generado. Principal M6 demostrado, B/stick pendientes. Última prueba A actúa como clic; no cambios PAD ni SDL key. No afirmar Enter. No M7.
