Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);

cbuffer RadialBlurParams : register(b0)
{
	float blurStrength; // 0-1
	float blurRadius;   // 0-1
	float2 padding;
};

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

// 簡易 8tap ラジアルブラー
float4 main(VSOut i) : SV_Target {
	float2 center = float2(0.5, 0.5);
	float2 dir    = i.uv - center;
	float dist    = length(dir);
	float blurAmt = saturate((dist - blurRadius) / (1.0 - blurRadius)) * blurStrength;

	// サンプル位置を内側へ収束
	const int TAP = 8;
	float3 col = 0;
	float weightSum = 0;
	for (int t = 0; t < TAP; ++t) {
		float k = (float)t / (TAP - 1);       // 0 → 1
		float2 uv = i.uv - dir * blurAmt * k; // 0=外側,1=中心
		float w = (1.0 - k);                  // 緩やかな重み
		col += gTex.Sample(gSamp, uv).rgb * w;
		weightSum += w;
	}
	col /= weightSum;
	return float4(col, 1.0);
}
