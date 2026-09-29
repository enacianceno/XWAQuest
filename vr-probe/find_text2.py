with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_openxr.c', 'r') as f:
    content = f.read()

# Find the exact text
search_text = 'VRPROBE XR session+space ready, waiting for READY event'
idx = content.find(search_text)
if idx >= 0:
    print(f"Found at {idx}")
    print(repr(content[idx:idx+150]))
else:
    print("Not found")

# Also find VrXr_PollEvents
idx2 = content.find('void VrXr_PollEvents')
if idx2 >= 0:
    print(f"VrXr_PollEvents at {idx2}")
    print(repr(content[idx2:idx2+100]))