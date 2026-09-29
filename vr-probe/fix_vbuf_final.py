with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

old = '''    SDL_Log("VRPROBE TRACE: Pipeline created, creating vertex buffer");
    /* Create vertex buffer with triangle data */
    float triangle_vertices[9] = {
        -0.5f, -0.5f, 0.0f,  /* vertex 0 */
         0.5f, -0.5f, 0.0f,  /* vertex 1 */
         0.0f,  0.5f, 0.0f   /* vertex 2 */
    };
    SDL_GPUBufferCreateInfo vbuf_info;
    memset(&vbuf_info, 0, sizeof vbuf_info);
    vbuf_info.size = sizeof triangle_vertices;
    vbuf_info.usage = SDL_GPU_BUFFERUSAGE_VERTEX | SDL_GPU_BUFFERUSAGE_TRANSFER_DST;
    s_v10_vbuf = SDL_CreateGPUBuffer(dev, &vbuf_info);'''

new = '''    SDL_Log("VRPROBE TRACE: Pipeline created, creating vertex buffer");
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
    s_v10_vbuf = SDL_CreateGPUBuffer(dev, &vbuf_info);'''

if old in content:
    content = content.replace(old, new)
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Pattern not found")
    idx = content.find('SDL_Log("VRPROBE TRACE: Pipeline created, creating vertex buffer");')
    if idx >= 0:
        print(f"Found at {idx}")
        print(repr(content[idx:idx+300]))
    else:
        print("Pattern not found")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
    f.write(content)