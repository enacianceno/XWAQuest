with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

init_start = content.find('int VrStereo_Init(int w, int h) {')
render_eye = content.find('AeronTexture *VrStereo_RenderEye')

print('VrStereo_Init starts at:', init_start)
print('VrStereo_RenderEye starts at:', render_eye)

between = content[init_start:render_eye]
print(f'Braces in VrStereo_Init region: open={between.count("{")}, close={between.count("}")}')

# Check total file
print(f'Total file braces: open={content.count("{")}, close={content.count("}")}')