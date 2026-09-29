with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

# The code from line 128 to 159 is broken. Let me replace the entire block from "/* xrBeginSession can fail..." to the end of the function
old = '''        /* xrBeginSession can fail with XR_ERROR_SESSION_NOT_READY if the runtime
         * needs more time to process swapchain creation. Retry a few times. */
        XrResult r = XR_ERROR_SESSION_NOT_READY;
        for (int attempt = 0; attempt < 10; ++attempt) {
            XrResult r = s_xrBeginSession(s_session, &begin);
        XrResult r = s_xrBeginSession(s_session, &begin);
        SDL_Log("VRPROBE xrBeginSession returned %d (0x%X)", (int)r, (unsigned int)r);
        if (r != XR_SUCCESS) {
            SDL_Log("VRPROBE xrBeginSession failed with error %d (0x%X)", (int)r, (unsigned int)r);
            if (r == XR_ERROR_SESSION_NOT_READY) {
                SDL_Log("VRPROBE Session not ready - retrying...");
            } else {
                SDL_Log("VRPROBE xrBeginSession failed with error %d (0x%X)", (int)r, (unsigned int)r);
                return 0;
            }
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
            SDL_Log("VRPROBE xrBeginSession returned %d (0x%X)", (int)r, (unsigned int)r);
            if (r != XR_SUCCESS) {
                SDL_Log("VRPROBE xrBeginSession failed with error %d (0x%X)", (int)r, (unsigned int)r);
                if (r == XR_ERROR_SESSION_NOT_READY) {
                    SDL_Log("VRPROBE Session not ready - retrying...");
                } else {
                    return 0;
                }
            } else {
                break;
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

if old in content:
    content = content.replace(old, new)
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Pattern not found - trying alternative approach")
    # Try to find the start and end markers
    start = content.find('/* xrBeginSession can fail with XR_ERROR_SESSION_NOT_READY if the runtime')
    if start >= 0:
        # Find the end of the function (two closing braces followed by blank line and int VrXr_IsSessionRunning)
        end = content.find('int VrXr_IsSessionRunning(void)', start)
        if end >= 0:
            # Find the second '}' before that
            brace_count = 0
            actual_end = -1
            for i in range(start, end):
                if content[i] == '{':
                    brace_count += 1
                elif content[i] == '}':
                    brace_count -= 1
                    if brace_count == 0:
                        actual_end = i + 1
                        break
            if actual_end > 0:
                print(f"Found block from {start} to {actual_end}")
                content = content[:start] + new + content[actual_end:]
                with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
                    f.write(content)
                print("Fixed with alternative approach!")
            else:
                print("Could not find end of block")
        else:
            print("Could not find VrXr_IsSessionRunning")
    else:
        print("Could not find start pattern")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
    f.write(content)