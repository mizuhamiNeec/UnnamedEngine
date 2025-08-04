Texture2D gTex : register(t0);
SamplerState gSamp : register(s0);

cbuffer CAParams : register(b0)
{
	float strength;   // ピクセル単位
	float blend;	  // ブレンド
	float2 padding;
};

struct VSOut { float4 pos : SV_Position; float2 uv : TEXCOORD0; };

float3 ApplyCA(Texture2D tex, SamplerState s, float2 uv, float2 texel, float k)
{
	float2 center = float2(0.5, 0.5);
	float2 dir    = normalize(uv - center);
	float dist    = length(uv - center);

	float2 offR = dir *  k * dist * 0.8;
	float2 offB = dir * -k * dist * 1.2;

	float r = tex.Sample(s, uv + offR).r;
	float g = tex.Sample(s, uv).g;
	float b = tex.Sample(s, uv + offB).b;
	return float3(r, g, b);
}

float4 main(VSOut i) : SV_Target
{
	uint w, h; gTex.GetDimensions(w, h);
	float2 texel = 1.0 / float2(w, h);

	float3 src = gTex.Sample(gSamp, i.uv).rgb;
	float3 ca  = ApplyCA(gTex, gSamp, i.uv, texel, strength);

	float3 outCol = lerp(src, ca, blend);
	return float4(outCol, 1.0);
}