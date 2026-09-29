with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

# Fix the indentation and structure
old = '''        }
        
                /* Flat pipeline creation SKIPPED: embedded SPIR-V shaders cause
         * vkCreateGraphicsPipelines to hang on Meta Quest. V10 pipeline (shadercross)
         * works and is used for triangle draw. */
VrLog("VRPROBE init VrStereo_Init OK %dx%d", w, h);
    SDL_Log("VRPROBE stereo scenes ready %dx%d", w, h);
    return 1;
}

AeronTexture *VrStereo_RenderEye'''

new = '''        }
        
        /* Flat pipeline creation SKIPPED: embedded SPIR-V shaders cause
         * vkCreateGraphicsPipelines to hang on Meta Quest. V10 pipeline (shadercross)
         * works and is used for triangle draw. */
    VrLog("VRPROBE init VrStereo_Init OK %dx%d", w, h);
    SDL_Log("VRPROBE stereo scenes ready %dx%d", w, h);
    return 1;
}

AeronTexture *VrStereo_RenderEye'''

if old in content:
    content = content.replace(old, new)
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Pattern not found - trying alternative")
    # Just fix the indentation of the comment and log lines
    content = content.replace(
        '        /* Flat pipeline creation SKIPPED: embedded SPIR-V shaders cause\n         * vkCreateGraphicsPipelines to hang on Meta Quest. V10 pipeline (shadercross)\n         * works and is used for triangle draw. */\nVrLog("VRPROBE init VrStereo_Init OK %dx%d", w, h);\n    SDL_Log("VRPROBE stereo scenes ready %dx%d", w, h);\n    return 1;\n}\n\nAeronTexture *VrStereo_RenderEye',
        '        /* Flat pipeline creation SKIPPED: embedded SPIR-V shaders cause\n         * vkCreateGraphicsPipelines to hang on Meta Quest. V10 pipeline (shadercross)\n         * works and is used for triangle draw. */\n    VrLog("VRPROBE init VrStereo_Init OK %dx%d", w, h);\n    SDL_Log("VRPROBE stereo scenes ready %dx%d", w, h);\n    return 1;\n}\n\nAeronTexture *VrStereo_RenderEye'
    )
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
        f.write(content)
    print("Fixed with alternative!")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
    f.write(content)