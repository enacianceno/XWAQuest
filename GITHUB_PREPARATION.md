# Preparacion del repositorio personal XWAQuest

Estado al preparar el primer commit: seleccion local revisada, lista para publicar.

Cuenta personal indicada por el usuario: `enacianceno`.
Destino: `enacianceno/XWAQuest`. Verificado mediante GitHub el 2026-09-28:
repositorio privado, vacio y con permiso de escritura para `enacianceno`.

## Alcance

Este repositorio conserva el trabajo de XWAQuest por milestones: fuentes,
scripts de construccion/aprovisionamiento, pruebas y documentacion.
El README actual describe M1; no representa el estado completo de M8.
Para el trabajo integrado, consultar `m8integrated1/CHECKPOINT-M8INTEGRATED1.md`.
Las afirmaciones de pruebas en los checkpoints son registros historicos;
esta preparacion de GitHub no ejecuta ni revalida pruebas en el visor.

Los assets originales del juego, pilotos, claves, APKs, bibliotecas compiladas,
caches y evidencia local quedan fuera de Git. Las exclusiones no borran ni
mueven esos archivos. Los manifiestos de nombres de assets del aprovisionador
son instrucciones de seleccion, no contienen los assets originales.
Los shaders existentes y los wrappers de Gradle necesarios se conservan.

## Dependencias externas pendientes de preservar

M8 integrado utiliza el checkout padre de OpenXWA y bibliotecas generadas
en `m2` y `vr-probe/staging`; no es un checkout autonomo reproducible todavia.
La inspeccion del 2026-09-28 encontro en `C:/OpenXWA`:

- HEAD: `f063965a4647ed6955d45e73095006788e0830e2`.
- Cambios locales en `CMakeLists.txt`, `src/xwa/assets/opt_model.c`,
  `src/xwa/assets/opt_model.h`, `src/xwa/audio/imuse/imcodec.c` y el submodulo `aeron`.

Antes de declarar una compilacion reproducible desde GitHub, hay que auditar
y preservar esos cambios externos y sus versiones, ademas de documentar como
regenerar las bibliotecas. Este trabajo no modifica el checkout padre.
Un commit de XWAQuest por si solo no respalda esos cambios externos.

## Revision antes de publicar

1. Confirmar cuenta personal, nombre del repositorio y visibilidad privada.
2. Pausar otros agentes que esten editando el proyecto durante el primer snapshot.
3. Revisar la lista final de archivos y comprobar que no incluya datos de juego,
   datos personales ni secretos. `.gitignore` es una barrera, no una auditoria completa.
4. Revisar licencias y atribuciones de las fuentes y dependencias incluidas.
5. Crear el primer commit revisado y publicar en el repositorio acordado.

La seleccion inicial contiene 600 archivos. Se comprobaron las exclusiones y
patrones conocidos de credenciales sin encontrar coincidencias; esto no es una
garantia exhaustiva de ausencia de secretos. No se ejecutaron builds ni ADB.
Los hashes de la seleccion previa se conservaron en evidencia local excluida de Git.
GitHub conserva unicamente los archivos versionados: mantener los respaldos
locales de GameData, pilotos, APKs y evidencia por separado.
