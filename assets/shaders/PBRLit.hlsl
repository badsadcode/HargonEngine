cbuffer SceneCB : register(b0)
{
    row_major float4x4 u_ViewProj;
    row_major float4x4 u_LightViewProj;
    float4 u_CameraPos;       // xyz = cam pos, w = time
    float4 u_DirLightDir;     // xyz = dir toward light, w = intensity
    float4 u_DirLightColor;   // xyz = color, w = ambientStrength
    float4 u_SkyColor;        // xyz = sky horizon color, w = fogDensity
    float4 u_GroundColor;     // xyz = ground color, w = pointLightCount
    float4 u_PointLightPosRange[8];
    float4 u_PointLightColorIntensity[8];
    float4 u_EnvParams0;      // x = tonemapMode, y = exposure, z = white, w = glowIntensity
    float4 u_EnvParams1;      // x = fogHeight, y = fogHeightDensity, z = fogSunScatter, w = contrast
    float4 u_EnvParams2;      // x = brightness, y = saturation, z = glowBloom, w = shadowStrength
    float4 u_ShadowParams;    // x = shadowBias, y = 1.0/shadowSize, z = shadowsEnabled, w = 0
};

cbuffer ObjectCB : register(b1)
{
    row_major float4x4 u_Model;
    row_major float4x4 u_NormalMatrix;
    float4 u_AlbedoColor;
    float4 u_EmissiveColor;   // xyz = emissive color, w = emissiveStrength
    float4 u_MaterialParams;  // x = metallic, y = roughness, z = uvScale, w = patternType
};

Texture2D g_AlbedoTex : register(t0);
Texture2D<float> g_ShadowMap : register(t1);
SamplerState g_Sampler : register(s0);
SamplerState g_ShadowSampler : register(s1);

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
    float3 worldPos : TEXCOORD0;
    float3 normal   : TEXCOORD1;
    float2 uv       : TEXCOORD2;
    float4 color    : COLOR0;
};

PSInput VSMain(VSInput input)
{
    PSInput output;
    float4 worldPos = mul(float4(input.position, 1.0f), u_Model);
    output.worldPos = worldPos.xyz;
    output.position = mul(worldPos, u_ViewProj);
    output.normal = normalize(mul(float4(input.normal, 0.0f), u_NormalMatrix).xyz);
    output.uv = input.uv * max(u_MaterialParams.z, 0.01f);
    output.color = input.color;
    return output;
}

static const float PI = 3.14159265359f;

float DistributionGGX(float3 N, float3 H, float roughness)
{
    float a = roughness * roughness;
    float a2 = a * a;
    float NdotH = max(dot(N, H), 0.0f);
    float NdotH2 = NdotH * NdotH;
    float denom = (NdotH2 * (a2 - 1.0f) + 1.0f);
    denom = PI * denom * denom;
    return a2 / max(denom, 0.0001f);
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0f);
    float k = (r * r) / 8.0f;
    return NdotV / max(NdotV * (1.0f - k) + k, 0.0001f);
}

float GeometrySmith(float3 N, float3 V, float3 L, float roughness)
{
    float NdotV = max(dot(N, V), 0.0f);
    float NdotL = max(dot(N, L), 0.0f);
    return GeometrySchlickGGX(NdotV, roughness) * GeometrySchlickGGX(NdotL, roughness);
}

float3 FresnelSchlick(float cosTheta, float3 F0)
{
    return F0 + (1.0f - F0) * pow( saturate(1.0f - cosTheta), 5.0f );
}

float3 EvaluateLight(float3 N, float3 V, float3 L, float3 radiance, float3 albedo, float metallic, float roughness, float3 F0)
{
    float3 H = normalize(V + L);
    float NDF = DistributionGGX(N, H, roughness);
    float G = GeometrySmith(N, V, L, roughness);
    float3 F = FresnelSchlick(max(dot(H, V), 0.0f), F0);

    float3 numerator = NDF * G * F;
    float denominator = 4.0f * max(dot(N, V), 0.0f) * max(dot(N, L), 0.0f) + 0.0001f;
    float3 specular = numerator / denominator;

    float3 kS = F;
    float3 kD = (float3(1.0f, 1.0f, 1.0f) - kS) * (1.0f - metallic);
    float NdotL = max(dot(N, L), 0.0f);
    return (kD * albedo / PI + specular) * radiance * NdotL;
}

float ComputeDirectionalShadow(float3 worldPos, float3 N, float3 Ldir)
{
    if (u_ShadowParams.z < 0.5f)
        return 1.0f;

    float4 sc = mul(float4(worldPos, 1.0f), u_LightViewProj);
    float3 proj = sc.xyz / max(sc.w, 0.0001f);
    float2 uv = float2(proj.x * 0.5f + 0.5f, -proj.y * 0.5f + 0.5f);

    if (uv.x <= 0.001f || uv.x >= 0.999f || uv.y <= 0.001f || uv.y >= 0.999f || proj.z <= 0.001f || proj.z >= 0.999f)
        return 1.0f;

    float2 edgeDist = min(uv, 1.0f - uv);
    float edgeFade = smoothstep(0.0f, 0.08f, min(edgeDist.x, edgeDist.y));

    float cosTheta = saturate(dot(N, Ldir));
    float bias = clamp(u_ShadowParams.x * (1.25f - 0.8f * cosTheta), 0.0005f, 0.015f);
    float currentDepth = proj.z - bias;
    float texel = max(u_ShadowParams.y, 1.0f / 2048.0f);

    // 3x3 PCF filter inspired by Godot DirectionalLight3D soft shadows
    float sum = 0.0f;
    [unroll]
    for (int y = -1; y <= 1; ++y)
    {
        [unroll]
        for (int x = -1; x <= 1; ++x)
        {
            float sampled = g_ShadowMap.SampleLevel(g_ShadowSampler, uv + float2(x, y) * texel * 1.35f, 0).r;
            sum += (currentDepth <= sampled) ? 1.0f : 0.0f;
        }
    }
    float pcf = sum / 9.0f;
    return lerp(1.0f, pcf, edgeFade * saturate(u_EnvParams2.w));
}

float4 PSMain(PSInput input) : SV_Target
{
    float3 N = normalize(input.normal);
    float3 V = normalize(u_CameraPos.xyz - input.worldPos);

    float4 texSample = g_AlbedoTex.Sample(g_Sampler, input.uv);
    float3 baseColor = u_AlbedoColor.rgb * input.color.rgb * texSample.rgb;

    int pattern = (int)(u_MaterialParams.w + 0.5f);
    float3 wp = input.worldPos;
    // Triplanar world-space UV for crisp Quake-style brush & prop texturing
    float2 triUV = (abs(N.y) > 0.6f) ? wp.xz : ((abs(N.x) > 0.6f) ? wp.zy : wp.xy);
    triUV *= max(u_MaterialParams.z, 0.01f);

    // Simple procedural hash for weathered Quake grit & mortar
    float2 cellId = floor(triUV * 8.0f);
    float grit = frac(sin(dot(cellId, float2(12.9898f, 78.233f))) * 43758.5453f);
    float fineGrit = frac(sin(dot(floor(triUV * 32.0f), float2(39.346f, 11.135f))) * 24634.6345f);

    if (pattern == 1) // World-space Sci-Fi Arena Grid
    {
        float2 gridUV = input.worldPos.xz * 0.5f;
        float2 deriv = fwidth(gridUV);
        float2 grid = abs(frac(gridUV - 0.5f) - 0.5f) / max(deriv, 0.001f);
        float lineFactor = 1.0f - saturate(min(grid.x, grid.y));

        float2 majorUV = input.worldPos.xz * 0.1f;
        float2 majorDeriv = fwidth(majorUV);
        float2 majorGrid = abs(frac(majorUV - 0.5f) - 0.5f) / max(majorDeriv, 0.001f);
        float majorFactor = 1.0f - saturate(min(majorGrid.x, majorGrid.y));

        baseColor = lerp(baseColor, baseColor * 1.45f + float3(0.05f, 0.16f, 0.25f), lineFactor * 0.45f);
        baseColor = lerp(baseColor, float3(0.12f, 0.45f, 0.65f), majorFactor * 0.65f);
    }
    else if (pattern == 2) // Checkerboard
    {
        float2 c = floor(input.uv * 4.0f);
        float checker = fmod(abs(c.x + c.y), 2.0f);
        baseColor *= lerp(0.72f, 1.15f, checker);
    }
    else if (pattern == 3) // Tech panels
    {
        float2 p = frac(input.uv * 3.0f);
        float border = step(0.06f, p.x) * step(0.06f, p.y) * step(p.x, 0.94f) * step(p.y, 0.94f);
        baseColor *= lerp(0.55f, 1.08f, border);
    }
    else if (pattern == 4) // Energy Crystal / Runic Shimmer
    {
        float pulse = 0.85f + 0.25f * sin(u_CameraPos.w * 4.0f + input.worldPos.y * 3.0f);
        float rim = pow(1.0f - max(dot(N, V), 0.0f), 2.2f);
        baseColor = baseColor * pulse + rim * float3(0.4f, 0.9f, 1.0f);
    }
    else if (pattern == 5) // Quake 1 Weathered Gothic Stone Bricks (Walls)
    {
        float2 brickUV = triUV * float2(1.5f, 3.0f);
        float row = floor(brickUV.y);
        brickUV.x += fmod(abs(row), 2.0f) * 0.5f;
        float2 bFrac = frac(brickUV);
        float2 bId = floor(brickUV);
        float brickHash = frac(sin(dot(bId, float2(127.1f, 311.7f))) * 43758.5453f);
        float mortar = step(0.06f, bFrac.x) * step(bFrac.x, 0.94f) * step(0.09f, bFrac.y) * step(bFrac.y, 0.91f);
        float bevel = smoothstep(0.0f, 0.16f, min(min(bFrac.x, 1.0f - bFrac.x), min(bFrac.y, 1.0f - bFrac.y)));
        float3 stoneTint = baseColor * lerp(0.76f, 1.18f, brickHash) * lerp(0.86f, 1.08f, fineGrit);
        // Subtle moss/soot banding along vertical height
        float grime = 0.80f + 0.20f * sin(wp.y * 1.4f + grit * 2.5f);
        baseColor = lerp(baseColor * 0.28f, stoneTint * bevel * grime, mortar);
    }
    else if (pattern == 6) // Quake 1 Runic Cobblestone / Flagstone Floor Tiles
    {
        float2 tileUV = triUV * 2.0f;
        float2 tFrac = frac(tileUV);
        float2 tId = floor(tileUV);
        float tileHash = frac(sin(dot(tId, float2(41.1f, 289.7f))) * 43758.5453f);
        float seam = step(0.05f, tFrac.x) * step(tFrac.x, 0.95f) * step(0.05f, tFrac.y) * step(tFrac.y, 0.95f);
        float innerBorder = abs(max(abs(tFrac.x - 0.5f), abs(tFrac.y - 0.5f)) - 0.36f) < 0.025f ? 0.78f : 1.0f;
        float3 tileCol = baseColor * lerp(0.80f, 1.16f, tileHash) * lerp(0.88f, 1.08f, fineGrit) * innerBorder;
        baseColor = lerp(baseColor * 0.30f, tileCol, seam);
    }
    else if (pattern == 7) // Quake 1 Vaulted Coffered Ceiling / Ribbed Timber-Stone
    {
        float2 cofUV = triUV * 1.5f;
        float2 cFrac = frac(cofUV);
        float dEdge = min(min(cFrac.x, 1.0f - cFrac.x), min(cFrac.y, 1.0f - cFrac.y));
        float rib = step(dEdge, 0.14f);
        float sunken = smoothstep(0.14f, 0.28f, dEdge);
        baseColor = baseColor * lerp(1.12f, 0.62f, sunken) * lerp(0.88f, 1.06f, fineGrit);
        baseColor = lerp(baseColor, baseColor * 1.25f, rib * 0.5f);
    }
    else if (pattern == 8) // Quake 1 Riveted Rusted Iron & Base Panels (Industrial/Bunker Walls)
    {
        float2 pUV = triUV * float2(1.2f, 1.2f);
        float2 pFrac = frac(pUV);
        float2 pId = floor(pUV);
        float panelVar = frac(sin(dot(pId, float2(93.1f, 157.3f))) * 43758.5453f);
        float seam = step(0.04f, pFrac.x) * step(pFrac.x, 0.96f) * step(0.04f, pFrac.y) * step(pFrac.y, 0.96f);
        // Corner rivets
        float2 cornerDist = abs(pFrac - 0.5f);
        float rivet = step(0.39f, cornerDist.x) * step(0.39f, cornerDist.y) * step(cornerDist.x, 0.44f) * step(cornerDist.y, 0.44f);
        // Rust & hazard stripe accent
        float rust = smoothstep(0.55f, 0.95f, grit) * 0.35f;
        float3 metalCol = lerp(baseColor * lerp(0.82f, 1.15f, panelVar), float3(0.48f, 0.24f, 0.12f), rust);
        metalCol *= lerp(0.90f, 1.08f, fineGrit);
        baseColor = lerp(baseColor * 0.22f, metalCol + rivet * 0.25f, seam);
    }
    else if (pattern == 9) // Quake 1 Industrial Metal Grating & Diamond Plate Floor
    {
        float2 gUV = triUV * 4.0f;
        float2 gFrac = frac(gUV);
        float crossBar = step(abs(gFrac.x - 0.5f), 0.14f) + step(abs(gFrac.y - 0.5f), 0.14f);
        float frame = step(0.92f, max(frac(triUV.x * 1.5f), frac(triUV.y * 1.5f)));
        float3 plate = baseColor * lerp(0.45f, 1.15f, saturate(crossBar)) * lerp(0.88f, 1.08f, fineGrit);
        baseColor = lerp(plate, baseColor * 1.30f, frame);
    }
    else if (pattern == 10) // Quake 1 Runic Key-Door / Ornate Metal Slab
    {
        float2 dUV = input.uv * 4.0f;
        float2 dFrac = frac(dUV);
        float border = step(0.08f, dFrac.x) * step(dFrac.x, 0.92f) * step(0.08f, dFrac.y) * step(dFrac.y, 0.92f);
        float runeGlow = step(abs(dFrac.x - 0.5f), 0.08f) * step(abs(dFrac.y - 0.5f), 0.32f)
                       + step(abs(dFrac.y - 0.5f), 0.08f) * step(abs(dFrac.x - 0.5f), 0.32f);
        baseColor = baseColor * lerp(0.45f, 1.05f, border) * lerp(0.88f, 1.08f, fineGrit);
        baseColor += saturate(runeGlow) * u_EmissiveColor.rgb * 0.45f;
    }
    else if (pattern == 11) // Glowing Lava / Slime / Slipgate Portal Swirl
    {
        float t = u_CameraPos.w * 1.6f;
        float2 sUV = triUV * 1.5f;
        float w1 = sin(sUV.x * 3.2f + t) * cos(sUV.y * 3.2f - t * 0.8f);
        float w2 = sin((sUV.x + sUV.y) * 4.5f - t * 1.3f);
        float magma = saturate(0.5f + 0.35f * w1 + 0.25f * w2);
        baseColor = lerp(baseColor * 0.35f, baseColor * 1.65f + float3(0.35f, 0.25f, 0.05f), magma);
    }
    else if (pattern == 12) // Quake 1 Blood Spatter / Gib Gore Decal
    {
        float2 bUV = input.uv * 2.0f - 1.0f;
        float r2 = dot(bUV, bUV);
        float splat = sin(atan2(bUV.y, bUV.x) * 7.0f + grit * 6.28f) * 0.22f;
        if (r2 > 0.72f + splat)
            discard;
        float coreDark = smoothstep(0.0f, 0.75f, r2);
        baseColor = lerp(float3(0.28f, 0.01f, 0.01f), float3(0.68f, 0.04f, 0.03f), coreDark) * lerp(0.80f, 1.15f, fineGrit);
    }
    else if (pattern == 13) // Wooden Crate / Timber Planks with Iron Bands
    {
        float2 wUV = input.uv * 3.0f;
        float plank = step(0.05f, frac(wUV.y));
        float grain = 0.88f + 0.12f * sin(wUV.x * 18.0f + sin(wUV.y * 6.0f) * 2.0f);
        float ironBand = step(0.88f, max(abs(input.uv.x - 0.5f) * 2.0f, abs(input.uv.y - 0.5f) * 2.0f));
        float3 wood = baseColor * plank * grain * lerp(0.88f, 1.08f, fineGrit);
        baseColor = lerp(wood, float3(0.22f, 0.20f, 0.18f), ironBand);
    }
    else if (pattern == 14) // Monster Flesh / Hide / Chitin Armor Scales
    {
        float2 mUV = input.uv * 8.0f;
        float scalePattern = 0.82f + 0.22f * sin(mUV.x * 3.14159f) * cos(mUV.y * 3.14159f);
        baseColor = baseColor * scalePattern * lerp(0.85f, 1.15f, fineGrit);
    }

    float metallic = saturate(u_MaterialParams.x);
    float roughness = clamp(u_MaterialParams.y, 0.06f, 1.0f);

    float3 F0 = float3(0.04f, 0.04f, 0.04f);
    F0 = lerp(F0, baseColor, metallic);

    // Directional Sun Light with 3x3 PCF Real-Time Shadowing
    float3 Ldir = normalize(u_DirLightDir.xyz);
    float3 dirRadiance = u_DirLightColor.rgb * u_DirLightDir.w;
    float shadowFactor = ComputeDirectionalShadow(input.worldPos, N, Ldir);
    float3 Lo = EvaluateLight(N, V, Ldir, dirRadiance, baseColor, metallic, roughness, F0) * shadowFactor;

    // Dynamic Point Lights
    int pointCount = min((int)(u_GroundColor.w + 0.5f), 8);
    for (int i = 0; i < pointCount; ++i)
    {
        float3 lightPos = u_PointLightPosRange[i].xyz;
        float lightRange = max(u_PointLightPosRange[i].w, 0.1f);
        float3 toLight = lightPos - input.worldPos;
        float dist = length(toLight);
        if (dist < lightRange)
        {
            float3 L = toLight / max(dist, 0.0001f);
            float atten = saturate(1.0f - (dist / lightRange));
            atten = atten * atten;
            float3 radiance = u_PointLightColorIntensity[i].rgb * u_PointLightColorIntensity[i].w * atten;
            Lo += EvaluateLight(N, V, L, radiance, baseColor, metallic, roughness, F0);
        }
    }

    // Hemisphere Ambient Lighting (with subtle shadow occlusion for grounded contact contrast)
    float hemi = N.y * 0.5f + 0.5f;
    float3 ambientIrradiance = lerp(u_GroundColor.rgb, u_SkyColor.rgb, hemi) * u_DirLightColor.w;
    float3 ambient = ambientIrradiance * baseColor * lerp(0.82f, 1.0f, shadowFactor);

    // Subtle Fresnel rim for 3D readability
    float rimTerm = pow(1.0f - max(dot(N, V), 0.0f), 3.5f);
    ambient += rimTerm * u_SkyColor.rgb * 0.18f * (1.0f - roughness * 0.5f);

    // Emissive + Godot Glow / Bloom Boost
    float glowBoost = 1.0f + u_EnvParams0.w + u_EnvParams2.z * 2.0f;
    float3 emissive = u_EmissiveColor.rgb * u_EmissiveColor.w * glowBoost;

    float3 color = ambient + Lo + emissive;

    // Godot Environment Fog: Distance Exponential Fog + Height Fog + Sun Scatter
    float camDist = length(u_CameraPos.xyz - input.worldPos);
    float distFog = 1.0f - exp(-camDist * u_SkyColor.w);
    float heightFog = 0.0f;
    if (abs(u_EnvParams1.y) > 0.0001f)
    {
        heightFog = saturate(exp(-(input.worldPos.y - u_EnvParams1.x) * u_EnvParams1.y) * 0.35f);
    }
    float fogFactor = saturate(distFog + heightFog * distFog);

    float sunAlign = pow(saturate(dot(-V, Ldir)), 6.0f) * u_EnvParams1.z;
    float3 fogColor = lerp(u_SkyColor.rgb * 0.65f, u_DirLightColor.rgb * 0.85f, saturate(sunAlign));
    color = lerp(color, fogColor, fogFactor);

    // Exposure & Godot ToneMappers (0=Linear, 1=Reinhard, 2=Filmic, 3=ACES, 4=AgX)
    float exposure = max(u_EnvParams0.y, 0.05f);
    float3 x = color * exposure;
    int tmMode = (int)(u_EnvParams0.x + 0.5f);

    float3 mapped = x;
    if (tmMode == 0) // Linear
    {
        mapped = saturate(x);
    }
    else if (tmMode == 1) // Reinhard (with white point)
    {
        float w2 = max(u_EnvParams0.z * u_EnvParams0.z, 0.1f);
        mapped = (x * (1.0f + x / w2)) / (1.0f + x);
    }
    else if (tmMode == 2) // Filmic (Hable / Uncharted 2)
    {
        float3 A = 0.15f, B = 0.50f, C = 0.10f, D = 0.20f, E = 0.02f, F = 0.30f;
        mapped = ((x * (A * x + C * B) + D * E) / (x * (A * x + B) + D * F)) - E / F;
        float3 whiteScale = 1.0f / (((11.2f * (A * 11.2f + C * B) + D * E) / (11.2f * (A * 11.2f + B) + D * F)) - E / F);
        mapped = saturate(mapped * whiteScale);
    }
    else if (tmMode == 3) // ACES Fitted
    {
        mapped = saturate((x * (2.51f * x + 0.03f)) / (x * (2.43f * x + 0.59f) + 0.14f));
    }
    else // 4 = AgX (Godot / Blender AgX Filmic with highlight chroma preservation)
    {
        float3 agx = x / (x + 0.52f);
        float luma = dot(agx, float3(0.2126f, 0.7152f, 0.0722f));
        float highlightDesat = smoothstep(0.55f, 1.0f, luma) * 0.28f;
        mapped = lerp(agx, float3(luma, luma, luma), highlightDesat);
    }

    // Gamma Correction
    mapped = pow(max(mapped, 0.0f), 1.0f / 2.2f);

    // Godot Environment Color Adjustments (Brightness, Contrast, Saturation)
    float brightness = u_EnvParams2.x;
    float contrast   = u_EnvParams1.w;
    float saturation = u_EnvParams2.y;
    mapped = mapped * brightness;
    mapped = (mapped - 0.5f) * contrast + 0.5f;
    float gray = dot(mapped, float3(0.2126f, 0.7152f, 0.0722f));
    mapped = saturate(lerp(float3(gray, gray, gray), mapped, saturation));

    return float4(mapped, u_AlbedoColor.a);
}
