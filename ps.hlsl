#include "hdr.hlsli"
#include "playerdefs.h"

Texture2D mytexture : register(t0);
SamplerState mysampler : register(s0);

float random(in float2 st)
{
    return frac(sin(dot(st.xy,
                         float2(12.9898, 78.233)))
                 * 43758.5453123);
}

float opUnion(float d1, float d2)
{
    return min(d1, d2);
}
float opSubtraction(float d1, float d2)
{
    return max(-d1, d2);
}
float opIntersection(float d1, float d2)
{
    return max(d1, d2);
}

float opSmoothUnion(float d1, float d2, float k)
{
    k *= 4.0;
    float h = max(k - abs(d1 - d2), 0.0);
    return min(d1, d2) - h * h * 0.25 / k;
}

float opSmoothSubtraction(float d1, float d2, float k)
{
    return -opSmoothUnion(d1, -d2, k);

    //k *= 4.0;
    // float h = max(k-abs(-d1-d2),0.0);
    // return max(-d1, d2) + h*h*0.25/k;
}

float opSmoothIntersection(float d1, float d2, float k)
{
    return -opSmoothUnion(-d1, -d2, k);

    //k *= 4.0;
    // float h = max(k-abs(d1-d2),0.0);
    // return max(d1, d2) + h*h*0.25/k;
}

float sdPlane(float3 p, float3 n, float h)
{
  // n must be normalized
    return dot(p, n) + h;
}

float sdSphere(float3 p, float s)
{
    return length(p) - s;
}

float sdCappedCylinder(float3 p, float h, float r)
{
    float2 d = abs(float2(length(p.xy), p.z)) - float2(r, h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0));
}

float sdRoundedCylinder(float3 p, float ra, float rb, float h)
{
    float2 d = float2(length(p.xy) - 2.0 * ra + rb, abs(p.z) - h);
    return min(max(d.x, d.y), 0.0) + length(max(d, 0.0)) - rb;
}

float sdRoundedTruncatedCone(float3 position, float radius1, float radius2, float halfHeight, float round)
{
    float2 p = float2(length(position.xy) - 0.5 * (radius2 + radius1), position.z);
    float2 end = float2(0.5 * (radius2 - radius1), halfHeight);
    float2 q = p - end * clamp(dot(p, end) / dot(end, end), -1.0, 1.0);
    float d = length(q);
    if (q.x > 0.0)
    {
        return d - round;
    }
    return max(-d, abs(p.y) - halfHeight) - round;
}

float sdGoPiece(float3 p, float r, float d)
{
    return opIntersection(
        sdSphere(p - float3(0, 0, d), r),
        sdSphere(p + float3(0, 0, d), r)
    );
}

float map(float3 p)
{
    float a = opSmoothUnion(
        sdPlane(p, float3(0,0,1), 0),
        sdGoPiece(p - float3(PLAYER_WIDTH / 2, PLAYER_HEIGHT - 75 * SCALE, 0), 26.0 * SCALE, 26.0 * SCALE/2),
        1.0
    );
    a = opSmoothSubtraction(
        sdGoPiece(p - float3(PLAYER_WIDTH / 2, PLAYER_HEIGHT - 75*2 * SCALE, 0), 26.0 * SCALE, 26.0 * SCALE / 2),
        a,
        1.0
    );
    a = opSmoothUnion(
        a,
        sdRoundedTruncatedCone(p - float3(PLAYER_WIDTH / 2, PLAYER_HEIGHT - 75 * 3 * SCALE, 0), 40, 30, 10, 10),
        8.0
    );
    return a;
}

float3 getnormal(float3 p)
{
    const float EPS = 0.001;
    
    float3 v1 = float3(
        map(p + float3(EPS, 0.0, 0.0)),
        map(p + float3(0.0, EPS, 0.0)),
        map(p + float3(0.0, 0.0, EPS))
    );
    float3 v2 = float3(
        map(p - float3(EPS, 0.0, 0.0)),
        map(p - float3(0.0, EPS, 0.0)),
        map(p - float3(0.0, 0.0, EPS))
    );
    
    return normalize(v1 - v2);
}

float3 raymarchvertical(float2 ro)
{
    float t = 0.0; // Total distance traveled
    for (int i = 0; i < 256; i++)
    { // Limit iterations for performance
        float3 p = float3(ro, 500.0 - t); // Current point in space
        float h = map(p); // SDF distance at point p

        if (h < 0.001)
        { // Intersection found
            return getnormal(p); // Return distance to intersection
        }

        t += h; // Step forward by the SDF distance
        if (t > 1000.0)
        { // Maximum distance exceeded (miss)
            return float3(0, 0, 1);
        }
    }
    return float3(0, 0, 1); // Miss after max iterations
}

float4 ps_main(VS_Output input) : SV_Target
{
    const float3 lightdir = normalize(float3(0.25, 0.25, -1));
    float2 px = input.uv;
    px.x *= (float) PLAYER_WIDTH;
    px.y *= (float) PLAYER_HEIGHT;
    float2 imgcenter = float2(PLAYER_WIDTH / 2, 164*SCALE);
    float imgradius = (170.0 / 2) * SCALE;
    float expradius = imgradius + 4;
    float2 imgpx = (px - (imgcenter - float2(expradius, expradius))) / (2.0 * expradius);
    float imgsdf = clamp(distance(px, imgcenter) - imgradius, 0.0, 1.0);
    float3 norm = raymarchvertical(px);
    float brightness = max(dot(norm, -lightdir), 0.0) * 
        lerp(1.0, 0.4583, input.uv.y * input.uv.y) +
        random(input.uv) * 0.025;
    const float4 grey = float4(
        0.2,
        0.2235294117647059,
        0.24313725490196078,
        1.0);
    const float4 white = float4(1, 1, 1, 1);
    return brightness*lerp(
        mytexture.Sample(mysampler, imgpx), 
        grey,
        imgsdf
    );
}