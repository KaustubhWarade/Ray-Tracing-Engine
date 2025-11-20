struct vertex_output
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float4 main(vertex_output input) : SV_TARGET
{
    return float4(input.texcoord.x, input.texcoord.y, 0.5f, 1.0f);
}