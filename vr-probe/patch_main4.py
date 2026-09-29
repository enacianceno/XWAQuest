with open('C:/OpenXWA/XWAQuest/vr-probe/vr_main.c', 'r') as f:
    content = f.read()

old = """        VrXr_EndFrame(displayTime, poses, fovs, frameOk);
        if (!frameOk) {
            consecSkips++;
            VrLog("VRPROBE stereo frame %u SKIPPED (no layers) consec=%u", stereo,
                  consecSkips);
        } else {
            consecSkips = 0;
            submitted++;
        }"""

new = """        VrXr_EndFrame(displayTime, poses, fovs, frameOk);
        /* W3: Collect readback results for scene_tex and s_present */
        if (in_w3) {
            for (int i = 0; i < 2; i++) {
                uint8_t rgba[VR_READBACK_NUM_SAMPLES * 4];
                int collected = VrReadback_Collect(g_vrReadback, i, rgba);
                if (collected > 0) {
                    for (int s = 0; s < collected; s++) {
                        float ndc_x = g_vrReadback->samples[i][s].ndc_x;
                        float ndc_y = g_vrReadback->samples[i][s].ndc_y;
                        uint8_t r = rgba[s * 4 + 0];
                        uint8_t g = rgba[s * 4 + 1];
                        uint8_t b = rgba[s * 4 + 2];
                        uint8_t a = rgba[s * 4 + 3];
                        VrLog("VRPROBE readback eye=%d sample=%d ndc=(%.3f,%.3f) rgba=(%u,%u,%u,%u)",
                              i, s, g_vrReadback->samples[i][s].ndc_x, g_vrReadback->samples[i][s].ndc_y,
                              rgba[s * 4 + 0], rgba[s * 4 + 1], rgba[s * 4 + 2], rgba[s * 4 + 3]);
                    }
                }
            }
        if (!frameOk) {
            consecSkips++;
            VrLog("VRPROBE stereo frame %u SKIPPED (no layers) consec=%u", stereo,
                  consecSkips);
        } else {
            consecSkips = 0;
            submitted++;
        }"""

if old in content:
    content = content.replace(old, new)
    with open('C:/OpenXWA/XWAQuest/vr-probe/vr_main.c', 'w') as f:
        f.write(content)
    print('Replacement successful!')
else:
    print('Old string not found!')
    idx = content.find('VrXr_EndFrame(displayTime, poses, fovs, frameOk);')
    if idx >= 0:
        print('Found at index:', idx)
        print(repr(content[idx:idx+400]))
    else:
        print('Not found at all')