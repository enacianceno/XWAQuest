with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    lines = f.readlines()

# Look at lines around 1240-1255
for i in range(1235, 1260):
    print(f'{i}: {lines[i].rstrip()}')