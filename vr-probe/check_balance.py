with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

# Find VrStereo_Init
init_start = content.find('int VrStereo_Init(int w, int h) {')
if init_start >= 0:
    # Find the matching closing brace for the function
    balance = 0
    in_function = False
    for i in range(init_start, len(content)):
        if content[i] == '{':
            if not in_function:
                in_function = True
            balance += 1
        elif content[i] == '}':
            balance -= 1
            if in_function and balance == 0:
                print(f'Function ends at position {i}')
                print(f'Context: ...{content[i-50:i+50]}...')
                break
    print(f'Final balance: {balance}')
else:
    print('Function not found')