cbuffer SliceParams : register(b0)
{
    float g_SliceDepth; 
    int g_VizMode;
};

Texture3D g_NoiseTexture : register(t0);

SamplerState g_Sampler : register(s0);


float4 main(float4 pos : SV_POSITION, float2 uv : TEXCOORD0) : SV_TARGET
{
    float3 sampleCoord = float3(uv.x, uv.y, g_SliceDepth);
    float4 color = g_NoiseTexture.Sample(g_Sampler, sampleCoord);
    
    switch (g_VizMode)
    {
        case 0: // Grayscale
            float gray;
            gray = dot(color.rgb, float3(0.299, 0.587, 0.114));
            return float4(gray, gray, gray, 1.0f);
        case 1: // Red
            return float4(color.r, 0, 0, 1.0f);
        case 2: // Green
            return float4(0, color.g, 0, 1.0f);
        case 3: // Blue
            return float4(0, 0, color.b, 1.0f);
        case 4: // Alpha
            return float4(color.a, color.a, color.a, 1.0f);

    }
    return color;
}