with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

# The code from "/* Vertex input: 1 buffer..." to the end of the function needs restructuring
# Let me find the key sections and reorder

# Find the start of vertex input section
start_idx = content.find('/* Vertex input: 1 buffer, stride=12 (vec3), location 0 FLOAT3 */')
if start_idx < 0:
    print("Start marker not found")
else:
    print(f"Found vertex input at {start_idx}")

# Find the pipeline creation
pipe_idx = content.find('VrLog("VRPROBE DIAG v10 pipeline create begin fmt=29 (shadercross VS+FS + flat VI)");', start_idx)
if pipe_idx < 0:
    print("Pipeline create log not found")
else:
    print(f"Pipeline create log at {pipe_idx}")

# Find the vertex buffer creation trace
trace_idx = content.find('SDL_Log("VRPROBE TRACE: Pipeline created, creating vertex buffer");', start_idx)
if trace_idx < 0:
    print("Trace log not found")
else:
    print(f"Trace log at {trace_idx}")

# Find the vertex buffer creation
vbuf_idx = content.find('SDL_Log("VRPROBE TRACE: Pipeline created, creating vertex buffer");', start_idx)
if vbuf_idx < 0:
    print("Trace log not found")
else:
    print(f"Trace log at {vbuf_idx}")

# Find the vertex buffer creation
vbuf_create_idx = content.find('s_v10_vbuf = SDL_CreateGPUBuffer(dev, &vbuf_info);', start_idx)
if vbuf_create_idx < 0:
    print("Buffer create not found")
else:
    print(f"Buffer create at {vbuf_create_idx}")

# Find the end of the function (return 1)
end_idx = content.find('VrLog("VRPROBE DIAG v10 init end");', start_idx)
if end_idx < 0:
    print("End marker not found")
else:
    print(f"End marker at {end_idx}")

# Let me find the exact boundaries for the section to replace
# From "/* Vertex input: 1 buffer..." to the end of the function
# Actually, let me just rewrite the entire VrStereo_InitV10Pipeline function with the correct order

print("Done analyzing")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
    f.write(content)