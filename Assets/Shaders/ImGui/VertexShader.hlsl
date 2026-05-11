struct Vertex
{
    float4 position;
    float4 color;
};

StructuredBuffer<Vertex> Vertices : register(t0);

cbuffer MatrrixBuffer : register(b0)
{
    float4x4 World;
    float4x4 View;
    float4x4 Projection;
};

struct PixelInputType
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

PixelInputType VS(uint vertexId : SV_VertexID)
{
    Vertex input = Vertices[vertexId];

    float4 position = input.position;
    position.w = 1.0f;

    PixelInputType output;
    output.Pos = mul(position, World);
    output.Pos = mul(output.Pos, View);
    output.Pos = mul(output.Pos, Projection);
    output.Color = input.color;

    return output;
}