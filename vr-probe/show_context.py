with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

vr_render_eye = content.find('AeronTexture *VrStereo_RenderEye')

# Find the extra } before VrStereo_RenderEye
# We know it's around offset 2299 from the V10 end
v10_end = content.find('SDL_GPUColorTargetDescription s_ct_diag;')
vr_render_eye = content.find('AeronTexture *VrStereo_RenderEye')

segment = content[content.find('SDL_GPUColorTargetDescription s_ct_diag;'):vr_render_eye]

# Print with line numbers
lines = content[:vr_render_eye].split('\n')
for i, line in enumerate(lines[-50:], len(lines) - 50):
    print(f'{len(lines) - 50 + i}: {line.rstrip()}')