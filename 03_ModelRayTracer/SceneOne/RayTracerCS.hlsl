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


struct Triangle
{
    float3 v0, v1, v2; // Vertex positions
    int MaterialIndex;
    float3 n0, n1, n2; // Vertex normals
    float2 tc0, tc1, tc2;
};

struct BVHNode
{
    float3 aabbMin;
    int leftChildOrFirstTriangleIndex;
    float3 aabbMax;
    int triangleCount;
};

struct Ray
{
    float3 Origin;
    float3 Direction;
};

struct HitData
{
    float HitDistance;
    int PrimitiveIndex;
    int InstanceIndex;
    float3 HitPosition;
    float3 HitNormal;
    float2 TexCoord;
};

struct ModelInstance
{
    float4x4 Transform;
    float4x4 InverseTransform;
    uint BaseTriangleIndex;
    uint BaseNodeIndex;
    uint MaterialOffset;
    float _padding0;
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
    uint g_useEnvMap;
};

StructuredBuffer<Material> g_Materials : register(t0);
StructuredBuffer<Triangle> g_UberTriangles : register(t1);
StructuredBuffer<BVHNode> g_UberBLAS : register(t2);
StructuredBuffer<BVHNode> g_TLAS : register(t3);
StructuredBuffer<ModelInstance> g_Instances : register(t4);

RWTexture2D<float4> g_AccumulationTexture : register(u0);
RWTexture2D<float4> g_OutputTexture : register(u1);

Texture2D g_EnvironmentTexture : register(t5);
SamplerState g_StaticSampler : register(s0);

Texture2D g_Textures[] : register(t0, space1);

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
    hit.PrimitiveIndex = -1;

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
        float w = 1.0f - u - v;
        float3 interpolatedNormal = w * tri.n0 + u * tri.n1 + v * tri.n2;
        hit.HitNormal = normalize(interpolatedNormal);
        hit.TexCoord = w * tri.tc0 + u * tri.tc1 + v * tri.tc2;
        hit.PrimitiveIndex = triIndex;
    }
    
    return hit;
}

bool RayAABB(Ray ray, float3 min_aabb, float3 max_aabb, out float hitDist)
{
    float3 invD = 1.0f / ray.Direction;
    float3 t0s = (min_aabb - ray.Origin) * invD;
    float3 t1s = (max_aabb - ray.Origin) * invD;

    float3 tmin = min(t0s, t1s);
    float3 tmax = max(t0s, t1s);

    float t_enter = max(max(tmin.x, tmin.y), tmin.z);
    float t_exit = min(min(tmax.x, tmax.y), tmax.z);
    
    if (t_exit >= t_enter && t_exit > 0)
    {
        hitDist = max(t_enter, 0.0f);
        return true;
    }
    hitDist = 3.4e38f;
    return false;
}

HitData TraverseBLAS(Ray modelSpaceRay, uint baseNodeIndex, uint baseTriangleIndex)
{
    int stack[32];
    int stackPtr = 0;
    stack[stackPtr++] = baseNodeIndex; // Start at the root of this model's BLAS
    HitData closestHit;
    closestHit.HitDistance = 3.4e38f;
    closestHit.PrimitiveIndex = -1;
    closestHit.InstanceIndex = -1;
    closestHit.HitNormal = float3(0, 0, 0);
    closestHit.HitPosition = float3(0, 0, 0);

    while (stackPtr > 0)
    {
        int nodeIndex = stack[--stackPtr];
        BVHNode node = g_UberBLAS[nodeIndex];
        
        float distToAABB;
        if (RayAABB(modelSpaceRay, node.aabbMin, node.aabbMax, distToAABB))
        {
            if (distToAABB > closestHit.HitDistance)
                continue;

            if (node.triangleCount > 0)
            {
                for (int i = 0; i < node.triangleCount; ++i)
                {
                    uint triIndex = baseTriangleIndex + node.leftChildOrFirstTriangleIndex + i;
                    HitData hit = IntersectTriangle(modelSpaceRay, g_UberTriangles[triIndex], triIndex);
                    if (hit.HitDistance > 0 && hit.HitDistance < closestHit.HitDistance)
                    {
                        closestHit = hit;
                    }
                }
            }
            else
            {
                 // Calculate the GLOBAL indices of the two children.
                uint leftChildIndex = node.leftChildOrFirstTriangleIndex;
                uint rightChildIndex = leftChildIndex + 1;

                BVHNode leftChild = g_UberBLAS[leftChildIndex];
                BVHNode rightChild = g_UberBLAS[rightChildIndex];

                float distLeft, distRight;
                bool hitLeft = RayAABB(modelSpaceRay, leftChild.aabbMin, leftChild.aabbMax, distLeft);
                bool hitRight = RayAABB(modelSpaceRay, rightChild.aabbMin, rightChild.aabbMax, distRight);
                
                if (hitLeft && distLeft >= closestHit.HitDistance)
                {
                    hitLeft = false; 
                }
                if (hitRight && distRight >= closestHit.HitDistance)
                {
                    hitRight = false; 
                }

                if (hitLeft && hitRight)
                {
                // Push the farther child first so the closer one is processed next
                    if (distLeft < distRight)
                    {
                        stack[stackPtr++] = rightChildIndex; // Right child (farther)
                        stack[stackPtr++] = leftChildIndex; // Left child (closer)
                    }
                    else
                    {
                        stack[stackPtr++] = leftChildIndex; // Left child (farther)
                        stack[stackPtr++] = rightChildIndex; // Right child (closer)
                    }
                }
                else if (hitLeft)
                {
                    stack[stackPtr++] = leftChildIndex;
                }
                else if (hitRight)
                {
                    stack[stackPtr++] = rightChildIndex;
                }
            }
        }
    }
    return closestHit;

}

HitData TraceRay(Ray ray)
{
        
    const bool DEBUG_VISUALIZE_TLAS = false;
    
    HitData closestHit;
    closestHit.HitDistance = 3.4e38f;
    closestHit.PrimitiveIndex = -1;
    closestHit.InstanceIndex = -1;
    closestHit.HitNormal = float3(0, 0, 0);
    closestHit.HitPosition = float3(0, 0, 0);
    
    int tlasStack[64];
    int tlasStackPtr = 0;
    tlasStack[tlasStackPtr++] = 0; // Start at root of TLAS

    while (tlasStackPtr > 0)
    {
        int nodeIndex = tlasStack[--tlasStackPtr];
        BVHNode node = g_TLAS[nodeIndex];

        float distToAABB;
        if (RayAABB(ray, node.aabbMin, node.aabbMax, distToAABB))
        {
            if (distToAABB > closestHit.HitDistance)
                continue;

            if (node.triangleCount > 0) // Leaf node in TLAS points to an instance
            {
                if (DEBUG_VISUALIZE_TLAS)
                {
                    closestHit.HitDistance = distToAABB;
                    closestHit.PrimitiveIndex = -2; // Use -2 as a special flag
                    closestHit.HitPosition = ray.Origin + ray.Direction * distToAABB;
                    uint instanceID = node.leftChildOrFirstTriangleIndex;
                    closestHit.HitNormal = float3((float) instanceID/10.0f, (float) instanceID/10.0f, (float) instanceID/10.0f);
                    return closestHit;
                }
                
                uint instanceID = node.leftChildOrFirstTriangleIndex;
                ModelInstance inst = g_Instances[instanceID];

                // Transform ray into model's local space
                Ray modelSpaceRay;
                modelSpaceRay.Origin = mul(inst.InverseTransform, float4(ray.Origin, 1.0f)).xyz;
                modelSpaceRay.Direction = mul(inst.InverseTransform, float4(ray.Direction, 0.0f)).xyz;

                // Hit data for this instance's BLAS
                HitData modelHit;
                modelHit.HitDistance = 3.4e38f;
                modelHit.PrimitiveIndex = -1; 
                modelHit.InstanceIndex = -1;
                modelHit.HitNormal = float3(0, 0, 0);
                modelHit.HitPosition = float3(1, 0, 0);

                modelHit = TraverseBLAS(modelSpaceRay, inst.BaseNodeIndex, inst.BaseTriangleIndex);
                
                // If we found a closer hit within this BLAS
                if (modelHit.PrimitiveIndex != -1)
                {
                    // Transform hit point and normal back to world space
                    float3 worldHitPos = mul(inst.Transform, float4(modelHit.HitPosition, 1.0f)).xyz;
                    float worldHitDist = distance(ray.Origin, worldHitPos);

                    if (worldHitDist < closestHit.HitDistance)
                    {
                        closestHit.HitDistance = worldHitDist;
                        closestHit.HitPosition = worldHitPos;
                        float4x4 inverseTranspose = transpose(inst.InverseTransform);
                        closestHit.HitNormal = normalize(mul(inverseTranspose, float4(modelHit.HitNormal, 0.0f)).xyz);
                        closestHit.PrimitiveIndex = modelHit.PrimitiveIndex;
                        closestHit.InstanceIndex = instanceID;
                    }
                }
                
            }
            else // Internal node in TLAS
            {
                uint leftChildIndex = node.leftChildOrFirstTriangleIndex;
                uint rightChildIndex = leftChildIndex + 1;

                BVHNode leftChild = g_TLAS[leftChildIndex];
                BVHNode rightChild = g_TLAS[rightChildIndex];

                float distLeft, distRight;
                bool hitLeft = RayAABB(ray, leftChild.aabbMin, leftChild.aabbMax, distLeft);
                bool hitRight = RayAABB(ray, rightChild.aabbMin, rightChild.aabbMax, distRight);

                if (hitLeft && distLeft >= closestHit.HitDistance)
                    hitLeft = false;
                if (hitRight && distRight >= closestHit.HitDistance)
                    hitRight = false;
    
                if (hitLeft && hitRight)
                {
                    if (distLeft < distRight)
                    {
                        tlasStack[tlasStackPtr++] = rightChildIndex;
                        tlasStack[tlasStackPtr++] = leftChildIndex;
                    }
                    else
                    {
                        tlasStack[tlasStackPtr++] = leftChildIndex;
                        tlasStack[tlasStackPtr++] = rightChildIndex;
                    }
                }
                else if (hitLeft)
                {
                    tlasStack[tlasStackPtr++] = leftChildIndex;
                }
                else if (hitRight)
                {
                    tlasStack[tlasStackPtr++] = rightChildIndex;
                }
            }
        }
    }
    
    return closestHit;
}

// =========================================================================
// MATERIAL HANDLING
// =========================================================================

void HandleOpaqueMaterial(inout Ray ray, inout float3 rayColor, const Material material, float3 normal, inout uint seed)
{
    // --- Metal ---
    if (PCG_RandomFloat(seed) < material.MetallicFactor) // Treat as pure metal
    {
        float3 specularDir = reflect(ray.Direction, normal);
        ray.Direction = normalize(specularDir + (material.RoughnessFactor * material.RoughnessFactor) * PCG_InUnitSphere(seed));
        rayColor *= material.BaseColorFactor.rgb;
        return;
    }

    // --- Dielectric plastic wood --
    float cos_theta = min(dot(-ray.Direction, normal), 1.0f);
    float ior_ratio = 1.0f / material.IOR;
    float reflectance = schlick_reflectance(cos_theta, ior_ratio);

    if (PCG_RandomFloat(seed) < reflectance)
    {
        float3 specularDir = reflect(ray.Direction, normal);
        ray.Direction = normalize(specularDir + (material.RoughnessFactor * material.RoughnessFactor) * PCG_InUnitSphere(seed));
    }
    else
    {
		// Treat as pure dielectric/diffuse
        ray.Direction = normalize(PCG_InHemisphere(normal, seed));
        rayColor *= material.BaseColorFactor.rgb;
    }
}

void HandleDielectricMaterial(inout Ray ray, inout float3 rayColor, const Material material, bool front_face, float3 normal, inout uint seed)
{
    float3 attenuation = material.BaseColorFactor.rgb; // For tinted glass

	//Determine IOR Ratio and cosine angle
    float ior_ratio = front_face ? (1.0f / material.IOR) : material.IOR;
    float cos_theta = min(dot(-ray.Direction, normal), 1.0f);
    float sin_theta = sqrt(1.0f - cos_theta * cos_theta);

    bool cannot_refract = ior_ratio * sin_theta > 1.0f;
    float reflectance = schlick_reflectance(cos_theta, ior_ratio);

    if (cannot_refract || PCG_RandomFloat(seed) < reflectance)
    {
        //reflection
        ray.Direction = reflect(ray.Direction, normal);
    }
    else
    {
        // Handle refraction
        ray.Direction = refract_hlsl(ray.Direction, normal, ior_ratio);
        rayColor *= material.BaseColorFactor.rgb;
        // --- BEER'S LAW IMPLEMENTATION ---
        //if (front_face)
        //{
        //    HitData exitHit = TraceRay(ray);
        //    if (exitHit.PrimitiveIndex != -1)
        //    {
        //        float3 absorption = -material.AbsorptionColor * exitHit.HitDistance;
        //        rayColor *= exp(absorption);
        //        
        //        bool exit_front_face = dot(ray.Direction, exitHit.HitNormal) < 0;
        //        float3 exit_normal = exit_front_face ? exitHit.HitNormal : -exitHit.HitNormal;
        //        ray.Origin = exitHit.HitPosition + exit_normal * 0.0001f;
        //
        //        ray.Direction = refract_hlsl(ray.Direction, exit_normal, material.IOR);
        //    }
        //}
    }

    ray.Direction = normalize(ray.Direction + material.RoughnessFactor * material.RoughnessFactor * PCG_InUnitSphere(seed)); //frosted glass effect
}

float3 DispatchRay(Ray ray, inout uint seed)
{

    float3 light = float3(0.0f, 0.0f, 0.0f);
    float3 rayColor = float3(1.0f, 1.0f, 1.0f);

    for (uint i = 0; i < g_numBounces; i++)
    {

        HitData hitData = TraceRay(ray);
        
        if (hitData.PrimitiveIndex == -2)
        {
            return float3(hitData.HitPosition/5);
        }
        
        
        if (hitData.PrimitiveIndex == -1 && g_useEnvMap==1)
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
        
        Triangle hitTriangle = g_UberTriangles[hitData.PrimitiveIndex];
        ModelInstance inst = g_Instances[hitData.InstanceIndex];
        Material material = g_Materials[inst.MaterialOffset + hitTriangle.MaterialIndex];
        
        // adjust textures
        float4 baseColor = material.BaseColorFactor;
        if (material.BaseColorTextureIndex != -1)
        {
            uint idx = NonUniformResourceIndex(material.BaseColorTextureIndex);
            baseColor *= g_Textures[idx].Sample(g_StaticSampler, hitData.TexCoord);
        }
        
        float metallic = material.MetallicFactor;
        float roughness = material.RoughnessFactor;
        if (material.MetallicRoughnessTextureIndex != -1)
        {
            uint idx = NonUniformResourceIndex(material.MetallicRoughnessTextureIndex);
            float4 mrSample = g_Textures[idx].Sample(g_StaticSampler, hitData.TexCoord);
            roughness *= mrSample.g; 
            metallic *= mrSample.b;
        }
        
        light += material.EmissiveFactor * rayColor;
        bool isLight = (material.EmissiveFactor.r > 0.0f || material.EmissiveFactor.g > 0.0f || material.EmissiveFactor.b > 0.0f);
        if (!isLight || dot(ray.Direction, hitData.HitNormal) < 0.0)
        {
            light += material.BaseColorFactor.rgb * material.EmissiveFactor * rayColor;
        }
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
