# Estado VR-PROBE — sonda estéreo experimental, aislada de M6

**Última versión: v9.2** (2026-09-19) — Corrección de estructura del loop estereoscópico.
APK: `DA35A6E5...50F9`. Corrige `stereo++`/`submitted++` encerrados en `if(in_w3)` y
cleanup dentro del while loop. Stereo loop ahora avanza por todas las ventanas W1–W5.

Proyecto independiente `vr-probe/`, paquete `org.openxwa.xwaquest.vrprobe`,
biblioteca `libOpenXWAVRP.so`. Reutiliza archivos estáticos, dependencias y
shaders aprobados de M2; solo compila el adaptador VR de esta carpeta.

Preservación M6: APK M6 `6FFCE008...0FED` intacto, GameData M6 intacto,
`m0-m6/` sin editar, correcciones OPT sin tocar. Único archivo compartido
modificado: `aeron/src/render_backend.c` (bloque XR opt-in tras
`XWAQUEST_XR_ENABLE`, 21 líneas). Reversión: aplicar
`evidence/shared-render_backend.patch` en reversa o restaurar
`evidence/original-render_backend.c` (SHA256 723E02BD...).

Alcance: visor estéreo pasivo de una X-Wing real (OPT cocinado por la ruta
real snapshot->SyncAssets). Sin gameplay, sin HUD VR, sin controles de vuelo.
