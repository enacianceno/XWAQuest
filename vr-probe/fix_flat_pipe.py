with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

old = '''        /* Flat pipeline creation (independent of V10) */
        memset(&s_ct, 0, sizeof s_ct);
        s_ct.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        VrLog("VRPROBE init flat pipeline create begin fmt=%d", (int)s_ct.format);
        fflush(g_vrLog);
        {
            SDL_GPUGraphicsPipelineCreateInfo pi;
            memset(&pi, 0, sizeof pi);
            pi.vertex_shader                             = s_flat_vs;
            pi.fragment_shader                           = s_flat_fs;
            pi.primitive_type                            = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

            /* vertex_input_state: connect existing s_vbd and s_attrs */
            pi.vertex_input_state.vertex_buffer_descriptions = &s_vbd;
            pi.vertex_input_state.num_vertex_buffers     = 1;
            pi.vertex_input_state.vertex_attributes      = s_attrs;
            pi.vertex_input_state.num_vertex_attributes  = 1;

            /* rasterizer_state: match VrBlit exactly */
            pi.rasterizer_state.fill_mode                = SDL_GPU_FILLMODE_FILL;
            pi.rasterizer_state.cull_mode                = SDL_GPU_CULLMODE_NONE;
            pi.rasterizer_state.front_face               = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

            /* depth_stencil_state: flat draw uses pass without depth target (zero = disabled) */
            /* multisample_state: zero = sample_count_1 (match VrBlit) */

            /* color target: only format, zero-initialized like VrBlit */
            pi.target_info.color_target_descriptions     = &s_ct;
            pi.target_info.num_color_targets             = 1;

            VrLog("VRPROBE init flat pipeline create begin fmt=%d", (int)s_ct.format);
            fflush(g_vrLog);
            s_flat_pipe = SDL_CreateGPUGraphicsPipeline(dev, &pi);
            if (!s_flat_pipe) {
                const char* err = SDL_GetError();
                VrLog("VRPROBE init WARN: flat pipeline create failed (non-fatal): %s", err ? err : "(null)");
                fflush(g_vrLog);
            } else {
                VrLog("VRPROBE init flat pipeline create ok pipe=%p", (void *)s_flat_pipe);
                SDL_Log("VRPROBE flat-color pipeline created (VS=%p FS=%p PIPE=%p)",
                        (void*)s_flat_vs, (void*)s_flat_fs, (void*)s_flat_pipe);
            }
        }
    VrLog("VRPROBE init VrStereo_Init OK %dx%d", w, h)'''

new = '''        /* Flat pipeline creation (independent of V10) - SKIPPED on Meta Quest
         * because embedded SPIR-V shaders cause vkCreateGraphicsPipelines to hang.
         * V10 pipeline (shadercross) works and is used for triangle draw. */
#if 0
        memset(&s_ct, 0, sizeof s_ct);
        s_ct.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        VrLog("VRPROBE init flat pipeline create begin fmt=%d", (int)s_ct.format);
        fflush(g_vrLog);
        {
            SDL_GPUGraphicsPipelineCreateInfo pi;
            memset(&pi, 0, sizeof pi);
            pi.vertex_shader                             = s_flat_vs;
            pi.fragment_shader                           = s_flat_fs;
            pi.primitive_type                            = SDL_GPU_PRIMITIVETYPE_TRIANGLELIST;

            /* vertex_input_state: connect existing s_vbd and s_attrs */
            pi.vertex_input_state.vertex_buffer_descriptions = &s_vbd;
            pi.vertex_input_state.num_vertex_buffers     = 1;
            pi.vertex_input_state.vertex_attributes      = s_attrs;
            pi.vertex_input_state.num_vertex_attributes  = 1;

            /* rasterizer_state: match VrBlit exactly */
            pi.rasterizer_state.fill_mode                = SDL_GPU_FILLMODE_FILL;
            pi.rasterizer_state.cull_mode                = SDL_GPU_CULLMODE_NONE;
            pi.rasterizer_state.front_face               = SDL_GPU_FRONTFACE_COUNTER_CLOCKWISE;

            /* depth_stencil_state: flat draw uses pass without depth target (zero = disabled) */
            /* multisample_state: zero = sample_count_1 (match VrBlit) */

            /* color target: only format, zero-initialized like VrBlit */
            pi.target_info.color_target_descriptions     = &s_ct;
            pi.target_info.num_color_targets             = 1;

            VrLog("VRPROBE init flat pipeline create begin fmt=%d", (int)s_ct.format);
            fflush(g_vrLog);
            s_flat_pipe = SDL_CreateGPUGraphicsPipeline(dev, &pi);
            if (!s_flat_pipe) {
                const char* err = SDL_GetError();
                VrLog("VRPROBE init WARN: flat pipeline create failed (non-fatal): %s", err ? err : "(null)");
                fflush(g_vrLog);
            } else {
                VrLog("VRPROBE init flat pipeline create ok pipe=%p", (void *)s_flat_pipe);
                SDL_Log("VRPROBE flat-color pipeline created (VS=%p FS=%p PIPE=%p)",
                        (void*)s_flat_vs, (void*)s_flat_fs, (void*)s_flat_pipe);
            }
        }
#endif
    VrLog("VRPROBE init VrStereo_Init OK %dx%d", w, h)'''

if old in content:
    content = content.replace(old, new)
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Pattern not found")
    idx = content.find('/* Flat pipeline creation (independent of V10) */')
    if idx >= 0:
        print(f"Found at {idx}")
        print(repr(content[idx:idx+200]))
    else:
        print("Pattern not found")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
    f.write(content)