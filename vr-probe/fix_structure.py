with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

# Fix the structure after the V10 diagnostic test
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
    print("Pattern not found")
    idx = content.find('/* Flat pipeline creation SKIPPED')
    if idx >= 0:
        print(f"Found at {idx}")
        print(repr(content[idx:idx+300]))
    else:
        print("Pattern not found")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
    f.write(content)