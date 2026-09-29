with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

# Fix the extra braces
old = '''#endif
    VrLog("VRPROBE init VrStereo_Init OK %dx%d", w, h);
    SDL_Log("VRPROBE stereo scenes ready %dx%d", w, h);
    return 1;
}
}
}'''

new = '''#endif
    VrLog("VRPROBE init VrStereo_Init OK %dx%d", w, h);
    SDL_Log("VRPROBE stereo scenes ready %dx%d", w, h);
    return 1;
}'''

if old in content:
    content = content.replace(old, new)
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Pattern not found")
    idx = content.find('#endif')
    if idx >= 0:
        print(f"Found #endif at {idx}")
        print(repr(content[idx:idx+200]))
    else:
        print("#endif not found")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
    f.write(content)