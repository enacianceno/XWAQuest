with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

old = '''        XrResult r = s_xrBeginSession(s_session, &begin);
        if (r != XR_SUCCESS) {
            SDL_Log("VRPROBE xrBeginSession failed result=%d", (int)r);
            return 0;
        }'''

new = '''        XrResult r = s_xrBeginSession(s_session, &begin);
        if (r != XR_SUCCESS) {
            SDL_Log("VRPROBE xrBeginSession failed result=%d (0x%X)", (int)r, (unsigned int)r);
            if (r == XR_ERROR_SESSION_NOT_READY) {
                SDL_Log("VRPROBE Session not ready - may need to wait for READY event first");
            } else if (r == XR_ERROR_SESSION_LOSS_PENDING) {
                SDL_Log("VRPROBE Session loss pending");
            } else if (r == XR_ERROR_HANDLE_INVALID) {
                SDL_Log("VRPROBE Invalid session handle");
            }
            return 0;
        }'''

if old in content:
    content = content.replace(old, new)
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Pattern not found")
    idx = content.find('XrResult r = s_xrBeginSession(s_session, &begin);')
    if idx >= 0:
        print(f"Found at {idx}")
        print(repr(content[idx:idx+200]))

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
    f.write(content)