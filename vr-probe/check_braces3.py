with open('C:/OpenXWA/XWAQuest/vr-probe/vr_main.c', 'r') as f:
    content = f.read()

idx = content.find('int main(int argc, char **argv) {')
if idx < 0:
    print('Main not found')
    exit()

text = content[idx:]
depth = 0
in_string = False
in_char = False
in_line_comment = False
in_block_comment = False
i = 0
last_depth = 0

while i < len(content) - idx:
    ch = content[idx + i]
    
    prev_depth = depth
    
    if in_line_comment:
        if ch == '\n':
            in_line_comment = False
    elif in_block_comment:
        if ch == '*' and i+1 < len(content) - idx and content[idx + i + 1] == '/':
            in_block_comment = False
            i += 1
    elif in_string:
        if ch == '\\':
            i += 1
        elif ch == '"':
            in_string = False
    elif in_char:
        if ch == '\\':
            i += 1
        elif ch == '\'':
            in_char = False
    else:
        if ch == '/' and i+1 < len(content) - idx and content[idx + i + 1] == '/':
            in_line_comment = True
            i += 1
        elif ch == '/' and i+1 < len(content) - idx and content[idx + i + 1] == '*':
            in_block_comment = True
            i += 1
        elif ch == '"':
            in_string = True
        elif ch == '\'':
            in_char = True
        elif ch == '{':
            depth += 1
            print(f'Depth {depth} at offset {i}: {{')
        elif ch == '}':
            depth -= 1
            print(f'Depth {depth} at offset {i}: }}')
    
    if depth != last_depth:
        last_depth = depth
    
    i += 1

print(f'Final depth: {depth}')