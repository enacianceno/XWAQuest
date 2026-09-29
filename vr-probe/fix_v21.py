with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

# Find the xrBeginSession block and replace it with retry logic
import re

# Find the xrBeginSession call
idx = content.find('XrResult r = s_xrBeginSession(s_session, &begin);')
if idx >= 0:
    # Find the end of the block (the closing brace of the VrXr_Init function)
    start_idx = content.rfind('{\n        XrSessionBeginInfo begin;', 0, content.find('XrResult r = s_xrBeginSession(s_session, &begin);'))
    if start_idx < 0:
        start_idx = content.find('XrSessionBeginInfo begin;')
    
    # Find the end of the function
    end_idx = content.find('}\n}\n\nint VrXr_IsSessionRunning', content.find('XrResult r = s_xrBeginSession(s_session, &begin);'))
    
    if idx >= 0:
        print(f"Found xrBeginSession at {idx}")
        # Find the end of the function
        brace_count = 0
        in_block = False
        end_idx = -1
        for i in range(idx, len(content)):
            if content[i] == '{':
                brace_count += 1
                in_block = True
            elif content[i] == '}':
                brace_count -= 1
                if brace_count == 0 and in_block:
                    end_idx = i + 1
                    break
        
        if end_idx > 0:
            print(f"Function ends at {end_idx}")
            print(repr(content[idx:end_idx+50]))
        else:
            print("Could not find end of function")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

# Let's just do a simple replacement using a more flexible approach
# Find the block from "SDL_Log("VRPROBE XR swapchains created, starting session");" to the end of the function
idx = content.find('SDL_Log("VRPROBE XR swapchains created, starting session");')
if idx >= 0:
    print(f"Found swapchains log at {idx}")
    # Find the end of the function
    brace_count = 0
    in_block = False
    end_idx = -1
    for i in range(idx, len(content)):
        if content[i] == '{':
            brace_count += 1
            in_block = True
        elif content[i] == '}':
            brace_count -= 1
            if brace_count == 0:
                end_idx = i + 1
                break
    
    if end_idx > 0:
        print(f"Function ends at {end_idx}")
        # Replace from the swapchains log to the end of function
        old_block = content[idx:end_idx+1]
        print(f"Block to replace: {repr(old_block[:200])}...")
        
        new_block = '''    SDL_Log("VRPROBE XR swapchains created, starting session");
    {
        XrSessionBeginInfo begin;
        memset(&begin, 0, sizeof begin);
        begin.type = XR_TYPE_SESSION_BEGIN_INFO;
        begin.primaryViewConfigurationType = XR_VIEW_CONFIGURATION_TYPE_PRIMARY_STEREO;
        /* xrBeginSession can fail with XR_ERROR_SESSION_NOT_READY if the runtime
         * needs more time to process swapchain creation. Retry a few times. */
        XrResult r = XR_ERROR_SESSION_NOT_READY;
        for (int attempt = 0; attempt < 10; ++attempt) {
            XrResult r = s_xrBeginSession(s_session, &begin);
            if (r == XR_SUCCESS) {
                break;
            }
            SDL_Log("VRPROBE xrBeginSession attempt %d failed result=%d, retrying...", attempt + 1, (int)r);
            if (r != XR_ERROR_SESSION_NOT_READY) {
                break;  /* Don't retry other errors */
            }
            SDL_Delay(200);
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

        if old_block in content:
            content = content[:idx] + new_block + content[end_idx:]
            with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
                f.write(content)
            print("Fixed!")
        else:
            print("Block not found exactly")
            print(f"Expected: {repr(content[idx:idx+200])}")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
    f.write(content)