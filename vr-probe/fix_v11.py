with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

# Fix 1: Add forward declaration for create_swapchains before VrXr_Init
old = 'static int create_swapchains(SDL_GPUDevice *device) {'
new = 'static int create_swapchains(SDL_GPUDevice *device);\n\nstatic int create_swapchains(SDL_GPUDevice *device) {'
if 'static int create_swapchains(SDL_GPUDevice *device) {' in content:
    content = content.replace('static int create_swapchains(SDL_GPUDevice *device) {', new)
    print("Added forward declaration")

# Fix 2: Remove duplicate VrXr_PollEvents if any
# Check for duplicate
count = content.count('void VrXr_PollEvents(SDL_GPUDevice *device) {')
if content.count('void VrXr_PollEvents(SDL_GPUDevice *device) {') > 1:
    # Find and remove duplicate
    idx1 = content.find('void VrXr_PollEvents(SDL_GPUDevice *device) {')
    idx2 = content.find('void VrXr_PollEvents(SDL_GPUDevice *device) {', idx1 + 1)
    if idx2 > idx1:
        # Remove the second occurrence
        end_idx = content.find('\n}', idx2) + 2
        content = content[:idx2] + content[end_idx:]
        print("Removed duplicate VrXr_PollEvents")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
    f.write(content)
print("Fixed!")