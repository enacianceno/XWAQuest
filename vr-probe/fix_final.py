with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

# Fix the missing newlines between function definitions
# Fix 1: Add proper newline and closing brace for VrXr_PollEvents
content = content.replace(
    'void VrXr_PollEvents(SDL_GPUDevice *device) {\nint VrXr_ShouldQuit(void) { return s_shouldQuit; }',
    'void VrXr_PollEvents(SDL_GPUDevice *device) {\n    XrEventDataBuffer ev;\n    memset(&ev, 0, sizeof ev);\n    ev.type = XR_TYPE_EVENT_DATA_BUFFER;\n    while (s_xrPollEvent && s_xrPollEvent(s_instance, &ev) == XR_SUCCESS) {\n        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {\n            XrEventDataSessionStateChanged *st = (XrEventDataSessionStateChanged *)&ev;\n            VrLog("VRPROBE session state=%d", (int)st->state);\n            if (st->state == XR_SESSION_STATE_READY) {\n                XrSessionBeginInfo begin;\n                memset(&begin, 0, sizeof begin);\n                begin.type = XR_TYPE_SESSION_BEGIN_INFO;\n                begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;\n                if (s_xrBeginSession(s_session, &begin) == XR_SUCCESS) {\n                    s_running = 1;\n                    SDL_Log("VRPROBE session begun");\n                    if (s_viewCount == 0 && !create_swapchains(device)) {\n                        s_shouldQuit = 1;\n                    }\n                }\n            } else if (st->state == XR_SESSION_STATE_STOPPING) {\n                s_xrEndSession(s_session);\n                s_running = 0;\n                s_shouldQuit = 1;\n            } else if (st->state == XR_SESSION_STATE_EXITING ||\n                       st->state == XR_SESSION_STATE_LOSS_PENDING) {\n                s_shouldQuit = 1;\n            }\n        } else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {\n            s_shouldQuit = 1;\n        }\n        memset(&ev, 0, sizeof ev);\n        ev.type = XR_TYPE_EVENT_DATA_BUFFER;\n    }\n}\n\nint VrXr_ShouldQuit(void) { return s_shouldQuit; }'
)

# Fix the other function definitions
content = content.replace(
    'int VrXr_ShouldQuit(void) { return s_shouldQuit; }\nint VrXr_ViewCount(void) { return (int)s_viewCount; }\nSDL_GPUTextureFormat VrXr_SwapchainFormat(void) { return s_format; }\nVrEyeSwapchain *VrXr_Eye(int eye) {\n    if (eye < 0 || eye >= VR_EYE_COUNT || s_viewCount != VR_EYE_COUNT) {\n        return NULL;\n    }\n    return &s_eyes[eye];\n}\nint VrXr_EyeSize(int eye, int32_t *w, int32_t *h) {\n    VrEyeSwapchain *e = VrXr_Eye(eye);\n    if (!e) {\n        return 0;\n    }\n    *w = e->width;\n    *h = e->height;\n    return 1;\n}\nstatic int create_swapchains(SDL_GPUDevice *device) {',
    '\nint VrXr_ShouldQuit(void) { return s_shouldQuit; }\n\nint VrXr_ViewCount(void) { return (int)s_viewCount; }\n\nSDL_GPUTextureFormat VrXr_SwapchainFormat(void) { return s_format; }\n\nVrEyeSwapchain *VrXr_Eye(int eye) {\n    if (eye < 0 || eye >= VR_EYE_COUNT || s_viewCount != VR_EYE_COUNT) {\n        return NULL;\n    }\n    return &s_eyes[eye];\n}\n\nint VrXr_EyeSize(int eye, int32_t *w, int32_t *h) {\n    VrEyeSwapchain *e = VrXr_Eye(eye);\n    if (!e) {\n        return 0;\n    }\n    *w = e->width;\n    *h = e->height;\n    return 1;\n}\n\nstatic int create_swapchains(SDL_GPUDevice *device) {'
)

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
    f.write(content)
print("Fixed!")