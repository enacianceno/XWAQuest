/* VR-PROBE dual-eye renderer (isolated test code).
 * Each eye owns an AeronScene3D so Begin/Render histories never mix.
 * The mesh is the real cooked OPT mirror from XwaRemasterShip; only the
 * per-eye camera differs, which is what makes the stereoscopy real. */
#include "vr_stereo.h"
#include "vr_log.h"
#include "vr_flat_shaders.h"
#include "internal.h"
#include "vr_openxr.h"
#include "vr_convert.h"

#include "aeron/scene/scene3d.h"
#include "aeron/scene/mesh.h"
#include "xwa_remaster/ship.h"
#include "xwa_runtime/snapshot/snapshot.h"

#include <string.h>
#include <math.h>

static AeronScene3D *s_scene[2] = { NULL, NULL };
static AeronRenderTarget *s_present[2] = { NULL, NULL };
static AeronScenePresentChain *s_chain = NULL;
static AeronSampler *s_sampler = NULL;
static int s_w, s_h;
static int s_rw, s_rh;
/* v9.7: Flat-color diagnostic pipeline for geometry visibility test.
 * Draws the real X-Wing mesh with a constant color (magenta=eye0, cyan=eye1)
 * to verify that the 284 triangles produce visible fragments. */
static SDL_GPUShader *s_flat_vs;
static SDL_GPUShader *s_flat_fs;
static SDL_GPUGraphicsPipeline *s_flat_pipe;
/* V10: Triangle drawing pipeline (shadercross VS+FS, vertex input, format 29) */
static SDL_GPUShader *s_v10_vs;
static SDL_GPUShader *s_v10_fs;
static SDL_GPUGraphicsPipeline *s_v10_pipe;
static SDL_GPUBuffer *s_v10_vbuf;
/* X-Wing mesh buffers (independent V10 VBO/IBO for real geometry) */
static SDL_GPUBuffer *s_v10_mesh_vbuf = NULL;
static SDL_GPUBuffer *s_v10_mesh_ibuf = NULL;
static uint32_t s_v10_mesh_index_count = 0;
static int s_v10_mesh_ready = 0;
/* XWING_DRAW_RECORDED is logged once (first successful mesh draw), not per frame. */
static int s_v10_mesh_draw_logged = 0;
/* UBO for the flat-color pipeline: view_proj (64) + model (64) + color (16) = 144 bytes */
typedef struct FlatColorUBO {
    float view_proj[16];
    float model[16];
    float color[4];
} FlatColorUBO;

/* v9.7: Global flag to enable/disable flat-color draw in VrStereo_RenderEye */
int g_flatDrawEnabled = 0;
/* Stage 2 (diagnostic): render at half eye resolution to test whether frame
 * cost is pixel-bound. Presentation stays correct: the blit upscales the
 * smaller present RT onto the full XR image. Restore path: set back to 1. */
#define VRSTEREO_SCALE_DIV 2
/* Diagnostic solid-color frames remaining (eye0=red, eye1=green), drawn
 * into the SAME present RTs that feed the XR swapchain blits. Temporary:
 * proves the compositor shows our images even if the ship is misframed. */
#define VRPROBE_DIAG_FRAMES 10
static int s_diagFrames;

void VrStereo_SetDiagFrames(int n) {
    s_diagFrames = n;
    VrLog("VRPROBE diag frames set=%d (eye0=red eye1=green first)", n);
}

static void build_model_matrix(float out[16], float scale, float yaw, const float pos[3]) {
    /* Rmap rows (OPT->XR): xr=(ex, ez, -ey). Then yaw about XR up: R = Ry*Rmap. */
    const float c = cosf(yaw), s = sinf(yaw);
    const float r00 = c, r01 = -s, r02 = 0.0f;
    const float r10 = 0.0f, r11 = 0.0f, r12 = 1.0f;
    const float r20 = -s, r21 = -c, r22 = 0.0f;
    out[0] = scale * r00;
    out[1] = scale * r01;
    out[2] = scale * r02;
    out[3] = pos[0];
    out[4] = scale * r10;
    out[5] = scale * r11;
    out[6] = scale * r12;
    out[7] = pos[1];
    out[8] = scale * r20;
    out[9] = scale * r21;
    out[10] = scale * r22;
    out[11] = pos[2];
    out[12] = 0.0f;
    out[13] = 0.0f;
    out[14] = 0.0f;
    out[15] = 1.0f;
}

static void camera_from_xr(AeronSceneCamera *cam, const XrPosef *pose, const XrFovf *fov,
                           int w, int h) {
    /* Orientation/position convention handled by VrConvert_Camera
     * (D-conversion, runtime self-tested). FOV stays per-eye symmetric. */
    float tanL = tanf(fov->angleLeft), tanR = tanf(fov->angleRight);
    float tanU = tanf(fov->angleUp), tanD = tanf(fov->angleDown);
    VrConvert_Camera(pose, atanf((tanR - tanL) * 0.5f), atanf((tanU - tanD) * 0.5f),
                     0.05f, w, h, cam);
}

int VrStereo_Init(int w, int h) {
    if (s_scene[0]) {
        return 1;
    }
    /* Full presentation size is kept for the blit; internal render size is
     * divided (Stage 2). s_w/s_h stay at presentation size for the chain. */
    const int rw = w / VRSTEREO_SCALE_DIV > 0 ? w / VRSTEREO_SCALE_DIV : w;
    const int rh = h / VRSTEREO_SCALE_DIV > 0 ? h / VRSTEREO_SCALE_DIV : h;
    VrLog("VRPROBE stereo render %dx%d presentation %dx%d (div=%d)", rw, rh, w, h,
          VRSTEREO_SCALE_DIV);
    s_w = w;
    s_h = h;
    s_rw = rw;
    s_rh = rh;
    for (int i = 0; i < 2; i++) {
        VrLog("VRPROBE init[%d] AeronScene_Create begin %dx%d fmt=%d", i, rw, rh,
              (int)AERON_TEXTURE_FORMAT_RGBA16_FLOAT);
        s_scene[i] = AeronScene_Create(&(AeronScene3DDesc) {
            .rt_width = rw,
            .rt_height = rh,
            .color_format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
            .with_normal_rt = 0,
            .sample_count = 0,
            .temporal_mode = 0,
            .view_space_to_meters = 1.0f,
        });
        if (!s_scene[i]) {
            VrLog("VRPROBE init[%d] FAIL: AeronScene_Create returned NULL", i);
            SDL_Log("VRPROBE stereo scene %d create failed", i);
            return 0;
        }
        VrLog("VRPROBE init[%d] AeronScene_Create ok scene=%p", i, (void *)s_scene[i]);
        AeronScene_SetClearColor(s_scene[i], (const float[4]){ 0.01f, 0.015f, 0.03f, 1.0f });
        VrLog("VRPROBE init[%d] Aeron_CreateRenderTarget begin %dx%d fmt=%d", i, rw, rh,
              (int)AERON_TEXTURE_FORMAT_RGBA16_FLOAT);
        s_present[i] = Aeron_CreateRenderTarget(&(AeronRenderTargetDesc) {
            .width = rw,
            .height = rh,
            .format = AERON_TEXTURE_FORMAT_RGBA16_FLOAT,
            .debug_name = "vrprobe.present",
        });
        if (!s_present[i]) {
            VrLog("VRPROBE init[%d] FAIL: Aeron_CreateRenderTarget returned NULL", i);
            SDL_Log("VRPROBE stereo present RT %d failed", i);
            return 0;
        }
        VrLog("VRPROBE init[%d] Aeron_CreateRenderTarget ok rt=%p", i, (void *)s_present[i]);
    }
    VrLog("VRPROBE init AeronScenePresentChain_Create begin fmt=%d",
          (int)AERON_TEXTURE_FORMAT_RGBA16_FLOAT);
    s_chain = AeronScenePresentChain_Create(AERON_TEXTURE_FORMAT_RGBA16_FLOAT);
    VrLog("VRPROBE init AeronScenePresentChain_Create end chain=%p", (void *)s_chain);
    VrLog("VRPROBE init Aeron_CreateSampler begin");
    s_sampler = Aeron_CreateSampler(&(AeronSamplerDesc) {
        .min_filter = AERON_FILTER_LINEAR,
        .mag_filter = AERON_FILTER_LINEAR,
        .address_u = AERON_ADDRESS_CLAMP_TO_EDGE,
        .address_v = AERON_ADDRESS_CLAMP_TO_EDGE,
    });
    VrLog("VRPROBE init Aeron_CreateSampler end sampler=%p", (void *)s_sampler);
    if (!s_chain || !s_sampler) {
        VrLog("VRPROBE init FAIL: chain=%p sampler=%p (one is NULL)", (void *)s_chain,
              (void *)s_sampler);
        SDL_Log("VRPROBE stereo present chain failed");
        return 0;
    }
    /* Flat pipeline + diagnostic tests v4-v10 REMOVED:
     * Embedded SPIR-V shaders cause vkCreateGraphicsPipelines to hang on Meta Quest.
     * V10 pipeline (shadercross) works and is used for triangle draw. */

    /* DIAGNOSTIC TEST 10: Create and store the V10 pipeline (shadercross VS + FS with vertex input)
     * for actual triangle drawing. This pipeline WORKS (proven in V9).
     * We store it for actual triangle drawing in V10. */
    SDL_GPUDevice *dev = g_aeron.gpu_device;
    SDL_Log("VRPROBE TRACE: Calling VrStereo_InitV10Pipeline");
    if (!VrStereo_InitV10Pipeline(dev)) {
        SDL_Log("VRPROBE DIAG v10 pipeline creation failed");
        return 0;
    }
    SDL_Log("VRPROBE TRACE: VrStereo_InitV10Pipeline returned OK");
    SDL_Log("VRPROBE init VrStereo_Init OK %dx%d", w, h);
    return 1;
}

AeronTexture *VrStereo_RenderEye(AeronCommandBuffer *cmd, int eye,
                                 const AeronSceneCamera *cam,
                                 const AeronSceneMesh *mesh,
                                 const float model_matrix[16],
                                 const float light_dir[3],
                                 const float light_color[3],
                                 float light_intensity,
                                 float emissive) {
    AeronScene3D *scene = s_scene[eye];
    if (!scene || !mesh || !cam) {
        return NULL;
    }
    VrLog("VRPROBE render eye%d begin mesh=%p campos=(%.3f,%.3f,%.3f) emissive=%.2f", eye,
          (void *)mesh, cam->pos[0], cam->pos[1], cam->pos[2], emissive);
    if (s_diagFrames > 0) {
        /* Diagnostic solid color straight into the presented RT. */
        static const float diag[2][4] = { { 1.0f, 0.0f, 0.0f, 1.0f },
                                          { 0.0f, 1.0f, 0.0f, 1.0f } };
        s_diagFrames--;
        AeronRenderPass *dp = Aeron_BeginRenderPass(&(AeronRenderPassDesc) {
            .color_target = s_present[eye],
            .clear_color = 1,
            .clear_color_rgba = { diag[eye][0], diag[eye][1], diag[eye][2], diag[eye][3] },
            .command_buffer = cmd,
            .debug_label = "VRPROBE diag solid",
        });
        if (!dp) {
            return NULL;
        }
        Aeron_EndRenderPass(dp);
        VrLog("VRPROBE render eye%d DIAG solid remaining=%d", eye, s_diagFrames);
        return Aeron_RenderTargetGetTexture(s_present[eye]);
    }
    if (!AeronScene_Begin(scene, cam)) {
        Aeron_CommandBufferSetFailure(cmd, "VRPROBE scene begin failed");
        return NULL;
    }
    /* World-space studio light (surface->light). No AO, no point tuning. */
    XwaDirLight light;
    memset(&light, 0, sizeof light);
    light.world_dir[0] = light_dir[0];
    light.world_dir[1] = light_dir[1];
    light.world_dir[2] = light_dir[2];
    light.intensity = light_intensity;
    light.color[0] = light_color[0];
    light.color[1] = light_color[1];
    light.color[2] = light_color[2];
    XwaRemasterShip_SetPbrEnv(scene, &light, 1, NULL, cam->pos, NULL, NULL, NULL, NULL);

    AeronSceneMeshInstance inst;
    memset(&inst, 0, sizeof inst);
    inst.mesh = mesh;
    memcpy(inst.transform, model_matrix, sizeof inst.transform);
    memcpy(inst.prev_transform, model_matrix, sizeof inst.transform);
    inst.no_local_lights = 1;
    inst.zero_velocity = 1;
    /* Stage 4 (temporary): >0 renders sampled base color as unlit emission,
     * isolating geometry/culling/framing from lighting/materials. */
    inst.base_color_emissive_strength = emissive;
    /* Cooked glbs are CCW-front (same convention as preview.c). */
    inst.cull_mode = AERON_CULL_BACK;
    AeronScene_AddMeshInstance(scene, &inst);

    if (!AeronScene_Render(scene, cmd)) {
        VrLog("VRPROBE render eye%d SCENE failed", eye);
        return NULL;
    }
    /* v10: X-Wing mesh draw happens in vr_main.c AFTER VrStereo_RenderEye
     * returns (post-tonemap, pre-blit) so the tonemap clear cannot erase it.
     * See VrStereo_DrawMesh / VrStereo_DrawTriangle call site. */
    /* v9.7: Flat-color diagnostic draw (if enabled).
     * Draws the real mesh with magenta/cyan on top of the scene RT
     * AFTER AeronScene_Render but BEFORE the present pass. */
    if (g_flatDrawEnabled) {
        VrStereo_DrawFlatDiagnostic(cmd, eye, cam, mesh, model_matrix);
    }
    AeronTexture *scene_tex = Aeron_RenderTargetGetTexture(AeronScene_SceneRt(scene));
    if (!scene_tex) {
        Aeron_CommandBufferSetFailure(cmd, "VRPROBE scene produced no output");
        return NULL;
    }
    AeronRenderPass *pp = Aeron_BeginRenderPass(&(AeronRenderPassDesc) {
        .color_target = s_present[eye],
        .clear_color = 1,
        .clear_color_rgba = { 0.01f, 0.015f, 0.03f, 1.0f },
        .command_buffer = cmd,
        .debug_label = "VRPROBE tonemap",
    });
    if (!pp) {
        return NULL;
    }
    static const float tint[4] = { 1.0f, 1.0f, 1.0f, 1.0f };
    AeronScenePresentChain_Draw(s_chain, pp, scene_tex, s_sampler, NULL, 0.0f,
                                s_rw, s_rh, 1.0f, tint, 0);
    Aeron_EndRenderPass(pp);
    VrLog("VRPROBE render eye%d end ok", eye);
    return Aeron_RenderTargetGetTexture(s_present[eye]);
}

AeronTexture *VrStereo_GetSceneTexture(int eye) {
    if (eye < 0 || eye >= 2 || !s_scene[eye]) {
        return NULL;
    }
    return Aeron_RenderTargetGetTexture(AeronScene_SceneRt(s_scene[eye]));
}

void VrStereo_DrawFlatDiagnostic(AeronCommandBuffer *cmd, int eye,
                                 const AeronSceneCamera *cam,
                                 const AeronSceneMesh *mesh,
                                 const float model_matrix[16]) {
    if (!s_flat_pipe || !s_scene[eye] || !mesh || !mesh->vbo || !mesh->ibo ||
        mesh->index_count == 0 || !cmd) {
        VrLog("VRPROBE flat-draw: SKIP pipe=%p scene=%p mesh=%p vbo=%p ibo=%p idx=%u cmd=%p",
              (void*)s_flat_pipe, (void*)s_scene[eye], (void*)mesh,
              mesh ? (void*)mesh->vbo : NULL, mesh ? (void*)mesh->ibo : NULL,
              mesh ? mesh->index_count : 0, (void*)cmd);
        return;
    }
    AeronRenderTarget *scene_rt = AeronScene_SceneRt(s_scene[eye]);
    if (!scene_rt) {
        VrLog("VRPROBE flat-draw: scene_rt NULL");
        return;
    }
    /* Compute view-projection matrix from the camera (same as AeronScene_Begin) */
    float view_proj[16];
    AeronScene_ComputeViewProj(cam, view_proj);
    /* Prepare UBO */
    FlatColorUBO ubo;
    memcpy(ubo.view_proj, view_proj, sizeof(ubo.view_proj));
    memcpy(ubo.model, model_matrix, sizeof(ubo.model));
    /* Magenta for eye 0, cyan for eye 1 */
    static const float flat_colors[2][4] = {
        { 1.0f, 0.0f, 1.0f, 1.0f },  /* magenta */
        { 0.0f, 1.0f, 1.0f, 1.0f }   /* cyan */
    };
    memcpy(ubo.color, flat_colors[eye & 1], sizeof(ubo.color));
    /* Open render pass on the scene color RT (no clear, no depth target) */
    AeronRenderPass *dp = Aeron_BeginRenderPass(&(AeronRenderPassDesc) {
        .color_target  = scene_rt,
        .command_buffer = cmd,
        .debug_label   = "VRPROBE flat-color diagnostic",
    });
    if (!dp) {
        VrLog("VRPROBE flat-draw: render pass creation failed");
        return;
    }
    /* Bind SDL pipeline directly to the underlying SDL render pass.
     * s_flat_pipe is SDL_GPUGraphicsPipeline*, not AeronGraphicsPipeline*. */
    SDL_BindGPUGraphicsPipeline(dp->render_pass, s_flat_pipe);
    Aeron_BindUniformData(dp, AERON_SHADER_STAGE_VERTEX, 0, &ubo, sizeof(ubo));
    Aeron_BindVertexBuffer(dp, 0, mesh->vbo, 0);
    Aeron_BindIndexBuffer(dp, mesh->ibo, AERON_INDEX_FORMAT_UINT16, 0);
    Aeron_DrawIndexed(dp, mesh->index_count, 0, 0);
    Aeron_EndRenderPass(dp);
    VrLog("VRPROBE flat-draw eye%d ok idx=%u color=(%.1f,%.1f,%.1f,%.1f)",
          eye, mesh->index_count, ubo.color[0], ubo.color[1], ubo.color[2], ubo.color[3]);
}

/* v10: Draw a triangle using the V10 pipeline (shadercross VS+FS with vertex input)
 * into the given render target. Returns 1 on success, 0 on failure. */
int VrStereo_DrawTriangle(AeronCommandBuffer *cmd, AeronTexture *color_target) {
    if (!s_v10_pipe || !s_v10_vbuf || !cmd || !color_target) {
        VrLog("VRPROBE triangle-draw: SKIP pipe=%p vbuf=%p cmd=%p target=%p",
              (void*)s_v10_pipe, (void*)s_v10_vbuf, (void*)cmd, (void*)color_target);
        return 0;
    }
    AeronRenderTarget *present_target = NULL;
    for (int i = 0; i < 2; i++) {
        if (s_present[i] && Aeron_RenderTargetGetTexture(s_present[i]) == color_target) {
            present_target = s_present[i];
            break;
        }
    }
    if (!present_target) {
        VrLog("VRPROBE triangle-draw: color target has no present RT owner");
        return 0;
    }
    /* Open render pass on the given color target (no clear, no depth target) */
    AeronRenderPass *dp = Aeron_BeginRenderPass(&(AeronRenderPassDesc) {
        .color_target  = present_target,
        .command_buffer = cmd,
        .debug_label   = "VRPROBE triangle diagnostic",
    });
    if (!dp) {
        VrLog("VRPROBE triangle-draw: render pass creation failed");
        return 0;
    }
    /* Bind V10 pipeline */
    SDL_BindGPUGraphicsPipeline(dp->render_pass, s_v10_pipe);
    /* Bind vertex buffer */
    SDL_GPUBufferBinding binding = { .buffer = s_v10_vbuf, .offset = 0 };
    SDL_BindGPUVertexBuffers(dp->render_pass, 0, &binding, 1);
    /* Draw 3 vertices (1 triangle) */
    Aeron_Draw(dp, 3, 0);
    Aeron_EndRenderPass(dp);
    VrLog("VRPROBE triangle-draw ok");
    return 1;
}

/* X-Wing mesh: create independent V10 vertex/index buffers from AeronSceneMesh */
int VrStereo_CreateMeshBuffers(AeronSceneMesh *mesh) {
    if (s_v10_mesh_ready) {
        return 1;
    }
    if (!mesh || !mesh->cpu_vertices || !mesh->cpu_indices) {
        VrLog("VRPROBE XWING mesh invalid: mesh=%p cpu_v=%p cpu_i=%p",
              (void*)mesh, mesh ? (void*)mesh->cpu_vertices : NULL,
              mesh ? (void*)mesh->cpu_indices : NULL);
        return 0;
    }
    if (mesh->vertex_count == 0 || mesh->index_count == 0) {
        VrLog("VRPROBE XWING mesh empty: verts=%u idx=%u",
              mesh->vertex_count, mesh->index_count);
        return 0;
    }
    if (mesh->index_count % 3 != 0) {
        VrLog("VRPROBE XWING index_count not multiple of 3: idx=%u",
              mesh->index_count);
        return 0;
    }
    VrLog("XWING_MESH_CPU_READY: verts=%u idx=%u tris=%u",
          mesh->vertex_count, mesh->index_count, mesh->index_count / 3);

    /* Calculate bounds using ONLY indexed vertices */
    float minx = 1e30f, miny = 1e30f, minz = 1e30f;
    float maxx = -1e30f, maxy = -1e30f, maxz = -1e30f;
    for (uint32_t i = 0; i < mesh->index_count; i++) {
        uint16_t idx = mesh->cpu_indices[i];
        if (idx >= mesh->vertex_count) {
            VrLog("VRPROBE XWING INVALID INDEX: idx=%u >= vertex_count=%u at i=%u",
                  idx, mesh->vertex_count, i);
            return 0;
        }
        float x = mesh->cpu_vertices[idx].pos[0];
        float y = mesh->cpu_vertices[idx].pos[1];
        float z = mesh->cpu_vertices[idx].pos[2];
        if (x < minx) minx = x;
        if (x > maxx) maxx = x;
        if (y < miny) miny = y;
        if (y > maxy) maxy = y;
        if (z < minz) minz = z;
        if (z > maxz) maxz = z;
    }
    float cx = (minx + maxx) * 0.5f;
    float cy = (miny + maxy) * 0.5f;
    float cz = (minz + maxz) * 0.5f;
    float ex = maxx - minx;
    float ey = maxy - miny;
    float ez = maxz - minz;
    float max_extent = ex;
    if (ey > max_extent) max_extent = ey;
    if (ez > max_extent) max_extent = ez;
    const float epsilon = 1e-4f;
    if (max_extent <= epsilon) {
        VrLog("VRPROBE XWING max_extent too small: %f", max_extent);
        return 0;
    }
    float scale = 1.6f / max_extent;
    VrLog("XWING_REFERENCED_BOUNDS_OK: min=(%.3f,%.3f,%.3f) max=(%.3f,%.3f,%.3f) "
          "center=(%.3f,%.3f,%.3f) scale=%.6f",
          minx, miny, minz, maxx, maxy, maxz, cx, cy, cz, scale);

    SDL_GPUDevice *dev = g_aeron.gpu_device;

    /* Create VBO: float3 per vertex (stride 12) */
    float *vdata = (float *)SDL_malloc(mesh->vertex_count * 3 * sizeof(float));
    if (!vdata) {
        VrLog("VRPROBE XWING malloc VBO data failed");
        return 0;
    }
    for (uint32_t i = 0; i < mesh->vertex_count; i++) {
        float x = (mesh->cpu_vertices[i].pos[0] - cx) * scale;
        float y = (mesh->cpu_vertices[i].pos[1] - cy) * scale;
        float z = (mesh->cpu_vertices[i].pos[2] - cz) * scale;
        vdata[i * 3 + 0] = x;
        vdata[i * 3 + 1] = y;
        vdata[i * 3 + 2] = z;
    }
    SDL_GPUBufferCreateInfo vbuf_info;
    memset(&vbuf_info, 0, sizeof vbuf_info);
    vbuf_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    vbuf_info.size = mesh->vertex_count * 3 * sizeof(float);
    s_v10_mesh_vbuf = SDL_CreateGPUBuffer(dev, &vbuf_info);
    if (!s_v10_mesh_vbuf) {
        VrLog("VRPROBE XWING VBO create failed: %s", SDL_GetError());
        SDL_free(vdata);
        return 0;
    }
    VrLog("XWING_VBO_CREATED: verts=%u size=%u bytes",
          mesh->vertex_count, mesh->vertex_count * 3 * (int)sizeof(float));

    /* Create IBO: uint16 indices */
    s_v10_mesh_index_count = mesh->index_count;
    SDL_GPUBufferCreateInfo ibuf_info;
    memset(&ibuf_info, 0, sizeof ibuf_info);
    ibuf_info.usage = SDL_GPU_BUFFERUSAGE_INDEX;
    ibuf_info.size = mesh->index_count * sizeof(uint16_t);
    s_v10_mesh_ibuf = SDL_CreateGPUBuffer(dev, &ibuf_info);
    if (!s_v10_mesh_ibuf) {
        VrLog("VRPROBE XWING IBO create failed: %s", SDL_GetError());
        /* vdata is freed by the vbo_fail block below (single free, no double-free). */
        goto vbo_fail;
    }
    VrLog("XWING_IBO_CREATED: idx=%u size=%u bytes",
          mesh->index_count, mesh->index_count * (int)sizeof(uint16_t));

    /* Upload VBO */
    SDL_GPUTransferBufferCreateInfo tvbuf_info;
    memset(&tvbuf_info, 0, sizeof tvbuf_info);
    tvbuf_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tvbuf_info.size = mesh->vertex_count * 3 * sizeof(float);
    SDL_GPUTransferBuffer *tvbuf = SDL_CreateGPUTransferBuffer(dev, &tvbuf_info);
    if (!tvbuf) {
        VrLog("VRPROBE XWING VBO transfer buffer create failed: %s", SDL_GetError());
        goto ibuf_fail;
    }
    void *mapped = SDL_MapGPUTransferBuffer(dev, tvbuf, false);
    if (!mapped) {
        VrLog("VRPROBE XWING VBO map failed");
        /* tvbuf_fail releases tvbuf exactly once (no double-release). */
        goto tvbuf_fail;
    }
    memcpy(mapped, vdata, mesh->vertex_count * 3 * sizeof(float));
    SDL_UnmapGPUTransferBuffer(dev, tvbuf);
    SDL_free(vdata);
    vdata = NULL;

    SDL_GPUCommandBuffer *vcmd = SDL_AcquireGPUCommandBuffer(dev);
    if (!vcmd) {
        VrLog("VRPROBE XWING VBO upload: no cmd");
        goto tvbuf_fail;
    }
    SDL_GPUCopyPass *vcopy = SDL_BeginGPUCopyPass(vcmd);
    if (!vcopy) {
        VrLog("VRPROBE XWING VBO copy pass failed: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(vcmd);
        goto tvbuf_fail;
    }
    SDL_GPUTransferBufferLocation vsrc = { .transfer_buffer = tvbuf, .offset = 0 };
    SDL_GPUBufferRegion vdst = { .buffer = s_v10_mesh_vbuf, .offset = 0, .size = mesh->vertex_count * 3 * sizeof(float) };
    SDL_UploadToGPUBuffer(vcopy, &vsrc, &vdst, 1);
    SDL_EndGPUCopyPass(vcopy);
    if (!SDL_SubmitGPUCommandBuffer(vcmd)) {
        VrLog("VRPROBE XWING VBO submit failed: %s", SDL_GetError());
        goto tvbuf_fail;
    }
    SDL_ReleaseGPUTransferBuffer(dev, tvbuf);
    VrLog("XWING_VBO_UPLOAD_SUBMITTED");

    /* Upload IBO */
    SDL_GPUTransferBufferCreateInfo tibuf_info;
    memset(&tibuf_info, 0, sizeof tibuf_info);
    tibuf_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tibuf_info.size = mesh->index_count * sizeof(uint16_t);
    SDL_GPUTransferBuffer *tibuf = SDL_CreateGPUTransferBuffer(dev, &tibuf_info);
    if (!tibuf) {
        VrLog("VRPROBE XWING IBO transfer buffer create failed: %s", SDL_GetError());
        goto ibuf_fail;
    }
    mapped = SDL_MapGPUTransferBuffer(dev, tibuf, false);
    if (!mapped) {
        VrLog("VRPROBE XWING IBO map failed");
        /* tibuf_fail releases tibuf exactly once (no double-release). */
        goto tibuf_fail;
    }
    memcpy(mapped, mesh->cpu_indices, mesh->index_count * sizeof(uint16_t));
    SDL_UnmapGPUTransferBuffer(dev, tibuf);

    SDL_GPUCommandBuffer *icmd = SDL_AcquireGPUCommandBuffer(dev);
    if (!icmd) {
        VrLog("VRPROBE XWING IBO upload: no cmd");
        goto tibuf_fail;
    }
    SDL_GPUCopyPass *icopy = SDL_BeginGPUCopyPass(icmd);
    if (!icopy) {
        VrLog("VRPROBE XWING IBO copy pass failed: %s", SDL_GetError());
        SDL_CancelGPUCommandBuffer(icmd);
        goto tibuf_fail;
    }
    SDL_GPUTransferBufferLocation isrc = { .transfer_buffer = tibuf, .offset = 0 };
    SDL_GPUBufferRegion idst = { .buffer = s_v10_mesh_ibuf, .offset = 0, .size = mesh->index_count * sizeof(uint16_t) };
    SDL_UploadToGPUBuffer(icopy, &isrc, &idst, 1);
    SDL_EndGPUCopyPass(icopy);
    if (!SDL_SubmitGPUCommandBuffer(icmd)) {
        VrLog("VRPROBE XWING IBO submit failed: %s", SDL_GetError());
        goto tibuf_fail;
    }
    SDL_ReleaseGPUTransferBuffer(dev, tibuf);
    VrLog("XWING_IBO_UPLOAD_SUBMITTED");
    VrLog("XWING_UPLOAD_SUBMITTED");
    s_v10_mesh_ready = 1;
    return 1;

tibuf_fail:
    if (tibuf) SDL_ReleaseGPUTransferBuffer(dev, tibuf);
    /* tvbuf was already released after the successful VBO submit: skip it. */
    goto ibuf_fail;
tvbuf_fail:
    if (tvbuf) SDL_ReleaseGPUTransferBuffer(dev, tvbuf);
    /* fall through */
ibuf_fail:
    if (s_v10_mesh_ibuf) {
        SDL_ReleaseGPUBuffer(dev, s_v10_mesh_ibuf);
        s_v10_mesh_ibuf = NULL;
    }
vbo_fail:
    if (s_v10_mesh_vbuf) {
        SDL_ReleaseGPUBuffer(dev, s_v10_mesh_vbuf);
        s_v10_mesh_vbuf = NULL;
    }
    s_v10_mesh_index_count = 0;
    if (vdata) SDL_free(vdata);
    return 0;
}

/* v10 X-Wing: mirrors VrStereo_DrawTriangle exactly (same target resolution,
 * same Aeron render-pass ownership, same dp->render_pass handle for every
 * SDL_GPU call). Only the binding changes: mesh VBO + mesh IBO + indexed draw. */
int VrStereo_DrawMesh(AeronCommandBuffer *cmd, AeronTexture *color_target) {
    if (!s_v10_mesh_ready || !s_v10_pipe || !s_v10_mesh_vbuf || !s_v10_mesh_ibuf ||
        !cmd || !color_target) {
        /* No per-frame log here: this runs every frame when the mesh is absent. */
        return 0;
    }
    AeronRenderTarget *present_target = NULL;
    for (int i = 0; i < 2; i++) {
        if (s_present[i] && Aeron_RenderTargetGetTexture(s_present[i]) == color_target) {
            present_target = s_present[i];
            break;
        }
    }
    if (!present_target) {
        VrLog("VRPROBE XWING draw: color target has no present RT owner");
        return 0;
    }
    /* Open render pass on the given color target (no clear, no depth target) */
    AeronRenderPass *dp = Aeron_BeginRenderPass(&(AeronRenderPassDesc) {
        .color_target  = present_target,
        .command_buffer = cmd,
        .debug_label   = "VRPROBE xwing mesh V10",
    });
    if (!dp) {
        VrLog("VRPROBE XWING draw: render pass creation failed");
        return 0;
    }
    /* Bind V10 pipeline */
    SDL_BindGPUGraphicsPipeline(dp->render_pass, s_v10_pipe);
    /* Bind mesh vertex buffer (float3, stride 12) */
    SDL_GPUBufferBinding vbinding = { .buffer = s_v10_mesh_vbuf, .offset = 0 };
    SDL_BindGPUVertexBuffers(dp->render_pass, 0, &vbinding, 1);
    /* Bind mesh index buffer (uint16) */
    SDL_GPUBufferBinding ibinding = { .buffer = s_v10_mesh_ibuf, .offset = 0 };
    SDL_BindGPUIndexBuffer(dp->render_pass, &ibinding, SDL_GPU_INDEXELEMENTSIZE_16BIT);
    /* Indexed draw of the whole X-Wing LOD0 */
    SDL_DrawGPUIndexedPrimitives(dp->render_pass, s_v10_mesh_index_count, 1, 0, 0, 0);
    Aeron_EndRenderPass(dp);
    if (!s_v10_mesh_draw_logged) {
        s_v10_mesh_draw_logged = 1;
        VrLog("XWING_DRAW_RECORDED: tris=%u", s_v10_mesh_index_count / 3);
    }
    return 1;
}

void VrStereo_Shutdown(void) {
    for (int i = 0; i < 2; i++) {
        if (s_present[i]) {
            Aeron_DestroyRenderTarget(s_present[i]);
            s_present[i] = NULL;
        }
        if (s_scene[i]) {
            AeronScene_Destroy(s_scene[i]);
            s_scene[i] = NULL;
        }
    }
    if (s_chain) {
        AeronScenePresentChain_Destroy(s_chain);
        s_chain = NULL;
    }
    if (s_sampler) {
        Aeron_DestroySampler(s_sampler);
        s_sampler = NULL;
    }
    /* v9.7: Clean up flat-color diagnostic pipeline (SDL, not Aeron) */
    if (s_flat_pipe) {
        SDL_ReleaseGPUGraphicsPipeline(g_aeron.gpu_device, s_flat_pipe);
        s_flat_pipe = NULL;
    }
    if (s_flat_vs) {
        SDL_ReleaseGPUShader(g_aeron.gpu_device, s_flat_vs);
        s_flat_vs = NULL;
    }
    if (s_flat_fs) {
        SDL_ReleaseGPUShader(g_aeron.gpu_device, s_flat_fs);
        s_flat_fs = NULL;
    }
    /* v10: Clean up triangle pipeline */
    if (s_v10_pipe) {
        SDL_ReleaseGPUGraphicsPipeline(g_aeron.gpu_device, s_v10_pipe);
        s_v10_pipe = NULL;
    }
    if (s_v10_vs) {
        SDL_ReleaseGPUShader(g_aeron.gpu_device, s_v10_vs);
        s_v10_vs = NULL;
    }
    if (s_v10_fs) {
        SDL_ReleaseGPUShader(g_aeron.gpu_device, s_v10_fs);
        s_v10_fs = NULL;
    }
    /* v10: Clean up X-Wing mesh buffers */
    if (s_v10_mesh_vbuf) {
        SDL_ReleaseGPUBuffer(g_aeron.gpu_device, s_v10_mesh_vbuf);
        s_v10_mesh_vbuf = NULL;
    }
    if (s_v10_mesh_ibuf) {
        SDL_ReleaseGPUBuffer(g_aeron.gpu_device, s_v10_mesh_ibuf);
        s_v10_mesh_ibuf = NULL;
    }
    s_v10_mesh_ready = 0;
    s_v10_mesh_index_count = 0;
    s_v10_mesh_draw_logged = 0;
    if (s_v10_vbuf) {
        SDL_ReleaseGPUBuffer(g_aeron.gpu_device, s_v10_vbuf);
        s_v10_vbuf = NULL;
    }
}

/* V10: Initialize the triangle pipeline (shadercross VS+FS with vertex input).
 * Returns 1 on success, 0 on failure. */
int VrStereo_InitV10Pipeline(SDL_GPUDevice *dev) {
    VrLog("VRPROBE DIAG v10 init begin: creating triangle pipeline");
    fflush(g_vrLog);

    /* Load V10 VS: minimal.vert.spv (shadercross-compiled, location 0 vec3 pos, NO UBO) */
    SDL_GPUShader *v10_vs = NULL;
    {
        char vs_path[1024];
        size_t vs_size = 0;
        SDL_snprintf(vs_path, sizeof vs_path, "%s/minimal.vert.spv", g_aeron.shader_root);
        Uint8 *vs_code = (Uint8 *)SDL_LoadFile(vs_path, &vs_size);
        if (vs_code) {
            SDL_GPUShaderCreateInfo sci;
            memset(&sci, 0, sizeof sci);
            sci.code = vs_code;
            sci.code_size = vs_size;
            sci.entrypoint = "main";
            sci.format = SDL_GPU_SHADERFORMAT_SPIRV;
            sci.stage = SDL_GPU_SHADERSTAGE_VERTEX;
            sci.num_samplers = 0;
            sci.num_uniform_buffers = 0;
            sci.num_storage_buffers = 0;
            v10_vs = SDL_CreateGPUShader(dev, &sci);
            SDL_free(vs_code);
        }
    }
    VrLog("VRPROBE DIAG v10 VS load: %s", v10_vs ? "OK" : "FAIL");
    fflush(g_vrLog);

    /* Load V10 FS: minimal.frag.spv (shadercross, hardcoded magenta, NO UBO) */
    SDL_GPUShader *v10_fs = NULL;
    {
        char fs_path[1024];
        size_t fs_size = 0;
        SDL_snprintf(fs_path, sizeof fs_path, "%s/minimal.frag.spv", g_aeron.shader_root);
        Uint8 *fs_code = (Uint8 *)SDL_LoadFile(fs_path, &fs_size);
        if (fs_code) {
            SDL_GPUShaderCreateInfo sci;
            memset(&sci, 0, sizeof sci);
            sci.code = fs_code;
            sci.code_size = fs_size;
            sci.entrypoint = "main";
            sci.format = SDL_GPU_SHADERFORMAT_SPIRV;
            sci.stage = SDL_GPU_SHADERSTAGE_FRAGMENT;
            sci.num_samplers = 0;
            sci.num_uniform_buffers = 0;
            sci.num_storage_buffers = 0;
            v10_fs = SDL_CreateGPUShader(dev, &sci);
            SDL_free(fs_code);
        }
    }
    VrLog("VRPROBE DIAG v10 FS load: %s", v10_fs ? "OK" : "FAIL");
    fflush(g_vrLog);

    if (!v10_vs || !v10_fs) {
        VrLog("VRPROBE DIAG v10 init FAIL: shader load failed");
        if (v10_vs) SDL_ReleaseGPUShader(dev, v10_vs);
        if (v10_fs) SDL_ReleaseGPUShader(dev, v10_fs);
        return 0;
    }

    SDL_GPUColorTargetDescription test_ct;
    memset(&test_ct, 0, sizeof test_ct);
    test_ct.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;  /* format 29 */

    /* Vertex input: 1 buffer, stride=12 (vec3), location 0 FLOAT3 */
    SDL_GPUVertexAttribute v10_attr = { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 0 };
    SDL_GPUVertexBufferDescription v10_vbd = { .slot = 0, .pitch = 12, .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX };

    SDL_GPUGraphicsPipelineCreateInfo test_pi;
    memset(&test_pi, 0, sizeof test_pi);
    test_pi.vertex_shader = v10_vs;
    test_pi.fragment_shader = v10_fs;
    test_pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

    test_pi.vertex_input_state.vertex_buffer_descriptions = &v10_vbd;
    test_pi.vertex_input_state.num_vertex_buffers = 1;
    test_pi.vertex_input_state.vertex_attributes = &v10_attr;
    test_pi.vertex_input_state.num_vertex_attributes = 1;

    test_pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    test_pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    test_pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

    test_pi.target_info.color_target_descriptions = &test_ct;
    test_pi.target_info.num_color_targets = 1;

    VrLog("VRPROBE DIAG v10 pipeline create begin fmt=29 (shadercross VS+FS + flat VI)");
    fflush(g_vrLog);

    s_v10_pipe = SDL_CreateGPUGraphicsPipeline(dev, &test_pi);
    const char *test_err = s_v10_pipe ? NULL : SDL_GetError();
    VrLog("VRPROBE DIAG v10 pipeline result: %s err=%s",
          s_v10_pipe ? "OK" : "FAIL", test_err ? test_err : "(null)");
    fflush(g_vrLog);

    if (!s_v10_pipe) {
        VrLog("VRPROBE DIAG v10 init FAIL: pipeline creation failed");
        SDL_ReleaseGPUShader(dev, v10_vs);
        SDL_ReleaseGPUShader(dev, v10_fs);
        return 0;
    }

    SDL_Log("VRPROBE TRACE: Pipeline created, creating vertex buffer");
    /* Create vertex buffer with triangle data */
    float triangle_vertices[9] = {
        -0.5f, -0.5f, 0.0f,  /* vertex 0 */
         0.5f, -0.5f, 0.0f,  /* vertex 1 */
         0.0f,  0.5f, 0.0f   /* vertex 2 */
    };
    /* Pad to 64 bytes (16-byte alignment) for GPU buffer alignment requirements */
    float triangle_vertices_padded[16] = {0};
    memcpy(triangle_vertices_padded, triangle_vertices, sizeof triangle_vertices);
    SDL_GPUBufferCreateInfo vbuf_info;
    memset(&vbuf_info, 0, sizeof vbuf_info);
    vbuf_info.size = sizeof triangle_vertices_padded;
    vbuf_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    s_v10_vbuf = SDL_CreateGPUBuffer(dev, &vbuf_info);
    if (!s_v10_vbuf) {
        VrLog("VRPROBE DIAG v10 vertex buffer creation: FAIL");
        SDL_ReleaseGPUGraphicsPipeline(dev, s_v10_pipe);
        s_v10_pipe = NULL;
        SDL_ReleaseGPUShader(dev, v10_vs);
        SDL_ReleaseGPUShader(dev, v10_fs);
        return 0;
    }

    /* Upload vertex data via transfer buffer */
    SDL_GPUTransferBufferCreateInfo tbuf_info;
    memset(&tbuf_info, 0, sizeof tbuf_info);
    tbuf_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbuf_info.size = sizeof triangle_vertices_padded;
    SDL_GPUTransferBuffer *tbuf = SDL_CreateGPUTransferBuffer(dev, &tbuf_info);
    if (!tbuf) {
        VrLog("VRPROBE DIAG v10 vertex buffer creation: FAIL (transfer buffer)");
        SDL_ReleaseGPUBuffer(dev, s_v10_vbuf);
        s_v10_vbuf = NULL;
        SDL_ReleaseGPUGraphicsPipeline(dev, s_v10_pipe);
        s_v10_pipe = NULL;
        SDL_ReleaseGPUShader(dev, v10_vs);
        SDL_ReleaseGPUShader(dev, v10_fs);
        return 0;
    }

    void *mapped = SDL_MapGPUTransferBuffer(dev, tbuf, false);
    if (!mapped) {
        VrLog("VRPROBE DIAG v10 vertex buffer upload: FAIL (map failed)");
        SDL_ReleaseGPUTransferBuffer(dev, tbuf);
        SDL_ReleaseGPUBuffer(dev, s_v10_vbuf);
        s_v10_vbuf = NULL;
        SDL_ReleaseGPUGraphicsPipeline(dev, s_v10_pipe);
        s_v10_pipe = NULL;
        SDL_ReleaseGPUShader(dev, v10_vs);
        SDL_ReleaseGPUShader(dev, v10_fs);
        return 0;
    }
    memcpy(mapped, triangle_vertices_padded, sizeof triangle_vertices_padded);
    SDL_UnmapGPUTransferBuffer(dev, tbuf);

    /* Upload via command buffer */
    SDL_GPUCommandBuffer *upload_cmd = SDL_AcquireGPUCommandBuffer(dev);
    if (!upload_cmd) {
        VrLog("VRPROBE DIAG v10 vertex buffer upload: FAIL (no cmd)");
        SDL_ReleaseGPUTransferBuffer(dev, tbuf);
        SDL_ReleaseGPUBuffer(dev, s_v10_vbuf);
        s_v10_vbuf = NULL;
        SDL_ReleaseGPUGraphicsPipeline(dev, s_v10_pipe);
        s_v10_pipe = NULL;
        SDL_ReleaseGPUShader(dev, v10_vs);
        SDL_ReleaseGPUShader(dev, v10_fs);
        return 0;
    }
    SDL_GPUCopyPass *copy_pass = SDL_BeginGPUCopyPass(upload_cmd);
    if (!copy_pass) {
        VrLog("VRPROBE DIAG v10 vertex buffer upload: FAIL (copy pass): %s", SDL_GetError());
        if (!SDL_CancelGPUCommandBuffer(upload_cmd)) {
            VrLog("VRPROBE DIAG v10 upload cancel: FAIL: %s", SDL_GetError());
        }
        goto upload_failed;
    }
    SDL_GPUTransferBufferLocation src = { .transfer_buffer = tbuf, .offset = 0 };
    SDL_GPUBufferRegion dst = { .buffer = s_v10_vbuf, .offset = 0, .size = sizeof triangle_vertices_padded };
    SDL_UploadToGPUBuffer(copy_pass, &src, &dst, true);
    SDL_EndGPUCopyPass(copy_pass);
    if (!SDL_SubmitGPUCommandBuffer(upload_cmd)) {
        VrLog("VRPROBE DIAG v10 vertex buffer upload: FAIL (submit): %s", SDL_GetError());
        goto upload_failed;
    }
    VrLog("VRPROBE DIAG v10 vertex buffer upload submitted (GPU completion not checked)");
    SDL_ReleaseGPUTransferBuffer(dev, tbuf);

    VrLog("VRPROBE DIAG v10 init end");
    fflush(g_vrLog);
    return 1;

upload_failed:
    SDL_ReleaseGPUTransferBuffer(dev, tbuf);
    SDL_ReleaseGPUBuffer(dev, s_v10_vbuf);
    s_v10_vbuf = NULL;
    SDL_ReleaseGPUGraphicsPipeline(dev, s_v10_pipe);
    s_v10_pipe = NULL;
    SDL_ReleaseGPUShader(dev, v10_vs);
    SDL_ReleaseGPUShader(dev, v10_fs);
    return 0;
}