struct vertex_output
{
    float4 pos : SV_POSITION;
    nointerpolation uint materialIndex : MATERIALINDEX;
};

float4 main(vertex_output input) : SV_TARGET
{
    uint id = input.materialIndex + 1;
    
    id = id % 1000;
    
    uint hundreds = id / 100; 
    uint tens = (id / 10) % 10; 
    uint ones = id % 10; 
    
    float r = (float) hundreds / 10.0f;
    float g = (float) tens / 10.0f; 
    float b = (float) ones / 10.0f; 
    
    return float4(r, g, b, 1.0f);
}