cbuffer RootConstant : register(b0)
{
    float4 color;
    float scale;
};


float4 VS(uint vID : SV_VERTEXID) : SV_Position
{
    float4 triangle_position[] =
    {
        float4(-0.0f * scale, 0.5f * scale, 0.0F, 1.0f),
        float4(0.5f * scale, -0.5f * scale, 0.0f, 1.0f),
        float4(-0.5f * scale, -0.5f * scale, 0.0f, 1.0f)
    };
    
    return triangle_position[vID];

}