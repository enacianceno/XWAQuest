with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

old_text = 'SDL_Log("VRPROBE XR session+space ready, waiting for READY event");\n    return 1;\n}\n\nint VrXr_IsSessionRunning(void) { return s_running; }'

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

if 'SDL_Log("VRPROBE XR session+space ready, waiting for READY event");\n    return 1;\n}\n\nint VrXr_IsSessionRunning(void) { return s_running; }' in content:
    content = content.replace(
        'SDL_Log("VRPROBE XR session+space ready, waiting for READY event");\n    return 1;\n}\n\nint VrXr_IsSessionRunning(void) { return s_running; }',
        '''SDL_Log("VRPROBE XR session+space ready, starting session");
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
    )
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Pattern not found")