with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

# Remove the entire flat pipeline block from line 165 "    {" to line 407 "        }"
# Find the start: "    {" after "v9.7: Create flat-color diagnostic pipeline"
start_marker = '    /* v9.7: Create flat-color diagnostic pipeline.\n     * Vertex input: only location 0 (pos, float3) from AeronGltfVertex.\n     * Stride = sizeof(AeronGltfVertex) = 56 bytes.\n     * Depth test OFF, depth write OFF, cull NONE, blend OFF. */\n    {'
start_idx = content.find(start_marker)
if start_idx >= 0:
    print(f"Found start at {start_idx}")
    
    # Find the matching closing brace for the flat pipeline block
    # It should be after all diagnostic tests. Look for "        }" followed by V10 test
    # The flat pipeline block ends before "        /* DIAGNOSTIC TEST 10:"
    end_marker = '        /* DIAGNOSTIC TEST 10: Create and store the V9 pipeline'
    end_idx = content.find(end_marker, start_idx)
    if end_idx >= 0:
        # Find the closing brace before this marker
        # Search backwards from end_idx for "        }"
        brace_idx = content.rfind('        }', start_idx, end_idx)
        if brace_idx >= 0:
            # Include the newline after the brace
            end_idx = content.find('\n', brace_idx) + 1
            print(f"Found block from {start_idx} to {end_idx}")
            
            replacement = '''    /* Flat pipeline + diagnostic tests v4-v10 REMOVED:
     * Embedded SPIR-V shaders cause vkCreateGraphicsPipelines to hang on Meta Quest.
     * V10 pipeline (shadercross) works and is used for triangle draw. */
'''
            content = content[:start_idx] + replacement + content[end_idx:]
            with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
                f.write(content)
            print("Fixed!")
        else:
            print("Closing brace not found")
    else:
        print("End marker not found")
else:
    print("Start marker not found")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
    f.write(content)