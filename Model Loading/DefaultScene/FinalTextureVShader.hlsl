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
    float4 position : SV_POSITION;
    float3 worldPos : WORLDPOS; 
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
    nointerpolation uint materialIndex : MATERIALINDEX;
    float3x3 TBN : TANGENT;
};
		
vertex_output main(vertex_input input)
{
    vertex_output output;
    
    // Calculate world position and normal
    float4 worldPos = mul(worldMatrix, input.pos);
    output.worldPos = worldPos.xyz;
    output.position = mul(projectionMatrix, mul(viewMatrix, worldPos));
    
    // Create TBN matrix for normal mapping
    float3 worldNormal = normalize(mul((float3x3) worldMatrix, input.norm));
    float3 worldTangent = normalize(mul((float3x3) worldMatrix, input.tangent.xyz));
    worldTangent = normalize(worldTangent - dot(worldTangent, worldNormal) * worldNormal);
    float3 worldBitangent = cross(worldNormal, worldTangent) * input.tangent.w;

    output.TBN = float3x3(worldTangent, worldBitangent, worldNormal);
    output.normal = worldNormal;
    // Pass through other attributes
    output.texcoord = input.tex;
    output.materialIndex = materialIndex;
    return output;
}