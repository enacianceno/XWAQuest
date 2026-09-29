with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

old = '''    /* Upload vertex data via transfer buffer */
    SDL_GPUTransferBufferCreateInfo tbuf_info;
    memset(&tbuf_info, 0, sizeof tbuf_info);
    tbuf_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbuf_info.size = sizeof triangle_vertices;
    SDL_GPUTransferBuffer *tbuf = SDL_CreateGPUTransferBuffer(dev, &tbuf_info);'''

new = '''    /* Upload vertex data via transfer buffer */
    SDL_GPUTransferBufferCreateInfo tbuf_info;
    memset(&tbuf_info, 0, sizeof tbuf_info);
    tbuf_info.usage = SDL_GPU_TRANSFERBUFFERUSAGE_UPLOAD;
    tbuf_info.size = sizeof triangle_vertices_padded;
    SDL_GPUTransferBuffer *tbuf = SDL_CreateGPUTransferBuffer(dev, &tbuf_info);'''

if old in content:
    content = content.replace(old, new)
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Pattern not found")
    idx = content.find('tbuf_info.size = sizeof triangle_vertices;')
    if idx >= 0:
        print(f"Found at {idx}")
        print(repr(content[idx:idx+200]))
    else:
        print("Pattern not found")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
    f.write(content)