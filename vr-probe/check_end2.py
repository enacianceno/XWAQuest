with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    lines = f.readlines()

print('Total lines:', len(lines))
print('Last 30 lines:')
for i, line in enumerate(lines[-30:], len(lines) - 29):
    print(f'{i}: {line.rstrip()}')

# Find the return 1; and closing braces near the end of VrStereo_Init
for i, line in enumerate(lines):
    if 'VRPROBE init VrStereo_Init OK' in line:
        print(f'Line {i}: {line.rstrip()}')
    if 'return 1;' in line and 'VrStereo_Init' in ''.join(lines[max(0,i-10):i+1]):
        print(f'Line {i}: {line.rstrip()}')
    if line.strip() == '}' and i > 2400:
        context = ''.join(lines[max(0,i-5):i+1])
        if 'return 1' in context:
            print(f'Line {i}: {line.rstrip()} (after return)')