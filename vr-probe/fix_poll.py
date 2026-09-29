with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

# Replace the READY event handler to remove xrBeginSession call
old = '''            if (st->state == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo begin;
                memset(&begin, 0, sizeof begin);
                begin.type = XR_TYPE_SESSION_BEGIN_INFO;
                begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                if (s_xrBeginSession(s_session, &begin) == XR_SUCCESS) {
                    s_running = 1;
                    SDL_Log("VRPROBE session begun");
                    if (s_viewCount == 0 && !create_swapchains(device)) {
                        s_shouldQuit = 1;
                    }
                }
            } else if (st->state == XR_SESSION_STATE_STOPPING) {'''

new = '''            if (st->state == XR_SESSION_STATE_READY) {
                VrLog("VRPROBE session READY (session already begun)");
                if (s_viewCount == 0 && !create_swapchains(device)) {
                    s_shouldQuit = 1;
                }
            } else if (st->state == XR_SESSION_STATE_STOPPING) {'''

if 'if (st->state == XR_SESSION_STATE_READY) {\n                XrSessionBeginInfo begin;\n                memset(&begin, 0, sizeof begin);\n                begin.type = XR_TYPE_SESSION_BEGIN_INFO;\n                begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;\n                if (s_xrBeginSession(s_session, &begin) == XR_SUCCESS) {\n                    s_running = 1;\n                    SDL_Log("VRPROBE session begun");\n                    if (s_viewCount == 0 && !create_swapchains(device)) {\n                        s_shouldQuit = 1;\n                    }\n                }\n            } else if (st->state == XR_SESSION_STATE_STOPPING) {' in content:
    content = content.replace(
        '''            if (st->state == XR_SESSION_STATE_READY) {
                XrSessionBeginInfo begin;
                memset(&begin, 0, sizeof begin);
                begin.type = XR_TYPE_SESSION_BEGIN_INFO;
                begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
                if (s_xrBeginSession(s_session, &begin) == XR_SUCCESS) {
                    s_running = 1;
                    SDL_Log("VRPROBE session begun");
                    if (s_viewCount == 0 && !create_swapchains(device)) {
                        s_shouldQuit = 1;
                    }
                }
            } else if (st->state == XR_SESSION_STATE_STOPPING) {''',
        '''            if (st->state == XR_SESSION_STATE_READY) {
                VrLog("VRPROBE session READY (session already begun)");
                if (s_viewCount == 0 && !create_swapchains(device)) {
                    s_shouldQuit = 1;
                }
            } else if (st->state == XR_SESSION_STATE_STOPPING) {'''
    )
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Pattern not found")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
    f.write(content)