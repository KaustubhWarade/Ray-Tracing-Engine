cbuffer ConstantBufferData : register(b0)
{
    float4x4 worldMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
};

struct vertex_output
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

vertex_output main(float4 pos : POSITION, float2 tex : TEXCOORD)
{
    vertex_output output;
    output.position = mul(projectionMatrix, mul(viewMatrix, mul(worldMatrix, pos)));
    output.texcoord = tex;
    return output;
}