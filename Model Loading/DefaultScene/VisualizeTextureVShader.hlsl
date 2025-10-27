struct vertex_output
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD0;
};
		
vertex_output main(uint vertID : SV_VertexID)
{
    vertex_output output;
    
    output.texcoord = float2((vertID << 1) & 2, vertID & 2);
    
    output.position = float4(output.texcoord * 2.0f - 1.0f, 0.0f, 1.0f);
    
    output.position.y = -output.position.y;
    return output;
}