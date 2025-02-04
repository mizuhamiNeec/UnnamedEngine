#include "Default.hlsli"

Texture2D diffuseTexture : register(t0);
SamplerState samplerState : register(s0);

float4 PSMain(VSOutput input) : SV_TARGET
{
    float4 color = diffuseTexture.Sample(samplerState, input.texcoord);
    //color.xyz = input.normal;
    return color;
}