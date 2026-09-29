/* VR-PROBE swapchain copy (isolated test code).
 * Mirrors Aeron_DrawTexture's fullscreen path (render_backend.c) but targets
 * OpenXR swapchain images instead of the Android window swapchain.
 * Supports HDR→SDR conversion for diagnostic blit of scene_tex. */
#include "vr_blit.h"
#include "vr_log.h"

#include "internal.h"

#include <string.h>

static SDL_GPUShader *s_vs;
static SDL_GPUShader *s_fs;
static SDL_GPUShader *s_fs_hdr;
static SDL_GPUGraphicsPipeline *s_pipe;
static SDL_GPUTextureFormat s_fmt;
static SDL_GPUSampler *s_sampler;

static SDL_GPUShader *load_spv(SDL_GPUDevice *device, const char *name,
                               SDL_GPUShaderStage stage, Uint32 samplers) {
    char path[1024];
    size_t size = 0;
    SDL_snprintf(path, sizeof path, "%s/%s.spv", g_aeron.shader_root, name);
    Uint8 *code = (Uint8 *)SDL_LoadFile(path, &size);
    if (!code) {
        SDL_Log("VRPROBE blit shader missing %s: %s", path, SDL_GetError());
        return NULL;
    }
    SDL_GPUShaderCreateInfo info;
    memset(&info, 0, sizeof info);
    info.code = code;
    info.code_size = size;
    info.entrypoint = "main";
    info.format = SDL_GPU_SHADERFORMAT_SPIRV;
    info.stage = stage;
    info.num_samplers = samplers;
    info.num_uniform_buffers = stage == SDL_GPU_SHADERSTAGE_FRAGMENT ? 1u : 0u;
    SDL_GPUShader *sh = SDL_CreateGPUShader(device, &info);
    SDL_free(code);
    return sh;
}

/* HDR→SDR tonemap shader for diagnostic blit of scene_tex (RGBA16_FLOAT → RGBA8_SRGB).
 * Optional: if the shader is missing, the HDR pipeline is disabled and a warning is logged. */
static SDL_GPUShader *s_fs_hdr_to_sdr = NULL;
static SDL_GPUGraphicsPipeline *s_pipe_hdr = NULL;

static int VrBlit_EnsureHdrPipeline(SDL_GPUDevice *device, SDL_GPUTextureFormat xr_format) {
    if (s_pipe_hdr && s_fmt == xr_format) return 1;
    if (s_fs_hdr) {
        SDL_ReleaseGPUShader(device, s_fs_hdr);
        s_fs_hdr = NULL;
    }
    if (s_pipe_hdr) {
        SDL_ReleaseGPUGraphicsPipeline(device, s_pipe_hdr);
        s_pipe_hdr = NULL;
    }
    s_fs_hdr = load_spv(device, "fullscreen_hdr_to_sdr.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 1);
    if (!s_fs_hdr) {
        VrLog("VRPROBE blit: HDR→SDR shader missing, HDR pipeline disabled");
        return 1; /* Optional: continue without HDR pipeline */
    }
    if (!s_vs) return 0;

    SDL_GPUColorTargetDescription ct;
    memset(&ct, 0, sizeof ct);
    ct.format = xr_format;
    SDL_GPUGraphicsPipelineCreateInfo pi;
    memset(&pi, 0, sizeof pi);
    pi.vertex_shader = s_vs;
    pi.fragment_shader = s_fs_hdr;
    pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    pi.target_info.color_target_descriptions = &ct;
    pi.target_info.num_color_targets = 1;
    s_pipe_hdr = SDL_CreateGPUGraphicsPipeline(device, &pi);
    if (!s_pipe_hdr) {
        VrLog("VRPROBE blit: HDR pipeline creation failed, HDR pipeline disabled");
        s_fs_hdr = NULL; /* Clean up */
        return 1; /* Optional: continue without HDR pipeline */
    }
    return 1;
}

int VrBlit_Init(SDL_GPUDevice *device, SDL_GPUTextureFormat xr_format) {
    if (s_pipe && s_fmt == xr_format) {
        VrLog("VRPROBE blit init SKIP: already initialized fmt=%d", (int)xr_format);
        return 1;
    }
    VrLog("VRPROBE blit init begin device=%p fmt=%d", (void *)device, (int)xr_format);
    VrBlit_Shutdown(device);
    VrLog("VRPROBE blit init load VS begin");
    s_vs = load_spv(device, "fullscreen.vert", SDL_GPU_SHADERSTAGE_VERTEX, 0);
    VrLog("VRPROBE blit init load VS end vs=%p", (void *)s_vs);
    VrLog("VRPROBE blit init load FS begin");
    s_fs = load_spv(device, "fullscreen.frag", SDL_GPU_SHADERSTAGE_FRAGMENT, 1);
    VrLog("VRPROBE blit init load FS end fs=%p", (void *)s_fs);
    if (!s_vs || !s_fs) {
        VrLog("VRPROBE blit init FAIL: shaders missing vs=%p fs=%p", (void *)s_vs, (void *)s_fs);
        return 0;
    }
    SDL_GPUColorTargetDescription ct;
    memset(&ct, 0, sizeof ct);
    ct.format = xr_format;
    SDL_GPUGraphicsPipelineCreateInfo pi;
    memset(&pi, 0, sizeof pi);
    pi.vertex_shader = s_vs;
    pi.fragment_shader = s_fs;
    pi.primitive_type = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;
    pi.rasterizer_state.fill_mode = SDL_GPU_FILLMODE_FILL;
    pi.rasterizer_state.cull_mode = SDL_GPU_CULLMODE_NONE;
    pi.rasterizer_state.front_face = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;
    pi.target_info.color_target_descriptions = &ct;
    pi.target_info.num_color_targets = 1;
    VrLog("VRPROBE blit init pipeline create begin fmt=%d", (int)xr_format);
    s_pipe = SDL_CreateGPUGraphicsPipeline(device, &pi);
    VrLog("VRPROBE blit init pipeline create end pipe=%p", (void *)s_pipe);
    if (!s_pipe) {
        VrLog("VRPROBE blit init FAIL: pipeline creation failed fmt=%d err=%s",
              (int)xr_format, SDL_GetError());
        return 0;
    }
    VrLog("VRPROBE blit init HDR pipeline begin");
    if (!VrBlit_EnsureHdrPipeline(device, xr_format)) {
        VrLog("VRPROBE blit init FAIL: HDR pipeline failed");
        return 0;
    }
    VrLog("VRPROBE blit init sampler create begin");
    SDL_GPUSamplerCreateInfo si;
    memset(&si, 0, sizeof si);
    si.min_filter = SDL_GPU_FILTER_LINEAR;
    si.mag_filter = SDL_GPU_FILTER_LINEAR;
    si.mipmap_mode = SDL_GPU_SAMPLERMIPMAPMODE_NEAREST;
    si.address_mode_u = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    si.address_mode_v = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    si.address_mode_w = SDL_GPU_SAMPLERADDRESSMODE_CLAMP_TO_EDGE;
    s_sampler = SDL_CreateGPUSampler(device, &si);
    VrLog("VRPROBE blit init sampler create end sampler=%p", (void *)s_sampler);
    if (!s_sampler) {
        VrLog("VRPROBE blit init FAIL: sampler creation failed err=%s", SDL_GetError());
        return 0;
    }
    s_fmt = xr_format;
    VrLog("VRPROBE blit ready format=%d", (int)xr_format);
    return 1;
}

int VrBlit_Copy(AeronCommandBuffer *cmd, SDL_GPUDevice *device,
                AeronTexture *src, SDL_GPUTexture *dst, int w, int h) {
    SDL_GPUCommandBuffer *cb;
    SDL_GPURenderPass *pass;
    SDL_GPUColorTargetInfo ct;
    SDL_GPUTextureSamplerBinding bind;
    float u[12];
    SDL_GPUViewport vp;
    SDL_Rect sc;
    (void)device;
    if (!cmd || !cmd->command_buffer || !src || !src->texture || !dst || (!s_pipe && !s_pipe_hdr)) {
        return 0;
    }
    cb = cmd->command_buffer;
    memset(&ct, 0, sizeof ct);
    ct.texture = dst;
    ct.load_op = SDL_GPU_LOADOP_DONT_CARE;
    ct.store_op = SDL_GPU_STOREOP_STORE;
    ct.cycle = false;
    VrLog("VRPROBE blit SDL_BeginGPURenderPass begin dst=%p", (void *)dst);
    pass = SDL_BeginGPURenderPass(cb, &ct, 1, NULL);
    VrLog("VRPROBE blit SDL_BeginGPURenderPass end pass=%p", (void *)pass);
    if (!pass) {
        VrLog("VRPROBE blit pass failed: %s", SDL_GetError());
        return 0;
    }
    bind.texture = src->texture;
    bind.sampler = s_sampler;
    /* Determine if we need HDR→SDR conversion based on source format and known swapchain format. */
    int is_hdr_src = (src->format == AERON_TEXTURE_FORMAT_RGBA16_FLOAT);
    int is_sdr_dst = (s_fmt == 20 || s_fmt == 21 || s_fmt == 22 || s_fmt == 23);
    int use_hdr_pipe = is_hdr_src && is_sdr_dst;
    SDL_GPUGraphicsPipeline *pipe = use_hdr_pipe ? s_pipe_hdr : s_pipe;
    if (!pipe) {
        VrLog("VRPROBE blit: required pipeline not available");
        return 0;
    }
    /* Present RTs are linear HDR carrying their own tonemap; copy exact.
     * HDR source → SDR dest: use HDR→SDR tonemap shader. */
    u[0] = 0.0f;  // mode: 0=copy, 1=HDR→SDR tonemap
    u[1] = 0.0f;
    u[2] = 1.0f;  // exposure
    u[3] = 0.0f;
    u[4] = u[5] = u[6] = u[7] = 1.0f;
    u[8] = u[9] = u[10] = 0.0f;
    u[11] = 0.0f;
    if (use_hdr_pipe) {
        u[0] = 1.0f;  // enable HDR→SDR tonemap
        u[2] = 1.0f;  // exposure
    }
    VrLog("VRPROBE blit BindPipeline begin");
    SDL_BindGPUGraphicsPipeline(pass, pipe);
    VrLog("VRPROBE blit BindPipeline end");
    VrLog("VRPROBE blit BindSamplers begin");
    SDL_BindGPUFragmentSamplers(pass, 0, &bind, 1);
    VrLog("VRPROBE blit BindSamplers end");
    VrLog("VRPROBE blit PushUniforms begin");
    SDL_PushGPUFragmentUniformData(cb, 0, u, sizeof u);
    VrLog("VRPROBE blit PushUniforms end");
    vp.x = 0.0f;
    vp.y = 0.0f;
    vp.w = (float)w;
    vp.h = (float)h;
    vp.min_depth = 0.0f;
    vp.max_depth = 1.0f;
    VrLog("VRPROBE blit SetViewport begin");
    SDL_SetGPUViewport(pass, &vp);
    VrLog("VRPROBE blit SetViewport end");
    sc.x = 0;
    sc.y = 0;
    sc.w = w;
    sc.h = h;
    VrLog("VRPROBE blit SetScissor begin");
    SDL_SetGPUScissor(pass, &sc);
    VrLog("VRPROBE blit SetScissor end");
    VrLog("VRPROBE blit DrawPrimitives begin");
    SDL_DrawGPUPrimitives(pass, 3, 1, 0, 0);
    VrLog("VRPROBE blit DrawPrimitives end");
    VrLog("VRPROBE blit EndRenderPass begin");
    SDL_EndGPURenderPass(pass);
    VrLog("VRPROBE blit EndRenderPass end");
    return 1;
}

void VrBlit_Shutdown(SDL_GPUDevice *device) {
    if (s_sampler) {
        SDL_ReleaseGPUSampler(device, s_sampler);
        s_sampler = NULL;
    }
    if (s_pipe_hdr) {
        SDL_ReleaseGPUGraphicsPipeline(device, s_pipe_hdr);
        s_pipe_hdr = NULL;
    }
    if (s_fs_hdr) {
        SDL_ReleaseGPUShader(device, s_fs_hdr);
        s_fs_hdr = NULL;
    }
    if (s_pipe) {
        SDL_ReleaseGPUGraphicsPipeline(device, s_pipe);
        s_pipe = NULL;
    }
    if (s_fs) {
        SDL_ReleaseGPUShader(device, s_fs);
        s_fs = NULL;
    }
    if (s_vs) {
        SDL_ReleaseGPUShader(device, s_vs);
        s_vs = NULL;
    }
    s_fmt = SDL_GPU_TEXTUREFORMAT_INVALID;
}
