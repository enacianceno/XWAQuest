with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

start_idx = content.find('/* Vertex input: 1 buffer, stride=12 (vec3), location 0 FLOAT3 */')
end_log_idx = content.find('VrLog("VRPROBE DIAG v10 init end");', start_idx)
return_idx = content.find('return 1;', end_log_idx)

print(f'Start: {start_idx}')
print(f'End log: {end_log_idx}')
print(f'Return: {return_idx}')

if return_idx >= 0:
    brace_count = 0
    actual_end = -1
    for i in range(return_idx, len(content)):
        if content[i] == '{':
            brace_count += 1
        elif content[i] == '}':
            brace_count -= 1
            if brace_count == 0:
                actual_end = i + 1
                break
        if i > return_idx + 100:
            break
    
    if actual_end >= 0:
        print(f'Actual end: {actual_end}')
        # Show context
        print(f'Context: {repr(content[actual_end-20:actual_end+20])}')
    else:
        print('Could not find end')