Texture2D g_VisualizeTexture : register(t0);
SamplerState g_PointSampler : register(s0); 

float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    return g_VisualizeTexture.Sample(g_PointSampler, uv);
}