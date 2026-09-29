with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

old = '''/* Create swapchains BEFORE entering the wait loop.
     * The READY event will be sent after swapchains are created,
     * and we'll call xrBeginSession in the READY event handler. */
    if (!create_swapchains(device)) {
        return 0;
    }
    SDL_Log("VRPROBE XR swapchains created, starting session");
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
    }
    return 1;
}'''

new = '''/* Create swapchains FIRST, then explicitly begin the session.
     * xrBeginSession must be called explicitly after swapchains are created.
     * The READY event is sent AFTER xrBeginSession succeeds. */
    if (!create_swapchains(device)) {
        return 0;
    }
    SDL_Log("VRPROBE XR swapchains created, starting session");
    {
        XrSessionBeginInfo begin;
        memset(&begin, 0, sizeof begin);
        begin.type = XR_TYPE_SESSION_BEGIN_INFO;
        begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        /* xrBeginSession can fail with XR_ERROR_SESSION_NOT_READY if the runtime
         * needs more time to process swapchain creation. Retry a few times. */
        XrResult r = XR_ERROR_SESSION_NOT_READY;
        for (int attempt = 0; attempt < 5; ++attempt) {
            XrResult r = s_xrBeginSession(s_session, &begin);
            if (r == XR_SUCCESS) {
                break;
            }
            SDL_Log("VRPROBE xrBeginSession attempt %d failed result=%d, retrying...", attempt + 1, (int)r);
            if (r != XR_ERROR_SESSION_NOT_READY) {
                break;  /* Don't retry other errors */
            }
            SDL_Delay(100);
        }
        if (r != XR_SUCCESS) {
            SDL_Log("VRPROBE xrBeginSession failed after retries result=%d", (int)r);
            return 0;
        }
        s_running = 1;
        SDL_Log("VRPROBE session begun");
    }
    return 1;
}'''

if old in content:
    content = content.replace(old, new)
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Pattern not found")
    # Debug
    idx = content.find('if (!create_swapchains(device))')
    if idx >= 0:
        print(f"Found at {idx}")
        print(repr(content[idx:idx+500]))
    else:
        print("Pattern not found")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
    f.write(content)