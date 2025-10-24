

// =========================================================================
// STRUCTS
// =========================================================================
#define PI 3.1415926535f

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
    bool DoubleSided;
    bool Unlit;
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

struct Sphere
{
    float3 Position;
    float Radius;
    int MaterialIndex;
};

struct Triangle
{
    float3 v0, v1, v2;
    int MaterialIndex;
    float3 n0, n1, n2;
    float2 tc0, tc1, tc2;
};

struct Ray
{
    float3 Origin;
    float3 Direction;
};

struct HitData
{
    float HitDistance;
    int ObjectType;
    int ObjectIndex;
    float3 HitPosition;
    float3 HitNormal;
};



// =========================================================================
// RESOURCES AND CONSTANTS
// =========================================================================


cbuffer SceneConstants : register(b0)
{
    float3 g_CameraPosition;
    uint g_FrameIndex;
    float4x4 g_InverseProjection;
    float4x4 g_InverseView;
    uint g_numBounces;
    uint g_numRaysPerPixel;
    float c_exposure;
};

StructuredBuffer<Sphere> g_Spheres : register(t0);
StructuredBuffer<Triangle> g_Triangles : register(t1);
StructuredBuffer<Material> g_Materials : register(t2);

RWTexture2D<float4> g_AccumulationTexture : register(u0);
RWTexture2D<float4> g_OutputTexture : register(u1);

Texture2D g_EnvironmentTexture : register(t3);
SamplerState g_StaticSampler : register(s0);


// =========================================================================
// UTILITY AND PHYSICS FUNCTIONS
// =========================================================================


float PCG_RandomFloat(inout uint seed)
{
    seed = seed * 747796405u + 2891336453u;
    uint word = ((seed >> ((seed >> 28u) + 4u)) ^ seed) * 277803737u;
    word = (word >> 22u) ^ word;
    return (float) word / 4294967295.0f;
}

float3 PCG_InUnitSphere(inout uint seed)
{
    float z = PCG_RandomFloat(seed) * 2.0f - 1.0f;
    float a = PCG_RandomFloat(seed) * 2.0f * 3.14159265f;
    float r = sqrt(1.0f - z * z);
    float x = r * cos(a);
    float y = r * sin(a);
    return float3(x, y, z);
}

float3 PCG_InHemisphere(float3 normal, inout uint seed)
{
    float3 p = PCG_InUnitSphere(seed);
    if (dot(p, normal) < 0.0f)
    {
        p = -p;
    }
    return p;
}

float3 refract_hlsl(float3 incident, float3 normal, float ior_ratio)
{
    float cos_theta = min(dot(-incident, normal), 1.0f);
    float3 r_out_perp = ior_ratio * (incident + cos_theta * normal);
    float3 r_out_parallel = -sqrt(abs(1.0f - dot(r_out_perp, r_out_perp))) * normal;
    return r_out_perp + r_out_parallel;
}

float schlick_reflectance(float cosine, float ior_ratio)
{
    float r0 = (1.0f - ior_ratio) / (1.0f + ior_ratio);
    r0 = r0 * r0;
    return r0 + (1.0f - r0) * pow((1.0f - cosine), 5);
}

bool RussianRoulette(inout float3 rayColor, inout uint seed)
{
    float p = max(rayColor.r, max(rayColor.g, rayColor.b));
    if (PCG_RandomFloat(seed) > p && p < 1.0f)
    {
        return true;
    }
    rayColor *= 1.0f / p;
    return false;
}

float3 ACESFilm(float3 x)
{
    float a = 2.51f;
    float b = 0.03f;
    float c = 2.43f;
    float d = 0.59f;
    float e = 0.14f;
    return saturate((x * (a * x + b)) / (x * (c * x + d) + e));
}

float3 LinearToSRGB(float3 rgb)
{
    rgb = saturate(rgb);
    float3 a = pow(abs(rgb), float3(1.0f / 2.4f, 1.0f / 2.4f, 1.0f / 2.4f)) * 1.055f - 0.055f;
    float3 b = rgb * 12.92f;
    return float3(
		(rgb.r < 0.0031308f) ? b.r : a.r,
		(rgb.g < 0.0031308f) ? b.g : a.g,
		(rgb.b < 0.0031308f) ? b.b : a.b
	);
}


// =========================================================================
// TRACE RAY FUNCTIONS
// =========================================================================

HitData IntersectTriangle(Ray ray, Triangle tri, uint triIndex)
{
    HitData hit;
    hit.HitDistance = -1.0f;
    hit.ObjectIndex = -1;
    hit.ObjectType = -1;

    const float3 edge1 = tri.v1 - tri.v0;
    const float3 edge2 = tri.v2 - tri.v0;
    const float3 h = cross(ray.Direction, edge2);
    const float a = dot(edge1, h);
    
    // Check if ray is parallel to triangle
    if (a > -1e-6 && a < 1e-6) 
        return hit;
    
    float f = 1.0f / a;
    float3 s = ray.Origin - tri.v0;
    float u = f * dot(s, h);

    if (u < 0.0 || u > 1.0)
        return hit;

    const float3 q = cross(s, edge1);
    const float v = f * dot(ray.Direction, q);

    if (v < 0.0 || u + v > 1.0)
        return hit;
    
    // At this point we know there is a hit. Calculate the distance 't'.
    float t = f * dot(edge2, q);

    if (t > 0.0001f) // Ensure the intersection is in front of the ray's origin
    {
        hit.HitDistance = t;
        hit.HitPosition = ray.Origin + ray.Direction * t;
        hit.HitNormal = normalize(cross(edge1, edge2));
        hit.ObjectIndex = triIndex;
        hit.ObjectType = 1;
    }
    
    return hit;
}

HitData TraceRay(Ray ray)
{
    HitData closestHit;
    closestHit.HitDistance = 3.4e38f;
    closestHit.ObjectIndex = -1;
    closestHit.ObjectType = -1;

    uint numSpheres, sphereStride;
    g_Spheres.GetDimensions(numSpheres, sphereStride);

    //Sphere Intersection
    for (uint i = 0; i < numSpheres; i++)
    {
        Sphere sphere = g_Spheres[i];

        float3 originToCenter = ray.Origin - sphere.Position;
        float a = dot(ray.Direction, ray.Direction);
        float b = 2.0f * dot(originToCenter, ray.Direction);
        float c = dot(originToCenter, originToCenter) - (sphere.Radius * sphere.Radius);

        float discriminant = b * b - 4.0f * a * c;
        if (discriminant < 0.0f)
            continue;

        float closestT = (-b - sqrt(discriminant)) / (2.0f * a);

        if (closestT > 0.0001f && closestT < closestHit.HitDistance)
        {
            closestHit.HitDistance = closestT;
            closestHit.ObjectIndex = i;
            closestHit.ObjectType = 0;

        }
    }
    
    //triangle Intersection
    uint numTriangles, stride;
    g_Triangles.GetDimensions(numTriangles, stride);
    for (uint j = 0; j < numTriangles; j++)
    {
        HitData triHit = IntersectTriangle(ray, g_Triangles[j], j);
        if (triHit.HitDistance > 0 && triHit.HitDistance < closestHit.HitDistance)
        {
            closestHit = triHit;
        }
    }
    
    if (closestHit.ObjectIndex < 0)
    {
        closestHit.ObjectIndex = -1;
        closestHit.ObjectType = -1;
        return closestHit;
    }
    
    if (closestHit.ObjectType == 0)
    {
        closestHit.HitPosition = ray.Origin + ray.Direction * closestHit.HitDistance;
        Sphere hitSphere = g_Spheres[closestHit.ObjectIndex];
        closestHit.HitNormal = normalize(closestHit.HitPosition - hitSphere.Position);
    }
    return closestHit;
}


// =========================================================================
// MATERIAL HANDLING
// =========================================================================

void HandleOpaqueMaterial(inout Ray ray, inout float3 rayColor, const Material material, float3 normal, inout uint seed)
{
    // --- Metal ---
    if (material.MetallicFactor > 0.99f) // Treat as pure metal
    {
        float3 specularDir = reflect(ray.Direction, normal);
        ray.Direction = normalize(specularDir + (material.RoughnessFactor * material.RoughnessFactor) * PCG_InUnitSphere(seed));
        rayColor *= material.BaseColorFactor.rgb;
        return;
    }

    // --- Dielectric (non-metal) ---
    float cos_theta = min(dot(-ray.Direction, normal), 1.0f);
    float ior_ratio = 1.0f / material.IOR;
    float reflectance = schlick_reflectance(cos_theta, ior_ratio);

    if (PCG_RandomFloat(seed) < reflectance) // Specular reflection
    {
        float3 specularDir = reflect(ray.Direction, normal);
        ray.Direction = normalize(specularDir + (material.RoughnessFactor * material.RoughnessFactor) * PCG_InUnitSphere(seed));
    }
    else // Diffuse reflection
    {
        ray.Direction = normalize(PCG_InHemisphere(normal, seed));
        rayColor *= material.BaseColorFactor.rgb;
    }
}


void HandleDielectricMaterial(inout Ray ray, inout float3 rayColor, const Material material, bool front_face, float3 normal, inout uint seed)
{
    // Determine IOR Ratio and cosine angle
    float ior_ratio = front_face ? (1.0f / material.IOR) : material.IOR;
    float cos_theta = min(dot(-ray.Direction, normal), 1.0f);
    float sin_theta = sqrt(1.0f - cos_theta * cos_theta);

    bool cannot_refract = ior_ratio * sin_theta > 1.0f;
    float reflectance = schlick_reflectance(cos_theta, ior_ratio);

    if (cannot_refract || PCG_RandomFloat(seed) < reflectance)
    {
        // Reflection
        ray.Direction = reflect(ray.Direction, normal);
    }
    else
    {
        // Refraction
        ray.Direction = refract_hlsl(ray.Direction, normal, ior_ratio);
        // Note: Beer's law logic removed as AbsorptionColor is not in the new material.
    }
    
    // Apply roughness for frosted glass effect
    ray.Direction = normalize(ray.Direction + material.RoughnessFactor * material.RoughnessFactor * PCG_InUnitSphere(seed));
}
float3 DispatchRay(Ray ray, inout uint seed)
{

    float3 light = float3(0.0f, 0.0f, 0.0f);
    float3 rayColor = float3(1.0f, 1.0f, 1.0f);

    for (uint i = 0; i < g_numBounces; i++)
    {

        HitData hitData = TraceRay(ray);
        if (hitData.ObjectIndex == -1)
        {
            float phi = atan2(ray.Direction.z, ray.Direction.x);
            float theta = asin(ray.Direction.y);
            float u = 1.0 - (phi + PI) / (2.0 * PI);
            float v = (theta + PI / 2.0) / PI;
            v = 1.0 - v;
            float4 envColorSample = g_EnvironmentTexture.Sample(g_StaticSampler, float2(u, v));
            light += envColorSample.rgb * rayColor;
            break;
        }
        
        Material material;
        if (hitData.ObjectType == 0) // Sphere
        {
            Sphere hitSphere = g_Spheres[hitData.ObjectIndex];
            material = g_Materials[hitSphere.MaterialIndex];
        }
        else if (hitData.ObjectType == 1) // Triangle
        {
            Triangle hitTriangle = g_Triangles[hitData.ObjectIndex];
            material = g_Materials[hitTriangle.MaterialIndex];
        }
        else
        {
            break;
        }
        
        light += material.EmissiveFactor * rayColor;
         
        bool front_face = dot(ray.Direction, hitData.HitNormal) < 0;
        float3 normal = front_face ? hitData.HitNormal : -hitData.HitNormal;

        ray.Origin = hitData.HitPosition + normal * 0.0001f;

        if (material.Transmission > 0.0f)
        {
            HandleDielectricMaterial(ray, rayColor, material, front_face, normal, seed);
        }
        else
        {
            HandleOpaqueMaterial(ray, rayColor, material, normal, seed);
        }
        
        if (i > 2)
        {
            if (RussianRoulette(rayColor, seed))
            {
                break; // Terminate ray
            }
        }

    }
    return light;
}

[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixelCoord = dispatchThreadID.xy;
    uint width, height;
    g_OutputTexture.GetDimensions(width, height);
    if (pixelCoord.x >= width || pixelCoord.y >= height)
    {
        return;
    }

    Ray ray;
    ray.Origin = g_CameraPosition;

    uint seed = pixelCoord.x + pixelCoord.y * width + g_FrameIndex * (width * height);
    float3 totalColor = 0;

    for (int rayIndex = 0; rayIndex < g_numRaysPerPixel; rayIndex++)
    {
       //for anti aliasong jitter
        float randomX = PCG_RandomFloat(seed);
        float randomY = PCG_RandomFloat(seed);

        float px = 2.0f * (pixelCoord.x + randomX) / width - 1.0f;
        float py = 2.0f * (pixelCoord.y + randomY) / height - 1.0f;
        px = -px;
        py = -py;

		// The rest of your ray generation logic
        float4 coord = float4(px, py, 1, 1);
        float4 viewSpace = mul(g_InverseProjection, coord);
        viewSpace.xyz /= viewSpace.w;
        float3 worldDirection = mul(g_InverseView, float4(viewSpace.xyz, 0.0f)).xyz;
        ray.Direction = normalize(worldDirection);

        totalColor += DispatchRay(ray, seed);
    }

    float4 accumulatedData = g_AccumulationTexture[pixelCoord];
    if (g_FrameIndex == 0)
        accumulatedData = float4(0, 0, 0, 0);

    accumulatedData.rgb += totalColor / g_numRaysPerPixel;
    accumulatedData.a += 1.0f;

    g_AccumulationTexture[pixelCoord] = accumulatedData;
    
    float3 linearColor = accumulatedData.rgb / accumulatedData.a;
    linearColor *= c_exposure;
    float3 tonemappedColor = ACESFilm(linearColor);
    float3 finalColor = LinearToSRGB(tonemappedColor);
    g_OutputTexture[pixelCoord] = float4(finalColor, 1.0f);
}