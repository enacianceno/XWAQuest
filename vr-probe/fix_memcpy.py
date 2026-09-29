with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

old = '''memcpy(mapped, (const float[9]){ -0.5f, -0.5f, 0.0f,  0.5f, -0.5f, 0.0f,  0.0f,  0.5f, 0.0f }, sizeof(float)*9);'''

new = '''memcpy(mapped, triangle_vertices_padded, sizeof triangle_vertices_padded);'''

if old in content:
    content = content.replace(old, new)
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Pattern not found")
    idx = content.find('memcpy(mapped, (const float[9])')
    if idx >= 0:
        print(f"Found at {idx}")
        print(repr(content[idx:idx+200]))
    else:
        print("Pattern not found")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
    f.write(content)