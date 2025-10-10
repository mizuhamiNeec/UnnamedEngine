#include "CopyImage.hlsli"

Texture2D gTexture : register(t0);
SamplerState gSampler : register(s0);

cbuffer BloomParams : register(b0)
{
    float bloomStrength;
    float bloomThreshold;
    float2 padding;
};

float GetLuminance(float3 col)
{
    return dot(col, float3(0.299, 0.587, 0.114));
}

// 多タップ・広範囲ガウシアンカーネル
static const int BLUR_RADIUS = 7;
static const float BLUR_WEIGHTS[7] =
{
    0.070159f, 0.131515f, 0.190254f, 0.215144f, 0.190254f, 0.131515f, 0.070159f
};

// 多タップガウスブラー
float4 WideGaussianBlur(Texture2D tex, SamplerState samp, float2 uv, float2 texelSize, float radius)
{
    float4 color = float4(0, 0, 0, 0);
    float weightSum = 0.0;

	// 横
    for (int i = -BLUR_RADIUS / 2; i <= BLUR_RADIUS / 2; i++)
    {
        int idx = abs(i);
        float2 offset = float2(i, 0) * texelSize * radius;
        color += tex.Sample(samp, uv + offset) * BLUR_WEIGHTS[idx];
        weightSum += BLUR_WEIGHTS[idx];
    }
    float4 horiz = color / weightSum;

    color = float4(0, 0, 0, 0);
    weightSum = 0.0;

	// 縦
    for (int i = -BLUR_RADIUS / 2; i <= BLUR_RADIUS / 2; i++)
    {
        int idx = abs(i);
        float2 offset = float2(0, i) * texelSize * radius;
        color += tex.Sample(samp, uv + offset) * BLUR_WEIGHTS[idx];
        weightSum += BLUR_WEIGHTS[idx];
    }
    float4 vert = color / weightSum;

    return (horiz + vert) * 0.5;
}

// 高品質ブルーム（4段階ガウスブラー＋しきい値適用）
float4 HighQualityBloom(Texture2D tex, SamplerState samp, float2 uv, float2 texelSize, float threshold)
{
    float4 b0 = WideGaussianBlur(tex, samp, uv, texelSize, 1.0);
    float4 b1 = WideGaussianBlur(tex, samp, uv, texelSize, 2.5);
    float4 b2 = WideGaussianBlur(tex, samp, uv, texelSize, 5.0);
    float4 b3 = WideGaussianBlur(tex, samp, uv, texelSize, 10.0);
    float4 bloom = (b0 * 0.5 + b1 * 0.35 + b2 * 0.2 + b3 * 0.1) / (0.5 + 0.35 + 0.2 + 0.1);

    float lum = GetLuminance(bloom.rgb);
    if (lum < threshold)
    {
        bloom = float4(0, 0, 0, 0);
    }
    return bloom;
}

float4 main(VertexShaderOutput input) : SV_TARGET
{
    float4 srcColor = gTexture.Sample(gSampler, input.texcoord);

    uint width, height;
    gTexture.GetDimensions(width, height);
    float2 texelSize = float2(1.0 / width, 1.0 / height);

    float4 bloom = HighQualityBloom(gTexture, gSampler, input.texcoord, texelSize, bloomThreshold);
    float4 bloomCombined = srcColor + bloom * bloomStrength;
    bloomCombined.a = srcColor.a;

    return bloomCombined;
}