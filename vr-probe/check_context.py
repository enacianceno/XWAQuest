with open('C:/OpenXWA/XWAQuest/vr-probe/vr_main.c', 'r') as f:
    content = f.read()

idx = content.find('VrLog("VRPROBE frame %u blit eye%d end"')
if idx >= 0:
    print('Found at index:', idx)
    print('Context (200 chars):')
    print(repr(content[idx:idx+200]))
else:
    print('Not found')