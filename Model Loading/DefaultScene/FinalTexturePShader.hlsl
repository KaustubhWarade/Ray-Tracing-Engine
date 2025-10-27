struct Material
{
    float4 BaseColorFactor;
    
    float3 EmissiveFactor;
    float MetallicFactor;

    float RoughnessFactor;
    float IOR;
    float Transmission;
    int AlphaMode;
    
    float AlphaCutoff;
    uint DoubleSided; 
    uint Unlit;
    float ClearcoatFactor;
    
    float ClearcoatRoughnessFactor;
    float3 SheenColorFactor;
    
    float SheenRoughnessFactor;
    int BaseColorTextureIndex;
    int MetallicRoughnessTextureIndex;
    int NormalTextureIndex;
    
    int OcclusionTextureIndex;
    int EmissiveTextureIndex;
    int ClearcoatTextureIndex;
    int ClearcoatRoughnessTextureIndex;
    
    int ClearcoatNormalTextureIndex;
    int SheenColorTextureIndex;
    int SheenRoughnessTextureIndex;
    int TransmissionTextureIndex;
};

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

StructuredBuffer<Material> g_Materials : register(t0); 
Texture2D g_Textures[] : register(t0, space1); 
SamplerState g_sampler : register(s0);

struct vertex_output
{
    float4 position : SV_POSITION;
    float3 worldPos : WORLDPOS;
    float3 normal : NORMAL;
    float2 texcoord : TEXCOORD0;
    nointerpolation uint materialIndex : MATERIALINDEX;
    float3x3 TBN : TANGENT;
};

#define PI 3.14159265359

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0);
    float NdotH2 = NdotH * NdotH;
	
    float nom = a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return nom / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;
    float nom = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	
    return nom / denom;
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0);
    float NdotL = max(dot(N, L), 0.0);
    float ggx2 = GeometrySchlickGGX(NdotV, roughness);
    float ggx1 = GeometrySchlickGGX(NdotL, roughness);
	
    return ggx1 * ggx2;
}

float3 fresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

float4 main(vertex_output input) : SV_TARGET
{
    // 1. Get the material for this pixel
    Material mat = g_Materials[input.materialIndex];
    
    // 2. Sample Base Textures
    float4 baseColorAlbedo = mat.BaseColorFactor;
    if (mat.BaseColorTextureIndex != -1)
    {
        uint texIdx = NonUniformResourceIndex(mat.BaseColorTextureIndex);
        baseColorAlbedo *= g_Textures[texIdx].Sample(g_sampler, input.texcoord);
    }
    
    if (mat.AlphaMode == 1) 
    {
        if (baseColorAlbedo.a < mat.AlphaCutoff)
        {
            clip(-1); // Discard this pixel
        }
    }
    
    if (mat.Unlit > 0)
    {
        // For unlit, just use the base color and emissive properties
        float3 unlitColor = baseColorAlbedo.rgb + mat.EmissiveFactor;
        
        if (mat.EmissiveTextureIndex != -1)
        {
            uint texIdx = NonUniformResourceIndex(mat.EmissiveTextureIndex);
            unlitColor += g_Textures[texIdx].Sample(g_sampler, input.texcoord).rgb;
        }

        // Apply gamma correction before returning
        unlitColor = pow(unlitColor, 1.0f / 2.2f);
        return float4(unlitColor, baseColorAlbedo.a);
    }
    
    // 3. Get Metallic and Roughness
    float roughness = mat.RoughnessFactor;
    float metallic = mat.MetallicFactor;
    if (mat.MetallicRoughnessTextureIndex != -1)
    {
        uint texIdx = NonUniformResourceIndex(mat.MetallicRoughnessTextureIndex);
        float4 mrSample = g_Textures[texIdx].Sample(g_sampler, input.texcoord);
        roughness *= mrSample.g; 
        metallic *= mrSample.b; 
    }
    
    // 4. Get Surface Normal
    float3 N = normalize(input.normal);
    if (mat.NormalTextureIndex != -1)
    {
        uint texIdx = NonUniformResourceIndex(mat.NormalTextureIndex);
        float3 tangentNormal = g_Textures[texIdx].Sample(g_sampler, input.texcoord).xyz * 2.0 - 1.0;
        N = normalize(mul(tangentNormal, input.TBN));
    }    
    
    // 5. Calculate Indirect/Ambient Illumination
    float ao = 1.0f;
    float3 ambient = float3(0.03, 0.03, 0.03) * baseColorAlbedo.rgb;
    if (mat.OcclusionTextureIndex != -1)
    {
        uint texIdx = NonUniformResourceIndex(mat.OcclusionTextureIndex);
        ao = g_Textures[texIdx].Sample(g_sampler, input.texcoord).r;
        ambient *= ao;
    }
    
    // 6. Get Emmissive data
    float3 emissive = mat.EmissiveFactor;
    if (mat.EmissiveTextureIndex != -1)
    {
        uint texIdx = NonUniformResourceIndex(mat.EmissiveTextureIndex);
        
        emissive *= g_Textures[texIdx].Sample(g_sampler, input.texcoord).rgb;
    }
    
    switch (lightingMode)
    {
        case 1:
            return baseColorAlbedo;
        case 2:
            return float4(N * 0.5 + 0.5, 1.0); // Remap normal from [-1,1] to [0,1]
        case 3:
            return float4(metallic, metallic, metallic, 1.0);
        case 4:
            return float4(roughness, roughness, roughness, 1.0);
        case 5:
            return float4(emissive, 1.0);
        case 6:
            return float4(ao, ao, ao, 1.0);
    }
    
    // 7. Setup PBR vectors
    float3 V = normalize(cameraPosition - input.worldPos);
    float3 L = normalize(lightPosition.xyz - input.worldPos);
    float3 H = normalize(V + L);
    float NdotL = max(dot(N, L), 0.0);

    // 8. Calculate Direct Illumination
    float3 F0 = lerp(float3(0.04, 0.04, 0.04), baseColorAlbedo.rgb, metallic);
    
    // Cook-Torrance BRDF
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = fresnelSchlick(max(dot(H, V), 0.0), F0);

    float3 numerator = NDF * G * F;
    float denominator = 4.0 * max(dot(N, V), 0.0) * NdotL + 0.0001;
    float3 specular = numerator / denominator;

    float3 kS = F;
    float3 kD = float3(1.0, 1.0, 1.0) - kS;
    kD *= (1.0 - metallic);
    
    float distance = length(lightPosition.xyz - input.worldPos);
    float attenuation = 1.0 / (distance * distance);
    float3 radiance = lD.rgb * attenuation * 20.0f; 
    
    float3 directLighting = (kD * baseColorAlbedo.rgb / PI + specular) * radiance * NdotL;
   
    
    float3 finalColor = float3(0, 0, 0);

        finalColor = ambient + directLighting + mat.EmissiveFactor;
    
    // HDR to LDR Tonemapping & Gamma Correction
    finalColor = finalColor / (finalColor + 1.0f);
    finalColor = pow(finalColor, 1.0f / 2.2f);
    return float4(finalColor, baseColorAlbedo.a);
}
