with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

old = '''        if (r != XR_SUCCESS) {
            SDL_Log("VRPROBE xrBeginSession failed after retries result=%d (0x%X)", (int)r, (unsigned int)r);
            if (r == XR_ERROR_SESSION_NOT_READY) {
                SDL_Log("VRPROBE Session not ready - session may not be in IDLE state");
            } else if (r == XR_ERROR_SESSION_LOSS_PENDING) {
                SDL_Log("VRPROBE Session loss pending");
            } else if (r == XR_ERROR_HANDLE_INVALID) {
                SDL_Log("VRPROBE Invalid session handle");
            } else if (r == XR_ERROR_INSTANCE_LOSS_PENDING) {
                SDL_Log("VRPROBE Instance loss pending");
            } else if (r == XR_ERROR_OUT_OF_MEMORY) {
                SDL_Log("VRPROBE Out of memory");
            } else if (r == XR_ERROR_OUT_OF_DATE) {
                SDL_Log("VRPROBE Out of date");
            } else if (r == XR_ERROR_VALIDATION_FAILURE) {
                SDL_Log("VRPROBE Validation failure");
            }
            return 0;
        }'''

new = '''        if (r != XR_SUCCESS) {
            SDL_Log("VRPROBE xrBeginSession failed after retries result=%d (0x%X)", (int)r, (unsigned int)r);
            return 0;
        }'''

if old in content:
    content = content.replace(old, new)
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Pattern not found")
    idx = content.find('if (r != XR_SUCCESS) {')
    if idx >= 0:
        print(f"Found at {idx}")
        print(repr(content[idx:idx+300]))
    else:
        print("Pattern not found")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
    f.write(content)