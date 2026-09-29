with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

init_start = content.find('int VrStereo_Init(int w, int h) {')
init_end = content.find('return 1;', init_start) + 8
brace_end = content.find('}', content.find('return 1;', init_start))
print('VrStereo_Init ends at:', brace_end)

init_body = content[init_start:brace_end+1]
print(f'VrStereo_Init braces: open={init_body.count("{")}, close={init_body.count("}")}')

# Check total file
print(f'Total file braces: open={content.count("{")}, close={content.count("}")}')