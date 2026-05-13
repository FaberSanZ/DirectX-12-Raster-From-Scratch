struct Vertex
{
    float4 position;
    float4 color;
};

StructuredBuffer<Vertex> Vertices : register(t1);

cbuffer MatrixBuffer : register(b2)
{
    float4x4 View;
    float4x4 Projection;
};

StructuredBuffer<float4x4> Instances : register(t3);

struct PixelInputType
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

PixelInputType VS(uint vertexId : SV_VertexID, uint instanceId : SV_InstanceID)
{
    Vertex vertex = Vertices[vertexId];
    float4x4 world = Instances[instanceId];

    PixelInputType output;

    float4 pos = vertex.position;
    pos.w = 1.0f;

    output.Pos = mul(pos, world);
    output.Pos = mul(output.Pos, View);
    output.Pos = mul(output.Pos, Projection);

    output.Color = vertex.color;

    return output;
}