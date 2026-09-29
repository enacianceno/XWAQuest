with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

start_idx = content.find('/* Vertex input: 1 buffer, stride=12 (vec3), location 0 FLOAT3 */')
end_log_idx = content.find('VrLog("VRPROBE DIAG v10 init end");', start_idx)
return_idx = content.find('return 1;', end_log_idx)

print(f'Start: {start_idx}')
print(f'End log: {end_log_idx}')
print(f'Return: {return_idx}')

# Show context after return
print(f'Context after return: {repr(content[return_idx:return_idx+200])}')