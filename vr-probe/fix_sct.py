with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

# Fix the s_ct redeclaration issue
old_text = '''VrLog("VRPROBE DIAG v10 init end");
            fflush(g_vrLog);
        }

        /* Flat pipeline creation (independent of V10) */
        SDL_GPUColorTargetDescription s_ct;
        memset(&s_ct, 0, sizeof s_ct);
        s_ct.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        VrLog("VRPROBE init flat pipeline create begin fmt=%d", (int)s_ct.format);
        fflush(g_vrLog);
        {'''

new_text = '''VrLog("VRPROBE DIAG v10 init end");
            fflush(g_vrLog);
        }

        /* Flat pipeline creation (independent of V10) */
        memset(&s_ct, 0, sizeof s_ct);
        s_ct.format = SDL_GPU_TEXTUREFORMAT_R16G16B16A16_FLOAT;
        VrLog("VRPROBE init flat pipeline create begin fmt=%d", (int)s_ct.format);
        fflush(g_vrLog);
        {'''

content = content.replace(old_text, new_text)

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
    f.write(content)

print("Fixed s_ct redeclaration")