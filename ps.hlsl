#include "hdr.hlsli"
#include "playerdefs.h"

Texture2D mytexture : register(t0);
SamplerState mysampler : register(s0);

#define imgradius ((170.0 / 2) * SCALE)
#define expradius (imgradius + 4)
#define progress 0.33
#define proglen (progress * (PLAYER_WIDTH - 44 * SCALE))
#define pbradius 30
#define skipradius 28

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

float sdVerticalCapsule(float3 p, float h, float r)
{
    p.y -= clamp(p.y, 0.0, h);
    return length(p) - r;
}

float sdHorizontalCapsule(float3 p, float h, float r)
{
    p.x -= clamp(p.x, 0.0, h);
    return length(p) - r;
}

float sd2dCapsule(float2 p, float2 a, float2 b, float r)
{
    float2 pa = p - a, ba = b - a;
    float h = clamp(dot(pa, ba) / dot(ba, ba), 0.0, 1.0);
    return length(pa - ba * h) - r;
}

float capsuleSDF(
    in float2 P, // Sample coordinates
    in float2 B, // Capsule begin coordinates
    in float2 E, // Capsule end coordinates
    in float R)  // Thickness
{
    float2 BP = P - B; // from B to P
    float2 BE = E - B; // from B to E
    
    // dot(SP, SE) - squared length of projection SP to SE.
    // dot(SE, SE) - capsule squared length.
    float t = clamp(dot(BP, BE) / dot(BE, BE), 0.0, 1.0);

    // Minimal distance from P to line BE * t.
    float2 K = BP - BE * t;

    return sqrt(dot(K, K)) - R;
    //return abs(sqrt(dot(K, K)) - R); // outline
}

float sdCircle(float2 p, float r)
{
    return length(p) - r;
}

float sdRing(float2 p, float outerRadius, float innerRadius)
{
    return abs(sdCircle(p, outerRadius)) - innerRadius;
}

float2 rotate(float2 samplePosition, float rotation)
{
    const float PI = 3.14159;
    float angle = rotation * PI * 2 * -1;
    float sine, cosine;
    sincos(angle, sine, cosine);
    return float2(cosine * samplePosition.x + sine * samplePosition.y, cosine * samplePosition.y - sine * samplePosition.x);
}

float sdEquilateralTriangle(in float2 p, in float r)
{
    const float k = sqrt(3.0);
    p.x = abs(p.x) - r;
    p.y = p.y + r / k;
    if (p.x + k * p.y > 0.0)
        p = float2(p.x - k * p.y, -k * p.x - p.y) / 2.0;
    p.x -= clamp(p.x, -2.0 * r, 0.0);
    return -length(p) * sign(p.y);
}

float map(float3 p)
{
    float a = opSmoothUnion(
        sdPlane(p, float3(0,0,1), 0),
        sdRoundedTruncatedCone(p - float3(PLAYER_WIDTH / 2, PLAYER_HEIGHT - 75 * SCALE, 0), pbradius, pbradius-2, 5, 5),
        8.0
    );
    a = opSmoothUnion(
        a,
        sdRoundedTruncatedCone(p - float3(56 * SCALE, PLAYER_HEIGHT - 75 * SCALE, 0), skipradius, skipradius - 2, 5, 5),
        8.0
    );
    a = opSmoothUnion(
        a,
        sdRoundedTruncatedCone(p - float3(PLAYER_WIDTH - 56 * SCALE, PLAYER_HEIGHT - 75 * SCALE, 0), skipradius, skipradius - 2, 5, 5),
        8.0
    );
    a = opSmoothUnion(
        sdHorizontalCapsule(p - float3(22 * SCALE, PLAYER_HEIGHT - 157 * SCALE, 0), proglen, 3),
        a,
        1.0
    );
    a = opSmoothUnion(
        sdRoundedTruncatedCone(p - float3(22 * SCALE + proglen, PLAYER_HEIGHT - 157 * SCALE, 0), 10, 9, 5, 1),
        a,
        4.0
    );
    a = opSmoothSubtraction(
        sdHorizontalCapsule(p - float3(22 * SCALE + proglen, PLAYER_HEIGHT - 157 * SCALE, 0), PLAYER_WIDTH - 44 * SCALE - proglen, 3),
        a,
        1.0
    );
    a = opSmoothUnion(
        a,
        sdRoundedTruncatedCone(p - float3(PLAYER_WIDTH - 35 * SCALE, 35 * SCALE, 0), 15, 12, 5, 5),
        8.0
    );
    a = opSmoothUnion(
        a,
        sdRoundedTruncatedCone(p - float3(35 * SCALE, 35 * SCALE, 0), 15, 12, 5, 5),
        8.0
    );
    a = opSmoothUnion(
        a,
        sdRoundedTruncatedCone(p - float3(PLAYER_WIDTH / 2, 164 * SCALE, 0), imgradius+10, imgradius+10 - 10, 10, 10),
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
    const float2 imgcenter = float2(PLAYER_WIDTH / 2, 164 * SCALE);
    float2 px = input.uv;
    px.x *= (float) PLAYER_WIDTH;
    px.y *= (float) PLAYER_HEIGHT;
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
    const float4 orange = float4(
        0.9333333333333333,
        0.3333333333333333,
        0.050980392156862744,
        1.0
    );
    const float4 blue = float4(
        0.8627450980392157,
        0.6627450980392157,
        0.12941176470588237,
        1.0
    );
    const float4 white = float4(
        1.0,
        0.9882352941176471,
        0.984313725490196,
        1.0
    );
    const float4 barcolor = lerp(orange, blue, (max(px.x, 22 * SCALE) - 22 * SCALE) / proglen);
    const float4 paint = float4(0.5254901960784314, 0.5333333333333333, 0.5450980392156862, 1);
    float barsdf = capsuleSDF(
        px, float2(22 * SCALE, PLAYER_HEIGHT - 157 * SCALE),
        float2(22 * SCALE + proglen,
        PLAYER_HEIGHT - 157 * SCALE), 3
    );
    barsdf = opUnion(
        barsdf, 
        sdCircle(px - float2(22 * SCALE + proglen, PLAYER_HEIGHT - 157 * SCALE), 12)
    );
    barsdf = opSmoothSubtraction(
        sdRing(px - float2(22 * SCALE + proglen, PLAYER_HEIGHT - 157 * SCALE), 10, 6),
        barsdf,
        0.25
    );
    barsdf = clamp(barsdf, 0.0, 1.0);
    float playbtnsdf = clamp(sdCircle(
        px - float2(PLAYER_WIDTH / 2, PLAYER_HEIGHT - 75 * SCALE), pbradius+10
    ), 0.0, 1.0);
    #define xlen 4.0
    #define hburgydif 4.0
    float xsdf = opUnion(
        capsuleSDF(
            px,
            float2(PLAYER_WIDTH - 35 * SCALE - xlen, 35 * SCALE - xlen),
            float2(PLAYER_WIDTH - 35 * SCALE + xlen, 35 * SCALE + xlen),
            1
        ),
        capsuleSDF(
            px,
            float2(PLAYER_WIDTH - 35 * SCALE - xlen, 35 * SCALE + xlen),
            float2(PLAYER_WIDTH - 35 * SCALE + xlen, 35 * SCALE - xlen),
            1
    ));
    xsdf = opUnion(
        xsdf,
        capsuleSDF(
            px,
            float2(35 * SCALE - xlen, 35 * SCALE),
            float2(35 * SCALE + xlen, 35 * SCALE),
            1
        )
    );
    xsdf = opUnion(
        xsdf,
        capsuleSDF(
            px,
            float2(35 * SCALE - xlen, 35 * SCALE - hburgydif),
            float2(35 * SCALE + xlen, 35 * SCALE - hburgydif),
            1
        )
    );
    xsdf = opUnion(
        xsdf,
        capsuleSDF(
            px,
            float2(35 * SCALE - xlen, 35 * SCALE + hburgydif),
            float2(35 * SCALE + xlen, 35 * SCALE + hburgydif),
            1
        )
    );
    xsdf = clamp(xsdf, 0.0, 1.0);
    float playsdf = sdEquilateralTriangle(rotate(px - float2(PLAYER_WIDTH / 2, PLAYER_HEIGHT - 75 * SCALE), 0.25), 6) - 1.0;
    playsdf = clamp(playsdf, 0.0, 1.0);
    float4 c = lerp(mytexture.Sample(mysampler, imgpx), grey, imgsdf);
    c = lerp(c, barcolor, 1.0 - barsdf);
    c = lerp(c, lerp(orange * 1.5, white * 1.5, 1.0 - playsdf), 1.0 - playbtnsdf);
    c = lerp(c, paint, 1.0 - xsdf);
    float skipsdf = sdCircle(px - float2(56 * SCALE, PLAYER_HEIGHT - 75 * SCALE), skipradius + 6);
    skipsdf = opUnion(skipsdf, sdCircle(px - float2(PLAYER_WIDTH - 56 * SCALE, PLAYER_HEIGHT - 75 * SCALE), skipradius + 6));
    skipsdf = clamp(skipsdf, 0.0, 1.0);
    return lerp(brightness, brightness + 0.15, 1.0 - skipsdf) * c;
}