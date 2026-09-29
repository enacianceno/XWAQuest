with open('C:/OpenXWA/XWAQuest/vr-probe/vr_main.c', 'r') as f:
    content = f.read()

old = '            VrLog("VRPROBE frame %u blit eye%d end", stereo, i);\n            }\n            if (frameOk) {\n                Uint64 tSub = SDL_GetTicks();\n                VrLog("VRPROBE frame %u gpu-submit begin", stereo);'

new = '''            VrLog("VRPROBE frame %u blit eye%d end", stereo, i);
            }
            /* W3: Schedule readback of scene_tex and s_present for both eyes */
            if (in_w3 && frameOk) {
                AeronTexture *scene_tex[2];
                for (int i = 0; i < 2; i++) {
                    scene_tex[i] = VrStereo_GetSceneTexture(i);
                    if (scene_tex[i]) {
                        VrReadback_Schedule(g_vrReadback, i, scene_tex[i], sample_ndc, VR_READBACK_NUM_SAMPLES, cmd);
                    }
                    VrReadback_Schedule(g_vrReadback, i, s_present[i], sample_ndc, VR_READBACK_NUM_SAMPLES, cmd);
                }
            }
            if (frameOk) {
                Uint64 tSub = SDL_GetTicks();
                VrLog("VRPROBE frame %u gpu-submit begin", stereo);'''

if old in content:
    content = content.replace(old, new)
    with open('C:/OpenXWA/XWAQuest/vr-probe/vr_main.c', 'w') as f:
        f.write(content)
    print('Replacement successful!')
else:
    print('Old string not found!')
    idx = content.find('VrLog("VRPROBE frame %u blit eye%d end"')
    if idx >= 0:
        print('Found at index:', idx)
        print(repr(content[idx:idx+200]))
    else:
        print('Not found at all')