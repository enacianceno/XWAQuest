with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

# V10 init ends at s_ct_diag
v10_end = content.find('SDL_GPUColorTargetDescription s_ct_diag;')
vr_render_eye = content.find('AeronTexture *VrStereo_RenderEye')

between = content[v10_end:content.find('AeronTexture *VrStereo_RenderEye')]
print(f'Between V10 end and VrStereo_RenderEye: open={between.count("{")}, close={between.count("}")}')

# Print what's in that region
print('Content between V10 end and VrStereo_RenderEye:')
print(repr(between[:500]))