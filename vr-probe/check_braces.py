with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

funcs = [
    ('VrStereo_RenderEye', content.find('AeronTexture *VrStereo_RenderEye')),
    ('VrStereo_GetSceneTexture', content.find('AeronTexture *VrStereo_GetSceneTexture')),
    ('VrStereo_DrawFlatDiagnostic', content.find('void VrStereo_DrawFlatDiagnostic')),
    ('VrStereo_DrawTriangle', content.find('int VrStereo_DrawTriangle')),
    ('VrStereo_Shutdown', content.find('void VrStereo_Shutdown')),
]

print("Function positions:")
for name, pos in funcs:
    if pos >= 0:
        print(f'{name} at: {pos}')
    else:
        print(f'{name}: NOT FOUND')

# Check braces between functions
prev = 3724  # end of VrStereo_Init
for name, pos in funcs:
    if pos > 0:
        segment = content[prev:pos]
        open_b = segment.count('{')
        close_b = segment.count('}')
        print(f'Braces between prev and {name}: open={segment.count("{")}, close={segment.count("}")}, balance={open_b - close_b}')
        prev = pos

# Check after last function
segment = content[prev:]
print(f'After last function: open={segment.count("{")}, close={segment.count("}")}, balance={segment.count("{") - segment.count("}")}')

# Total braces
print(f'Total open: {content.count("{")}, close: {content.count("}")}, balance: {content.count("{") - content.count("}")}')