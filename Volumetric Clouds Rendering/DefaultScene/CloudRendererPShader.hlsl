
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
    int g_NumSteps;
    float _padding1, _padding2;

    float3 g_VolumeMin; // Cloud volume bounds
    float _padding3;
    float3 g_VolumeMax;
    float _padding4;
};


Texture3D g_NoiseTexture : register(t0);
SamplerState g_Sampler : register(s0);

struct VS_OUTPUT
{
    float4 position : SV_POSITION;
    float3 worldPos : TEXCOORD0;
};

bool RayAABBIntersect(float3 rayOrigin, float3 rayDir, float3 boxMin, float3 boxMax, out float tMin, out float tMax)
{
    float3 invDir = 1.0f / rayDir;
    float3 t0s = (boxMin - rayOrigin) * invDir;
    float3 t1s = (boxMax - rayOrigin) * invDir;

    float3 tSmaller = min(t0s, t1s);
    float3 tLarger = max(t0s, t1s);

    tMin = max(max(tSmaller.x, tSmaller.y), tSmaller.z);
    tMax = min(min(tLarger.x, tLarger.y), tLarger.z);

    return (tMax >= tMin) && (tMax > 0.0f);
}
float GetLightEnergy(float3 samplePos)
{
    int lightSteps = 16; // Fewer steps are usually okay for shadows
    float lightStepSize = 15.0f;
    
    float lightTransmittance = 1.0f;
    
    [loop]
    for (int i = 1; i < lightSteps; i++) // Start at i=1 to avoid self-shadowing
    {
        float3 currentLightPos = samplePos + g_LightDirection * (float) i * lightStepSize;

        // Check if we are still inside the main cloud volume
        if (any(currentLightPos < g_VolumeMin) || any(currentLightPos > g_VolumeMax))
            break;
            
        float3 texCoord = saturate((currentLightPos - g_VolumeMin) / (g_VolumeMax - g_VolumeMin));
        float density = g_NoiseTexture.SampleLevel(g_Sampler, texCoord, 0).r;
        density *= g_DensityMultiplier;
        
        // Accumulate transmittance along the light ray
        lightTransmittance *= exp(-density * g_Absorption * lightStepSize);

        if (lightTransmittance < 0.01f)
            break;
    }
    
    return lightTransmittance;
}


float4 main(VS_OUTPUT input) : SV_TARGET
{
    float3 rayDir = normalize(input.worldPos - g_CameraPosition);
    float3 rayOrigin = g_CameraPosition;
    
    float tEnter, tExit;
    if (!RayAABBIntersect(rayOrigin, rayDir, g_VolumeMin, g_VolumeMax, tEnter, tExit))
    {
        // When a ray misses the box, it should be fully transparent
        return float4(0, 0, 0, 0);
    }
    
    tEnter = max(tEnter, 0.0f);

    float transmittance = 1.0f;
    float3 accumulatedColor = 0.0f;
    
    float t = tEnter;
    [loop]
    for (int i = 0; i < g_NumSteps; ++i)
    {
        if (t > tExit || transmittance < 0.01f)
            break;

        float3 currentPos = rayOrigin + rayDir * t;
        float3 texCoord = saturate((currentPos - g_VolumeMin) / (g_VolumeMax - g_VolumeMin));
        float density = g_NoiseTexture.SampleLevel(g_Sampler, texCoord, 0).r * g_DensityMultiplier;

        if (density > 0.001f)
        {
            // --- NEW LIGHTING CALCULATION ---
            // Get the amount of light reaching this point
            float lightEnergy = GetLightEnergy(currentPos);
            
            // For now, let's just add a simple ambient term so shadows aren't pure black
            lightEnergy = lightEnergy * 0.8 + 0.2;
            
            float3 scattering = g_CloudColor * lightEnergy;
            // --- END OF NEW LIGHTING ---

            float stepTransmittance = exp(-density * g_Absorption * g_StepSize);

            float3 contrib = scattering * transmittance * (1.0f - stepTransmittance) * density;
            accumulatedColor += contrib;

            transmittance *= stepTransmittance;
        }

        t += g_StepSize;
    }

    float3 finalColor = accumulatedColor;
    float alpha = 1.0f - transmittance;

    // Blend the cloud color over a clear background. The Render Target's clear color will be the sky.
    return float4(finalColor*1.5, alpha);
}