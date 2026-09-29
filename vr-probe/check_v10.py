with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

v10_start = content.find('VRPROBE DIAG v10 init begin')
ct_diag = content.find('SDL_GPUColorTargetDescription s_ct_diag;')

v10_block = content[v10_start:content.find('SDL_GPUColorTargetDescription s_ct_diag;')]

print(f'V10 block braces: open={v10_block.count("{")}, close={v10_block.count("}")}')

# Trace braces in V10 block
balance = 0
for i, ch in enumerate(v10_block):
    if ch == '{':
        print(f'  {{ at offset {i}: ...{v10_block[max(0,i-30):i+30]}...')
    elif ch == '}':
        print(f'  }} at offset {i}: ...{v10_block[max(0,i-30):i+30]}...')