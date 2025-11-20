cbuffer NoiseParams : register(b0)
{
    int4 g_numPoints;
};  

StructuredBuffer<float3> g_FeaturePointsR : register(t0);
StructuredBuffer<float3> g_FeaturePointsG : register(t1);
StructuredBuffer<float3> g_FeaturePointsB : register(t2);
StructuredBuffer<float3> g_FeaturePointsA : register(t3);

RWTexture3D<float4> g_Output : register(u0);

// Precomputed offsets for 3x3x3 neighborhood
static const int3 offsets[27] =
{
    int3(-1, -1, -1), int3(0, -1, -1), int3(1, -1, -1),
    int3(-1, 0, -1), int3(0, 0, -1), int3(1, 0, -1),
    int3(-1, 1, -1), int3(0, 1, -1), int3(1, 1, -1),
    int3(-1, -1, 0), int3(0, -1, 0), int3(1, -1, 0),
    int3(-1, 0, 0), int3(0, 0, 0), int3(1, 0, 0),
    int3(-1, 1, 0), int3(0, 1, 0), int3(1, 1, 0),
    int3(-1, -1, 1), int3(0, -1, 1), int3(1, -1, 1),
    int3(-1, 0, 1), int3(0, 0, 1), int3(1, 0, 1),
    int3(-1, 1, 1), int3(0, 1, 1), int3(1, 1, 1)
};

// Utility: get min/max component of int3
int minComponent(int3 v)
{
    return min(v.x, min(v.y, v.z));
}
int maxComponent(int3 v)
{
    return max(v.x, max(v.y, v.z));
}

// Core Worley function
float worley(float3 samplePos, StructuredBuffer<float3> points, uint numCells)
{
    int3 cellID = int3(floor(samplePos * numCells));
    float minDist = 1e9;

    for (int cellOffsetIndex = 0; cellOffsetIndex < 27; cellOffsetIndex++)
    {
        int3 adjID = cellID + offsets[cellOffsetIndex];
        // cell outside map
        if (minComponent(adjID) == -1 || maxComponent(adjID) == numCells)
        {
            int3 wrappedID = (adjID + numCells) % (uint3) numCells;
            int adjCellIndex = wrappedID.x + numCells * (wrappedID.y + wrappedID.z * numCells);
            float3 wrappedPoint = points[adjCellIndex];
            
            for (int wrapOffsetIndex = 0; wrapOffsetIndex < 27; wrapOffsetIndex++)
            {
                float3 sampleOffset = (samplePos - (wrappedPoint + offsets[wrapOffsetIndex]));
                minDist = min(minDist, dot(sampleOffset, sampleOffset));
            }
        }
        //cell inside map
        else
        {
            int adjCellIndex = adjID.x + numCells * (adjID.y + adjID.z * numCells);
            float3 sampleOffset = samplePos - points[adjCellIndex];
            minDist = min(minDist, dot(sampleOffset, sampleOffset));
        }
  
    }

    float dist = sqrt(minDist);
    float contrast = 10.0; // higher = more contrast
    return pow(1.0 - saturate(dist), contrast);
}

[numthreads(8, 8, 8)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    uint width, height, depth;
    g_Output.GetDimensions(width, height, depth);

    if (DTid.x >= width || DTid.y >= height || DTid.z >= depth)
        return;

    float3 uvw = float3(DTid) / float3(width, height, depth);
    
    float noiseR = worley(uvw, g_FeaturePointsR, g_numPoints.x);
    float noiseG = worley(uvw, g_FeaturePointsG, g_numPoints.y);
    float noiseB = worley(uvw, g_FeaturePointsB, g_numPoints.z);
    float noiseA = worley(uvw, g_FeaturePointsA, g_numPoints.w);

    g_Output[DTid] = float4(noiseR, noiseG, noiseB, noiseA);
}