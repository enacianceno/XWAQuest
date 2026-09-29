with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'r') as f:
    content = f.read()

# Fix the extra braces - replace "}\n}\n}\n\nAeronTexture" with "}\n\nAeronTexture"
old = '}\n}\n}\n\nAeronTexture'
new = '}\n\nAeronTexture'

if old in content:
    content = content.replace(old, new)
    with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
        f.write(content)
    print("Fixed!")
else:
    print("Pattern not found")
    idx = content.find('#endif')
    if idx >= 0:
        print(f"Found #endif at {idx}")
        print(repr(content[idx:idx+200]))
    else:
        print("#endif not found")

with open('C:\\OpenXWA\\XWAQuest\\vr-probe\\vr_stereo.c', 'w') as f:
    f.write(content)