#include "CopyImage.hlsli"

Texture2D    gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer BloomParams : register(b0) {
	float  gBloomStrength;
	float  gBloomThreshold;
	float2 gPadding;
};

float GetLuminance(float3 col) { return dot(col, float3(0.299, 0.587, 0.114)); }

static const int   BLUR_RADIUS     = 3;
static const float BLUR_WEIGHTS[4] = {
	0.227027f, 0.1945946f, 0.1216216f, 0.054054f
};

static const int    kBlurDirectionCount                  = 4;
static const float2 kBlurDirections[kBlurDirectionCount] =
{
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(0.70710678f, 0.70710678f), // 斜め方向
	float2(0.70710678f, -0.70710678f) // 斜め方向
};

/// @brief ブラーのサンプル数を増やして、より高品質なブラーを実現する関数
/// @param tex ブラーをかけるテクスチャ
/// @param samp サンプラーステート
/// @param uv テクスチャ座標
/// @param texelSize テクセルサイズ（1.0 / テクスチャの幅と高さ）
/// @param radius ブラーの半径
/// @return ブラーがかかった色
float4 WideGaussianBlur(
	Texture2D    tex,
	SamplerState samp,
	float2       uv,
	float2       texelSize,
	float        radius
) {
	float4 color     = tex.Sample(samp, uv) * BLUR_WEIGHTS[0];
	float  weightSum = BLUR_WEIGHTS[0];

	for (int i = 1; i <= BLUR_RADIUS; ++i) {
		float weight      = BLUR_WEIGHTS[i];
		float offsetScale = radius * i;
		
		for (int dirIndex = 0; dirIndex < kBlurDirectionCount; ++dirIndex) {
			float2 offset = kBlurDirections[dirIndex] * texelSize * offsetScale;
			color         += tex.Sample(samp, uv + offset) * weight;
			color         += tex.Sample(samp, uv - offset) * weight;
			weightSum     += weight * 2.0f;
		}
	}

	return color / max(weightSum, 0.0001f);
}

float4 HighQualityBloom(
	Texture2D tex, SamplerState samp, float2 uv, float2 texelSize,
	float     threshold
) {
	float4 b0    = WideGaussianBlur(tex, samp, uv, texelSize, 1.0f);
	float4 b1    = WideGaussianBlur(tex, samp, uv, texelSize, 2.5f);
	float4 b2    = WideGaussianBlur(tex, samp, uv, texelSize, 5.0f);
	float4 b3    = WideGaussianBlur(tex, samp, uv, texelSize, 10.0f);
	float4 bloom = (b0 * 0.5f + b1 * 0.35f + b2 * 0.2f + b3 * 0.1f) / (
		               0.5f + 0.35f + 0.2f + 0.1f);

	float lum = GetLuminance(bloom.rgb);
	if (lum < threshold) { bloom = float4(0.0f, 0.0f, 0.0f, 0.0f); }

	return bloom;
}

float4 main(VertexShaderOutput input) : SV_TARGET {
	float4 srcColor = gTexture.Sample(gSampler, input.texcoord);

	uint width, height;
	gTexture.GetDimensions(width, height);
	float2 texelSize = float2(1.0 / width, 1.0 / height);

	float4 bloom = HighQualityBloom(
		gTexture, gSampler, input.texcoord, texelSize, gBloomThreshold
	);
	float4 bloomCombined = srcColor + bloom * gBloomStrength;
	bloomCombined.a      = srcColor.a;

	return bloomCombined;
}
