struct vertex_output
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

vertex_output main(float4 pos : POSITION, float2 tex : TEXCOORD)
{
    vertex_output output;
    output.position = pos;
    output.texcoord = tex;
    return output;
}