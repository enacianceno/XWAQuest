with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

# Remove duplicate accessor functions (keep the first occurrence)
# Find and remove the second set of accessor functions

# Find the second occurrence of the accessor functions
idx1 = content.find('int VrXr_ShouldQuit(void) { return s_shouldQuit; }\nint VrXr_ViewCount(void) { return (int)s_viewCount; }')
if content.count('int VrXr_ShouldQuit(void) { return s_shouldQuit; }\nint VrXr_ViewCount(void) { return (int)s_viewCount; }') > 1:
    # Find the second occurrence
    idx1 = content.find('int VrXr_ShouldQuit(void) { return s_shouldQuit; }')
    idx2 = content.find('int VrXr_ShouldQuit(void) { return s_shouldQuit; }', idx1 + 1)
    if idx2 > idx1:
        # Find the end of the second block
        idx3 = content.find('static int create_swapchains', idx2)
        if idx3 > idx2:
            # Remove the duplicate block
            content = content[:idx2] + content[idx3:]
            print("Removed duplicate accessor functions")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
    f.write(content)
print("Done")