struct vertex_output
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

float4 main(vertex_output input) : SV_TARGET
{
    float4 color = float4(input.texcoord.x,input.texcoord.y,0.5f,0.0f);
    return color;
}