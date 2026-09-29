with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

# Add the missing accessor functions after VrXr_Init
old = '}\n\nvoid VrXr_PollEvents(SDL_GPUDevice *device) {'
new = '''}
int VrXr_IsSessionRunning(void) { return s_running; }
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
void VrXr_PollEvents(SDL_GPUDevice *device) {'''

if '}\n\nvoid VrXr_PollEvents(SDL_GPUDevice *device) {' in content:
    content = content.replace(
        '}\n\nvoid VrXr_PollEvents(SDL_GPUDevice *device) {',
        '}\n\nint VrXr_IsSessionRunning(void) { return s_running; }\nint VrXr_ShouldQuit(void) { return s_shouldQuit; }\nint VrXr_ViewCount(void) { return (int)s_viewCount; }\nSDL_GPUTextureFormat VrXr_SwapchainFormat(void) { return s_format; }\nVrEyeSwapchain *VrXr_Eye(int eye) {\n    if (eye < 0 || eye >= VR_EYE_COUNT || s_viewCount != VR_EYE_COUNT) {\n        return NULL;\n    }\n    return &s_eyes[eye];\n}\nint VrXr_EyeSize(int eye, int32_t *w, int32_t *h) {\n    VrEyeSwapchain *e = VrXr_Eye(eye);\n    if (!e) {\n        return 0;\n    }\n    *w = e->width;\n    *h = e->height;\n    return 1;\n}\n\nvoid VrXr_PollEvents(SDL_GPUDevice *device) {'
    )
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Pattern not found")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
    f.write(content)