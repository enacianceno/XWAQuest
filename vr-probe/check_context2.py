with open('C:/OpenXWA/XWAQuest/vr-probe/vr_main.c', 'r') as f:
    content = f.read()

idx = content.find('VrXr_EndFrame(displayTime, poses, fovs, frameOk);')
if idx >= 0:
    print('Found at index:', idx)
    print('Context (300 chars):')
    print(repr(content[idx:idx+300]))
else:
    print('Not found')