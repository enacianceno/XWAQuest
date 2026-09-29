with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

# Fix 1: Add forward declaration for create_swapchains before VrXr_Init
old = 'int VrXr_Init(SDL_GPUDevice *device) {'
new = '''static int create_swapchains(SDL_GPUDevice *device);

int VrXr_Init(SDL_GPUDevice *device) {'''

if 'int VrXr_Init(SDL_GPUDevice *device) {' in content and 'static int create_swapchains(SDL_GPUDevice *device);' not in content:
    content = content.replace('int VrXr_Init(SDL_GPUDevice *device) {', new)
    print("Added forward declaration")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
    f.write(content)

# Also remove the duplicate static declaration
if 'static int create_swapchains(SDL_GPUDevice *device);\n\nstatic int create_swapchains(SDL_GPUDevice *device) {' in content:
    content = content.replace(
        'static int create_swapchains(SDL_GPUDevice *device);\n\nstatic int create_swapchains(SDL_GPUDevice *device) {',
        'static int create_swapchains(SDL_GPUDevice *device) {'
    )
    print("Removed duplicate declaration")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
    f.write(content)

print("Done")