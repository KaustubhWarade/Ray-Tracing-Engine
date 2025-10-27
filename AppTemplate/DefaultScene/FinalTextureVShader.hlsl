cbuffer ConstantBufferData : register(b0)
{
    float4x4 worldMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
};

struct vertex_input
{
    float4 pos : POSITION;
    float3 norm : NORMAL;
    float2 tex : TEXCOORD;
    float4 tangent : TANGENT;
};

struct vertex_output
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};
		
vertex_output main(vertex_input input)
{
    vertex_output output;
    
    output.position = mul(projectionMatrix, mul(viewMatrix, mul(worldMatrix, input.pos)));
    
    output.texcoord = input.tex;
    
    return output;
}