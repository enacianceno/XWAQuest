# Aprovisionamiento M8 — selección explícita v2

Origen único: /data/user/0/org.openxwa.xwaquest.m7/files/GameData.
Destino: /data/user/0/org.openxwa.xwaquest.m8/files/GameData.

asset-selection.sh enumera 7744 rutas exactas: las7660 de reference-inventory.csv y84 assets adicionales (MUSIC/*.IMC, RESOURCE/*.ICO, plantillas SKIRMISH/*.SKM, fuentes, paleta, SPEC.RCI y planes AI PAIPLAN.PLN/PLO). Estos últimos se leen como assets por mission_setup/skirmish/pai_plan. XWAHS.TBL es puntuación personal y queda excluido. No pilotos, backups, configuración, temporales SKIRMISH ni ejecutables/DLL/scripts. No se utiliza el total7773 del origen como condición de aceptación.

asset-manifest.sh calcula SHA-256 y bytes de cada ruta explícita. manifest.tsv es el único inventario que alimenta tar, conteo, suma de bytes y comparaciones. Se recalculan origen y destino, se exige igualdad exacta y conteo del staging. Tras promoción sin sobrescritura se verifican de nuevo todos los hashes antes de emitir M8_DATA_HASHES_OK y M8_DATA_READY.

El staging antiguo files/.m8-gamedata-import se conserva intacto. Se usa files/.m8-gamedata-import-v2 (0700); si existe staging-v2 o GameData, se aborta. Copia por tubería entre UIDs, sin tar de assets en almacenamiento público. El origen sólo se lee; el worker rechaza M7 en ejecución y sólo detiene M8 antes de copiar. No instala ni inicia aplicaciones. Cualquier fallo bloquea el lanzamiento; no elimina ni repara el staging.

Pruebas locales: tests/manifest-tests.py con fixtures sintéticos y comandos Android simulados. PASS success, existing, stageexists, corrupt, missing, sourcechange; sintaxis y cobertura del baseline completo verificadas. Logs: evidence/gamedata/manifest-v2-fjt650ph. Fuentes M8 y APK intactos según application-preservation-current.csv.

El usuario autorizó esta copia y lanzamiento condicionado del M8 YA instalado, sin recompilar/reinstalar. Confirmó conexión física. Log real: evidence/gamedata/device-provision-v2.log. No declarar datos preparados sin los dos marcadores reales. Después del lanzamiento no diagnosticar ni corregir y no cerrar M8 automáticamente.
