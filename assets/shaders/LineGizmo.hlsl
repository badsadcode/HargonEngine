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
