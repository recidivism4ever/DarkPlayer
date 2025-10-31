struct VS_Input
{
    float2 pos : POS;
    float2 uv : TEX;
};

struct VS_Output
{
    float4 pos : SV_POSITION;
    float2 uv : TEXCOORD0;
    nointerpolation uint albumId : TEXCOORD1;
};