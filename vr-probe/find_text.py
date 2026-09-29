with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

# Find the exact text
idx = content.find('VRPROBE XR session+space ready, waiting for READY event")
    return 1;
}

void VrXr_PollEvents(SDL_GPUDevice *device) {')
if idx >= 0:
    print(f"Found at {idx}")
    print(repr(content[idx:idx+200]))
else:
    print("Not found with exact match")
    # Try to find the pattern
    idx = content.find("waiting for READY event")
    if idx >= 0:
        print(f"Found partial at {idx}: {repr(content[idx:idx+200])}")