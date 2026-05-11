struct Vertex
{
    float4 position;
    float4 color;
};

StructuredBuffer<Vertex> Vertices : register(t0);

cbuffer MatrixBuffer : register(b1)
{
    float4x4 View;
    float4x4 Projection;
};

cbuffer RootConstant : register(b2)
{
    float4x4 World;
};

struct PixelInputType
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

PixelInputType VS(uint vertexId : SV_VertexID)
{
    Vertex vertex = Vertices[vertexId];

    PixelInputType output;

    float4 pos = vertex.position;
    pos.w = 1.0f;

    output.Pos = mul(pos, World);
    output.Pos = mul(output.Pos, View);
    output.Pos = mul(output.Pos, Projection);

    output.Color = vertex.color;

    return output;
}