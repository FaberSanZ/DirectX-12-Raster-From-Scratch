cbuffer RootConstant : register(b0)
{
    float4 color;
    float scale;
};


float4 PS() : SV_Target
{
    return color;
}