cbuffer SSAOCB : register(b0)
{
    row_major float4x4 u_Proj;
    row_major float4x4 u_InvProj;
    float4 u_SSAOParams;    // x = radius, y = intensity, z = bias, w = enabled
    float4 u_ViewportInfo;  // x = invW, y = invH, z = time, w = 0
};

Texture2D<float> g_DepthTex : register(t0);
Texture2D<float4> g_SSAORaw : register(t1);
SamplerState g_PointClamp : register(s0);

struct VSOut
{
    float4 position : SV_Position;
    float2 uv       : TEXCOORD0;
};

VSOut VSMain(uint id : SV_VertexID)
{
    VSOut o;
    float2 uv = float2((id << 1) & 2, id & 2);
    o.position = float4(uv.x * 2.0f - 1.0f, 1.0f - uv.y * 2.0f, 0.0f, 1.0f);
    o.uv = uv;
    return o;
}

float QuickHash(float2 pos)
{
    return frac(52.9829189f * frac(dot(pos, float2(0.06711056f, 0.00583715f))));
}

float3 ReconstructViewPos(float2 uv, float depth)
{
    float4 clipPos = float4(uv.x * 2.0f - 1.0f, (1.0f - uv.y) * 2.0f - 1.0f, depth, 1.0f);
    float4 viewPos = mul(clipPos, u_InvProj);
    return viewPos.xyz / max(viewPos.w, 0.0001f);
}

static const float2 kVogelDisk[12] = {
    float2(-0.20412f,  0.00000f),
    float2( 0.26036f, -0.23338f),
    float2(-0.04393f,  0.45471f),
    float2(-0.34014f, -0.43150f),
    float2( 0.60619f,  0.14072f),
    float2(-0.55670f,  0.40504f),
    float2( 0.14193f, -0.73517f),
    float2( 0.42758f,  0.67807f),
    float2(-0.83123f, -0.19450f),
    float2( 0.72047f, -0.54132f),
    float2(-0.21942f,  0.90364f),
    float2(-0.58384f, -0.76152f)
};

float4 PSMain(VSOut input) : SV_Target
{
    if (u_SSAOParams.w < 0.5f)
        return float4(1.0f, 1.0f, 1.0f, 1.0f);

    float centerDepth = g_DepthTex.SampleLevel(g_PointClamp, input.uv, 0).r;
    if (centerDepth >= 0.9999f)
        return float4(1.0f, 1.0f, 1.0f, 1.0f);

    float3 P = ReconstructViewPos(input.uv, centerDepth);
    float2 texel = u_ViewportInfo.xy;

    // Reconstruct sharp view-space normal using nearest small depth gradient
    float dR = g_DepthTex.SampleLevel(g_PointClamp, saturate(input.uv + float2(texel.x, 0.0f)), 0).r;
    float dL = g_DepthTex.SampleLevel(g_PointClamp, saturate(input.uv - float2(texel.x, 0.0f)), 0).r;
    float dD = g_DepthTex.SampleLevel(g_PointClamp, saturate(input.uv + float2(0.0f, texel.y)), 0).r;
    float dU = g_DepthTex.SampleLevel(g_PointClamp, saturate(input.uv - float2(0.0f, texel.y)), 0).r;

    float3 pR = ReconstructViewPos(saturate(input.uv + float2(texel.x, 0.0f)), dR);
    float3 pL = ReconstructViewPos(saturate(input.uv - float2(texel.x, 0.0f)), dL);
    float3 pD = ReconstructViewPos(saturate(input.uv + float2(0.0f, texel.y)), dD);
    float3 pU = ReconstructViewPos(saturate(input.uv - float2(0.0f, texel.y)), dU);

    float3 dPdx = (abs(pR.z - P.z) < abs(P.z - pL.z)) ? (pR - P) : (P - pL);
    float3 dPdy = (abs(pD.z - P.z) < abs(P.z - pU.z)) ? (pD - P) : (P - pU);
    float3 N = cross(dPdy, dPdx);
    float nLenSq = dot(N, N);
    N = (nLenSq > 1e-8f) ? (N * rsqrt(nLenSq)) : float3(0.0f, 0.0f, -1.0f);
    if (dot(N, -P) < 0.0f)
        N = -N;

    float radius = max(u_SSAOParams.x, 0.2f);
    float bias = max(u_SSAOParams.z, 0.02f);
    float angle = QuickHash(input.position.xy) * 6.2831853f;
    float s = sin(angle);
    float c = cos(angle);

    // Project view-space radius into approximate UV step scale
    float2 uvRadius = float2(u_Proj[0][0], u_Proj[1][1]) * 0.5f * (radius / max(P.z, 0.25f));
    uvRadius = clamp(uvRadius, texel * 1.5f, float2(0.14f, 0.14f));

    float occ = 0.0f;
    [unroll]
    for (int i = 0; i < 12; ++i)
    {
        float2 d = kVogelDisk[i];
        float2 rot = float2(d.x * c - d.y * s, d.x * s + d.y * c);
        float2 suv = input.uv + rot * uvRadius;
        if (suv.x < 0.002f || suv.x > 0.998f || suv.y < 0.002f || suv.y > 0.998f)
            continue;

        float sd = g_DepthTex.SampleLevel(g_PointClamp, suv, 0).r;
        if (sd >= 0.9999f)
            continue;

        float3 Q = ReconstructViewPos(suv, sd);
        float3 V = Q - P;
        float dist = length(V);
        if (dist > 0.001f && dist < radius)
        {
            float dirDot = max(dot(N, V / dist) - bias, 0.0f);
            float rangeAtten = saturate(1.0f - dist / radius);
            occ += dirDot * rangeAtten * rangeAtten;
        }
    }

    float ao = saturate(1.0f - (occ / 12.0f) * u_SSAOParams.y * 2.35f);
    return float4(ao, centerDepth, 0.0f, 1.0f);
}

float4 PSBlur(VSOut input) : SV_Target
{
    if (u_SSAOParams.w < 0.5f)
        return float4(1.0f, 1.0f, 1.0f, 1.0f);

    float2 center = g_SSAORaw.SampleLevel(g_PointClamp, input.uv, 0).xy;
    if (center.y >= 0.9999f)
        return float4(1.0f, 1.0f, 1.0f, 1.0f);

    float2 texel = u_ViewportInfo.xy * 2.0f; // half-res texel step
    float accum = 0.0f;
    float weightSum = 0.0f;

    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float2 suv = clamp(input.uv + float2(x, y) * texel, float2(0.001f, 0.001f), float2(0.999f, 0.999f));
            float2 tap = g_SSAORaw.SampleLevel(g_PointClamp, suv, 0).xy;
            float depthDiff = abs(tap.y - center.y);
            float w = exp(-depthDiff * 180.0f);
            accum += tap.x * w;
            weightSum += w;
        }
    }

    float ao = saturate(accum / max(weightSum, 0.0001f));
    return float4(ao, ao, ao, 1.0f);
}
