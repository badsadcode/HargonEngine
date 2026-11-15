#pragma once
#include <cstdint>
#include <cstddef>

#include "../../assets/shaders/PBRLit_vs_spv.h"
#include "../../assets/shaders/PBRLit_ps_spv.h"
#include "../../assets/shaders/ShadowDepth_vs_spv.h"
#include "../../assets/shaders/SSAO_vs_spv.h"
#include "../../assets/shaders/SSAO_ps_spv.h"
#include "../../assets/shaders/SSAO_blur_ps_spv.h"
#include "../../assets/shaders/Skybox_vs_spv.h"
#include "../../assets/shaders/Skybox_ps_spv.h"
#include "../../assets/shaders/LineGizmo_vs_spv.h"
#include "../../assets/shaders/LineGizmo_ps_spv.h"
#include "../../assets/shaders/ImGuiShader_vs_spv.h"
#include "../../assets/shaders/ImGuiShader_ps_spv.h"

namespace Hargon::Shaders
{
    inline constexpr const char* kLineGizmoHLSL = R"HLSL(
cbuffer LineCB : register(b0)
{
    row_major float4x4 u_ViewProj;
};
struct VSInput
{
    float3 position : POSITION;
    float4 color    : COLOR0;
};
struct PSInput
{
    float4 position : SV_Position;
    float4 color    : COLOR0;
};
PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = mul(float4(input.position, 1.0f), u_ViewProj);
    output.color = input.color;
    return output;
}
float4 PSMain(PSInput input) : SV_Target
{
    return input.color;
}
)HLSL";

    inline constexpr const char* kImGuiHLSL = R"HLSL(
cbuffer ImGuiCB : register(b0)
{
    row_major float4x4 u_ProjMatrix;
};
Texture2D g_Texture : register(t0);
SamplerState g_Sampler : register(s0);
struct VSInput
{
    float2 pos : POSITION;
    float2 uv  : TEXCOORD0;
    float4 col : COLOR0;
};
struct PSInput
{
    float4 pos : SV_Position;
    float4 col : COLOR0;
    float2 uv  : TEXCOORD0;
};
PSInput VSMain(VSInput input)
{
    PSInput output;
    output.pos = mul(float4(input.pos.xy, 0.0f, 1.0f), u_ProjMatrix);
    output.col = input.col;
    output.uv  = input.uv;
    return output;
}
float4 PSMain(PSInput input) : SV_Target
{
    return input.col * g_Texture.Sample(g_Sampler, input.uv);
}
)HLSL";
}
