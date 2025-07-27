cbuffer ConstantBufferData
{
    float4x4 worldMatrix;
    float4x4 viewMatrix;
    float4x4 projectionMatrix;
    float4 lD;
    float4 kD;
    float4 lightPosition;
    uint keyPressed;
};

struct vertex_output
{
    float4 position : SV_POSITION;
    float3 diffusedLight : COLOR;
};
		
vertex_output main(float4 pos : POSITION, float3 norm : NORMAL)
{
    vertex_output output;
    if (keyPressed == 1)
    {
        float4 iPosition = mul(mul(worldMatrix, viewMatrix), pos);
        float3x3 normalMatrix = (float3x3) (mul(worldMatrix, viewMatrix));
        float3 n = normalize(mul(normalMatrix, norm));
        float3 s = normalize((float3) (lightPosition - iPosition));
        output.diffusedLight = lD * kD * max(dot(s, n), 0.0f);
    }
    else
    {
        output.diffusedLight = float3(norm);
    }
    float4 position = mul(projectionMatrix, mul(mul(worldMatrix, viewMatrix), pos));
    output.position = position;
    return output;
}