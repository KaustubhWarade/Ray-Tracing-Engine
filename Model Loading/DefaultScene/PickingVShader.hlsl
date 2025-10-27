cbuffer ConstantBufferData : register(b0)
{
    float4x4 worldMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4 lD;
    float4 kD;
    float4 lightPosition;
    uint lightingMode;
    float3 cameraPosition;
};

cbuffer PerDrawConstants : register(b1)
{
    uint materialIndex;
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
    float4 pos : SV_POSITION;
    nointerpolation uint materialIndex : MATERIALINDEX;
};
		
vertex_output main(vertex_input input)
{
    vertex_output output;
    
    float4 worldPos = mul(worldMatrix, input.pos);
    output.pos = mul(projectionMatrix, mul(viewMatrix, worldPos));
    output.materialIndex = materialIndex;
    return output;
}