#!/usr/bin/env python3
"""Generate minimal SPIR-V shaders for the flat-color diagnostic pipeline.
Outputs vr_flat_shaders.h with embedded byte arrays."""
import struct, sys

def encode_str(s):
    """Encode a string as SPIR-V words (null-terminated, 4-byte aligned)."""
    data = s.encode('utf-8') + b'\x00'
    while len(data) % 4:
        data += b'\x00'
    return [struct.unpack('<I', data[i:i+4])[0] for i in range(0, len(data), 4)]

class SPIRVWriter:
    def __init__(self):
        self.words = []
    
    def w(self, v):
        self.words.append(v & 0xFFFFFFFF)
    
    def inst(self, op, result_type, result_id, *operands):
        wc = 3 + len(operands)
        self.w((wc << 16) | op)
        self.w(result_type)
        self.w(result_id)
        for o in operands:
            self.w(o)
    
    def inst_nort(self, result_id, op, *operands):
        """Instruction with result but no result type (variables, labels)."""
        wc = 2 + len(operands)
        self.w((wc << 16) | op)
        self.w(0)
        self.w(result_id)
        for o in operands:
            self.w(o)
    
    def inst_nores(self, op, *operands):
        """Instruction with no result."""
        wc = 1 + len(operands)
        self.w((wc << 16) | op)
        for o in operands:
            self.w(o)
    
    def name(self, target, s):
        enc = encode_str(s)
        wc = 2 + len(enc)
        self.w((wc << 16) | 5)  # OpName
        self.w(0)
        self.w(target)
        for e in enc:
            self.w(e)
    
    def member_name(self, target, idx, s):
        enc = encode_str(s)
        wc = 3 + len(enc)
        self.w((wc << 16) | 6)  # OpMemberName
        self.w(0)
        self.w(target)
        self.w(idx)
        for e in enc:
            self.w(e)
    
    def decorate(self, target, dec, *args):
        self.inst_nores(71, target, dec, *args)
    
    def member_decorate(self, target, member, dec, *args):
        self.inst_nores(72, target, member, dec, *args)
    
    def entry_point(self, model, func_id, name):
        enc = encode_str(name)
        wc = 2 + len(enc)
        self.w((wc << 16) | 15)  # OpEntryPoint
        self.w(model)
        self.w(func_id)
        for e in enc:
            self.w(e)
    
    def to_bytes(self):
        return struct.pack(f'<{len(self.words)}I', *self.words)


# Constants
VOID=1; F32=2; V3=3; V4=4; MAT4=5
UBO_T=6        # struct {mat4, mat4, vec4}
P_UNI=7; P_IN=8; P_OUT=9
U32=10
UBO_V=11; POS_V=12; OUT_V=13    # variables (vert)
FC_V=14                          # fragColor variable (frag)
U32_0=15; U32_1=16; U32_2=17
F32_1=18
VF_T=19; LABEL=20
AC_VP=21; AC_M=22; AC_C=23
LOAD_P=24; COMP_V4=25; MUL1=26; MUL2=27
MAIN=28
LOAD_C=29
BOUND=30


def gen_vert():
    s = SPIRVWriter()
    
    # Header
    s.w(0x07230203)  # magic
    s.w(0x00010000)  # version 1.0
    s.w(0x00080012)  # generator
    s.w(BOUND)
    s.w(0)           # schema
    
    # Capabilities
    s.inst_nores(19, 1)   # OpCapability Shader
    
    # ExtInstImport
    s.inst_nores(11, 43, 0x474C534C)  # "GLSL"
    
    # Memory model
    s.inst_nores(14, 1, 1)  # Logical GLSL450
    
    # EntryPoint Vertex
    s.entry_point(0, MAIN, "main")  # 0 = Vertex
    
    # ExecutionMode: OriginUpperLeft
    s.inst_nores(16, MAIN, 7, 3)
    
    # Source GLSL 450
    s.inst_nores(17, 3, 450)
    
    # Names
    s.name(MAIN, "main")
    s.name(POS_V, "pos")
    s.name(OUT_V, "gl_Position")
    s.name(UBO_T, "UBO")
    s.member_name(UBO_T, 0, "view_proj")
    s.member_name(UBO_T, 1, "model")
    s.member_name(UBO_T, 2, "color")
    
    # Decorates
    s.decorate(OUT_V, 11, 0)    # BuiltIn Position
    s.decorate(POS_V, 30, 0)    # Location 0
    s.decorate(UBO_T, 2)        # Block
    s.member_decorate(UBO_T, 0, 5)           # ColMajor
    s.member_decorate(UBO_T, 0, 7, 16)       # MatrixStride 16
    s.member_decorate(UBO_T, 0, 35, 0)       # Offset 0
    s.member_decorate(UBO_T, 1, 5)
    s.member_decorate(UBO_T, 1, 7, 16)
    s.member_decorate(UBO_T, 1, 35, 64)
    s.member_decorate(UBO_T, 2, 35, 128)
    # Explicit descriptor set and binding for UBO (set 0, binding 0)
    s.decorate(UBO_V, 33, 0)   # Binding 0
    s.decorate(UBO_V, 34, 0)   # DescriptorSet 0
    
    # Types
    s.inst(19, 0, VOID)                    # TypeVoid
    s.inst(22, 0, F32, 32)                 # TypeFloat 32
    s.inst(23, 0, V3, F32, 3)              # TypeVector vec3
    s.inst(23, 0, V4, F32, 4)              # TypeVector vec4
    s.inst(24, 0, MAT4, V4, 4)             # TypeMatrix mat4
    s.inst(30, 0, UBO_T, MAT4, MAT4, V4)   # TypeStruct
    s.inst(32, 0, P_UNI, 2, UBO_T)         # TypePointer Uniform
    s.inst(32, 0, P_IN, 1, V3)             # TypePointer Input
    s.inst(32, 0, P_OUT, 3, V4)            # TypePointer Output
    s.inst(21, 0, U32, 32, 0)              # TypeInt 32 unsigned
    s.inst(33, 0, VF_T, VOID)              # TypeFunction
    
    # Constants
    s.inst(43, F32, F32_1, 0x3F800000)     # float 1.0
    s.inst(43, U32, U32_0, 0)              # uint 0
    s.inst(43, U32, U32_1, 1)              # uint 1
    
    # Variables
    s.inst(65, P_UNI, UBO_V, 2, 0)         # Uniform
    s.inst(65, P_OUT, OUT_V, 3, 0)         # Output
    s.inst(65, P_IN, POS_V, 1, 0)          # Input
    
    # Function main
    s.inst(54, 0, MAIN, VF_T, 0)           # OpFunction
    s.inst(248, 0, LABEL)                   # OpLabel
    
    # Body
    s.inst(65, P_UNI, AC_VP, UBO_V, U32_0) # view_proj
    s.inst(65, P_UNI, AC_M, UBO_V, U32_1)  # model
    s.inst(61, V3, LOAD_P, POS_V)           # load pos
    s.inst(80, V4, COMP_V4, LOAD_P, F32_1)  # vec4(pos, 1.0)
    s.inst(143, V4, MUL1, AC_VP, COMP_V4)   # vp * vec4
    s.inst(143, V4, MUL2, AC_M, MUL1)       # model * (vp * vec4)
    s.inst(62, V4, OUT_V, MUL2)             # store gl_Position
    
    # Return
    s.w((1 << 16) | 253)  # OpReturn
    s.w((1 << 16) | 254)  # OpFunctionEnd
    
    return s.to_bytes()


def gen_vert_no_ubo():
    """Vertex shader WITHOUT UBO - identity transform (pass-through pos)."""
    s = SPIRVWriter()
    
    # Header
    s.w(0x07230203)  # magic
    s.w(0x00010000)  # version 1.0
    s.w(0x00080012)  # generator
    s.w(BOUND)
    s.w(0)           # schema
    
    # Capabilities
    s.inst_nores(19, 1)   # OpCapability Shader
    
    # ExtInstImport
    s.inst_nores(11, 43, 0x474C534C)  # "GLSL"
    
    # Memory model
    s.inst_nores(14, 1, 1)  # Logical GLSL450
    
    # EntryPoint Vertex
    s.entry_point(0, MAIN, "main")  # 0 = Vertex
    
    # ExecutionMode: OriginUpperLeft
    s.inst_nores(16, MAIN, 7, 3)
    
    # Source GLSL 450
    s.inst_nores(17, 3, 450)
    
    # Names
    s.name(MAIN, "main")
    s.name(POS_V, "pos")
    s.name(OUT_V, "gl_Position")
    
    # Decorates
    s.decorate(OUT_V, 11, 0)    # BuiltIn Position
    s.decorate(POS_V, 30, 0)    # Location 0
    
    # Types
    s.inst(19, 0, VOID)                    # TypeVoid
    s.inst(22, 0, F32, 32)                 # TypeFloat 32
    s.inst(23, 0, V3, F32, 3)              # TypeVector vec3
    s.inst(23, 0, V4, F32, 4)              # TypeVector vec4
    s.inst(32, 0, P_IN, 1, V3)             # TypePointer Input
    s.inst(32, 0, P_OUT, 3, V4)            # TypePointer Output
    s.inst(21, 0, U32, 32, 0)              # TypeInt 32 unsigned
    s.inst(33, 0, VF_T, VOID)              # TypeFunction
    
    # Constants
    s.inst(43, F32, F32_1, 0x3F800000)     # float 1.0
    
    # Variables
    s.inst(65, P_OUT, OUT_V, 3, 0)         # Output
    s.inst(65, P_IN, POS_V, 1, 0)          # Input
    
    # Function main
    s.inst(54, 0, MAIN, VF_T, 0)           # OpFunction
    s.inst(248, 0, LABEL)                   # OpLabel
    
    # Body: gl_Position = vec4(pos, 1.0)  -- identity transform
    s.inst(61, V3, LOAD_P, POS_V)           # load pos
    s.inst(80, V4, COMP_V4, LOAD_P, F32_1)  # vec4(pos, 1.0)
    s.inst(62, V4, OUT_V, COMP_V4)          # store gl_Position
    
    # Return
    s.w((1 << 16) | 253)  # OpReturn
    s.w((1 << 16) | 254)  # OpFunctionEnd
    
    return s.to_bytes()


def gen_frag():
    s = SPIRVWriter()
    
    # Header
    s.w(0x07230203)
    s.w(0x00010000)
    s.w(0x00080012)
    s.w(BOUND)
    s.w(0)
    
    s.inst_nores(19, 1)  # Shader
    s.inst_nores(11, 43, 0x474C534C)  # GLSL.std.450
    s.inst_nores(14, 1, 1)  # Logical GLSL450
    
    # EntryPoint Fragment
    s.entry_point(4, MAIN, "main")  # 4 = Fragment
    
    s.inst_nores(16, MAIN, 7, 3)  # OriginUpperLeft
    s.inst_nores(17, 3, 450)      # Source GLSL 450
    
    # Names
    s.name(MAIN, "main")
    s.name(FC_V, "fragColor")
    
    # Decorates
    s.decorate(FC_V, 30, 0)   # Location 0
    
    # Types
    s.inst(19, 0, VOID)
    s.inst(22, 0, F32, 32)
    s.inst(23, 0, V4, F32, 4)
    s.inst(32, 0, P_OUT, 3, V4)
    s.inst(21, 0, U32, 32, 0)
    s.inst(33, 0, VF_T, VOID)
    
    # Constants: hardcoded magenta color (1, 0, 1, 1)
    s.inst(43, F32, F32_1, 0x3F800000)  # 1.0
    s.inst(43, F32, 18, 0x00000000)     # 0.0 (reuse F32_1 for 0? need new constant)
    # We need constants for 0.0 and 1.0
    s.inst(43, F32, 18, 0x00000000)  # 0.0
    
    # Variables
    s.inst(65, P_OUT, FC_V, 3, 0)
    
    # Function main
    s.inst(54, 0, MAIN, VF_T, 0)
    s.inst(248, 0, LABEL)
    
    # Body: fragColor = vec4(1.0, 0.0, 1.0, 1.0)  -- magenta
    # Use OpCompositeConstruct to build vec4
    # F32_1 = 1.0, constant 18 = 0.0
    s.inst(80, V4, 24, F32_1, 18, F32_1, F32_1)  # OpCompositeConstruct vec4(1,0,1,1)
    s.inst(62, V4, FC_V, 24)  # store fragColor
    
    s.w((1 << 16) | 253)
    s.w((1 << 16) | 254)
    
    return s.to_bytes()


def to_c_array(name, data):
    lines = [f"static const unsigned char {name}[] = {{"]
    for i in range(0, len(data), 12):
        chunk = data[i:i+12]
        hex_vals = ", ".join(f"0x{b:02X}" for b in chunk)
        lines.append(f"    {hex_vals},")
    lines.append("};")
    lines.append(f"static const unsigned int {name}_size = {len(data)};")
    return "\n".join(lines)


if __name__ == "__main__":
    vert = gen_vert()
    vert_no_ubo = gen_vert_no_ubo()
    frag = gen_frag()
    
    with open("vr_flat_shaders.h", "w") as f:
        f.write("/* Auto-generated flat-color diagnostic SPIR-V shaders.\n")
        f.write(" * VS (g_flat_vert_spv): gl_Position = view_proj * model * vec4(pos, 1)  [WITH UBO]\n")
        f.write(" * VS (g_flat_vert_no_ubo_spv): gl_Position = vec4(pos, 1)  [NO UBO, identity]\n")
        f.write(" * FS (g_flat_frag_spv): fragColor = vec4(1,0,1,1)  [NO UBO, hardcoded magenta]\n")
        f.write(" */\n")
        f.write("#ifndef VR_FLAT_SHADERS_H\n#define VR_FLAT_SHADERS_H\n\n")
        f.write(to_c_array("g_flat_vert_spv", vert))
        f.write("\n\n")
        f.write(to_c_array("g_flat_vert_no_ubo_spv", vert_no_ubo))
        f.write("\n\n")
        f.write(to_c_array("g_flat_frag_spv", frag))
        f.write("\n\n#endif\n")
    
    print(f"Vertex shader (with UBO): {len(vert)} bytes, {len(vert)//4} words")
    print(f"Vertex shader (no UBO): {len(vert_no_ubo)} bytes, {len(vert_no_ubo)//4} words")
    print(f"Fragment shader: {len(frag)} bytes, {len(frag)//4} words")
    print(f"Output: vr_flat_shaders.h")
