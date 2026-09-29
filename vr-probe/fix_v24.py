with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

old = '''            if (r == XR_SUCCESS) {
                break;
            }
            SDL_Log("VRPROBE xrBeginSession attempt %d failed result=%d, retrying...", attempt + 1, (int)r);
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

new = '''            if (r == XR_SUCCESS) {
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

if old in content:
    content = content.replace(old, new)
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Pattern not found")
    idx = content.find('if (r == XR_SUCCESS) {')
    if idx >= 0:
        print(f"Found at {idx}")
        print(repr(content[idx:idx+500]))
    else:
        print("Pattern not found")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
    f.write(content)