#include "SceneConstants.hlsli"

struct VsIn {
	float3 pos : POSITION;
	float2 uv : TEXCOORD0;
};

struct VsOut {
	float4 pos : SV_POSITION;
	float2 uv : TEXCOORD0;
};

VsOut VsMain(VsIn input) {
	VsOut        output;
	const float4 worldPos = mul(float4(input.pos, 1.0f), gWorld);
	output.pos            = mul(worldPos, gViewProj);
	// shader では常時Y反転しません。RendererGraph から渡された uvMin/uvMax をそのまま使います。
	// gSkinningInfo.xy=uvMin, zw=uvMax。uvFlipY の責務は RendererGraph 側です。
	const float2 uvMin    = gSkinningInfo.xy;
	const float2 uvMax    = gSkinningInfo.zw;
	const float uvX       = lerp(uvMin.x, uvMax.x, input.uv.x);
	const float uvY       = lerp(uvMin.y, uvMax.y, input.uv.y);
	output.uv             = float2(uvX, uvY);
	return output;
}
