cbuffer ShadowCB : register(b0)
{
    row_major float4x4 u_LightViewProj;
};

cbuffer ObjectCB : register(b1)
{
    row_major float4x4 u_Model;
    row_major float4x4 u_NormalMatrix;
    float4 u_AlbedoColor;
    float4 u_EmissiveColor;
    float4 u_MaterialParams;
};

struct VSInput
{
    float3 position : POSITION;
    float3 normal   : NORMAL;
    float2 uv       : TEXCOORD0;
    float4 color    : COLOR0;
};

float4 VSMain(VSInput input) : SV_Position
{
    float4 worldPos = mul(float4(input.position, 1.0f), u_Model);
    return mul(worldPos, u_LightViewProj);
}
