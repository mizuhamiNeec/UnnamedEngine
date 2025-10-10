#include "CopyImage.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

struct PixelShaderOutput
{
    float4 color;
};

PixelShaderOutput main(VertexShaderOutput input) : SV_TARGET
{
    PixelShaderOutput output;

	// これは普通のコピー
    output.color = gTexture.Sample(gSampler, input.texcoord);

	// これはグレースケール変換
	// float gray = dot(gTexture.Sample(gSampler, input.texcoord).rgb, float3(0.299, 0.587, 0.114));
	// output.color = float4(gray, gray, gray, 1.0);

    return output;
}
