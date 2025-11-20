cbuffer CameraConstants : register(b0)
{
    float4x4 g_InverseView;
    float4x4 g_InverseProjection;
    float3 g_CameraPosition;
};

cbuffer CloudConstants : register(b1)
{
    float3 g_LightDirection; // Direction to the sun
    float g_DensityMultiplier;

    float3 g_CloudColor; // Base scattering color
    float g_Absorption; // Absorption factor

    float g_StepSize; // Distance between samples

    float3 g_VolumeMin; // Cloud volume bounds
    float3 g_VolumeMax;
    
    int g_NumSteps;
};


struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
};
		
VS_OUTPUT main(uint vertID : SV_VertexID)
{
    VS_OUTPUT output;
    
    // Generate a full-screen triangle
    float2 texcoord = float2((vertID << 1) & 2, vertID & 2);
    output.position = float4(texcoord * 2.0f - 1.0f, 0.0f, 1.0f);
    output.position.y = -output.position.y;

    // Unproject to get the world-space position on the far plane
    float4 world = mul(g_InverseProjection, output.position);
    world /= world.w;
    world = mul(g_InverseView, world);
    
    output.worldPos = world.xyz;
    
    return output;
}