struct VertexInput
{
    float3 position : POSITION;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD;
};

struct vertex_output
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

vertex_output main(VertexInput input)
{
    vertex_output output;
    output.position = float4(input.position, 1.0f);
    output.texcoord = input.texcoord;
    return output;
}