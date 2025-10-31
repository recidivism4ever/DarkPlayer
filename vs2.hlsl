#include "hdr.hlsli"

cbuffer constants : register(b0)
{
    float2 pos;
};

VS_Output vs2_main(VS_Input input)
{
    VS_Output output;
    output.pos = float4(input.pos + pos, 0.0f, 1.0f);
    output.uv = input.uv;
    return output;
}
