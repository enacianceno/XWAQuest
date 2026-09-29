with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

# Replace the VrXr_Init return section
old_text = '''SDL_Log("VRPROBE XR session+space ready, waiting for READY event");
    return 1;
}

void VrXr_PollEvents(SDL_GPUDevice *device) {'''

new_text = '''SDL_Log("VRPROBE XR session+space ready, starting session");
    {
        XrSessionBeginInfo begin;
        memset(&begin, 0, sizeof begin);
        begin.type = XR_TYPE_SESSION_BEGIN_INFO;
        begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        XrResult r = s_xrBeginSession(s_session, &begin);
        if (r != XR_SUCCESS) {
            SDL_Log("VRPROBE xrBeginSession failed result=%d", (int)r);
            return 0;
        }
        s_running = 1;
        SDL_Log("VRPROBE session begun");
        if (!create_swapchains(device)) {
            return 0;
        }
    }
    return 1;
}

void VrXr_PollEvents(SDL_GPUDevice *device) {'''

if old_text in content:
    content = content.replace(old_text, new_text)
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Pattern not found!")
    # Let's find what's actually there
    idx = content.find("VRPROBE XR session+space ready")
    if idx >= 0:
        print(f"Found at {idx}: {repr(content[idx:idx+100])}")
    else:
        print("Not found")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
    f.write(content)