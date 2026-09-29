with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

# Find and replace the entire flat pipeline section with a simple comment
# From "#if 0" to "#endif" inclusive
start_idx = content.find('#if 0\n/* Flat pipeline creation (independent of V10) */')
if start_idx >= 0:
    # Find the matching #endif
    end_idx = content.find('#endif\nVrLog("VRPROBE init VrStereo_Init OK', start_idx)
    if end_idx >= 0:
        end_idx = content.find('\n', end_idx) + 1  # Include the newline after #endif
        print(f"Found block from {start_idx} to {end_idx}")
        
        replacement = '''        /* Flat pipeline creation SKIPPED: embedded SPIR-V shaders cause
         * vkCreateGraphicsPipelines to hang on Meta Quest. V10 pipeline (shadercross)
         * works and is used for triangle draw. */
'''
        content = content[:start_idx] + replacement + content[end_idx:]
        with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
            f.write(content)
        print("Fixed!")
    else:
        print("End marker not found")
else:
    print("Start marker not found")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
    f.write(content)