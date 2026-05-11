struct Vertex
{
    float4 position;
    float4 color;
};

StructuredBuffer<Vertex> Vertices : register(t0);

struct PixelInputType
{
    float4 Pos : SV_POSITION;
    float4 Color : COLOR;
};

PixelInputType VS(uint vertexId : SV_VertexID)
{
    Vertex input = Vertices[vertexId];
    PixelInputType output;

    output.Pos = input.position;
    output.Color = input.color;


    return output;
}
