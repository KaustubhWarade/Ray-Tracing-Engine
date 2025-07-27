struct vertex_output
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};

Texture2D myTexture2D : register(t0);
SamplerState mySamplerState : register(s0);
float4 main(vertex_output input) : SV_TARGET
{
    float4 color = myTexture2D.Sample(mySamplerState, input.texcoord);
    return color;
}