with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

# Find the extra } in the flat pipeline creation block
v10_end = content.find('SDL_GPUColorTargetDescription s_ct_diag;')
vr_render_eye = content.find('AeronTexture *VrStereo_RenderEye')

# Print the region with brace tracking
segment = content[v10_end:vr_render_eye]
balance = 0
for i, ch in enumerate(segment):
    if ch == '{':
        balance += 1
        print(f'  {{ at {i}: balance={balance}')
    elif ch == '}':
        balance -= 1
        print(f'  }} at {i}: balance={balance}')
        if balance < 0:
            print(f'  *** NEGATIVE BALANCE at {i}! ***')
            print(f'  Context: ...{segment[max(0,i-50):i+50]}...')
            break