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
