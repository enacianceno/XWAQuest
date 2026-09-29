with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

# Find the xrBeginSession call and replace the error handling
idx = content.find('XrResult r = s_xrBeginSession(s_session, &begin);')
if idx >= 0:
    print(f"Found xrBeginSession call at {idx}")
    # Find the end of the error handling block
    idx2 = content.find('s_running = 1;\n        SDL_Log("VRPROBE session begun");', idx)
    if idx2 >= 0:
        print(f"Found end at {idx2}")
        print(repr(content[idx:idx+500]))
    else:
        print("End pattern not found")

# Let's just replace the whole block from "XrResult r = s_xrBeginSession" to "return 1;}"
import re
pattern = r'XrResult r = s_xrBeginSession\(s_session, &begin\);\s+if \(r != XR_SUCCESS\) \{\s+SDL_Log\("VRPROBE xrBeginSession failed result=%d", \(int\)r\);\s+return 0;\s+}\s+s_running = 1;\s+SDL_Log\("VRPROBE session begun"\);\s+}\s+return 1;'
match = re.search(pattern, content, re.DOTALL)
if match:
    print("Found pattern")
    print(match.group(0)[:500])
else:
    print("Pattern not found")
    # Let's just see the relevant section
    idx = content.find('XrResult r = s_xrBeginSession(s_session, &begin);')
    if idx >= 0:
        print(content[idx:idx+500])