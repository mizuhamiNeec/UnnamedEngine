#include "CopyImage.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer VignetteParams : register(b0)
{
	float vignetteStrength;
	float vignetteRadius;
	float2 padding;
};

float4 main(VertexShaderOutput input) : SV_TARGET {
	float4 col = gTexture.Sample(gSampler, input.texcoord);

	float2 center = float2(0.5, 0.5);
	float dist    = distance(input.texcoord, center);
	float vig     = 1.0 - vignetteStrength * smoothstep(vignetteRadius, 1.0, dist);

	col.rgb *= vig;
	return col;
}
