with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

start_idx = content.find('/* Vertex input: 1 buffer, stride=12 (vec3), location 0 FLOAT3 */')
end_log_idx = content.find('VrLog("VRPROBE DIAG v10 init end");', start_idx)
return_idx = content.find('return 1;', content.find('VrLog("VRPROBE DIAG v10 init end");', start_idx))

brace_count = 0
actual_end = -1
for i in range(return_idx, len(content)):
    if content[i] == '{':
        brace_count += 1
    elif content[i] == '}':
        brace_count -= 1
        if brace_count == 0:
            actual_end = i + 1
            break

if actual_end < 0:
    print("Could not find end of function")
    exit(1)

print(f"Replacing from {start_idx} to {actual_end}")

new_section = '''/* Vertex input: 1 buffer, stride=12 (vec3), location 0 FLOAT3 */
    SDL_GPUVertexAttribute v10_attr = { .location = 0, .buffer_slot = 0, .format = SDL_GPU_VERTEXELEMENTFORMAT_FLOAT3, .offset = 0 };
    SDL_GPUVertexBufferDescription v10_vbd = { .slot = 0, .pitch = 12, .input_rate = SDL_GPU_VERTEXINPUTRATE_VERTEX };

    /* Create vertex buffer FIRST (before pipeline) to avoid hang on Meta Quest */
    SDL_Log("VRPROBE TRACE: Creating vertex buffer before pipeline");
    float triangle_vertices[9] = {
        -0.5f, -0.5f, 0.0f,  /* vertex 0 */
         0.5f, -0.5f, 0.0f,  /* vertex 1 */
         0.0f,  0.5f, 0.0f   /* vertex 2 */
    };
    float triangle_vertices_padded[16] = {0};
    memcpy(triangle_vertices_padded, triangle_vertices, sizeof triangle_vertices);
    SDL_GPUBufferCreateInfo vbuf_info;
    memset(&vbuf_info, 0, sizeof vbuf_info);
    vbuf_info.size = sizeof triangle_vertices_padded;
    vbuf_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX;
    s_v10_vbuf = SDL_CreateGPUBuffer(dev, &vbuf_info);
    if (!s_v10_vbuf) {
        VrLog("VRPROBE DIAG v10 vertex buffer creation: FAIL");
        SDL_ReleaseGPUShader(dev, v10_vs);
        SDL_ReleaseGPUShader(dev, v10_fs);
        return 0;
    }
    SDL_Log("VRPROBE TRACE: Vertex buffer created, uploading data");
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
        SDL_ReleaseGPUShader(dev, v10_vs);
        SDL_ReleaseGPUShader(dev, v10_fs);
        return 0;
    }
    SDL_GPUTransferBufferLocation src = { .transfer_buffer = tbuf, .offset = 0 };
    SDL_GPUBufferRegion dst = { .buffer = s_v10_vbuf, .offset = 0, .size = sizeof triangle_vertices_padded };
    SDL_UploadToGPUBuffer(upload_cmd, &src, &dst, 1);
    SDL_SubmitGPUCommandBuffer(upload_cmd);
    SDL_ReleaseGPUTransferBuffer(dev, tbuf);
    SDL_Log("VRPROBE TRACE: Vertex buffer uploaded successfully");

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

    SDL_Log("VRPROBE TRACE: Pipeline created successfully");
    VrLog("VRPROBE DIAG v10 init end");
    fflush(g_vrLog);
    return 1;
}'''

start_idx = content.find('/* Vertex input: 1 buffer, stride=12 (vec3), location 0 FLOAT3 */')
end_log_idx = content.find('VrLog("VRPROBE DIAG v10 init end");', start_idx)
return_idx = content.find('return 1;', content.find('VrLog("VRPROBE DIAG v10 init end");', start_idx))

brace_count = 0
actual_end = -1
for i in range(return_idx, len(content)):
    if content[i] == '{':
        brace_count += 1
    elif content[i] == '}':
        brace_count -= 1
        if brace_count == 0:
            actual_end = i + 1
            break

if actual_end > 0:
    print(f"Replacing from {start_idx} to {actual_end}")
    content = content[:start_idx] + new_section + content[actual_end:]
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Could not find end of function")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
    f.write(content)