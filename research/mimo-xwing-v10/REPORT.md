# REPORT — Auditoría de la implementación incompleta X-Wing V10 (relevo de Nemotron)

Fecha: 2026-09-24
Fase: **INVESTIGACIÓN SOLAMENTE (read-only)**. No se compiló la aplicación, no se modificó
ningún fuente principal, no ADB, no APK, no shaders, no `fix_*.py`.

Contexto cargado antes de auditar:
- `.opencode/skills/xwaquest-development/SKILL.md`
- `XWAQUEST_STATUS.md` (nota: está **desactualizado**, describe la era del fallo
  `VrStereo_Init ok=0`; el éxito V10 físico con triángulo en Quest 3S no está registrado.
  No se ha modificado por estar fuera de scope.)

---

## 1. Estado real de los tres archivos modificados por Nemotron

Hashes SHA-256. "Antes" = registrado al inicio de la sesión, antes de cualquier edición.
"Ahora" = estado actual del disco.

| Archivo | SHA-256 antes | SHA-256 ahora | ¿Modificado? |
|---|---|---|---|
| `vr-probe/vr_stereo.c` | `3a13619a2f8af3987595e4c42eaf739d730591d1325ce111ab4152b6d18e275c` | `19aada583a607c4cf0b6dce941fabfbd881e81cf9f926c2e3bc73656c96f13d1` | **SÍ** |
| `vr-probe/vr_stereo.h` | `e539acdc8324c724378920d5432f49f7738875641c382fc2e7ffb8c6806023bc` | `b225ccbc1a5ec325bd143bc5225f52ca493568d67930c3737bc2770a737bded7` | **SÍ** |
| `vr-probe/vr_main.c` | `949b7f70bd6a642c24c1d79507bea79127449a18658aea8139f4a495920279e4` | `eeb32245e5f36b9d181cf9d1a0581162a0747b791e5dc64ef34fb35b7a19b36f` | **SÍ** |

**Resolución de la inconsistencia del reporte de Nemotron:** el hash de `vr_stereo.h`
CAMBIÓ → **`vr_stereo.h` SÍ fue modificado**. La frase posterior ("vr_stereo.h no fue
modificado/correcto") es **falsa**. La evidencia adicional es la propia línea nueva:

- `vr_stereo.h:58-61` — bloque nuevo (comentario + 2 declaraciones):
  ```c
  /* X-Wing mesh: create independent V10 vertex/index buffers from AeronSceneMesh
   * and draw using the existing V10 pipeline (shadercross VS+FS, clip space). */
  int VrStereo_CreateMeshBuffers(AeronSceneMesh *mesh);
  void VrStereo_DrawMesh(AeronCommandBuffer *cmd);
  ```

### 1.1 Cambios exactos por archivo

**`vr-probe/vr_stereo.h`** (1 inserción):
- Líneas 58-61: declaraciones de `VrStereo_CreateMeshBuffers` y `VrStereo_DrawMesh`.

**`vr-probe/vr_stereo.c`** (5 inserciones, ninguna sustitución de código validado):
1. Líneas 37-41 — globals nuevos:
   `s_v10_mesh_vbuf`, `s_v10_mesh_ibuf`, `s_v10_mesh_index_count`, `s_v10_mesh_ready`.
2. Líneas 251-254 — llamada `VrStereo_DrawMesh(cmd)` dentro de `VrStereo_RenderEye`,
   justo después de `AeronScene_Render` y **antes** del pass de tonemap.
3. Líneas 384-592 — función nueva `VrStereo_CreateMeshBuffers`.
4. Líneas 594-608 — función nueva `VrStereo_DrawMesh` (la rota).
5. Líneas 656-665 — cleanup en `VrStereo_Shutdown` (libera ambos buffers, resetea flags).

**`vr-probe/vr_main.c`** (1 inserción):
- Líneas 435-446 — tras `VrStereo_Init`:
  `if (stereo_ok && mesh) { if (!VrStereo_CreateMeshBuffers(mesh)) → FATAL + shutdown + return 1; }`

**NO tocados** (verificado): `VrStereo_DrawTriangle` (345-382), `VrStereo_InitV10Pipeline`
(674-878, incluye `SDL_GPU_CULLMODE_NONE` en 755), shaders, `vr_openxr.c`, `vr_blit.c`,
parser OPT, Aeron, `vr_framing.c`, `vr_convert.c`, `vr_audit.c`, `vr_readback.c`,
`vr_trace.c`, `XWAQUEST_STATUS.md`.

### 1.2 Archivo huérfano creado (fuente de scope)
- `vr-probe/vr_stereo_add_mesh.c` (escrito por Nemotron con la tool `write`) — 4 líneas,
  solo declaraciones, **sin headers** → si algún día entrara en el build daría error
  (`AeronSceneMesh` desconocido). **Verificado: `CMakeLists.txt:17` usa lista explícita de
  fuentes, NO glob** → este archivo **no se compila** hoy. No afecta al build; debe
  eliminarse en la fase de corrección (borrado = cambio de archivo nuevo, no de fuente
  principal).

---

## 2. Errores actuales de compilación

### 2.1 Textuales, capturados del último `ninja` real ( sesión, salida truncada por el harness )

```
FAILED: CMakeFiles/OpenXWAVRP.dir/vr_stereo.c.o
C:/OpenXWA/XWAQuest/vr-probe/vr_stereo.c:505:14: error: use of undeclared label 'tvbuf_fail'
  505 |         goto tvbuf_fail;
      |              ^
C:/OpenXWA/XWAQuest/vr-probe/vr_stereo.c:602:33: warning: incompatible pointer types passing
      'SDL_GPUCommandBuffer *' (aka 'struct SDL_GPUCommandBuffer *') to parameter of type
      'SDL_GPURenderPass *' (aka 'struct SDL_GPURenderPass *') [-Wincompatible-pointer-types]
  602 |     SDL_BindGPUGraphicsPipeline(cmd->command_buffer, s_v10_pipe);
C:/OpenXWA/XWAQuest/vr-probe/vr_stereo.c:604:30: warning: incompatible pointer types ... [TRAZADO CORRIDO AQUÍ]
```
La captura se cortó en la línea 604. Los diagnósticos de 605/606 no quedaron en el trazo
guardado.

### 2.2 Derivados por inspección estática (no compilados de nuevo, según la consigna)

| Línea | Severidad | Diagnóstico derivado de las firmas reales |
|---|---|---|
| 515, 521, 529 | (mismo que 505) | Otros 3 × `goto tvbuf_fail` hacia la misma label inexistente (`grep goto` confirma 4 usos: 505/515/521/529; labels existentes: 578 `tibuf_fail`, 580 `ibuf_fail`, 585 `vbo_fail`). Clang reportó 505 una sola vez en el trazo capturado; los otros tres referencian la misma label ausente. |
| 605 | **error** | `too many arguments to function call` — `SDL_BindGPUIndexBuffer` espera **3** argumentos; se pasan **4**. Además el argumento 2 es `SDL_GPUBuffer *` donde se espera `const SDL_GPUBufferBinding *`. |
| 606 | warning | incompatible pointer types: `SDL_GPUCommandBuffer *` → `SDL_GPURenderPass *` (mismo caso que 602/604). |

**Evidencia de alcance del fallo:** el segundo `ninja` mostró `[1/2]` con solo
`vr_stereo.c.o` en cola → `vr_main.c.o` **compiló con éxito** en la primera ejecución, y con
él `vr_stereo.h` (incluido por `vr_main.c:51`). Por tanto: **el error está confinado a
`vr_stereo.c`**; la cabecera y `vr_main.c` compilan limpios.

---

## 3. Verificación / refutación de cada diagnóstico de Nemotron

| # | Diagnóstico de Nemotron | Veredicto | Evidencia |
|---|---|---|---|
| 1 | `vr_stereo.c:505 use of undeclared label 'tvbuf_fail'` | **CONFIRMADO** | Trazo del ninja (§2.1) + `grep goto`: 4 usos (505/515/521/529), label no existe en la función. |
| 2 | `DrawMesh` pasa `cmd->command_buffer` donde SDL espera `SDL_GPURenderPass*` | **CONFIRMADO** | `aeron/src/internal.h:111` → `struct AeronCommandBuffer { SDL_GPUCommandBuffer* command_buffer; ... }`. `SDL_gpu.h:3645-3647` → `SDL_BindGPUGraphicsPipeline(SDL_GPURenderPass*, ...)`. Es *warning* en C (no error) → compila pero la llamada es inválida en runtime (bind fuera de render pass). |
| 3 | `SDL_BindGPUIndexBuffer` con firma incorrecta | **CONFIRMADO (y la corrección propuesta de Nemotron es insuficiente)** | `SDL_gpu.h:3729-3732` → `(SDL_GPURenderPass*, const SDL_GPUBufferBinding*, SDL_GPUIndexElementSize)`. Nemotron pasó 4 args. **Pero su propuesta D** (`SDL_BindGPUIndexBuffer(dp->render_pass, s_v10_mesh_ibuf, SDL_GPU_INDEXELEMENTSIZE_16BIT)`) sigue siendo **incorrecta**: el arg 2 debe ser **puntero a `SDL_GPUBufferBinding`**, no `SDL_GPUBuffer*`. |
| 4 | `SDL_DrawGPUIndexedPrimitives` recibe `command_buffer` | **CONFIRMADO (parcialmente)** | `SDL_gpu.h:3905-3911` → 6 parámetros: el llamado actual pasa 6 args ⇒ **el número es correcto**; el defecto es solo el tipo del arg 1 (`SDL_GPUCommandBuffer*` → `SDL_GPURenderPass*`), warning. |
| — | (omisión de Nemotron) | **AÑADIDO POR ESTA AUDITORÍA** | `DrawMesh` **no abre ningún render pass** y no recibe color target → aun arregladas las firmas, dibujar sin pass activo es inválido. Ver §7-P1. |
| — | (omisión de Nemotron) | **AÑADIDO POR ESTA AUDITORÍA** | Sitio de llamada dentro de `RenderEye` **antes** del tonemap que limpia `s_present[eye]` (`clear_color=1`, línea 266-272) → cualquier dibujo sobre el present RT ahí sería **borrado**. Ver §7-P2. |
| — | (omisión de Nemotron) | **AÑADIDO POR ESTA AUDITORÍA** | Doble `SDL_free(vdata)` (485→590) y doble `SDL_ReleaseGPUTransferBuffer(tibuf)` (547→579) en rutas de fallo. Ver §7-P3/P4. |

---

## 4. Firma real de cada API SDL_GPU relevante

Fuente: `C:\OpenXWA\third_party\SDL3\include\SDL3\SDL_gpu.h` (SDL 3.5.0, revisión 8f8ed75).

| API | Firma exacta | Línea |
|---|---|---|
| `SDL_BindGPUGraphicsPipeline` | `(SDL_GPURenderPass *render_pass, SDL_GPUGraphicsPipeline *graphics_pipeline)` | 3645 |
| `SDL_BindGPUVertexBuffers` | `(SDL_GPURenderPass *render_pass, Uint32 first_slot, const SDL_GPUBufferBinding *bindings, Uint32 num_bindings)` | 3712 |
| `SDL_BindGPUIndexBuffer` | `(SDL_GPURenderPass *render_pass, const SDL_GPUBufferBinding *binding, SDL_GPUIndexElementSize index_element_size)` | 3729 |
| `SDL_DrawGPUIndexedPrimitives` | `(SDL_GPURenderPass *render_pass, Uint32 num_indices, Uint32 num_instances, Uint32 first_index, Sint32 vertex_offset, Uint32 first_instance)` | 3905 |
| `SDL_DrawGPUPrimitives` | `(SDL_GPURenderPass*, Uint32 num_vertices, Uint32 num_instances, Uint32 first_vertex, Uint32 first_instance)` | 3933 |
| `SDL_UploadToGPUBuffer` | `(SDL_GPUCopyPass *copy_pass, const SDL_GPUTransferBufferLocation *source, const SDL_GPUBufferRegion *destination, bool cycle)` | 4280 |
| `SDL_BeginGPUCopyPass` | `(SDL_GPUCommandBuffer *command_buffer) → SDL_GPUCopyPass*` | 4240 |
| `SDL_EndGPUCopyPass` | `(SDL_GPUCopyPass *copy_pass)` | 4380 |
| `SDL_AcquireGPUCommandBuffer` | `(SDL_GPUDevice*) → SDL_GPUCommandBuffer*` | (usado, validado) |
| `SDL_SubmitGPUCommandBuffer` | `(SDL_GPUCommandBuffer*) → bool` | 4719 |
| `SDL_CancelGPUCommandBuffer` | `(SDL_GPUCommandBuffer*) → bool` | 4771 |
| `SDL_CreateGPUBuffer` | `(SDL_GPUDevice*, const SDL_GPUBufferCreateInfo*)` ; struct = `{ usage; Uint32 size; props; }` | 3220 / 1815 |
| `SDL_CreateGPUTransferBuffer` | `(SDL_GPUDevice*, const SDL_GPUTransferBufferCreateInfo*)` ; struct = `{ usage; Uint32 size; props; }` | 3255 / 1831 |
| `SDL_MapGPUTransferBuffer` | `(SDL_GPUDevice*, SDL_GPUTransferBuffer*, bool cycle) → void*` | 4207 |
| `SDL_GPU_INDEXELEMENTSIZE_16BIT` | enum, "The index elements are 16-bit." | 673 |
| `SDL_GPUBufferBinding` | `{ SDL_GPUBuffer *buffer; Uint32 offset; }` | 2159-2162 |

Aeron (relación de tipos):
- `AeronCommandBuffer.command_buffer : SDL_GPUCommandBuffer*` — `aeron/src/internal.h:111`
- `AeronRenderPass.render_pass : SDL_GPURenderPass*` — `aeron/src/internal.h:137`  ← **el handle correcto**
- `Aeron_BeginRenderPass(desc) → AeronRenderPass*` / `Aeron_EndRenderPass(pass)` — `aeron/render.h:914`
- `Aeron_Draw(pass, vertex_count, first_vertex)` — `render.h:981`
- `Aeron_DrawIndexed(pass, index_count, first_index, ...)` — `render.h:984`, trabaja con el
  **`AeronBuffer*` ibo ya enlazado vía `Aeron_BindIndexBuffer`**, NO con un `SDL_GPUBuffer*`
  → por eso el IBO propiedad de SDL debe dibujarse con `SDL_BindGPUIndexBuffer` +
  `SDL_DrawGPUIndexedPrimitives` sobre `dp->render_pass` (coincide con la consigna del usuario).

---

## 5. Comparación paso a paso: `VrStereo_DrawTriangle` (baseline validada) vs `VrStereo_DrawMesh` (actual)

Baseline: `vr_stereo.c:345-382`, llamada desde `vr_main.c:752`, **triángulo visible
físicamente en Quest 3S** (`VISUAL_CONFIRMED_BY_USER`).

| Paso | DrawTriangle (VALIDADA) | DrawMesh (ACTUAL) | ¿Correcto? |
|---|---|---|---|
| 1. Guardas | `pipe, vbuf, cmd, color_target != NULL` → si no, log + `return 0` | `ready, pipe, vbuf, ibuf, cmd != NULL` → log + `return` (void). **Falta color target** | ✗ Sin target no hay a dónde dibujar |
| 2. Resolver target | Recorre `s_present[i]` y compara `Aeron_RenderTargetGetTexture(s_present[i]) == color_target` → `present_target` | **No existe** | ✗ |
| 3. Abrir pass | `Aeron_BeginRenderPass({.color_target=present_target, .command_buffer=cmd, .debug_label=...})` → `dp` | **No abre pass** | ✗ (sin pass activo, todo lo demás es inválido) |
| 4. Bind pipeline | `SDL_BindGPUGraphicsPipeline(dp->render_pass, s_v10_pipe)` | `SDL_BindGPUGraphicsPipeline(cmd->command_buffer, ...)` | ✗ handle equivocado (`SDL_GPUCommandBuffer*`) |
| 5. Bind VBO | `binding={s_v10_vbuf,0}` → `SDL_BindGPUVertexBuffers(dp->render_pass, 0, &binding, 1)` | mismo patrón pero con `cmd->command_buffer` | ✗ handle |
| 6. Bind IBO | (no aplica: triángulo non-indexed) | `SDL_BindGPUIndexBuffer(cmd->command_buffer, s_v10_mesh_ibuf, ENUM, 0)` — 4 args y `SDL_GPUBuffer*` | ✗ **firma: 3 args y `const SDL_GPUBufferBinding*`** |
| 7. Draw | `Aeron_Draw(dp, 3, 0)` | `SDL_DrawGPUIndexedPrimitives(cmd->command_buffer, count, 1, 0, 0, 0)` — 6 args ✓ | ✗ solo el handle del arg 1 |
| 8. Cerrar pass | `Aeron_EndRenderPass(dp)` + log | **No cierra pass** (no abrió) | ✗ |
| 9. Retorno/ integración | `int`, llamado en `vr_main.c` tras `RenderEye`, antes de `VrBlit_Copy` | `void`, llamado en `vr_stereo.c:252` **antes del tonemap** | ✗ sitio de llamada distinto de la baseline |

Conclusión: la **única** diferencia conceptual autorizada es paso 6-7
(VBO→VBO+IBO, `Draw`→`DrawIndexed`). El resto de desviaciones (sin pass, sin target,
handle equivocado, sitio de llamada) son defectos, no decisiones de diseño.

---

## 6. Auditoría completa de `VrStereo_CreateMeshBuffers` (líneas 384-592)

### 6.1 Conforme a la consigna ✓
| Requisito | Estado | Evidencia |
|---|---|---|
| Usa `AeronSceneMesh`, no re-parsea OPT | ✓ | 389-394 solo lee `cpu_vertices/cpu_indices` |
| Validación `mesh/vertex_count/index_count > 0` | ✓ | 389-399 |
| `index_count % 3 == 0` | ✓ | 400-404 |
| Log `XWING_MESH_CPU_READY` con vertex/index/triangle_count | ✓ | 405-406 |
| Bounds SOLO de vértices referenciados, recorriendo `cpu_indices` | ✓ | 408-427 |
| Validación `index < vertex_count` → DETIENE (return 0) sin corregir | ✓ | 413-417 |
| `center=(min+max)/2`, `extent`, `max_extent`, `epsilon=1e-4`, `scale=1.6/max_extent` | ✓ | 428-442 |
| Log `XWING_REFERENCED_BOUNDS_OK` con min/max/center/scale | ✓ | 443-445 |
| Transform CPU `p'=(p-center)*scale` | ✓ | 455-462 |
| Sin matrices MVP (clip space directo) | ✓ | ninguna matriz en la función |
| VBO `float3`, stride 12 | ✓ | 449-466; coincide con `v10_vbd.pitch=12` (741) y attr `FLOAT3` (740) |
| IBO copia `cpu_indices` (`uint16_t`) | ✓ | 550 |
| `SDL_GPU_BUFFERUSAGE_VERTEX` / `_INDEX` separados | ✓ | 465 / 480 |
| Secuencia upload validada: Acquire → BeginGPUCopyPass → UploadToGPUBuffer → EndGPUCopyPass → Submit | ✓ **ambas** | VBO 512-530; IBO 553-571; idéntica a la baseline 834-861 |
| `SDL_UploadToGPUBuffer(..., cycle)` 4º arg | ✓ | `1` (525/566) ≡ `true` de la baseline (856) |
| Logs de progreso `XWING_VBO_CREATED`, `XWING_IBO_CREATED`, `XWING_UPLOAD_SUBMITTED` | ✓ | 473, 488, 574 |
| `SDL_GPU_INDEXELEMENTSIZE_16BIT` para `uint16_t` | ✓ (en intención) | 605, pero en la llamada con firma rota |
| `SDL_CULLMODE_NONE` intacto | ✓ | 755, no tocado |
| Código del triángulo V10 conservado | ✓ | `DrawTriangle` 345-382 sin cambios |

### 6.2 Defectos ✗
| # | Línea(s) | Defecto |
|---|---|---|
| D1 | 505, 515, 521, 529 | `goto tvbuf_fail` → **label inexistente → error de compilación** (frena el build). |
| D2 | 485 → 590 | `SDL_free(vdata)` y acto seguido `goto vbo_fail`, cuyo bloque vuelve a hacer `if (vdata) SDL_free(vdata)` → **double-free** en fallo de creación de IBO. |
| D3 | 547 → 579 | `SDL_ReleaseGPUTransferBuffer(dev, tibuf); goto tibuf_fail;` y `tibuf_fail` vuelve a liberar `tibuf` → **double-release** en fallo de map del IBO. |
| D4 | 504 → (505) | Mismo patrón: libera `tvbuf` y salta a una label cuyo cuerpo (intencionado) lo liberaría otra vez; la estructura de labels es inconsistente. |
| D5 | 477 | `s_v10_mesh_index_count` se asigna **antes** de que el IBO exista; en fallo queda seteado (inofensivo porque `ready=0`, pero sucio). |
| D6 | 607 | `VrLog("XWING_DRAW_RECORDED...")` en **cada draw** (2 por frame, indefinidamente) → viola "no inundar logcat; solo primer draw". |
| D7 | 596-599 | Log "draw skip" también se dispararía **por frame** si el mesh no está listo (la llamada está en `RenderEye`). |
| D8 | 601-606 | No abre render pass / no recibe target (§5). |
| D9 | 251-254 | Sitio de llamada: pre-tonemap, `s_present[eye]` se limpia después (`clear_color=1`, 266-272) → el dibujo se perdería si se apunta al present RT. |

Rutas de cleanup correctas (sin defecto): 499→`ibuf_fail` (fallo crear tvbuf) y
542→`ibuf_fail` (fallo crear tibuf) son correctas; 548/556/562/570→`tibuf_fail` correctas
salvo el double-release D3 del caso 548.

---

## 7. Problemas adicionales encontrados (fuera de lo reportado por Nemotron)

- **P1 — `DrawMesh` sin render pass ni color target** (defecto de diseño central; su
  corrección determina la firma de la función).
- **P2 — Orden vs tonemap**: dentro de `RenderEye`, el pass de tonemap (266-279) limpia
  `s_present[eye]` y sobreescribe cualquier cosa dibujada antes sobre ese RT. La baseline
  validada dibuja **después** de `RenderEye`, en `vr_main.c:752`, sobre `present_tex[i]`.
- **P3 — double-free `vdata`** (D2) y **P4 — double-release `tibuf`** (D3): rutas de fallo,
  pero la consigna pide "cleanup correcto ante fallos".
- **P5 — `vr_main.c:437-445` FATAL + shutdown** si `CreateMeshBuffers` falla: mata la app
  aunque el fallback del triángulo siga siendo válido. Contradice "triángulo disponible
  como fallback diagnóstico". Recomendación: log + continuar.
- **P6 — Archivo huérfano** `vr-probe/vr_stereo_add_mesh.c` (§1.2). No entra en el build
  (`CMakeLists.txt:17` lista explícita), pero debe borrarse.
- **P7 — Doble geometría en pantalla**: `RenderEye` sigue pintando el barco real vía
  `AeronScene_AddMeshInstance/Render` (233-247) con su model matrix, y el overlay V10 añade
  una copia magenta centrada/escalada distinta (clip space). Se verán **dos representaciones
  no coincidentes** (o magenta encima del barco). Riesgo visual aceptable para la prueba,
  pero debe anticiparse en el informe al usuario.
- **P8 — Color magenta fijo**: `minimal.frag.spv` es magenta hardcodeado (703). Esperado
  (sin cambios de shader), pero el triángulo fallback es **del mismo magenta** → si ambos se
  dibujan, no se distinguen visualmente. Refuerza la recomendación de hacer fallback mutuamente
  excluyente.
- **P9 — Tamaño IBO y alineación**: `index_count*3` múltiplo de 3 ⇒ tamaño
  `index_count*2` bytes puede no ser múltiplo de 4 (p. ej. index_count=853 no aplica, pero
  index_count=3·impar ⇒ 6·impar bytes ≡ 2 mod 4). El header **no documenta** requisito de
  alineación de `size` (1815-1821); la baseline paddingó el VBO del triángulo a 64 B con un
  comentario sobre "GPU buffer alignment requirements" sin respaldo documental en el header.
  → **Hipótesis, no hecho**: si `SDL_CreateGPUBuffer` fallara en device con sizes no
  alineados, endurecer redondeando `size` al múltiplo de 4/64 (la región de upload puede ser
  menor que el buffer). Marcar para verificar solo si aparece el fallo.
- **P10 — Sin fence de completado tras los uploads**: idéntico a la baseline validada
  (862: "GPU completion not checked") → la documentación de `SDL_UploadToGPUBuffer` garantiza
  que el upload termina en comandos posteriores (4269-4270). Riesgo aceptado, sin cambio.
- **P11 — `XWAQUEST_STATUS.md` desactualizado** (no registra el éxito V10). Documentado aquí;
  no modificado por estar fuera de scope.

---

## 8. Patch mínimo recomendado

Filosofía: hacer que `DrawMesh` sea **estructuralmente idéntica a `DrawTriangle`**, cambiando
solo IBO + draw indexado, y llamarla **en el mismo sitio de llamada validado**
(`vr_main.c`, tras `RenderEye`, antes de `VrBlit_Copy`).

### 8.1 `vr_stereo.h`
```c
int VrStereo_CreateMeshBuffers(AeronSceneMesh *mesh);
/* Misma firma y mismo contrato que VrStereo_DrawTriangle (retorno 1/0). */
int VrStereo_DrawMesh(AeronCommandBuffer *cmd, AeronTexture *color_target);
```

### 8.2 `vr_stereo.c` — cuerpo nuevo de `VrStereo_DrawMesh` (reemplaza 594-608)
```c
int VrStereo_DrawMesh(AeronCommandBuffer *cmd, AeronTexture *color_target) {
    if (!s_v10_mesh_ready || !s_v10_pipe || !s_v10_mesh_vbuf || !s_v10_mesh_ibuf ||
        !cmd || !color_target) {
        return 0;                       /* sin log por frame (evita D7) */
    }
    AeronRenderTarget *present_target = NULL;          /* igual que DrawTriangle:351-361 */
    for (int i = 0; i < 2; i++) {
        if (s_present[i] && Aeron_RenderTargetGetTexture(s_present[i]) == color_target) {
            present_target = s_present[i];
            break;
        }
    }
    if (!present_target) return 0;
    AeronRenderPass *dp = Aeron_BeginRenderPass(&(AeronRenderPassDesc) {  /* igual que :363-367 */
        .color_target   = present_target,
        .command_buffer = cmd,
        .debug_label    = "VRPROBE xwing mesh V10",
    });
    if (!dp) return 0;
    SDL_BindGPUGraphicsPipeline(dp->render_pass, s_v10_pipe);
    SDL_GPUBufferBinding vb = { .buffer = s_v10_mesh_vbuf, .offset = 0 };
    SDL_BindGPUVertexBuffers(dp->render_pass, 0, &vb, 1);
    SDL_GPUBufferBinding ib = { .buffer = s_v10_mesh_ibuf, .offset = 0 };
    SDL_BindGPUIndexBuffer(dp->render_pass, &ib, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    SDL_DrawGPUIndexedPrimitives(dp->render_pass, s_v10_mesh_index_count, 1, 0, 0, 0);
    Aeron_EndRenderPass(dp);
    if (!s_v10_mesh_draw_logged) {                    /* D6: solo primer draw */
        s_v10_mesh_draw_logged = 1;
        VrLog("XWING_DRAW_RECORDED: tris=%u", s_v10_mesh_index_count / 3);
    }
    return 1;
}
```
Nuevo global estático: `static int s_v10_mesh_draw_logged = 0;` (y reset en `VrStereo_Shutdown`).

### 8.3 `vr_stereo.c` — eliminar el call site defectuoso
Borrar líneas 251-254 (el `if (s_v10_mesh_ready) VrStereo_DrawMesh(cmd);` dentro de
`RenderEye`). No se toca nada más de `RenderEye`.

### 8.4 `vr_stereo.c` — arreglos mínimos de cleanup en `CreateMeshBuffers`
- **485**: quitar `SDL_free(vdata);` antes de `goto vbo_fail;` (el bloque `vbo_fail` ya lo libera) → elimina D2.
- **504**: quitar `SDL_ReleaseGPUTransferBuffer(dev, tvbuf);` y dejar solo `goto tvbuf_fail;`, definiendo:
  ```c
  tvbuf_fail:
      if (tvbuf) SDL_ReleaseGPUTransferBuffer(dev, tvbuf);
      /* fallthrough */
  ibuf_fail:
      ...
  ```
  → elimina D1 (label) y D4 (double-release).
- **547**: quitar `SDL_ReleaseGPUTransferBuffer(dev, tibuf);` antes de `goto tibuf_fail;`
  (el cuerpo `tibuf_fail` ya lo libera) → elimina D3.
- **477**: mover `s_v10_mesh_index_count = mesh->index_count;` a después de que el IBO
  exista (o resetearlo en `vbo_fail`/`ibuf_fail`) → elimina D5.

### 8.5 `vr_main.c` — sitio de llamada (replica exacta del validado) y fallback
```c
/* v10: X-Wing mesh into the present texture; triangle diagnostic as fallback. */
if (!VrStereo_DrawMesh(cmd, present_tex[i])) {
    if (!VrStereo_DrawTriangle(cmd, present_tex[i])) {
        VrLog("VRPROBE frame %u eye%d mesh/triangle draw failed", stereo, i);
        frameOk = 0;
        break;
    }
}
```
(reemplaza las líneas 751-756; `VrStereo_DrawTriangle` se conserva intacta y solo
actúa si el mesh no está listo → cumple el requisito 14.)

Además, en 437-445: **recomendado** cambiar el `FATAL … return 1` por
`VrLog("VRPROBE X-Wing mesh buffers failed (fallback to triangle)");` y continuar, para
conservar el fallback en device. (Si el usuario prefiere mantener FATAL —coincide con el
"DETENTE" del índice fuera de rango— se deja como está; es una decisión de política, no técnica.)

### 8.6 Limpieza
Borrar `vr-probe/vr_stereo_add_mesh.c` (archivo huérfano, no compilado).

### 8.7 Cosas que NO se tocan
Shaders (`minimal.vert/frag.spv`), `vr_openxr.c`, `vr_blit.c`, parser OPT, Aeron,
`VrStereo_DrawTriangle`, `VrStereo_InitV10Pipeline` (incluye `CULLMODE_NONE`),
el resto de `VrStereo_RenderEye`.

---

## 9. Cambios de Nemotron que CONSERVO

1. Globals `s_v10_mesh_vbuf/ibuf/index_count/ready` + cleanup en `VrStereo_Shutdown` (656-665) — correcto.
2. Declaraciones nuevas en `vr_stereo.h` (con el ajuste de firma de `DrawMesh`).
3. Todo el cuerpo de validación + bounds referenciados + transform CPU de
   `CreateMeshBuffers` (389-462): **cumple literalmente los puntos 1-8 de la consigna**,
   incluida la parada ante índice fuera de rango y el `scale = 1.6/max_extent`.
4. Secuencia de upload por comandos (Acquire/BeginCopy/Upload/EndCopy/Submit) — idéntica a
   la validada, aplicada a VBO **e** IBO.
5. Secuencia de logs `XWING_*` (con el ajuste D6/D7 del primer-draw).
6. Llamada de creación en `vr_main.c` después de `VrStereo_Init` con `mesh` (348) en scope.

## 10. Cambios de Nemotron que DESCARTO

1. El cuerpo completo de `VrStereo_DrawMesh` (601-607): sin pass, sin target, handle
   equivocado, firma de `BindGPUIndexBuffer` mal aunque se use su propuesta D.
2. El call site dentro de `RenderEye` (251-254): pre-tonemap, se perdería el dibujo.
3. La estructura de labels/cleanup de `CreateMeshBuffers` (485-591): label inexistente,
   double-free, double-release.
4. `VrStereo_DrawMesh(...)` con retorno `void` y sin `color_target`: no puede replicar el
   contrato de `DrawTriangle`.
5. Log `XWING_DRAW_RECORDED` por frame y log "draw skip" por frame.
6. `FATAL + return 1` en `vr_main.c` (P5) — sustituir por fallback salvo decisión en contrario.
7. Archivo huérfano `vr_stereo_add_mesh.c`.

## 11. Riesgos restantes (tras aplicar el patch)

| Riesgo | Tipo | Nota |
|---|---|---|
| R1 — Dibujo magenta encima / junto al barco ya renderizado por Aeron (P7) | visual, esperado | Dos representaciones distintas; no es fallo del código V10. Informar al usuario antes de la prueba. |
| R2 — Orientación del X-Wing en clip space sin rotación (convenio OPT→XR: ex, ez, −ey) | visual | El mesh quedará centrado pero posiblemente "de canto"/de lado. `CULLMODE_NONE` evita que desaparezca; afinar encuadre es milestone posterior. |
| R3 — Tamaño de IBO no múltiplo de 4/64 (P9) | hipótesis, no verificada | No documentado en SDL; solo actuar si `SDL_CreateGPUBuffer` falla en device. |
| R4 — Sin espera de fence tras uploads (P10) | aceptado | Igual que la baseline físicamente validada; la doc de SDL garantiza orden en comandos posteriores. |
| R5 — Cambios de firma (`DrawMesh`) obligan a tocar `vr_stereo.h` + `vr_stereo.c` + `vr_main.c` | scope | Sigue siendo exactamente los 3 archivos autorizados; ningún otro fuente. |
| R6 — Éxito de compilación ≠ éxito visual (regla del skill §3) | proceso | Compilar solo habilita la prueba física pendiente en Quest 3S. |
| R7 — `XWAQUEST_STATUS.md` desactualizado (P11) | documentación | Actualizar solo con autorización, en fase de documentación. |

---

## Veredicto

- La propuesta central de Nemotron (**replicar el patrón de `VrStereo_DrawTriangle`**) es
  **correcta y es la vía recomendada** — pero su implementación concreta de la corrección
  (paso D del informe anterior) **es incorrecta en el argumento de `SDL_BindGPUIndexBuffer`**
  (debe ser `&binding`, no el buffer) y **omite abrir el render pass y recibir el target**.
- La baseline validada no se ha tocado; los shaders no se han tocado.
- Estado: **0 builds nuevos en esta fase**, 0 archivos fuente modificados por esta auditoría.

---

# Runtime Readiness Audit

Auditoría read-only de la pregunta: *¿la geometría que `VrStereo_CreateMeshBuffers`
prepara tiene alta probabilidad de ser VISIBLE Y RECONOCIBLE con el shader V10 actual?*
Cada conclusión está marcada como **DEMONSTRADO** (código/log citable),
**FUERTE EVIDENCIA** (circunstancial pero sólida), **INFERENCIA** (deducción)
o **DESCONOCIDO HASTA RUNTIME**.

## R1. Ruta exacta de la geometría (FASE 1)

```
GameData FLIGHTMODELS/xwing.OPT
→ vr_main.c:325 "VRPROBE loading X-Wing OPT" / :390 "VRPROBE cooked xwing mesh=%p"
→ XwaRemasterOptMesh_Build (src/xwa_remaster/opt_mesh.c:192)
→ Aeron_OptModelBuildMemory (aeron/src/asset/opt_model.c:277)
   ├─ opt_load_memory (parser OPT, LODs 1..N quedan en el struct pero...)
   ├─ OptGltf_BuildMemory (aeron/tools/opt2gltf/opt2gltf.c:878)
   │    ├─ built_mesh_from_opt (:589): SOLO lod = m->lods[0]  ← LOD0 EXCLUSIVAMENTE (:596)
   │    │    ├─ esquinas deduplicadas (pos+nrm+uv) vía built_mesh_emit_corner (:553)
   │    │    ├─ quads → 2 triángulos (:729-733); normales re-generadas por ángulo
   │    │    └─ UNA primitiva glTF POR face group (:1249-1318)
   │    │         ├─ POSITION/NORMAL/UV: accessor ÚNICO COMPARTIDO por componente
   │    │         │   (pos_acc :1225-1226; attrs[0].data = pos_acc :1279)
   │    │         └─ índices: accessor PROPIO por face group (fg_index_count_arr[g], :1262)
   │    └─ ejes: swap_axis_v3 = (−ox, oz, −oy) (:34-39); ×0.024414 m/un (OPT_METERS_PER_UNIT :41)
   ├─ build_opt_semantics: componentes/topología para flight (también solo lods[0], opt_model.c:119/144)
   └─ aeron_gltf_cook_data → opt_model_consumer → Aeron_GltfMeshBuildData (scene/gltf_mesh.c:383)
        ├─ Pass1 (:450-460): total_v += pos->count POR PRIMITIVA (accessor compartido
        │   se cuenta una vez por face group → INFLA vertex_count) ; total_i = Σ índices reales
        ├─ Pass2 + append_primitive_vertices (:306-372): copia el bloque de vértices del
        │   accessor POR CADA primitiva (duplicación real) y sesga índices +voff (:365-366)
        ├─ gltf_to_aeron3 = (−gx, −gz, gy) (:44-49) → compuesto con swap_axis_v3 = IDENTIDAD
        │   respecto a las coordenadas OPT originales: aeron_pos = (ox, oy, oz)
        └─ partición índices opaco/mask/blend (:548-590): xwing = 852/0/0 (todo opaco)
→ AeronScene_MeshCreate (scene/mesh.c:266-296): sube VBO/IBO fusionados,
   conserva cpu_vertices / cpu_indices (mismo array fusionado)
→ VrStereo_CreateMeshBuffers (vr_stereo.c:384-592):
   valida los 852 índices (bad_idx=0), bounds SOLO sobre referenciados,
   center/scale, clip = (pos − center)·scale en CPU (:456-463),
   VBO float3 stride 12 (10380·12 = 124 560 B), IBO uint16 (852·2 = 1704 B), uploads XWING_*
→ VrStereo_DrawMesh (vr_stereo.c:601-647) llamado en vr_main.c:751,
   DESPUÉS del tonemap y ANTES de VrBlit_Copy (mismo sitio validado del triángulo)
```

**Nota de evidencia:** los totales `10380 verts, 852 indices (852 opaque, 0 mask,
0 blend), 90 prims, 39 materials, 4 variants` provienen del log de dispositivo real
`[flight_gltf] FLIGHTMODELS/xwing.OPT:` (gltf_mesh.c:597), presente en dos sesiones
independientes (`evidence/runtime-full.txt:135680+`). — **DEMONSTRADO**

## R2. LOD y submesh realmente utilizados

- **DEMONSTRADO:** solo **LOD0**. Toda la conversión referencia `m->lods[0]`
  (opt2gltf.c:596, :1169, :1203; opt_model.c:119, :144). No existe ninguna lectura
  de `lods[1..]` en la conversión → ni LOD1+ ni "otros LODs" entran en los 852 índices.
- **DEMONSTRADO:** se convierten **TODOS los componentes (meshes) del OPT** con caras
  no vacías (`for mi < opt->mesh_count`, `if (built[mi].vertex_count == 0) continue;`,
  opt2gltf.c:1167/1199) y **todos sus face groups con ic>0** (:1253-1254), sin ningún
  filtro posterior en Pass2 de gltf_mesh.c. 90 primitivas = 90 face groups de LOD0.
- **DEMONSTRADO:** dentro de LOD0 no se omite ningún submesh: los 852 índices son la
  suma de TODOS los face groups (`total_i += idx->count` por primitiva, gltf_mesh.c:458;
  append sin filtrado, :495-498). Únicos descartes posibles: face groups vacíos y
  componentes sin caras (no aplican a xwing: 90 prims cocinadas).

## R3. Explicación vertex_count ≈ 10380 vs index_count ≈ 852 (530 referenciados)

- **DEMONSTRADO:** `index_count = 852` es **correcto y completo**: es la suma real de
  los índices de las 90 primitivas = **284 triángulos**, con 530 vértices únicos
  referenciados (852 esquinas → 530 únicos es normal en malla indexada con costuras
  por material/face group). Auditoría previa: `bad_idx=0, degen=0`.
- **DEMONSTRADO:** `vertex_count = 10380` está **inflado, no representa geometría
  extra**: Pass1 suma `pos->count` **por primitiva** y el accessor POSITION es
  COMPARTIDO entre los face groups de un componente (opt2gltf.c:1279) → el mismo
  buffer de vértices se cuenta G veces (una por face group). Además Pass2 lo duplica
  realmente en el VBO fusionado (append por primitiva con sesgo de índices).
  Ecuación consistente: Σ_prims V_componente = 10380.
- **FUERTE EVIDENCIA** de que NO dibujamos "solo una parte": los bounds de los 530
  referenciados (audit: min≈(−5.46,−6.49,−1.69), max≈(5.51,6.48,1.50)) coinciden con
  los bounds globales del modelo en el log de framing (min=(−5.467,−6.529,−1.695),
  max=(5.508,6.480,1.503)) — diferencias ≤0.04 m, dentro del ruido de redondeo del
  log (±0.0125). Los referenciados alcanzan las MISMAS extremas que el modelo completo.
- **Conclusión:** los 852 índices = TODO el LOD0 cocinado; los 10380 vértices = coste
  de conteo/duplicación (108 KB extra de VBO), **impacto visual 0**. No falta geometría.
- **INFERENCIA / parcial DESCONOCIDO:** la discrepancia con el sondeo raw
  "OPT solid probe: 401 triángulos" (log hello_xr) no se pudo resolver estáticamente;
  hipótesis más probable: ese sondeo cuenta otros LODs o solo ciertos tipos de mesh.
  No afecta: lo que se dibuja es lo cocinado (284), no el sondeo.

## R4. Cobertura geométrica de los 284 triángulos

- **FUERTE EVIDENCIA:** 90 face groups de TODOS los componentes LOD0 entran
  (log device `90 prims` + código sin filtro) → fuselaje, alas, cabinas, motores:
  todas las partes del LOD0 tienen face group propio y ninguna se excluye.
- **FUERTE EVIDENCIA:** los referenciados cubren el bounding box completo del modelo
  (comparación R3) → nariz, cola, punta de ala y altura máximas están representadas.
- **INFERENCIA:** 284 triángulos / 530 vértices es un X-Wing low-poly completo
  (los OPT de XWA son de baja poligonización; era viable en la era PS1). La silueta
  (fuselaje + 4 alas en X) es reconocible con esa densidad; la distribución EXACTA de
  caras por parte del modelo no es enumerable estáticamente sin volcar índices (no se
  hizo en esta fase).
- **DESCONOCIDO HASTA RUNTIME:** que un observador lo identifique como X-Wing.
  Riesgo de reconocimiento no viene de la cantidad de triángulos sino de la
  orientación (R5) y del color (R8).

## R5. Orientación y ejes (FASE 2)

- **DEMONSTRADO:** `cpu_vertices` está en **coordenadas OPT originales** (los dos
  cambios de eje se cancelan: `swap_axis_v3` (−ox,oz,−oy) y luego `gltf_to_aeron3`
  (−gx,−gz,gy) ⇒ a = (ox,oy,oz)), en metros (×0.024414).
- **FUERTE EVIDENCIA** de los ejes por extensión (bounds 10.98 × 13.01 × 3.20 m,
  coincide con dimensiones T-65: largo ≈12.5 m, envergadura ≈11.8 m, alto ≈2.6-3.2 m):
  **X = envergadura (alar), Y = longitud (nariz-cola), Z = vertical (altura)**.
- **DEMONSTRADO** cruzado con la ruta Aeron validada: `build_model_matrix` aplica
  `xr=(ex, ez, −ey)` (vr_stereo.c:62-63; matriz de framing con filas 0.08·ex /
  0.08·ez / −0.08·ey) — es decir, en espacio-malla Aeron Z es arriba y Y es
  profundidad, consistente con la lectura de bounds.
- **Transformación V10 (sin rotación):** `clip = (pos − center)·scale` ⇒
  - **clip X ← envergadura** → eje horizontal de pantalla
  - **clip Y ← longitud** → eje VERTICAL de pantalla
  - **clip Z ← altura** → profundidad
- **Respuesta a la pregunta principal (DEMONSTRADO por aritmética): NO existe riesgo
  de "de lado", "de canto" ni de invisible.** El modelo nunca se mira borde a borde:
  envergadura y longitud cubren los dos ejes de pantalla con margen (R6). Lo que se
  ve es una **VISTA CENITAL (plano/al cenital)**: como mirar el X-Wing desde arriba,
  con el fuselaje VERTICAL en pantalla y las alas extendidas horizontalmente.
- **INFERENCIA:** esa vista cenital es reconocible (silueta X), pero aparecerá
  "rotado 90°" respecto a una vista 3/4 habitual → el usuario reportará probablemente
  orientación "incorrecta" (caso F del protocolo). Esto es consecuencia esperada del
  diseño actual, no un fallo.
- **Signo de nariz (arriba/abajo en pantalla):** depende de si la nariz OPT es +y o −y
  — **DESCONOCIDO HASTA RUNTIME** (no se volcó el modelo). No afecta visibilidad.
- **Espejado:** visto desde +Z (cenital), +x derecha / +y arriba en NDC estándar ⇒
  **INFERENCIA:** no espejado.
- **Rotación necesaria si se quiere una vista lateral/3-4 (SOLO DOCUMENTAR, NO
  IMPLEMENTAR):** permutar ejes en CPU para que pantalla-X ← longitud (e_y),
  pantalla-Y ← altura (e_z), profundidad ← envergadura (e_x), con signos de nariz a
  ajustar — equivalente a aplicar parcialmente la convención Rmap `(ex, ez, −ey)`
  dentro de la transformación CPU. Milestone posterior.

## R6. Bounds post-transform (FASE 3 — cálculo)

Con los bounds inferidos de la evidencia (precisión ±0.0125; valores definitivos en
`XWING_REFERENCED_BOUNDS_OK`): center=(0.025, −0.006, −0.094),
max_extent=12.9625 (Y), scale=1.6/12.9625=0.123433.

| Eje clip | Rango | Fracción NDC | ¿Dentro del volumen? |
|---|---|---|---|
| X | [−0.677, +0.677] | 67.7% del ancho | Sí (⊂[−1,1]) |
| Y | [−0.800, +0.800] | 80% de la altura | Sí (⊂[−1,1]) |
| Z | [−0.197, +0.197] | — | Sí, vía depth-clamp (ver R7) |
| W | =1 en todos los vértices | — | Sin perspectiva: x/w = x |

- **DEMONSTRADO:** `scale = 1.6/max_extent` garantiza que el eje dominante quede en
  ±0.8 ⇒ X/Y SIEMPRE dentro de pantalla con 20% de margen (por diseño del 1.6).
- **DEMONSTRADO que Z NO se recorta**, a pesar de que Vulkan usa rango [0,1]:
  el rasterizer state queda en 0 (memset) ⇒ `enable_depth_clip = false` ⇒
  `depthClampEnable = !enable_depth_clip = true` (SDL_gpu_vulkan.c:6532) y el feature
  `depthClamp` del device se habilita **por defecto** (SDL_gpu_vulkan.c:12851,
  propiedad default `true`). Los fragmentos con z<0 se **recortan a 0 (clamp)**,
  no se descartan ⇒ el 100% del modelo se rasteriza. Riesgo residual: solo si el
  device SDL se creó con esa propiedad desactivada (el código actual no lo hace).

## R7. Clip / depth / culling / viewport (FASE 3)

| Aspecto | Estado actual | Evidencia |
|---|---|---|
| Interpretación x,y,z,w | `float4(pos,1)`: x,y,z directos a NDC, w=1 | shaders_v9/minimal.vert.hlsl:5-8 — **DEMONSTRADO** |
| Depth test | OFF (sin depth attachment; enable_depth_test=0 por memset) | vr_stereo.c:798-799 (solo 1 color target) — **DEMONSTRADO** |
| Depth write | OFF (write requiere test) | SDL_gpu.h:1911 — **DEMONSTRADO** |
| Culling | `SDL_GPU_CULLMODE_NONE` → winding irrelevante | vr_stereo.c:795 — **DEMONSTRADO** |
| Viewport/scissor | No seteados → default = framebuffer completo (min_depth 0, max_depth 1) | SDL_gpu_vulkan.c:8118-8129 + validación empírica del triángulo — **DEMONSTRADO** |
| Clipping | Solo NDC [−1,1] en x/y (dentro) y z con clamp [0,1] | R6 — **DEMONSTRADO** |
| Topology | TRIANGLELIST, 852 índices = 284 tris | vr_stereo.c:787 — **DEMONSTRADO** |
| Z range esperado | [−0.197, +0.197] → clampado a [0,1] | R6 — **DEMONSTRADO** |

## R8. Apariencia visual esperada (FASE 4)

- **DEMONSTRADO — color:** `minimal.frag.hlsl:1-3` → `return float4(1,0,1,1)` =
  **MAGENTA SÓLIDO OPAQUE**. Sin iluminación, sin textura, sin wireframe, sin
  silueta: masa magenta rellena.
- **DEMONSTRADO — blending:** `SDL_GPUBlendState.enable_blend = 0` (memset;
  SDL_gpu.h:1744) ⇒ sobrescribe opaco lo que cubre.
- **DEMONSTRADO — no hay clear que lo borre:** `Aeron_BeginRenderPass` sin
  `clear_color` ⇒ `LOADOP_LOAD` (render_backend.c:3752) — el contenido previo del
  present RT se conserva y el magenta se pinta ENCIMA.
- **DEMONSTRADO — orden de frame:** RenderEye (escena Aeron + tonemap escribe el
  present) → **DrawMesh** → VrBlit_Copy (copia, no clear) → xrEndFrame. Nada dibuja
  después en el present. Sin depth test que pueda ocultarlo; ni tonemap, ni blit, ni
  otro render pass posterior lo tapan.
- **DEMONSTRADO (no ocultable):** depth buffer ❌, clear posterior ❌, tonemap ❌
  (ocurre antes), VrBlit_Copy ❌ (copia byte a byte), render pass posterior ❌.
- **Otra geometría Aeron:** el magenta va DESPUÉS y encima donde cubre; si la ruta
  PBR de escena pinta el X-Wing texturizado, este se verá **alrededor/dentro de la
  silueta no cubierta** → dos copias posibles (R1/R2 de la tabla anterior).
- **INFERENCIA (resultado esperado para el usuario):** X-Wing magenta sólido,
  **vista cenital**, centrado en pantalla, **fijo respecto a la cabeza** (head-locked:
  no hay matriz de vista) y **plano** (MISMO NDC en ambos ojos ⇒ disparidad cero ⇒
  sin paralaje, percibido "pegado al infinito"). Es lo esperable sin MVP.
- **DESCONOCIDO HASTA RUNTIME:** si el barco PBR de Aeron también aparece y dónde.

## R9. Auditoría de `SDL_DrawGPUIndexedPrimitives` (FASE 5)

Firma local real (SDL_gpu.h:3905-3911):
`(render_pass, num_indices, num_instances, first_index, vertex_offset(Sint32), first_instance)`.

Nuestra llamada (vr_stereo.c:640):
`SDL_DrawGPUIndexedPrimitives(dp->render_pass, s_v10_mesh_index_count, 1, 0, 0, 0)`

| Argumento | Valor | Significado | ¿Correcto? |
|---|---|---|---|
| num_indices | 852 | índices por instancia → dibuja [0,852) = buffer completo | ✓ **DEMONSTRADO** |
| num_instances | 1 | una instancia | ✓ **DEMONSTRADO** |
| first_index | 0 | primer índice del buffer | ✓ **DEMONSTRADO** |
| vertex_offset | 0 | suma a cada índice antes de indexar el VBO | ✓ **DEMONSTRADO** — los índices YA están sesgados por primitiva en el cook (gltf_mesh.c:365-366); cualquier offset≠0 los desplazaría |
| first_instance | 0 | ID de primera instancia | ✓ **DEMONSTRADO** |

**Cantidad de índices vs valor máximo representable — distinción clave:**
- `index_count` = CUÁNTOS índices (puede superar 65535 con IBO uint16; `num_indices`
  es Uint32). Nuestro 852 está muy por debajo de cualquier límite.
- El límite real del uint16 es el **VALOR** de cada índice ≤ 65535, es decir
  `vertex_count ≤ 65536`. El cook ya aborta si `total_v > 0xFFFF` (gltf_mesh.c:463-467)
  y clamparía índices a 0xFFFF (inaccesible aquí). Nuestro caso: valores ≤ 10379,
  audit `bad_idx=0` ⇒ **DEMONSTRADO seguro hoy y protegido en el cook para el futuro**.

## R10. Riesgos ordenados por probabilidad técnica (NO por gravedad)

1. **Orientación percibida como "incorrecta" (vista cenital, fuselaje vertical)**
   — prob. MUY ALTA (consecuencia aritmética directa de no rotar, R5). No es fallo;
   produce el caso F del protocolo. Fix documentado, no implementado.
2. **Doble X-Wing (magenta cenital + barco PBR de Aeron si la ruta de escena lo
   pinta)** — prob. ALTA (R1/R2 previos ya lo anticipan). Confusión visual, no rompe
   el draw V10.
3. **Sin estéreo/paralaje (plano, head-locked)** — prob. ALTA por diseño (mismo NDC
   ambos ojos). Esperado en esta fase; posible molestia de fusión visual.
4. **Mitad con z<0 ausente (caso G)** — prob. BAJA: clamp demostrado por defecto
   (R6/R7); solo si el device SDL se creó sin depth-clamping (el código actual no).
5. **Buffers no creados → fallback triángulo (casos A/B)** — prob. BAJA: 124.6 KB +
   1.7 KB; la hipótesis P9 (tamaño de IBO 1704 B no múltiplo de 64) sigue siendo la
   única causa técnica posible y ya degrada al triángulo sin matar la app.
6. **`vertex_count` inflado 10380 (≈19× lo referenciado)** — prob. 100% de existir,
   impacto visual 0; solo 108 KB extra de VBO. No dibuja de más ni de menos.
7. **Discrepancia 401 (sondeo raw) vs 284 (cocinado)** — prob. cierta, impacto
   documental; lo cocinado es lo que se dibuja (R3).
8. **Uploads sin fence (P10)** — aceptado, idéntico a la baseline físicamente
   validada.

## R11. Protocolo de la primera prueba (sin ejecutar nada)

**Precondición del skill (ADB):** antes de CUALQUIER comando adb, preguntar literalmente:
*"¿Ya encendiste el Quest 3S, lo conectaste a la PC y autorizaste la depuración USB?
Confírmame cuando esté listo para continuar."* — esperar confirmación.

**Secuencia:** instalar APK nuevo → lanzar → capturar logcat → filtrar por
`XWING_` y `VRPROBE`. Clasificación con logs YA EXISTENTES:

| Caso | Señal exacta (archivo:línea) |
|---|---|
| **A. buffers no creados** | Existen `[flight_gltf] FLIGHTMODELS/xwing.OPT: ...` (gltf_mesh.c:597) y `VRPROBE cooked xwing mesh=%p` (vr_main:390) pero **NO** aparece `XWING_MESH_CPU_READY` (vr_stereo.c:406); aparece algún `VRPROBE XWING … FAIL/invalid/empty/malloc/create failed` de init (vr_stereo.c:470/485/499/504/515/520/529/542/547/556/561/570) |
| **B. fallback al triángulo** | `VRPROBE XWING mesh buffers FAILED - fallback to V10 triangle diagnostic` (vr_main.c:440, init) y/o `VRPROBE XWING mesh draw inactive - fallback ACTIVE (V10 triangle)` (vr_main.c:755, **una sola vez**) + `VRPROBE triangle-draw ok` (vr_stereo.c:381, **cada frame**) |
| **C. X-Wing draw registrado** | `XWING_DRAW_RECORDED: tris=284` (vr_stereo.c:644, **una sola vez**) + **ausencia** de `VRPROBE triangle-draw ok` (mutua exclusión vr_main.c:751-762) + cadena previa completa: `XWING_MESH_CPU_READY` (406) → `XWING_REFERENCED_BOUNDS_OK` (444) → `XWING_VBO_CREATED` (474) → `XWING_IBO_CREATED` (489) → `XWING_VBO_UPLOAD_SUBMITTED` (533) / `XWING_IBO_UPLOAD_SUBMITTED` (574) / `XWING_UPLOAD_SUBMITTED` (575) |
| **D. frame enviado** | `VRPROBE frame %u eye%d blit eye%d end` (vr_main.c:770) + `VRPROBE xrEndFrame end … result=0` (vr_openxr.c:454) + `VRPROBE render eye%d end ok` (vr_stereo.c:281) |
| **E. geometría visible** | **Solo observación humana** — no hay marcador automático. Esperar: magenta sólido, cenital, centrado, sin paralaje |
| **F. visible pero orientación incorrecta** | E positivo + coincide con la predicción cenital de R5 → documentar; fix = rotación (R5), milestone posterior |
| **G. geometría parcial** | Observación humana; correlacionar con `XWING_REFERENCED_BOUNDS_OK` (bounds CPU completos ⇒ si falta mitad en pantalla, sospechar clamp z no activo o clear — ninguno debería ocurrir) |
| **H. pantalla vacía** | D presente + C presente + E negativo ⇒ pipeline/formato (muy improbable: mismo pipeline que el triángulo validado). Si C **y** B ausentes ⇒ init roto antes (revisar `VRPROBE FATAL`, vr_main.c:237/327/384/421/447/469) |

**Marcadores FALTANTES (solo documentar, NO implementar):**
- **M1:** log del rango Z clip post-transform (min/max) — confirmaría la relevancia
  del clamp de profundidad en runtime.
- **M2:** lectura automática del **present** RT (readback/NDC-sampling sobre
  `present_tex`) — el mecanismo W3 actual muestrea `scene_tex`, no el present;
  cerraría E/G/H sin depender de observación humana.

## R12. Marcadores de log existentes que debemos capturar

| Grupo | Marcador | Ubicación | Frecuencia |
|---|---|---|---|
| Cocción | `[flight_gltf] FLIGHTMODELS/xwing.OPT: 10380 verts, 852 indices (852 opaque, 0 mask, 0 blend), 90 prims, 39 materials, 4 variants` | gltf_mesh.c:597 (log device real ya en evidence) | init |
| Carga | `VRPROBE cooked xwing mesh=%p` | vr_main.c:390 | init |
| CPU ready | `XWING_MESH_CPU_READY: verts=10380 idx=852 tris=284` | vr_stereo.c:406 | init |
| Bounds | `XWING_REFERENCED_BOUNDS_OK: min=… max=… center=… scale=…` | vr_stereo.c:444 | init |
| VBO | `XWING_VBO_CREATED` / `XWING_VBO_UPLOAD_SUBMITTED` | vr_stereo.c:474/533 | init |
| IBO | `XWING_IBO_CREATED` / `XWING_IBO_UPLOAD_SUBMITTED` / `XWING_UPLOAD_SUBMITTED` | vr_stereo.c:489/574/575 | init |
| Draw | `XWING_DRAW_RECORDED: tris=284` | vr_stereo.c:644 | **una vez** |
| Fallback | `VRPROBE XWING mesh buffers FAILED - fallback to V10 triangle diagnostic` | vr_main.c:440 | init |
| Fallback | `VRPROBE XWING mesh draw inactive - fallback ACTIVE (V10 triangle)` | vr_main.c:755 | **una vez** |
| Triángulo | `VRPROBE triangle-draw ok` | vr_stereo.c:381 | **cada frame** (su PRESENCIA = fallback activo; su AUSENCIA con C✓ = rama X-Wing activa) |
| Draw fallido | `VRPROBE XWING draw: color target has no present RT owner` / `… render pass creation failed` | vr_stereo.c:618/628 | por ocurrencia |
| Frame | `VRPROBE render eye%d end ok` / `VRPROBE frame %u blit eye%d end` | vr_stereo.c:281 / vr_main.c:770 | por frame/eye |
| Frame XR | `VRPROBE xrEndFrame end dt=… result=0` | vr_openxr.c:454 | por frame |

**Regla de decisión rápida:** C✓ + D✓ ⇒ geometría registrada y frame enviado;
entonces E es observación humana. B✓ (sin C) ⇒ fallback intacto. A ⇒ inspeccionar
el texto exacto del `VRPROBE XWING …` de init para la causa.

---

## Cierre del milestone — FIRST REAL X-WING VISUALLY CONFIRMED ON QUEST 3S (2026-09-24)

**Milestone CERRADO.** Evidencia de la prueba física controlada del 2026-09-24:

- **APK:** `C:\OpenXWA\XWAQuest\vr-probe\app\build\outputs\apk\debug\app-debug.apk`
  — SHA-256 `D41B7AAAE0F2EAA6F149AE64927C15661E187BB0125C270F021F6824C55E8937`
  — package `org.openxwa.xwaquest.vrprobe` (arm64-v8a, libOpenXWAVRP.so
  verificada byte a byte contra `vr-probe/build-android` tras strip).
- **Log:** `C:\OpenXWA\XWAQuest\vr-probe\evidence\xwing-v10-run-20260924-222207.log`
  (1 510 976 bytes, PID 8072, 24/09/2026 22:21).
- **Runtime real (de esta ejecución):** `vertex_count=10380`, `index_count=852`,
  `triangle_count=284`; bounds `min=(-5.467,-6.480,-1.695)`
  `max=(5.508,6.480,1.503)`; `center=(0.021,0.000,-0.096)`, `scale=0.123456`.
- **DEMONSTRADO en runtime:** `MESH_CPU_READY`, `BOUNDS_OK`, `VBO_CREATED`
  (124 560 B), `IBO_CREATED` (1704 B), `UPLOAD_SUBMITTED`, `DRAW_RECORDED`
  (tris=284, una vez), `30/30 xrEndFrame result=0`, `stereo=30`, `submitted=30`.
  **Sin fallback al triángulo** (`triangle-draw ok`=0, `mesh draw inactive`=0).
- **Terminación NORMAL:** `VRPROBE loop end stereo=30 submitted=30
  reason=auto-exit submitted cap quit=0` + `VRPROBE exit code=0`.
  El cierre observado fue el auto-exit diagnóstico de 30 frames — **NO es un crash**.
- **VISUAL_CONFIRMED_BY_USER = DEMOSTRADO:** el usuario confirmó físicamente
  en el Quest 3S haber visto **UN X-WING MAGENTA RECONOCIBLE**.
- **Resolución de R10-1 / R10-2:** el riesgo de "orientación incorrecta" se
  resolvió como *reconocible* para el usuario; la coexistencia con la escena
  Aeron no fue reportada como problema. Resto de R10 permanece como estaba.

Siguiente documento: `research/CODEX_HANDOFF_M8.md` (handoff para M8).
