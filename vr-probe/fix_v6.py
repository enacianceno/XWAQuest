with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

old = '''void VrXr_PollEvents(SDL_GPUDevice *device) {
int VrXr_ShouldQuit(void) { return s_shouldQuit; }
int VrXr_ViewCount(void) { return (int)s_viewCount; }
SDL_GPUTextureFormat VrXr_SwapchainFormat(void) { return s_format; }
VrEyeSwapchain *VrXr_Eye(int eye) {
    if (eye < 0 || eye >= VR_EYE_COUNT || s_viewCount != VR_EYE_COUNT) {
        return NULL;
    }
    return &s_eyes[eye];
}
int VrXr_EyeSize(int eye, int32_t *w, int32_t *h) {
    VrEyeSwapchain *e = VrXr_Eye(eye);
    if (!e) {
        return 0;
    }
    *w = e->width;
    *h = e->height;
    return 1;
}
static int create_swapchains(SDL_GPUDevice *device) {'''

new = '''void VrXr_PollEvents(SDL_GPUDevice *device) {
    XrEventDataBuffer ev;
    memset(&ev, 0, sizeof ev);
    ev.type = XR_TYPE_EVENT_DATA_BUFFER;
    while (s_xrPollEvent && s_xrPollEvent(s_instance, &ev) == XR_SUCCESS) {
        if (ev.type == XR_TYPE_EVENT_DATA_SESSION_STATE_CHANGED) {
            XrEventDataSessionStateChanged *st = (XrEventDataSessionStateChanged *)&ev;
            VrLog("VRPROBE session state=%d", (int)st->state);
            if (st->state == XR_SESSION_STATE_READY) {
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
            } else if (st->state == XR_SESSION_STATE_STOPPING) {
                s_xrEndSession(s_session);
                s_running = 0;
                s_shouldQuit = 1;
            } else if (st->state == XR_SESSION_STATE_EXITING ||
                       st->state == XR_SESSION_STATE_LOSS_PENDING) {
                s_shouldQuit = 1;
            }
        } else if (ev.type == XR_TYPE_EVENT_DATA_INSTANCE_LOSS_PENDING) {
            s_shouldQuit = 1;
        }
        memset(&ev, 0, sizeof ev);
        ev.type = XR_TYPE_EVENT_DATA_BUFFER;
    }
}

int VrXr_ShouldQuit(void) { return s_shouldQuit; }
int VrXr_ViewCount(void) { return (int)s_viewCount; }
SDL_GPUTextureFormat VrXr_SwapchainFormat(void) { return s_format; }
VrEyeSwapchain *VrXr_Eye(int eye) {
    if (eye < 0 || eye >= VR_EYE_COUNT || s_viewCount != VR_EYE_COUNT) {
        return NULL;
    }
    return &s_eyes[eye];
}
int VrXr_EyeSize(int eye, int32_t *w, int32_t *h) {
    VrEyeSwapchain *e = VrXr_Eye(eye);
    if (!e) {
        return 0;
    }
    *w = e->width;
    *h = e->height;
    return 1;
}
static int create_swapchains(SDL_GPUDevice *device) {'''

if old_text in content:
    content = content.replace(old_text, new_text)
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Pattern not found")
    # Debug
    if 'void VrXr_PollEvents(SDL_GPUDevice *device) {' in content:
        print("Found VrXr_PollEvents but with issues")
    else:
        print("VrXr_PollEvents not found")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
    f.write(content)