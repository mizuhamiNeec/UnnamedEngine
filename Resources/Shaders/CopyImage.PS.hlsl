#include "CopyImage.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer PostProcessParams : register(b0) {
	float bloomStrength;
	float bloomThreshold;
	float vignetteStrength;
	float vignetteRadius;
	float chromaticAberrationStrength;
	float3 padding;
}

struct PixelShaderOutput {
	float4 color;
};

float GetLuminance(float3 col) {
	return dot(col, float3(0.299, 0.587, 0.114));
}

// 多タップ・広範囲ガウシアンカーネル
static const int BLUR_RADIUS = 7;
static const float BLUR_WEIGHTS[7] = {
	0.070159f, 0.131515f, 0.190254f, 0.215144f, 0.190254f, 0.131515f, 0.070159f
};

// 多タップガウスブラー
float4 WideGaussianBlur(Texture2D tex, SamplerState samp, float2 uv, float2 texelSize, float radius) {
	float4 color = float4(0, 0, 0, 0);
	float weightSum = 0.0;

	// 横
	for (int i = -BLUR_RADIUS / 2; i <= BLUR_RADIUS / 2; i++) {
		int idx = abs(i);
		float2 offset = float2(i, 0) * texelSize * radius;
		color += tex.Sample(samp, uv + offset) * BLUR_WEIGHTS[idx];
		weightSum += BLUR_WEIGHTS[idx];
	}
	float4 horiz = color / weightSum;

	color = float4(0, 0, 0, 0);
	weightSum = 0.0;

	// 縦
	for (int i = -BLUR_RADIUS / 2; i <= BLUR_RADIUS / 2; i++) {
		int idx = abs(i);
		float2 offset = float2(0, i) * texelSize * radius;
		color += tex.Sample(samp, uv + offset) * BLUR_WEIGHTS[idx];
		weightSum += BLUR_WEIGHTS[idx];
	}
	float4 vert = color / weightSum;

	return (horiz + vert) * 0.5;
}

// 高品質ブルーム
float4 HighQualityBloom(Texture2D tex, SamplerState samp, float2 uv, float2 texelSize, float threshold) {
	float4 b0 = WideGaussianBlur(tex, samp, uv, texelSize, 1.0);
	float4 b1 = WideGaussianBlur(tex, samp, uv, texelSize, 2.5);
	float4 b2 = WideGaussianBlur(tex, samp, uv, texelSize, 5.0);
	float4 b3 = WideGaussianBlur(tex, samp, uv, texelSize, 10.0);
	float4 bloom = (b0 * 0.5 + b1 * 0.35 + b2 * 0.2 + b3 * 0.1) / (0.5 + 0.35 + 0.2 + 0.1);

	// 輝度しきい値抽出
	float lum = GetLuminance(bloom.rgb);
	if (lum < threshold) {
		bloom = float4(0, 0, 0, 0);
	}
	return bloom;
}

// クロマティックアベレーション
float3 ChromaticAberration(Texture2D tex, SamplerState samp, float2 uv, float2 texelSize, float strength) {
	float2 center = float2(0.5, 0.5);
	float2 toCenter = uv - center;
	float dist = length(toCenter);

	float chromaR = strength * dist * 0.8;
	float chromaB = -strength * dist * 1.2;

	float r = tex.Sample(samp, uv + normalize(toCenter) * chromaR).r;
	float g = tex.Sample(samp, uv).g;
	float b = tex.Sample(samp, uv + normalize(toCenter) * chromaB).b;

	return float3(r, g, b);
}

PixelShaderOutput main(VertexShaderOutput input) : SV_TARGET {
	PixelShaderOutput output;

	// これは普通のコピー
	//	output.color = gTexture.Sample(gSampler, input.texcoord);

	// これはグレースケール変換
	// float gray = dot(gTexture.Sample(gSampler, input.texcoord).rgb, float3(0.299, 0.587, 0.114));
	// output.color = float4(gray, gray, gray, 1.0);

	float4 srcColor = gTexture.Sample(gSampler, input.texcoord);
	uint width, height;
	gTexture.GetDimensions(width, height);
	float2 texelSize = float2(1.0 / width, 1.0 / height);

	// 高品質ブルーム
	float4 bloom = HighQualityBloom(gTexture, gSampler, input.texcoord, texelSize, bloomThreshold);
	float4 bloomCombined = srcColor + bloom * bloomStrength;
	bloomCombined.a = srcColor.a;

	// クロマティックアベレーション
	float aberrationStrength = chromaticAberrationStrength;
	float3 aberrationColor = ChromaticAberration(gTexture, gSampler, input.texcoord, texelSize, aberrationStrength);

	output.color.rgb = lerp(bloomCombined.rgb, aberrationColor.rgb, 0.5); // ブレンドはお好みで
	output.color.a = srcColor.a;

	// ビネットのみ（FXAA等省略でOK・必要なら復活してください）
	float2 center = float2(0.5, 0.5);
	float dist = distance(input.texcoord, center);
	float vignette = 1.0 - vignetteStrength * smoothstep(vignetteRadius, 1.0, dist);
	output.color.rgb *= vignette;

	return output;
}
