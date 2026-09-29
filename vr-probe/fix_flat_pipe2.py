with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

# Find the flat pipeline creation block and wrap it in #if 0
import re

# Find the start of the flat pipeline creation
start_marker = '/* Flat pipeline creation (independent of V10) */'
start_idx = content.find(start_marker)
if start_idx >= 0:
    print(f"Found start at {start_idx}")
    
    # Find the end of the flat pipeline block (before "VrLog(\"VRPROBE init VrStereo_Init OK")
    end_marker = 'VrLog("VRPROBE init VrStereo_Init OK'
    end_idx = content.find(end_marker, start_idx)
    if end_idx >= 0:
        print(f"Found end at {end_idx}")
        
        # Extract the block
        block = content[start_idx:end_idx]
        print(f"Block length: {len(block)}")
        print(f"Block start: {repr(block[:100])}")
        print(f"Block end: {repr(block[-100:])}")
        
        # Wrap in #if 0 ... #endif
        new_block = '#if 0\n' + block + '#endif\n'
        
        content = content[:start_idx] + new_block + content[end_idx:]
        
        with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
            f.write(content)
        print("Fixed!")
    else:
        print("End marker not found")
else:
    print("Start marker not found")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
    f.write(content)