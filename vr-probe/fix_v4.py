with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

# Fix the VrXr_Init function to have proper structure
# Find the end of VrXr_Init and ensure proper closing
# Then fix VrXr_PollEvents

# First, fix the s_running assignment to be at function scope level
content = content.replace(
    's_running = 1;\n        SDL_Log("VRPROBE session begun");\n        if (!create_swapchains(device)) {\n            return 0;\n        }\n    }\n    return 1;\n}\n\nvoid VrXr_PollEvents(SDL_GPUDevice *device) {',
    '''s_running = 1;
        SDL_Log("VRPROBE session begun");
        if (!create_swapchains(device)) {
            return 0;
        }
    }
    return 1;
}

void VrXr_PollEvents(SDL_GPUDevice *device) {''')

if 'SDL_Log("VRPROBE xrBeginSession failed result=%d", (int)r);' in content and 'return 0;\n}\n}\n\nvoid VrXr_PollEvents' not in content:
    # Apply the fix
    content = content.replace(
        '        s_running = 1;\n        SDL_Log("VRPROBE session begun");\n        if (!create_swapchains(device)) {\n            return 0;\n        }\n    }\n    return 1;\n}\n\nvoid VrXr_PollEvents(SDL_GPUDevice *device) {',
        '''        s_running = 1;
        SDL_Log("VRPROBE session begun");
        if (!create_swapchains(device)) {
            return 0;
        }
    }
    return 1;
}

void VrXr_PollEvents(SDL_GPUDevice *device) {'''
    )

    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'w') as f:
        f.write(content)
    print("Fixed!")