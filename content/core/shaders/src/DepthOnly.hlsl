#include "SceneConstants.hlsli"

struct VsIn {
	float3 pos : POSITION;
	float3 nrm : NORMAL;
	float2 uv : TEXCOORD0;
	float4 tangent : TANGENT;
	float4 boneIndices : TEXCOORD1;
	float4 boneWeights : TEXCOORD2;
};

struct VsOut {
	float4 pos : SV_POSITION;
};

VsOut VsMain(VsIn i) {
	VsOut  o;
	float3 localPos = i.pos;

	if (gSkinningInfo.y > 0.5f) {
		float4 skinnedPos = 0.0f;

		[unroll]
		for (uint k = 0; k < 4; ++k) {
			const float weight = i.boneWeights[k];
			if (weight <= 0.0f) {
				continue;
			}

			const uint boneIndex = min((uint)i.boneIndices[k], 511u);
			skinnedPos           += mul(
				float4(i.pos, 1.0f), gSkinMatrices[boneIndex]
			) * weight;
		}

		localPos = skinnedPos.xyz;
	}

	const float4 wp = mul(float4(localPos, 1.0f), gWorld);
	o.pos           = mul(wp, gViewProj);
	return o;
}

void PsMain() {
}
