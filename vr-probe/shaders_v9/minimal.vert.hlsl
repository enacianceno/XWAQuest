struct VertexOutput {
    float4 position : SV_Position;
};

VertexOutput main(float3 pos : POSITION0) {
    VertexOutput output;
    output.position = float4(pos, 1.0);
    return output;
}