#include "hdr.hlsli"

cbuffer constants : register(b0)
{
    float2 pos;
    float2 selpos;
};

VS_Output vs2_main(VS_Input input, uint vertexId : SV_VertexID)
{
    VS_Output output;
    output.pos = float4(input.pos + pos, 0.0f, 1.0f);
    output.uv = input.uv;
    output.albumId = vertexId / 6;
    return output;
}
