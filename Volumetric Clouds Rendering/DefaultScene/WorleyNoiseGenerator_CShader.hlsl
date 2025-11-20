
uint g_NumCells : register(b0); // number of cells per axis
StructuredBuffer<float3> g_FeaturePoints : register(t0);
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
float worley(float3 samplePos)
{
    int3 cellID = int3(floor(samplePos * g_NumCells));
    float minDist = 1e9;

    for (int cellOffsetIndex = 0; cellOffsetIndex < 27; cellOffsetIndex++)
    {
        int3 adjID = cellID + offsets[cellOffsetIndex];
     // cell outside map
        if (minComponent(adjID) == -1 || maxComponent(adjID) == g_NumCells)
        {
            int3 wrappedID = (adjID + g_NumCells) % (uint3) g_NumCells;
            int adjCellIndex = wrappedID.x + g_NumCells * (wrappedID.y + wrappedID.z * g_NumCells);
            float3 wrappedPoint = g_FeaturePoints[adjCellIndex];
            
            for (int wrapOffsetIndex = 0; wrapOffsetIndex < 27; wrapOffsetIndex++)
            {
                float3 sampleOffset = (samplePos - (wrappedPoint + offsets[wrapOffsetIndex]));
                minDist = min(minDist, dot(sampleOffset, sampleOffset));
            }
        }
        //cell inside map
        else
        {
            int adjCellIndex = adjID.x + g_NumCells * (adjID.y + adjID.z * g_NumCells);
            float3 sampleOffset = samplePos - g_FeaturePoints[adjCellIndex];
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
    
    float noise = worley(uvw);

    g_Output[DTid] = float4(noise, noise, noise, 1.0);
}