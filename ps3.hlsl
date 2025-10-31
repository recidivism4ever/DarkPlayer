#include "hdr.hlsli"
#include "playerdefs.h"

cbuffer constants : register(b0)
{
    float2 pos;
    float sely;
};

Texture2DArray albums : register(t0);
Texture2DArray basemaps : register(t1);
SamplerState mysampler : register(s0);

float random(in float2 st)
{
    return frac(sin(dot(st.xy,
                         float2(12.9898, 78.233)))
                 * 43758.5453123);
}

float sdBox(float2 p, float2 b)
{
    float2 d = abs(p) - b;
    return length(max(d, 0.0)) + min(max(d.x, d.y), 0.0);
}

float4 ps3_main(VS_Output input) : SV_Target
{
    const float4 grey = float4(
        0.2,
        0.2235294117647059,
        0.24313725490196078,
        1.0);
    
    float4 s = basemaps.Sample(mysampler, float3(input.uv, NBUTTONS-1));
    float4 c = grey;
    float2 posTL = input.pos.xy * float2(1.0 / PLAYER_WIDTH, 1.0 / PLAYER_HEIGHT);
    float selbox = sdBox(input.pos.xy - float2(0.366666 * PLAYER_WIDTH, sely), float2(132, 30 * SCALE)) - 4.0;
    selbox = clamp(selbox, 0.0, 1.0);
    float selb = lerp(1.0, 0.75, 1.0 - selbox);
    selb = lerp(selb, 1.0, 1.0 - s.g);
    float brightness = selb * s.r *
        lerp(1.0, 0.4583, posTL.y * posTL.y) + random(posTL) * 0.025;
    
    c = lerp(c, albums.Sample(mysampler, float3(s.b, s.a, float(input.albumId))), 1.0 - s.g);
    
    return c * brightness;
}