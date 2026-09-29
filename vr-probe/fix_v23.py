with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

old = '''        XrResult r = s_xrBeginSession(s_session, &begin);
        if (r != XR_SUCCESS) {
            SDL_Log("VRPROBE xrBeginSession failed result=%d", (int)r);
            return 0;
        }
        s_running = 1;
        SDL_Log("VRPROBE session begun");
    }
    return 1;
}'''

new = '''        /* xrBeginSession can fail with XR_ERROR_SESSION_NOT_READY if the runtime
         * needs more time to process swapchain creation. Retry a few times. */
        XrResult r = XR_ERROR_SESSION_NOT_READY;
        for (int attempt = 0; attempt < 10; ++attempt) {
            XrResult r = s_xrBeginSession(s_session, &begin);
            SDL_Log("VRPROBE xrBeginSession attempt %d returned %d (0x%X)", attempt + 1, (int)r, (unsigned int)r);
            if (r == XR_SUCCESS) {
                break;
            }
            SDL_Log("VRPROBE xrBeginSession attempt %d failed result=%d (0x%X), retrying...", attempt + 1, (int)r, (unsigned int)r);
            if (r != XR_ERROR_SESSION_NOT_READY) {
                break;  /* Don't retry other errors */
            }
            SDL_Delay(200);
        }
        if (r != XR_SUCCESS) {
            SDL_Log("VRPROBE xrBeginSession failed after retries result=%d (0x%X)", (int)r, (unsigned int)r);
            return 0;
        }
        s_running = 1;
        SDL_Log("VRPROBE session begun");
    }
    return 1;
}'''

if 'XrResult r = s_xrBeginSession(s_session, &begin);\n        if (r != XR_SUCCESS) {\n            SDL_Log("VRPROBE xrBeginSession failed result=%d", (int)r);\n            return 0;\n        }\n        s_running = 1;\n        SDL_Log("VRPROBE session begun");\n    }\n    return 1;\n}' in content:
    content = content.replace(
        '''        XrResult r = s_xrBeginSession(s_session, &begin);
        if (r != XR_SUCCESS) {
            SDL_Log("VRPROBE xrBeginSession failed result=%d", (int)r);
            return 0;
        }
        s_running = 1;
        SDL_Log("VRPROBE session begun");
    }
    return 1;
}''',
        '''        /* xrBeginSession can fail with XR_ERROR_SESSION_NOT_READY if the runtime
         * needs more time to process swapchain creation. Retry a few times. */
        XrResult r = XR_ERROR_SESSION_NOT_READY;
        for (int attempt = 0; attempt < 10; ++attempt) {
            XrResult r = s_xrBeginSession(s_session, &begin);
            SDL_Log("VRPROBE xrBeginSession attempt %d returned %d (0x%X)", attempt + 1, (int)r, (unsigned int)r);
            if (r == XR_SUCCESS) {
                break;
            }
            SDL_Log("VRPROBE xrBeginSession attempt %d failed result=%d (0x%X), retrying...", attempt + 1, (int)r, (unsigned int)r);
            if (r != XR_ERROR_SESSION_NOT_READY) {
                break;  /* Don't retry other errors */
            }
            SDL_Delay(200);
        }
        if (r != XR_SUCCESS) {
            SDL_Log("VRPROBE xrBeginSession failed after retries result=%d (0x%X)", (int)r, (unsigned int)r);
            return 0;
        }
        s_running = 1;
        SDL_Log("VRPROBE session begun");
    }
    return 1;
}''')
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Pattern not found")
    idx = content.find('XrResult r = s_xrBeginSession(s_session, &begin);')
    if idx >= 0:
        print(f"Found at {idx}")
        print(repr(content[idx:idx+300]))
    else:
        print("Pattern not found")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
    f.write(content)