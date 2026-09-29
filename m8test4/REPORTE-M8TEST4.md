# M8TEST4 — trabajo en curso

Fuente viva: CHECKPOINT-M8TEST4.md. Baseline test3 congelado. Identidad prevista org.openxwa.xwaquest.m8test4,0.8.2-test4/code5. Sin APK test4 ni validación física todavía. Prioridad instrumentar timing, conservar cleanup contra fuga, investigar estrellas; sin controles ni cambios de simulación.

## Diagnóstico implementado (sin mejora de comportamiento todavía)

M8test4 es un APK de medición previo al experimento de cleanup. No se afirma que alcance72Hz ni arregle flicker/starfield. Se mantiene exactamente el camino WSI conocido y las matemáticas/escala/FOV/meshes/input del baseline.

Archivos distintos de test3: evidence/changed-from-test3.txt; nuevos t4_timing.c/h y documentos test4. m8test3 verificado sin cambios por baseline-m8test3-sha256.csv y mismo SHAAPK3C43C3D5D6E75CB80134CC6C4422E07914A3D13E771AC05D0158960ED6F31175. No escrituras en m8/m8test2.

M8T4_FRAME_TIMING cada≈1s: fps de ciclos host, cpu_wall_ms y máximo; cada etapa avg/calls/max. Etapas xr_wait/xr_begin/locate/acquire/wait_image/release/xr_end; eye0/eye1; blit; cb; submit; wsi_acquire; sim; remaster; memory_sample; cockpit_record/target_record/stars_record. Anidadas: NO sumar. Draw recording no es ejecución GPU. Ventana puede abarcar transición de frontend a Flight; immersive indica último frame del bucket. Tiempo logging queda fuera de cpu_wall del frame pero incluido en intervalo fps. Sin timestamps GPU: gpu_ms=unavailable. Early/Stale/Tear se correlacionan desde VrApi por PID/timestamp, no counters inventados.

M8T4_MEMORY sustituye sólo nombre de muestra periódica M8_MEM_SAMPLE; conserva RSS/PSS/VM/heap/uploads/recursos. M8T4_STARFIELD por ojo demuestra retorno del callback de grabación, vertices_expected36864; no confirma GPU/pixels. Se conservan M8T3_COCKPIT_DRAW_RECORDED, M8_VR_TARGET_DRAW_RECORDED, M8_VR_FIRST_STEREO_FRAME, M8T2_CLEANUP_ACQUIRE y M8_XR_FRAME_PRESENTED (equivalentes a los markers T4 solicitados, documentados sin duplicar logs).

WSI audit local: acquire real añade presentData y marca swapchainRequested, submit usa mismo CB y vkQueuePresentKHR; no CB adicional creado por el wrapper cleanup. Gate `(claimedWindowCount>0 && swapchainRequested)||claimedWindowCount==0` activa limpieza de CBs/fences/uniform/descriptor/pending destroys, además defrag cuando procede. `wsi_acquire_ms` mide espera adquisición; `submit_ms` incluye queue/present/cleanup que todavía no se separan. No eliminar cleanup hasta medir.

Alternativas pendientes: SDL_WaitForGPUFences ejecuta limpieza después de esperar (alternativa pública controlable, riesgo serialización); ReleaseWindowFromGPUDevice podría dejar claimedWindowCount0 pero Aeron aún necesita swapchainformat/SetOutputHdr (no es reemplazo inmediato seguro). No se parchearon internals de SDL. No se probó ni descartó experimentalmente ninguna alternativa todavía: no repetir como hecho una hipótesis.

CPU eye/seat/convert7/7 PASS. Native ARM64 PASS. APK/audit pendientes mientras corre empaquetado. Causa de estrella invisible abierta: shader por SV_VertexID, uniforme VP por ojo, directions w0, depthfar0/GE/writeOFF/cullNONE; callback nuevo permite comprobar llegada sin alterar shaders/brillo. No se implementó corrección no sustentada.

Próxima prueba física (después de autorizar instalación): instalar test4/code5 conservando datos de otros packages; aprovisionar desde test3 usando script local retargeteado y verificar7744/847224595/hash completo antes de lanzar. Captura limpia allbuffers; único lanzamiento; navegar manualmente misma configuración X-Wing/TIE. Mantener20–30s frontend, observar momento exacto pre-flight, al menos60s Flight si sobrevive, luego retorno. Registrar flicker, cockpit/head/TIE/estrellas; correlacionar tiempos por etapa y memoria5s con VrApi. No corregir durante test. No ADB utilizado para test4 en esta sesión.
