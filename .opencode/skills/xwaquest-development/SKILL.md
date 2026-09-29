---
name: xwaquest-development
description: Desarrollo, diagnóstico y revisión del proyecto XWAQuest para Meta Quest standalone, incluyendo OpenXR, SDL, Aeron y controles VR.
---

# XWAQuest Development Skill

## 1. OBJETIVO DEL PROYECTO

Portar Star Wars X-Wing Alliance a Meta Quest 3/3S como aplicación standalone Android ARM64, utilizando los archivos originales del juego y evolucionando progresivamente hacia una experiencia VR estereoscópica.

## 2. PROTECCIÓN DE VERSIONES

Preservar M6, M7A y M7B, especialmente M7B, que ha ejecutado el juego original en un panel 2D.

No modificar, eliminar, sobrescribir ni reinstalar versiones funcionales sin autorización explícita del usuario.

Preservar GameData, los archivos originales del juego y los datos del piloto.

Trabajar en versiones de prueba separadas y documentar las rutas y paquetes Android utilizados.

## 3. REGLAS DE DIAGNÓSTICO

Distinguir siempre entre:
- Hechos observados en código, logs o pruebas.
- Hipótesis técnicas.
- Cambios propuestos que aún no han sido comprobados.

No afirmar que una causa raíz está demostrada si los registros admiten explicaciones alternativas.

Antes de modificar código:
- Identificar los archivos y funciones implicados.
- Explicar qué problema pretende resolver el cambio.
- Proponer una prueba mínima y reversible.
- Solicitar autorización del usuario.

No presentar una compilación exitosa como prueba de que el juego funciona correctamente en el visor.

## 3. SEGURIDAD ADB

Antes de CUALQUIER comando ADB, incluso adb devices, preguntar literalmente:

"¿Ya encendiste el Quest 3S, lo conectaste a la PC y autorizaste la depuración USB? Confírmame cuando esté listo para continuar."

Esperar la confirmación del usuario antes de continuar.

No ejecutar comandos ADB destructivos ni desinstalar versiones funcionales sin autorización específica.

## 4. ARQUITECTURA VR

Mantener separados, en la medida que lo permita la arquitectura actual:
- Motor y lógica del juego.
- Entrada de controles.
- Cámara del jugador.
- Renderizado del juego.
- Presentación OpenXR.

El panel 2D es una etapa intermedia. El objetivo final es una experiencia VR con perspectivas estereoscópicas reales, no una imagen plana ampliada.

No modificar el comportamiento de vuelo, la cámara o los controles sin documentar primero cómo funcionan en el juego original.

## 5. CONTROLES QUEST

Diseño previsto, pendiente de implementación y validación:

- Grip derecho mantenido: control de orientación de la nave mediante la inclinación del controlador.
- Al soltar el grip: neutralizar la entrada sin devolver la nave a su orientación anterior.
- Thumbstick derecho, eje vertical: control de aceleración.
- Movimiento de cabeza: orientación de la vista del piloto, independiente de la dirección de vuelo.
- Gatillo derecho: disparo durante el vuelo; conservar la interacción ya comprobada en el hangar.

No asumir que estos controles ya están implementados en M7C.

## 6. COLABORACIÓN CON CHATGPT

ChatGPT se utiliza en una aplicación separada para revisar diagnósticos, discutir arquitectura y preparar propuestas.

No asumir que ChatGPT puede ver automáticamente los archivos locales, las sesiones de OpenCode ni los cambios recientes.

Cuando el usuario solicite un informe para ChatGPT, incluir:
- Versión y carpeta examinadas.
- Objetivo de la tarea.
- Archivos y funciones relevantes.
- Evidencia concreta, con rutas y líneas cuando sea posible.
- Resultados comprobados.
- Hipótesis pendientes.
- Próxima prueba propuesta.

No ejecutar cambios propuestos por ChatGPT sin autorización del usuario.