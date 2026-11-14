cbuffer SkyboxCB : register(b0)
{
    row_major float4x4 u_ViewRotProj;
    float4 u_SunDir;      // xyz = sun direction, w = time
    float4 u_ZenithColor; // xyz = zenith color, w = starIntensity
    float4 u_HorizonColor;// xyz = horizon color, w = sunSize
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

struct PSInput
{
    float4 position : SV_Position;
    float3 localDir : TEXCOORD0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.localDir = input.position;
    float4 clipPos = mul(float4(input.position, 1.0f), u_ViewRotProj);
    output.position = clipPos.xyww;
    return output;
}

float Hash31(float3 p)
{
    p = frac(p * float3(443.897f, 441.423f, 437.195f));
    p += dot(p, p.yzx + 19.19f);
    return frac((p.x + p.y) * p.z);
}

float4 PSMain(PSInput input) : SV_Target
{
    float3 dir = normalize(input.localDir);
    float y = dir.y;

    // Sky gradient from nadir -> horizon -> zenith
    float horizonBlend = pow(1.0f - saturate(abs(y)), 3.0f);
    float3 upperSky = lerp(u_ZenithColor.rgb, u_HorizonColor.rgb, pow(1.0f - saturate(y), 2.2f));
    float3 lowerSky = lerp(u_HorizonColor.rgb * 0.28f, u_HorizonColor.rgb * 0.75f, pow(1.0f - saturate(-y), 2.5f));
    float3 sky = (y >= 0.0f) ? upperSky : lowerSky;
    sky += u_HorizonColor.rgb * 0.25f * horizonBlend;

    // Procedural Nebula bands
    if (y > -0.05f)
    {
        float nebulaWave = sin(dir.x * 4.5f + dir.z * 3.0f + u_SunDir.w * 0.05f) * cos(dir.y * 6.0f - dir.x * 2.5f);
        float nebulaMask = saturate(nebulaWave * 0.5f + 0.5f) * saturate(y + 0.1f);
        sky += float3(0.08f, 0.03f, 0.16f) * pow(nebulaMask, 2.0f);
        sky += float3(0.02f, 0.12f, 0.18f) * pow(1.0f - nebulaMask, 3.0f) * saturate(y);

        // Procedural Starfield
        float3 starCell = floor(dir * 180.0f);
        float starVal = Hash31(starCell);
        if (starVal > 0.992f)
        {
            float twinkle = 0.65f + 0.35f * sin(u_SunDir.w * 3.5f + starVal * 100.0f);
            float brightness = (starVal - 0.992f) / 0.008f;
            sky += float3(0.85f, 0.92f, 1.0f) * brightness * twinkle * saturate(y * 2.0f) * u_ZenithColor.w;
        }
    }

    // Sun Disc and Corona
    float3 sunDir = normalize(u_SunDir.xyz);
    float sunCos = dot(dir, sunDir);
    float sunCore = smoothstep(0.9975f, 0.9992f, sunCos);
    float sunGlow = pow(saturate(sunCos), 16.0f) * 0.45f + pow(saturate(sunCos), 128.0f) * 0.8f;
    sky += float3(1.0f, 0.85f, 0.6f) * sunGlow + float3(1.0f, 0.98f, 0.9f) * sunCore * 2.0f;

    return float4(saturate(sky), 1.0f);
}
